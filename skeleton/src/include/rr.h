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
#define RR_RGBA(r, g, b, a) ((rr_color)(((r) << 24) | ((g) << 16) | ((b) << 8) | (a)))

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

/* ---- drawing (current target) ---- */
void rr_draw(const rr_frame* f, float x, float y, const rr_draw_opts* o);
/* Draws only the part of the frame inside [0,w)x[0,h) of the untrimmed frame
 * (used for HP/EXP bar fills, which upstream crops). */
void rr_draw_cropped(const rr_frame* f, float x, float y, int w, int h, const rr_draw_opts* o);
void rr_rect(float x, float y, float w, float h, rr_color c);
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
