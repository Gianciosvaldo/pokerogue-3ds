/* generateMoveset (src/ai/ai-moveset-gen.ts @ 8555c08) for a wild Pokemon that is neither a boss nor a trainer's, with no
 * passive and no fusion: what every Pokemon of a mystery encounter (the Global Trade System's, the Salesman's) and, one day,
 * of a wild wave is made with. What a trainer's or a boss's moveset adds (TM and egg moves, the superceded and forbidden
 * lists, the weight bumps, instant Tera) is not here: gc_generate_moveset says so and leaves the Pokemon alone.
 *
 * Step by step, with the order upstream draws from the RNG:
 *  1. the level pool: getLevelMoves({startingLevel: 1, includeEvolutionMoves, includePrevolutionMoves}) in its own order, up
 *     to the Pokemon's level, each move once, unimplemented moves left out; the weight is the earliest level + 20, an
 *     evolution move 60, a level 1 move of power 70 or more ("move reminder") 50;
 *  2. filterMovePool: only Memento-like moves (SacrificialAttrOnHit) go for a wild Pokemon;
 *  3. adjustDamageMoveWeights: each damaging move's weight x clamp(effective power / the highest (40 to 120), 1/4, 1), and
 *     the ones that use the lower of Attack and Special Attack are scaled by min((lower/higher)^3 * 2, 1) (Body Press by
 *     min((Defense/higher)^3 * 1.3, 1.1)); effective power is Move.calculateEffectivePower with the Pokemon's ability;
 *  4. every weight becomes ceil(weight^1.6 * 100);
 *  5. forceSignatureMove (a 60% coin, randSeedInt(100), only when a signature move is in the pool; randSeedItem for a list)
 *     and, unless that gave a damaging move of one of the Pokemon's types, forceStabMove (randSeedInt(total) over the STAB
 *     moves, walking with rand > weight);
 *  6. fillInRemainingMovesetSlots: randSeedInt(total) over what is left, until four moves or nothing is left; then
 *     filterUselessMoves may take one move out (a weather move without its use, a stat boost nothing profits from...), and
 *     the fill runs again for as long as it does and the pool it started from was not empty.
 * Things upstream does that this reproduces as written, because the differential sees them: effectivePowerAbilityCheck's
 * holders are objects, so `res?.maxMultiHit` and `res?.instantCharge` are truthy whenever a Pokemon is given (Triple Axel is
 * always 6 hits, a multi-hit move is never counted by its hit count and charging moves never cost a turn); a pool of nothing
 * stays empty. */
#include "internal.h"
#include "gc/movegen.h"
#include <math.h>
#include <string.h>

#define MG_POOL_MAX 256

typedef struct { uint16_t move; double w; } mg_ent;
typedef struct { mg_ent e[MG_POOL_MAX]; int n; } mg_pool;

static int pool_find(const mg_pool *p, int move)
{
    for (int i = 0; i < p->n; i++) if (p->e[i].move == move) return i;
    return -1;
}
static void pool_delete(mg_pool *p, int move)
{
    const int i = pool_find(p, move);
    if (i < 0) return;
    memmove(&p->e[i], &p->e[i + 1], (size_t)(p->n - i - 1) * sizeof p->e[0]);
    p->n--;
}

/* the Pokemon being made and what it needs from the scene */
typedef struct {
    pr_battle bt;               /* the constructor's view: no modifiers, the arena's weather and terrain */
    pr_pokemon p;               /* the Pokemon with its constructor stats */
    int8_t types[PR_MAX_TYPES];
    int n_types;
    uint16_t ability;           /* getAbility() */
    uint16_t moves[4];          /* the moveset so far */
    int n_moves;
} mg_ctx;

static const gc_mg_move *mgm(int move) { return &gc_mg_moves[move]; }
static bool mg_flag(int move, uint32_t f) { return (mgm(move)->flags & f) != 0; }
static bool mg_has_type(const mg_ctx *c, int type)
{
    for (int i = 0; i < c->n_types; i++) if (c->types[i] == type) return true;
    return false;
}
static bool mg_ability_is(const mg_ctx *c, int id) { return c->p.base_ability == id; }
static uint16_t mg_ab_bits(const mg_ctx *c)
{
    return c->ability < (unsigned)gc_mg_ability_count ? gc_mg_abilities[c->ability].bits : 0;
}

