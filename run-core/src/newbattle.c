/* BattleScene.newBattle() for a new (not loaded) run wave: the order of its rolls on the wave's RNG,
 * joining the pieces ported in wave.c and biome.c (src/battle-scene.ts newBattle,
 * handleNonFixedBattle, generateNewBattleTrainer, checkIsDouble; src/field/trainer.ts constructor;
 * src/battle.ts constructor). The JS each step replaces is quoted where it happens. */
#include "pr/run.h"
#include <string.h>

static double get_double_chance(int wave, const pr_new_battle_ctx *ctx)
{
    return ctx->double_chance ? ctx->double_chance(ctx->double_chance_ctx, wave)
        : pr_run_get_double_battle_chance(wave, ctx->n_lures, ctx->n_double_chance_abilities);
}

static const pr_trainer_info *trainer_info(int type)
{
    int lo = 0, hi = pr_trainer_count - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (pr_trainers[mid].type == type) return &pr_trainers[mid];
        if (pr_trainers[mid].type < type) lo = mid + 1; else hi = mid - 1;
    }
    return NULL;
}

/* randSeedItem over n items: one item needs no roll */
static int seed_item(pr_rng *rng, int n)
{
    return n == 1 ? 0 : pr_rng_integer_in_range(rng, 0, n - 1);
}

static int classic_rival_type(int mode, int wave)
{
    if (mode != GAME_MODES_CLASSIC && mode != GAME_MODES_CHALLENGE) return -1;
    switch (wave) {
    case CLASSIC_FIXED_BOSS_WAVES_RIVAL_1: return TRAINER_TYPE_RIVAL;
    case CLASSIC_FIXED_BOSS_WAVES_RIVAL_2: return TRAINER_TYPE_RIVAL_2;
    case CLASSIC_FIXED_BOSS_WAVES_RIVAL_3: return TRAINER_TYPE_RIVAL_3;
    case CLASSIC_FIXED_BOSS_WAVES_RIVAL_4: return TRAINER_TYPE_RIVAL_4;
    case CLASSIC_FIXED_BOSS_WAVES_RIVAL_5: return TRAINER_TYPE_RIVAL_5;
    case CLASSIC_FIXED_BOSS_WAVES_RIVAL_6: return TRAINER_TYPE_RIVAL_6;
    default: return -1;
    }
}

/* getRandomLocaleEntry(key): randSeedItem(Object.keys(i18next.t(key, { returnObjects: true }))) */
static int locale_entry(pr_run *run, int count)
{
    if (count < 1) { /* a missing key: Object.keys of the key string itself, not modelled */
        run->unported |= PR_RUN_UNPORTED_TRAINER;
        return -1;
    }
    return seed_item(&run->rng, count);
}

/* trainer.getPartyTemplate(): config.partyTemplateFunc?.() ?? config.partyTemplates[partyTemplateIndex].
 * The func reads the scene's currentBattle, which newBattle hasn't replaced yet when the Battle
 * constructor asks: the previous battle's wave (0 = none). */
static const pr_party_template *party_template(const pr_run *run, const pr_trainer_info *t, int index, int prev_wave)
{
    if (!t->has_template_func) return &pr_party_templates[t->templates[index]];
    const pr_template_segment *best = NULL;
    for (int i = 0; i < t->n_segments; i++) {
        const pr_template_segment *s = &pr_template_segments[t->segment_start + i];
        if (s->mode != run->mode) continue;
        if (prev_wave == 0 ? s->from_wave == 0 : (s->from_wave > 0 && s->from_wave <= prev_wave)) best = s;
    }
    return best ? &pr_party_templates[best->template] : NULL;
}

