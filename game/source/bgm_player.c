/* The music player (see bgm.h): bgm_play and its fade over assets-pipeline/app-src/snd.c. */
#include <stdio.h>
#include <string.h>

#include "bgm.h"
#include "bgm_choose.h"
#include "snd.h"

static struct {
  char name[BGM_NAME_LEN]; /* the track started ("" = none) */
  bool fading;
  float fade_t, fade_ms;
} P;

const char* bgm_current(void) { return P.name; }

void bgm_stop(void) {
  P.fading = false;
  P.name[0] = 0;
  snd_bgm_stop();
  snd_bgm_volume(1.0f);
}

void bgm_play(const char* name) {
  if (!name || !*name) {
    bgm_stop();
    return;
  }
  if (!strcmp(P.name, name)) {
    if (P.fading) { /* asked for again while it fades out: it stays */
      P.fading = false;
      snd_bgm_volume(1.0f);
    }
    return;
  }
  P.fading = false;
  P.name[0] = 0;
  snd_bgm_volume(1.0f);
  if (!snd_ready()) return;
  char key[BGM_NAME_LEN + 16];
  snprintf(key, sizeof key, "audio/bgm/%s", name);
  if (snd_bgm_play(key, true))
    snprintf(P.name, sizeof P.name, "%s", name);
  else
    snd_bgm_stop(); /* not in the pack: silence, not the last scene's music */
}

void bgm_fade_out(float ms) {
  if (!P.name[0]) return;
  if (ms <= 0) {
    bgm_stop();
    return;
  }
  P.fading = true;
  P.fade_t = 0;
  P.fade_ms = ms;
}

void bgm_tick(float dt_ms) {
  if (!P.fading) return;
  P.fade_t += dt_ms;
  if (P.fade_t >= P.fade_ms) {
    bgm_stop();
    return;
  }
  snd_bgm_volume(1.0f - P.fade_t / P.fade_ms);
}
