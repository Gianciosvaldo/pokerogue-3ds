/* Challenge Mode (GAME_MODES_CHALLENGE): the twelve challenges' values, and the rules of the ones game-core plays.
 *
 * Upstream's Challenge (src/data/challenge.ts) is a value 0..maxValue and a set of hooks that game code reads through applyChallenges
 * (src/utils/challenge-utils.ts). Challenge Mode is Classic (isClassic) with every challenge listed, so the run, its fixed battles, its
 * Mystery Encounter waves and its win are Classic's; what the challenges change is here and in the places that call this file.
 *
 * Upstream lists ten challenges in Challenge Mode (initChallenges: allChallenges). LOWER_MAX_STARTER_COST and LOWER_STARTER_POINTS have a class
 * and an id, but are not in that list: a game mode never has them, setChallengeValue on them does nothing, and the title screen cannot offer
 * them. Here their values must be 0 (gc_challenge_max) so no run ever carries one.
 *
 * Ported (gc_challenge_played_mask): FRESH_START, with these hooks:
 *   STARTER_COST           the base cost, the profile's value reduction ignored                         gc_challenge_starter_cost
 *   STARTER_CHOICE         value 1: only the default starters and their evolutions                      gc_challenge_starter_valid
 *   STARTER_SELECT_MODIFY  the profile as a new save has it (getStarterData)                            gc_challenge_starter_view
 *   STARTER_MODIFY         the starter Pokemon as a new save has it                                     gc_challenge_modify_starter
 * and, for every challenge, the win's ribbons (ribbonAwarded, GameOverPhase.awardRibbons)               gc_challenge_ribbon_flags
 * Not ported yet (a run that sets one is refused by gc_run_set_challenges): the other eight.
 *
 * Checked against the pinned upstream by tests/challenge-starters.producer.test.ts (tools/challengestartersdiff.sh). */
#include "internal.h"

static const uint8_t challenge_max[GC_CHALLENGE_COUNT] = {
    9,  /* SINGLE_GENERATION */
    18, /* SINGLE_TYPE */
    0,  /* LOWER_MAX_STARTER_COST: not in allChallenges (see above) */
    0,  /* LOWER_STARTER_POINTS: likewise */
    2,  /* FRESH_START */
    1,  /* INVERSE_BATTLE */
    1,  /* FLIP_STAT */
    1,  /* LIMITED_CATCH */
    3,  /* LIMITED_SUPPORT */
    1,  /* HARDCORE */
    2,  /* PASSIVES */
    2,  /* MOVESET_RANDOMIZER */
};

int gc_challenge_max(int challenge)
{
    return challenge >= 0 && challenge < GC_CHALLENGE_COUNT ? challenge_max[challenge] : -1;
}

uint32_t gc_challenge_played_mask(void)
{
    return 1u << CHALLENGES_FRESH_START;
}

bool gc_challenge_set_valid(const gc_challenge_set *set, const char **why)
{
    if (!set) { if (why) *why = "no challenge values"; return false; }
    for (int i = 0; i < GC_CHALLENGE_COUNT; i++)
        if (set->value[i] > challenge_max[i]) { if (why) *why = "a challenge value above its maximum"; return false; }
    return true;
}

static uint32_t active_flags(const gc_challenge_set *set)
{
    uint32_t f = 0;
    if (set) for (int i = 0; i < GC_CHALLENGE_COUNT; i++) if (set->value[i]) f |= 1u << i;
    return f;
}

/* run.challenge_flags and the fields run-core and battle-core read, from the values */
void gc_challenge_sync(gc_run *run)
{
    pr_run *r = &run->run;
    const gc_challenge_set *set = &run->challenges;
    r->challenge_flags = active_flags(set);
    r->has_challenges = r->challenge_flags != 0;
    r->challenge_generation = set->value[CHALLENGES_SINGLE_GENERATION];
    r->challenge_type = set->value[CHALLENGES_SINGLE_TYPE];
    r->challenge_fresh_start = set->value[CHALLENGES_FRESH_START];
}

/* Whether the run has an active challenge whose rules game-core does not play yet. An active set nobody told game-core about (run.has_challenges
 * with no flags: a caller that predates the values) counts as unplayed. */
bool gc_challenges_unplayed(const gc_run *run)
{
    if (!run->run.has_challenges) return false;
    if (!run->run.challenge_flags) return true;
    return (run->run.challenge_flags & ~gc_challenge_played_mask()) != 0;
}

