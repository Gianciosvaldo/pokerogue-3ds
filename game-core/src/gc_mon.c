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

/* Pokemon.getAbility(): PokemonSpeciesForm.getAbility(abilityIndex), and a NONE (a one-ability species at index 1, no hidden
 * ability at index 2) is the species' ability1 */
int gc_form_ability(int species, const pr_form *f, int ability_index)
{
    const int id = ability_index == 0 ? f->ability1 : ability_index == 1 ? f->ability2 : f->ability_hidden;
    return id ? id : gc_form(species, 0)->ability1;
}

bool gc_mon_fainted(const gc_mon *m) { return m->b.hp <= 0; }
/* isAllowedInBattle(): not fainted (challenges aren't modelled) */
bool gc_mon_allowed(const gc_mon *m) { return m->b.hp > 0; }

static void init_common(gc_mon *m)
{
    pr_pokemon *p = &m->b;
    p->variant = m->variant;
    p->ability_lookup_known = true;
    p->ability_index = (int8_t)m->ability_index;
    p->custom_ability = p->fusion_custom_ability = -1;
    p->lookup_ability_override = p->daily_boss_ability = -1;
    p->ability_override = -1;
    p->custom_passive = -1;
    p->status_toxic_turns = 0;
    p->status_sleep_turns = p->status_freeze_turns = -1;
    for (int i = 0; i < 4; i++) PR_MAX_PP(p)[i] = -1;
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
    p->level = level;
    memcpy(p->ivs, s->ivs, 6);
    p->nature = s->nature;
    m->ability_index = s->ability_index;
    const pr_form *f = gc_form(s->species, s->form);
    p->base_ability = p->ability = (uint16_t)gc_form_ability(s->species, f, s->ability_index);
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
    m->met_level = level;   /* Pokemon's constructor: metLevel = level, metBiome = -1 before a battle, metSpecies */
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
    /* generateFusionSpecies has already run in the constructor snapshot.
     * Preserve it both before moveset generation and in gc_refresh_enemy. */
    if (g->fusion_species > 0) {
        p->fusion_species = (uint16_t)g->fusion_species;
        p->fusion_form = (uint8_t)g->fusion_form;
        p->fusion_ability_index = (int8_t)g->fusion_ability_index;
        p->fusion_gender = (int8_t)g->fusion_gender;
        p->fusion_gender_known = true;
    }
    p->level = level;
    for (int i = 0; i < 6; i++) p->ivs[i] = (uint8_t)g->ivs[i];
    p->nature = (uint8_t)g->nature;
    m->ability_index = (uint8_t)g->ability_index;
    const pr_form *f = gc_form(species, g->form);
    p->base_ability = p->ability = (uint16_t)gc_form_ability(p->species, f, g->ability_index);
    p->gender = (int8_t)g->gender;
    p->shiny = g->shiny;
    m->variant = (uint8_t)g->variant;
    m->luck = (int8_t)g->luck;
    p->friendship = g->friendship >= 0 ? (uint8_t)g->friendship : (sp ? sp->base_friendship : 0);
    p->tera_type = (int8_t)g->tera_type;
    p->has_trainer = trainer_slot != 0;
    p->trainer_slot = (uint8_t)trainer_slot;
    /* EnemyPokemon.setBoss: bossSegments from run-core's getEncounterBossSegments, one shield fewer than
     * segments to start with (battle-core clears them as the damage breaks them; 0 = not a boss) */
    p->is_boss = g->boss_segments > 0;
    p->boss_segments = (uint8_t)(g->boss_segments > 0 ? g->boss_segments : 0);
    p->boss_segment_index = (uint8_t)(g->boss_segments > 0 ? g->boss_segments - 1 : 0);
    m->trainer_slot = (uint8_t)trainer_slot;
    m->met_wave = (int16_t)run->wave;
    m->met_level = level;
    m->met_biome = (int8_t)run->biome;
    m->met_species = (uint16_t)species;
    init_common(m);
    m->exp = gc_level_total_exp(level, sp ? sp->growth_rate : 0);
}

const pr_species_gen *gc_species_gen(int id)
{
    int lo = 0, hi = pr_species_gen_count - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (pr_species_gens[mid].id == id) return &pr_species_gens[mid];
        if (pr_species_gens[mid].id < id) lo = mid + 1; else hi = mid - 1;
    }
    return NULL;
}

