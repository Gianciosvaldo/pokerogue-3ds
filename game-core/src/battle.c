/* The battle as game-core drives it through battle-core, until battle-core publishes the battle API of
 * its own (the battle thread's "a battle the game can drive"): the field at the start of a battle, the
 * TurnInitPhase's bookkeeping, the player's CommandPhase, the turn itself and the switch prompts.
 *
 * Singles only. What this layer leaves out is counted in run->stubs or comes back from battle-core as
 * unported: the summon phases at the start of a battle (entry abilities, hazards), the trainer's next
 * Pokemon's SwitchSummonPhase, catching and fleeing, and the command restrictions (Disable, Taunt,
 * Encore, Imprison, trapping abilities) CommandPhase checks. */
#include "internal.h"

static int index_of(const gc_mon *arr, int n, const pr_pokemon *p)
{
    for (int i = 0; i < n; i++) if (&arr[i].b == p) return i;
    return -1;
}
int gc_player_active_index(const gc_run *run) { return run->bt.field[0] ? index_of(run->party, run->n_party, run->bt.field[0]) : -1; }
int gc_enemy_active_index(const gc_run *run) { return run->bt.field[2] ? index_of(run->enemy, run->n_enemy, run->bt.field[2]) : -1; }
const gc_mon *gc_field_mon(const gc_run *run, int side)
{
    int i = side ? gc_enemy_active_index(run) : gc_player_active_index(run);
    return i < 0 ? NULL : side ? &run->enemy[i] : &run->party[i];
}

static void summon(gc_run *run, int side, int idx)
{
    gc_mon *m = side ? &run->enemy[idx] : &run->party[idx];
    pr_reset_summon_data(&m->b);
    pr_reset_turn_data(&m->b);
    m->b.on_field = true;
    m->b.switch_out_status = false;
    run->bt.field[side ? 2 : 0] = &m->b;
    gc_event *ev = gc_ev_new(run, GC_EV_SUMMON);
    if (ev) { ev->a = side; ev->b = idx; ev->c = 0; }
}

/* The scene for a new battle: battle-core's pr_battle built from the run. SummonPhase swaps a fainted
 * lead with the first Pokemon that can fight, as upstream reorders the party.
 *
 * Between two battles in the same biome (no trainer, no mystery encounter, not the final boss before),
 * upstream keeps the arena (weather, terrain, the arena tags) and leaves the player's lead on the field
 * with its summon data (stat stages, tags): doPostBattleCleanup only recalls and resets on
 * resetArenaState. reset_arena says which case this is. true when the player's lead was sent out. */
