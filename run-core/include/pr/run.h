/* run-core: the run around the battles (waves now; the shop and rewards next).
 * Ported from upstream src/game-mode.ts, src/battle.ts and src/battle-scene.ts; function
 * names follow upstream so each one diffs against its TS original. It reads battle-core's
 * RNG, V8 Math.pow and generated tables and never changes battle-core. */
#ifndef PR_RUN_H
#define PR_RUN_H
#include <stdbool.h>
#include <stdint.h>
#include "pr/rng.h"
#include "pr_data.h"   /* battle-core's generated tables: GameModes, BiomeId, species */

/* Mode rules the port doesn't model yet. A function that meets one sets its bit in
 * run->unported and returns a placeholder, so a caller (or the test) never takes a guess
 * for upstream's answer. */
enum {
    PR_RUN_UNPORTED_DAILY = 1u << 0,      /* the daily run's seed config (trainer waves, money, final boss) */
    PR_RUN_UNPORTED_CHALLENGES = 1u << 1, /* challenge hooks (FIXED_BATTLES...) */
};

typedef struct pr_run {
    int mode;                  /* gameMode.modeId (GameModes) */
    bool has_challenges;       /* gameMode.challenges has an active challenge */
    pr_seedstr seed;           /* globalScene.seed: the run seed */
    pr_seedstr wave_seed;      /* globalScene.waveSeed */
    pr_rng rng;                /* Phaser.Math.RND */
    bool offset_gym;           /* globalScene.offsetGym */
    unsigned unported;
} pr_run;

/* ---- GameMode (src/game-mode.ts) ---- */
int pr_run_get_starting_level(pr_run *run);
int pr_run_get_starting_money(pr_run *run);
int pr_run_get_wave_for_difficulty(const pr_run *run, int wave, bool ignore_curve_changes);
bool pr_run_is_wave_final(int mode, int wave);
bool pr_run_is_boss(int wave);
bool pr_run_is_battle_classic_final_boss(const pr_run *run, int wave);
bool pr_run_is_endless_boss(const pr_run *run, int wave);
bool pr_run_is_endless_minor_boss(const pr_run *run, int wave);
bool pr_run_is_endless_major_boss(const pr_run *run, int wave);
bool pr_run_is_trainer_boss(const pr_run *run, int wave, int biome, bool offset_gym);
bool pr_run_is_fixed_battle(pr_run *run, int wave);
int pr_run_get_clear_score_bonus(const pr_run *run);
int pr_run_get_enemy_modifier_chance(const pr_run *run, bool is_boss);
/* isWaveTrainer(wave), with arena.trainerChance of the current biome. The final roll uses
 * run->rng (Phaser.Math.RND, as newBattle leaves it after resetSeed); the checks of earlier
 * waves use executeWithSeedOffset on the run seed and leave run->rng untouched. */
bool pr_run_is_wave_trainer(pr_run *run, int wave, int trainer_chance);

/* ---- BattleScene (src/battle-scene.ts) ---- */
/* resetSeed(wave): waveSeed = shiftCharCodes(seed, wave); Phaser.Math.RND.sow([waveSeed]) */
void pr_run_reset_seed(pr_run *run, int wave);
/* getMaxExpLevel(ignoreLevelCap) with currentBattle.waveIndex = wave (0 = no battle yet) */
double pr_run_get_max_exp_level(const pr_run *run, int wave, bool ignore_level_cap);
/* getEncounterBossSegments(wave, level, species, forceBoss); species = SpeciesId, or -1 for none */
int pr_run_get_encounter_boss_segments(pr_run *run, int wave, int level, int species, bool force_boss);
/* getDoubleBattleChance(wave): x1/4 for each Lure-type modifier the player holds and for each
 * field Pokemon whose ability applies DoubleBattleChanceAbAttr (the caller counts those
 * through battle-core's modifier lists and ability dispatch) */
double pr_run_get_double_battle_chance(int wave, int n_lure_modifiers, int n_double_chance_abilities);

/* ---- Battle (src/battle.ts) ---- */
/* getLevelForWave() for a battle of `wave`, rolling on `rng` (newBattle builds the Battle inside
 * executeWithSeedOffset(waveIndex << 3, waveSeed), so pass pr_run_battle_rng's result) */
int pr_run_get_level_for_wave(pr_run *run, pr_rng *rng, int wave);
/* The Phaser.Math.RND a new Battle is built with: sown from shiftCharCodes(waveSeed, wave << 3) */
pr_rng pr_run_battle_rng(const pr_run *run, int wave);

/* ---- helpers ---- */
/* executeWithSeedOffset(fn, offset, seedOverride): fn rolls on a fresh RNG sown from
 * shiftCharCodes(seedOverride || seed, offset), and RND's state comes back afterwards, so a
 * copy stands in for it. seed_override NULL = the run seed. */
pr_rng pr_run_seed_offset_rng(const pr_run *run, int offset, const pr_seedstr *seed_override);
double js_round(double x);   /* battle-core */
double js_pow(double x, double y);

#endif
