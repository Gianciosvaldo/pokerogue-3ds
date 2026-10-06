/* A run's Pokemon: the player's (starters, experience, level ups, moves learned by level, evolution)
 * and the enemies' (from run-core's rolls). Ports of src/field/pokemon.ts (PlayerPokemon, addExp,
 * getEvolution, evolve), src/field/learnsets.ts (getLevelMoves) and src/data/balance/pokemon-evolutions.ts
 * (SpeciesFormEvolution.validate and its conditions). */
#include "internal.h"

const pr_species *gc_species(int id)
{
    int lo = 0, hi = PR_SPECIES_COUNT - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (pr_species_table[mid].id == id) return &pr_species_table[mid];
        if (pr_species_table[mid].id < id) lo = mid + 1; else hi = mid - 1;
    }
    return NULL;
}

const pr_form *gc_form(int species, int form)
{
    const pr_species *s = gc_species(species);
    if (!s) return &pr_forms[0];
    return &pr_forms[s->form0 + (form >= 0 && form < s->nforms ? form : 0)];
}

/* PokemonSpeciesForm.getAbility(abilityIndex): ability2 is ability1 for a one-ability species */
int gc_form_ability(const pr_form *f, int ability_index)
{
    if (ability_index == 0) return f->ability1;
    if (ability_index == 1) return f->ability2;
    return f->ability_hidden;
}

bool gc_mon_fainted(const gc_mon *m) { return m->b.hp <= 0; }
/* isAllowedInBattle(): not fainted (challenges aren't modelled) */
bool gc_mon_allowed(const gc_mon *m) { return m->b.hp > 0; }

static void init_common(gc_mon *m)
{
    pr_pokemon *p = &m->b;
    p->ability_override = -1;
    p->custom_passive = -1;
    p->status_toxic_turns = 0;
    p->status_sleep_turns = p->status_freeze_turns = -1;
    for (int i = 0; i < 4; i++) p->max_pp_override[i] = -1;
    pr_reset_summon_data(p);
    pr_reset_turn_data(p);
}

double gc_level_total_exp(int level, int growth_rate) { return pr_run_get_level_total_exp(level, growth_rate); }

/* new PlayerPokemon(species, level, abilityIndex, formIndex, gender, shiny, variant, ivs, nature), then
 * SelectStarterPhase's moveset, passive, pokerus and tera type. The constructor's own rolls (the id first)
 * are on Phaser.Math.RND; its generateAndPopulateMoveset isn't run: the starter's moveset replaces it
 * (tryPopulateMoveset), and without one the starter select screen's default is used. */
void gc_mon_new_player(gc_run *run, gc_mon *m, const gc_starter *s, int level)
{
    memset(m, 0, sizeof *m);
    pr_pokemon *p = &m->b;
    const pr_species *sp = gc_species(s->species);
    p->id = (uint32_t)floor(pr_rng_real_in_range(&run->run.rng, 0, 4294967296.0));
    p->species = s->species;
    p->form = s->form;
    p->level = (uint8_t)level;
    memcpy(p->ivs, s->ivs, 6);
    p->nature = s->nature;
    m->ability_index = s->ability_index;
    const pr_form *f = gc_form(s->species, s->form);
    p->base_ability = p->ability = (uint16_t)gc_form_ability(f, s->ability_index);
    p->gender = sp && sp->male_permille < 0 ? -1 : (s->female ? 1 : 0);
    p->shiny = s->shiny;
    m->variant = s->variant;
    m->luck = (int8_t)(s->shiny ? s->variant + 1 : 0);   /* Pokemon's constructor: luck = shiny ? variant + 1 : 0 */
    p->friendship = sp ? sp->base_friendship : 0;
    p->is_player = true;
    p->passive_flag = s->passive;
    p->tera_type = (int8_t)(s->tera_type >= 0 ? s->tera_type : f->type1);
    m->pokerus = s->pokerus;
    m->met_wave = -1;
    m->met_level = (uint8_t)level;   /* Pokemon's constructor: metLevel = level, metBiome = -1 before a battle, metSpecies */
    m->met_biome = -1;
    m->met_species = s->species;
    m->pokeball = POKEBALL_TYPE_POKEBALL;
    init_common(m);
    uint16_t mv[4] = {0};
    int n = 0;
    for (int i = 0; i < 4; i++) if (s->moves[i]) mv[n++] = s->moves[i];
    if (!n) n = gc_starter_default_moves(s->species, mv);
    for (int i = 0; i < n; i++) gc_mon_set_move(m, i, mv[i]);
    m->exp = gc_level_total_exp(level, sp ? sp->growth_rate : 0);
    pr_update_passive(&run->bt, p);
    pr_calculate_stats(&run->bt, p);
}