bool gc_battle_setup_field(gc_run *run, bool reset_arena)
{
    pr_battle *bt = &run->bt;
    /* what the arena keeps */
    const int lead = run->party_order[0];
    const bool keep_lead = !reset_arena && lead >= 0 && lead < run->n_party && run->party[lead].b.on_field
                           && run->party[lead].b.hp > 0;
    uint8_t weather = bt->weather, terrain = bt->terrain;
    int16_t wt = bt->weather_turns_left, wm = bt->weather_max_duration, tt = bt->terrain_turns_left,
            tm = bt->terrain_max_duration;
    static pr_arena_tag tags[PR_MAX_ARENA_TAGS];
    static pr_positional_tag pos[PR_MAX_POSITIONAL_TAGS];
    const uint8_t n_tags = bt->n_arena_tags, n_pos = bt->n_pos_tags;
    memcpy(tags, bt->arena_tags, sizeof tags);
    memcpy(pos, bt->pos_tags, sizeof pos);
    const int player_faints = bt->player_faints;

    memset(bt, 0, sizeof *bt);
    if (!reset_arena) {
        bt->weather = weather; bt->terrain = terrain;
        bt->weather_turns_left = wt; bt->weather_max_duration = wm;
        bt->terrain_turns_left = tt; bt->terrain_max_duration = tm;
        memcpy(bt->arena_tags, tags, sizeof tags); bt->n_arena_tags = n_tags;
        memcpy(bt->pos_tags, pos, sizeof pos); bt->n_pos_tags = n_pos;
        bt->player_faints = player_faints;
    } else {
        /* resetArenaEffects: the biome's weather is rolled again (not ported: no weather) */
        gc_stub(run, GC_STUB_WEATHER);
    }
    bt->rng = run->run.rng;
    bt->rng_live = true;
    pr_seed_from_ascii(&bt->battle_seed, run->battle_seed);
    bt->battle_rng_live = false;
    bt->wave_seed = run->run.wave_seed;
    bt->turn = 1;
    bt->double_battle = false;
    bt->wave_index = run->wave;
    bt->game_mode = (int8_t)run->run.mode;
    bt->battle_type = (uint8_t)run->battle_type;
    bt->biome = (int16_t)run->biome;
    bt->has_biome = true;
    bt->last_player_involved = bt->last_enemy_involved = -1;
    bt->ignoring_effect_source = -1;
    bt->last_used_pokeball = -1;
    bt->max_exp_level = (int)pr_run_get_max_exp_level(&run->run, run->wave, false);
    for (int k = 0; k < 4; k++) bt->commands[k].command = bt->pre_commands[k].command = -1;
    gc_items_to_battle(run);

    if (!keep_lead) {
        /* ReturnPhase and resetBattleAndWaveData: everyone off the field, their summon data cleared */
        for (int i = 0; i < run->n_party; i++) {
            if (run->party[i].b.on_field) {
                gc_event *ev = gc_ev_new(run, GC_EV_RECALL);
                if (ev) { ev->a = 0; ev->b = i; }
            }
            run->party[i].b.on_field = false;
            pr_reset_summon_data(&run->party[i].b);
            pr_reset_turn_data(&run->party[i].b);
        }
        int first = -1;
        for (int k = 0; k < run->n_party; k++) if (gc_mon_allowed(&run->party[run->party_order[k]])) { first = k; break; }
        if (first > 0) {
            int8_t t = run->party_order[0];
            run->party_order[0] = run->party_order[first];
            run->party_order[first] = t;
        }
    }
    for (int k = 0; k < run->n_party; k++) {
        gc_mon *m = &run->party[run->party_order[k]];
        m->b.is_player = true;
        bt->party[0][k] = &m->b;
        pr_update_passive(bt, &m->b);
    }
    bt->n_party[0] = (uint8_t)run->n_party;
    for (int i = 0; i < run->n_enemy; i++) {
        run->enemy[i].b.on_field = false;
        run->enemy[i].b.is_player = false;
        bt->party[1][i] = &run->enemy[i].b;
        pr_update_passive(bt, &run->enemy[i].b);
    }
    bt->n_party[1] = (uint8_t)run->n_enemy;
    summon(run, 1, 0);
    gc_stub(run, GC_STUB_BATTLE_START);
    if (keep_lead) {
        bt->field[0] = &run->party[lead].b;
        pr_reset_turn_data(&run->party[lead].b);
        return false;
    }
    if (run->n_party && gc_mon_allowed(&run->party[run->party_order[0]])) {
        summon(run, 0, run->party_order[0]);
        return true;
    }
    return false;
}

/* battle-core's party list is upstream's party order: switching swaps its entries */
void gc_sync_party_order(gc_run *run)
{
    const pr_battle *bt = &run->bt;
    if (bt->n_party[0] != run->n_party) return;
    for (int k = 0; k < run->n_party; k++) {
        int i = index_of(run->party, run->n_party, bt->party[0][k]);
        if (i >= 0) run->party_order[k] = (int8_t)i;
    }
}

int gc_battle_slot_of(const gc_run *run, int party_index)
{
    for (int k = 0; k < run->n_party; k++) if (run->party_order[k] == party_index) return k;
    return -1;
}

