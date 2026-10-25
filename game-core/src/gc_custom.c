/* The optional hooks of the custom content (gc/custom.h, task C10): what game-core tells the app about a wave and a faint, and the one
 * thing it lets the app decide, the identity of the enemy of an ordinary wild encounter the player agreed to fight.
 * Custom callbacks run only with installed hooks. The faint observer also keeps
 * the run's read-only final-blow provenance for ordinary run reports. */
#include "internal.h"
#include <stdio.h>

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

/* ---------------------------------------------------------------- a trainer's team in place of the ordinary party */
bool gc_custom_team_check(const gc_custom_team *team, const char **why)
{
    const char *dummy;
    if (!why) why = &dummy;
    *why = "";
    if (!team) { *why = "no team"; return false; }
    if (!team->name || !team->name[0] || strlen(team->name) >= sizeof((gc_internal *)0)->custom_trainer_name) { *why = "trainer name"; return false; }
    if (team->n < 1 || team->n > GC_CUSTOM_MAX_TEAM) { *why = "team size"; return false; }
    if (team->passive < -1 || team->passive >= PR_ABILITY_COUNT) { *why = "unknown passive"; return false; }
    for (int e = 0; e < team->n; e++) {
        const gc_custom_team_member *t = &team->member[e];
        const pr_species *sp = gc_species(t->species);
        if (!sp) { *why = "unknown species"; return false; }
        if (t->form >= sp->nforms) { *why = "unknown form"; return false; }
        if (pr_forms[sp->form0 + t->form].unobtainable) { *why = "form not available"; return false; }
        if (t->ability_slot > 2) { *why = "ability slot"; return false; }
        if (t->variant > 2 || (!t->shiny && t->variant)) { *why = "shiny tier"; return false; }
        int n = 0;
        for (int i = 0; i < GC_CUSTOM_MAX_MOVES; i++) {
            const int m = t->moves[i];
            if (!m) continue;
            if (m < 0 || m >= PR_MOVE_COUNT || pr_moves[m].unimplemented) { *why = "unknown or unimplemented move"; return false; }
            for (int j = 0; j < i; j++) if (t->moves[j] == m) { *why = "repeated move"; return false; }
            n++;
        }
        if (!n) { *why = "no move"; return false; }
        if (t->n_items > GC_CUSTOM_MAX_TEAM_ITEMS) { *why = "too many items"; return false; }
        for (int i = 0; i < t->n_items; i++) {
            if (t->items[i].stack < 1 || t->items[i].stack > 99) { *why = "item stack"; return false; }
            if (!gc_item_stock_ported(t->items[i].key, t->items[i].arg, t->items[i].stack)) { *why = "item the battle layer does not know"; return false; }
        }
    }
    return true;
}

void gc_custom_set_trainer_name(gc_run *run, const char *name)
{
    gc_internal *in = gc_int(run);
    snprintf(in->custom_trainer_name, sizeof in->custom_trainer_name, "%s", name ? name : "");
    if (run->battle_type == BATTLE_TYPE_TRAINER) gc_trainer_names(run);
}

/* The ordinary trainer party (generated, not finished) becomes the team. Levels: the ordinary party's, a team with fewer members takes its
 * highest ones, one with more repeats the highest; ids, IVs and natures are the ordinary members' (the last one's for the extra members, with
 * the id moved so the ids stay apart), so nothing is drawn. */
