/* Weird Dream (src/data/mystery-encounters/encounters/weird-dream-encounter.ts, upstream 8555c08).
 *
 * onInit (executeWithSeedOffset(.., waveIndex)): the party is shuffled (randSeedShuffle) and, in that order, each member gets a new species whose base stat total is
 * its own plus 90..110 (the first two) or 40..50 (the others), from the registry's species (getAllSpecies(), in id order) without Arceus, Eternatus, the Ultra Beasts, the
 * Paradox Pokemon and the legendaries that must stay out of the pool below 570; at most one result above 600 and one from 570 to 600; a window of 20 species or fewer
 * is widened by 10 each way until 21 fit; the candidates are shuffled and popped until one the party and the earlier results do not hold. The widening is written
 * into the two module-level range arrays upstream shares between calls: it stays for every later Weird Dream of the page's life (a static here, for the life of
 * the process). Then, for every member in party order, randSeedInt(ability count), the new PlayerPokemon at the old one's level (the constructor's own rolls)
 * and randSeedInt(18) for its Tera type. The held items of each member but its form change items are kept for later.
 *
 * Option 1 (the transformation): onPreOptionPhase (the offset stream, no rolls) takes each old Pokemon out of the party and puts its new one at the end, one at a time
 * (each removal prunes the items that lost their holder). onOptionPhase: everything after its first await is on the RUN's RNG. For every Pokemon in party order, the
 * postProcess: a second hidden ability roll (Ability Charm), the IVs rolled again (the larger of the two), a neutral nature, for a species of base stat total 570
 * or less (or a shiny) its place in the dex (IVs, caught, a message when it is a new starter), the old one's Pokerus and luck, the IVs at least the old ones,
 * a candy, the generated moveset kept aside while the old moveset is copied, one egg move of its root species added (unlocked when the dex holds the species),
 * one move of its first type from the generated set, a random second type and the old one's passive flag. Its items come back (a Pokemon of 570 or less gets an
 * Old Gateau), the stats are calculated again and the HP ratio, the status (unless it is Comatose) or the faint come across. Then one Pokemon without a passive gets
 * it. PartyHealPhase(true) and the shop of a Memory Mushroom, a Rogue Ball and four Mints follow.
 *
 * Option 3 (refuse): every level drops by a tenth (rounded up, at least 1), the experience goes back to the level's start, and the heal-only shop follows.
 *
 * Not ported: the achievements the transformation validates (hidden ability, catching a sub-legendary, legendary or mythical); a species whose form changes on learning
 * a move (Keldeo with Secret Sword) keeps its form instead of changing it in a phase of its own. */
#include "internal.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define WD_PARTY 6
#define SUPER_LEGENDARY_BST 600
#define NON_LEGENDARY_BST 570

/* module-level in upstream: HIGH_BST_TRANSFORM_BASE_VALUES and STANDARD_BST_TRANSFORM_BASE_VALUES, widened in place by getTransformedSpecies */
static int range[2][2] = {{90, 110}, {40, 50}};
void gc_weird_reset_ranges(void) { gc_weird_set_ranges(0, 0); }

/* tests: the ranges as widened by `high` and `standard` steps of 10 each way */
void gc_weird_set_ranges(int high, int standard)
{
    range[0][0] = 90 - 10 * high; range[0][1] = 110 + 10 * high;
    range[1][0] = 40 - 10 * standard; range[1][1] = 50 + 10 * standard;
}
void gc_weird_get_ranges(int *high, int *standard)
{
    *high = (90 - range[0][0]) / 10;
    *standard = (40 - range[1][0]) / 10;
}

