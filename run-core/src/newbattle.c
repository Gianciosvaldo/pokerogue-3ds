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
static int locale_entry(pr_run *run, pr_rng *rng, int count)
{
    if (count < 1) { /* a missing key: Object.keys of the key string itself, not modelled */
        run->unported |= PR_RUN_UNPORTED_TRAINER;
        return -1;
    }
    return seed_item(rng, count);
}

/* new Trainer(type, variant) after the variant is known: partyTemplateIndex = randSeedItem(the config's templates), then the name
 * (and the partner's) from the class's locale entries, all on `rng` */
static void trainer_ctor(pr_run *run, pr_rng *rng, const pr_trainer_info *t, pr_new_battle *out)
{
    out->template_index = seed_item(rng, t->n_templates);
    if (t->names >= 0) {
        int count = t->names_male >= 0 ? (out->variant == TRAINER_VARIANT_FEMALE ? t->names_female : t->names_male)
                                       : t->names;
        out->name = locale_entry(run, rng, count);
        if (out->variant == TRAINER_VARIANT_DOUBLE && !t->double_only)
            out->partner_name = locale_entry(run, rng, t->names_female >= 0 ? t->names_female : t->names);
    }
}

/* The fixed battles of the evil teams' grunts (fixed-battle-configs.ts: getRandomTrainerFunc(grunts, true) under setSeedOffsetWave(EVIL_GRUNT_1),
 * so every grunt fight of a run is the same team) and the Town Youngster */
static const uint16_t evil_grunts[10] = {
    TRAINER_TYPE_ROCKET_GRUNT, TRAINER_TYPE_MAGMA_GRUNT, TRAINER_TYPE_AQUA_GRUNT, TRAINER_TYPE_GALACTIC_GRUNT, TRAINER_TYPE_PLASMA_GRUNT,
    TRAINER_TYPE_FLARE_GRUNT, TRAINER_TYPE_AETHER_GRUNT, TRAINER_TYPE_SKULL_GRUNT, TRAINER_TYPE_MACRO_GRUNT, TRAINER_TYPE_STAR_GRUNT,
};

/* 1 = the Town Youngster, 2 = a grunt fight, 0 = neither (the other fixed waves) */
static int evil_grunt_kind(int mode, int wave)
{
    if (mode != GAME_MODES_CLASSIC && mode != GAME_MODES_CHALLENGE) return 0;
    switch (wave) {
    case CLASSIC_FIXED_BOSS_WAVES_TOWN_YOUNGSTER: return 1;
    case CLASSIC_FIXED_BOSS_WAVES_EVIL_GRUNT_1: case CLASSIC_FIXED_BOSS_WAVES_EVIL_GRUNT_2:
    case CLASSIC_FIXED_BOSS_WAVES_EVIL_GRUNT_3: case CLASSIC_FIXED_BOSS_WAVES_EVIL_GRUNT_4: return 2;
    default: return 0;
    }
}

/* The admins' and bosses' pools (the same file): an entry is one trainer or a group to pick one from. Admin fights pick the group member with
 * randSeedItem (the first) or randSeedUniqueItem(group, 1 / 2) (the second and third, so that a team never sends the same admin twice) */
typedef struct { uint8_t n; uint16_t t[5]; } evil_entry;
#define EV1(a) {1, {a}}
#define EV2(a, b) {2, {a, b}}
#define EV3(a, b, c) {3, {a, b, c}}
#define EV4(a, b, c, d) {4, {a, b, c, d}}
#define EV5(a, b, c, d, e) {5, {a, b, c, d, e}}
#define EV_GROUPS EV2(TRAINER_TYPE_TABITHA, TRAINER_TYPE_COURTNEY), EV2(TRAINER_TYPE_MATT, TRAINER_TYPE_SHELLY), \
    EV3(TRAINER_TYPE_JUPITER, TRAINER_TYPE_MARS, TRAINER_TYPE_SATURN)