bool gc_run_set_challenges(gc_run *run, const gc_challenge_set *set, const char **why)
{
    const char *dummy;
    if (!why) why = &dummy;
    if (run->run.mode != GAME_MODES_CHALLENGE) { *why = "not a Challenge Mode run"; return false; }
    if (run->screen.kind != GC_SCREEN_STARTERS) { *why = "not on the starter screen"; return false; }
    if (!gc_challenge_set_valid(set, why)) return false;
    if (active_flags(set) & ~gc_challenge_played_mask()) { *why = "a challenge that is not ported yet"; return false; }
    run->challenges = *set;
    gc_challenge_sync(run);
    return true;
}

/* ---------------------------------------------------------------- the starter select's rules */
static const gc_challenge_set none;

/* GameData.getSpeciesStarterValue: the value reduction, then the challenges' STARTER_COST (FreshStartChallenge: the base cost) */
double gc_challenge_starter_cost(const gc_system *sys, const gc_challenge_set *set, int species)
{
    if (set && set->value[CHALLENGES_FRESH_START]) return gc_starter_cost(species);
    return gc_starter_value(sys, species);
}

/* the species an evolution of `species` leads to, as indexes into gc_evo_data */
static const gc_species_evo *evolutions_of(int species)
{
    int lo = 0, hi = gc_species_evo_count - 1;
    while (lo <= hi) {
        const int mid = (lo + hi) / 2;
        if (gc_species_evos[mid].id == species) return &gc_species_evos[mid];
        if (gc_species_evos[mid].id < species) lo = mid + 1; else hi = mid - 1;
    }
    return NULL;
}

/* checkSpeciesValidForChallenge without the form-change step (soft false): every STARTER_CHOICE hook on the species and the form's dex props.
 * An out-of-range form index reads the species itself, as speciesDataRegistry.getPokemonSpeciesForm does */
static bool choice_valid(const gc_challenge_set *set, int species, int form)
{
    const pr_species *sp = gc_species(species);
    if (!sp) return false;
    if (form < 0 || form >= sp->nforms) form = 0;
    /* Single Generation, Single Type and Fresh Start: run-core's rules (challenges.c) */
    pr_run tmp;
    memset(&tmp, 0, sizeof tmp);
    tmp.mode = GAME_MODES_CHALLENGE;
    tmp.has_challenges = true;
    tmp.challenge_flags = active_flags(set);
    tmp.challenge_generation = set->value[CHALLENGES_SINGLE_GENERATION];
    tmp.challenge_type = set->value[CHALLENGES_SINGLE_TYPE];
    tmp.challenge_fresh_start = set->value[CHALLENGES_FRESH_START];
    return pr_run_challenge_starter_allowed(&tmp, species, form);
}

/* checkStarterValidForChallenge(starterId, props, soft). With `soft` the starter also passes when one of its evolutions does (the party select
 * allows such a starter next to a valid one); an evolution whose preFormKey is not the starter's form is not followed.
 * Not done: soft's other step, a species' item-triggered form change to a valid form (speciesDataRegistry.getFormChanges: no trigger data here).
 * None of the challenges played so far looks at the form, so it changes no answer. */
bool gc_challenge_starter_valid(const gc_challenge_set *set, int species, int form, bool soft)
{
    if (!set) set = &none;
    if (!soft) return choice_valid(set, species, form);
    const pr_form *own = gc_form(species, form);
    const char *own_key = own ? own->form_key : NULL;
    uint16_t stack[64];
    int n = 0;
    stack[n++] = (uint16_t)species;
    while (n > 0) {
        const int checking = stack[--n];
        if (choice_valid(set, checking, form)) return true;
        const gc_species_evo *ev = evolutions_of(checking);
        if (!ev) continue;
        for (int k = 0; k < ev->n; k++) {
            const gc_evo *e = &gc_evo_data[ev->start + k];
            if (e->pre_form != -1) {
                /* `e.preFormKey === species.forms[props.formIndex].formKey`, the starter's own form against the evolving species' key */
                const pr_form *pf = e->pre_form >= 0 ? gc_form(checking, e->pre_form) : NULL;
                if (!pf || !own_key || strcmp(pf->form_key, own_key)) continue;
            }
            if (n < (int)(sizeof stack / sizeof stack[0])) stack[n++] = e->species;
        }
    }
    return false;
}

/* The party select's challenge rules: every member is valid, or (next to a valid one) one an evolution of which is (checkStarterValidForChallenge
 * with soft = isPartyValid), and the party starts only with at least one valid member (isPartyValid). `why` is the locale key / reason on a refusal. */
