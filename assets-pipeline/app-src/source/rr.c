/* rr renderer: citro2d backend. See include/rr.h. */
#include "rr.h"

#include <3ds.h>
#include <citro2d.h>
#include <citro3d.h>
#include <jansson.h>
#include <math.h>
#include <png.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pak.h"

#include <ft2build.h>
#include FT_FREETYPE_H

struct rr_tex {
  C3D_Tex tex;
  int w, h;         /* image size */
  size_t bytes;
  bool dirty;       /* CPU wrote texels this frame; flush before submit */
  rr_tex* next;     /* live list, for stats + dirty flush */
};

struct rr_atlas {
  rr_tex* tex; /* page 0 */
  int count;
  rr_frame* frames;
  char** names;
  char* name_blob; /* RRT atlases: names point into this */
  int npages;
  rr_tex** pages;  /* RRT atlases; NULL for JSON atlases (tex only) */
  uint32_t* palette;
  int npal;
};

const rr_draw_opts RR_DEFAULT_OPTS = {0, 0, 1, 1, 0, RR_RGBA(255, 255, 255, 255)};

static C3D_RenderTarget* g_targets[2];
static int g_origin[2][2] = {{40, 30}, {0, 60}};
static rr_screen g_cur = RR_TOP;
static rr_tex* g_live;
static C2D_TextBuf g_sysbuf;
static float g_cpu_ms, g_gpu_ms;

/* ------------------------------------------------------------------ */
/* textures                                                             */

static int next_pow2(int v) {
  int p = 8;
  while (p < v) p <<= 1;
  return p;
}

/* Morton offset of (x,y) inside an 8x8 tile, PICA200 layout. */
static inline u32 morton8(u32 x, u32 y) {
  return (x & 1) | ((y & 1) << 1) | ((x & 2) << 1) | ((y & 2) << 2) | ((x & 4) << 2) | ((y & 4) << 3);
}

/* Texel index for image pixel (x,y), y measured from the TOP of the image.
 * Memory row 0 is sampled at v = 1.0, which citro2d treats as the top edge,
 * so image rows map straight to memory rows (checked in Azahar). */
static inline u32 texel_index(u32 x, u32 y, u32 tw, u32 th) {
  (void)th;
  return (((y >> 3) * (tw >> 3) + (x >> 3)) << 6) + morton8(x, y);
}

static rr_tex* tex_create(int w, int h, GPU_TEXCOLOR fmt) {
  rr_tex* t = calloc(1, sizeof *t);
  if (!t) return NULL;
  int tw = next_pow2(w), th = next_pow2(h);
  if (tw > 1024 || th > 1024 || !C3D_TexInit(&t->tex, tw, th, fmt)) {
    free(t);
    return NULL;
  }
  C3D_TexSetFilter(&t->tex, GPU_NEAREST, GPU_NEAREST);
  C3D_TexSetWrap(&t->tex, GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);
  memset(t->tex.data, 0, t->tex.size);
  t->w = w;
  t->h = h;
  t->bytes = t->tex.size;
  t->next = g_live;
  g_live = t;
  return t;
}

rr_tex* rr_tex_load_png(const char* path) {
  png_image img;
  memset(&img, 0, sizeof img);
  img.version = PNG_IMAGE_VERSION;
  if (!png_image_begin_read_from_file(&img, path)) return NULL;
  img.format = PNG_FORMAT_RGBA;
  u8* px = malloc(PNG_IMAGE_SIZE(img));
  if (!px) {
    png_image_free(&img);
    return NULL;
  }
  if (!png_image_finish_read(&img, NULL, px, 0, NULL)) {
    free(px);
    return NULL;
  }
  int w = img.width, h = img.height;
  bool binary_alpha = true;
  for (int i = 0; i < w * h; i++) {
    u8 a = px[i * 4 + 3];
    if (a != 0 && a != 255) {
      binary_alpha = false;
      break;
    }
  }
  rr_tex* t = tex_create(w, h, binary_alpha ? GPU_RGBA5551 : GPU_RGBA8);
  if (!t) {
    free(px);
    return NULL;
  }
  u32 tw = t->tex.width, th = t->tex.height;
  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      const u8* p = &px[(y * w + x) * 4];
      u32 i = texel_index(x, y, tw, th);
      if (binary_alpha) {
        ((u16*)t->tex.data)[i] =
            ((p[0] >> 3) << 11) | ((p[1] >> 3) << 6) | ((p[2] >> 3) << 1) | (p[3] ? 1 : 0);
      } else {
        ((u32*)t->tex.data)[i] = ((u32)p[0] << 24) | ((u32)p[1] << 16) | ((u32)p[2] << 8) | p[3];
      }
    }
  }
  free(px);
  C3D_TexFlush(&t->tex);
  return t;
}