static const uint16_t excluded[] = {
    SPECIES_ID_ARCEUS, SPECIES_ID_ETERNATUS,
    SPECIES_ID_NIHILEGO, SPECIES_ID_BUZZWOLE, SPECIES_ID_PHEROMOSA, SPECIES_ID_XURKITREE, SPECIES_ID_CELESTEELA, SPECIES_ID_KARTANA, SPECIES_ID_GUZZLORD,
    SPECIES_ID_POIPOLE, SPECIES_ID_NAGANADEL, SPECIES_ID_STAKATAKA, SPECIES_ID_BLACEPHALON,
    SPECIES_ID_GREAT_TUSK, SPECIES_ID_SCREAM_TAIL, SPECIES_ID_BRUTE_BONNET, SPECIES_ID_FLUTTER_MANE, SPECIES_ID_SLITHER_WING, SPECIES_ID_SANDY_SHOCKS,
    SPECIES_ID_ROARING_MOON, SPECIES_ID_WALKING_WAKE, SPECIES_ID_GOUGING_FIRE, SPECIES_ID_RAGING_BOLT, SPECIES_ID_IRON_TREADS, SPECIES_ID_IRON_BUNDLE,
    SPECIES_ID_IRON_HANDS, SPECIES_ID_IRON_JUGULIS, SPECIES_ID_IRON_MOTH, SPECIES_ID_IRON_THORNS, SPECIES_ID_IRON_VALIANT, SPECIES_ID_IRON_LEAVES,
    SPECIES_ID_IRON_BOULDER, SPECIES_ID_IRON_CROWN,
    SPECIES_ID_PHIONE, SPECIES_ID_TYPE_NULL, SPECIES_ID_COSMOG, SPECIES_ID_COSMOEM, SPECIES_ID_MELTAN, SPECIES_ID_KUBFU, SPECIES_ID_URSHIFU, SPECIES_ID_CALYREX,
    SPECIES_ID_OGERPON, SPECIES_ID_OKIDOGI, SPECIES_ID_MUNKIDORI, SPECIES_ID_FEZANDIPITI, SPECIES_ID_TERAPAGOS,
};

static bool is_excluded(int id)
{
    for (size_t i = 0; i < sizeof excluded / sizeof excluded[0]; i++) if (excluded[i] == id) return true;
    return false;
}

static int species_bst(int id)
{
    const pr_form *f = gc_form(id, 0);
    return f ? f->base_total : 0;
}

/* getTransformedSpecies: 0 when every candidate is used (upstream would loop for ever) */
static int transformed_species(pr_rng *r, int original_bst, int *rng_range, bool has_super, bool has_legend, const uint16_t *used, int n_used)
{
    static uint16_t list[PR_SPECIES_COUNT];
    for (int guard = 0; guard < 200; guard++) {
        const int cap = original_bst + rng_range[1];
        const int min = original_bst + rng_range[0] > 0 ? original_bst + rng_range[0] : 0;
        int n = 0;
        for (int i = 0; i < PR_SPECIES_COUNT; i++) {
            const int id = pr_species_table[i].id;
            const int bst = species_bst(id);
            const bool in_range = bst >= min && bst <= cap;
            const bool valid_bst = (!has_legend || bst < NON_LEGENDARY_BST || bst > SUPER_LEGENDARY_BST) && (!has_super || bst <= SUPER_LEGENDARY_BST);
            if (in_range && valid_bst && !is_excluded(id)) list[n++] = (uint16_t)id;
        }
        if (n > 20) {
            for (int i = n - 1; i > 0; i--) {
                const int j = pr_rng_integer_in_range(r, 0, i);
                const uint16_t t = list[i];
                list[i] = list[j];
                list[j] = t;
            }
            while (n > 0) {
                const int id = list[--n];
                bool seen = false;
                for (int k = 0; k < n_used; k++) if (used[k] == id) { seen = true; break; }
                if (!seen) return id;
            }
            return 0;
        }
        rng_range[0] -= 10;
        rng_range[1] += 10;
    }
    return 0;
}

void gc_weird_start(gc_run *run) { gc_int(run)->weird.valid = false; }

/* getTeamTransformations, the species half (the shuffle and the species), once per encounter */
bool gc_weird_plan(gc_run *run)
{
    gc_internal *in = gc_int(run);
    if (in->weird.valid && in->weird.wave == run->wave && in->weird.n == run->n_party) return true;
    in->weird.valid = false;
    const int n = run->n_party;
    if (n < 1 || n > WD_PARTY) return false;
    pr_rng r = pr_run_seed_offset_rng(&run->run, run->wave, NULL);
    uint16_t used[WD_PARTY * 2];
    int n_used = 0;
    int shuffled[WD_PARTY];
    for (int k = 0; k < n; k++) { used[n_used++] = run->party[run->party_order[k]].b.species; shuffled[k] = k; }
    for (int i = n - 1; i > 0; i--) {
        const int j = pr_rng_integer_in_range(&r, 0, i);
        const int t = shuffled[i];
        shuffled[i] = shuffled[j];
        shuffled[j] = t;
    }
    bool has_super = false, has_legend = false;
    uint16_t species[WD_PARTY] = {0};
    for (int i = 0; i < n; i++) {
        const int pos = shuffled[i];
        const gc_mon *removed = &run->party[run->party_order[pos]];
        const pr_form *f = gc_form(removed->b.species, removed->b.form);
        const int bst = f ? f->base_total : 0;
        const int id = transformed_species(&r, bst, range[i < 2 ? 0 : 1], has_super, has_legend, used, n_used);
        if (!id) return false;
        const int new_bst = species_bst(id);
        if (new_bst > SUPER_LEGENDARY_BST) has_super = true;
        if (new_bst <= SUPER_LEGENDARY_BST && new_bst >= NON_LEGENDARY_BST) has_legend = true;
        species[pos] = (uint16_t)id;
        used[n_used++] = (uint16_t)id;
    }
    in->weird.n = (uint8_t)n;
    in->weird.wave = (int16_t)run->wave;
    memcpy(in->weird.species, species, sizeof species);
    in->weird.s1 = r;
    in->weird.valid = true;
    return true;
}

