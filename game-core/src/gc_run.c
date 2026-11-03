/* The run: a new run and its starters, the event queue, the screen/choice loop, and the run-level
 * phase queue (upstream's PhaseTree, src/phase-tree.ts, for the phases between battle-core's turns). */
#include "internal.h"
#include <stdarg.h>
#include <limits.h>
#include <stdio.h>

_Static_assert(sizeof(gc_internal) <= sizeof(((gc_run *)0)->internal_), "gc_run.internal_ is too small");
_Static_assert(PR_AI_FEAT_COUNT_ <= GC_MAX_UNPORTED, "gc_run.unported is too small for ai-core's ids");
_Static_assert(GC_STUB_COUNT_ <= 16, "gc_run.stubs is too small");
_Static_assert(sizeof(gc_trainer_configs[0].names[0]) / sizeof(uint16_t) == GC_TNAME_COUNT_, "gc_trainer_config.names");
gc_internal *gc_int(gc_run *run) { return (gc_internal *)(void *)run->internal_; }

const char *const gc_stub_name[GC_STUB_COUNT_] = {
    [GC_STUB_ENEMY_MOVESET] = "enemy moveset (last 4 level moves)",
    [GC_STUB_REWARDS] = "reward screen reward-core could not roll (empty)",
    [GC_STUB_DOUBLE_AS_SINGLE] = "double battle played as single",
    [GC_STUB_MYSTERY_ENCOUNTER] = "mystery encounter played as wild",
    [GC_STUB_EVOLUTION] = "evolution condition not checked",
    [GC_STUB_UNPORTED_ENEMY] = "enemy run-core can't generate (stand-in)",
    [GC_STUB_TRAINER_REWARDS] = "trainer's own rewards",
    [GC_STUB_POST_BATTLE] = "post-battle abilities, Pay Day, lapsing items",
    [GC_STUB_TRAINER_TITLE] = "trainer's name without its title in a message",
    [GC_STUB_REWARD_INPUT] = "reward-core input game-core can't give",
    [GC_STUB_REWARD_APPLY] = "reward taken without its own effect",
};

void gc_stub(gc_run *run, int which)
{
    if (which >= 0 && which < GC_STUB_COUNT_) run->stubs[which]++;
}

/* The stand-ins' own RNG (xorshift32): never Phaser.Math.RND, so a stand-in can't move upstream's rolls */
uint32_t gc_stub_rand(gc_run *run, uint32_t n)
{
    gc_internal *in = gc_int(run);
    uint32_t x = in->stub_rng ? in->stub_rng : 0x9e3779b9u;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    in->stub_rng = x;
    return n ? x % n : 0;
}

/* ---------------------------------------------------------------- events */
gc_event *gc_ev_new(gc_run *run, int kind)
{
    if (run->ev_count >= GC_MAX_EVENTS) { run->ev_overflow = true; return NULL; }
    gc_event *ev = &run->events[(run->ev_head + run->ev_count++) % GC_MAX_EVENTS];
    memset(ev, 0, sizeof *ev);
    ev->kind = (uint8_t)kind;
    return ev;
}

/* Wallet and rewards can exceed int32 well before the Endless cap. */
void gc_ev_money(gc_run *run, double change)
{
    gc_event *ev = gc_ev_new(run, GC_EV_MONEY);
    if (!ev) return;
    ev->money_change = change;
    ev->money_after = run->money;
    ev->a = change >= INT32_MIN && change <= INT32_MAX ? (int32_t)change : 0;
    ev->b = run->money >= INT32_MIN && run->money <= INT32_MAX ? (int32_t)run->money : 0;
}

gc_event *gc_ev_msg(gc_run *run, const char *key, int n, ...)
{
    gc_event *ev = gc_ev_new(run, GC_EV_MESSAGE);
    if (!ev) return NULL;
    ev->msg.key = key;
    va_list ap;
    va_start(ap, n);
    for (int i = 0; i < n; i++) {
        pr_msg_arg a = va_arg(ap, pr_msg_arg);
        if (i < PR_MSG_MAX_ARGS) ev->msg.args[ev->msg.n_args++] = a;
    }
    va_end(ap);
    return ev;
}

pr_msg_arg gc_m_mon(gc_run *run, const char *name, int side, int idx)
{
    const gc_mon *m = side ? &run->enemy[idx] : &run->party[idx];
    return pr_arg_pokemon(&run->bt, name, &m->b);
}