void gc_mon_new_enemy(gc_run *run, gc_mon *m, int species, int level, const pr_enemy_gen *g, int trainer_slot)
{
    memset(m, 0, sizeof *m);
    pr_pokemon *p = &m->b;
    const pr_species *sp = gc_species(species);
    p->id = g->id;
    p->species = (uint16_t)species;
    p->form = (uint8_t)g->form;
    p->level = (uint8_t)level;
    for (int i = 0; i < 6; i++) p->ivs[i] = (uint8_t)g->ivs[i];
    p->nature = (uint8_t)g->nature;
    m->ability_index = (uint8_t)g->ability_index;
    const pr_form *f = gc_form(species, g->form);
    p->base_ability = p->ability = (uint16_t)gc_form_ability(f, g->ability_index);
    p->gender = (int8_t)g->gender;
    p->shiny = g->shiny;
    m->variant = (uint8_t)g->variant;
    m->luck = (int8_t)g->luck;
    p->friendship = g->friendship >= 0 ? (uint8_t)g->friendship : (sp ? sp->base_friendship : 0);
    p->tera_type = (int8_t)g->tera_type;
    p->has_trainer = trainer_slot != 0;
    /* EnemyPokemon.setBoss: bossSegments from run-core's getEncounterBossSegments, one shield fewer than
     * segments to start with (battle-core clears them as the damage breaks them; 0 = not a boss) */
    p->is_boss = g->boss_segments > 0;
    p->boss_segments = (uint8_t)(g->boss_segments > 0 ? g->boss_segments : 0);
    p->boss_segment_index = (uint8_t)(g->boss_segments > 0 ? g->boss_segments - 1 : 0);
    m->trainer_slot = (uint8_t)trainer_slot;
    m->met_wave = (int16_t)run->wave;
    m->met_level = (uint8_t)level;
    m->met_biome = (int8_t)run->biome;
    m->met_species = (uint16_t)species;
    init_common(m);
    m->exp = gc_level_total_exp(level, sp ? sp->growth_rate : 0);
}

void gc_mon_set_move(gc_mon *m, int slot, int move)
{
    pr_pokemon *p = &m->b;
    if (move == 0 || slot < 0 || slot > 3) return;
    p->moves[slot] = (uint16_t)move;
    p->pp_used[slot] = 0;
    p->pp_up[slot] = 0;
    p->max_pp_override[slot] = -1;
    if (slot >= p->n_moves) p->n_moves = (uint8_t)(slot + 1);
}

/* PokemonMove.getMovePp(): maxPpOverride || pp + ppUp * toDmgValue(pp / 5) */
int gc_move_max_pp(const pr_pokemon *p, int slot)
{
    if (p->max_pp_override[slot] > 0) return p->max_pp_override[slot];
    int pp = pr_moves[p->moves[slot]].pp;
    int step = pp / 5 < 1 ? 1 : pp / 5;
    return pp + p->pp_up[slot] * step;
}

/* PokemonMove.isUsable's PP and implementation checks (the move restrictions Disable, Taunt, Torment,
 * Imprison, Encore... are battle-core's CommandPhase work: not checked here yet) */
bool gc_move_usable(const pr_pokemon *p, int slot, const char **why)
{
    if (slot < 0 || slot >= p->n_moves || !p->moves[slot]) { if (why) *why = ""; return false; }
    if (pr_moves[p->moves[slot]].unimplemented) { if (why) *why = "battle:moveNotImplemented"; return false; }
    if (p->pp_used[slot] >= gc_move_max_pp(p, slot)) { if (why) *why = "battle:moveNoPp"; return false; }
    return true;
}