/* ---- Move.calculateEffectivePower(pokemon) ---- */

static const gc_mg_pow *find_pow(int ability, int move, int dim, int val)
{
    int lo = 0, hi = gc_mg_pow_count - 1;
    while (lo <= hi) {
        const int mid = (lo + hi) / 2;
        const gc_mg_pow *r = &gc_mg_pows[mid];
        int cmp = (int)r->ability - ability;
        if (!cmp) cmp = (int)r->move - move;
        if (!cmp) cmp = (int)r->dim - dim;
        if (!cmp) cmp = (int)r->val - val;
        if (!cmp) return r;
        if (cmp < 0) lo = mid + 1; else hi = mid - 1;
    }
    return NULL;
}

/* effectivePowerAbilityCheck: what the Pokemon's ability does to this move's power and accuracy */
static void ability_stats(mg_ctx *c, int move, double *power_mult, double *acc_mult)
{
    *power_mult = 1;
    *acc_mult = 1;
    const int ab = c->ability;
    if (ab == ABILITY_ID_NEUROFORCE) {
        /* MovePowerBoostAbAttr((user, target, move) => target.getMoveEffectiveness(user, move) >= 2, 1.25), target = the user */
        if (pr_get_move_effectiveness(&c->bt, &c->p, &c->p, move) >= 2) *power_mult = 1.25;
        return;
    }
    const gc_mg_pow *row = find_pow(ab, move, 0, 0);
    const gc_mg_pow *alt = NULL;
    if (c->p.gender == 0 || c->p.gender == 1) alt = find_pow(ab, 0, 2, c->p.gender);
    if (!alt && c->bt.weather) alt = find_pow(ab, move, 1, c->bt.weather);
    if (!alt && mg_has_type(c, POKEMON_TYPE_FLYING)) alt = find_pow(ab, move, 3, 1);
    if (!alt && mg_flag(move, GC_MGF_VARIABLE_TYPE)) {
        const int type = pr_get_move_type(&c->bt, &c->p, move);
        if (type != pr_moves[move].type) alt = find_pow(ab, move, 4, type);
    }
    if (alt) row = alt;
    if (row) {
        *power_mult = row->power_mult;
        *acc_mult = row->acc_mult;
    }
}

static double effective_power(mg_ctx *c, int move)
{
    const pr_move *mv = &pr_moves[move];
    if (mv->category == MOVE_CATEGORY_STATUS || mv->power <= 0) return 0;
    double power_mult, acc_mult;
    ability_stats(c, move, &power_mult, &acc_mult);
    double effective;
    if (mg_flag(move, GC_MGF_MULTIHIT_POWER_INC)) {
        /* `res?.maxMultiHit || ...`: the holder is always truthy */
        effective = mv->power * power_mult * 6 * acc_mult;
    } else {
        /* `multiHitAttr && !res?.maxMultiHit` is never true with a Pokemon */
        effective = mv->power * power_mult * (mv->accuracy == -1 ? 1 : fmin(mv->accuracy * acc_mult, 100) / 100);
    }
    int turns = 1;
    if (mg_flag(move, GC_MGF_DELAYED)) turns += 2;
    if (mg_flag(move, GC_MGF_RECHARGE)) turns += 1;
    /* a charging move costs a turn only `!res?.instantCharge`, a holder: never */
    return effective / turns;
}

/* ---- types ---- */