pr_msg_arg gc_m_mon_name(gc_run *run, const char *name, int side, int idx)
{
    const gc_mon *m = side ? &run->enemy[idx] : &run->party[idx];
    return pr_arg_pokemon_name(&run->bt, name, &m->b);
}

/* trainer.getName() (GC_TNAME_*) as a message argument. A name that is one locale key goes as that key;
 * a title and a name ("$t(trainerClasses:aceTrainer) $t(trainersCommon:aceTrainer.male.3)") need more
 * than one argument can hold, so the name goes without its title for now (GC_STUB_TRAINER_TITLE;
 * gc_trainer_names picks the key) */
pr_msg_arg gc_m_trainer(gc_run *run, const char *name, int which)
{
    gc_internal *in = gc_int(run);
    pr_msg_arg a = {name, PR_ARG_STR, 0, ""};
    const char *t = run->trainer_names[which];
    if (!t) return a;
    const char *k = in->name_arg[in->name_ring][which];
    if (!k[0]) { a.s = t; return a; }   /* plain text: a class with no names, as upstream prints it */
    if (!(in->name_exact[in->name_ring] >> which & 1)) gc_stub(run, GC_STUB_TRAINER_TITLE);
    a.kind = PR_ARG_KEY;
    a.s = k;
    return a;
}

/* modifierType.name: "modifierType:ModifierType.<KEY>.name" for the plain modifier types */
pr_msg_arg gc_m_item(const char *name, const char *item_key)
{
    /* Known types have stable catalog identities. A long run must not exhaust a small
     * first-seen cache and start printing internal IDs for later reward types. */
    const int item = gc_reward_item_index(item_key);
    if (item >= 0) {
        static char keys[PR_ITEM_COUNT_][72];
        if (!keys[item][0]) snprintf(keys[item], sizeof keys[item], "modifierType:ModifierType.%s.name", item_key);
        return pr_arg_key(name, keys[item]);
    }
    /* Preserve the existing fallback for a stand-in key outside the catalog. */
    static struct { const char *item; char key[72]; } pool[48];
    static int n_pool;
    pr_msg_arg a = {name, PR_ARG_STR, 0, item_key};
    for (int i = 0; i < n_pool; i++)
        if (pool[i].item == item_key || !strcmp(pool[i].item, item_key)) { a.kind = PR_ARG_KEY; a.s = pool[i].key; return a; }
    if (n_pool >= 48) return a;
    pool[n_pool].item = item_key;
    snprintf(pool[n_pool].key, sizeof pool[n_pool].key, "modifierType:ModifierType.%s.name", item_key);
    a.kind = PR_ARG_KEY;
    a.s = pool[n_pool++].key;
    return a;
}

pr_msg_arg gc_m_item_arg(const char *name, const char *item_key, int arg)
{
    const int item = gc_reward_item_index(item_key);
    /* A concrete type also carries pack/voucher names (e.g. "5× Poke Ball"), which
     * have no generic ModifierType.<ID>.name locale entry. */
    if (item >= 0) {
        const int id = pr_item_type_name((pr_item_type){(uint16_t)item, (int16_t)arg});
        if (id > 0 && id < pr_reward_string_count && pr_reward_strings[id])
            return (pr_msg_arg){name, PR_ARG_STR, 0, pr_reward_strings[id]};
    }
    return gc_m_item(name, item_key);
}

const pr_event *gc_run_battle_event_data(const gc_run *run)
{
    return run->battle_event_storage ? run->battle_event_storage : run->battle_events;
}

void gc_run_set_battle_event_storage(gc_run *run, gc_battle_event_reserve_fn reserve, void *ctx)
{
    run->battle_event_reserve = reserve;
    run->battle_event_reserve_ctx = ctx;
}

/* battle-core's events from its last call, kept for the app until the next choice */
void gc_ev_battle_chunk(gc_run *run, const pr_event *events, int n)
{
    if (n <= 0) return;
    const int first = run->n_battle_events;
    int capacity = run->battle_event_storage ? run->battle_event_capacity : GC_MAX_BATTLE_EVENTS;
    if (first < 0 || first > capacity || n > INT_MAX-first) { run->battle_events_overflow = true; return; }
    if (n > capacity-first && run->battle_event_reserve && !run->battle_events_overflow) {
        pr_event *storage = NULL; int reserved = 0;
        if (run->battle_event_reserve(run->battle_event_reserve_ctx,
                                     gc_run_battle_event_data(run), first, first+n, &storage, &reserved)
            && storage && reserved >= first+n) {
            run->battle_event_storage = storage;
            run->battle_event_capacity = capacity = reserved;
        }
    }
    if (n > capacity-first) { n = capacity-first; run->battle_events_overflow = true; }
    if (n <= 0) return;
    pr_event *storage = run->battle_event_storage ? run->battle_event_storage : run->battle_events;
    memcpy(storage+first, events, sizeof(pr_event) * (size_t)n);
    run->n_battle_events += n;
    gc_event *ev = gc_ev_new(run, GC_EV_BATTLE);
    if (ev) { ev->a = first; ev->b = n; }
}