#define BASE_SHINY_CHANCE 64        /* src/data/balance/rates.ts */
#define SHINY_VARIANT_CHANCE 4
#define SHINY_EPIC_CHANCE 1

/* The Pokemon constructor's rolls for a Pokemon without a data source, in upstream's order: the id (and the IVs it
 * holds), the gender (a species with a male percentage rolls randSeedFloat() * 100 <= malePercent), the form (only a
 * species without forms, which rolls nothing), trySetShiny() (no roll: the id against the player's trainer and secret
 * ids, a Shiny Charm's 2^(1 + stacks) on the threshold), the shiny variant (randSeedInt(10) on the wave seed at offset
 * id, not on `rng`), the nature (randSeedInt(25)) and the tera type (randSeedItem of its types: no roll with one). Then
 * what the constructor sets without rolling: friendship, where it was met, the experience for the level, the luck. */
bool gc_mon_roll_player(gc_run *run, pr_rng *rng, gc_mon *m, int species, int level, int ability_index, int form, int shiny)
{
    const pr_species_gen *sg = gc_species_gen(species);
    const pr_species *sp = gc_species(species);
    if (!sg || !sp || (form < 0 && form != -2 && sg->n_forms) || form >= (sg->n_forms ? sg->n_forms : 1)) return false;
    memset(m, 0, sizeof *m);
    pr_pokemon *p = &m->b;
    p->id = (uint32_t)floor(pr_rng_real_in_range(rng, 0, 4294967296.0));
    const uint32_t id = p->id;
    p->ivs[0] = (id & 0x3e000000u) >> 25;
    p->ivs[1] = (id & 0x01f00000u) >> 20;
    p->ivs[2] = (id & 0x000f8000u) >> 15;
    p->ivs[3] = (id & 0x00007c00u) >> 10;
    p->ivs[4] = (id & 0x000003e0u) >> 5;
    p->ivs[5] = id & 0x0000001fu;
    if (sg->male_percent < 0) p->gender = -1;
    else p->gender = pr_rng_frac(rng) * 100 <= sg->male_percent ? 0 : 1;
    if (form == -2) {
        const pr_enemy_gen_ctx ctx = {.species=species,.wave=run->wave,.biome=run->biome,
            .battle_type=run->battle_type,.trainer_type=run->trainer_type};
        form = pr_run_get_egg_form_index(rng,&run->run,&ctx,p->gender);
    }
    p->form = (uint8_t)(form < 0 ? 0 : form);

    if (shiny < 0) {
        const uint32_t e = (uint32_t)(run->trainer_id ^ run->secret_id), f = (id >> 16) ^ (id & 0xffffu);
        double threshold = BASE_SHINY_CHANCE;
        const int charms = gc_item_stack(run, "SHINY_CHARM");
        if (charms > 0) threshold *= pow(2, 1 + charms);
        shiny = (e ^ f) < threshold;
    }
    p->shiny = shiny != 0;
    int variant = 0;
    const pr_form_gen *fg = &pr_form_gens[sg->form_start + (sg->n_forms ? p->form : 0)];
    if (p->shiny && fg->has_variants) {
        pr_rng r = pr_run_seed_offset_rng(&run->run, (int)(id & 0xffffu), &run->run.wave_seed);
        const int rand = pr_rng_seed_int(&r, 10, 0);
        variant = rand >= SHINY_VARIANT_CHANCE ? 0 : rand >= SHINY_EPIC_CHANCE ? 1 : 2;
    }
    m->variant = (uint8_t)variant;
    p->nature = (uint8_t)pr_rng_seed_int(rng, PR_NATURE_COUNT, 0);

    p->species = (uint16_t)species;
    p->level = level;
    m->ability_index = (uint8_t)ability_index;
    const pr_form *f = gc_form(species, p->form);
    p->base_ability = p->ability = (uint16_t)gc_form_ability(species, f, ability_index);
    p->friendship = sp->base_friendship;
    m->luck = (int8_t)(p->shiny ? variant + 1 : 0);
    m->met_wave = (int16_t)run->wave;
    m->met_level = level;
    m->met_biome = (int8_t)run->biome;
    m->met_species = (uint16_t)species;
    m->pokeball = POKEBALL_TYPE_POKEBALL;

    pr_enemy_gen shape;
    memset(&shape, 0, sizeof shape);
    shape.fusion_species = -1;
    shape.form = p->form;
    int8_t types[2];
    const int n_types = pr_run_enemy_types(&shape, species, types);
    if (n_types < 1) return false;
    p->tera_type = n_types == 1 ? types[0] : types[pr_rng_integer_in_range(rng, 0, n_types - 1)];

    p->is_player = true;   /* the constructor's stats are a PlayerPokemon's */
    init_common(m);
    m->exp = gc_level_total_exp(level, sp->growth_rate);
    pr_update_passive(&run->bt, p);
    pr_calculate_stats(&run->bt, p);
    p->is_player = false;
    return true;
}