static bool knows(const pr_pokemon *p, int move)
{
    for (int i = 0; i < p->n_moves; i++) if (p->moves[i] == move) return true;
    return false;
}

/* getLevelMoves({startingLevel, includePrevolutionMoves: true}) filtered to the non-PREVO moves, as
 * LevelUpPhase.end uses them: the species' own level moves with startingLevel <= level <= this.level,
 * sorted by level (stable), first of each move kept. Form learnsets (pokemonFormLevelMoves) aren't in
 * battle-core's tables yet, so a form uses its species' list. */
int gc_level_up_moves(const gc_mon *m, int starting_level, uint16_t *out, int max)
{
    const pr_species *sp = gc_species(m->b.species);
    if (!sp) return 0;
    int n = 0;
    for (int lv = starting_level; lv <= m->b.level; lv++)
        for (int i = 0; i < sp->level_moves.n; i++) {
            const pr_level_move *lm = &sp->level_moves.v[i];
            if (lm->level != lv || lm->level <= 0) continue;
            bool dup = false;
            for (int k = 0; k < n; k++) dup |= out[k] == lm->move;
            if (!dup && n < max) out[n++] = lm->move;
        }
    return n;
}

int gc_evolution_moves(const gc_mon *m, uint16_t *out, int max)
{
    const pr_species *sp = gc_species(m->b.species);
    if (!sp) return 0;
    int n = 0;
    for (int i = 0; i < sp->level_moves.n; i++) {
        const pr_level_move *lm = &sp->level_moves.v[i];
        if (lm->level != 0) continue;
        bool dup = false;
        for (int k = 0; k < n; k++) dup |= out[k] == lm->move;
        if (!dup && n < max) out[n++] = lm->move;
    }
    return n;
}

/* addExp(exp): levels up to getMaxExpLevel(), and the exp is cut back to the cap's level */
void gc_mon_add_exp(gc_run *run, gc_mon *m, double exp)
{
    const pr_species *sp = gc_species(m->b.species);
    int rate = sp ? sp->growth_rate : 0;
    double max_level = pr_run_get_max_exp_level(&run->run, run->wave, false);
    double initial = m->exp;
    m->exp += exp;
    while (m->b.level < max_level && m->b.level < 200 && m->exp >= gc_level_total_exp(m->b.level + 1, rate))
        m->b.level++;
    if (m->b.level >= max_level) {
        double t = gc_level_total_exp(m->b.level, rate);
        m->exp = t > initial ? t : initial;
    }
}

void gc_mon_recalc(gc_run *run, gc_mon *m)
{
    pr_update_passive(&run->bt, &m->b);
    pr_recalculate_stats(&run->bt, &m->b);
}

static const gc_species_evo *species_evo(int id)
{
    int lo = 0, hi = gc_species_evo_count - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (gc_species_evos[mid].id == id) return &gc_species_evos[mid];
        if (gc_species_evos[mid].id < id) lo = mid + 1; else hi = mid - 1;
    }
    return NULL;
}

static bool has_value(const gc_evo_cond *c, int v)
{
    for (int i = 0; i < c->n; i++) if (c->v[i] == v) return true;
    return false;
}

