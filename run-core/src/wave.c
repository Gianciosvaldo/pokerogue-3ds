/* Waves: which battle a wave is and how strong it is (src/game-mode.ts, src/battle.ts,
 * src/battle-scene.ts). The JS each function replaces is quoted above it. */
#include "pr/run.h"
#include <math.h>

static bool is_classic(int mode) { return mode == GAME_MODES_CLASSIC || mode == GAME_MODES_CHALLENGE; }
static bool is_endless(int mode) { return mode == GAME_MODES_ENDLESS || mode == GAME_MODES_SPLICED_ENDLESS; }

pr_rng pr_run_seed_offset_rng(const pr_run *run, int offset, const pr_seedstr *seed_override)
{
    const pr_seedstr *base = seed_override && seed_override->len ? seed_override : &run->seed;
    pr_seedstr s;
    pr_rng r;
    pr_seed_shift(&s, base, offset);
    pr_rng_sow(&r, &s);
    return r;
}

/* getStartingLevel(): DAILY ? 20 : 5 (the STARTING_LEVEL_OVERRIDE dev override isn't modelled) */
int pr_run_get_starting_level(pr_run *run)
{
    return run->mode == GAME_MODES_DAILY ? 20 : 5;
}

/* getStartingMoney(): DAILY ? getDailyStartingMoney() ?? 1000 : 1000 */
int pr_run_get_starting_money(pr_run *run)
{
    if (run->mode == GAME_MODES_DAILY) run->unported |= PR_RUN_UNPORTED_DAILY;
    return 1000;
}

/* getWaveForDifficulty(waveIndex, ignoreCurveChanges):
 * DAILY: waveIndex + 30 + (ignoreCurveChanges ? 0 : Math.floor(waveIndex / 5)); otherwise waveIndex */
int pr_run_get_wave_for_difficulty(const pr_run *run, int wave, bool ignore_curve_changes)
{
    if (run->mode == GAME_MODES_DAILY) return wave + 30 + (ignore_curve_changes ? 0 : wave / 5);
    return wave;
}

/* isWaveFinal: CLASSIC/CHALLENGE 200, ENDLESS/SPLICED every 250, DAILY 50 */
bool pr_run_is_wave_final(int mode, int wave)
{
    switch (mode) {
    case GAME_MODES_CLASSIC: case GAME_MODES_CHALLENGE: return wave == 200;
    case GAME_MODES_ENDLESS: case GAME_MODES_SPLICED_ENDLESS: return wave % 250 == 0;
    case GAME_MODES_DAILY: return wave == 50;
    }
    return false;
}

bool pr_run_is_boss(int wave) { return wave % 10 == 0; }

bool pr_run_is_battle_classic_final_boss(const pr_run *run, int wave)
{
    return is_classic(run->mode) && pr_run_is_wave_final(run->mode, wave);
}

bool pr_run_is_endless_boss(const pr_run *run, int wave) { return wave % 50 == 0 && is_endless(run->mode); }
bool pr_run_is_endless_minor_boss(const pr_run *run, int wave) { return wave % 250 == 0 && is_endless(run->mode); }
bool pr_run_is_endless_major_boss(const pr_run *run, int wave) { return wave % 1000 == 0 && is_endless(run->mode); }

/* isTrainerBoss(waveIndex, biomeType, offsetGym):
 * DAILY: waveIndex > 10 && waveIndex < 50 && !(waveIndex % 10)
 * else:  waveIndex % 30 === (offsetGym ? 0 : 20) && (biomeType !== END || isClassic || isWaveFinal(waveIndex)) */
bool pr_run_is_trainer_boss(const pr_run *run, int wave, int biome, bool offset_gym)
{
    if (run->mode == GAME_MODES_DAILY) return wave > 10 && wave < 50 && wave % 10 == 0;
    return wave % 30 == (offset_gym ? 0 : 20)
           && (biome != BIOME_ID_END || is_classic(run->mode) || pr_run_is_wave_final(run->mode, wave));
}

/* isFixedBattle(waveIndex): Object.hasOwn(this.battleConfig, waveIndex) || applyChallenges(FIXED_BATTLES, ...).
 * battleConfig is classicFixedBattles for CLASSIC and CHALLENGE, {} otherwise; its keys are the
 * ClassicFixedBossWaves values. */