#define EV_STAR EV5(TRAINER_TYPE_GIACOMO, TRAINER_TYPE_MELA, TRAINER_TYPE_ATTICUS, TRAINER_TYPE_ORTEGA, TRAINER_TYPE_ERI)
static const evil_entry evil_admins[3][10] = {
    { EV1(TRAINER_TYPE_PETREL), EV_GROUPS, EV1(TRAINER_TYPE_COLRESS),
      EV4(TRAINER_TYPE_BRYONY, TRAINER_TYPE_ALIANA, TRAINER_TYPE_CELOSIA, TRAINER_TYPE_MABLE), EV1(TRAINER_TYPE_FABA),
      EV1(TRAINER_TYPE_PLUMERIA), EV1(TRAINER_TYPE_OLEANA), EV_STAR },
    { EV1(TRAINER_TYPE_PROTON), EV_GROUPS, EV1(TRAINER_TYPE_ZINZOLIN),
      EV4(TRAINER_TYPE_BRYONY, TRAINER_TYPE_ALIANA, TRAINER_TYPE_CELOSIA, TRAINER_TYPE_MABLE), EV1(TRAINER_TYPE_FABA),
      EV1(TRAINER_TYPE_PLUMERIA), EV1(TRAINER_TYPE_OLEANA), EV_STAR },
    { EV2(TRAINER_TYPE_ARCHER, TRAINER_TYPE_ARIANA), EV2(TRAINER_TYPE_TABITHA, TRAINER_TYPE_COURTNEY), EV2(TRAINER_TYPE_MATT, TRAINER_TYPE_SHELLY),
      EV3(TRAINER_TYPE_JUPITER, TRAINER_TYPE_MARS, TRAINER_TYPE_SATURN), EV1(TRAINER_TYPE_COLRESS), EV1(TRAINER_TYPE_XEROSIC),
      EV1(TRAINER_TYPE_FABA), EV1(TRAINER_TYPE_PLUMERIA), EV1(TRAINER_TYPE_OLEANA), EV_STAR },
};
static const uint16_t evil_bosses[2][10] = {
    { TRAINER_TYPE_ROCKET_BOSS_GIOVANNI_1, TRAINER_TYPE_MAXIE, TRAINER_TYPE_ARCHIE, TRAINER_TYPE_CYRUS, TRAINER_TYPE_GHETSIS, TRAINER_TYPE_LYSANDRE,
      TRAINER_TYPE_LUSAMINE, TRAINER_TYPE_GUZMA, TRAINER_TYPE_ROSE, TRAINER_TYPE_PENNY },
    { TRAINER_TYPE_ROCKET_BOSS_GIOVANNI_2, TRAINER_TYPE_MAXIE_2, TRAINER_TYPE_ARCHIE_2, TRAINER_TYPE_CYRUS_2, TRAINER_TYPE_GHETSIS_2,
      TRAINER_TYPE_LYSANDRE_2, TRAINER_TYPE_LUSAMINE_2, TRAINER_TYPE_GUZMA_2, TRAINER_TYPE_ROSE_2, TRAINER_TYPE_PENNY_2 },
};

/* 3..5 = the admin fights of waves 66, 114 and 164, 6 and 7 = the bosses of 115 and 165, 0 = neither */
static int evil_leader_kind(int mode, int wave)
{
    if (mode != GAME_MODES_CLASSIC && mode != GAME_MODES_CHALLENGE) return 0;
    switch (wave) {
    case CLASSIC_FIXED_BOSS_WAVES_EVIL_ADMIN_1: return 3;
    case CLASSIC_FIXED_BOSS_WAVES_EVIL_ADMIN_2: return 4;
    case CLASSIC_FIXED_BOSS_WAVES_EVIL_ADMIN_3: return 5;
    case CLASSIC_FIXED_BOSS_WAVES_EVIL_BOSS_1: return 6;
    case CLASSIC_FIXED_BOSS_WAVES_EVIL_BOSS_2: return 7;
    default: return 0;
    }
}

/* getRandomTrainerFunc's choice for an admin pool: randSeedItem(pool), then for a group seedOffset 0: randSeedItem(group), else
 * randSeedUniqueItem(group, seedOffset): a group no longer than the offset picks at random on this RNG, a longer one shuffles a copy on the RNG of
 * executeWithSeedOffset(.., 0) and takes the entry at the offset */