int gc_weird_species(gc_run *run, int position)
{
    if (!gc_weird_plan(run) || position < 0 || position >= run->n_party) return 0;
    return gc_int(run)->weird.species[position];
}

/* getTeamTransformations, the Pokemon half: for each member in party order randSeedInt(getAbilityCount()), the PlayerPokemon constructor and randSeedInt(MAX_POKEMON_TYPE).
 * Makes all of them (each draws from the stream the one before left). */
static bool build_all(gc_run *run, gc_mon *out)
{
    gc_internal *in = gc_int(run);
    if (!gc_weird_plan(run)) return false;
    pr_rng r = in->weird.s1;
    for (int k = 0; k < run->n_party; k++) {
        const int species = in->weird.species[k];
        const gc_mon *prev = &run->party[run->party_order[k]];
        const pr_form *f = gc_form(species, 0);
        if (!f) return false;
        const int ability_count = f->ability_hidden ? 3 : 2;
        const int ability = pr_rng_seed_int(&r, ability_count, 0);
        /* getSpeciesFormIndex(.., isPlayer): a Gimmighoul of a mystery-encounter game mode is the Wandering form without a roll */
        const int form = species == SPECIES_ID_GIMMIGHOUL ? 1 : -2;
        if (!gc_mon_roll_player(run, &r, &out[k], species, prev->b.level, ability, form, -1)) return false;
        out[k].b.tera_type = (int8_t)pr_rng_seed_int(&r, 18, 0);
        out[k].b.is_player = true;
    }
    return true;
}

bool gc_weird_new_pokemon(gc_run *run, int position, gc_mon *out)
{
    static gc_mon all[WD_PARTY];
    if (position < 0 || position >= run->n_party || !build_all(run, all)) return false;
    *out = all[position];
    return true;
}

/* ---------------------------------------------------------------- the transformation */
static void sync_party(gc_run *run)
{
    run->bt.n_party[0] = (uint8_t)run->n_party;
    for (int k = 0; k < run->n_party; k++) run->bt.party[0][k] = &run->party[run->party_order[k]].b;
    gc_items_to_battle(run);
}

static void put_move(gc_mon *m, int slot, int move)
{
    m->b.moves[slot] = (uint16_t)move;
    m->b.pp_used[slot] = 0;
    m->b.pp_up[slot] = 0;
    m->b.max_pp_override[slot] = -1;
    if (slot >= m->b.n_moves) m->b.n_moves = (uint8_t)(slot + 1);
}

static bool knows(const gc_mon *m, int move)
{
    for (int k = 0; k < m->b.n_moves; k++) if (m->b.moves[k] == move) return true;
    return false;
}

/* addEggMoveToNewPokemonMoveset: the index of the move it replaced in a full moveset, -1 when it added one (or none) */
static int add_egg_move(gc_run *run, gc_mon *m, int root, bool for_battle)
{
    int egg_index = -1;
    const gc_starter_egg_moves *eggs = NULL;
    for (int i = 0; i < gc_starter_egg_moves_count; i++)
        if (gc_starter_egg_moves_data[i].species == root) { eggs = &gc_starter_egg_moves_data[i]; break; }
    if (!eggs) return -1;
    int indices[4] = {0, 1, 2, 3}, n = 4;
    for (int i = 3; i > 0; i--) {
        const int j = pr_rng_integer_in_range(&run->run.rng, 0, i);
        const int t = indices[i];
        indices[i] = indices[j];
        indices[j] = t;
    }
    int idx = n > 0 ? indices[--n] : -1;
    int egg = idx < 0 ? 0 : eggs->egg_moves[idx];
    int retries = 0;
    while (retries < 3 && (!egg || knows(m, egg))) {
        idx = n > 0 ? indices[--n] : -1;
        egg = idx < 0 ? 0 : eggs->egg_moves[idx];
        retries++;
    }
    if (egg) {
        if (!knows(m, egg)) {
            if (m->b.n_moves < 4) put_move(m, m->b.n_moves, egg);
            else {
                egg_index = pr_rng_seed_int(&run->run.rng, 4, 0);
                put_move(m, egg_index, egg);
            }
        }
        if (!for_battle && idx >= 0 && run->sys && run->sys->dex[root].caught_attr) run->sys->starters[root].egg_moves |= (uint8_t)(1u << idx);
    }
    return egg_index;
}

