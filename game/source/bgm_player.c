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
  char pending[BGM_NAME_LEN]; /* bgm_play_faded: the track that starts when pend_ms has run ("" = none) */
  float pend_ms;
} P;

const char* bgm_current(void) { return P.name; }

static void stop_player(void) {
  P.fading = false;
  P.name[0] = 0;
  snd_bgm_stop();
  snd_bgm_volume(1.0f);
}

void bgm_stop(void) {
  P.pending[0] = 0;
  stop_player();
}

void bgm_play(const char* name) {
  P.pending[0] = 0;
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
  P.pending[0] = 0;   /* a fade of its own: any track that was to follow another fade is off */
  if (!P.name[0]) return;
  if (ms <= 0) {
    bgm_stop();
    return;
  }
  P.fading = true;
  P.fade_t = 0;
  P.fade_ms = ms;
}

void bgm_play_faded(const char* name, float fade_ms, float delay_ms) {
  if (!name || !*name) return;
  if (!strcmp(P.pending, name)) return;                    /* already on its way */
  if (!strcmp(P.name, name) && !P.fading) return;          /* playing */
  if (!P.name[0] || !snd_ready()) {                        /* nothing to fade: previous?.isPlaying is false */
    bgm_play(name);
    return;
  }
  bgm_fade_out(fade_ms);
  snprintf(P.pending, sizeof P.pending, "%s", name);
  P.pend_ms = delay_ms;
}

void bgm_tick(float dt_ms) {
  if (P.pending[0]) {
    P.pend_ms -= dt_ms;
    if (P.pend_ms <= 0) {
      char name[BGM_NAME_LEN];
      snprintf(name, sizeof name, "%s", P.pending);
      bgm_play(name); /* clears the wait */
    }
  }
  if (!P.fading) return;
  P.fade_t += dt_ms;
  if (P.fade_t >= P.fade_ms) {
    stop_player();   /* a track waiting behind this fade (bgm_play_faded) still starts */
    return;
  }
  snd_bgm_volume(1.0f - P.fade_t / P.fade_ms);
}
