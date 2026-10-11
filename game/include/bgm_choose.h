/*
 * bgm_choose: which music upstream plays where. Pure: it picks a track name ("battle_wild", "bw/victory_trainer"), it plays
 * nothing, so the host tests can check every answer (game/tests/bgmtest.c does, against what upstream's own code answered).
 *
 * Ported from the pinned upstream (8555c08):
 *   - Battle.getBgmOverride (src/battle.ts:246): trainer battles (the encounter music before the battle starts, then the
 *     trainer's battle music), the Classic end waves, the final boss, and the legendary / sub-legendary / mythical / Paradox
 *     wild Pokemon (both music preferences; the default, and the only one the app has, is "all generations")
 *   - the default track of AudioManager.playBgm (src/audio/audio-manager.ts:100): the override, else the biome's music
 *     (Arena.bgm: the biome's lower-case name, src/field/arena.ts:97 and getBiomeKey)
 *   - the trainer table (game/gen/gm_bgm_data.c, from trainerConfigs): battleBgm, mixedBattleBgm, victoryBgm, the encounter
 *     music of each TrainerVariant and whether it has encounter dialogue (the music needs it)
 *   - the title music (src/phases/title-phase.ts:42): winter_title in December and January 1-15, else title
 *
 * Not ported: the timed events' music replacements (timedEventManager.getEventBgmReplacement; timed events are not in the app),
 * the mystery encounters' own tracks (their encounters are stand-ins), the Classic final boss's second stage (not in game-core),
 * the Daily/Endless/Challenge modes. Loop points are not here: the track in bgm.pak carries upstream's.
 *
 * Species whose category starts with "Paradox" are the ones the pinned English data names so; upstream tests the localized
 * category text, so in another language its check (and with it a Paradox Pokemon's music) differs. The app follows English.
 */
#ifndef GM_BGM_CHOOSE_H
#define GM_BGM_CHOOSE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BGM_NAME_LEN 64

/* One TrainerType's music (generated: game/gen/gm_bgm_data.c; NULL = none) */
typedef struct {
  uint16_t trainer_type;
  const char *battle, *mixed, *victory;
  const char *encounter[3]; /* Trainer.getEncounterBgm() for TrainerVariant DEFAULT, FEMALE, DOUBLE */
  uint8_t has_encounter;    /* config.encounterBgm is set */
  uint8_t has_messages;     /* bit per TrainerVariant: getEncounterMessages().length > 0 */
} gm_trainer_bgm;
extern const gm_trainer_bgm gm_trainer_bgms[];
extern const int gm_trainer_bgm_count;
extern const uint16_t gm_paradox_species[];
extern const int gm_paradox_species_count;

/* What Battle.getBgmOverride looks at */
typedef struct {
  int wave;            /* Battle.waveIndex */
  int biome;           /* arena.biomeId (BiomeId) */
  int battle_type;     /* BattleType */
  bool me_default;     /* a mystery encounter still in its DEFAULT mode */
  bool started;        /* Battle.started: the trainer has sent out its first Pokemon */
  bool final_boss;     /* Battle.isClassicFinalBoss */
  bool all_gens;       /* settings.musicPreferenceAllGens (the default) */
  int trainer_type;    /* TrainerType; -1 = none */
  int trainer_variant; /* TrainerVariant */
  struct {
    uint16_t species;
    uint8_t form;
  } enemy[6]; /* globalScene.getEnemyParty(): the species and form of each */
  int n_enemy;
} bgm_scene;

/* Battle.getBgmOverride for a Classic run: the track, or NULL for no override (the biome's music plays). `buf` holds a name
 * that is made up ("encounter_<trainer>"). */
const char *bgm_battle_override(const bgm_scene *s, char buf[BGM_NAME_LEN]);
/* Arena.bgm: the biome's music */
const char *bgm_arena_track(int biome, char buf[BGM_NAME_LEN]);
/* playBgm() with no name: the override, else the biome's music. Never NULL. */
const char *bgm_scene_track(const bgm_scene *s, char buf[BGM_NAME_LEN]);
/* The trainer's TrainerConfig.victoryBgm (TrainerVictoryPhase) */
const char *bgm_trainer_victory(int trainer_type);
/* The title screen's music for the date (month 0 = January, day 1-31) */
const char *bgm_title_track(int month, int day);
const gm_trainer_bgm *bgm_trainer_of(int trainer_type);

#ifdef __cplusplus
}
#endif
#endif