int pr_run_new_battle(pr_run *run, int wave, const pr_new_battle_ctx *ctx, pr_new_battle *out)
{
    memset(out, 0, sizeof *out);
    out->trainer_type = out->variant = out->template_index = out->name = out->partner_name = -1;
    /* this.resetSeed(waveIndex) */
    pr_run_reset_seed(run, wave);
    const int fixed_rival = ctx->enable_fixed_rivals ? classic_rival_type(run->mode, wave) : -1;
    const int fixed_e4 = ctx->enable_gym_e4 ? pr_run_fixed_e4_trainer(run, wave) : -1;
    /* Tasks 48/49 close rivals plus the Classic/Challenge E4 and Champion fixed battles. */
    if (pr_run_is_fixed_battle(run, wave) && fixed_rival < 0 && fixed_e4 < 0) {
        run->unported |= PR_RUN_UNPORTED_FIXED_BATTLE;
        return -1;
    }
    bool has_trainers = run->mode == GAME_MODES_CLASSIC || run->mode == GAME_MODES_CHALLENGE || run->mode == GAME_MODES_DAILY;
    out->battle_type = (fixed_rival >= 0 || fixed_e4 >= 0) ? BATTLE_TYPE_TRAINER
        : has_trainers && pr_run_is_wave_trainer(run, wave, ctx->trainer_chance) ? BATTLE_TYPE_TRAINER
                                                                                : BATTLE_TYPE_WILD;
    if (fixed_rival < 0 && fixed_e4 < 0
        && pr_run_is_wave_mystery_encounter(run, out->battle_type == BATTLE_TYPE_WILD, wave, &ctx->me)) {
        out->battle_type = BATTLE_TYPE_MYSTERY_ENCOUNTER;
        out->me_spawn_chance_reset = true;
    }
    const pr_trainer_info *t = NULL;
    if (out->battle_type == BATTLE_TYPE_TRAINER) {
        if (fixed_rival >= 0 || fixed_e4 >= 0) {
            /* Fixed BattleConfig constructs the chosen trainer under its own seed-offset RNG.  The
             * rival variant follows the player gender; these static classes have one party template. */
            out->trainer_type = fixed_rival >= 0 ? fixed_rival : fixed_e4;
            t = trainer_info(out->trainer_type);
            if (!t || !t->n_templates) { run->unported |= PR_RUN_UNPORTED_TRAINER; return -1; }
            out->variant = fixed_rival >= 0 && !ctx->player_female ? TRAINER_VARIANT_FEMALE : TRAINER_VARIANT_DEFAULT;
            out->template_index = 0;
        } else {
            /* generateNewBattleTrainer */
            out->trainer_type = pr_run_random_trainer_type(run, ctx->biome, wave, false);
            t = trainer_info(out->trainer_type);
            if (!t || !t->n_templates) { run->unported |= PR_RUN_UNPORTED_TRAINER; return -1; }
            bool dbl;
            if (t->double_only) dbl = true;
            else if (!t->has_double || (t->has_type_double && out->trainer_type != TRAINER_TYPE_TATE
                                        && out->trainer_type != TRAINER_TYPE_LIZA))
                dbl = false;
            else dbl = pr_rng_seed_int(&run->rng, (int32_t)get_double_chance(wave, ctx), 0) == 0;
            out->variant = dbl ? TRAINER_VARIANT_DOUBLE : pr_rng_seed_int(&run->rng, 2, 0) ? TRAINER_VARIANT_FEMALE
                                                                                         : TRAINER_VARIANT_DEFAULT;
            out->template_index = seed_item(&run->rng, t->n_templates);
            if (t->names >= 0) {
                int count = t->names_male >= 0 ? (out->variant == TRAINER_VARIANT_FEMALE ? t->names_female : t->names_male)
                                               : t->names;
                out->name = locale_entry(run, count);
                if (out->variant == TRAINER_VARIANT_DOUBLE && !t->double_only)
                    out->partner_name = locale_entry(run, t->names_female >= 0 ? t->names_female : t->names);
            }
        }
    }
    /* checkIsDouble: never on a final wave, an endless boss wave or a mystery encounter;
     * wild: randSeedInt(getDoubleBattleChance(waveIndex)) === 0; trainer: variant === DOUBLE */
    if (pr_run_is_wave_final(run->mode, wave) || pr_run_is_endless_boss(run, wave)
        || out->battle_type == BATTLE_TYPE_MYSTERY_ENCOUNTER)
        out->double_battle = false;
    else if (out->battle_type == BATTLE_TYPE_WILD)
        out->double_battle = pr_rng_seed_int(&run->rng, (int32_t)get_double_chance(wave, ctx), 0) == 0;
    else
        out->double_battle = out->variant == TRAINER_VARIANT_DOUBLE;
    /* new Battle(...) inside executeWithSeedOffset(waveIndex << 3, waveSeed): enemyLevels =
     * trainer ? trainer.getPartyLevels(waveIndex) : one getLevelForWave() per enemy */
    pr_rng battle_rng = pr_run_battle_rng(run, wave);
    /* the field initializer runs first: battleSeed = randomString(16, true) */
    pr_run_random_seed_string(&battle_rng, out->battle_seed, 16);
    if (out->battle_type == BATTLE_TYPE_TRAINER) {
        const pr_party_template *p = party_template(run, t, out->template_index, ctx->prev_wave);
        if (!p) { run->unported |= PR_RUN_UNPORTED_TRAINER; return -1; }
        uint8_t strengths[PR_MAX_PARTY_TEMPLATE];
        for (int i = 0; i < p->size && i < PR_MAX_PARTY_TEMPLATE; i++) strengths[i] = (uint8_t)p->strengths[i];
        bool trainer_double = t->double_only || out->variant == TRAINER_VARIANT_DOUBLE;
        out->n_levels = pr_run_get_party_levels(run, wave, p->size, strengths, p->strength, trainer_double, out->levels);
    } else if (out->battle_type == BATTLE_TYPE_WILD) {
        out->n_levels = out->double_battle ? 2 : 1;
        for (int i = 0; i < out->n_levels; i++) out->levels[i] = pr_run_get_level_for_wave(run, &battle_rng, wave);
    } else {
        out->n_levels = 1;   /* a mystery encounter's Battle gets one getLevelForWave(), like a single wild battle */
        out->levels[0] = pr_run_get_level_for_wave(run, &battle_rng, wave);
    }
    return 0;
}

/* randomString(length, seeded = true): each character is
 * "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789"[randSeedInt(62)] */
void pr_run_random_seed_string(pr_rng *rng, char *out, int length)
{
    static const char chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    for (int i = 0; i < length; i++) out[i] = chars[pr_rng_seed_int(rng, 62, 0)];
    out[length] = 0;
}