void gc_mon_set_move(gc_mon *m, int slot, int move)
{
    pr_pokemon *p = &m->b;
    if (move == 0 || slot < 0 || slot > 3) return;
    pr_set_original_move(p, slot, (uint16_t)move);
}

/* PokemonMove.getMovePp(): maxPpOverride || pp + ppUp * toDmgValue(pp / 5) */
int gc_move_max_pp(const pr_pokemon *p, int slot)
{
    if (PR_MAX_PP(p)[slot] > 0) return PR_MAX_PP(p)[slot];
    int pp = pr_moves[PR_MOVES(p)[slot]].pp;
    int step = pp / 5 < 1 ? 1 : pp / 5;
    return pp + PR_PP_UP(p)[slot] * step;
}

/* PokemonMove.isUsable's PP and implementation checks (the move restrictions Disable, Taunt, Torment,
 * Imprison, Encore... are battle-core's CommandPhase work: not checked here yet) */
bool gc_move_usable(const pr_pokemon *p, int slot, const char **why)
{
    if (slot < 0 || slot >= PR_N_MOVES(p) || !PR_MOVES(p)[slot]) { if (why) *why = ""; return false; }
    if (pr_moves[PR_MOVES(p)[slot]].unimplemented) { if (why) *why = "battle:moveNotImplemented"; return false; }
    if (PR_PP_USED(p)[slot] >= gc_move_max_pp(p, slot)) { if (why) *why = "battle:moveNoPp"; return false; }
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

/* addExp(exp): levels up to getMaxExpLevel(), and the exp is cut back to the cap's level */
void gc_mon_add_exp(gc_run *run, gc_mon *m, double exp)
{
    const pr_species *sp = gc_species(m->b.species);
    int rate = sp ? sp->growth_rate : 0;
    double max_level = pr_run_get_max_exp_level(&run->run, run->wave, false);
    double initial = m->exp;
    m->exp += exp;
    while (m->b.level < max_level && m->b.level < INT32_MAX && m->exp >= gc_level_total_exp(m->b.level + 1, rate))
        m->b.level++;
    if (m->b.level == INT32_MAX && m->b.level < max_level)
        run->run.unported |= PR_REWARD_UNPORTED_LEVEL;
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

/* correctForm in SpeciesFormEvolution.validate: preFormKey null, or the mon's getFormKey() equals it. -2 is a key no form
 * has (never matches); -3 is "" with no form that has it, which a species without forms (or a form out of range) answers */
static bool pre_form_identity(const gc_mon *m, const gc_evo *e, bool fusion)
{
    const int species = fusion ? m->b.fusion_species : m->b.species;
    const int form = fusion ? m->b.fusion_form : m->b.form;
    if (e->pre_form == -1) return true;
    if (e->pre_form >= 0) return e->pre_form == form;
    if (e->pre_form != -3) return false;
    const pr_species *s = gc_species(species);
    return !s || s->nforms == 0 || form >= s->nforms || !gc_form(species, form)->form_key[0];
}

static bool pre_form_ok(const gc_mon *m, const gc_evo *e) { return pre_form_identity(m, e, false); }

/* The move a member does not know and, as far as level-ups go, has no more chance to learn: its species learns it only at levels the member
 * has gone past (from_level, the level before the level-up) or not by level at all (a relearn or egg move). */
static bool move_missed(const gc_mon *m, int move, int from_level)
{
    const pr_species *sp = gc_species(m->b.species);
    if (!sp) return false;
    for (int i = 0; i < sp->level_moves.n; i++)
        if (sp->level_moves.v[i].move == move && sp->level_moves.v[i].level > from_level) return false;
    return true;
}

/* SpeciesEvolutionCondition.conditionsFulfilled. waive: a bit for each condition key (1u << GC_EVO_*) that counts as met whether it is or
 * not; missed_from: with a level (not -1), a move the member does not know counts as met when move_missed. 0 and -1 for every upstream
 * rule: only the football fallback (gc_mon_football_evolutions) passes any */
static bool conditions_fulfilled(gc_run *run, const gc_mon *m, const gc_evo *e, bool football_choice, bool fusion, unsigned waive, int missed_from)
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
        case GC_EVO_MOVE: ok = knows(p, c->v[0]) || (missed_from >= 0 && move_missed(m, c->v[0], missed_from)); break;
        case GC_EVO_MOVE_TYPE:
            for (int k = 0; k < PR_N_MOVES(p); k++) ok |= PR_MOVES(p)[k] && pr_moves[PR_MOVES(p)[k]].type == c->v[0];
            break;
        case GC_EVO_PARTY_TYPE:
            for (int k = 0; k < run->n_party; k++) {
                const pr_form *f = gc_form(run->party[k].b.species, run->party[k].b.form);
                ok |= f->type1 == c->v[0] || f->type2 == c->v[0];
            }
            break;
        case GC_EVO_GENDER: ok = (fusion ? p->fusion_gender : p->gender) == c->v[0]; break;
        case GC_EVO_SHEDINJA: ok = football_choice; break;
        case GC_EVO_BIOME: ok = has_value(c, run->biome); break;
        case GC_EVO_WEATHER: ok = has_value(c, run->bt.weather); break;
        case GC_EVO_TYROGUE: {
            /* the first of Low Sweep, Mach Punch, Rapid Spin in the moveset is the one named */
            int first = 0;
            for (int k = 0; k < PR_N_MOVES(p) && !first; k++)
                if (PR_MOVES(p)[k] == MOVE_ID_LOW_SWEEP || PR_MOVES(p)[k] == MOVE_ID_MACH_PUNCH || PR_MOVES(p)[k] == MOVE_ID_RAPID_SPIN)
                    first = PR_MOVES(p)[k];
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
        case GC_EVO_HELD_ITEM: {
            /* getHeldItems().some(SpeciesStatBoosterModifier with this key): "SPECIES_STAT_BOOSTER" and its rare twin carry
             * the key as their argument, in the order of SpeciesStatBoosterModifierTypeGenerator.items */
            static const char *const keys[] = {"LIGHT_BALL", "THICK_CLUB", "METAL_POWDER", "QUICK_POWDER", "DEEP_SEA_SCALE", "DEEP_SEA_TOOTH"};
            int want = -1;
            for (int k = 0; k < 6; k++) if (c->item_key && !strcmp(c->item_key, keys[k])) want = k;
            for (int k = 0; want >= 0 && k < run->n_items && !ok; k++)
                ok = run->items[k].pokemon_id == p->id && run->items[k].arg == want
                     && (!strcmp(run->items[k].key, "SPECIES_STAT_BOOSTER") || !strcmp(run->items[k].key, "RARE_SPECIES_STAT_BOOSTER"));
            break;
        }
        case GC_EVO_TREASURE_TRACKER: {
            /* Count objects, not their stacks. A tracker must exist even if
             * the persistent treasure alone reaches the threshold. */
            int treasure = 0;
            for (int k = 0; k < run->n_items; k++) {
                const gc_item *it = &run->items[k];
                treasure += (!strcmp(it->key, "GOLDEN_PUNCH") && it->pokemon_id == p->id)
                    || !strcmp(it->key, "AMULET_COIN") || !strcmp(it->key, "GOLDEN_POKEBALL");
            }
            for (int k = 0; k < run->n_items; k++) {
                const gc_item *it = &run->items[k];
                if (it->pokemon_id == p->id && !strcmp(it->key, "EVOLUTION_TRACKER_GIMMIGHOUL")
                    && (int64_t)it->stack + it->virtual_stack + treasure >= c->v[0]) { ok = true; break; }
            }
            break;
        }
        }
        if (!ok && !((waive >> c->key) & 1u)) return false;
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
        if (!pre_form_ok(m, e)) continue;                         /* getFormKey() === preFormKey */
        if (!conditions_fulfilled(run, m, e, false, false, 0, -1)) continue;
        return se->start + i;
    }
    return -1;
}