/* SpeciesEvolutionCondition.conditionsFulfilled (not a fusion) */
static bool conditions_fulfilled(gc_run *run, const gc_mon *m, const gc_evo *e)
{
    const pr_pokemon *p = &m->b;
    for (int i = 0; i < e->n_cond; i++) {
        const gc_evo_cond *c = &gc_evo_conds[e->cond_start + i];
        bool ok = false;
        switch (c->key) {
        case GC_EVO_FRIENDSHIP: ok = p->friendship >= c->v[0]; break;
        case GC_EVO_TIME:
            ok = has_value(c, pr_run_get_time_of_day(run->biome, run->wave, run->wave_cycle_offset));
            break;
        case GC_EVO_MOVE: ok = knows(p, c->v[0]); break;
        case GC_EVO_MOVE_TYPE:
            for (int k = 0; k < p->n_moves; k++) ok |= p->moves[k] && pr_moves[p->moves[k]].type == c->v[0];
            break;
        case GC_EVO_PARTY_TYPE:
            for (int k = 0; k < run->n_party; k++) {
                const pr_form *f = gc_form(run->party[k].b.species, run->party[k].b.form);
                ok |= f->type1 == c->v[0] || f->type2 == c->v[0];
            }
            break;
        case GC_EVO_GENDER: ok = p->gender == c->v[0]; break;
        case GC_EVO_SHEDINJA: ok = false; break;
        case GC_EVO_BIOME: ok = has_value(c, run->biome); break;
        case GC_EVO_WEATHER: ok = has_value(c, run->bt.weather); break;
        case GC_EVO_TYROGUE: {
            /* the first of Low Sweep, Mach Punch, Rapid Spin in the moveset is the one named */
            int first = 0;
            for (int k = 0; k < p->n_moves && !first; k++)
                if (p->moves[k] == MOVE_ID_LOW_SWEEP || p->moves[k] == MOVE_ID_MACH_PUNCH || p->moves[k] == MOVE_ID_RAPID_SPIN)
                    first = p->moves[k];
            ok = first == c->v[0];
            break;
        }
        case GC_EVO_NATURE: ok = has_value(c, p->nature); break;
        case GC_EVO_RANDOM_FORM: {
            /* executeWithSeedOffset(() => ret = !randSeedInt(value), pokemon.id) */
            pr_rng r = pr_run_seed_offset_rng(&run->run, (int)p->id, NULL);
            ok = !pr_rng_seed_int(&r, c->v[0], 0);
            break;
        }
        case GC_EVO_SPECIES_CAUGHT:
            if (run->sys) {
                uint16_t req_species = (uint16_t)c->v[0];
                ok = (req_species <= GC_MAX_SPECIES_ID && run->sys->dex[req_species].caught_attr != 0);
            } else {
                gc_stub(run, GC_STUB_EVOLUTION);
                ok = false;
            }
            break;
        case GC_EVO_TREASURE_TRACKER:
        case GC_EVO_HELD_ITEM:
            /* held items (Gimmighoul's coins, Deep Sea Tooth/Scale) wait on reward-core */
            gc_stub(run, GC_STUB_EVOLUTION);
            ok = false;
            break;
        }
        if (!ok) return false;
    }
    return true;
}

/* getEvolution(): the first evolution whose validate(pokemon) holds (no item) */
int gc_mon_get_evolution(gc_run *run, const gc_mon *m)
{
    const gc_species_evo *se = species_evo(m->b.species);
    if (!se) return -1;
    for (int i = 0; i < se->n; i++) {
        const gc_evo *e = &gc_evo_data[se->start + i];
        if (e->item) continue;                                   /* item === this.item: NONE only */
        if (m->b.level < e->level) continue;
        if (e->pre_form >= 0 && e->pre_form != m->b.form) continue;   /* getFormKey() === preFormKey */
        if (e->pre_form == -2) continue;                          /* a key no form has never matches */
        if (!conditions_fulfilled(run, m, e)) continue;
        return se->start + i;
    }
    return -1;
}

/* Pokemon.evolve(evolution, preEvolution), not a fusion: species, form (when preFormKey is set), ability
 * index (hidden into a species with 2 abilities becomes 1), stats. Nincada's Shedinja isn't made yet. */
void gc_mon_evolve(gc_run *run, gc_mon *m, int evo)
{
    const gc_evo *e = &gc_evo_data[evo];
    pr_pokemon *p = &m->b;
    const pr_form *pre = gc_form(p->species, p->form);
    int pre_count = pre->ability_hidden ? 3 : 2;
    m->pause_evolutions = false;
    p->species = e->species;
    if (e->pre_form != -1) p->form = (uint8_t)(e->evo_form >= 0 ? e->evo_form : 0);
    const pr_form *f = gc_form(p->species, p->form);
    int count = f->ability_hidden ? 3 : 2;
    if (m->ability_index > 2) m->ability_index = 0;
    if (m->ability_index == 2 && pre_count == 3 && count == 2) m->ability_index = 1;
    p->base_ability = (uint16_t)gc_form_ability(f, m->ability_index);
    if (p->ability_override < 0) p->ability = p->base_ability;
    gc_mon_recalc(run, m);

    if (run->sys) {
        gc_system_update_species_dex_ivs(run->sys, m->b.species, m->b.ivs);
        gc_system_set_pokemon_seen(run->sys, m, false, false);
        gc_system_set_pokemon_caught(run->sys, m, false, false);
    }
}