void rr_tex_free(rr_tex* t) {
  if (!t) return;
  for (rr_tex** pp = &g_live; *pp; pp = &(*pp)->next) {
    if (*pp == t) {
      *pp = t->next;
      break;
    }
  }
  C3D_TexDelete(&t->tex);
  free(t);
}

int rr_tex_width(const rr_tex* t) { return t->w; }
int rr_tex_height(const rr_tex* t) { return t->h; }
size_t rr_tex_bytes(const rr_tex* t) { return t->bytes; }

rr_frame rr_tex_frame(rr_tex* t) {
  rr_frame f = {t, 0, 0, t->w, t->h, 0, 0, t->w, t->h};
  return f;
}

/* ------------------------------------------------------------------ */
/* atlases                                                              */

static int jint(json_t* o, const char* k) { return (int)json_integer_value(json_object_get(o, k)); }

rr_atlas* rr_atlas_load(const char* png_path, const char* json_path) {
  json_error_t err;
  json_t* root = json_load_file(json_path, 0, &err);
  if (!root) return NULL;
  /* Upstream uses the TexturePacker "multiatlas" layout: textures[0].frames[].
   * Plain "frames" arrays/objects also exist in a few UI files. */
  json_t* frames = NULL;
  json_t* textures = json_object_get(root, "textures");
  if (json_is_array(textures) && json_array_size(textures) > 0)
    frames = json_object_get(json_array_get(textures, 0), "frames");
  if (!frames) frames = json_object_get(root, "frames");
  if (!frames) {
    json_decref(root);
    return NULL;
  }
  rr_tex* tex = rr_tex_load_png(png_path);
  if (!tex) {
    json_decref(root);
    return NULL;
  }
  rr_atlas* a = calloc(1, sizeof *a);
  a->tex = tex;
  size_t n = json_is_array(frames) ? json_array_size(frames) : json_object_size(frames);
  a->frames = calloc(n, sizeof *a->frames);
  a->names = calloc(n, sizeof *a->names);
  size_t i = 0;
  const char* key;
  json_t* fr;
  void* it = json_is_object(frames) ? json_object_iter(frames) : NULL;
  for (;;) {
    if (json_is_array(frames)) {
      if (i >= n) break;
      fr = json_array_get(frames, i);
      key = json_string_value(json_object_get(fr, "filename"));
    } else {
      if (!it) break;
      key = json_object_iter_key(it);
      fr = json_object_iter_value(it);
      it = json_object_iter_next(frames, it);
    }
    json_t* r = json_object_get(fr, "frame");
    json_t* ss = json_object_get(fr, "spriteSourceSize");
    json_t* src = json_object_get(fr, "sourceSize");
    rr_frame* f = &a->frames[a->count];
    f->tex = tex;
    f->x = jint(r, "x");
    f->y = jint(r, "y");
    f->w = jint(r, "w");
    f->h = jint(r, "h");
    f->off_x = ss ? jint(ss, "x") : 0;
    f->off_y = ss ? jint(ss, "y") : 0;
    f->src_w = src ? jint(src, "w") : f->w;
    f->src_h = src ? jint(src, "h") : f->h;
    a->names[a->count] = strdup(key ? key : "");
    a->count++;
    i++;
  }
  json_decref(root);
  return a;
}

void rr_atlas_free(rr_atlas* a) {
  if (!a) return;
  if (a->name_blob)
    free(a->name_blob);
  else
    for (int i = 0; i < a->count; i++) free(a->names[i]);
  free(a->names);
  free(a->frames);
  if (a->pages) {
    for (int i = 0; i < a->npages; i++) rr_tex_free(a->pages[i]);
    free(a->pages);
  } else {
    rr_tex_free(a->tex);
  }
  free(a->palette);
  free(a);
}

/* ------------------------------------------------------------------ */
/* RRT (asset pipeline) atlases                                         */

typedef struct {
  char magic[4];
  u8 kind, gpu;
  u16 pages, frames, npal, img_w, img_h;
  u32 names_size;
} __attribute__((packed)) rrt_header;

typedef struct {
  u16 page, x, y, w, h;
  s16 off_x, off_y;
  u16 src_w, src_h, name_off;
} __attribute__((packed)) rrt_frame;

/* 8-bit RGBA -> RGBA5551, truncating like the PNG path. Visible colours that
 * differ in 8 bits but collide in 5 bits (8% of upstream Pokemon sheets have at
 * least one such pair) are nudged to the nearest free 5-bit colour so shading
 * steps don't merge. */