/* addFavoredMoveToNewPokemonMoveset: the first generated move of the new first type it does not know, else the first it does not know */
static void add_favored_move(gc_run *run, gc_mon *m, const gc_mon *generated, int egg_index)
{
    int8_t types[PR_MAX_TYPES];
    const int n_types = pr_get_types(&m->b, types, 0);
    int favored = 0;
    for (int k = 0; k < generated->b.n_moves; k++) {
        const int move = generated->b.moves[k];
        if (n_types > 0 && pr_moves[move].type == types[0] && !knows(m, move)) { favored = move; break; }
    }
    if (!favored) {
        for (int k = 0; k < generated->b.n_moves; k++) {
            if (!knows(m, generated->b.moves[k])) { favored = generated->b.moves[k]; break; }
        }
    }
    if (!favored) return;
    if (m->b.n_moves < 4) { put_move(m, m->b.n_moves, favored); return; }
    int slot = pr_rng_seed_int(&run->run.rng, 4, 0);
    while (egg_index >= 0 && slot == egg_index) slot = pr_rng_seed_int(&run->run.rng, 4, 0);
    put_move(m, slot, favored);
}

/* whether setPokemonCaught(.., showMessage false) reports a new starter: the first species of the line (from the new one down the prevolutions)
 * that is a starter and not caught yet (it stops the walk down) */
static bool unlocks_new_starter(const gc_run *run, int species)
{
    for (int depth = 0; depth < 10 && species > 0; depth++) {
        if (run->sys->dex[species].caught_attr == 0 && gc_starter_cost(species) >= 0) return true;
        const int pre = gc_species_prevolution((uint16_t)species);
        if (pre <= 0) break;
        species = pre;
    }
    return false;
}

