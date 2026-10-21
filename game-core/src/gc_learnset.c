/* The level moves a Pokemon can learn: Pokemon.getLevelMoves (src/field/learnsets.ts and
 * src/data/species-data-registry.ts at 8555c08) and PlayerPokemon.getUnlockedEggMoves, as the reward screen
 * reads them (reward-core takes the rows from its caller). Function names follow upstream's. A species'
 * own level moves are battle-core's pr_species.level_moves; the form-specific ones are gen/out/gc_learnsets.c.
 * Not modelled: the LEVEL_UP_MOVESET challenge hook (Species.getLevelMoves applies it). */
#include "internal.h"

/* Rows before the filter: a three-stage line with form moves, twice over for a fusion. A list that doesn't
 * fit is reported (-1), never cut. */
#define ROWS_MAX 768
typedef gc_learn_row row;

static int base_source(int source) { return source - source % 2; }

/* speciesDataRegistry.getFormKey(species, form): "" for a species without forms, else the form's key
 * (the base form's for an index no form has) */
static const char *form_key(const pr_species *sp, int form)
{
    return pr_forms[sp->form0 + (form >= 0 && form < sp->nforms ? form : 0)].form_key;
}

/* PokemonSpecies.getLevelMoves(form): levelMoves then formLevelMoves[formKey] (no duplicate removal: the
 * Set holds distinct arrays), applyChallenges aside, sorted by level (a stable sort) */
static int species_level_moves(int species, int form, row *out, int cap)
{
    const pr_species *sp = gc_species(species);
    if (!sp) return 0;
    int n = 0;
    if (sp->level_moves.n > cap) return -1;
    for (int i = 0; i < sp->level_moves.n; i++)
        out[n++] = (row){sp->level_moves.v[i].level, sp->level_moves.v[i].move, 0};
    const char *key = form_key(sp, form);
    for (int i = 0; i < gc_form_level_moves_count; i++) {
        const gc_form_level_moves *f = &gc_form_level_moves_data[i];
        if (f->species != species || strcmp(f->form_key, key)) continue;
        if (n + f->n > cap) return -1;
        for (int k = 0; k < f->n; k++) out[n++] = (row){f->v[k].level, f->v[k].move, 0};
    }
    for (int i = 1; i < n; i++) {
        const row v = out[i];
        int j = i;
        while (j > 0 && out[j - 1].level > v.level) { out[j] = out[j - 1]; j--; }
        out[j] = v;
    }
    return n;
}

/* getRegularLevelMoves(context, includeEvolutionMoves, includeRelearnerMoves, fromFusion) */
static int regular_level_moves(const gc_mon *m, bool evo, bool relearn, bool from_fusion, row *out, int cap)
{
    static row moves[ROWS_MAX];
    const pr_pokemon *p = &m->b;
    const int n = from_fusion ? species_level_moves(p->fusion_species, p->fusion_form, moves, ROWS_MAX)
                              : species_level_moves(p->species, p->form, moves, ROWS_MAX);
    if (n < 0) return -1;
    int w = 0;
    for (int i = 0; i < n; i++) {
        const int level = moves[i].level;
        if (!((evo && level == 0) || (relearn && level == -1) || level > 0)) continue;
        if (w >= cap) return -1;
        const int source = level == -1 ? PR_LEARN_SOURCE_RELEARN : level == 0 ? PR_LEARN_SOURCE_EVOLUTION
                                                                              : PR_LEARN_SOURCE_LEVEL;
        out[w++] = (row){(int16_t)level, moves[i].move, (int8_t)(source + from_fusion)};
    }
    return w;
}

/* getPrevolutionMoves(context, includeEvolutionMoves, includeRelearnerMoves, fromFusion). Upstream reads
 * every stage's moves for the primary's form index, the fusion's chain included. */
static int prevolution_moves(const gc_mon *m, bool evo, bool relearn, bool from_fusion, row *out, int cap)
{
    static row moves[ROWS_MAX];
    const pr_pokemon *p = &m->b;
    const int base = from_fusion ? p->fusion_species : p->species;
    if (!base) return 0;
    /* evolutionLine = [...getPrevolutionChain(base), base]: the nearest prevolution first, the root next to
     * last, the species itself last. Only the first entry counts as `index 0` below. */
    int line[16], n_line = 0;
    {
        const pr_species *sp = gc_species(base);
        for (int id = sp ? sp->prevolution : -1, guard = 0; id > 0 && guard < 14; guard++) {
            line[n_line++] = id;
            sp = gc_species(id);
            id = sp ? sp->prevolution : -1;
        }
        line[n_line++] = base;
    }
    int w = 0;
    for (int k = 0; k < n_line; k++) {
        const int index = k;
        const bool is_prevo = k < n_line - 1;
        const int n = species_level_moves(line[k], p->form, moves, ROWS_MAX);
        if (n < 0) return -1;
        for (int i = 0; i < n; i++) {
            const int level = moves[i].level;
            const bool include_level_one = !index || level > 1 || relearn;
            int source;
            if (relearn && level == -1) source = is_prevo ? PR_LEARN_SOURCE_PREVO : PR_LEARN_SOURCE_RELEARN;
            else if (evo && level == 0) source = is_prevo ? PR_LEARN_SOURCE_PREVO : PR_LEARN_SOURCE_EVOLUTION;
            else if (include_level_one && (!is_prevo || level <= p->level))
                source = is_prevo ? PR_LEARN_SOURCE_PREVO : PR_LEARN_SOURCE_LEVEL;
            else continue;
            if (w >= cap) return -1;
            out[w++] = (row){(int16_t)level, moves[i].move, (int8_t)(source + from_fusion)};
        }
    }
    return w;
}