/* getMoveType(move, pokemon, willTera = false) of ai-moveset-gen.ts: VariableMoveTypeAttr.getTypeForMovegen */
static int movegen_type(mg_ctx *c, int move)
{
    const pr_move *mv = &pr_moves[move];
    if (mv->category == MOVE_CATEGORY_STATUS) return mv->type;
    int type = mv->type;
    switch (mgm(move)->vt_kind) {
    case GC_MGV_FORM_CHANGE_ITEM: case GC_MGV_TECHNO_BLAST: case GC_MGV_RAGING_BULL: case GC_MGV_IVY_CUDGEL:
    case GC_MGV_HIDDEN_POWER:
        /* getTypesForItemSpawn(user, move)[0]: apply() on a holder that starts as the move's own type */
        pr_apply_variable_move_type(&c->bt, &c->p, move, &type);
        break;
    case GC_MGV_WEATHER_BALL: {
        /* the weather the Pokemon's own ability summons (getAbilityAttrs("PostSummonWeatherChangeAbAttr").at(-1)) */
        const int w = c->ability < (unsigned)gc_mg_ability_count ? gc_mg_abilities[c->ability].summon_weather : -1;
        switch (w) {
        case WEATHER_TYPE_SUNNY: case WEATHER_TYPE_HARSH_SUN: type = POKEMON_TYPE_FIRE; break;
        case WEATHER_TYPE_RAIN: case WEATHER_TYPE_HEAVY_RAIN: type = POKEMON_TYPE_WATER; break;
        case WEATHER_TYPE_SANDSTORM: type = POKEMON_TYPE_ROCK; break;
        case WEATHER_TYPE_HAIL: case WEATHER_TYPE_SNOW: type = POKEMON_TYPE_ICE; break;
        default: break;
        }
        break;
    }
    case GC_MGV_MATCH_USER_TYPE: {
        /* getTypes({includeTeraType: false, bypassSummonData: true, ignoreThirdType: true})[0] ?? move.type */
        int8_t t[PR_MAX_TYPES];
        if (pr_get_types(&c->p, t, PR_TYPES_NO_TERA | PR_TYPES_BYPASS_SUMMON | PR_TYPES_IGNORE_THIRD) > 0) type = t[0];
        break;
    }
    default:   /* Aura Wheel, Tera Blast and Starstorm (willTera is false), Terrain Pulse, the Pledges: the move's own type */
        break;
    }
    return type;
}

static bool stab_blacklisted(int move)
{
    for (int i = 0; i < gc_mg_stab_blacklist_count; i++) if (gc_mg_stab_blacklist[i] == move) return true;
    return false;
}

/* ---- pools and the RNG ---- */

/* the walk of forceStabMove and fillInRemainingMovesetSlots: `while (rand > weight) rand -= weight[index++]` */
static int pick_weighted(pr_rng *rng, const mg_ent *pool, int n, double total)
{
    double rand = pr_rng_seed_int(rng, (int32_t)total, 0);
    int index = 0;
    while (index < n - 1 && rand > pool[index].w) rand -= pool[index++].w;
    return index;
}

static void add_to_moveset(mg_ctx *c, mg_pool *base, int move)
{
    pool_delete(base, move);
    c->moves[c->n_moves++] = (uint16_t)move;
}

static bool in_moveset(const mg_ctx *c, int move)
{
    for (int i = 0; i < c->n_moves; i++) if (c->moves[i] == move) return true;
    return false;
}

/* forceSignatureMove: the move added, or -1 */
static int force_signature(mg_ctx *c, pr_rng *rng, mg_pool *base)
{
    const gc_mg_signature *sig = NULL;
    for (int i = 0; i < gc_mg_sig_count; i++) if (gc_mg_signatures[i].species == c->p.species) sig = &gc_mg_signatures[i];
    if (!sig) return -1;
    int forced;
    if (!sig->is_list) {
        forced = gc_mg_sig_moves[sig->first];
        if (pool_find(base, forced) < 0) return -1;
        if (pr_rng_seed_int(rng, 100, 0) >= gc_mg_const.forced_signature_move_chance) return -1;
    } else {
        int avail[16], n = 0;
        for (int i = 0; i < sig->n && n < 16; i++) if (pool_find(base, gc_mg_sig_moves[sig->first + i]) >= 0) avail[n++] = gc_mg_sig_moves[sig->first + i];
        if (!n) return -1;
        if (pr_rng_seed_int(rng, 100, 0) >= gc_mg_const.forced_signature_move_chance) return -1;
        /* randSeedItem: no roll for a single item */
        forced = n == 1 ? avail[0] : avail[pr_rng_seed_int(rng, n, 0)];
    }
    add_to_moveset(c, base, forced);
    return forced;
}

static void force_stab(mg_ctx *c, pr_rng *rng, mg_pool *base)
{
    mg_ent chosen[MG_POOL_MAX];
    int n = 0;
    double total = 0;
    for (int i = 0; i < base->n; i++) {
        const int move = base->e[i].move;
        if (pr_moves[move].category == MOVE_CATEGORY_STATUS) continue;
        if (!mg_has_type(c, movegen_type(c, move)) || stab_blacklisted(move)) continue;
        chosen[n++] = base->e[i];
        total += base->e[i].w;
    }
    if (!n) return;
    const int index = pick_weighted(rng, chosen, n, total);
    add_to_moveset(c, base, chosen[index].move);
}