/* Pokemon.getEvolution: primary candidates first, then secondary candidates.
 * Secondary validation changes only the form/gender reads. */
int gc_mon_get_evolution_target(gc_run *run, const gc_mon *m, bool *fusion)
{
    *fusion = false;
    int evo = gc_mon_get_evolution(run, m);
    if (evo >= 0 || !m->b.fusion_species) return evo;
    const gc_species_evo *se = species_evo(m->b.fusion_species);
    if (!se) return -1;
    for (int i = 0; i < se->n; i++) {
        const gc_evo *e = &gc_evo_data[se->start + i];
        if (e->item || m->b.level < e->level || !pre_form_identity(m, e, true)) continue;
        if (!m->b.fusion_gender_known) {
            for (int j = 0; j < e->n_cond; j++) if (gc_evo_conds[e->cond_start + j].key == GC_EVO_GENDER) {
                pr_note_unported(&run->bt, PR_FEAT_FORM_CHANGE);
                return -1; /* an earlier gender-gated candidate cannot be guessed */
            }
        }
        if (!conditions_fulfilled(run, m, e, false, true, 0, -1)) continue;
        *fusion = true;
        return se->start + i;
    }
    return -1;
}

/* What a football member has no way to meet: weather and a party type (a member plays alone, in no biome's weather), a number of treasures,
 * and a gender it was not drafted with. A move it missed is the same (move_missed): Annihilape's Rage Fist, which a ten-level jump passes
 * while the member is still a Mankey, or Mamoswine's Ancient Power, which Piloswine only relearns. */
