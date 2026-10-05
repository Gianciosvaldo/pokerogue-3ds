/* The battle as game-core drives it through battle-core, until battle-core publishes the battle API of
 * its own (the battle thread's "a battle the game can drive"): the field at the start of a battle, the
 * TurnInitPhase's bookkeeping, the player's CommandPhase, the turn itself and the switch prompts.
 *
 * Singles only. What this layer leaves out is counted in run->stubs or comes back from battle-core as
 * unported: fleeing (catching is src/gc_catch.c; its roll is battle-core's). The enemy's commands, the
 * command restrictions and trapping come from ai-core, and so does the trainer's pick of its next Pokemon
 * (next_summon_cb), which battle-core's own SwitchSummonPhase asks for. */
#include "internal.h"

static void note_unported(gc_run *run);
static int next_summon_cb(pr_battle *bt, int trainer_slot, void *ctx);

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
static bool setup_field(gc_run *run, bool reset_arena, bool restoring)
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

    if (!keep_lead && !restoring) {
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
        if (!restoring) run->enemy[i].b.on_field = false;
        run->enemy[i].b.is_player = false;
        bt->party[1][i] = &run->enemy[i].b;
        pr_update_passive(bt, &run->enemy[i].b);
    }
    bt->n_party[1] = (uint8_t)run->n_enemy;
    if (restoring) {
        /* The legal save point is after summon effects. Rebuild pointers without
         * summoning again: otherwise Intimidate/hazards/item effects run twice. */
        bt->field[0] = &run->party[lead].b;
        for (int i = 0; i < run->n_enemy; i++)
            if (run->enemy[i].b.on_field) { bt->field[2] = &run->enemy[i].b; break; }
    } else if (run->battle_type != BATTLE_TYPE_TRAINER && run->n_enemy) {
        summon(run, 1, 0);
    }
    if (keep_lead) {
        bt->field[0] = &run->party[lead].b;
        pr_reset_turn_data(&run->party[lead].b);
    }
    /* CheckSwitchPhase asks "Will you switch?" only in the SWITCH battle style (run->ask_switch) */
    bt->battle_style_set = !run->ask_switch;
    bt->next_summon_index = next_summon_cb;
    bt->next_summon_ctx = run;
    bt->trainer_names[0] = run->trainer_names[GC_TNAME_TRAINER];
    bt->trainer_names[1] = run->trainer_names[GC_TNAME_PARTNER];
    if (!restoring) {
        pr_start_battle(bt, &run->eng);
        note_unported(run);
        gc_items_from_battle(run);
        gc_ev_battle(run);
    }
    gc_sync_party_order(run);
    run->run.rng = bt->rng;
    return !keep_lead && run->n_party && gc_mon_allowed(&run->party[run->party_order[0]]);
}

bool gc_battle_setup_field(gc_run *run, bool reset_arena)
{
    return setup_field(run, reset_arena, false);
}