/* ---- filterUselessMoves ---- */

static bool matches_offensive_category(int move, int category)
{
    return pr_moves[move].category == category
        && !mg_flag(move, GC_MGF_FIXED_DAMAGE | GC_MGF_DEF_ATK | GC_MGF_PHOTON | GC_MGF_SHELL_SIDE | GC_MGF_TARGET_ATK_USER_ATK);
}

/* getExistingDamageMoveTypes(pokemon, false), as a bit per PokemonType */
static uint32_t existing_damage_types(mg_ctx *c)
{
    uint32_t set = 0;
    for (int i = 0; i < c->n_moves; i++) {
        const int move = c->moves[i];
        if (pr_moves[move].category != MOVE_CATEGORY_STATUS && !mg_flag(move, GC_MGF_FIXED_DAMAGE)) set |= 1u << movegen_type(c, move);
    }
    return set;
}

static bool any_instant_charge(const mg_ctx *c, int weather)
{
    for (int i = 0; i < c->n_moves; i++) if (mgm(c->moves[i])->weather_instant & (1u << weather)) return true;
    return false;
}

static bool should_remove_rain_dance(mg_ctx *c)
{
    if (existing_damage_types(c) & (1u << POKEMON_TYPE_WATER)) return false;
    static const int abilities[] = {ABILITY_ID_RAIN_DISH, ABILITY_ID_FORECAST, ABILITY_ID_SWIFT_SWIM, ABILITY_ID_DRY_SKIN};
    for (int i = 0; i < 4; i++) if (mg_ability_is(c, abilities[i])) return false;
    return !any_instant_charge(c, WEATHER_TYPE_RAIN);
}

static bool should_remove_sunny_day(mg_ctx *c)
{
    if (existing_damage_types(c) & (1u << POKEMON_TYPE_FIRE)) return false;
    static const int abilities[] = {ABILITY_ID_CHLOROPHYLL, ABILITY_ID_FLOWER_GIFT, ABILITY_ID_PROTOSYNTHESIS, ABILITY_ID_HARVEST, ABILITY_ID_FORECAST};
    for (int i = 0; i < 5; i++) if (mg_ability_is(c, abilities[i])) return false;
    const bool solar_power = mg_ability_is(c, ABILITY_ID_SOLAR_POWER);
    for (int i = 0; i < c->n_moves; i++) {
        const int move = c->moves[i];
        if (mg_flag(move, GC_MGF_WEATHER_BALL_TYPE) || move == MOVE_ID_HYDRO_STEAM
            || (pr_moves[move].category == MOVE_CATEGORY_SPECIAL && solar_power)
            || (mgm(move)->weather_instant & (1u << WEATHER_TYPE_SUNNY)))
            return false;
    }
    return true;
}

static bool should_remove_snowscape_hail(mg_ctx *c)
{
    int8_t t[PR_MAX_TYPES];
    const int n = pr_get_types(&c->p, t, PR_TYPES_NO_TERA);
    for (int i = 0; i < n; i++) if (t[i] == POKEMON_TYPE_ICE) return false;
    static const int abilities[] = {ABILITY_ID_SLUSH_RUSH, ABILITY_ID_FORECAST, ABILITY_ID_ICE_FACE, ABILITY_ID_SNOW_CLOAK, ABILITY_ID_ICE_BODY};
    for (int i = 0; i < 5; i++) if (mg_ability_is(c, abilities[i])) return false;
    for (int i = 0; i < c->n_moves; i++)
        if (c->moves[i] == MOVE_ID_AURORA_VEIL || (mgm(c->moves[i])->weather_instant & (1u << WEATHER_TYPE_SNOW))) return false;
    return true;
}