static void palette_to_5551(const u32* pal, int n, u16* out) {
  out[0] = 0;
  for (int i = 1; i < n; i++) {
    u32 c = pal[i];
    int r = (c >> 27) & 31, g = (c >> 19) & 31, b = (c >> 11) & 31, a = (c & 0xFF) ? 1 : 0;
    u16 v = (u16)((r << 11) | (g << 6) | (b << 1) | a);
    bool clash = false;
    for (int j = 1; j < i && !clash; j++)
      clash = out[j] == v && (pal[j] & 0xFFFFFF00u) != (c & 0xFFFFFF00u);
    if (clash) {
      /* nearest free colour, searching +-1 then +-2 per channel */
      for (int d = 1; d <= 2 && clash; d++) {
        for (int dr = -d; dr <= d && clash; dr++)
          for (int dg = -d; dg <= d && clash; dg++)
            for (int db = -d; db <= d && clash; db++) {
              int rr2 = r + dr, gg = g + dg, bb = b + db;
              if (rr2 < 0 || gg < 0 || bb < 0 || rr2 > 31 || gg > 31 || bb > 31) continue;
              u16 w = (u16)((rr2 << 11) | (gg << 6) | (bb << 1) | a);
              bool used = false;
              for (int j = 1; j < i && !used; j++) used = out[j] == w;
              if (!used) {
                v = w;
                clash = false;
              }
            }
      }
    }
    out[i] = v;
  }
}

static void apply_recolor(u32* pal, int n, const rr_recolor_pair* map, int nmap) {
  if (nmap > 32) nmap = 32; /* upstream's shader takes 32 pairs */
  for (int i = 1; i < n; i++) {
    for (int k = 0; k < nmap; k++) {
      if ((pal[i] & 0xFFFFFF00u) == (map[k].from & 0xFFFFFF00u)) {
        pal[i] = (map[k].to & 0xFFFFFF00u) | (pal[i] & 0xFF);
        break;
      }
    }
  }
}

rr_atlas* rr_atlas_load_rrt(const void* data, size_t len, const rr_recolor_pair* map, int nmap) {
  const u8* p = data;
  if (len < sizeof(rrt_header)) return NULL;
  rrt_header h;
  memcpy(&h, p, sizeof h);
  if (memcmp(h.magic, "RRT1", 4)) return NULL;
  size_t off = sizeof h;
  size_t need = off + h.npal * 4u + h.pages * 4u + h.frames * sizeof(rrt_frame) + h.names_size;
  if (need > len) return NULL;
  rr_atlas* a = calloc(1, sizeof *a);
  if (!a) return NULL;
  a->npal = h.npal;
  if (h.npal) {
    a->palette = malloc(h.npal * 4u);
    memcpy(a->palette, p + off, h.npal * 4u);
  }
  off += h.npal * 4u;
  const u16* page_dims = (const u16*)(p + off);
  off += h.pages * 4u;
  const rrt_frame* fr = (const rrt_frame*)(p + off);
  off += h.frames * sizeof(rrt_frame);
  a->name_blob = malloc(h.names_size + 1);
  memcpy(a->name_blob, p + off, h.names_size);
  a->name_blob[h.names_size] = 0;
  off += h.names_size;
  off = (off + 3) & ~(size_t)3;

  bool rgba8 = h.gpu == 1;
  u32 pal32[256];
  u16 pal16[256];
  if (h.kind == 0) {
    int n = h.npal > 256 ? 256 : h.npal;
    memcpy(pal32, a->palette, n * 4u);
    if (map && nmap > 0) apply_recolor(pal32, n, map, nmap);
    if (rgba8)
      pal32[0] = 0;
    else
      palette_to_5551(pal32, n, pal16);
  }
  a->npages = h.pages;
  a->pages = calloc(h.pages, sizeof *a->pages);
  bool ok = a->pages != NULL;
  for (int i = 0; ok && i < h.pages; i++) {
    int pw = page_dims[i * 2], ph = page_dims[i * 2 + 1];
    size_t texels = (size_t)pw * ph;
    size_t bytes = h.kind == 0 ? texels : texels * (rgba8 ? 4 : 2);
    if (off + bytes > len) {
      ok = false;
      break;
    }
    /* texture "image size" = page size: frames address pages directly */
    rr_tex* t = tex_create(pw, ph, rgba8 ? GPU_RGBA8 : GPU_RGBA5551);
    if (!t) {
      ok = false;
      break;
    }
    const u8* src = p + off;
    if (h.kind == 0) {
      if (rgba8) {
        u32* dst = t->tex.data;
        for (size_t k = 0; k < texels; k++) dst[k] = pal32[src[k]];
      } else {
        u16* dst = t->tex.data;
        for (size_t k = 0; k < texels; k++) dst[k] = pal16[src[k]];
      }
    } else {
      memcpy(t->tex.data, src, bytes);
    }
    C3D_TexFlush(&t->tex);
    a->pages[i] = t;
    off += bytes;
  }
  if (!ok) {
    rr_atlas_free(a);
    return NULL;
  }
  a->tex = a->npages ? a->pages[0] : NULL;
  a->count = h.frames;
  a->frames = calloc(h.frames ? h.frames : 1, sizeof *a->frames);
  a->names = calloc(h.frames ? h.frames : 1, sizeof *a->names);
  for (int i = 0; i < h.frames; i++) {
    rrt_frame f;
    memcpy(&f, &fr[i], sizeof f);
    rr_frame* o = &a->frames[i];
    o->tex = f.page < a->npages ? a->pages[f.page] : NULL;
    o->x = f.x;
    o->y = f.y;
    o->w = f.w;
    o->h = f.h;
    o->off_x = f.off_x;
    o->off_y = f.off_y;
    o->src_w = f.src_w;
    o->src_h = f.src_h;
    a->names[i] = a->name_blob + (f.name_off < h.names_size ? f.name_off : h.names_size);
  }
  return a;
}

