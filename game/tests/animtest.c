/* Host check of the battle animation player (battle_anim.c): plays every move, common and charge
 * animation upstream has, from both sides, with the SD card, images, sound and drawing stubbed
 * out. Reports each one's length and how many sprites it drew. Build with ASan to catch memory
 * errors: make animtest && build/animtest path/to/pokerogue-assets */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "battle_anim.h"
#include "gm_enum_names.h"
#include "pr_data.h"
#include "pr_enums.h"

static const char* g_assets;
static int g_draws, g_sounds;

void* pak_read(const char* key, size_t* len) {
  char p[512];
  snprintf(p, sizeof p, "%s/%s", g_assets, key);
  FILE* f = fopen(p, "rb");
  if (!f) return NULL;
  fseek(f, 0, SEEK_END);
  long n = ftell(f);
  fseek(f, 0, SEEK_SET);
  char* d = malloc(n + 1);
  *len = fread(d, 1, n, f);
  d[*len] = 0;
  fclose(f);
  return d;
}
/* images: one fake frame for everything */
static rr_frame g_frame = {NULL, 0, 0, 40, 40, 0, 0, 96, 96};
rr_atlas* img_get(const char* key) { return (rr_atlas*)&g_frame; }
const rr_frame* img_frame(const char* key, const char* frame) { return &g_frame; }
void img_drop_prefix(const char* prefix) {}
bool snd_sfx_preload(const char* key) { return true; }
bool snd_sfx_play_ex(const char* key, float v, float r) { g_sounds++; return true; }
float snd_sfx_seconds(const char* key) { return 0.5f; }
void rr_set_blend(rr_blend b) {}
void rr_set_tone(int r, int g, int b) {}
void rr_draw(const rr_frame* f, float x, float y, const rr_draw_opts* o) { g_draws++; }
void rr_rect(float x, float y, float w, float h, rr_color c) { g_draws++; }
const rr_draw_opts RR_DEFAULT_OPTS = {0, 0, 1, 1, 0, 0xFFFFFFFFu};

static ba_mon g_field[4];

static float play(ba_kind kind, int id, int user, int target) {
  g_draws = g_sounds = 0;
  if (!ba_play(kind, id, user, target, g_field)) return -1;
  float t = 0;
  while (ba_active() && t < 120000) {
    ba_update(1000.0f / 60, g_field);
    for (int l = 0; l < 3; l++) ba_draw((ba_layer)l, g_field);
    t += 1000.0f / 60;
  }
  return t;
}

int main(int argc, char** argv) {
  if (argc < 2) return fprintf(stderr, "usage: animtest assets-dir\n"), 2;
  g_assets = argv[1];
  g_field[0] = (ba_mon){true, true, 106, 148, 30, &g_frame, 257};
  g_field[2] = (ba_mon){true, false, 236, 84, 25, &g_frame, 445};
  int n = 0, none = 0;
  float longest = 0;
  int longest_id = 0;
  for (int m = 1; m < PR_MOVE_COUNT; m++) {
    for (int side = 0; side < 2; side++) {
      float t = play(BA_MOVE, m, side ? 2 : 0, side ? 0 : 2);
      if (t < 0) none++;
      else n++;
      if (t > longest) longest = t, longest_id = m;
      if (m == MOVE_ID_TACKLE || m == MOVE_ID_EARTHQUAKE || m == MOVE_ID_FLAMETHROWER || m == MOVE_ID_SWORDS_DANCE)
        printf("%-14s side %d: %6.0f ms, %4d draws, %d sounds\n", gm_enum_name(GM_E_MOVE_ID, m), side, t, g_draws,
               g_sounds);
    }
    if (!(m % 100)) ba_drop_moves();
  }
  printf("moves: %d played, %d not, longest %s %.0f ms\n", n, none, gm_enum_name(GM_E_MOVE_ID, longest_id), longest);
  for (int c = 2000; c < 2200; c++) {
    if (!gm_enum_name(GM_E_COMMON_ANIM, c)) continue;
    float t = play(BA_COMMON, c, 0, -1);
    if (t < 0 || c < 2003) printf("common %s: %.0f ms, %d draws\n", gm_enum_name(GM_E_COMMON_ANIM, c), t, g_draws);
  }
  for (int c = 1000; c < 1100; c++) {
    if (!gm_enum_name(GM_E_CHARGE_ANIM, c)) continue;
    float t = play(BA_CHARGE, c, 2, 0);
    printf("charge %s: %.0f ms, %d draws\n", gm_enum_name(GM_E_CHARGE_ANIM, c), t, g_draws);
  }
  ba_drop_moves();
  return 0;
}
