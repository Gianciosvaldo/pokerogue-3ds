/*
 * snd: music streaming and sound effects on the 3DS DSP (ndsp), playing the
 * RSND (DSP-ADPCM) entries the asset pipeline writes to bgm.pak / snd.pak.
 *
 * Decoding is done by the DSP, so audio costs almost no ARM11 time. Music is
 * streamed by a small high-priority thread, so a long frame or a slow battle
 * turn on the main thread does not starve it (about 1.7 s of audio is queued).
 *
 * On a real console ndsp needs sdmc:/3ds/dspfirm.cdc (dumped once with the
 * DSP1 homebrew); without it snd_init() fails and everything here is a no-op.
 * Needs pak_mount_default() first.
 */
#ifndef SND_H
#define SND_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

bool snd_init(void);
void snd_exit(void);
bool snd_ready(void);

/* key e.g. "audio/bgm/battle_wild". loop: restart from the track's default
 * loop point (upstream's) when it ends. snd_bgm_play_from also takes a loop
 * point in seconds (negative = the track's default), which upstream passes
 * per biome. Starting a track stops the previous one. */
bool snd_bgm_play(const char* key, bool loop);
bool snd_bgm_play_from(const char* key, bool loop, float loop_seconds);
void snd_bgm_stop(void);
bool snd_bgm_playing(void);
void snd_bgm_volume(float v); /* 0..1 */

/* key e.g. "audio/se/select", "audio/cry/445", "audio/battle_anims/Absorb2".
 * Sounds are cached (LRU, about 1 MB) after first use. */
bool snd_sfx_play(const char* key);
bool snd_sfx_preload(const char* key);
void snd_sfx_volume(float v); /* 0..1 */
/* With a volume (0..1, times the sfx volume) and a playback rate (1 = as recorded; Phaser's
 * rate, which also changes the pitch). */
bool snd_sfx_play_ex(const char* key, float volume, float rate);
/* The sound's length in seconds at rate 1 (loads it into the cache), 0 when missing. */
float snd_sfx_seconds(const char* key);

/* Optional per-frame housekeeping (frees finished voices). Music does not depend on it. */
void snd_update(void);

#ifdef __cplusplus
}
#endif
#endif