rr_atlas* rr_atlas_load_key(const char* key) {
  size_t len;
  void* d = pak_read(key, &len);
  if (!d) return NULL;
  rr_atlas* a = rr_atlas_load_rrt(d, len, NULL, 0);
  free(d);
  return a;
}

rr_atlas* rr_atlas_load_key_variant(const char* key, const char* map_key, int variant) {
  size_t len, mlen;
  void* d = pak_read(key, &len);
  if (!d) return NULL;
  u8* m = map_key ? pak_read(map_key, &mlen) : NULL;
  rr_recolor_pair pairs[64];
  int n = 0;
  if (m && mlen >= 5 && !memcmp(m, "RVM1", 4)) {
    size_t o = 5;
    for (int v = 0; v < m[4] && o + 4 <= mlen; v++) {
      int which = m[o];
      u16 cnt = m[o + 2] | (m[o + 3] << 8);
      o += 4;
      if (which == variant) {
        for (int k = 0; k < cnt && k < 64 && o + 8 * (k + 1) <= mlen; k++) {
          memcpy(&pairs[k].from, m + o + 8 * k, 4);
          memcpy(&pairs[k].to, m + o + 8 * k + 4, 4);
          n = k + 1;
        }
      }
      o += 8u * cnt;
    }
  }
  free(m);
  rr_atlas* a = rr_atlas_load_rrt(d, len, pairs, n);
  free(d);
  return a;
}

rr_tex* rr_tex_load_key(const char* key) {
  rr_atlas* a = rr_atlas_load_key(key);
  if (!a) return NULL;
  if (a->npages != 1 || a->count != 1 || a->frames[0].x || a->frames[0].y || a->frames[0].off_x ||
      a->frames[0].off_y) {
    rr_atlas_free(a);
    return NULL;
  }
  rr_tex* t = a->pages[0];
  t->w = a->frames[0].w; /* image size, so rr_tex_frame() covers just the image */
  t->h = a->frames[0].h;
  a->npages = 0; /* keep the texture alive */
  rr_atlas_free(a);
  return t;
}

int rr_atlas_pages(const rr_atlas* a) { return a ? (a->pages ? a->npages : 1) : 0; }
rr_tex* rr_atlas_page(const rr_atlas* a, int i) {
  if (!a) return NULL;
  if (!a->pages) return i == 0 ? a->tex : NULL;
  return i >= 0 && i < a->npages ? a->pages[i] : NULL;
}
int rr_atlas_palette(const rr_atlas* a, const uint32_t** pal) {
  if (pal) *pal = a ? a->palette : NULL;
  return a ? a->npal : 0;
}

int rr_atlas_count(const rr_atlas* a) { return a ? a->count : 0; }
const rr_frame* rr_atlas_frame(const rr_atlas* a, int i) {
  return (a && i >= 0 && i < a->count) ? &a->frames[i] : NULL;
}
const rr_frame* rr_atlas_find(const rr_atlas* a, const char* name) {
  if (!a) return NULL;
  for (int i = 0; i < a->count; i++)
    if (!strcmp(a->names[i], name)) return &a->frames[i];
  return NULL;
}
rr_tex* rr_atlas_tex(const rr_atlas* a) { return a ? a->tex : NULL; }

void rr_draw_all(const rr_atlas* a, float x, float y, const rr_draw_opts* o) {
  if (!a) return;
  for (int i = 0; i < a->count; i++) rr_draw(&a->frames[i], x, y, o);
}

/* ------------------------------------------------------------------ */
/* drawing                                                              */

bool rr_init(void) {
  gfxInitDefault();
  if (!C3D_Init(C3D_DEFAULT_CMDBUF_SIZE)) return false;
  if (!C2D_Init(C2D_DEFAULT_MAX_OBJECTS)) return false;
  C2D_Prepare();
  g_targets[RR_TOP] = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
  g_targets[RR_BOTTOM] = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);
  g_sysbuf = C2D_TextBufNew(4096);
  return g_targets[0] && g_targets[1];
}

void rr_exit(void) {
  C2D_TextBufDelete(g_sysbuf);
  C2D_Fini();
  C3D_Fini();
  gfxExit();
}

void rr_set_origin(rr_screen s, int px, int py) {
  g_origin[s][0] = px;
  g_origin[s][1] = py;
}