bool gc_challenge_party_ok(const gc_challenge_set *set, const gc_starter *team, int n, const char **why)
{
    const char *dummy;
    if (!why) why = &dummy;
    bool any_valid = false;
    for (int i = 0; i < n; i++) {
        const bool valid = gc_challenge_starter_valid(set, team[i].species, team[i].form, false);
        if (!valid && !gc_challenge_starter_valid(set, team[i].species, team[i].form, true)) {
            *why = "a starter this challenge does not allow";
            return false;
        }
        any_valid |= valid;
    }
    if (!any_valid) { *why = "starterSelectUiHandler:invalidParty"; return false; }
    return true;
}

/* The species whose forms Fresh Start takes back to the default one (FreshStartChallenge: Pikachu, Eevee, Pichu, Rotom, Meloetta) */
static bool fresh_start_form_reset(int species)
{
    return species == SPECIES_ID_PIKACHU || species == SPECIES_ID_EEVEE || species == SPECIES_ID_PICHU || species == SPECIES_ID_ROTOM
           || species == SPECIES_ID_MELOETTA;
}

/* getStarterData(starterId): a copy of the profile's dex and starter entries with every STARTER_SELECT_MODIFY applied */
void gc_challenge_starter_view(const gc_system *sys, const gc_challenge_set *set, int species, gc_starter_view *out)
{
    memset(out, 0, sizeof *out);
    if (!sys || species <= 0 || species > GC_MAX_SPECIES_ID) return;
    const gc_dex_entry *d = &sys->dex[species];
    const gc_starter_entry *s = &sys->starters[species];
    out->caught_attr = d->caught_attr;
    out->nature_attr = d->nature_attr;
    out->ability_attr = s->ability_attr;
    out->passive_attr = s->passive_attr;
    out->value_reduction = s->value_reduction;
    out->egg_moves = s->egg_moves;
    memcpy(out->ivs, d->ivs, 6);
    if (set && set->value[CHALLENGES_FRESH_START]) {
        out->egg_moves = 0;                                   /* all egg moves */
        out->ability_attr &= 1u | 2u;                         /* hidden ability */
        out->passive_attr = 0;
        out->value_reduction = 0;
        /* natures except the neutral ones: Hardy, Docile, Serious, Bashful, Quirky (bit nature + 1) */
        out->nature_attr &= (1u << 1) | (1u << 7) | (1u << 13) | (1u << 19) | (1u << 25);
        for (int i = 0; i < 6; i++) if (out->ivs[i] > 15) out->ivs[i] = 15;
        if (fresh_start_form_reset(species)) out->caught_attr &= 0xFF;   /* (DexAttr.DEFAULT_FORM << 1) - 1: no forms but the default */
    }
}

/* ChallengeType.STARTER_MODIFY on a starter Pokemon (SelectStarterPhase.initBattle, after its nickname and fusion): Fresh Start takes it back
 * to what a new save's starter is. The stats are recalculated here: upstream does at the first battle's updateModifiers, nothing in between
 * reads them. */
void gc_challenge_modify_starter(gc_run *run, gc_mon *m)
{
    if (run->run.mode != GAME_MODES_CHALLENGE || !run->challenges.value[CHALLENGES_FRESH_START]) return;
    pr_pokemon *p = &m->b;
    const pr_species *sp = gc_species(p->species);
    if (!sp) return;

    m->ability_index %= 2;   /* always the base ability: a hidden one wraps to the first */
    p->ability_index = (int8_t)m->ability_index;
    p->passive_flag = false;

    /* moveset: keep what is a level 1-5 move of the species, then fill the empty slots with the level 1-5 moves not known, in learnset order */
    uint16_t valid[64], list[4];
    uint8_t vlev[64];
    int nv = 0, nl = 0;
    /* species.getLevelMoves() = levelMoves + formLevelMoves[forms[0].formKey] (no duplicate removal), stable-sorted by level */
    const int cap = (int)(sizeof valid / sizeof valid[0]);
    for (int i = 0; i < sp->level_moves.n && nv < cap; i++) {
        const pr_level_move *lm = &sp->level_moves.v[i];
        if (lm->level >= 1 && lm->level <= 5) { vlev[nv] = (uint8_t)lm->level; valid[nv++] = lm->move; }
    }
    {
        const char *key = gc_form(p->species, 0)->form_key;
        for (int i = 0; i < gc_form_level_moves_count; i++) {
            const gc_form_level_moves *f = &gc_form_level_moves_data[i];
            if (f->species != p->species || strcmp(f->form_key, key)) continue;
            for (int k = 0; k < f->n && nv < cap; k++)
                if (f->v[k].level >= 1 && f->v[k].level <= 5) { vlev[nv] = (uint8_t)f->v[k].level; valid[nv++] = f->v[k].move; }
        }
    }
    for (int i = 1; i < nv; i++) {
        const uint16_t mv = valid[i]; const uint8_t lv = vlev[i];
        int j = i;
        while (j > 0 && vlev[j - 1] > lv) { valid[j] = valid[j - 1]; vlev[j] = vlev[j - 1]; j--; }
        valid[j] = mv; vlev[j] = lv;
    }
    for (int i = 0; i < p->n_moves && nl < 4; i++)
        for (int k = 0; k < nv; k++) if (valid[k] == p->moves[i]) { list[nl++] = p->moves[i]; break; }
    if (nl < 4) {
        const int known = nl;
        for (int k = 0; k < nv && nl < 4; k++) {
            bool have = false;
            for (int j = 0; j < known; j++) have |= list[j] == valid[k];
            if (!have) list[nl++] = valid[k];
        }
    }
    p->n_moves = 0;
    for (int i = 0; i < 4; i++) { p->moves[i] = 0; p->pp_used[i] = 0; p->pp_up[i] = 0; p->max_pp_override[i] = -1; }
    for (int i = 0; i < nl; i++) pr_set_original_move(p, i, list[i]);

    m->luck = 0;   /* no luck, even if shiny */
    if (p->form > 0 && fresh_start_form_reset(p->species)) p->form = 0;
    for (int i = 0; i < 6; i++) if (p->ivs[i] > 15) p->ivs[i] = 15;
    {
        const pr_form *f0 = gc_form(p->species, 0);
        if (f0) p->tera_type = f0->type1;   /* always the primary tera type: the species' own first type */
    }
    const pr_form *f = gc_form(p->species, p->form);
    if (f) p->base_ability = p->ability = (uint16_t)gc_form_ability(p->species, f, m->ability_index);
    pr_update_passive(&run->bt, p);
    pr_calculate_stats(&run->bt, p);
}

