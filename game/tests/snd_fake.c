#include "snd_fake.h"

#include <stdio.h>
#include <string.h>

#include "snd.h"

snd_fake_state SND = {.ready = true, .volume = 1.0f};

void snd_fake_reset(void) {
  bool (*has)(const char*) = SND.has;
  void (*on_start)(const char*) = SND.on_start;
  memset(&SND, 0, sizeof SND);
  SND.ready = true;
  SND.volume = 1.0f;
  SND.has = has;
  SND.on_start = on_start;
}

bool snd_ready(void) { return SND.ready; }

/* as snd.c: a key the pack lacks leaves the stream that was playing alone and returns false; otherwise the old stream is gone and the
 * new one plays at once */
bool snd_bgm_play(const char* key, bool loop) {
  (void)loop;
  if (SND.on_start) SND.on_start(key);
  if (!SND.ready) return false;
  if (SND.has && !SND.has(key)) {
    SND.refused++;
    return false;
  }
  snprintf(SND.key, sizeof SND.key, "%s", key);
  SND.starts++;
  return true;
}

void snd_bgm_stop(void) {
  SND.key[0] = 0;
  SND.stops++;
}

void snd_bgm_volume(float v) { SND.volume = v; }