/* postProcessTransformedPokemon (forBattle false): the Pokemon `m` the party member `prev` becomes */
static bool post_process(gc_run *run, const gc_mon *prev, gc_mon *m, bool for_battle, bool option_stream)
{
    /* the first member's postProcess runs in the option's seed offset up to its first await (the one on addEggMoveToNewPokemonMoveset): its rolls up to and with the egg move are drawn
     * from the option's stream, which restarts from its seed; everything after is on the run's RNG, as is every later member's */
    const pr_rng run_rng = run->run.rng;
    if (option_stream) run->run.rng = gc_me_option_rng(run);
    bool is_new_starter = false;
    pr_pokemon *p = &m->b;
    const int root = gc_root_species(p->species);
    const pr_form *base = gc_form(p->species, 0);
    static gc_mon ctor;   /* the Pokemon as its constructor made it: the moveset generator reads its stats (Pokemon.stats is only recalculated by calculateStats) */
    ctor = *m;

    /* a second hidden ability roll */
    if (base->ability_hidden) {
        const int hidden_index = base->ability2 ? 2 : 1;
        if (m->ability_index < hidden_index) {
            double chance = 256;
            const int charms = gc_item_stack(run, "ABILITY_CHARM");
            if (charms > 0) chance *= js_pow(2, -1 - charms);   /* HiddenAbilityRateBoosterModifier.apply: value *= 2^(-1 - stacks) */
            if (!pr_rng_seed_int(&run->run.rng, (int)chance, 0)) {
                m->ability_index = (uint8_t)hidden_index;
                p->ability_index = (int8_t)hidden_index;
                p->ability = p->base_ability = (uint16_t)gc_form_ability(p->species, gc_form(p->species, p->form), hidden_index);
                pr_refresh_ability(p);
            }
        }
    }
    /* the IVs a second time */
    for (int k = 0; k < 6; k++) {
        const int v = pr_rng_seed_int(&run->run.rng, 31, 0);
        if (v > p->ivs[k]) p->ivs[k] = (uint8_t)v;
    }
    /* a neutral nature (assigned, not set: the stats stay as they were) */
    {
        static const uint8_t neutral[5] = {NATURE_HARDY, NATURE_DOCILE, NATURE_BASHFUL, NATURE_QUIRKY, NATURE_SERIOUS};
        const uint8_t nature = neutral[pr_rng_seed_int(&run->run.rng, 5, 0)];
        p->nature = nature;
        m->base_nature = nature;
        m->has_custom_nature = false;
    }
    /* at or below 570, or shiny: unlocked as if caught */
    if (!for_battle && run->sys && (gc_form(p->species, p->form)->base_total <= NON_LEGENDARY_BST || p->shiny)) {
        gc_system_update_species_dex_ivs(run->sys, gc_species_root_for_starter(p->species), p->ivs);
        is_new_starter = unlocks_new_starter(run, p->species);
        gc_system_set_pokemon_caught_ex(run->sys, m, true, false, false, run->run.mode == GAME_MODES_DAILY, NULL);
        if (is_new_starter) {
            pr_msg_arg name = {"pokemonName", PR_ARG_POKEMON_NAME, pr_msg_pokemon((uint16_t)root, 0, 0), NULL};
            gc_ev_msg(run, "battle:addedAsAStarter", 1, name);
        }
    }
    m->pokerus = prev->pokerus;
    m->luck = (int8_t)(prev->luck + (prev->b.fusion_species ? prev->fusion_luck : 0));
    for (int k = 0; k < 6; k++) if (prev->b.ivs[k] > p->ivs[k]) p->ivs[k] = prev->b.ivs[k];
    /* a candy for a species the player owns */
    if (!for_battle && run->sys && run->sys->dex[root].caught_attr) gc_system_add_starter_candy(run->sys, (uint16_t)root, 1);

    /* the generated moveset kept aside, the old one copied, an egg move and a favored move */
    static gc_mon generated;
    generated = *m;
    if (!gc_generate_moveset_player(run, &run->run.rng, &generated, &ctor)) {
        gc_stub(run, GC_STUB_ENEMY_MOVESET);
        generated.b.n_moves = 0;
        memset(generated.b.moves, 0, sizeof generated.b.moves);
    }
    p->n_moves = prev->b.n_moves;
    memcpy(p->moves, prev->b.moves, sizeof p->moves);
    memcpy(p->pp_used, prev->b.pp_used, sizeof p->pp_used);
    memcpy(p->pp_up, prev->b.pp_up, sizeof p->pp_up);
    memcpy(p->max_pp_override, prev->b.max_pp_override, sizeof p->max_pp_override);
    const int egg_index = add_egg_move(run, m, root, for_battle);
    if (option_stream) run->run.rng = run_rng;
    add_favored_move(run, m, &generated, egg_index);

    /* a random second type */
    p->custom_types[0] = 0;
    p->custom_types[1] = (uint8_t)(pr_rng_seed_int(&run->run.rng, 18, 0) + 1);
    /* the old one's passive */
    p->passive_flag = prev->b.passive_flag;
    pr_update_passive(&run->bt, p);
    return is_new_starter;
}

/* ---------------------------------------------------------------- the option phases */
static int find_by_id(const gc_run *run, uint32_t id)
{
    for (int i = 0; i < run->n_party; i++) if (run->party[i].b.id == id) return i;
    return -1;
}

/* addModifier(item, false, false, false, true) of a held item for `m`: an equal item already held stacks when the stack fits (incrementStack), else it is dropped;
 * a new one goes at the end of the list whatever its stack */
static void add_held(gc_run *run, const gc_mon *m, gc_item it)
{
    it.pokemon_id = m->b.id;
    it.virtual_stack = 0;
    /* the updateModifiers that follows every addModifier drops a held item that names a species (the Gimmighoul evolution tracker) its holder is not */
    if (!strcmp(it.key, "EVOLUTION_TRACKER_GIMMIGHOUL") && m->b.species != SPECIES_ID_GIMMIGHOUL && m->b.fusion_species != SPECIES_ID_GIMMIGHOUL) return;
    for (int i = 0; i < run->n_items; i++) {
        gc_item *have = &run->items[i];
        if (!strcmp(have->key, it.key) && have->arg == it.arg && have->pokemon_id == it.pokemon_id && !have->eviolite_base) {
            if (have->stack + it.stack <= gc_item_max_held(run, it.key, it.arg, m)) have->stack += it.stack;
            return;
        }
    }
    if (run->n_items >= PR_MAX_MODIFIERS || run->n_items >= GC_MAX_ITEMS) { pr_note_unported(&run->bt, PR_FEAT_ENGINE_LIMIT); return; }
    run->items[run->n_items++] = it;
}

static bool is_comatose(gc_run *run, const gc_mon *m)
{
    return m->b.base_ability == ABILITY_ID_COMATOSE
        || (pr_has_passive(&run->bt, &m->b) && pr_get_passive_ability(&m->b) == ABILITY_ID_COMATOSE);
}

