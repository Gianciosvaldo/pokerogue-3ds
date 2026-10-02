/* Challenge Mode validation: Single Generation, Single Type and Fresh Start.
 * Mirrors the validation hooks in upstream src/data/challenge.ts. */
#include "pr/run.h"

static const pr_species *challenge_species(int id)
{
    int lo = 0, hi = PR_SPECIES_COUNT - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (pr_species_table[mid].id == id) return &pr_species_table[mid];
        if (pr_species_table[mid].id < id) lo = mid + 1; else hi = mid - 1;
    }
    return NULL;
}

static const pr_form *challenge_form(int species, int form)
{
    const pr_species *sp = challenge_species(species);
    if (!sp || form < 0 || form >= sp->nforms) return NULL;
    return &pr_forms[sp->form0 + form];
}

static bool active(const pr_run *run, unsigned bit)
{
    return run && run->mode == GAME_MODES_CHALLENGE && (run->challenge_flags & bit);
}

bool pr_run_has_unported_challenges(const pr_run *run)
{
    if (!run || run->mode != GAME_MODES_CHALLENGE) return false;
    if (!run->has_challenges && !run->challenge_flags) return false;
    if (run->has_challenges && !run->challenge_flags) return true; /* legacy caller: active set is unknown */
    return (run->challenge_flags & ~PR_CHALLENGE_RULES_PORTED) != 0;
}

static bool generation_allowed(const pr_run *run, const pr_challenge_mon *mon)
{
    if (!active(run, PR_CHALLENGE_SINGLE_GENERATION)) return true;
    if (run->challenge_generation < 1 || run->challenge_generation > 9) return false;

    const pr_species *sp = challenge_species(mon->species);
    if (!sp || sp->generation != run->challenge_generation) return false;
    if (mon->fusion_species) {
        const pr_species *fsp = challenge_species(mon->fusion_species);
        if (!fsp || fsp->generation != run->challenge_generation) return false;
    }
    return true;
}

static bool type_allowed(const pr_run *run, const pr_challenge_mon *mon)
{
    if (!active(run, PR_CHALLENGE_SINGLE_TYPE)) return true;
    if (run->challenge_type < 1 || run->challenge_type > 18) return false;
    const int wanted = run->challenge_type - 1;

    /* SingleTypeChallenge.TYPE_OVERRIDES: Castform counts as Normal in battle.
     * The override is on the base species even when Castform is fused. */
    if (wanted == POKEMON_TYPE_NORMAL && mon->species == SPECIES_ID_CASTFORM) return true;

    pr_enemy_gen g = {0};
    g.form = mon->form;
    g.fusion_species = mon->fusion_species ? mon->fusion_species : -1;
    g.fusion_form = mon->fusion_form;
    int8_t types[2];
    int n = pr_run_enemy_types(&g, mon->species, types);
    if (n < 0) return false;
    for (int i = 0; i < n; i++) if (types[i] == wanted) return true;
    return false;
}

bool pr_run_challenge_mon_allowed(const pr_run *run, const pr_challenge_mon *mon)
{
    if (!run || !mon) return false;
    return generation_allowed(run, mon) && type_allowed(run, mon);
}

bool pr_run_challenge_party_allowed(const pr_run *run, const pr_challenge_mon *party, int n_party)
{
    if (!run || n_party < 0 || (n_party && !party)) return false;
    for (int i = 0; i < n_party; i++)
        if (!pr_run_challenge_mon_allowed(run, &party[i])) return false;
    return true;
}

static bool raw_form_has_type(int species, int form, int wanted)
{
    const pr_form *f = challenge_form(species, form);
    return f && (f->type1 == wanted || f->type2 == wanted);
}

static int root_species(int species)
{
    const pr_species *sp = challenge_species(species);
    if (!sp) return -1;
    while (sp->prevolution >= 0) {
        sp = challenge_species(sp->prevolution);
        if (!sp) return -1;
    }
    return sp->id;
}

static bool fresh_start_default_line(int species)
{
    static const uint16_t roots[] = {
        SPECIES_ID_BULBASAUR, SPECIES_ID_CHARMANDER, SPECIES_ID_SQUIRTLE,
        SPECIES_ID_CHIKORITA, SPECIES_ID_CYNDAQUIL, SPECIES_ID_TOTODILE,
        SPECIES_ID_TREECKO, SPECIES_ID_TORCHIC, SPECIES_ID_MUDKIP,
        SPECIES_ID_TURTWIG, SPECIES_ID_CHIMCHAR, SPECIES_ID_PIPLUP,
        SPECIES_ID_SNIVY, SPECIES_ID_TEPIG, SPECIES_ID_OSHAWOTT,
        SPECIES_ID_CHESPIN, SPECIES_ID_FENNEKIN, SPECIES_ID_FROAKIE,
        SPECIES_ID_ROWLET, SPECIES_ID_LITTEN, SPECIES_ID_POPPLIO,
        SPECIES_ID_GROOKEY, SPECIES_ID_SCORBUNNY, SPECIES_ID_SOBBLE,
        SPECIES_ID_SPRIGATITO, SPECIES_ID_FUECOCO, SPECIES_ID_QUAXLY,
    };
    int root = root_species(species);
    if (root < 0) return false;
    for (unsigned i = 0; i < sizeof roots / sizeof roots[0]; i++)
        if (root == roots[i]) return true;
    return false;
}

bool pr_run_challenge_starter_allowed(const pr_run *run, int species, int form)
{
    if (!run) return false;
    const pr_species *sp = challenge_species(species);
    if (!sp || !challenge_form(species, form)) return false;

    if (active(run, PR_CHALLENGE_SINGLE_GENERATION)) {
        if (run->challenge_generation < 1 || run->challenge_generation > 9
            || sp->generation != run->challenge_generation)
            return false;
    }
    if (active(run, PR_CHALLENGE_SINGLE_TYPE)) {
        if (run->challenge_type < 1 || run->challenge_type > 18
            || !raw_form_has_type(species, form, run->challenge_type - 1))
            return false;
    }
    if (active(run, PR_CHALLENGE_FRESH_START)) {
        if (run->challenge_fresh_start < 1 || run->challenge_fresh_start > 2) return false;
        if (run->challenge_fresh_start == 1 && !fresh_start_default_line(species)) return false;
    }
    return true;
}

bool pr_run_challenge_capture_allowed(const pr_run *run, const pr_challenge_mon *mon)
{
    /* Fresh Start only constrains the initial starter state upstream. For the offline run layer,
     * reject captures that could not legally join the active Mono-gen/Mono-type battle party. */
    return pr_run_challenge_mon_allowed(run, mon);
}