/* TurnInitPhase: each active Pokemon's turn data is reset, and the player's join the participants */
void gc_battle_turn_init(gc_run *run)
{
    pr_battle *bt = &run->bt;
    for (int k = 0; k < 4; k++) {
        bt->commands[k].command = bt->pre_commands[k].command = -1;
        bt->commands[k].skip = bt->pre_commands[k].skip = false;
    }
    for (int s = 0; s < 4; s += 2) {
        pr_pokemon *p = bt->field[s];
        if (!p || !p->on_field || p->hp <= 0) continue;
        if (s == 0) {
            bool have = false;
            for (int i = 0; i < bt->n_participants; i++) have |= bt->participants[i] == p->id;
            if (!have && bt->n_participants < PR_MAX_PARTY) {
                /* kept sorted, as the producer lists the Set */
                int i = bt->n_participants++;
                while (i > 0 && bt->participants[i - 1] > p->id) { bt->participants[i] = bt->participants[i - 1]; i--; }
                bt->participants[i] = p->id;
            }
        }
        pr_reset_turn_data(p);
    }
}

static bool trapped(gc_run *run, const pr_pokemon *p)
{
    /* isTrapped(): the trapping tags; Shadow Tag, Arena Trap and Magnet Pull aren't checked yet */
    return pr_has_battler_tag_class(&run->bt, p, PR_TAGCLASS_TRAPPED);
}

static int allowed_bench(const gc_run *run)
{
    int n = 0;
    for (int i = 0; i < run->n_party; i++)
        if (gc_mon_allowed(&run->party[i]) && !run->party[i].b.on_field) n++;
    return n;
}

void gc_battle_command_screen(gc_run *run)
{
    gc_screen *s = &run->screen;
    memset(s, 0, sizeof *s);
    s->kind = GC_SCREEN_COMMAND;
    s->slot = 0;
    const pr_pokemon *p = run->bt.field[0];
    bool any = false;
    for (int i = 0; i < 4; i++) {
        s->move_ok[i] = p && gc_move_usable(p, i, &s->move_why[i]);
        any |= s->move_ok[i];
    }
    s->struggle = p && !any;
    s->can_switch = p && allowed_bench(run) > 0 && !trapped(run, p);
    s->can_ball = false;   /* catching isn't ported yet (AttemptCapturePhase) */
    s->can_run = false;    /* nor fleeing (AttemptRunPhase) */
}

/* CommandPhase.handleFightCommand / handleSwitchCommand for field slot 0 */
bool gc_battle_player_command(gc_run *run, const gc_choice *c, const char **why)
{
    pr_battle *bt = &run->bt;
    pr_pokemon *p = bt->field[0];
    struct pr_turn_command *tc = &bt->commands[0], *pc = &bt->pre_commands[0];
    if (!p) { *why = "no Pokemon on the field"; return false; }
    if (c->kind == GC_CH_FIGHT) {
        int slot = c->a;
        const char *reason = "";
        bool can = gc_move_usable(p, slot, &reason);
        bool struggle = false;
        if (!can) {
            bool any = false;
            for (int i = 0; i < 4; i++) any |= gc_move_usable(p, i, NULL);
            struggle = slot >= 0 && !any;
            if (!struggle) { *why = reason; return false; }
        }
        int move = struggle ? MOVE_ID_STRUGGLE : p->moves[slot];
        memset(tc, 0, sizeof *tc);
        tc->command = COMMAND_FIGHT;
        tc->cursor = (int8_t)slot;
        tc->move = (uint16_t)move;
        tc->use_mode = MOVE_USE_MODE_NORMAL;
        int8_t tg[4];
        bool multiple = false;
        int n = pr_get_move_targets(bt, p, move, -1, tg, &multiple);
        if (n > 4) n = 4;
        /* singles: a move with one target, or a spread move, takes them as they are */
        tc->n_targets = (uint8_t)n;
        memcpy(tc->targets, tg, (size_t)n);
        memset(pc, 0, sizeof *pc);
        pc->command = COMMAND_FIGHT;
        pc->targets[0] = 0;
        pc->n_targets = 1;
        pc->skip = true;
        pc->cursor = PR_CURSOR_UNDEFINED;
        return true;
    }
    if (c->kind == GC_CH_POKEMON) {
        int k = c->a;
        if (k < 0 || k >= run->n_party || run->party[k].b.on_field || !gc_mon_allowed(&run->party[k])) {
            *why = "partyUiHandler:noEnergy";
            return false;
        }
        if (trapped(run, p)) { *why = "battle:noEscapePokemon"; return false; }
        memset(tc, 0, sizeof *tc);
        tc->command = COMMAND_POKEMON;
        tc->cursor = (int8_t)gc_battle_slot_of(run, k);
        tc->baton = false;
        tc->no_move = true;
        return true;
    }
    if (c->kind == GC_CH_BALL) { gc_stub(run, GC_STUB_CATCH); *why = "catching isn't ported yet"; return false; }
    if (c->kind == GC_CH_RUN) { *why = "fleeing isn't ported yet"; return false; }
    *why = "not a command";
    return false;
}