void gc_ev_battle(gc_run *run)
{
    pr_engine *e = &run->eng;
    if (e->events_overflow) run->battle_events_overflow = true;
    int first = e->n_events_acknowledged;
    gc_ev_battle_chunk(run, e->events + first, e->n_events - first);
    pr_event_acknowledge(e);
}

bool gc_run_next_event(gc_run *run, gc_event *out)
{
    if (!run->ev_count) return false;
    *out = run->events[run->ev_head];
    run->ev_head = (run->ev_head + 1) % GC_MAX_EVENTS;
    run->ev_count--;
    return true;
}

/* ---------------------------------------------------------------- the phase queue */
void gc_rq_reset(gc_rqueue *q)
{
    memset(q, 0, sizeof *q);
    q->n_levels = 1;
}

static void rq_add(gc_rqueue *q, gc_rphase p, int level)
{
    if (level == q->current_level + 1 && level == q->n_levels) {
        if (q->n_levels >= GC_RQ_LEVELS) { q->overflow = true; return; }
        q->n[q->n_levels++] = 0;
    }
    if (level < 0 || level >= q->n_levels || q->n[level] >= GC_RQ_CAP) { q->overflow = true; return; }
    q->q[level][q->n[level]++] = p;
}

void gc_rq_push(gc_run *run, gc_rphase p) { rq_add(&gc_int(run)->rq, p, 0); }
void gc_rq_unshift(gc_run *run, gc_rphase p) { gc_rqueue *q = &gc_int(run)->rq; rq_add(q, p, q->current_level + 1); }

bool gc_rq_next(gc_rqueue *q, gc_rphase *out)
{
    q->current_level = q->n_levels - 1;
    while (q->current_level > 0 && q->n[q->current_level] == 0) {
        q->n_levels--;
        q->current_level--;
    }
    if (q->n[q->current_level] == 0) return false;
    *out = q->q[q->current_level][0];
    memmove(&q->q[q->current_level][0], &q->q[q->current_level][1], sizeof(gc_rphase) * (size_t)(q->n[q->current_level] - 1));
    q->n[q->current_level]--;
    return true;
}

bool gc_rq_has(gc_run *run, int type)
{
    gc_rqueue *q = &gc_int(run)->rq;
    for (int l = 0; l < q->n_levels; l++)
        for (int k = 0; k < q->n[l]; k++) if (q->q[l][k].type == type) return true;
    return false;
}

bool gc_rq_remove(gc_run *run, int type)
{
    gc_rqueue *q = &gc_int(run)->rq;
    for (int l = q->n_levels - 1; l >= 0; l--)
        for (int k = 0; k < q->n[l]; k++)
            if (q->q[l][k].type == type) {
                memmove(&q->q[l][k], &q->q[l][k + 1], sizeof(gc_rphase) * (size_t)(q->n[l] - k - 1));
                q->n[l]--;
                return true;
            }
    return false;
}

void gc_rq_remove_all(gc_run *run, int type) { while (gc_rq_remove(run, type)) {} }

void gc_rq_clear(gc_run *run)
{
    gc_rqueue *q = &gc_int(run)->rq;
    for (int l = 0; l < GC_RQ_LEVELS; l++) q->n[l] = 0;
    q->n_levels = 1;
    q->current_level = 0;
}