bool pr_run_is_fixed_battle(pr_run *run, int wave)
{
    static const int classic_fixed[] = {
        CLASSIC_FIXED_BOSS_WAVES_TOWN_YOUNGSTER, CLASSIC_FIXED_BOSS_WAVES_RIVAL_1, CLASSIC_FIXED_BOSS_WAVES_RIVAL_2,
        CLASSIC_FIXED_BOSS_WAVES_EVIL_GRUNT_1, CLASSIC_FIXED_BOSS_WAVES_RIVAL_3, CLASSIC_FIXED_BOSS_WAVES_EVIL_GRUNT_2,
        CLASSIC_FIXED_BOSS_WAVES_EVIL_GRUNT_3, CLASSIC_FIXED_BOSS_WAVES_EVIL_ADMIN_1, CLASSIC_FIXED_BOSS_WAVES_RIVAL_4,
        CLASSIC_FIXED_BOSS_WAVES_EVIL_GRUNT_4, CLASSIC_FIXED_BOSS_WAVES_EVIL_ADMIN_2, CLASSIC_FIXED_BOSS_WAVES_EVIL_BOSS_1,
        CLASSIC_FIXED_BOSS_WAVES_RIVAL_5, CLASSIC_FIXED_BOSS_WAVES_EVIL_ADMIN_3, CLASSIC_FIXED_BOSS_WAVES_EVIL_BOSS_2,
        CLASSIC_FIXED_BOSS_WAVES_ELITE_FOUR_1, CLASSIC_FIXED_BOSS_WAVES_ELITE_FOUR_2, CLASSIC_FIXED_BOSS_WAVES_ELITE_FOUR_3,
        CLASSIC_FIXED_BOSS_WAVES_ELITE_FOUR_4, CLASSIC_FIXED_BOSS_WAVES_CHAMPION, CLASSIC_FIXED_BOSS_WAVES_RIVAL_6,
    };
    if (is_classic(run->mode))
        for (unsigned i = 0; i < sizeof classic_fixed / sizeof classic_fixed[0]; i++)
            if (classic_fixed[i] == wave) return true;
    if (run->has_challenges) run->unported |= PR_RUN_UNPORTED_CHALLENGES;
    return false;
}

/* getClearScoreBonus(): CLASSIC/CHALLENGE 5000, DAILY 2500, otherwise 0 */
int pr_run_get_clear_score_bonus(const pr_run *run)
{
    return is_classic(run->mode) ? 5000 : run->mode == GAME_MODES_DAILY ? 2500 : 0;
}

/* getEnemyModifierChance(isBoss): CLASSIC/CHALLENGE/DAILY isBoss ? 6 : 18; ENDLESS/SPLICED isBoss ? 4 : 12 */
int pr_run_get_enemy_modifier_chance(const pr_run *run, bool is_boss)
{
    if (is_endless(run->mode)) return is_boss ? 4 : 12;
    return is_boss ? 6 : 18;
}

/* isWaveTrainer(waveIndex):
 *   if (isDaily) { getDailyTrainerManipulation(waveIndex) ?? (waveIndex % 10 === 5 || ...) }
 *   if (waveIndex % 30 === (offsetGym ? 0 : 20) && !isWaveFinal(waveIndex)) return true;
 *   if (waveIndex % 10 !== 1 && waveIndex % 10) {
 *     const trainerChance = arena.trainerChance; let allowTrainerBattle = true;
 *     if (trainerChance) {
 *       const waveBase = Math.floor(waveIndex / 10) * 10;
 *       for (let w = Math.max(waveIndex - 2, waveBase + 2); w <= Math.min(waveIndex + 2, waveBase + 10); w++) {
 *         if (w === waveIndex) continue;
 *         if (w % 30 === (offsetGym ? 0 : 20) || this.isFixedBattle(w)) { allowTrainerBattle = false; break; }
 *         if (w < waveIndex) {
 *           executeWithSeedOffset(() => { if (!randSeedInt(arena.trainerChance)) allowTrainerBattle = false; }, w);
 *           if (!allowTrainerBattle) break;
 *         }
 *       }
 *     }
 *     return Boolean(allowTrainerBattle && trainerChance && !randSeedInt(trainerChance));
 *   }
 *   return false; */