/* A move locked in by the last turn (a charging move's second turn, Outrage, a recharge turn): upstream's
 * CommandPhase.tryExecuteQueuedMove skips the screen */
bool gc_battle_queued_command(gc_run *run)
{
    pr_battle *bt = &run->bt;
    pr_pokemon *p = bt->field[0];
    if (!p || !p->n_move_queue) return false;
    const pr_turn_move *q = &p->move_queue[0];
    struct pr_turn_command *tc = &bt->commands[0], *pc = &bt->pre_commands[0];
    memset(tc, 0, sizeof *tc);
    tc->command = COMMAND_FIGHT;
    tc->move = q->move;
    tc->use_mode = q->use_mode;
    tc->n_targets = q->n_targets;
    memcpy(tc->targets, q->targets, sizeof tc->targets);
    int slot = -1;
    for (int i = 0; i < p->n_moves; i++) if (p->moves[i] == q->move) { slot = i; break; }
    tc->cursor = (int8_t)(q->move == 0 ? -1 : slot);
    memset(pc, 0, sizeof *pc);
    pc->command = COMMAND_FIGHT;
    pc->targets[0] = 0;
    pc->n_targets = 1;
    pc->skip = true;
    pc->cursor = PR_CURSOR_UNDEFINED;
    return true;
}

static void note_unported(gc_run *run)
{
    pr_battle *bt = &run->bt;
    for (int i = 0; i < bt->n_unported; i++) run->unported[bt->unported[i]]++;
    for (int i = 0; i < bt->n_deferred; i++) run->unported[bt->deferred[i]]++;
    bt->n_unported = bt->n_deferred = 0;
}

/* HP, faint and field changes seen from the state before and after a turn (battle-core's own event
 * stream will replace this) */
typedef struct { int32_t hp[2][6]; bool on[2][6]; } snapshot;
static void snap(const gc_run *run, snapshot *s)
{
    for (int i = 0; i < run->n_party; i++) { s->hp[0][i] = run->party[i].b.hp; s->on[0][i] = run->bt.field[0] == &run->party[i].b; }
    for (int i = 0; i < run->n_enemy; i++) { s->hp[1][i] = run->enemy[i].b.hp; s->on[1][i] = run->bt.field[2] == &run->enemy[i].b; }
}
static void diff(gc_run *run, const snapshot *a)
{
    snapshot b;
    snap(run, &b);
    for (int side = 0; side < 2; side++) {
        int n = side ? run->n_enemy : run->n_party;
        for (int i = 0; i < n; i++) {
            if (a->on[side][i] && !b.on[side][i]) {
                gc_event *ev = gc_ev_new(run, GC_EV_RECALL);
                if (ev) { ev->a = side; ev->b = i; }
            }
            if (!a->on[side][i] && b.on[side][i]) {
                gc_event *ev = gc_ev_new(run, GC_EV_SUMMON);
                if (ev) { ev->a = side; ev->b = i; ev->c = 0; }
            }
            if (a->hp[side][i] != b.hp[side][i]) {
                gc_event *ev = gc_ev_new(run, GC_EV_HP);
                if (ev) { ev->a = side; ev->b = i; ev->c = b.hp[side][i]; }
                if (b.hp[side][i] <= 0 && a->hp[side][i] > 0) {
                    ev = gc_ev_new(run, GC_EV_FAINT);
                    if (ev) { ev->a = side; ev->b = i; }
                    gc_ev_msg(run, "battle:fainted", 1, gc_a_mon("pokemonNameWithAffix", side, i));
                }
            }
        }
    }
}