static bool should_remove_sandstorm(mg_ctx *c)
{
    int8_t t[PR_MAX_TYPES];
    const int n = pr_get_types(&c->p, t, PR_TYPES_NO_TERA);
    for (int i = 0; i < n; i++) if (t[i] == POKEMON_TYPE_ROCK) return false;
    const uint32_t ground_steel_rock = (1u << POKEMON_TYPE_GROUND) | (1u << POKEMON_TYPE_STEEL) | (1u << POKEMON_TYPE_ROCK);
    if (mg_ability_is(c, ABILITY_ID_SAND_FORCE) && (existing_damage_types(c) & ground_steel_rock)) return false;
    static const int abilities[] = {ABILITY_ID_SAND_RUSH, ABILITY_ID_SAND_VEIL, ABILITY_ID_FORECAST};
    for (int i = 0; i < 3; i++) if (mg_ability_is(c, abilities[i])) return false;
    return !any_instant_charge(c, WEATHER_TYPE_SANDSTORM);
}

static bool has_sleep_inducing_move(const mg_ctx *c, bool self)
{
    for (int i = 0; i < c->n_moves; i++) if (mg_flag(c->moves[i], self ? GC_MGF_SLEEP_SELF : GC_MGF_SLEEP_STATUS)) return true;
    return false;
}

static bool remove_self_stat_boost(const mg_ctx *c, int move)
{
    const int cat = mgm(move)->boost_cat;
    if (!cat) return false;
    for (int i = 0; i < c->n_moves; i++) if (matches_offensive_category(c->moves[i], cat - 1)) return false;
    return true;
}

static bool has_sun_instant_charge(const mg_ctx *c)
{
    for (int i = 0; i < c->n_moves; i++) if (c->moves[i] == MOVE_ID_SUNNY_DAY) return true;
    return (mg_ab_bits(c) & GC_MGA_SUN_INSTANT) != 0;
}

static bool can_inflict_poison(const mg_ctx *c)
{
    const bool no_sheer_force = !mg_ability_is(c, ABILITY_ID_SHEER_FORCE);
    for (int i = 0; i < c->n_moves; i++) {
        const int move = c->moves[i];
        if (move == MOVE_ID_BANEFUL_BUNKER || (mg_flag(move, GC_MGF_POISON_ATTR) && (no_sheer_force || pr_moves[move].chance < 0))) return true;
    }
    const uint16_t bits = mg_ab_bits(c);
    if (bits & GC_MGA_DEF_POISON) return true;
    bool can_make_contact = false;
    for (int i = 0; i < c->n_moves; i++) if (pr_moves[c->moves[i]].flags & MOVE_FLAGS_MAKES_CONTACT) can_make_contact = true;
    can_make_contact = can_make_contact && !(bits & GC_MGA_IGNORE_CONTACT);
    return can_make_contact && (bits & (GC_MGA_ATK_POISON | GC_MGA_ATK_TOXIC));
}

static void splice_move(mg_ctx *c, int i)
{
    memmove(&c->moves[i], &c->moves[i + 1], (size_t)(c->n_moves - i - 1) * sizeof c->moves[0]);
    c->n_moves--;
}

/* filterUselessMoves(pokemon, false): true = a move was taken out */
static bool filter_useless_moves(mg_ctx *c)
{
    int weather_moves = 0;
    const bool overrides_weather = (mg_ab_bits(c) & (GC_MGA_WEATHER_CHANGE | GC_MGA_SUPPRESS_WEATHER | GC_MGA_PREATTACK_WEATHER)) != 0;
    for (int i = c->n_moves - 1; i >= 0; i--) {
        const int move = c->moves[i];
        if (mg_flag(move, GC_MGF_WEATHER_CHANGE)) weather_moves++;
        if ((mg_flag(move, GC_MGF_WEATHER_CHANGE) && overrides_weather)
            || (move == MOVE_ID_RAIN_DANCE && should_remove_rain_dance(c))
            || (move == MOVE_ID_SUNNY_DAY && should_remove_sunny_day(c))
            || ((move == MOVE_ID_SNOWSCAPE || move == MOVE_ID_HAIL) && should_remove_snowscape_hail(c))
            || (move == MOVE_ID_SANDSTORM && should_remove_sandstorm(c))
            || (mg_flag(move, GC_MGF_SELF_STATUS_SINGLE) && remove_self_stat_boost(c, move))
            || (mg_flag(move, GC_MGF_COND_TARGET_SLEEP) && !has_sleep_inducing_move(c, false))
            || (mg_flag(move, GC_MGF_COND_USER_SLEEP) && !has_sleep_inducing_move(c, true))
            || (move == MOVE_ID_AURORA_VEIL
                && !(mg_ability_is(c, ABILITY_ID_SNOW_WARNING) || in_moveset(c, MOVE_ID_HAIL) || in_moveset(c, MOVE_ID_SNOWSCAPE)))
            || ((move == MOVE_ID_SOLAR_BEAM || move == MOVE_ID_SOLAR_BLADE) && !has_sun_instant_charge(c))
            || (move == MOVE_ID_VENOM_DRENCH && !can_inflict_poison(c))) {
            splice_move(c, i);
            return true;
        }
    }
    if (weather_moves >= 2) {
        for (int i = 0; i < c->n_moves; i++) {
            if (mg_flag(c->moves[i], GC_MGF_WEATHER_CHANGE)) {
                splice_move(c, i);
                break;
            }
        }
        return true;
    }
    return false;
}