/* onInit's heldItems: getHeldItems() without the form change items, in list order (olds[k] = the party member at position k) */
static int collect_held(gc_run *run, gc_mon *olds, int n, gc_item *held, int *held_start, int *held_n)
{
    int total = 0;
    for (int k = 0; k < n; k++) {
        olds[k] = run->party[run->party_order[k]];
        held_start[k] = total;
        for (int i = 0; i < run->n_items; i++) {
            const gc_item *it = &run->items[i];
            if (it->pokemon_id != olds[k].b.id || it->detached || gc_item_key_form_change(it->key)) continue;
            held[total++] = *it;
        }
        held_n[k] = total - held_start[k];
    }
    return total;
}

static void transform(gc_run *run)
{
    static gc_mon news[WD_PARTY], olds[WD_PARTY];
    static gc_item held[GC_MAX_ITEMS];
    int held_start[WD_PARTY], held_n[WD_PARTY], total = 0;
    const int n = run->n_party;
    if (!build_all(run, news)) {
        /* a species the data does not cover: the dream ends as if it were refused */
        gc_stub(run, GC_STUB_UNPORTED_ENEMY);
        gc_me_leave_with_heal_shop(run);
        return;
    }
    total = collect_held(run, olds, n, held, held_start, held_n);

    /* onPreOptionPhase: out goes the old, in comes the new, one at a time */
    for (int k = 0; k < n; k++) {
        const int idx = find_by_id(run, olds[k].b.id);
        olds[k] = run->party[idx];   /* the stats it has now: every removal before this one calculated the party's stats again, and upstream's item pruning can take one of its items with it */
        gc_party_remove(run, idx);
        gc_me_prune_items(run);
        gc_me_update_modifiers(run);
        run->party[run->n_party] = news[k];
        run->party_order[run->n_party] = (int8_t)run->n_party;
        run->n_party++;
        sync_party(run);
    }
    for (int k = 0; k < n; k++) {
        gc_mon *m = &run->party[find_by_id(run, news[k].b.id)];
        m->b.on_field = false;
        m->b.switch_out_status = true;
    }

    /* onOptionPhase: the cutscene's text, the transformations, then the dream's end */
    gc_ev_msg(run, "mysteryEncounters/weirdDream:option.1.cutscene", 0);
    gc_ev_msg(run, "mysteryEncounters/weirdDream:option.1.dreamComplete", 0);

    for (int k = 0; k < n; k++) {
        const gc_mon *prev = &olds[k];
        gc_mon *m = &run->party[find_by_id(run, news[k].b.id)];
        const double old_ratio = (double)prev->b.hp / (double)prev->b.stats[0];
        post_process(run, prev, m, false, false);
        for (int i = 0; i < held_n[k]; i++) add_held(run, m, held[held_start[k] + i]);
        if (gc_form(m->b.species, m->b.form)->base_total < NON_LEGENDARY_BST) {
            gc_item gateau = {.key = "MYSTERY_ENCOUNTER_OLD_GATEAU", .arg = 0, .stack = 1, .non_transferable = true};
            add_held(run, m, gateau);
        }
        gc_items_to_battle(run);
        pr_calculate_stats(&run->bt, &m->b);
        if (old_ratio > 0) {
            m->b.hp = (int32_t)ceil(old_ratio * m->b.stats[0]);
            if (!is_comatose(run, m)) {
                m->b.status = prev->b.status;
                m->b.status_toxic_turns = prev->b.status_toxic_turns;
                m->b.status_sleep_turns = prev->b.status_sleep_turns;
                m->b.status_freeze_turns = prev->b.status_freeze_turns;
            }
        } else {
            m->b.hp = 0;
            m->b.status = STATUS_EFFECT_FAINT;
        }
    }

    /* one Pokemon without a passive gets it */
    int without[WD_PARTY], n_without = 0;
    for (int k = 0; k < run->n_party; k++) if (!run->party[run->party_order[k]].b.passive_flag) without[n_without++] = run->party_order[k];
    if (n_without > 0) {
        gc_mon *m = &run->party[without[pr_rng_seed_int(&run->run.rng, n_without, 0)]];
        m->b.passive_flag = true;
        pr_update_passive(&run->bt, &m->b);
    }

    gc_rq_unshift(run, gc_rp(RP_PARTY_HEAL, 0, 0, 0));
    pr_reward_custom *c = &gc_int(run)->reward.me_custom;
    static const char *const funcs[] = {"MEMORY_MUSHROOM", "ROGUE_BALL", "MINT", "MINT", "MINT", "MINT"};
    memset(c, 0, sizeof *c);
    for (size_t i = 0; i < sizeof funcs / sizeof funcs[0]; i++) c->funcs[c->n_funcs++] = (uint16_t)gc_reward_item_index(funcs[i]);
    c->fill_remaining = false;
    c->reroll_multiplier = NAN;
    c->allow_luck_upgrades = true;
    gc_me_end_shop(run);
}

