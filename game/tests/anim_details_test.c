/* Unit test for animation details (tone gray calculation and Substitute target selection).
 * Expected values derived directly from upstream's source code:
 * 1. tone gray formula: sprite-frag-shader.frag lines 218-222:
 *    luma = dot(color.rgb, vec3(.299, .587, .114))
 *    color.rgb = mix(color.rgb, vec3(luma), tone.w) + tone.rgb * (a / 255.0)
 * 2. Substitute target selection: battle-anims.ts lines 778 & 852:
 *    targetSubstitute = onSubstitute && user !== target ? target.getTag(BattlerTagType.SUBSTITUTE) : null
 */

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <string.h>

#include "battle_anim.h"
#include "pr_enums.h"

/* a move's animation with one graphic, one background and one sound, for the preload test */
static const char k_anim[] =
    "{\"graphic\":\"G\",\"frames\":[[]],\"frameTimedEvents\":{\"0\":[{\"eventType\":\"AnimTimedAddBgEvent\",\"resourceName\":\"BG\"},"
    "{\"eventType\":\"AnimTimedSoundEvent\",\"resourceName\":\"S.wav\"}]}}";
static bool g_serve_anim;
static int g_img_gets, g_sfx_preloads, g_drops;
static size_t g_linear_free = (size_t)1 << 30;
void* pak_read(const char* key, size_t* len) {
  if (!g_serve_anim) return NULL;
  *len = strlen(k_anim);
  return strdup(k_anim);
}
static rr_frame g_frame = {NULL, 0, 0, 40, 40, 0, 0, 96, 96};
rr_atlas* img_get(const char* key) { g_img_gets++; return (rr_atlas*)&g_frame; }
const rr_frame* img_frame(const char* key, const char* frame) { return &g_frame; }
void img_drop_prefix(const char* prefix) { if (!strcmp(prefix, "images/battle_anims/")) g_drops++; }
rr_stats rr_get_stats(void) { rr_stats s = {0}; s.linear_free = g_linear_free; return s; }
bool snd_sfx_preload(const char* key) { g_sfx_preloads++; return true; }
bool snd_sfx_play_ex(const char* key, float v, float r) { return true; }
float snd_sfx_seconds(const char* key) { return 0.5f; }
void rr_set_blend(rr_blend b) {}
void rr_set_tone(int r, int g, int b) {}
void rr_draw(const rr_frame* f, float x, float y, const rr_draw_opts* o) {}
void rr_rect(float x, float y, float w, float h, rr_color c) {}
const rr_draw_opts RR_DEFAULT_OPTS = {0, 0, 1, 1, 0, 0xFFFFFFFFu};

static void test_anim_get_target_is_substitute(void) {
  /* user != target, onSubstitute = true, target_has_substitute = true -> true */
  assert(anim_get_target_is_substitute(0, 2, true, true) == true);

  /* user == target (self move), onSubstitute = true, target_has_substitute = true -> false */
  assert(anim_get_target_is_substitute(2, 2, true, true) == false);

  /* onSubstitute = false -> false */
  assert(anim_get_target_is_substitute(0, 2, false, true) == false);

  /* target_has_substitute = false -> false */
  assert(anim_get_target_is_substitute(0, 2, true, false) == false);

  printf("test_anim_get_target_is_substitute PASSED\n");
}

static void test_anim_apply_tone_gray(void) {
  uint8_t out[3];

  /* Test case 1: tone = [0, 0, 0, 0] (no gray, no RGB offset) */
  int16_t tone0[4] = {0, 0, 0, 0};
  anim_apply_tone_gray(100, 150, 200, 255, tone0, out);
  assert(out[0] == 100);
  assert(out[1] == 150);
  assert(out[2] == 200);

  /* Test case 2: tone = [0, 0, 0, 255] (full gray / 100% luma desaturation)
   * r=100, g=150, b=200
   * luma = 0.299*100 + 0.587*150 + 0.114*200 = 29.9 + 88.05 + 22.8 = 140.75
   * rounded: 141 for r, g, b
   */
  int16_t tone_gray[4] = {0, 0, 0, 255};
  anim_apply_tone_gray(100, 150, 200, 255, tone_gray, out);
  assert(out[0] == 141);
  assert(out[1] == 141);
  assert(out[2] == 141);

  /* Test case 3: tone = [10, -20, 30, 128] (50% gray mix, with RGB tone offset)
   * r=100, g=150, b=200, a=255
   * luma = 140.75
   * tone_w = 128 / 255.0 = 0.50196078
   * mr = 100 + (140.75 - 100) * 0.50196078 + 10 * 1.0 = 100 + 20.4549 + 10 = 130.4549 -> 130
   * mg = 150 + (140.75 - 150) * 0.50196078 - 20 * 1.0 = 150 - 4.6431 - 20 = 125.3569 -> 125
   * mb = 200 + (140.75 - 200) * 0.50196078 + 30 * 1.0 = 200 - 29.7412 + 30 = 200.2588 -> 200
   */
  int16_t tone_mixed[4] = {10, -20, 30, 128};
  anim_apply_tone_gray(100, 150, 200, 255, tone_mixed, out);
  assert(out[0] == 130);
  assert(out[1] == 125);
  assert(out[2] == 200);

  printf("test_anim_apply_tone_gray PASSED\n");
}

/* Preloading the animations of a party's moves must leave the texture memory the Pokemon's sprites need: with 8 MB or more free it loads
 * the graphic, the background and the sound, and with less it loads nothing (they load when the move plays). */
static void test_preload_leaves_room(void) {
  g_serve_anim = true;
  g_linear_free = (size_t)64 << 20;
  g_img_gets = g_sfx_preloads = 0;
  ba_preload_move(MOVE_ID_TACKLE);
  assert(g_img_gets == 2 && g_sfx_preloads == 1);

  ba_drop_moves();
  g_linear_free = ((size_t)8 << 20) - 1;
  g_img_gets = g_sfx_preloads = 0;
  ba_preload_move(MOVE_ID_TACKLE);
  assert(g_img_gets == 0 && g_sfx_preloads == 0);

  g_linear_free = (size_t)8 << 20;
  ba_preload_move(MOVE_ID_TACKLE);
  assert(g_img_gets == 2 && g_sfx_preloads == 1);

  /* the idle animation images can be freed for a sprite, and are not while one plays */
  g_drops = 0;
  assert(!ba_active() && ba_release_graphics() && g_drops == 1);

  ba_drop_moves();
  g_serve_anim = false;
  printf("test_preload_leaves_room PASSED\n");
}

int main(void) {
  test_anim_get_target_is_substitute();
  test_anim_apply_tone_gray();
  test_preload_leaves_room();
  printf("All animation detail tests passed successfully.\n");
  return 0;
}