bool pr_run_is_wave_trainer(pr_run *run, int wave, int trainer_chance)
{
    if (run->mode == GAME_MODES_DAILY) {
        run->unported |= PR_RUN_UNPORTED_DAILY;
        return false;
    }
    int gym = run->offset_gym ? 0 : 20;
    if (wave % 30 == gym && !pr_run_is_wave_final(run->mode, wave)) return true;
    if (wave % 10 == 1 || wave % 10 == 0) return false;
    bool allow = true;
    if (trainer_chance) {
        int base = wave / 10 * 10;
        int lo = wave - 2 > base + 2 ? wave - 2 : base + 2, hi = wave + 2 < base + 10 ? wave + 2 : base + 10;
        for (int w = lo; w <= hi; w++) {
            if (w == wave) continue;
            if (w % 30 == gym || pr_run_is_fixed_battle(run, w)) { allow = false; break; }
            if (w < wave) {
                pr_rng r = pr_run_seed_offset_rng(run, w, NULL);
                if (!pr_rng_seed_int(&r, trainer_chance, 0)) allow = false;
                if (!allow) break;
            }
        }
    }
    return allow && trainer_chance && !pr_rng_seed_int(&run->rng, trainer_chance, 0);
}

void pr_run_reset_seed(pr_run *run, int wave)
{
    pr_seed_shift(&run->wave_seed, &run->seed, wave);
    pr_rng_sow(&run->rng, &run->wave_seed);
}

/* getMaxExpLevel(ignoreLevelCap) (LEVEL_CAP_OVERRIDE, a dev override, isn't modelled):
 *   if (ignoreLevelCap) return Number.MAX_SAFE_INTEGER;
 *   const waveIndex = Math.ceil((this.currentBattle?.waveIndex || 1) / 10) * 10;
 *   const difficultyWaveIndex = this.gameMode.getWaveForDifficulty(waveIndex);
 *   const baseLevel = (1 + difficultyWaveIndex / 2 + Math.pow(difficultyWaveIndex / 25, 2)) * 1.2;
 *   return Math.ceil(baseLevel / 2) * 2 + 2; */
double pr_run_get_max_exp_level(const pr_run *run, int wave, bool ignore_level_cap)
{
    if (ignore_level_cap) return 9007199254740991.0;
    double w = ceil((wave ? wave : 1) / 10.0) * 10;
    double d = pr_run_get_wave_for_difficulty(run, (int)w, false);
    double base = (1 + d / 2 + js_pow(d / 25, 2)) * 1.2;
    return ceil(base / 2) * 2 + 2;
}

/* getEncounterBossSegments(waveIndex, level, species, forceBoss) (ENEMY_HEALTH_SEGMENTS_OVERRIDE,
 * a dev override, isn't modelled):
 *   if (isDaily && isWaveFinal(waveIndex)) return dailyConfig?.boss?.segments ?? 5;
 *   if (forceBoss || (species && (species.subLegendary || species.legendary || species.mythical))) isBoss = true;
 *   else executeWithSeedOffset(() => { isBoss = waveIndex % 10 === 0 || (hasRandomBosses
 *          && randSeedInt(100) < Math.min(Math.max(Math.ceil((waveIndex - 250) / 50), 0) * 2, 30)); }, waveIndex << 2);
 *   if (!isBoss) return 0;
 *   let ret = 2; if (level >= 100) ret++; if (species && species.baseTotal >= 670) ret++;
 *   return ret + Math.floor(waveIndex / 250); */
static const pr_species *species_by_id(int id)
{
    int lo = 0, hi = PR_SPECIES_COUNT - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (pr_species_table[mid].id == id) return &pr_species_table[mid];
        if (pr_species_table[mid].id < id) lo = mid + 1; else hi = mid - 1;
    }
    return NULL;
}