/* option 3: PERCENT_LEVEL_LOSS_ON_REFUSE */
static void lose_levels(gc_run *run)
{
    for (int k = 0; k < run->n_party; k++) {
        gc_mon *m = &run->party[run->party_order[k]];
        const double level = ceil(((100 - 10) / 100.0) * m->b.level);
        m->b.level = (int32_t)(level > 1 ? level : 1);
        const pr_species *sp = gc_species(m->b.species);
        m->exp = gc_level_total_exp(m->b.level, sp ? sp->growth_rate : 0);
        gc_mon_recalc(run, m);
    }
    gc_me_leave_with_heal_shop(run);
}

/* addEnemyModifier of a held item for `e` (not transferable): an equal item the holder already has stacks when the stack fits (incrementStack), else it is dropped */
static void add_given(gc_run *run, gc_item *given, int *n_given, const gc_mon *e, const gc_item *src)
{
    gc_item it = *src;
    it.pokemon_id = e->b.id;
    it.virtual_stack = 0;
    it.non_transferable = true;
    /* the updateModifiers that ends generateEnemyModifiers drops a held item that names a species (the Gimmighoul evolution tracker) its holder is not */
    if (!strcmp(it.key, "EVOLUTION_TRACKER_GIMMIGHOUL") && e->b.species != SPECIES_ID_GIMMIGHOUL && e->b.fusion_species != SPECIES_ID_GIMMIGHOUL) return;
    for (int i = 0; i < *n_given; i++) {
        gc_item *have = &given[i];
        if (!strcmp(have->key, it.key) && have->arg == it.arg && have->pokemon_id == it.pokemon_id) {
            if (have->stack + it.stack <= gc_item_max_held(run, it.key, it.arg, e)) have->stack += it.stack;
            return;
        }
    }
    if (*n_given < GC_MAX_ITEMS) given[(*n_given)++] = it;
}

/* the fight's first turn (the trainer's lines, the send-outs, the lead's abilities and the held items they use up): tests hold it back to compare the party and the enemy as the option left them */
static bool hold_start;
void gc_weird_hold_start(bool hold) { hold_start = hold; }
void gc_weird_begin_fight(gc_run *run) { gc_me_say_trainer_fight(run, gc_int(run)); }