/* Runs phases until one waits on a screen, or the run is over */
void gc_run_until_screen(gc_run *run)
{
    gc_rqueue *q = &gc_int(run)->rq;
    for (int guard = 0; guard < 100000 && !q->waiting && !run->finished; guard++) {
        gc_rphase p;
        if (!gc_rq_next(q, &p)) {
            /* an empty queue starts the next turn (PhaseManager's turnStart), or there is nothing left */
            if (!run->in_battle) { run->finished = true; break; }
            p = gc_rp(RP_TURN_INIT, 0, 0, 0);
        }
        q->current = p;
        if (!gc_phase_start(run, &q->current)) { q->waiting = true; return; }
        q->current.type = RP_NONE;
    }
    if (!q->waiting && run->screen.kind != GC_SCREEN_GAME_OVER && run->screen.kind != GC_SCREEN_CLEAR) {
        /* the queue ran dry outside a battle, or looped: a game-core bug, shown as the end of the run */
        memset(&run->screen, 0, sizeof run->screen);
        run->screen.kind = GC_SCREEN_GAME_OVER;
        run->finished = true;
    }
}

/* ---------------------------------------------------------------- a new run */
void gc_run_new(gc_run *run, int mode, const char *seed, uint32_t entropy)
{
    memset(run, 0, sizeof *run);
    run->api_version = GC_API_VERSION;
    gc_internal *in = gc_int(run);
    gc_rq_reset(&in->rq);
    in->stub_rng = entropy * 2654435761u + 1;
    in->last_battle_type = -1;
    run->run.mode = mode;
    /* the entropy's own RNG: the run seed when none is given (randomString(24)), and the ids */
    char buf[32];
    snprintf(buf, sizeof buf, "%lu", (unsigned long)entropy);
    pr_seedstr es;
    pr_seed_from_ascii(&es, buf);
    pr_rng er;
    pr_rng_sow(&er, &es);
    char s[25];
    if (!seed || !*seed) { pr_run_random_seed_string(&er, s, 24); seed = s; }
    pr_seed_from_ascii(&run->run.seed, seed);
    /* gameData.trainerId / secretId: randInt(65536) at a new save */
    run->trainer_id = (int)floor(pr_rng_frac(&er) * 65536);
    run->secret_id = (int)floor(pr_rng_frac(&er) * 65536);
    /* setSeed(): the wave cycle offset and offsetGym come from the seed */
    run->wave_cycle_offset = pr_run_get_generated_wave_cycle_offset(&run->run);
    run->run.offset_gym = pr_run_get_generated_offset_gym(&run->run);
    /* initBiomeDepths runs once when the game loads, on whatever RND holds then: here, the run seed's */
    pr_run_reset_seed(&run->run, 0);
    pr_rng dr = run->run.rng;
    pr_run_init_biome_depths(&run->depths, &dr);
    run->biome = BIOME_ID_TOWN;
    run->money = pr_run_get_starting_money(&run->run);
    run->pokeballs[POKEBALL_TYPE_POKEBALL] = 5;
    run->me.spawn_chance = 3; /* BASE_MYSTERY_ENCOUNTER_SPAWN_WEIGHT */
    run->mystery_type = run->mystery_tier = -1;
    run->trainer_type = run->trainer_variant = -1;
    run->trainer_name = run->trainer_partner_name = -1;
    run->screen.kind = GC_SCREEN_STARTERS;
}

/* The constructor's tera type: randSeedItem(getTypes({includeTeraType: false, bypassSummonData: true, ignoreThirdType: true})), one
 * draw for two types and none for one (initBattle replaces the result with the starter's own tera type) */
static bool constructor_tera_draw(gc_run *run, const pr_enemy_gen *g, int species)
{
    int8_t types[2];
    const int n = pr_run_enemy_types(g, species, types);
    if (n < 1) return false;
    if (n > 1) (void)pr_rng_integer_in_range(&run->run.rng, 0, n - 1);
    return true;
}

/* A Spliced Endless starter's fusion. Upstream fuses it twice, and both draw from Phaser.Math.RND:
 *  1. the Pokemon constructor (isSplicedOnly, level 5 > 1): calculateStats(), generateFusionSpecies() with the ordinary filter
 *     (species.getCompatibleFusionSpeciesFilter()), then luck = shiny + fusionShiny luck, then the tera type
 *     randSeedItem(getTypes({includeTeraType: false, bypassSummonData: true, ignoreThirdType: true})) over the fused types
 *     (one draw for two types, none for one);
 *  2. SelectStarterPhase.initBattle, after the starter's luck is set from the dex: generateFusionSpecies(true), the starter filter,
 *     which replaces everything the first one chose (gc_mon_starter_fusion).
 * The first is run here only for the draws it makes (run-core's constructor fusion, the same rolls); its result is replaced. The
 * Ability Charm does not reach either: HiddenAbilityRateBoosterModifier is applied only when !this.hasTrainer(), and a player's
 * Pokemon has a trainer. */