/* ---- fillInRemainingMovesetSlots ---- */

static void fill_remaining(mg_ctx *c, pr_rng *rng, mg_pool *base)
{
    while (c->n_moves < 4) {
        mg_ent remaining[MG_POOL_MAX];
        int n = 0;
        double total = 0;
        for (int i = 0; i < base->n; i++) {
            const int move = base->e[i].move;
            bool taken = false;
            for (int k = 0; k < c->n_moves; k++)
                if (c->moves[k] == move || (mg_flag(move, GC_MGF_SACRIFICIAL) && mg_flag(c->moves[k], GC_MGF_SACRIFICIAL))) taken = true;
            if (taken) continue;
            remaining[n++] = base->e[i];
            total += base->e[i].w;
        }
        if (!n) return;
        const int index = pick_weighted(rng, remaining, n, total);
        pool_delete(base, remaining[index].move);
        c->moves[c->n_moves++] = remaining[index].move;
    }
}

/* ---- generateMoveset ---- */

/* the constructor's view of the Pokemon: before enemy items and summon effects, on a battle that has only the arena's weather */
static bool ctx_init(mg_ctx *c, const gc_run *run, const gc_mon *m)
{
    memset(c, 0, sizeof *c);
    c->bt.game_mode = run->run.mode;
    c->bt.wave_index = run->wave;
    c->bt.weather = run->bt.weather;
    c->bt.terrain = run->bt.terrain;
    c->p = m->b;
    pr_update_passive(&c->bt, &c->p);
    pr_calculate_stats(&c->bt, &c->p);
    if (pr_has_passive(&c->bt, &c->p)) return false;
    c->ability = c->p.ability;
    c->n_types = pr_get_types(&c->p, c->types, 0);
    return true;
}

bool gc_mg_effective_powers(gc_run *run, const gc_mon *m, const uint16_t *moves, int n, double *out)
{
    static mg_ctx c;
    if (!ctx_init(&c, run, m)) return false;
    for (int i = 0; i < n; i++) out[i] = effective_power(&c, moves[i]);
    return true;
}