bool gc_custom_build_trainer(gc_run *run, const gc_custom_team *team)
{
    gc_internal *in = gc_int(run);
    if (!gc_custom_team_check(team, NULL)) return false;
    const int n0 = run->n_enemy;
    if (run->battle_type != BATTLE_TYPE_TRAINER || run->double_battle || n0 < 1 || n0 > 6) return false;
    static gc_mon built[GC_CUSTOM_MAX_TEAM];
    uint32_t used_ids[6];
    int levels[6], ivs[6][6], nature[6];
    for (int i = 0; i < n0; i++) {
        levels[i] = run->enemy[i].b.level;
        used_ids[i] = run->enemy[i].b.id;
        nature[i] = run->enemy[i].b.nature;
        for (int k = 0; k < 6; k++) ivs[i][k] = run->enemy[i].b.ivs[k];
    }
    for (int e = 0; e < team->n; e++) {
        const gc_custom_team_member *t = &team->member[e];
        const int src = e < n0 ? e : n0 - 1;
        const int lvl = team->n < n0 ? levels[n0 - team->n + e] : levels[src];
        const pr_species *sp = gc_species(t->species);
        const pr_form *f = &pr_forms[sp->form0 + t->form];
        pr_enemy_gen g;
        memset(&g, 0, sizeof g);
        g.id = e < n0 ? used_ids[e] : used_ids[n0 - 1] + 0x9E3779B1u * (uint32_t)(e - n0 + 1);
        for (int k = 0; k < 6; k++) g.ivs[k] = ivs[src][k];
        g.ability_index = t->ability_slot;
        g.fusion_species = -1;
        if (sp->male_permille < 0) g.gender = -1;
        else if (sp->male_permille == 0) g.gender = 1;
        else if (sp->male_permille == 1000) g.gender = 0;
        else g.gender = (int)(((g.id * 2654435761u) >> 8) % 1000u) < sp->male_permille ? 0 : 1;
        g.form = t->form;
        g.nature = nature[src];
        g.tera_type = f->type1;
        g.shiny = t->shiny;
        g.variant = t->variant;
        g.luck = t->shiny ? t->variant + 1 : 0;
        g.friendship = -1;
        gc_mon *m = &built[e];
        gc_mon_new_enemy(run, m, t->species, lvl, &g, TRAINER_SLOT_TRAINER);
        m->b.n_moves = 0;
        for (int k = 0; k < 4; k++) m->b.moves[k] = 0;
        int slot = 0;
        for (int k = 0; k < GC_CUSTOM_MAX_MOVES; k++) if (t->moves[k]) gc_mon_set_move(m, slot++, t->moves[k]);
        if (team->passive >= 0) { m->b.custom_passive = team->passive; m->b.passive_flag = true; }
    }
    for (int e = 0; e < team->n; e++) run->enemy[e] = built[e];
    run->n_enemy = team->n;
    in->custom_team = *team;
    in->custom_team_on = true;
    snprintf(in->custom_trainer_name, sizeof in->custom_trainer_name, "%s", team->name);
    return true;
}

/* enemy_items for a team's battle: its members hold what the team lists (stacking equal items) instead of the wave's generated ones; the
 * tokens (no holder) stay. */
void gc_custom_team_items(gc_run *run)
{
    gc_internal *in = gc_int(run);
    if (!in->custom_team_on) return;
    int w = 0;
    for (int i = 0; i < run->n_enemy_items; i++) {
        bool member = false;
        for (int e = 0; e < run->n_enemy; e++) member |= run->enemy_items[i].pokemon_id && run->enemy_items[i].pokemon_id == run->enemy[e].b.id;
        if (!member) run->enemy_items[w++] = run->enemy_items[i];
    }
    run->n_enemy_items = w;
    for (int e = 0; e < run->n_enemy && e < in->custom_team.n; e++) {
        const gc_custom_team_member *t = &in->custom_team.member[e];
        for (int i = 0; i < t->n_items && run->n_enemy_items < GC_MAX_ITEMS; i++) {
            const char *key = t->items[i].key;
            bool stacked = false;
            for (int j = 0; j < run->n_enemy_items && !stacked; j++) {
                gc_item *o = &run->enemy_items[j];
                if (!strcmp(o->key, key) && o->arg == t->items[i].arg && o->pokemon_id == run->enemy[e].b.id) { o->stack += t->items[i].stack; stacked = true; }
            }
            if (stacked) continue;
            gc_item *it = &run->enemy_items[run->n_enemy_items++];
            memset(it, 0, sizeof *it);
            it->key = key;
            it->arg = t->items[i].arg;
            it->stack = t->items[i].stack;
            it->pokemon_id = run->enemy[e].b.id;
        }
    }
}

/* Whether the wave just generated is one where a team may replace the party: an ordinary random single trainer wave. Not a double, a boss, the
 * final wave, a fixed battle (rival, Gym, Elite Four) nor Classic's wave 20. `run` holds the generated, unfinished enemy party. */
bool gc_custom_trainer_wave(gc_run *run, int wave)
{
    if (run->battle_type != BATTLE_TYPE_TRAINER || run->double_battle || run->n_enemy < 1) return false;
    if (pr_run_is_wave_final(run->run.mode, wave) || pr_run_is_fixed_battle(&run->run, wave)) return false;
    if (run->run.mode == GAME_MODES_CLASSIC && wave == 20) return false;
    for (int i = 0; i < run->n_enemy; i++) if (run->enemy[i].b.is_boss) return false;
    return true;
}

/* The hook of an eligible trainer wave: asks the app for a team and builds it. True when the party became the team. */
bool gc_custom_trainer(gc_run *run, const gc_custom_encounter *enc)
{
    const gc_custom_hooks *h = gc_int(run)->custom;
    if (!h || !h->trainer) return false;
    static gc_custom_team team;
    memset(&team, 0, sizeof team);
    team.passive = -1;
    if (!h->trainer(h->user, run, enc, &team)) return false;
    return gc_custom_build_trainer(run, &team);
}