#define FOOTBALL_WAIVED ((1u << GC_EVO_WEATHER) | (1u << GC_EVO_PARTY_TYPE) | (1u << GC_EVO_TREASURE_TRACKER) | (1u << GC_EVO_GENDER))

static int football_evolutions(gc_run *run, const gc_mon *m, int *out, int cap, unsigned waive, int missed_from)
{
    const gc_species_evo *se = species_evo(m->b.species);
    if (!se || m->pause_evolutions) return 0;
    int n = 0;
    for (int i = 0; i < se->n; ++i) {
        const gc_evo *e = &gc_evo_data[se->start + i];
        if (e->item || m->b.level < e->level || !pre_form_ok(m,e)
            || !conditions_fulfilled(run,m,e,true,false,waive,missed_from)) continue;
        if (n == cap) return -1;
        out[n++] = se->start + i;
    }
    return n;
}

/* Football offers validated split outcomes instead of silently taking the first.
 * The Shedinja rule is a choice of replacement, rather than an extra spawn.
 * A species with none it can take by those rules takes the ones that ask for nothing but what a football member cannot give
 * (FOOTBALL_WAIVED, move_missed): so every level evolution happens by its level, while the ones that work keep working as they did.
 * from_level: the member's level before this level-up. */
int gc_mon_football_evolutions(gc_run *run, const gc_mon *m, int from_level, int *out, int cap)
{
    const int n = football_evolutions(run, m, out, cap, 0, -1);
    return n == 0 ? football_evolutions(run, m, out, cap, FOOTBALL_WAIVED, from_level) : n;
}

/* EvolutionItemModifier.apply, not a fusion: the first of the species' evolutions whose item matches and validates
 * (level, form, conditions); -1 = none. Mirrors getEvolution() with an item instead of NONE. */
int gc_mon_item_evolution(gc_run *run, const gc_mon *m, int item)
{
    const gc_species_evo *se = species_evo(m->b.species);
    if (!se) return -1;
    for (int i = 0; i < se->n; i++) {
        const gc_evo *e = &gc_evo_data[se->start + i];
        if (e->item != item || m->b.level < e->level || !pre_form_ok(m, e)) continue;
        if (!conditions_fulfilled(run, m, e, false, false, 0, -1)) continue;
        return se->start + i;
    }
    return -1;
}