/* option 2: the party this one would have become fights as the "alternate" player trainer */
static void fight(gc_run *run)
{
    gc_internal *in = gc_int(run);
    static gc_mon news[WD_PARTY], olds[WD_PARTY];
    static gc_item held[GC_MAX_ITEMS], given[GC_MAX_ITEMS];
    int held_start[WD_PARTY], held_n[WD_PARTY];
    const int n = run->n_party;
    if (!build_all(run, news)) {   /* a species the data does not cover: nobody fights and the dream ends as if it were refused */
        gc_stub(run, GC_STUB_UNPORTED_ENEMY);
        gc_me_leave_with_heal_shop(run);
        return;
    }
    collect_held(run, olds, n, held, held_start, held_n);

    /* onOptionPhase: for each transformation postProcessTransformedPokemon(.., forBattle) (the first one's start on the option's stream) */
    for (int k = 0; k < n; k++) post_process(run, &olds[k], &news[k], true, k == 0);

    /* setEncounterRewards: six tiers (no fill), the callback that gives one Pokemon its passive */
    pr_reward_custom *c = &in->reward.me_custom;
    memset(c, 0, sizeof *c);
    static const int8_t tiers[6] = {MODIFIER_TIER_ROGUE, MODIFIER_TIER_ROGUE, MODIFIER_TIER_ULTRA, MODIFIER_TIER_ULTRA, MODIFIER_TIER_GREAT, MODIFIER_TIER_GREAT};
    for (int k = 0; k < 6; k++) c->tiers[c->n_tiers++] = tiers[k];
    c->fill_remaining = false;
    c->reroll_multiplier = NAN;
    c->allow_luck_upgrades = true;
    gc_ev_msg(run, "mysteryEncounters/weirdDream:option.2.selected2", 0);

    /* initBattleWithEnemyConfig: the alternate trainer (the player's gender is not kept: always the male one), its Pokemon made from the new ones as data sources */
    run->trainer_type = TRAINER_TYPE_PLAYER_M_ALTERNATE;
    run->battle_type = BATTLE_TYPE_TRAINER;
    run->double_battle = false;
    run->trainer_variant = TRAINER_VARIANT_DEFAULT;
    run->trainer_name = run->trainer_partner_name = -1;
    in->custom_trainer_name[0] = 0;
    in->custom_team_on = false;
    int n_given = 0;
    for (int k = 0; k < n; k++) {
        gc_mon *e = &run->enemy[k];
        *e = news[k];
        pr_reset_summon_data(&e->b);
        e->b.is_player = false;
        e->b.on_field = false;
        e->b.has_trainer = true;
        e->b.trainer_slot = TRAINER_SLOT_TRAINER;
        e->trainer_slot = TRAINER_SLOT_TRAINER;
        e->b.status = STATUS_EFFECT_NONE;
        e->b.passive_flag = false;
        const bool boss = gc_form(e->b.species, e->b.form)->base_total > NON_LEGENDARY_BST;
        const int segments = boss ? pr_run_get_encounter_boss_segments(&run->run, run->wave, olds[k].b.level, e->b.species, true) : 0;
        e->b.is_boss = boss;
        e->b.boss_segments = (uint8_t)segments;
        e->b.boss_segment_index = (uint8_t)(segments > 0 ? segments - 1 : 0);
        e->b.hp = e->b.stats[STAT_HP];
        /* a data source: its id is rolled again */
        e->b.id = (uint32_t)floor(pr_rng_real_in_range(&run->run.rng, 0, 4294967296.0));
    }
    run->n_enemy = n;
    /* modifierConfigs: clones of the old Pokemon's items (not transferable, stacks as they are) and an Old Gateau for a Pokemon below 570 */
    memset(given, 0, sizeof given);
    for (int k = 0; k < n; k++) {
        const gc_mon *e = &run->enemy[k];
        for (int i = 0; i < held_n[k]; i++) add_given(run, given, &n_given, e, &held[held_start[k] + i]);
        if (gc_form(e->b.species, e->b.form)->base_total < NON_LEGENDARY_BST) {
            gc_item gateau = {.key = "MYSTERY_ENCOUNTER_OLD_GATEAU", .arg = 0, .stack = 1};
            add_given(run, given, &n_given, e, &gateau);
        }
    }
    run->me_battle = true;
    in->me_no_stats = false;
    gc_enemy_finish_party_holders(run, given, n_given);
    in->me_reward.active = false;
    run->me_catch_allowed = false;
    run->mystery_type = run->mystery_tier = -1;
    run->in_battle = true;
    in->weird.fight = true;
    if (!hold_start) gc_weird_begin_fight(run);
}

/* handleMysteryEncounterVictory (trainer battle): BattleEndPhase, TrainerVictoryPhase, then MysteryEncounterRewardsPhase: the callback (the passive) and the shop */
void gc_weird_victory(gc_run *run)
{
    gc_internal *in = gc_int(run);
    in->weird.fight = false;
    gc_rq_push(run, gc_rp(RP_BATTLE_END, 1, 0, 0));
    gc_rq_push(run, gc_rp(RP_TRAINER_VICTORY, 0, 0, 0));
    gc_rq_push(run, gc_rp(RP_ME_BEFORE_REWARDS, 0, 0, 0));
    gc_me_end_with_shop(run);
    in->weird.reward = true;
}

/* onBeforeRewards (the rewards phase runs it in executeWithSeedOffset(.., wave * 1000)): one random Pokemon without a passive gets it for the run */
void gc_weird_reward(gc_run *run)
{
    gc_internal *in = gc_int(run);
    if (!in->weird.reward) return;
    in->weird.reward = false;
    int without[WD_PARTY], n_without = 0;
    for (int k = 0; k < run->n_party; k++) if (!run->party[run->party_order[k]].b.passive_flag) without[n_without++] = run->party_order[k];
    if (n_without == 0) return;
    pr_rng r = pr_run_seed_offset_rng(&run->run, run->wave * 1000, NULL);
    gc_mon *m = &run->party[without[pr_rng_seed_int(&r, n_without, 0)]];
    m->b.passive_flag = true;
    pr_update_passive(&run->bt, &m->b);
}

void gc_weird_execute(gc_run *run, int option)
{
    if (option == 0) transform(run);
    else if (option == 1) fight(run);
    else lose_levels(run);
}
