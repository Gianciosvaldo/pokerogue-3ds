/* A fake DSP for the host tests: snd_fake.c stands in for assets-pipeline/app-src/source/snd.c and records what the music player asked of
 * it. */
#ifndef GM_SND_FAKE_H
#define GM_SND_FAKE_H

#include <stdbool.h>

typedef struct {
  bool ready;                   /* snd_ready(): the DSP firmware was found */
  char key[96];                 /* the stream playing ("" = none) */
  float volume;                 /* the last snd_bgm_volume() */
  int starts, stops, refused;   /* snd_bgm_play calls that started a stream / snd_bgm_stop calls / snd_bgm_play calls the pack refused */
  bool (*has)(const char* key); /* the keys the pack has; NULL = all of them */
  void (*on_start)(const char* key); /* called first in every snd_bgm_play (the SD card reads happen in there) */
} snd_fake_state;

extern snd_fake_state SND;
void snd_fake_reset(void);

#endif