static bool constructor_fusion_draws(gc_run *run, gc_mon *m)
{
    pr_pokemon *p = &m->b;
    pr_enemy_gen tmp;
    memset(&tmp, 0, sizeof tmp);
    tmp.id = p->id;
    tmp.form = p->form;
    tmp.nature = p->nature;
    tmp.fusion_species = -1;
    pr_enemy_gen_ctx ctx;
    memset(&ctx, 0, sizeof ctx);
    ctx.species = p->species;
    ctx.level = p->level;
    ctx.wave = run->wave;
    ctx.biome = run->biome;
    ctx.wave_cycle_offset = run->wave_cycle_offset;
    ctx.battle_type = BATTLE_TYPE_WILD;
    ctx.trainer_type = -1;
    if (pr_run_generate_fusion(&run->run, &ctx, &tmp) < 0) return false;
    return constructor_tera_draw(run, &tmp, p->species);
}

/* Pokemon.generateFusionSpecies(true) (SelectStarterPhase.initBattle, after the starter's luck):
 *   hasHiddenAbility = !randSeedInt(BASE_HIDDEN_ABILITY_RATE); randAbilityIndex = randSeedInt(2);
 *   fusionSpecies = randomSpecies(waveIndex || 0, level, false, filter, true): randSeedItem of the catchable species the filter
 *     keeps, as they are (filterAllEvolutions), in registry order. The starter filter: hasEvolutions && !hasPrevolution && not a
 *     sub-legendary / legendary / mythical && !isTrainerForbidden() && not this species && not Ditto;
 *   fusionAbilityIndex = abilityHidden && hasHiddenAbility ? 2 : ability2 === ability1 ? 0 : randAbilityIndex;
 *   fusionShiny = shiny, fusionVariant = variant; fusionGender = (id % 256) * 0.390625 < malePercent (genderless without one);
 *   fusionFormIndex = getSpeciesFormIndex(fusionSpecies, fusionGender, getNature(), true); fusionLuck = luck.
 * The form roll is run-core's egg-phase one (ignoreArena, no trainer). For the species this filter keeps it is the same roll: the
 * egg phase only changes Gimmighoul in Classic / Challenge, and the nature only Toxtricity (which has a prevolution).
 * Upstream recalculates the stats at the next updateModifiers, before the first battle; this is that recalculation. */
#define GC_BASE_HIDDEN_ABILITY_RATE 256   /* src/data/balance/rates.ts */
bool gc_mon_starter_fusion(gc_run *run, gc_mon *m)
{
    pr_rng *rng = &run->run.rng;
    const bool has_hidden = !pr_rng_seed_int(rng, GC_BASE_HIDDEN_ABILITY_RATE, 0);
    const int rand_ability = pr_rng_seed_int(rng, 2, 0);
    static uint16_t list[1400];
    int n = 0;
    for (int i = 0; i < pr_species_gen_count && n < (int)(sizeof list / sizeof list[0]); i++) {
        const pr_species_gen *c = &pr_species_gens[i];
        if (!c->catchable || !c->has_evolutions || c->has_prevolution || c->sub_legendary || c->legendary || c->mythical
            || c->trainer_forbidden || c->id == m->b.species || c->id == SPECIES_ID_DITTO)
            continue;
        list[n++] = c->id;
    }
    if (!n) return false;   /* randSeedItem([]) */
    const int fusion = list[n == 1 ? 0 : pr_rng_integer_in_range(rng, 0, n - 1)];
    const pr_species_gen *fs = gc_species_gen(fusion);
    if (!fs) return false;
    pr_pokemon *p = &m->b;
    p->fusion_species = (uint16_t)fusion;
    p->fusion_ability_index = (int8_t)(fs->has_hidden && has_hidden ? 2 : fs->same_abilities ? 0 : rand_ability);
    m->fusion_shiny = p->shiny;
    m->fusion_variant = m->variant;
    p->fusion_gender = (int8_t)(fs->male_percent < 0 ? GENDER_GENDERLESS
                                : (p->id % 256) * 0.390625 < fs->male_percent ? GENDER_MALE : GENDER_FEMALE);
    p->fusion_gender_known = true;
    pr_enemy_gen_ctx ctx;
    memset(&ctx, 0, sizeof ctx);
    ctx.species = fusion;
    ctx.level = p->level;
    ctx.wave = run->wave;
    ctx.biome = run->biome;
    ctx.wave_cycle_offset = run->wave_cycle_offset;
    ctx.battle_type = BATTLE_TYPE_WILD;
    ctx.trainer_type = -1;
    const unsigned before = run->run.unported;
    const int form = pr_run_get_egg_form_index(rng, &run->run, &ctx, p->fusion_gender);
    if (run->run.unported != before) return false;
    p->fusion_form = (uint8_t)form;
    m->fusion_luck = m->luck;
    pr_update_passive(&run->bt, p);
    pr_calculate_stats(&run->bt, p);
    return true;
}