void rr_frame_begin(void) {
  C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
  C2D_TextBufClear(g_sysbuf);
}

void rr_target(rr_screen s, rr_color clear) {
  g_cur = s;
  C2D_TargetClear(g_targets[s], __builtin_bswap32(clear)); /* citro2d wants 0xAABBGGRR */
  C2D_SceneBegin(g_targets[s]);
  rr_set_blend(RR_BLEND_NORMAL);
  rr_set_tone(0, 0, 0);
}

static rr_blend g_blend = RR_BLEND_NORMAL;
static int g_tone[3];

void rr_set_blend(rr_blend b) {
  if (b == g_blend) return;
  C2D_Flush();
  g_blend = b;
  if (b == RR_BLEND_ADD)
    C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA, GPU_ONE, GPU_ZERO, GPU_ONE);
  else /* citro3d's default */
    C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_SRC_ALPHA,
                   GPU_ONE_MINUS_SRC_ALPHA);
}

static int clamp255(int v) { return v < 0 ? 0 : v > 255 ? 255 : v; }

void rr_set_tone(int r, int g, int b) {
  if (r == g_tone[0] && g == g_tone[1] && b == g_tone[2]) return;
  C2D_Flush();
  g_tone[0] = r, g_tone[1] = g, g_tone[2] = b;
  /* citro2d uses texenv 0..3 for its image modes, keeps 4 as a no-op and 5 for its fade (unused
   * here). The tone takes 4 (add the positive part) and 5 (subtract the negative part). */
  C3D_TexEnv* env = C3D_GetTexEnv(4);
  C3D_TexEnvInit(env);
  C3D_TexEnv* env5 = C3D_GetTexEnv(5);
  C3D_TexEnvInit(env5);
  if (!r && !g && !b) {
    /* back to citro2d's stage 5: mix(previous, fade colour, fade alpha) with no fade */
    C3D_TexEnvSrc(env5, C3D_RGB, GPU_PREVIOUS, GPU_CONSTANT, GPU_CONSTANT);
    C3D_TexEnvOpRgb(env5, GPU_TEVOP_RGB_SRC_COLOR, GPU_TEVOP_RGB_SRC_COLOR, GPU_TEVOP_RGB_ONE_MINUS_SRC_ALPHA);
    C3D_TexEnvFunc(env5, C3D_RGB, GPU_INTERPOLATE);
    C3D_TexEnvColor(env5, 0);
    return;
  }
  u32 pos = (u32)clamp255(r) | (u32)clamp255(g) << 8 | (u32)clamp255(b) << 16;
  u32 neg = (u32)clamp255(-r) | (u32)clamp255(-g) << 8 | (u32)clamp255(-b) << 16;
  C3D_TexEnvSrc(env, C3D_RGB, GPU_PREVIOUS, GPU_CONSTANT, 0);
  C3D_TexEnvFunc(env, C3D_RGB, GPU_ADD);
  C3D_TexEnvColor(env, pos);
  C3D_TexEnvSrc(env5, C3D_RGB, GPU_PREVIOUS, GPU_CONSTANT, 0);
  C3D_TexEnvFunc(env5, C3D_RGB, GPU_SUBTRACT);
  C3D_TexEnvColor(env5, neg);
}

void rr_frame_end(void) {
  for (rr_tex* t = g_live; t; t = t->next) {
    if (t->dirty) {
      C3D_TexFlush(&t->tex);
      t->dirty = false;
    }
  }
  C3D_FrameEnd(0);
  g_cpu_ms = C3D_GetProcessingTime();
  g_gpu_ms = C3D_GetDrawingTime();
}

static inline u32 to_c2d(rr_color c) { return __builtin_bswap32(c); }

typedef enum { TINT_MULT, TINT_SOLID } tint_kind;
static C2D_TintMode g_tint_mode = C2D_TintSolid; /* citro2d's default */

static void draw_sub_k(rr_tex* t, int sx, int sy, int sw, int sh, float px, float py, float cx, float cy,
                       float scale_x, float scale_y, float angle, rr_color tint, tint_kind kind) {
  if (sw <= 0 || sh <= 0) return;
  float tw = t->tex.width, th = t->tex.height;
  Tex3DS_SubTexture sub = {
      (u16)sw, (u16)sh, sx / tw, 1.0f - sy / th, (sx + sw) / tw, 1.0f - (sy + sh) / th,
  };
  C2D_Image img = {&t->tex, &sub};
  C2D_DrawParams p = {{px + g_origin[g_cur][0], py + g_origin[g_cur][1], sw * scale_x, sh * scale_y},
                      {cx, cy}, 0.0f, angle};
  if (angle == 0.0f && fabsf(scale_x) == 1.0f && fabsf(scale_y) == 1.0f) {
    /* keep pixel art on the pixel grid */
    p.pos.x = floorf(p.pos.x + 0.5f);
    p.pos.y = floorf(p.pos.y + 0.5f);
    p.center.x = floorf(p.center.x + 0.5f);
    p.center.y = floorf(p.center.y + 0.5f);
  }
  if (kind == TINT_MULT && tint == 0xFFFFFFFFu) {
    C2D_DrawImage(img, &p, NULL);
    return;
  }
  C2D_ImageTint ti;
  if (kind == TINT_MULT && (tint | 0xFFu) == 0xFFFFFFFFu) {
    C2D_AlphaImageTint(&ti, (tint & 0xFF) / 255.0f);
  } else {
    C2D_TintMode want = kind == TINT_SOLID ? C2D_TintSolid : C2D_TintMult;
    if (want != g_tint_mode) {
      C2D_SetTintMode(want);
      g_tint_mode = want;
    }
    C2D_PlainImageTint(&ti, to_c2d(tint), 1.0f);
  }
  C2D_DrawImage(img, &p, &ti);
}