static int evil_admin_choice(pr_run *run, pr_rng *r, const evil_entry *pool, int seed_offset)
{
    const evil_entry *c = &pool[seed_item(r, 10)];
    if (c->n == 1) return c->t[0];
    if (seed_offset == 0 || c->n <= seed_offset) return c->t[seed_item(r, c->n)];
    uint16_t a[5];
    for (int i = 0; i < c->n; i++) a[i] = c->t[i];
    pr_rng r0 = pr_run_seed_offset_rng(run, 0, NULL);
    for (int i = c->n - 1; i > 0; i--) {
        const int j = pr_rng_integer_in_range(&r0, 0, i);
        const uint16_t tmp = a[i];
        a[i] = a[j];
        a[j] = tmp;
    }
    return a[seed_offset];
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
    const int fixed_evil = ctx->enable_evil_grunts ? evil_grunt_kind(run->mode, wave) : 0;
    const int fixed_leader = ctx->enable_evil_bosses ? evil_leader_kind(run->mode, wave) : 0;
    /* Tasks 48/49 close rivals plus the Classic/Challenge E4 and Champion fixed battles; the grunts and the Town Youngster too */
    if (pr_run_is_fixed_battle(run, wave) && fixed_rival < 0 && fixed_e4 < 0 && !fixed_evil && !fixed_leader) {
        run->unported |= PR_RUN_UNPORTED_FIXED_BATTLE;
        return -1;
    }
    bool has_trainers = run->mode == GAME_MODES_CLASSIC || run->mode == GAME_MODES_CHALLENGE || run->mode == GAME_MODES_DAILY;
    out->battle_type = (fixed_rival >= 0 || fixed_e4 >= 0 || fixed_evil || fixed_leader) ? BATTLE_TYPE_TRAINER
        : has_trainers && pr_run_is_wave_trainer(run, wave, ctx->trainer_chance) ? BATTLE_TYPE_TRAINER
                                                                                : BATTLE_TYPE_WILD;
    if (fixed_rival < 0 && fixed_e4 < 0 && !fixed_evil && !fixed_leader
        && pr_run_is_wave_mystery_encounter(run, out->battle_type == BATTLE_TYPE_WILD, wave, &ctx->me)) {
        out->battle_type = BATTLE_TYPE_MYSTERY_ENCOUNTER;
        out->me_spawn_chance_reset = true;
    }
    const pr_trainer_info *t = NULL;
    if (out->battle_type == BATTLE_TYPE_TRAINER) {
        if (fixed_leader) {
            /* the admins: getRandomTrainerFunc(pool, true, seedOffset 0 / 1 / 2) and the bosses getRandomTrainerFunc(pool), both under
             * setSeedOffsetWave(EVIL_GRUNT_1). An admin's gender is randInt(2), the caller's roll; a boss is never female and none is a grunt */
            pr_rng r = pr_run_seed_offset_rng(run, CLASSIC_FIXED_BOSS_WAVES_EVIL_GRUNT_1 << 8, NULL);
            if (fixed_leader <= 5) {
                out->trainer_type = evil_admin_choice(run, &r, evil_admins[fixed_leader - 3], fixed_leader - 3);
                out->variant = ctx->evil_gender_roll ? TRAINER_VARIANT_DEFAULT : TRAINER_VARIANT_FEMALE;   /* randInt(2) === 0 */
            } else {
                out->trainer_type = evil_bosses[fixed_leader - 6][seed_item(&r, 10)];
                out->variant = TRAINER_VARIANT_DEFAULT;
            }
            t = trainer_info(out->trainer_type);
            if (!t || !t->n_templates) { run->unported |= PR_RUN_UNPORTED_TRAINER; return -1; }
            trainer_ctor(run, &r, t, out);
        } else if (fixed_evil) {
            /* battleConfig.getTrainer() inside executeWithSeedOffset((seedOffsetWave || waveIndex) << 8) */
            pr_rng r = pr_run_seed_offset_rng(run, (fixed_evil == 1 ? wave : CLASSIC_FIXED_BOSS_WAVES_EVIL_GRUNT_1) << 8, NULL);
            if (fixed_evil == 1) {
                /* new Trainer(YOUNGSTER, randSeedInt(2) ? FEMALE : DEFAULT) */
                out->trainer_type = TRAINER_TYPE_YOUNGSTER;
                t = trainer_info(out->trainer_type);
                if (!t || !t->n_templates) { run->unported |= PR_RUN_UNPORTED_TRAINER; return -1; }
                out->variant = pr_rng_seed_int(&r, 2, 0) ? TRAINER_VARIANT_FEMALE : TRAINER_VARIANT_DEFAULT;
            } else {
                /* getRandomTrainerFunc(grunts, true): randSeedItem(pool), then randInt(2) === 0 ? FEMALE : DEFAULT and, as a grunt
                 * with a double, randInt(3) === 0 ? DOUBLE : that gender. randInt is Math.random: the caller's rolls */
                out->trainer_type = evil_grunts[seed_item(&r, 10)];
                t = trainer_info(out->trainer_type);
                if (!t || !t->n_templates) { run->unported |= PR_RUN_UNPORTED_TRAINER; return -1; }
                out->variant = ctx->evil_gender_roll == 0 ? TRAINER_VARIANT_FEMALE : TRAINER_VARIANT_DEFAULT;
                if (t->has_double && ctx->evil_double_roll == 0) out->variant = TRAINER_VARIANT_DOUBLE;
            }
            trainer_ctor(run, &r, t, out);
        } else if (fixed_rival >= 0 || fixed_e4 >= 0) {
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
            trainer_ctor(run, &run->rng, t, out);
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