/* ---------------------------------------------------------------- the win's ribbons */
/* GameOverPhase.awardRibbons: the ribbons a won run gives each Pokemon of the party (and its prevolutions); 0 = none (Moveset Randomizer).
 * A Classic run gives the Classic ribbon. In Challenge Mode every challenge with a value adds its own (Challenge.ribbonAwarded); Flip Stat or
 * Inverse Battle keep only their own ribbons, Passives on "all" only the Passives one; otherwise Classic's is added, and the Nuzlocke ribbon
 * when Fresh Start, Limited Catch and Hardcore are all on. */
uint64_t gc_challenge_ribbon_flags(const gc_run *run)
{
    if (run->run.mode != GAME_MODES_CHALLENGE) return GC_RIBBON_CLASSIC;
    const uint8_t *v = run->challenges.value;
    if (v[CHALLENGES_MOVESET_RANDOMIZER]) return 0;
    uint64_t f = 0;
    if (v[CHALLENGES_SINGLE_GENERATION]) f |= GC_RIBBON_MONO_GEN_1 << (v[CHALLENGES_SINGLE_GENERATION] - 1);
    if (v[CHALLENGES_SINGLE_TYPE]) f |= GC_RIBBON_MONO_NORMAL << (v[CHALLENGES_SINGLE_TYPE] - 1);
    if (v[CHALLENGES_FRESH_START]) f |= GC_RIBBON_FRESH_START;
    if (v[CHALLENGES_INVERSE_BATTLE]) f |= GC_RIBBON_INVERSE;
    if (v[CHALLENGES_FLIP_STAT]) f |= GC_RIBBON_FLIP_STATS;
    if (v[CHALLENGES_LIMITED_CATCH]) f |= GC_RIBBON_LIMITED_CATCH;
    if (v[CHALLENGES_HARDCORE]) f |= GC_RIBBON_HARDCORE;
    if (v[CHALLENGES_PASSIVES]) f |= GC_RIBBON_PASSIVE;
    switch (v[CHALLENGES_LIMITED_SUPPORT]) {
    case 1: f |= GC_RIBBON_NO_HEAL; break;
    case 2: f |= GC_RIBBON_NO_SHOP; break;
    case 3: f |= GC_RIBBON_NO_HEAL | GC_RIBBON_NO_SHOP | GC_RIBBON_NO_SUPPORT; break;
    default: break;
    }
    const uint64_t flip_or_inverse = f & (GC_RIBBON_FLIP_STATS | GC_RIBBON_INVERSE);
    if (flip_or_inverse) return flip_or_inverse;
    if (v[CHALLENGES_PASSIVES] == 2) return f & GC_RIBBON_PASSIVE;
    f |= GC_RIBBON_CLASSIC;
    if (v[CHALLENGES_FRESH_START] && v[CHALLENGES_LIMITED_CATCH] && v[CHALLENGES_HARDCORE]) f |= GC_RIBBON_NUZLOCKE;
    return f;
}
