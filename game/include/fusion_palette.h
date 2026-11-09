/* The colours of a fused Pokemon's sprite (upstream Pokemon.updateFusionPalette plus the sprite shader's fusion branch).
 *
 * A fusion is not a head-and-body mix: it is the first species' sprite, recoloured. updateFusionPalette collects the distinct colours of
 * the first frame of the first species' front and back sheets (spriteColors), quantizes those pixels and the fusion half's pixels to four
 * colours each (material-color-utilities' QuantizerCelebi: Wu, then weighted k-means in L*a*b*, seeded by the fixed string "This result
 * should not vary"), merges palette colours whose hue is within 30 degrees, and moves each sprite colour towards the fusion half's palette
 * colour nearest to it. The sprite shader then draws every pixel whose colour is one of the first 32 spriteColors as the overlay blend of
 * that colour's grey and its fusion colour. This file does the first part on RGBA pixels and folds the shader's blend in, so the result is
 * a list of colour pairs the app's recolouring loader (rr_atlas_load_rrt) already takes.
 *
 * Tested against the real updateFusionPalette in game/tests/fusion_palette_test.c (game/tests/fusion-palette.producer.test.ts). */
#ifndef FUSION_PALETTE_H
#define FUSION_PALETTE_H

#include <stddef.h>
#include <stdint.h>

#define FP_MAX_PAIRS 32   /* the shader reads the first 32 spriteColors */

/* One first frame, as RGBA bytes (w*h*4), row by row. `map`: the colour map of a shiny variant of a recoloured sheet, `n_map` pairs of
 * 0xRRGGBBAA (matched on RGB of fully opaque pixels); NULL/0 when the sheet is drawn as it is */
typedef struct {
  const uint8_t* rgba;
  int w, h;
} fp_image;

typedef struct {
  uint8_t from[FP_MAX_PAIRS][3];   /* spriteColors[i] (after the shiny variant's map) */
  uint8_t to[FP_MAX_PAIRS][3];     /* what the shader draws instead */
  uint8_t sprite_alpha[FP_MAX_PAIRS];
  uint8_t fusion[FP_MAX_PAIRS][3]; /* fusionSpriteColors[i] before the blend, for the tests */
  int n;                           /* pairs (at most FP_MAX_PAIRS); 0: the sprite stays as it is */
  int n_sprite_colors;             /* spriteColors.length, which can be more than the pairs */
} fp_result;

/* images[0] and [1]: the first species' front and back first frames, images[2] and [3]: the fusion half's. maps[0] and [1]: the shiny
 * variant colour maps of the first species' front and back sheets, maps[2] and [3]: the fusion half's (pairs `from` -> `to`, 0xRRGGBBAA,
 * NULL/0 for a sheet drawn as it is). The first species' map changes its sprite colours only (updateFusionPalette reads that half's
 * pixelColors unmapped); the fusion half's changes the pixels that go to the quantizer. Returns false when nothing is recoloured: the
 * fusion half has no visible pixel (upstream logs "Failed to create fusion palette") or the sprite has more distinct colours than the
 * port holds */
typedef struct {
  const uint32_t* from_to;   /* n pairs: from, to */
  int n;
} fp_map;

int fp_compute(const fp_image images[4], const fp_map maps[4], fp_result* out);

/* The first frame of an RRT sheet (assets-pipeline/gfx.py) as RGBA bytes; `rgba` must hold w*h*4 for the frame's trimmed size, which is
 * read first with rgba == NULL. False for a direct-colour sheet or a sheet with no frame */
int fp_rrt_first_frame(const void* rrt, size_t len, uint8_t* rgba, size_t cap, int* w, int* h);

#endif
