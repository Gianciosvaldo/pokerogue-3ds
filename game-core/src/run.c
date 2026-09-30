/* The run: a new run and its starters, the event queue, the screen/choice loop, and the run-level
 * phase queue (upstream's PhaseTree, src/phase-tree.ts, for the phases between battle-core's turns). */
#include "internal.h"
#include <stdarg.h>
#include <stdio.h>

_Static_assert(sizeof(gc_internal) <= sizeof(((gc_run *)0)->internal_), "gc_run.internal_ is too small");
_Static_assert(PR_AI_FEAT_COUNT_ <= GC_MAX_UNPORTED, "gc_run.unported is too small for ai-core's ids");
_Static_assert(GC_STUB_COUNT_ <= 16, "gc_run.stubs is too small");
gc_internal *gc_int(gc_run *run) { return (gc_internal *)(void *)run->internal_; }

const char *const gc_stub_name[GC_STUB_COUNT_] = {
    [GC_STUB_ENEMY_MOVESET] = "enemy moveset (last 4 level moves)",
    [GC_STUB_REWARDS] = "rewards (placeholder list)",
    [GC_STUB_DOUBLE_AS_SINGLE] = "double battle played as single",
    [GC_STUB_MYSTERY_ENCOUNTER] = "mystery encounter played as wild",
    [GC_STUB_TRAINER_NEXT] = "trainer's next Pokemon without summon phases",
    [GC_STUB_TURN_TAIL] = "rest of the turn after a victory lost",
    [GC_STUB_EVOLUTION] = "evolution condition not checked",
    [GC_STUB_WEATHER] = "biome weather roll",
    [GC_STUB_UNPORTED_ENEMY] = "enemy run-core can't generate (stand-in)",
    [GC_STUB_CATCH] = "catching",
    [GC_STUB_BATTLE_START] = "summon phases at battle start",
    [GC_STUB_TRAINER_REWARDS] = "trainer's own rewards",
    [GC_STUB_POST_BATTLE] = "post-battle abilities, Pay Day, lapsing items",
    [GC_STUB_ENEMY_SWITCH] = "trainer's switch-out (enemy loses the turn)",
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

void gc_ev_msg(gc_run *run, const char *key, int n, ...)
{
    gc_event *ev = gc_ev_new(run, GC_EV_MESSAGE);
    if (!ev) return;
    ev->msg.key = key;
    va_list ap;
    va_start(ap, n);
    for (int i = 0; i < n; i++) {
        pr_msg_arg a = va_arg(ap, pr_msg_arg);
        if (i < PR_MSG_MAX_ARGS) ev->msg.args[ev->msg.n_args++] = a;
    }
    va_end(ap);
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

/* trainer.getName(): the trainer classes' and names' locale keys aren't dumped yet */
pr_msg_arg gc_m_trainer(gc_run *run, const char *name)
{
    (void)run;
    pr_msg_arg a = {name, PR_ARG_STR, 0, "Trainer"};
    return a;
}

/* modifierType.name: "modifierType:ModifierType.<KEY>.name" for the plain modifier types */
pr_msg_arg gc_m_item(const char *name, const char *item_key)
{
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

/* battle-core's events from its last call, kept for the app until the next choice */
void gc_ev_battle(gc_run *run)
{
    const pr_engine *e = &run->eng;
    if (!e->n_events) return;
    const int first = run->n_battle_events;
    int n = e->n_events;
    if (first + n > GC_MAX_BATTLE_EVENTS) { n = GC_MAX_BATTLE_EVENTS - first; run->battle_events_overflow = true; }
    if (e->events_overflow) run->battle_events_overflow = true;
    if (n <= 0) return;
    memcpy(&run->battle_events[first], e->events, sizeof(pr_event) * (size_t)n);
    run->n_battle_events += n;
    gc_event *ev = gc_ev_new(run, GC_EV_BATTLE);
    if (ev) { ev->a = first; ev->b = n; }
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
static void run_until_screen(gc_run *run)
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
    run->trainer_type = run->trainer_variant = -1;
    run->screen.kind = GC_SCREEN_STARTERS;
}

bool gc_run_set_starters(gc_run *run, const gc_starter *starters, int n, const char **why)
{
    const char *dummy;
    if (!why) why = &dummy;
    if (run->screen.kind != GC_SCREEN_STARTERS) { *why = "not on the starter screen"; return false; }
    if (n < 1 || n > 6) { *why = "starterSelectUiHandler:invalidParty"; return false; }
    int cost = 0;
    for (int i = 0; i < n; i++) {
        int c = gc_starter_cost(starters[i].species);
        if (c < 0) { *why = "not a starter species"; return false; }
        const pr_species *sp = gc_species(starters[i].species);
        if (!sp || starters[i].form >= (sp->nforms ? sp->nforms : 1)) { *why = "no such form"; return false; }
        if (starters[i].ability_index > 2 || starters[i].nature > 24) { *why = "bad ability or nature"; return false; }
        for (int k = 0; k < i; k++)
            if (starters[k].species == starters[i].species) { *why = "the same species twice"; return false; }
        cost += c;
    }
    if (cost > gc_starter_budget(run->run.mode)) { *why = "starterSelectUiHandler:cost"; return false; }
    /* SelectStarterPhase.initBattle: the party at the starting level, each built on Phaser.Math.RND */
    const int level = pr_run_get_starting_level(&run->run);
    for (int i = 0; i < n; i++) {
        gc_mon_new_player(run, &run->party[i], &starters[i], level);
        run->party_order[i] = (int8_t)i;
    }
    run->n_party = n;
    memset(&run->screen, 0, sizeof run->screen);
    gc_rq_push(run, gc_rp(RP_NEW_BATTLE, 0, 0, 0));
    run_until_screen(run);
    return true;
}

const gc_screen *gc_run_screen(const gc_run *run) { return &run->screen; }

bool gc_run_choose(gc_run *run, const gc_choice *c, const char **why)
{
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
        run_until_screen(run);
    }
    return true;
}

/* ---------------------------------------------------------------- saving (the next slice) */
bool gc_run_can_save(const gc_run *run) { (void)run; return false; }
size_t gc_run_save(const gc_run *run, void *buf, size_t cap) { (void)run; (void)buf; (void)cap; return 0; }
bool gc_run_load(gc_run *run, const void *buf, size_t len, const char **why)
{
    (void)run; (void)buf; (void)len;
    if (why) *why = "saving isn't ported yet";
    return false;
}