void gc_battle_restore_field(gc_run *run)
{
    setup_field(run, false, true);
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

/* isTrapped(): the trapping tags, Fairy Lock, and the foe's Shadow Tag, Arena Trap or Magnet Pull */
static bool trapped(gc_run *run, pr_pokemon *p)
{
    return pr_ai_is_trapped(&run->bt, &gc_int(run)->ai, p);
}

/* PokemonMove.isUsable(pokemon, false, true) as the fight menu checks it: gc_move_usable's checks (with
 * upstream's text for no PP), then the restricting tags (Disable, Taunt, Torment, Encore, Throat Chop,
 * Heal Block, Gorilla Tactics) and the moves' own restrictions (Gravity, Belch, Stuff Cheeks, Gigaton
 * Hammer). Until battle-core's pr_move_usable gives each one's own text, a restricted move answers
 * with upstream's generic battle:moveCannotBeSelected. */
static bool player_move_usable(gc_run *run, const pr_pokemon *p, int slot, const char **why)
{
    if (!gc_move_usable(p, slot, why)) return false;
    if (!pr_ai_move_usable(&run->bt, p, slot, false, true)) {
        if (why) *why = "battle:moveCannotBeSelected";
        return false;
    }
    return true;
}

static int allowed_bench(const gc_run *run)
{
    int n = 0;
    for (int i = 0; i < run->n_party; i++)
        if (gc_mon_allowed(&run->party[i]) && !run->party[i].b.on_field) n++;
    return n;
}

static bool has_any_pokeball(const gc_run *run)
{
    for (int i = 0; i < 8; i++) if (run->pokeballs[i] > 0) return true;
    return false;
}

void gc_battle_command_screen(gc_run *run)
{
    gc_screen *s = &run->screen;
    memset(s, 0, sizeof *s);
    s->kind = GC_SCREEN_COMMAND;
    s->slot = 0;
    pr_pokemon *p = run->bt.field[0];
    bool any = false;
    for (int i = 0; i < 4; i++) {
        s->move_ok[i] = p && player_move_usable(run, p, i, &s->move_why[i]);
        any |= s->move_ok[i];
    }
    s->struggle = p && !any;
    s->can_switch = p && allowed_bench(run) > 0 && !trapped(run, p);
    s->can_ball = run->battle_type == BATTLE_TYPE_WILD && has_any_pokeball(run);
    s->can_run = p && pr_can_run(&run->bt, p, NULL);
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
        bool can = player_move_usable(run, p, slot, &reason);
        bool struggle = false;
        if (!can) {
            bool any = false;
            for (int i = 0; i < 4; i++) any |= player_move_usable(run, p, i, NULL);
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
    if (c->kind == GC_CH_BALL) {
        int ball = c->a;
        if (ball < 0 || ball >= 8 || run->pokeballs[ball] == 0) { *why = "no ball of that type"; return false; }
        /* CommandPhase.checkCanUseBall + handleBallCommand: the command takes the five types up to the Master Ball */
        const char *refused = gc_catch_ball_refusal(run, ball);
        if (refused) { *why = refused; return false; }
        if (ball >= 5) { *why = "that ball can't be thrown"; return false; }
        /* AttemptCapturePhase spends the ball when it starts; it always does once the command is given. The
         * battle's lastUsedPokeball is set only by a throw that fails (battle-core's phase), and the type a catch
         * is made with is this one */
        run->pokeballs[ball]--;
        gc_int(run)->thrown_ball = (uint8_t)ball;
        memset(tc, 0, sizeof *tc);
        tc->command = COMMAND_BALL;
        tc->cursor = (int8_t)ball;
        tc->n_targets = 1;
        tc->targets[0] = 2;
        memset(pc, 0, sizeof *pc);
        pc->command = COMMAND_BALL;
        pc->targets[0] = 0;
        pc->n_targets = 1;
        pc->skip = true;
        pc->cursor = PR_CURSOR_UNDEFINED;
        return true;
    }
    if (c->kind == GC_CH_RUN) {
        pr_msg run_why = {0};
        if (!p || !pr_can_run(bt, p, &run_why)) {
            *why = run_why.key ? run_why.key : "battle:noEscapePokemon";
            return false;
        }
        memset(tc, 0, sizeof *tc);
        tc->command = COMMAND_RUN;
        tc->cursor = PR_CURSOR_UNDEFINED;
        memset(pc, 0, sizeof *pc);
        pc->command = COMMAND_RUN;
        pc->targets[0] = 0;
        pc->n_targets = 1;
        pc->skip = true;
        pc->cursor = PR_CURSOR_UNDEFINED;
        return true;
    }
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
    for (int i = 0; i < bt->n_unported; i++) if (bt->unported[i] < GC_MAX_UNPORTED) run->unported[bt->unported[i]]++;
    for (int i = 0; i < bt->n_deferred; i++) if (bt->deferred[i] < GC_MAX_UNPORTED) run->unported[bt->deferred[i]]++;
    bt->n_unported = bt->n_deferred = 0;
}

/* ---- the enemy AI (ai-core) ---- */

/* A new battle: currentBattle.enemySwitchCounter = 0, and every Pokemon's waveData is new
 * (resetWaveData at each encounter), so no ability is revealed yet */
void gc_ai_reset(gc_run *run)
{
    gc_internal *in = gc_int(run);
    memset(&in->ai, 0, sizeof in->ai);
    memset(&in->ai_trainer, 0, sizeof in->ai_trainer);
    in->ai.seed = run->run.seed;
}

/* The enemy's AI entries once its party is made: initialTeamIndex is the place in the party when it
 * was generated (run->enemy[] never moves). The AI type follows isBoss / hasTrainer (the default). */
void gc_ai_enemy_party(gc_run *run)
{
    gc_internal *in = gc_int(run);
    for (int i = 0; i < run->n_enemy; i++) {
        pr_ai_mon *m = pr_ai_mon_of(&in->ai, &run->enemy[i].b);
        if (m) { m->initial_team_index = (int8_t)i; m->trainer_slot = TRAINER_SLOT_NONE; }
    }
}

/* currentBattle.trainer for the AI's calls (NULL in a wild battle); ai.trainer goes back to NULL after
 * each call. Only the random trainers' classes are met for now (the fixed battles, where gym leaders,
 * the Elite Four, champions and evil team bosses are, fall back to wild ones): none is a boss and none
 * terastallizes, which is what these defaults say. Doubles are played as singles. */
static void ai_trainer_on(gc_run *run)
{
    gc_internal *in = gc_int(run);
    in->ai_trainer.is_boss = false;
    in->ai_trainer.is_double = false;
    in->ai_trainer.tera_mode = PR_TERA_AI_NO_TERA;
    in->ai_trainer.n_instant_teras = 0;
    in->ai.trainer = run->battle_type == BATTLE_TYPE_TRAINER ? &in->ai_trainer : NULL;
}

/* EnemyCommandPhase for the enemy in field slot 2 */
void gc_port_enemy_command(gc_run *run, int enemy_slot)
{
    gc_internal *in = gc_int(run);
    pr_battle *bt = &run->bt;
    pr_pokemon *p = bt->field[enemy_slot];
    if (!p || !p->on_field || p->hp <= 0) return;
    ai_trainer_on(run);
    /* Some move conditions the scores call read the phase queue (Protect fails unless a MovePhase is
     * queued) or add to it, as upstream's queue stands at command time: empty, with the turn about to
     * start. The engine is set up the way pr_run_turn starts it (the last turn's leftovers, such as the
     * tail a victory stop cut off, must not show) */
    memset(&run->eng, 0, sizeof run->eng);
    run->eng.n_levels = 1;
    run->eng.current = -1;
    for (int i = 0; i < 4; i++) run->eng.command_targets[i] = -1;
    bt->engine = &run->eng;
    pr_ai_enemy_command(bt, &in->ai, enemy_slot - BATTLER_INDEX_ENEMY);
    bt->engine = NULL;
    in->ai.trainer = NULL;
    /* a COMMAND_POKEMON the AI picked (a trainer's switch-out) is left in bt->commands: TurnStartPhase queues
     * the enemy's SwitchSummonPhase for it */
}

/* Trainer.getNextSummonIndex(trainerSlot), asked by battle-core's SwitchSummonPhase when its slot is -1
 * (the Pokemon FaintPhase sends for a trainer whose Pokemon fainted): ai-core's pick, the best matchup
 * against the player's field with a tie settled by a roll on the session seed and bt->turn. */
static int next_summon_cb(pr_battle *bt, int trainer_slot, void *ctx)
{
    gc_run *run = ctx;
    gc_internal *in = gc_int(run);
    ai_trainer_on(run);
    const int k = pr_ai_next_summon_index(bt, &in->ai, trainer_slot);
    in->ai.trainer = NULL;
    return k;
}

/* The one Pokemon of a side with this species and form, NULL when none or several have them */
static pr_pokemon *unique_mon(pr_battle *bt, int side, const pr_event_field *f)
{
    pr_pokemon *found = NULL;
    for (int k = 0; k < bt->n_party[side]; k++) {
        pr_pokemon *q = bt->party[side][k];
        if (!q || q->species != f->species || q->form != f->form) continue;
        if (found) return NULL;
        found = q;
    }
    return found;
}

static void reveal(gc_run *run, int side, const pr_event_field *f)
{
    if (!f->species) return;
    pr_pokemon *q = unique_mon(&run->bt, side, f);
    pr_ai_mon *m = q ? pr_ai_mon_of(&gc_int(run)->ai, q) : NULL;
    if (m) m->ability_revealed = true;
}

/* waveData.abilityRevealed from the turn's events: ShowAbilityPhase reveals the Pokemon whose bar it
 * shows, Frisk reveals its opponents, Trace the one it copies (in singles, the one opponent). battle-core
 * doesn't keep the flag, and its events say where a Pokemon stands, not which one it is: it is matched
 * by species and form, and left unrevealed (as upstream's default) when two of a side share them. */
static void note_revealed(gc_run *run)
{
    const pr_event *ev = run->eng.events;
    for (int i = 0; i < run->eng.n_events; i++) {
        if (ev[i].type != PR_EV_ABILITY_SHOW || ev[i].battler < 0 || ev[i].battler > 3) continue;
        const int side = ev[i].battler / 2;
        reveal(run, side, &ev[i].field[ev[i].battler]);
        if (ev[i].id == ABILITY_ID_FRISK || ev[i].id == ABILITY_ID_TRACE)
            for (int b = (1 - side) * 2; b < (1 - side) * 2 + 2; b++) reveal(run, 1 - side, &ev[i].field[b]);
    }
}

/* What the run keeps from a battle-core call (the turn, a switch pick, the rest of a turn, the start's answer) */
static int engine_returned(gc_run *run)
{
    note_unported(run);
    note_revealed(run);
    gc_items_from_battle(run);
    gc_ev_battle(run);
    gc_sync_party_order(run);
    run->run.rng = run->bt.rng;
    return run->eng.stop;
}

int gc_battle_run(gc_run *run, int pick)
{
    pr_battle *bt = &run->bt;
    if (pick < -1) {
        gc_int(run)->at_save_point = false;
        gc_event *ev = gc_ev_new(run, GC_EV_TURN);
        if (ev) ev->a = bt->turn;
        pr_run_turn(bt, &run->eng);
    } else {
        pr_resume_switch(bt, &run->eng, pick >= 0 ? gc_battle_slot_of(run, pick) : -1);
    }
    return engine_returned(run);
}

int gc_battle_continue(gc_run *run)
{
    gc_int(run)->at_save_point = false;
    pr_continue(&run->bt, &run->eng);
    return engine_returned(run);
}

int gc_battle_answer_switch(gc_run *run, bool yes)
{
    pr_answer_check_switch(&run->bt, &run->eng, yes);
    return engine_returned(run);
}

void gc_battle_switch_screen(gc_run *run)
{
    gc_screen *s = &run->screen;
    memset(s, 0, sizeof *s);
    s->kind = GC_SCREEN_SWITCH;
    s->slot = run->eng.stop_phase.sw_field_index;
    /* the Pokemon being switched out (a move's switch-out: Teleport, U-turn) is still in its party slot, and
     * upstream's party screen doesn't let a Pokemon be sent into the slot it already holds. It's off the
     * field by now, so on_field doesn't exclude it: picking it left switch_out_status set and the battle
     * never went on. */
    const pr_pokemon *leaving = s->slot >= 0 && s->slot < 2 ? run->bt.field[s->slot] : NULL;
    bool any = false;
    for (int i = 0; i < run->n_party; i++) {
        s->pick_ok[i] = gc_mon_allowed(&run->party[i]) && !run->party[i].b.on_field && &run->party[i].b != leaving;
        any |= s->pick_ok[i];
    }
    /* a modal SwitchPhase (after a faint) must be answered; a move's switch-out can be left empty */
    s->can_skip = !run->eng.stop_phase.sw_modal || !any;
}

/* BattleEndPhase's part: the battle is over. The player's Pokemon stay where they are: the next
 * battle's setup recalls them or keeps the lead out (see gc_battle_setup_field). */
void gc_battle_end_cleanup(gc_run *run)
{
    gc_int(run)->at_save_point = false;
    run->run.rng = run->bt.rng;
    gc_sync_party_order(run);
    for (int i = 0; i < run->n_party; i++) pr_reset_turn_data(&run->party[i].b);
    run->bt.field[2] = run->bt.field[3] = NULL;
    run->bt.n_party[1] = 0;
    run->in_battle = false;
}