static void draw_sub(rr_tex* t, int sx, int sy, int sw, int sh, float px, float py, float cx, float cy,
                     float scale_x, float scale_y, float angle, rr_color tint) {
  draw_sub_k(t, sx, sy, sw, sh, px, py, cx, cy, scale_x, scale_y, angle, tint, TINT_MULT);
}

static void draw_frame_region(const rr_frame* f, float x, float y, int cw, int ch, const rr_draw_opts* o) {
  if (!f || !f->tex) return;
  if (!o) o = &RR_DEFAULT_OPTS;
  /* visible part of the untrimmed frame is [0,cw)x[0,ch); intersect with the trimmed rect */
  int x0 = f->off_x, y0 = f->off_y;
  int x1 = f->off_x + f->w, y1 = f->off_y + f->h;
  if (x1 > cw) x1 = cw;
  if (y1 > ch) y1 = ch;
  if (x1 <= x0 || y1 <= y0) return;
  float cx = (o->origin_x * f->src_w - x0) * o->scale_x;
  float cy = (o->origin_y * f->src_h - y0) * o->scale_y;
  draw_sub(f->tex, f->x, f->y, x1 - x0, y1 - y0, x, y, cx, cy, o->scale_x, o->scale_y, o->angle, o->tint);
}

void rr_draw(const rr_frame* f, float x, float y, const rr_draw_opts* o) {
  if (!f) return;
  draw_frame_region(f, x, y, f->src_w, f->src_h, o);
}

void rr_draw_cropped(const rr_frame* f, float x, float y, int w, int h, const rr_draw_opts* o) {
  draw_frame_region(f, x, y, w, h, o);
}

void rr_rect(float x, float y, float w, float h, rr_color c) {
  C2D_DrawRectSolid(x + g_origin[g_cur][0], y + g_origin[g_cur][1], 0.0f, w, h, to_c2d(c));
}

void rr_nineslice(const rr_frame* f, float x, float y, float w, float h, int b) {
  if (!f || !f->tex) return;
  int fw = f->w, fh = f->h, sx = f->x, sy = f->y;
  int mw = fw - 2 * b, mh = fh - 2 * b;
  float iw = w - 2 * b, ih = h - 2 * b;
  if (iw < 0) iw = 0;
  if (ih < 0) ih = 0;
  const int cols[3][2] = {{0, b}, {b, mw}, {b + mw, b}};
  const int rows[3][2] = {{0, b}, {b, mh}, {b + mh, b}};
  const float dx[3] = {x, x + b, x + b + iw}, dw[3] = {b, iw, b};
  const float dy[3] = {y, y + b, y + b + ih}, dh[3] = {b, ih, b};
  for (int r = 0; r < 3; r++) {
    for (int c = 0; c < 3; c++) {
      if (dw[c] <= 0 || dh[r] <= 0) continue;
      float scx = dw[c] / cols[c][1], scy = dh[r] / rows[r][1];
      draw_sub(f->tex, sx + cols[c][0], sy + rows[r][0], cols[c][1], rows[r][1], dx[c], dy[r], 0, 0, scx,
               scy, 0.0f, 0xFFFFFFFFu);
    }
  }
}

/* ------------------------------------------------------------------ */
/* text                                                                 */

#define GLYPH_TEX 512
#define GLYPH_SLOTS 509 /* prime-sized open-addressing table */

typedef struct {
  u32 cp; /* 0 = empty slot */
  s16 x, y, w, h;
  s16 left, top; /* bitmap offset from pen / baseline */
  s16 adv;
} glyph;

typedef struct {
  u32 cp;
  u16 page, x, y;
  u8 w, h;
  s8 off_x, off_y, adv;
  u8 pad;
} __attribute__((packed)) baked_glyph;

typedef struct {
  u32 first, last;
  rr_atlas* tex; /* NULL until used */
  baked_glyph* glyphs;
  int count;
  bool failed;
} baked_page;

