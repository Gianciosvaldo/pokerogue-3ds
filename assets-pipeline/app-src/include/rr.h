/*
 * rr: a small 2D renderer for the PokeRogue 3DS port.
 *
 * Plain C API on purpose: the native battle core can call it directly, and a
 * QuickJS binding layer can wrap it later. Nothing here knows about Phaser.
 *
 * Coordinates are PokeRogue "logical" pixels (the game's 1920x1080 canvas / 6
 * = 320x180). Each screen has a logical origin, so the same numbers upstream
 * uses (e.g. player Pokemon at 106,148) can be passed straight through.
 * Drawing is 1:1 with nearest filtering: no scaling of pixel art by default.
 */
#ifndef RR_H
#define RR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum { RR_TOP = 0, RR_BOTTOM = 1 } rr_screen;

/* 0xRRGGBBAA */
typedef uint32_t rr_color;
#define RR_RGBA(r, g, b, a) ((rr_color)(((uint32_t)(r) << 24) | ((uint32_t)(g) << 16) | ((uint32_t)(b) << 8) | (uint32_t)(a)))

typedef struct rr_tex rr_tex;
typedef struct rr_atlas rr_atlas;
typedef struct rr_font rr_font;

/* A sub-rectangle of a texture, with Phaser/TexturePacker trim info. */
typedef struct {
  rr_tex* tex;
  int16_t x, y, w, h;       /* rect inside the texture */
  int16_t off_x, off_y;     /* spriteSourceSize.x/y (trim offset) */
  int16_t src_w, src_h;     /* sourceSize (untrimmed frame size) */
} rr_frame;

typedef struct {
  float origin_x, origin_y; /* 0..1 of the untrimmed frame, like Phaser setOrigin */
  float scale_x, scale_y;   /* 1 = native; negative flips */
  float angle;              /* radians */
  rr_color tint;            /* multiplied; RR_RGBA(255,255,255,255) = none */
} rr_draw_opts;

extern const rr_draw_opts RR_DEFAULT_OPTS; /* origin 0,0, scale 1, no tint */

/* ---- lifecycle ---- */
bool rr_init(void);
void rr_exit(void);

/* Where logical (0,0) lands on each physical screen (top 400x240, bottom 320x240). */
void rr_set_origin(rr_screen s, int px, int py);

/* One frame: begin, then any number of rr_target + draws, then end. */
void rr_frame_begin(void);
void rr_target(rr_screen s, rr_color clear);
void rr_frame_end(void);

/* ---- textures ---- */
/* Loads a PNG file (e.g. from sdmc:/). Texture is padded to a power of two.
 * Uses RGBA5551 when every pixel is fully opaque or fully transparent
 * (true for nearly all PokeRogue pixel art), RGBA8 otherwise. */
rr_tex* rr_tex_load_png(const char* path);
void rr_tex_free(rr_tex* t);
int rr_tex_width(const rr_tex* t);   /* image size, not padded size */
int rr_tex_height(const rr_tex* t);
size_t rr_tex_bytes(const rr_tex* t); /* GPU memory used */
rr_frame rr_tex_frame(rr_tex* t);     /* whole image as one frame */

/* ---- atlases (TexturePacker JSON, the format every upstream sprite sheet uses) ---- */
rr_atlas* rr_atlas_load(const char* png_path, const char* json_path);
void rr_atlas_free(rr_atlas* a);
int rr_atlas_count(const rr_atlas* a);
const rr_frame* rr_atlas_frame(const rr_atlas* a, int index);
const rr_frame* rr_atlas_find(const rr_atlas* a, const char* name); /* NULL if absent */
rr_tex* rr_atlas_tex(const rr_atlas* a);

/* ---- pak textures (RRT entries written by the asset pipeline) ---- */
/* Needs pak_mount_default() first. Keys are upstream paths without extension,
 * e.g. "images/pokemon/back/642-therian". Every image is an atlas: sheets keep
 * their frame names; plain images have one frame named "" (or one per tile
 * when the image was over 1024 px and had to be split: draw them all with
 * rr_draw_all). Sheets may span several GPU pages. */
