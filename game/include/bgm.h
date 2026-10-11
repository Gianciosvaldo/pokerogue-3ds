/*
 * bgm: the app's music. Which track a scene plays is upstream's (bgm_choose.h); this plays it from bgm.pak on the DSP
 * (assets-pipeline/app-src/snd.h) and says, scene by scene, when to switch.
 *
 * Upstream's AudioManager (src/audio/audio-manager.ts) keeps one current track: playBgm(name) with the track already
 * playing does nothing, otherwise the old track ends and the new one starts, looping from its loop point (the pack's track
 * carries it). The same here, with one difference the console forces: a track starts with a short run of SD card reads on
 * the main thread (the file, its seek table and about 1.7 s of audio), so a track is only ever started where the screen is
 * black or a scene begins, never while something moves (see the scene calls below).
 *
 * Without bgm.pak, or without the DSP firmware, every call here is silent and does nothing else.
 */
#ifndef GM_BGM_H
#define GM_BGM_H

#include <stdbool.h>

#include "gc/run.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- the player (bgm_player.c) ---- */
/* AudioManager.playBgm(name): the pack's audio/bgm/<name>, looping. Nothing happens when it is the track already playing. A track
 * the pack lacks leaves silence (upstream's load failure destroys the old track too). NULL or "" stops the music. */
void bgm_play(const char* name);
void bgm_stop(void);
/* AudioManager.fadeOutBgm: the volume falls to 0 over `ms`, then the track is dropped */
void bgm_fade_out(float ms);
void bgm_tick(float dt_ms);    /* once per frame (ui_tick): runs the fade */
const char* bgm_current(void); /* the track playing ("" = none) */

/* ---- the scenes (bgm.c): each is called where upstream starts or changes the music ---- */
/* TitlePhase.start: title, or winter_title from December to January 15. Call as the title screen comes up (at black). */
void bgm_on_title(void);
/* SelectStarterPhase.start: menu. Call as the starter select comes up (at black). */
void bgm_on_starter_select(void);
/* EncounterPhase / NextEncounterPhase / NewBiomeEncounterPhase.doEncounter: playBgm(undefined, true), the battle's own music or the
 * biome's. `coming`: the scene is about to fade out (the old music fades with it, only when the track changes); then, at black,
 * `on_wave` starts the new one. battle_type is the WAVE event's BattleType. */
void bgm_on_wave_coming(const gc_run* run, int wave, int biome, int battle_type);
void bgm_on_wave(const gc_run* run, int wave, int biome, int battle_type);
/* A trainer sent out its first Pokemon: Battle.started becomes true, the encounter music gives way to the battle music
 * (EncounterPhase's doSummon). Harmless when the battle is not a trainer's or the music is already the battle's. */
void bgm_on_foe_sent_out(const gc_run* run);
/* TrainerVictoryPhase.start: the trainer's victory music (TrainerConfig.victoryBgm) */
void bgm_on_trainer_defeated(const gc_run* run);
/* GameOverPhase.handleGameOver: the music fades out, over 10 s after a win and 5 s after a loss */
void bgm_on_run_end(bool clear);

#ifdef __cplusplus
}
#endif
#endif