struct rr_font {
  /* baked (font.pak) fonts */
  bool baked;
  char key[64];
  int npages;
  baked_page* pages;
  FT_Face face;
  rr_tex* tex;
  int size, ascender;
  int shelf_x, shelf_y, shelf_h;
  glyph slots[GLYPH_SLOTS];
};

static FT_Library g_ft;

rr_font* rr_font_load(const char* path, int pixel_size) {
  if (!g_ft && FT_Init_FreeType(&g_ft)) return NULL;
  rr_font* f = calloc(1, sizeof *f);
  /* FT_New_Face streams from the file instead of holding it in RAM: the
   * upstream font is 5 MB (it carries CJK glyphs). */
  if (!f || FT_New_Face(g_ft, path, 0, &f->face)) {
    free(f);
    return NULL;
  }
  FT_Set_Pixel_Sizes(f->face, 0, pixel_size);
  f->size = pixel_size;
  f->ascender = (int)(f->face->size->metrics.ascender >> 6);
  f->tex = tex_create(GLYPH_TEX, GLYPH_TEX / 2, GPU_A8);
  return f;
}

rr_font* rr_font_load_key(const char* key) {
  size_t len;
  u8* d = pak_read(key, &len);
  if (!d) return NULL;
  rr_font* f = NULL;
  if (len >= 20 && !memcmp(d, "RFN1", 4)) {
    u16 size, npages;
    s16 ascent;
    memcpy(&size, d + 4, 2);
    memcpy(&ascent, d + 6, 2);
    memcpy(&npages, d + 16, 2);
    if (20 + (size_t)npages * 8 <= len && (f = calloc(1, sizeof *f))) {
      f->baked = true;
      snprintf(f->key, sizeof f->key, "%s", key);
      f->size = size;
      f->ascender = ascent;
      f->npages = npages;
      f->pages = calloc(npages ? npages : 1, sizeof *f->pages);
      for (int i = 0; i < npages; i++) {
        memcpy(&f->pages[i].first, d + 20 + i * 8, 4);
        memcpy(&f->pages[i].last, d + 24 + i * 8, 4);
      }
    }
  }
  free(d);
  return f;
}

static const baked_glyph* baked_find(rr_font* f, u32 cp, rr_atlas** tex) {
  int lo = 0, hi = f->npages - 1, p = -1;
  while (lo <= hi) {
    int mid = (lo + hi) / 2;
    if (cp < f->pages[mid].first)
      hi = mid - 1;
    else if (cp > f->pages[mid].last)
      lo = mid + 1;
    else {
      p = mid;
      break;
    }
  }
  if (p < 0) return NULL;
  baked_page* pg = &f->pages[p];
  if (!pg->tex && !pg->failed) {
    char k[96];
    size_t len;
    snprintf(k, sizeof k, "%s/%d", f->key, p);
    pg->tex = rr_atlas_load_key(k);
    snprintf(k, sizeof k, "%s/%d.glyphs", f->key, p);
    pg->glyphs = pak_read(k, &len);
    pg->count = pg->glyphs ? (int)(len / sizeof(baked_glyph)) : 0;
    if (!pg->tex || !pg->glyphs) pg->failed = true;
  }
  if (pg->failed) return NULL;
  lo = 0;
  hi = pg->count - 1;
  while (lo <= hi) {
    int mid = (lo + hi) / 2;
    if (pg->glyphs[mid].cp < cp)
      lo = mid + 1;
    else if (pg->glyphs[mid].cp > cp)
      hi = mid - 1;
    else {
      *tex = pg->tex;
      return &pg->glyphs[mid];
    }
  }
  return NULL;
}

void rr_font_free(rr_font* f) {
  if (!f) return;
  if (f->baked) {
    for (int i = 0; i < f->npages; i++) {
      rr_atlas_free(f->pages[i].tex);
      free(f->pages[i].glyphs);
    }
    free(f->pages);
    free(f);
    return;
  }
  FT_Done_Face(f->face);
  rr_tex_free(f->tex);
  free(f);
}