bool gc_generate_moveset(gc_run *run, pr_rng *rng, gc_mon *m)
{
    if (m->b.has_trainer || m->trainer_slot || m->b.is_boss || m->b.fusion_species || run->run.has_challenges) return false;
    if (m->b.species >= PR_SPECIES_COUNT) return false;

    static mg_ctx c;   /* ~the stack of a 3DS thread is 32 KB: the pools are too big for it */
    if (!ctx_init(&c, run, m)) return false;

    int pp_up[4] = {0, 0, 0, 0};
    if (m->b.species == SPECIES_ID_SMEARGLE) {
        /* Smeargle: four Sketches, nothing rolled */
        for (int i = 0; i < 4; i++) c.moves[i] = MOVE_ID_SKETCH;
        c.n_moves = 4;
    } else if (m->b.species == SPECIES_ID_ETERNATUS) {
        /* Eternatus: the fixed sets; the form's Recover has -4 PP ups */
        int fixed[4];
        pr_run_get_eternatus_moveset(m->b.form, false, fixed);
        for (int i = 0; i < 4; i++) {
            c.moves[i] = (uint16_t)fixed[i];
            if (fixed[i] == MOVE_ID_RECOVER) pp_up[i] = -4;
        }
        c.n_moves = 4;
    } else {
        static mg_pool pool, base;
        pool.n = base.n = 0;
        /* 1. the level pool */
        static gc_learn_row rows[512];
        const int count = gc_level_moves(m, 1, true, true, false, rows, 512);
        if (count < 0) return false;
        for (int i = 0; i < count; i++) {
            const int learn_level = rows[i].level, id = rows[i].move;
            if (c.p.level < learn_level) break;
            if (pr_moves[id].unimplemented || pool_find(&pool, id) >= 0) continue;
            double weight = learn_level + gc_mg_const.base_level_weight_offset;
            if (learn_level == 0) weight = gc_mg_const.evolution_move_weight;
            else if (learn_level == 1 && pr_moves[id].power >= gc_mg_const.evo_move_bp_threshold) weight = gc_mg_const.relearn_move_weight;
            else if (learn_level == -1) weight = 0;
            if (pool.n >= MG_POOL_MAX) return false;
            pool.e[pool.n].move = (uint16_t)id;
            pool.e[pool.n++].w = weight;
        }
        /* 2. filterMovePool */
        for (int i = pool.n - 1; i >= 0; i--)
            if (pool.e[i].w <= 0 || pr_moves[pool.e[i].move].unimplemented || mg_flag(pool.e[i].move, GC_MGF_SACRIFICIAL_ON_HIT)) pool_delete(&pool, pool.e[i].move);

        /* 3. adjustDamageMoveWeights */
        double max_power = 40;
        for (int i = 0; i < pool.n; i++) {
            if (pr_moves[pool.e[i].move].category == MOVE_CATEGORY_STATUS) continue;
            max_power = fmax(max_power, effective_power(&c, pool.e[i].move));
            if (max_power >= gc_mg_const.move_power_ceiling) {
                max_power = gc_mg_const.move_power_ceiling;
                break;
            }
        }
        const double atk = c.p.stats[STAT_ATK], sp_atk = c.p.stats[STAT_SPATK];
        const double lower = fmin(atk, sp_atk), higher = fmax(atk, sp_atk);
        const int worse_category = atk > sp_atk ? MOVE_CATEGORY_SPECIAL : MOVE_CATEGORY_PHYSICAL;
        const double adjustment_ratio = fmin(js_pow(lower / higher, 3) * 2, 1);
        for (int i = 0; i < pool.n; i++) {
            const int move = pool.e[i].move;
            if (pr_moves[move].category == MOVE_CATEGORY_STATUS) continue;
            double w = pool.e[i].w;
            w *= fmax(0.25, fmin(1, effective_power(&c, move) / max_power));
            if (mg_flag(move, GC_MGF_DEF_ATK)) {
                const double def_ratio = c.p.stats[STAT_DEF] / higher;
                w *= fmin(js_pow(def_ratio, 3) * 1.3, 1.1);
            } else if (pr_moves[move].category == worse_category && !mg_flag(move, GC_MGF_PHOTON) && !mg_flag(move, GC_MGF_SHELL_SIDE)) {
                w *= adjustment_ratio;
            }
            pool.e[i].w = w;
        }
        /* 4. the base weights */
        for (int i = 0; i < pool.n; i++) {
            if (pool.e[i].w <= 0) continue;
            base.e[base.n].move = pool.e[i].move;
            base.e[base.n++].w = ceil(js_pow(pool.e[i].w, gc_mg_const.base_weight_multiplier) * 100);
        }

        /* 5. a signature move, then a STAB move unless the signature is one */
        const int signature = force_signature(&c, rng, &base);
        if (signature < 0 || pr_moves[signature].category == MOVE_CATEGORY_STATUS || !mg_has_type(&c, movegen_type(&c, signature)))
            force_stab(&c, rng, &base);

        /* 6. the rest */
        int outer = 0;
        for (int i = 0; i < base.n; i++) if (!in_moveset(&c, base.e[i].move)) outer++;
        do {
            fill_remaining(&c, rng, &base);
        } while (outer > 0 && filter_useless_moves(&c));
    }

    memset(m->b.moves, 0, sizeof m->b.moves);
    memset(m->b.pp_used, 0, sizeof m->b.pp_used);
    memset(m->b.pp_up, 0, sizeof m->b.pp_up);
    m->b.n_moves = 0;
    for (int i = 0; i < 4; i++) m->b.max_pp_override[i] = -1;
    for (int i = 0; i < c.n_moves; i++) {
        gc_mon_set_move(m, i, c.moves[i]);
        m->b.pp_up[i] = (int8_t)pp_up[i];
    }
    return true;
}
