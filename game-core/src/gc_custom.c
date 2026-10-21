/* The optional hooks of the custom content (gc/custom.h, task C10): what game-core tells the app about a wave and a faint, and the one
 * thing it lets the app decide, the identity of the enemy of an ordinary wild encounter the player agreed to fight.
 * Custom callbacks run only with installed hooks. The faint observer also keeps
 * the run's read-only final-blow provenance for ordinary run reports. */
#include "internal.h"

static void actor_of(const gc_run *run, const pr_pokemon *p, bool enemy, gc_custom_actor *a)
{
    memset(a, 0, sizeof *a);
    a->id = p->id;
    a->species = a->start_species = p->species;
    a->form = a->start_form = p->form;
    if (enemy) {
        const gc_internal *in = gc_int((gc_run *)run);
        for (int i = 0; i < run->n_enemy && i < 6; i++)
            if (&run->enemy[i].b == p) { a->start_species = in->enemy_start_species[i]; a->start_form = in->enemy_start_form[i]; }
    }
    a->gender = p->gender;
    a->shiny = p->shiny;
    a->level = p->level;
    a->nature = p->nature;
    a->ability = p->base_ability;
    a->passive = p->passive;
    memcpy(a->ivs, p->ivs, 6);
    a->n_moves = p->n_moves > GC_CUSTOM_MAX_MOVES ? GC_CUSTOM_MAX_MOVES : p->n_moves;
    for (int i = 0; i < a->n_moves; i++) a->moves[i] = p->moves[i];
    a->is_boss = p->is_boss;
    a->fused = p->fusion_species != 0;
    a->tera = p->terastallized;
    a->trainer_owned = p->has_trainer;
    if (enemy) {
        for (int i = 0; i < run->n_enemy_items && a->n_items < GC_CUSTOM_MAX_ITEMS; i++) {
            const gc_item *it = &run->enemy_items[i];
            if (it->pokemon_id != p->id || !it->key) continue;
            a->items[a->n_items].key = it->key;
            a->items[a->n_items].arg = it->arg;
            a->items[a->n_items].stack = it->stack < 0 ? 0 : it->stack > 65535 ? 65535 : (uint16_t)it->stack;
            a->n_items++;
        }
    }
}

static bool is_enemy_of(const gc_run *run, const pr_pokemon *p)
{
    for (int i = 0; i < run->n_enemy; i++) if (&run->enemy[i].b == p) return true;
    return false;
}

/* Copy provenance before the note expires, then notify optional custom hooks. */
static void faint_cb(const pr_battle *bt, const pr_faint_note *n, void *ctx)
{
    gc_run *run = (gc_run *)ctx;
    (void)bt;
    const gc_custom_hooks *h = gc_int(run)->custom;
    if (!n->fainted || n->side > 1) return;
    gc_faint_record *last = &run->last_faint[n->side];
    memset(last, 0, sizeof *last);
    last->observed = true;
    last->remaining = n->remaining;
    last->wave = n->wave;
    last->turn = n->turn;
    if (n->direct && n->source && n->move) {
        last->direct = true;
        last->source_side = n->source->is_player ? 0 : 1;
        last->species = n->source->transformed ? n->source->transform_species : n->source->species;
        last->form = n->source->transformed ? n->source->transform_form : n->source->form;
        last->move = n->move;
    }
    if (!h || !h->faint) return;
    gc_custom_faint f;
    memset(&f, 0, sizeof f);
    f.wave = n->wave;
    f.turn = n->turn;
    f.side = n->side;
    f.remaining = n->remaining;
    f.direct = n->direct;
    f.move = n->move;
    actor_of(run, n->fainted, n->side == 1, &f.fainted);
    if (n->direct && n->source) {
        f.has_source = true;
        f.source_side = is_enemy_of(run, n->source) ? 1 : 0;
        actor_of(run, n->source, f.source_side == 1, &f.source);
    }
    h->faint(h->user, run, &f);
}

void gc_custom_install_observer(gc_run *run)
{
    run->bt.faint_observer = faint_cb;
    run->bt.faint_observer_ctx = run;
}

void gc_run_set_custom(gc_run *run, const gc_custom_hooks *hooks)
{
    gc_int(run)->custom = hooks;
    gc_custom_install_observer(run);
}

bool gc_custom_report(gc_run *run, const gc_custom_encounter *enc)
{
    const gc_custom_hooks *h = gc_int(run)->custom;
    if (!h || !h->encounter) return false;
    return h->encounter(h->user, run, enc);
}

/* ---------------------------------------------------------------- the enemy of an accepted rematch */
/* The index (0, 1, 2) of `ability` among the form's abilities; -1 when it is none of them. An ability the form does not have is never
 * mapped to another one. */