/* filterAndSortLevelMoves(context, levelMoves, includeEvolutionMoves, includeRelearnerMoves): sorted by level,
 * then by base source (a stable sort); in place, returns the new length */
static int filter_and_sort(const gc_mon *m, int starting_level, row *rows, int n, bool evo, bool relearn)
{
    const int level_cap = m->b.level;
    for (int i = 1; i < n; i++) {
        const row v = rows[i];
        int j = i;
        while (j > 0 && (rows[j - 1].level > v.level
                         || (rows[j - 1].level == v.level && base_source(rows[j - 1].source) > base_source(v.source)))) {
            rows[j] = rows[j - 1];
            j--;
        }
        rows[j] = v;
    }
    /* moves the species gets by level above its level, and the ones it already learns by level */
    static uint8_t above[PR_MOVE_COUNT], own[PR_MOVE_COUNT], seen[PR_MOVE_COUNT];
    memset(above, 0, sizeof above);
    memset(own, 0, sizeof own);
    memset(seen, 0, sizeof seen);
    for (int i = 0; i < n; i++) {
        if (base_source(rows[i].source) != PR_LEARN_SOURCE_LEVEL) continue;
        if (rows[i].level > level_cap) above[rows[i].move] = 1; else own[rows[i].move] = 1;
    }
    int w = 0;
    for (int i = 0; i < n; i++) {
        const row r = rows[i];
        const bool is_relearner = r.level < starting_level;
        const bool allowed_evolution_move = r.level == 0 && evo;
        const bool is_level_source = base_source(r.source) == PR_LEARN_SOURCE_LEVEL;
        const bool own_from_non_level = own[r.move] && !is_level_source;
        const bool locked_prevo = above[r.move] && (r.source == PR_LEARN_SOURCE_PREVO || r.source == PR_LEARN_SOURCE_FUSION_PREVO);
        if (r.level > level_cap || own_from_non_level || locked_prevo
            || !(relearn || !is_relearner || allowed_evolution_move))
            continue;
        if (seen[r.move]) continue;   /* getUniqueMoves, after the filter */
        seen[r.move] = 1;
        rows[w++] = r;
    }
    return w;
}

int gc_level_moves(const gc_mon *m, int starting_level, bool evo, bool prevo, bool relearn, gc_learn_row *out, int cap)
{
    static row levels[ROWS_MAX];
    const pr_pokemon *p = &m->b;
    const bool is_fusion = p->fusion_species != 0;
    if (relearn) evo = true;   /* relearner moves inherently allow evolution moves */
    int n = prevo ? prevolution_moves(m, evo, relearn, false, levels, ROWS_MAX)
                  : regular_level_moves(m, evo, relearn, false, levels, ROWS_MAX);
    if (n < 0) return -1;
    if (is_fusion) {
        const int k = prevo ? prevolution_moves(m, evo, relearn, true, levels + n, ROWS_MAX - n)
                            : regular_level_moves(m, evo, relearn, true, levels + n, ROWS_MAX - n);
        if (k < 0) return -1;
        n += k;
    }
    n = filter_and_sort(m, starting_level, levels, n, evo, relearn);
    if (n > cap) return -1;
    memcpy(out, levels, (size_t)n * sizeof *out);
    return n;
}

/* postEvolve: getLevelMoves({startingLevel: lastLevel + 1, includeEvolutionMoves}) filtered to the evolution moves (level 0)
 * of the evolved form. The starting level does not matter to level-0 rows (the evolution moves are always allowed). */
int gc_evolution_moves(const gc_mon *m, uint16_t *out, int max)
{
    gc_learn_row rows[64];
    const int n = gc_level_moves(m, m->b.level + 1, true, false, false, rows, 64);
    int k = 0;
    for (int i = 0; i < n && k < max; i++) if (rows[i].level == 0) out[k++] = rows[i].move;
    return k;
}

/* PokemonSpeciesForm.getRootSpeciesId(true): back along the prevolutions until a starter */
static int root_species_for_starter(int species)
{
    for (int guard = 0; guard < 15; guard++) {
        const pr_species *sp = gc_species(species);
        if (!sp || sp->prevolution <= 0 || gc_starter_cost(species) >= 0) break;
        species = sp->prevolution;
    }
    return species;
}

/* PlayerPokemon.getUnlockedEggMoves(): speciesEggMoves[metSpecies], or of the root starter when the Pokemon's
 * metSpecies has none; the ones the player unlocked (starterData.eggMoves bits). game-core keeps no
 * metSpecies: a starter's is its root species, which is what the current species resolves to. */