/* ---- starters ---- */
int gc_starter_budget(int mode) { (void)mode; return 10; }   /* the classic and endless cost limit */

/* speciesDataRegistry.getStarterCost (gen/out/gc_starters.c); -1 = not a starter species */
int gc_starter_cost(int species)
{
    int lo = 0, hi = gc_starter_cost_count - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (gc_starter_costs[mid].id == species) return gc_starter_costs[mid].cost;
        if (gc_starter_costs[mid].id < species) lo = mid + 1; else hi = mid - 1;
    }
    return -1;
}

/* game-data.ts:2018 getSpeciesStarterValue */
double gc_starter_value(const gc_system *sys, int species)
{
    int base = gc_starter_cost(species);
    if (base < 0) return -1.0;
    uint32_t red = (sys && species > 0 && species <= GC_MAX_SPECIES_ID) ? sys->starters[species].value_reduction : 0;
    double v = base;
    for (uint32_t i = 0; i < red; i++) {
        if (v > 1.0) v -= 1.0;
        else v /= 2.0;
    }
    return v;
}

/* Validates starter preferences / unlocks against system data (or defaults if NULL).
 * Cites upstream rules:
 * - starter-select-ui-handler.ts:638 (uncaught/locked species)
 * - starter-select-ui-handler.ts:646 (shiny unlock check against caughtAttr)
 * - starter-select-ui-handler.ts:657 (variant unlock check against caughtAttr)
 * - starter-select-ui-handler.ts:678 (gender unlock check against caughtAttr)
 * - starter-select-ui-handler.ts:688 (ability unlock check against abilityAttr and species single-ability logic)
 * - starter-select-ui-handler.ts:707 (form unlock check against formIndex & caughtAttr)
 * - starter-select-ui-handler.ts:718, game-data.ts:1989 (nature unlock check against natureAttr)
 * - starter-select-ui-handler.ts:1810 / 2219 (passive unlock check against passiveAttr & 1)
 */