int gc_battle_run(gc_run *run, int pick)
{
    pr_battle *bt = &run->bt;
    snapshot before;
    snap(run, &before);
    if (pick < -1) {
        gc_event *ev = gc_ev_new(run, GC_EV_TURN);
        if (ev) ev->a = bt->turn;
        pr_run_turn(bt, &run->eng);
    } else {
        pr_resume_switch(bt, &run->eng, pick >= 0 ? gc_battle_slot_of(run, pick) : -1);
    }
    note_unported(run);
    gc_sync_party_order(run);
    diff(run, &before);
    run->run.rng = bt->rng;
    return run->eng.stop;
}

void gc_battle_switch_screen(gc_run *run)
{
    gc_screen *s = &run->screen;
    memset(s, 0, sizeof *s);
    s->kind = GC_SCREEN_SWITCH;
    s->slot = run->eng.stop_phase.sw_field_index;
    for (int i = 0; i < run->n_party; i++)
        s->pick_ok[i] = gc_mon_allowed(&run->party[i]) && !run->party[i].b.on_field;
    /* a modal SwitchPhase (after a faint) must be answered; a move's switch-out can be left empty */
    s->can_skip = !run->eng.stop_phase.sw_modal;
}

/* The trainer sends out its next Pokemon (FaintPhase's SwitchSummonPhase for the enemy, which battle-core
 * would run after the VictoryPhase): the next one that can fight, straight onto the field */
bool gc_battle_trainer_next(gc_run *run)
{
    for (int i = 0; i < run->n_enemy; i++) {
        gc_mon *m = &run->enemy[i];
        if (m->b.hp > 0 && !m->b.on_field) {
            pr_pokemon *old = run->bt.field[2];
            if (old) old->on_field = false;
            summon(run, 1, i);
            gc_stub(run, GC_STUB_TRAINER_NEXT);
            gc_ev_msg(run, "battle:trainerSendOut", 2, gc_a_trainer("trainerName"), gc_a_mon("pokemonName", 1, i));
            return true;
        }
    }
    return false;
}

/* BattleEndPhase's part: the battle is over. The player's Pokemon stay where they are: the next
 * battle's setup recalls them or keeps the lead out (see gc_battle_setup_field). */
void gc_battle_end_cleanup(gc_run *run)
{
    run->run.rng = run->bt.rng;
    gc_sync_party_order(run);
    for (int i = 0; i < run->n_party; i++) pr_reset_turn_data(&run->party[i].b);
    run->bt.field[2] = run->bt.field[3] = NULL;
    run->bt.n_party[1] = 0;
    run->in_battle = false;
}

/* The engine stopped at a boundary phase (a VictoryPhase) with phases of the turn still queued: the
 * rest of the turn (other moves, the end-of-turn effects) is lost until battle-core can carry on past
 * a stop */
bool gc_battle_turn_tail_lost(const gc_run *run)
{
    const pr_engine *e = &run->eng;
    for (int l = 0; l < e->n_levels; l++)
        for (int k = 0; k < e->level_len[l]; k++) {
            int t = e->pool[e->levels[l][k]].type;
            if (t == PH_MARKER || (t >= PH_TURN_START && t <= PH_SWITCH)) return true;
        }
    for (int t = 0; t < PH_COUNT_; t++) if (e->n_dyn[t]) return true;
    return false;
}