int gc_unlocked_egg_moves(const gc_run *run, const gc_mon *m, uint16_t out[4])
{
    int n = 0;
    const int species = root_species_for_starter(m->b.species);
    for (int i = 0; i < gc_starter_egg_moves_count; i++) {
        const gc_starter_egg_moves *e = &gc_starter_egg_moves_data[i];
        if (e->species != species) continue;
        const unsigned unlocked = run->sys && species <= GC_MAX_SPECIES_ID ? run->sys->starters[species].egg_moves : 0;
        for (int k = 0; k < 4; k++)
            if ((unlocked & (1u << k)) && e->egg_moves[k]) out[n++] = e->egg_moves[k];
        break;
    }
    return n;
}

/* PokemonSpeciesForm.getRootSpeciesId(false): back along the prevolutions to the first stage */
int gc_root_species(int species)
{
    for (int guard = 0; guard < 15; guard++) {
        const pr_species *sp = gc_species(species);
        if (!sp || sp->prevolution <= 0) break;
        species = sp->prevolution;
    }
    return species;
}

/* Pokemon.getEggMoves(): speciesEggMoves[getSpeciesForm().getRootSpeciesId()], all four, unlocked or not */
int gc_egg_moves(const gc_mon *m, uint16_t out[4])
{
    const int species = gc_root_species(m->b.species);
    for (int i = 0; i < gc_starter_egg_moves_count; i++) {
        const gc_starter_egg_moves *e = &gc_starter_egg_moves_data[i];
        if (e->species != species) continue;
        int n = 0;
        for (int k = 0; k < 4; k++) if (e->egg_moves[k]) out[n++] = e->egg_moves[k];
        return n;
    }
    return 0;
}

/* species.getTms(getFormKey()) of one species form (speciesDataRegistry.getTms: its own list plus the prevolutions'
 * are all in the generated pr_tm_lists row). A form index past the species' forms (no Pokemon has one) is read as the base form */
static bool form_tms_have(int species, int form, int move)
{
    int lo = 0, hi = pr_reward_species_count - 1;
    const pr_reward_species *s = NULL;
    while (lo <= hi && !s) {
        const int mid = (lo + hi) / 2;
        const int id = pr_reward_species_data[mid].id;
        if (id == species) s = &pr_reward_species_data[mid];
        else if (id < species) lo = mid + 1;
        else hi = mid - 1;
    }
    if (!s) return false;
    const int f = s->n_forms && form >= 0 && form < s->n_forms ? form : 0;
    const pr_tm_list *l = &pr_tm_lists[pr_reward_forms[s->form_start + f].tms];
    for (unsigned i = 0; i < l->n; i++) if (pr_tm_pool[l->start + i] == move) return true;
    return false;
}

/* getCompatibleTms(false, false, false): raw species/form plus fusion TMs,
 * even while transformed. PLAYER_TM_COMPATIBILITY's randomizer remains unported. */
static bool tm_compatible(const gc_mon *m, int move)
{
    return m && move > 0 && move < PR_MOVE_COUNT
        && (form_tms_have(m->b.species, m->b.form, move)
            || (m->b.fusion_species && form_tms_have(m->b.fusion_species, m->b.fusion_form, move)));
}

/* TmModifierType.selectFilter calls isTmCompatible(move, true), which excludes
 * Pokemon.moveset (permanent moves), not getMoveset()'s temporary override. */
bool gc_mon_can_learn_tm(const gc_mon *m, int move)
{
    if (!tm_compatible(m, move)) return false;
    for (int i = 0; i < m->b.n_moves; i++) if (m->b.moves[i] == move) return false;
    return true;
}

/* PartySlot displays getMoveset()'s effective moves first, then compatibility.
 * A displayed Learned/Able label is therefore not itself the teaching filter. */
int gc_mon_tm_state(const gc_mon *m, int move)
{
    if (!m || move <= 0 || move >= PR_MOVE_COUNT) return 0;
    for (int i = 0; i < PR_N_MOVES(&m->b); i++) if (PR_MOVES(&m->b)[i] == move) return 2;
    return tm_compatible(m, move);
}

/* CanLearnMoveRequirement.getAllPokemonMoves(pkm) includes the move: getLevelMoves() with its defaults (startingLevel =
 * the level, nothing extra: the move learned at exactly this level), getCompatibleTms() (the species' TMs for its form
 * and the fusion's) and getEggMoves() */
bool gc_can_learn_move(const gc_mon *m, int move)
{
    gc_learn_row rows[64];
    const int n = gc_level_moves(m, m->b.level, false, false, false, rows, 64);
    for (int i = 0; i < n; i++) if (rows[i].move == move) return true;
    if (form_tms_have(m->b.species, m->b.form, move)) return true;
    if (m->b.fusion_species && form_tms_have(m->b.fusion_species, m->b.fusion_form, move)) return true;
    uint16_t egg[4];
    const int ne = gc_egg_moves(m, egg);
    for (int i = 0; i < ne; i++) if (egg[i] == move) return true;
    return false;
}