int pr_run_get_encounter_boss_segments(pr_run *run, int wave, int level, int species, bool force_boss)
{
    if (run->mode == GAME_MODES_DAILY && pr_run_is_wave_final(run->mode, wave)) {
        run->unported |= PR_RUN_UNPORTED_DAILY;
        return 5;
    }
    const pr_species *sp = species >= 0 ? species_by_id(species) : NULL;
    bool boss;
    if (force_boss || (sp && sp->rarity)) {
        boss = true;
    } else {
        boss = wave % 10 == 0;
        if (!boss && is_endless(run->mode)) {
            pr_rng r = pr_run_seed_offset_rng(run, wave << 2, NULL);
            double c = ceil((wave - 250) / 50.0);
            double pct = (c > 0 ? c : 0) * 2;
            boss = pr_rng_seed_int(&r, 100, 0) < (pct < 30 ? pct : 30);
        }
    }
    if (!boss) return 0;
    int ret = 2;
    if (level >= 100) ret++;
    if (sp && pr_forms[sp->form0].base_total >= 670) ret++;
    return ret + wave / 250;
}

/* getDoubleBattleChance(newWaveIndex):
 *   const doubleChance = new NumberHolder(newWaveIndex % 10 === 0 ? 32 : 8);
 *   this.applyModifiers(DoubleBattleChanceBoosterModifier, true, doubleChance);   // each: value /= 4
 *   for (const p of this.getPlayerField()) applyAbAttrs("DoubleBattleChanceAbAttr", ...); // each: value /= 4
 *   return Math.max(doubleChance.value, 1);
 * Dividing by 4 is exact, so the order of the divisions doesn't matter. */
double pr_run_get_double_battle_chance(int wave, int n_lure_modifiers, int n_double_chance_abilities)
{
    double chance = wave % 10 == 0 ? 32 : 8;
    for (int i = 0; i < n_lure_modifiers + n_double_chance_abilities; i++) chance /= 4;
    return chance > 1 ? chance : 1;
}

pr_rng pr_run_battle_rng(const pr_run *run, int wave)
{
    return pr_run_seed_offset_rng(run, wave << 3, &run->wave_seed);
}

/* getLevelForWave():
 *   if (isDailyFinalBoss(this.waveIndex)) return DAILY_BOSS_LEVEL;
 *   const levelWaveIndex = this.gameMode.getWaveForDifficulty(this.waveIndex);
 *   const baseLevel = 1 + levelWaveIndex / 2 + Math.pow(levelWaveIndex / 25, 2);
 *   if (this.gameMode.isBoss(this.waveIndex)) {
 *     const ret = Math.floor(baseLevel * 1.2);
 *     if (this.isClassicFinalBoss || !(this.waveIndex % 250)) return Math.ceil(ret / 25) * 25;
 *     let levelOffset = 0;
 *     if (!this.gameMode.isWaveFinal(this.waveIndex))
 *       levelOffset = Math.round(Phaser.Math.RND.realInRange(-1, 1) * Math.floor(levelWaveIndex / 10));
 *     return ret + levelOffset;
 *   }
 *   const deviation = 10 / levelWaveIndex;
 *   const levelOffset = Math.abs(this.randSeedGaussForLevel(deviation));
 *   return Math.max(Math.round(baseLevel + levelOffset), 1);
 * randSeedGaussForLevel(value): let rand = 0; for (let i = value; i > 0; i--) rand += randSeedFloat(); return rand / value;
 * isClassicFinalBoss is gameMode.isClassic && gameMode.isWaveFinal(waveIndex). */
int pr_run_get_level_for_wave(pr_run *run, pr_rng *rng, int wave)
{
    if (run->mode == GAME_MODES_DAILY) {
        run->unported |= PR_RUN_UNPORTED_DAILY; /* isDailyFinalBoss reads the daily config */
        return 0;
    }
    double lw = pr_run_get_wave_for_difficulty(run, wave, false);
    double base = 1 + lw / 2 + js_pow(lw / 25, 2);
    if (pr_run_is_boss(wave)) {
        double ret = floor(base * 1.2);
        if ((is_classic(run->mode) && pr_run_is_wave_final(run->mode, wave)) || wave % 250 == 0)
            return (int)(ceil(ret / 25) * 25);
        double offset = 0;
        if (!pr_run_is_wave_final(run->mode, wave))
            offset = js_round(pr_rng_real_in_range(rng, -1, 1) * floor(lw / 10));
        return (int)(ret + offset);
    }
    double deviation = 10 / lw, rand = 0;
    for (double i = deviation; i > 0; i--) rand += pr_rng_seed_float(rng);
    double level = js_round(base + fabs(rand / deviation));
    return (int)(level > 1 ? level : 1);
}