/* SelectStarterPhase.initBattle: the party at the starting level, each built on Phaser.Math.RND, a Spliced Endless starter fused as
 * it is made (before the next one). Every Pokemon's constructor also draws its tera type (overwritten by initBattle with the
 * starter's own), which shifts the ids of the members after a two-type one. */
void gc_starters_build_party(gc_run *run, const gc_starter *starters, int n)
{
    const int level = pr_run_get_starting_level(&run->run);
    /* the constructor's calculateStats reads gameMode.isSplicedOnly (the other modes leave it to the first encounter's setup) */
    if (run->run.mode == GAME_MODES_SPLICED_ENDLESS) run->bt.game_mode = (int8_t)run->run.mode;
    for (int i = 0; i < n; i++) {
        gc_mon_new_player(run, &run->party[i], &starters[i], level);
        if (run->run.mode == GAME_MODES_SPLICED_ENDLESS) {
            if (!(constructor_fusion_draws(run, &run->party[i]) && gc_mon_starter_fusion(run, &run->party[i])))
                gc_stub(run, GC_STUB_UNPORTED_ENEMY);
        } else {
            pr_enemy_gen plain;
            memset(&plain, 0, sizeof plain);
            plain.form = run->party[i].b.form;
            plain.fusion_species = -1;
            if (!constructor_tera_draw(run, &plain, run->party[i].b.species)) gc_stub(run, GC_STUB_UNPORTED_ENEMY);
        }
        run->party_order[i] = (int8_t)i;
    }
    run->n_party = n;
}

bool gc_run_set_starters(gc_run *run, const gc_starter *starters, int n, const char **why)
{
    const char *dummy;
    if (!why) why = &dummy;
    if (run->screen.kind != GC_SCREEN_STARTERS) { *why = "not on the starter screen"; return false; }
    if (n < 1 || n > 6) { *why = "starterSelectUiHandler:invalidParty"; return false; }
    double cost = 0;
    for (int i = 0; i < n; i++) {
        double c = 0;
        if (!gc_starter_is_available(run->sys, &starters[i], &c, why)) return false;
        for (int k = 0; k < i; k++)
            if (starters[k].species == starters[i].species) { *why = "the same species twice"; return false; }
        cost += c;
    }
    if (cost > (double)gc_starter_budget(run->run.mode) + 1e-9) { *why = "starterSelectUiHandler:cost"; return false; }
    gc_starters_build_party(run, starters, n);
    memset(&run->screen, 0, sizeof run->screen);
    gc_rq_push(run, gc_rp(RP_NEW_BATTLE, 0, 0, 0));
    gc_run_until_screen(run);
    return true;
}

const gc_screen *gc_run_screen(const gc_run *run) { return &run->screen; }

const pr_msg *gc_run_refusal_message(const gc_run *run)
{
    const pr_msg *m=&gc_int((gc_run *)run)->refusal_message;
    return m->key ? m : NULL;
}

bool gc_run_choose(gc_run *run, const gc_choice *c, const char **why)
{
    gc_int(run)->refusal_message=(pr_msg){0};
    const char *dummy;
    if (!why) why = &dummy;
    *why = "";
    gc_rqueue *q = &gc_int(run)->rq;
    run->n_battle_events = 0;
    run->battle_events_overflow = false;
    if (run->screen.kind == GC_SCREEN_GAME_OVER || run->screen.kind == GC_SCREEN_CLEAR) {
        if (c->kind == GC_CH_CONTINUE) return true;
        *why = "the run is over";
        return false;
    }
    if (!q->waiting) { *why = "nothing to answer"; return false; }
    bool ok = false;
    bool done = gc_phase_choose(run, &q->current, c, why, &ok);
    if (!ok) return false;
    if (done) {
        q->waiting = false;
        q->current.type = RP_NONE;
        memset(&run->screen, 0, sizeof run->screen);
        gc_run_until_screen(run);
    }
    return true;
}