typedef struct {
  uint32_t from, to; /* 0xRRGGBBAA; matched on RGB of visible colours, alpha kept */
} rr_recolor_pair;

rr_atlas* rr_atlas_load_rrt(const void* data, size_t len, const rr_recolor_pair* map, int n);
rr_atlas* rr_atlas_load_key(const char* key);
/* Shiny variant colours (upstream variant JSON maps, variant 0..2):
 * variant_map_key e.g. "images/pokemon/variant/back/6.json". Like upstream's
 * sprite shader, only the first 32 pairs apply and the first match wins. */
rr_atlas* rr_atlas_load_key_variant(const char* key, const char* variant_map_key, int variant);
/* Single-frame, single-page images as a plain texture (NULL otherwise). */
rr_tex* rr_tex_load_key(const char* key);
int rr_atlas_pages(const rr_atlas* a);
rr_tex* rr_atlas_page(const rr_atlas* a, int i);
/* Exact 8-bit palette of an indexed sheet (index 0 = transparent); 0 if direct colour. */
int rr_atlas_palette(const rr_atlas* a, const uint32_t** pal);
/* Draws every frame at the same position: reassembles a split image. */
void rr_draw_all(const rr_atlas* a, float x, float y, const rr_draw_opts* o);

/* ---- drawing (current target) ---- */
void rr_draw(const rr_frame* f, float x, float y, const rr_draw_opts* o);
/* Draws only the part of the frame inside [0,w)x[0,h) of the untrimmed frame
 * (used for HP/EXP bar fills, which upstream crops). */
void rr_draw_cropped(const rr_frame* f, float x, float y, int w, int h, const rr_draw_opts* o);
void rr_rect(float x, float y, float w, float h, rr_color c);
/* Blending for what is drawn next, until changed (reset to NORMAL at each rr_target):
 * NORMAL is alpha blending, ADD adds the source (times its alpha) to what is there
 * (Phaser's BlendModes.ADD). */
typedef enum { RR_BLEND_NORMAL, RR_BLEND_ADD } rr_blend;
void rr_set_blend(rr_blend b);
/* A signed colour (-255..255 per channel) added to every drawn pixel's RGB before blending,
 * until changed (reset to 0 at each rr_target): upstream's sprite shader "tone" (rgb part). */
void rr_set_tone(int r, int g, int b);
/* 9-slice window from a 24x24 upstream window image (ui/windows/window_N.png). */
void rr_nineslice(const rr_frame* f, float x, float y, float w, float h, int border);

/* ---- text ---- */
/* TTF/OTF via FreeType, rendered at pixel_size into a glyph cache texture.
 * Returns NULL if the file is missing. */
rr_font* rr_font_load(const char* path, int pixel_size);
void rr_font_free(rr_font* f);
/* Draws UTF-8 text with an optional 1px drop shadow (shadow alpha 0 = none).
 * Returns the advance width in logical pixels. */
int rr_text(rr_font* f, float x, float y, const char* utf8, rr_color c, rr_color shadow);
int rr_text_width(rr_font* f, const char* utf8);
/* Pre-rendered font from font.pak (asset pipeline), e.g. "fonts/emerald@16".
 * Pages (256x256 each) load on first use of a glyph in them. */
rr_font* rr_font_load_key(const char* key);
/* The font's pixel size and its ascent (baseline distance from the top of a line), in logical px. */
int rr_font_size(const rr_font* f);
int rr_font_ascent(const rr_font* f);
/* Fallback when no font file is on the SD card: the console's system font. */
void rr_text_sys(float x, float y, float size, const char* s, rr_color c);

/* ---- stats ---- */
typedef struct {
  size_t tex_bytes;       /* all live rr textures */
  int tex_count;
  size_t linear_free;     /* linear heap (GPU-visible) left */
  size_t vram_free;
  float cpu_ms;           /* last frame: CPU time spent building draw lists */
  float gpu_ms;           /* last frame: GPU time */
} rr_stats;
rr_stats rr_get_stats(void);

#ifdef __cplusplus
}
#endif
#endif