/* The items that would evolve the member now: for each distinct item in the species' table, the evolution gc_mon_item_evolution
 * picks for it (indexes into gc_evo_data, in the table's order). Football's shop sells these; -1 is never returned. */
int gc_mon_item_evolutions(gc_run *run, const gc_mon *m, int *out, int cap)
{
    const gc_species_evo *se = species_evo(m->b.species);
    if (!se || m->pause_evolutions || m->b.fusion_species) return 0;
    int n = 0;
    for (int i = 0; i < se->n && n < cap; i++) {
        const gc_evo *e = &gc_evo_data[se->start + i];
        if (!e->item) continue;
        bool seen = false;
        for (int k = 0; k < n; k++) seen |= gc_evo_data[out[k]].item == e->item;
        if (seen) continue;
        const int evo = gc_mon_item_evolution(run, m, e->item);
        if (evo >= 0) out[n++] = evo;
    }
    return n;
}

/* Pokemon.evolve: mutate the selected half, form and ability index, then stats.
 * The separate Nincada/Shedinja extra-party owner remains outside this helper. */
void gc_mon_evolve_target(gc_run *run, gc_mon *m, int evo, bool fusion)
{
    const gc_evo *e = &gc_evo_data[evo];
    pr_pokemon *p = &m->b;
    const int pre_species = fusion ? p->fusion_species : p->species;
    const pr_form *pre = gc_form(pre_species, fusion ? p->fusion_form : p->form);
    int pre_count = pre->ability_hidden ? 3 : 2;
    m->pause_evolutions = false;
    if (fusion) p->fusion_species = e->species;
    else p->species = e->species;
    if (e->pre_form != -1) {
        uint8_t form = (uint8_t)(e->evo_form >= 0 ? e->evo_form : 0);
        if (fusion) p->fusion_form = form; else p->form = form;
    }
    const pr_form *f = gc_form(e->species, fusion ? p->fusion_form : p->form);
    int count = f->ability_hidden ? 3 : 2;
    int index = fusion ? p->fusion_ability_index : m->ability_index;
    if (index < 0 || index > 2) index = 0;
    if (index == 2 && pre_count == 3 && count == 2) index = 1;
    if (fusion) p->fusion_ability_index = (int8_t)index;
    else { m->ability_index = (uint8_t)index; p->ability_index = (int8_t)index; }
    const int effective_species = p->fusion_species ? p->fusion_species : p->species;
    const int effective_form = p->fusion_species ? p->fusion_form : p->form;
    const pr_form *effective = gc_form(effective_species, effective_form);
    /* getAbility's fusion branch returns NONE directly; only the primary
     * branch falls back to its species' first ability. */
    p->base_ability = p->fusion_species
        ? (p->fusion_ability_index == 0 ? effective->ability1
           : p->fusion_ability_index == 1 ? effective->ability2 : effective->ability_hidden)
        : (uint16_t)gc_form_ability(effective_species, effective, m->ability_index);
    if (p->ability_override <= 0) p->ability = p->base_ability;
    pr_refresh_ability(p);
    if (pre_species == SPECIES_ID_GIMMIGHOUL) {
        for (int k = 0; k < run->n_items; k++) {
            if (run->items[k].pokemon_id == p->id
                && !strcmp(run->items[k].key, "EVOLUTION_TRACKER_GIMMIGHOUL")) {
                gc_item_remove_at(run, k); break; /* source removes the first tracker only */
            }
        }
        gc_items_to_battle(run);
    }
    gc_mon_recalc(run, m);

    if (run->sys) {
        gc_system_update_species_dex_ivs(run->sys, m->b.species, m->b.ivs);
        gc_system_set_pokemon_seen(run->sys, m, false, false);
        gc_system_set_pokemon_caught(run->sys, m, false, false);
    }
}