static glyph* font_glyph(rr_font* f, u32 cp) {
  u32 h = cp % GLYPH_SLOTS;
  for (int probe = 0; probe < GLYPH_SLOTS; probe++, h = (h + 1) % GLYPH_SLOTS) {
    glyph* g = &f->slots[h];
    if (g->cp == cp) return g;
    if (g->cp != 0) continue;
    /* rasterize: mono for a crisp pixel font */
    if (FT_Load_Char(f->face, cp, FT_LOAD_RENDER | FT_LOAD_TARGET_MONO | FT_LOAD_MONOCHROME)) return NULL;
    FT_GlyphSlot s = f->face->glyph;
    FT_Bitmap* bm = &s->bitmap;
    int w = bm->width, gh = bm->rows;
    if (f->shelf_x + w + 1 > (int)f->tex->tex.width) {
      f->shelf_x = 0;
      f->shelf_y += f->shelf_h + 1;
      f->shelf_h = 0;
    }
    if (f->shelf_y + gh > (int)f->tex->tex.height) return NULL; /* cache full; TODO evict */
    g->cp = cp;
    g->x = f->shelf_x;
    g->y = f->shelf_y;
    g->w = w;
    g->h = gh;
    g->left = s->bitmap_left;
    g->top = s->bitmap_top;
    g->adv = (s16)(s->advance.x >> 6);
    u8* texels = f->tex->tex.data;
    for (int y = 0; y < gh; y++) {
      for (int x = 0; x < w; x++) {
        u8 v;
        if (bm->pixel_mode == FT_PIXEL_MODE_MONO)
          v = (bm->buffer[y * bm->pitch + (x >> 3)] & (0x80 >> (x & 7))) ? 255 : 0;
        else
          v = bm->buffer[y * bm->pitch + x];
        texels[texel_index(g->x + x, g->y + y, f->tex->tex.width, f->tex->tex.height)] = v;
      }
    }
    f->tex->dirty = true;
    f->shelf_x += w + 1;
    if (gh > f->shelf_h) f->shelf_h = gh;
    return g;
  }
  return NULL;
}

static u32 utf8_next(const char** s) {
  const u8* p = (const u8*)*s;
  u32 c = *p++;
  if (c >= 0xF0 && p[0] && p[1] && p[2]) {
    c = ((c & 7) << 18) | ((p[0] & 63) << 12) | ((p[1] & 63) << 6) | (p[2] & 63);
    p += 3;
  } else if (c >= 0xE0 && p[0] && p[1]) {
    c = ((c & 15) << 12) | ((p[0] & 63) << 6) | (p[1] & 63);
    p += 2;
  } else if (c >= 0xC0 && p[0]) {
    c = ((c & 31) << 6) | (p[0] & 63);
    p += 1;
  }
  *s = (const char*)p;
  return c;
}

static int text_pass(rr_font* f, float x, float y, const char* s, rr_color c, bool draw) {
  float pen = 0;
  float line = 0;
  if (f->baked) {
    while (*s) {
      u32 cp = utf8_next(&s);
      if (cp == '\n') {
        pen = 0;
        line += f->size;
        continue;
      }
      rr_atlas* a = NULL;
      const baked_glyph* g = baked_find(f, cp, &a);
      if (!g) continue;
      if (draw && g->w > 0 && a)
        draw_sub_k(rr_atlas_page(a, 0), g->x, g->y, g->w, g->h, x + pen + g->off_x, y + line + g->off_y, 0, 0, 1,
                   1, 0, c, TINT_SOLID);
      pen += g->adv;
    }
    return (int)pen;
  }
  while (*s) {
    u32 cp = utf8_next(&s);
    if (cp == '\n') {
      pen = 0;
      line += f->size;
      continue;
    }
    glyph* g = font_glyph(f, cp);
    if (!g) continue;
    if (draw && g->w > 0)
      draw_sub_k(f->tex, g->x, g->y, g->w, g->h, x + pen + g->left, y + line + f->ascender - g->top, 0, 0, 1,
                 1, 0, c, TINT_SOLID);
    pen += g->adv;
  }
  return (int)pen;
}

int rr_text(rr_font* f, float x, float y, const char* s, rr_color c, rr_color shadow) {
  if (!f) {
    rr_text_sys(x, y, 0.5f, s, c);
    return 0;
  }
  if (shadow & 0xFF) text_pass(f, x + 1, y + 1, s, shadow, true);
  return text_pass(f, x, y, s, c, true);
}

int rr_text_width(rr_font* f, const char* s) { return f ? text_pass(f, 0, 0, s, 0, false) : 0; }

int rr_font_size(const rr_font* f) { return f ? f->size : 0; }
int rr_font_ascent(const rr_font* f) { return f ? f->ascender : 0; }

void rr_text_sys(float x, float y, float size, const char* s, rr_color c) {
  if (g_tint_mode != C2D_TintSolid) {
    C2D_SetTintMode(C2D_TintSolid);
    g_tint_mode = C2D_TintSolid;
  }
  C2D_Text t;
  C2D_TextParse(&t, g_sysbuf, s);
  C2D_TextOptimize(&t);
  C2D_DrawText(&t, C2D_WithColor, x + g_origin[g_cur][0], y + g_origin[g_cur][1], 0.0f, size, size,
               to_c2d(c));
}

/* ------------------------------------------------------------------ */

rr_stats rr_get_stats(void) {
  rr_stats s = {0};
  for (rr_tex* t = g_live; t; t = t->next) {
    s.tex_bytes += t->bytes;
    s.tex_count++;
  }
  s.linear_free = linearSpaceFree();
  s.vram_free = vramSpaceFree();
  s.cpu_ms = g_cpu_ms;
  s.gpu_ms = g_gpu_ms;
  return s;
}