static int ability_index_of(int species, const pr_form *f, int ability)
{
    if (!ability) return -1;
    if (f->ability1 == ability) return 0;
    if (f->ability2 == ability) return 1;
    if (f->ability_hidden == ability) return 2;
    (void)species;
    return -1;
}

bool gc_custom_enemy_check(const gc_custom_enemy *spec, const char **why)
{
    const char *dummy;
    if (!why) why = &dummy;
    *why = "";
    if (!spec) { *why = "no enemy"; return false; }
    const pr_species *sp = gc_species(spec->species);
    if (!sp) { *why = "unknown species"; return false; }
    if (spec->form >= sp->nforms) { *why = "unknown form"; return false; }
    const pr_form *f = &pr_forms[sp->form0 + spec->form];
    if (f->unobtainable) { *why = "form not available"; return false; }
    if (sp->male_permille < 0) {
        if (spec->gender != -1) { *why = "gender does not fit the species"; return false; }
    } else if (spec->gender != 0 && spec->gender != 1) {
        *why = "gender does not fit the species";
        return false;
    } else if ((sp->male_permille == 0 && spec->gender == 0) || (sp->male_permille == 1000 && spec->gender == 1)) {
        *why = "gender does not fit the species";
        return false;
    }
    if (spec->has_nature && spec->nature >= 25) { *why = "unknown nature"; return false; }
    if (ability_index_of(spec->species, f, spec->ability) < 0) { *why = "ability is not one of the form's abilities"; return false; }
    if (spec->n_moves < 1 || spec->n_moves > GC_CUSTOM_MAX_MOVES) { *why = "moveset size"; return false; }
    for (int i = 0; i < spec->n_moves; i++) {
        const int m = spec->moves[i];
        if (m <= 0 || m >= PR_MOVE_COUNT || pr_moves[m].unimplemented) { *why = "unknown or unimplemented move"; return false; }
        for (int j = 0; j < i; j++) if (spec->moves[j] == m) { *why = "repeated move"; return false; }
    }
    return true;
}

/* The ordinary wild enemy (the only one of the battle) becomes the rematch enemy, at its own level and with its own id, IVs and friendship, through
 * gc_mon_new_enemy and the shared post-generation paths (stats, seen flags, items, AI): no stat or level rule of its own. The caller has checked
 * the spec; a battle that is not a single ordinary wild encounter is refused. */
bool gc_custom_build_enemy(gc_run *run, const gc_custom_enemy *spec, const char **why)
{
    const char *dummy;
    if (!why) why = &dummy;
    if (!gc_custom_enemy_check(spec, why)) return false;
    if (run->battle_type != BATTLE_TYPE_WILD || run->n_enemy != 1 || run->enemy[0].b.is_boss || run->enemy[0].b.fusion_species) {
        *why = "not a single ordinary wild encounter";
        return false;
    }
    gc_mon *m = &run->enemy[0];
    const pr_species *sp = gc_species(spec->species);
    const pr_form *f = &pr_forms[sp->form0 + spec->form];
    pr_enemy_gen g;
    memset(&g, 0, sizeof g);
    g.id = m->b.id;
    for (int i = 0; i < 6; i++) g.ivs[i] = m->b.ivs[i];
    g.ability_index = ability_index_of(spec->species, f, spec->ability);
    g.fusion_species = -1;
    g.gender = spec->gender;
    g.form = spec->form;
    g.nature = spec->has_nature ? spec->nature : m->b.nature;
    g.tera_type = f->type1;
    g.shiny = spec->shiny;
    g.variant = 0;
    g.luck = spec->shiny ? 1 : 0;
    g.friendship = -1;
    const int level = m->b.level;
    gc_mon_new_enemy(run, m, spec->species, level, &g, 0);
    for (int i = 0; i < spec->n_moves; i++) gc_mon_set_move(m, i, spec->moves[i]);
    run->n_enemy = 1;
    gc_enemy_finish_party(run);
    return true;
}

/* The answer on GC_SCREEN_CUSTOM_OFFER: the app's hook decides what is built, the encounter's second half (the save point and the battle's start)
 * then runs as it does for any wave. */
void gc_custom_answer(gc_run *run, bool accept)
{
    gc_internal *in = gc_int(run);
    const gc_custom_hooks *h = in->custom;
    const bool reset = in->custom_offer_reset, new_biome = in->custom_offer_new_biome;
    in->custom_offering = false;
    if (h && h->answer) {
        gc_custom_enemy spec;
        memset(&spec, 0, sizeof spec);
        if (h->answer(h->user, run, accept, &spec) && accept) {
            const char *why = "";
            const bool ok = gc_custom_build_enemy(run, &spec, &why);
            if (h->built) h->built(h->user, run, ok, ok ? "" : why);
        }
    }
    gc_new_battle_start(run, reset, new_biome);
}