void gc_mon_evolve(gc_run *run, gc_mon *m, int evo)
{
    gc_mon_evolve_target(run, m, evo, false);
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

/* A starter handed over by the starter select screen, checked against the system data (upstream at 8555c08).
 * Upstream's UI offers only unlocked options and initStarterPrefs (starter-select-ui-handler.ts:634-736) purges any
 * stored preference that is not unlocked; here an unlocked-or-not mismatch is a rejection and nothing is mutated.
 *   :647      !dexEntry.caughtAttr                      -> lockedSpecies
 *   :655-662  shiny / non-shiny against SHINY / NON_SHINY
 *   :664-678  variant against DEFAULT_VARIANT / VARIANT_2 / VARIANT_3, only when shiny is unlocked; an index above 2
 *             is not unlocked (unlockedVariants[3] is undefined)
 *   :680-686  gender: female ? FEMALE : MALE (see the gender comment below for the default)
 *   :688-705  abilityIndex against abilityAttr; a single-ability species counts ability 2 only when ability 1 is locked
 *   :707-715  form against getFormAttr(form) = 1 << (7 + form)
 *   :717-723  nature against getNaturesForAttr(natureAttr): bit (nature + 1), game-data.ts:1989-1997
 *   :2219     passive: only when passiveAttr has UNLOCKED (1); starter-select-ui-handler.ts:1809-1810
 *   :3038-3039 cost: the party's getSpeciesStarterValue sum must not exceed the run's value limit (game-data.ts:2018)
 * Not done, the game-core data has nothing for them: a form's isStarterSelectable (:710), the tera type must be one of
 * the species' types (:725-734), and the challenge filters (FRESH_START, STARTER_COST, STARTER_POINTS).
 * With sys == NULL (no system data attached to the run) nothing is locked: only the structural checks run. */
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

    /* :647 species locked if never caught */
    if (!caught_attr) { *why = "starterSelectUiHandler:lockedSpecies"; return false; }

    /* :655-662 shiny / non-shiny */
    if (starter->shiny) {
        if (!(caught_attr & (1ULL << 1))) { *why = "starterSelectUiHandler:lockedShiny"; return false; }
        /* :664-678 variant (range first: 4 + variant is a shift, and bit 7 would alias DEFAULT_FORM) */
        if (starter->variant > 2) { *why = "starterSelectUiHandler:lockedVariant"; return false; }
        uint64_t variant_bit = 1ULL << (4 + starter->variant);
        if (!(caught_attr & variant_bit)) { *why = "starterSelectUiHandler:lockedVariant"; return false; }
    } else {
        if (!(caught_attr & (1ULL << 0))) { *why = "starterSelectUiHandler:lockedShiny"; return false; }
    }

    /* Gender (starter-select-ui-handler.ts:680-686): a preference is kept only if its bit is in caughtAttr. Without a
     * preference upstream uses getSpeciesDefaultDexAttrProps (game-data.ts:1923): female only for malePercent == 0, so the
     * default gender needs no bit. That matters for a genderless species: pokemon.ts:611-613 sets neither MALE nor
     * FEMALE for it, so its caughtAttr never has a gender bit and requiring one would make it unselectable.
     * So female is allowed when it is the species' default or its bit is set. */
    bool default_female = sp->male_permille == 0;
    uint64_t gender_bit = starter->female ? (1ULL << 3) : (1ULL << 2); /* FEMALE = 8, MALE = 4 */
    if (starter->female != default_female && !(caught_attr & gender_bit)) { *why = "starterSelectUiHandler:lockedGender"; return false; }

    /* :688-705 ability */
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

    /* :707-715 form */
    uint64_t form_bit = 1ULL << (7 + form); /* DEFAULT_FORM=128 (bit 7) */
    if (!(caught_attr & form_bit)) { *why = "starterSelectUiHandler:lockedForm"; return false; }

    /* :717-723, game-data.ts:1989-1997 nature */
    if (!(nature_attr & (1U << (starter->nature + 1)))) { *why = "starterSelectUiHandler:lockedNature"; return false; }

    /* :1809-1810, :2219 passive */
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
    if (m->b.level == INT32_MAX) return 0;
    double next = gc_level_total_exp(m->b.level + 1, rate);
    return next > m->exp ? next - m->exp : 0;
}

double gc_exp_level_fraction(const gc_mon *m)
{
    if (m->b.level == INT32_MAX) return 0;
    const pr_species *sp = gc_species(m->b.species);
    int rate = sp ? sp->growth_rate : 0;
    double rel = pr_run_get_level_rel_exp(m->b.level + 1, rate);
    if (rel <= 0) return 0;
    double into = m->exp - gc_level_total_exp(m->b.level, rate);
    double f = into / rel;
    return f < 0 ? 0 : f > 1 ? 1 : f;
}