bool gc_starter_is_available(const gc_system *sys, const gc_starter *starter, double *cost_out, const char **why)
{
    const char *dummy;
    if (!why) why = &dummy;
    if (!starter) { *why = "null starter"; return false; }

    int sp_id = starter->species;
    int base_c = gc_starter_cost(sp_id);
    if (base_c < 0) { *why = "not a starter species"; return false; }

    double val = gc_starter_value(sys, sp_id);
    if (cost_out) *cost_out = val;

    const pr_species *sp = gc_species(sp_id);
    if (!sp) { *why = "not a starter species"; return false; }

    uint64_t caught_attr = 0;
    uint32_t ability_attr = 0;
    uint32_t passive_attr = 0;
    uint32_t nature_attr = 0;

    if (starter->ability_index > 2 || starter->nature > 24) { *why = "bad ability or nature"; return false; }
    int form = starter->form;
    if (form >= sp->nforms) { *why = "no such form"; return false; }

    /* If sys is NULL, unconstrained mode (or run has no system attached yet) */
    if (!sys) {
        return true;
    }

    if (sp_id > GC_MAX_SPECIES_ID) { *why = "not a starter species"; return false; }

    caught_attr = sys->dex[sp_id].caught_attr;
    ability_attr = sys->starters[sp_id].ability_attr;
    passive_attr = sys->starters[sp_id].passive_attr;
    nature_attr = sys->dex[sp_id].nature_attr;

    /* starter-select-ui-handler.ts:638: species locked if not caught */
    if (!caught_attr) { *why = "starterSelectUiHandler:lockedSpecies"; return false; }

    /* starter-select-ui-handler.ts:646: shiny check */
    if (starter->shiny) {
        if (!(caught_attr & (1ULL << 1))) { *why = "starterSelectUiHandler:lockedShiny"; return false; }
        /* starter-select-ui-handler.ts:657: variant check */
        if (starter->variant > 2) { *why = "starterSelectUiHandler:lockedVariant"; return false; }
        uint64_t variant_bit = 1ULL << (4 + starter->variant);
        if (!(caught_attr & variant_bit)) { *why = "starterSelectUiHandler:lockedVariant"; return false; }
    } else {
        if (!(caught_attr & (1ULL << 0))) { *why = "starterSelectUiHandler:lockedShiny"; return false; }
    }

    /* starter-select-ui-handler.ts:678: gender check (matches upstream female ? FEMALE : MALE) */
    uint64_t gender_bit = starter->female ? (1ULL << 3) : (1ULL << 2); /* FEMALE=8 (bit 3), MALE=4 (bit 2) */
    if (!(caught_attr & gender_bit)) { *why = "starterSelectUiHandler:lockedGender"; return false; }

    /* starter-select-ui-handler.ts:688: ability check */
    bool species_has_single_ability = (sp->form0 < PR_FORM_COUNT && pr_forms[sp->form0].ability1 == pr_forms[sp->form0].ability2);
    bool has_abil1 = (ability_attr & 1U) != 0;
    bool has_abil2 = (ability_attr & 2U) != 0;
    bool has_abil_hidden = (ability_attr & 4U) != 0;
    bool unlocked_abils[3] = {
        has_abil1,
        species_has_single_ability ? (has_abil2 && !has_abil1) : has_abil2,
        has_abil_hidden
    };
    if (!unlocked_abils[starter->ability_index]) { *why = "starterSelectUiHandler:lockedAbility"; return false; }

    /* starter-select-ui-handler.ts:707: form check */
    uint64_t form_bit = 1ULL << (7 + form); /* DEFAULT_FORM=128 (bit 7) */
    if (!(caught_attr & form_bit)) { *why = "starterSelectUiHandler:lockedForm"; return false; }

    /* starter-select-ui-handler.ts:718, game-data.ts:1989: nature check */
    if (!(nature_attr & (1U << (starter->nature + 1)))) { *why = "starterSelectUiHandler:lockedNature"; return false; }

    /* starter-select-ui-handler.ts:1810,2219: passive check */
    if (starter->passive) {
        if (!(passive_attr & 1U)) { *why = "starterSelectUiHandler:lockedPassive"; return false; }
    }

    return true;
}

/* getStarterMoves(starterId, formIndex) with no egg moves unlocked, sliced to 4 */
int gc_starter_default_moves(int species, uint16_t out[4])
{
    const pr_species *sp = gc_species(species);
    int n = 0;
    if (!sp) return 0;
    for (int i = 0; i < sp->level_moves.n && n < 4; i++) {
        const pr_level_move *lm = &sp->level_moves.v[i];
        if (lm->level > 0 && lm->level <= 5) out[n++] = lm->move;
    }
    return n;
}

/* gc_default_starters (defaultStarterSpecies) is generated: gen/out/gc_starters.c */

/* ---- helpers for the app ---- */
double gc_exp_to_next_level(const gc_mon *m)
{
    const pr_species *sp = gc_species(m->b.species);
    int rate = sp ? sp->growth_rate : 0;
    if (m->b.level >= 200) return 0;
    double next = gc_level_total_exp(m->b.level + 1, rate);
    return next > m->exp ? next - m->exp : 0;
}

double gc_exp_level_fraction(const gc_mon *m)
{
    const pr_species *sp = gc_species(m->b.species);
    int rate = sp ? sp->growth_rate : 0;
    double rel = pr_run_get_level_rel_exp(m->b.level + 1, rate);
    if (rel <= 0) return 0;
    double into = m->exp - gc_level_total_exp(m->b.level, rate);
    double f = into / rel;
    return f < 0 ? 0 : f > 1 ? 1 : f;
}
