/* The phase engine: PhaseTree (src/phase-tree.ts), the PhaseManager's queueing
 * (src/phase-manager.ts), the DynamicQueueManager and its speed-sorted queues
 * (src/dynamic-queue-manager.ts, src/queues/, src/utils/speed-order.ts).
 * Each function names the upstream method it ports; quirks are kept on purpose. */
#include "pr/engine.h"
#include <stdio.h>
#include <string.h>

const char *const pr_phase_name[PH_COUNT_] = {
    [PH_NONE] = "?", [PH_MARKER] = "DynamicPhaseMarker",
    [PH_TURN_START] = "TurnStartPhase", [PH_MOVE] = "MovePhase", [PH_MOVE_EFFECT] = "MoveEffectPhase",
    [PH_MOVE_END] = "MoveEndPhase", [PH_FAINT] = "FaintPhase", [PH_CHECK_INTERLUDE] = "CheckInterludePhase",
    [PH_WEATHER_EFFECT] = "WeatherEffectPhase", [PH_POSITIONAL_TAG] = "PositionalTagPhase",
    [PH_BERRY] = "BerryPhase", [PH_CHECK_STATUS_EFFECT] = "CheckStatusEffectPhase",
    [PH_POST_TURN_STATUS_EFFECT] = "PostTurnStatusEffectPhase", [PH_TURN_END] = "TurnEndPhase", [PH_RESET_STATUS] = "ResetStatusPhase",
    [PH_MESSAGE] = "MessagePhase", [PH_DAMAGE_ANIM] = "DamageAnimPhase", [PH_COMMON_ANIM] = "CommonAnimPhase",
    [PH_SHOW_ABILITY] = "ShowAbilityPhase", [PH_HIDE_ABILITY] = "HideAbilityPhase",
    [PH_TURN_INIT] = "TurnInitPhase", [PH_VICTORY] = "VictoryPhase", [PH_SWITCH] = "SwitchPhase",
    [PH_SWITCH_SUMMON] = "SwitchSummonPhase", [PH_GAME_OVER] = "GameOverPhase", [PH_BATTLE_END] = "BattleEndPhase",
    [PH_SELECT_MODIFIER] = "SelectModifierPhase",
    [PH_STAT_STAGE_CHANGE] = "StatStageChangePhase", [PH_POKEMON_HEAL] = "PokemonHealPhase",
    [PH_OBTAIN_STATUS_EFFECT] = "ObtainStatusEffectPhase", [PH_MOVE_CHARGE] = "MoveChargePhase",
    [PH_MOVE_HEADER] = "MoveHeaderPhase", [PH_MOVE_REFLECT] = "MoveReflectPhase", [PH_DANCER] = "DancerPhase",
    [PH_TERA] = "TeraPhase", [PH_ATTEMPT_CAPTURE] = "AttemptCapturePhase", [PH_ATTEMPT_RUN] = "AttemptRunPhase",
    [PH_TOGGLE_DOUBLE_POSITION] = "ToggleDoublePositionPhase",
};

const char *pr_phase_name_of(int t)
{
    return t >= 0 && t < PH_COUNT_ && pr_phase_name[t] ? pr_phase_name[t] : "?";
}

/* DynamicQueueManager.isDynamicPhase: phases with a getPokemon method, minus the
 * nonDynamicPokemonPhases list */
bool pr_phase_is_dynamic(pr_phase_type t)
{
    switch (t) {
    case PH_MOVE: case PH_COMMON_ANIM: case PH_STAT_STAGE_CHANGE: case PH_OBTAIN_STATUS_EFFECT:
    case PH_POST_TURN_STATUS_EFFECT: case PH_MOVE_CHARGE: case PH_MOVE_HEADER: case PH_SWITCH_SUMMON:
    case PH_MOVE_REFLECT: case PH_DANCER: case PH_TERA:
        return true;
    default:
        /* not dynamic: MoveEffect, MoveEnd, Faint, DamageAnim, PokemonHeal, WeatherEffect,
         * ShowAbility, HideAbility, Victory (listed upstream) and phases without getPokemon */
        return false;
    }
}

static void limit(pr_battle *bt, pr_engine *e)
{
    e->overflow = true;
    pr_note_unported(bt, PR_FEAT_ENGINE_LIMIT);
}

int pr_phase_new(pr_engine *e, pr_phase_type type, int8_t battler)
{
    for (int i = 0; i < PR_MAX_PHASES; i++)
        if (!e->used[i]) {
            e->used[i] = true;
            pr_phase *p = &e->pool[i];
            memset(p, 0, sizeof *p);
            p->type = (uint8_t)type;
            p->battler = battler;
            p->targets = -1;
            p->source = -1;
            p->move.slot = -1;
            p->timing = MOVE_PHASE_TIMING_MODIFIER_NORMAL;
            p->seq = e->next_seq++;
            return i;
        }
    e->overflow = true;
    return -1;
}

static void phase_free(pr_engine *e, int ph)
{
    if (ph >= 0) e->used[ph] = false;
}

/* ---------------------------------------------------------------- PhaseTree */

/* PhaseTree.add(phase, level) */
static void tree_add(pr_battle *bt, pr_engine *e, int ph, int level)
{
    if (level == e->current_level + 1 && level == e->n_levels) {
        if (e->n_levels >= PR_MAX_LEVELS) { limit(bt, e); return; }
        e->level_len[e->n_levels++] = 0;
    }
    if (level < 0 || level >= e->n_levels) {
        /* upstream throws "Attempted to add a phase to a nonexistent level" */
        limit(bt, e);
        return;
    }
    if (e->level_len[level] >= PR_LEVEL_CAP) { limit(bt, e); return; }
    e->levels[level][e->level_len[level]++] = (uint8_t)ph;
}

static void level_insert(pr_battle *bt, pr_engine *e, int level, int at, int ph)
{
    if (e->level_len[level] >= PR_LEVEL_CAP) { limit(bt, e); return; }
    memmove(&e->levels[level][at + 1], &e->levels[level][at], (size_t)(e->level_len[level] - at));
    e->levels[level][at] = (uint8_t)ph;
    e->level_len[level]++;
}

/* PhaseTree.addPhase(phase, defer): deferral moves the current level up one and puts a new
 * level under it (splice(-1, 0, [])); the phase then goes to currentLevel + 1 - defer */
static void tree_add_phase(pr_battle *bt, pr_engine *e, int ph, bool defer)
{
    if (defer && !e->deferred_active) {
        if (e->n_levels >= PR_MAX_LEVELS) { limit(bt, e); return; }
        e->deferred_active = true;
        int at = e->n_levels - 1;
        memmove(&e->levels[at + 1], &e->levels[at], sizeof e->levels[0]);
        e->level_len[at + 1] = e->level_len[at];
        e->level_len[at] = 0;
        e->n_levels++;
        e->current_level += 1;
    }
    tree_add(bt, e, ph, e->current_level + 1 - (defer ? 1 : 0));
}

static bool phase_is(pr_engine *e, int ph, pr_phase_type t)
{
    return e->pool[ph].type == t;
}

/* PhaseTree.addAfter(phase, type): after the first `type` phase, searching from the top level */
static void tree_add_after(pr_battle *bt, pr_engine *e, int ph, pr_phase_type type)
{
    for (int i = e->n_levels - 1; i >= 0; i--)
        for (int k = 0; k < e->level_len[i]; k++)
            if (phase_is(e, e->levels[i][k], type)) {
                level_insert(bt, e, i, k + 1, ph);
                return;
            }
    tree_add_phase(bt, e, ph, false);
}

/* PhaseTree.getNextPhase: drop empty levels from the top, shift from the new top */
static int tree_next(pr_engine *e)
{
    e->current_level = e->n_levels - 1;
    while (e->current_level > 0 && e->level_len[e->current_level] == 0) {
        e->deferred_active = false;
        e->n_levels--;
        e->current_level--;
    }
    if (e->level_len[e->current_level] == 0) return -1;
    int ph = e->levels[e->current_level][0];
    memmove(&e->levels[e->current_level][0], &e->levels[e->current_level][1], (size_t)(e->level_len[e->current_level] - 1));
    e->level_len[e->current_level]--;
    return ph;
}

/* ---------------------------------------------------------------- speed order */

/* utils/speed-order.ts sortInSpeedOrder: group consecutive entries of one Pokemon, shuffle the
 * groups (seeded with the turn and the group count, on a throwaway RNG state), stable-sort them
 * by effective Speed, reverse the lot under Trick Room */
void pr_sort_in_speed_order(pr_battle *bt, pr_pokemon **mons, int *payload, int n)
{
    if (n <= 0) return;
    int start[PR_DYN_CAP + 1], ng = 0;
    for (int i = 0; i < n; i++)
        if (i == 0 || mons[i] != mons[i - 1]) start[ng++] = i;
    start[ng] = n;
    int order[PR_DYN_CAP];
    for (int g = 0; g < ng; g++) order[g] = g;

    /* shufflePokemonList: executeWithSeedOffset(randSeedShuffle, turn * 1000 + length, waveSeed) */
    if (bt->wave_seed.len == 0) pr_note_unported(bt, PR_FEAT_WAVE);
    pr_rng r;
    pr_seedstr s;
    pr_seed_shift(&s, &bt->wave_seed, bt->turn * 1000 + ng);
    pr_rng_sow(&r, &s);
    for (int i = ng - 1; i > 0; i--) {
        int j = pr_rng_integer_in_range(&r, 0, i);
        int t = order[i]; order[i] = order[j]; order[j] = t;
    }

    /* sortBySpeed: (turnCommandManager.setOrder is a test hook, never set here) */
    int32_t spd[PR_DYN_CAP];
    for (int g = 0; g < ng; g++) spd[g] = pr_get_effective_stat(bt, mons[start[order[g]]], STAT_SPD, NULL, -1, false, false);
    for (int i = 1; i < ng; i++) {           /* stable, like Array.prototype.sort */
        int ko = order[i];
        int32_t ks = spd[i];
        int j = i - 1;
        while (j >= 0 && ks - spd[j] > 0) {  /* comparator bSpeed - aSpeed > 0: key goes first */
            order[j + 1] = order[j];
            spd[j + 1] = spd[j];
            j--;
        }
        order[j + 1] = ko;
        spd[j + 1] = ks;
    }
    /* arena.applyTags(TRICK_ROOM, speedReversed): each Trick Room tag flips it */
    bool reversed = false;
    for (int i = 0; i < bt->n_arena_tags; i++)
        if (bt->arena_tags[i].type == ARENA_TAG_TYPE_TRICK_ROOM) reversed = !reversed;
    if (reversed)
        for (int a = 0, b = ng - 1; a < b; a++, b--) { int t = order[a]; order[a] = order[b]; order[b] = t; }

    pr_pokemon *m2[PR_DYN_CAP];
    int p2[PR_DYN_CAP], k = 0;
    for (int g = 0; g < ng; g++)
        for (int i = start[order[g]]; i < start[order[g] + 1]; i++) {
            m2[k] = mons[i];
            if (payload) p2[k] = payload[i];
            k++;
        }
    memcpy(mons, m2, sizeof(pr_pokemon *) * (size_t)n);
    if (payload) memcpy(payload, p2, sizeof(int) * (size_t)n);
}

/* globalScene.getField(true) filtered to a side, in field order */
static int side_field(pr_battle *bt, int side, pr_pokemon **out)
{
    int n = 0;
    for (int i = 0; i < 4; i++) {
        pr_pokemon *p = bt->field[i];
        if (!p || p->hp <= 0) continue;          /* isActive(): not fainted */
        if (side == ARENA_TAG_SIDE_PLAYER && i >= 2) continue;
        if (side == ARENA_TAG_SIDE_ENEMY && i < 2) continue;
        out[n++] = p;
    }
    return n;
}

/* utils/speed-order-generator.ts inSpeedOrder(side): a PokemonPriorityQueue popped one at a
 * time; every pop re-sorts what is left (and the shuffle seed follows its size). The list is
 * taken when iteration starts. The loop bodies upstream run between pops; callers here get
 * the whole order at once, which is the same as long as the body doesn't change Speed or
 * faint a Pokemon (callers that can do that use pr_speed_iter). */
void pr_speed_iter_init(pr_battle *bt, pr_speed_iter *it, int side)
{
    it->n = side_field(bt, side, it->queue);
}

pr_pokemon *pr_speed_iter_next(pr_battle *bt, pr_speed_iter *it)
{
    if (it->n == 0) return NULL;
    pr_sort_in_speed_order(bt, it->queue, NULL, it->n);
    pr_pokemon *p = it->queue[0];
    memmove(&it->queue[0], &it->queue[1], sizeof(pr_pokemon *) * (size_t)(it->n - 1));
    it->n--;
    return p;
}

/* ---------------------------------------------------------------- dynamic queues */

static pr_pokemon *phase_pokemon(pr_battle *bt, pr_engine *e, int ph)
{
    /* PokemonPhase.getPokemon(): the Pokemon in the phase's field slot */
    int b = e->pool[ph].battler;
    return b >= 0 && b < 4 ? bt->field[b] : NULL;
}

/* Move.getPriorityModifier(user, simulated = true) */
static int priority_modifier(pr_battle *bt, int move, const pr_pokemon *user)
{
    if (pr_get_battler_tag(user, BATTLER_TAG_TYPE_BYPASS_SPEED)) return MOVE_PRIORITY_IN_BRACKET_FIRST;
    pr_hook_ab(bt, user, PR_AC_ChangeMovePriorityInBracketAbAttr);
    return MOVE_PRIORITY_IN_BRACKET_NORMAL;
}

/* PriorityQueue.reorder for the queue of `type` */
static void dyn_reorder(pr_battle *bt, pr_engine *e, int type)
{
    int n = e->n_dyn[type];
    if (n == 0) return;
    pr_pokemon *mons[PR_DYN_CAP];
    int pay[PR_DYN_CAP];
    for (int i = 0; i < n; i++) {
        pay[i] = e->dyn[type][i];
        mons[i] = phase_pokemon(bt, e, pay[i]);
    }
    pr_sort_in_speed_order(bt, mons, pay, n);
    if (type == PH_MOVE) {
        /* MovePhasePriorityQueue.sortPostSpeed: timing modifier, move priority, in-bracket
         * priority (stable sort, each key descending) */
        int key[PR_DYN_CAP][3];
        for (int i = 0; i < n; i++) {
            pr_phase *p = &e->pool[pay[i]];
            key[i][0] = p->timing;
            key[i][1] = pr_move_get_priority(bt, p->move.id, p->pokemon);
            key[i][2] = priority_modifier(bt, p->move.id, p->pokemon);
        }
        for (int i = 1; i < n; i++) {
            int kp = pay[i], kk[3] = {key[i][0], key[i][1], key[i][2]};
            int j = i - 1;
            for (; j >= 0; j--) {
                int c = kk[0] != key[j][0] ? kk[0] - key[j][0]
                      : kk[1] != key[j][1] ? kk[1] - key[j][1] : kk[2] - key[j][2];
                if (c <= 0) break;
                pay[j + 1] = pay[j];
                memcpy(key[j + 1], key[j], sizeof key[j]);
            }
            pay[j + 1] = kp;
            memcpy(key[j + 1], kk, sizeof kk);
        }
    }
    for (int i = 0; i < n; i++) e->dyn[type][i] = (uint8_t)pay[i];
}

/* DynamicQueueManager.popNextPhase(type) */
static int dyn_pop(pr_battle *bt, pr_engine *e, int type)
{
    if (e->n_dyn[type] == 0) return -1;
    dyn_reorder(bt, e, type);
    int ph = e->dyn[type][0];
    memmove(&e->dyn[type][0], &e->dyn[type][1], (size_t)(e->n_dyn[type] - 1));
    e->n_dyn[type]--;
    if (type == PH_MOVE) {
        /* MovePhasePriorityQueue.pop records the turn order */
        if (bt->n_last_turn_order < PR_MAX_TURN_ORDER) bt->last_turn_order[bt->n_last_turn_order] = e->pool[ph].pokemon->id;
        if (bt->n_last_turn_order < UINT8_MAX) bt->n_last_turn_order++;
        pr_sync_queued_movers(bt, e);
    }
    return ph;
}

/* phaseManager.hasPhaseOfType("MovePhase") as the damage path reads it (Analytic) */
void pr_sync_queued_movers(pr_battle *bt, pr_engine *e)
{
    bt->n_queued_movers = 0;
    for (int i = 0; i < e->n_dyn[PH_MOVE] && i < PR_MAX_QUEUED_MOVERS; i++)
        bt->queued_movers[bt->n_queued_movers++] = e->pool[e->dyn[PH_MOVE][i]].pokemon->id;
}

/* PhaseManager.checkDynamic: a dynamic phase goes to its queue and a marker takes its place */
static int check_dynamic(pr_battle *bt, pr_engine *e, int ph)
{
    int t = e->pool[ph].type;
    if (!pr_phase_is_dynamic((pr_phase_type)t)) return ph;
    if (e->n_dyn[t] >= PR_DYN_CAP) { limit(bt, e); return ph; }
    e->dyn[t][e->n_dyn[t]++] = (uint8_t)ph;
    if (t == PH_MOVE) pr_sync_queued_movers(bt, e);
    int m = pr_phase_new(e, PH_MARKER, -1);
    if (m < 0) { limit(bt, e); return ph; }
    e->pool[m].marker_of = (uint8_t)t;
    return m;
}

static void note_unported_phase(pr_battle *bt, pr_engine *e, int ph)
{
    int t = e->pool[ph].type;
    if (t >= PH_FIRST_UNPORTED) pr_note_unported(bt, (uint16_t)(PR_FEAT_PHASE_0 + t));
}

/* PhaseManager.pushPhase */
void pr_phase_push(pr_battle *bt, pr_engine *e, int ph)
{
    if (ph < 0) { limit(bt, e); return; }
    note_unported_phase(bt, e, ph);
    tree_add(bt, e, check_dynamic(bt, e, ph), 0);
}

/* PhaseManager.unshiftPhase: MovePhase and DancerPhase go after the first MoveEndPhase */
void pr_phase_unshift(pr_battle *bt, pr_engine *e, int ph)
{
    if (ph < 0) { limit(bt, e); return; }
    note_unported_phase(bt, e, ph);
    int t = e->pool[ph].type;
    int add = check_dynamic(bt, e, ph);
    if (t == PH_MOVE || t == PH_DANCER) tree_add_after(bt, e, add, PH_MOVE_END);
    else tree_add_phase(bt, e, add, false);
}

/* PhaseTree.addPhase(phase, true) (queueFaintPhase, queueDeferred): no dynamic check */
void pr_phase_add_deferred(pr_battle *bt, pr_engine *e, int ph)
{
    if (ph < 0) { limit(bt, e); return; }
    note_unported_phase(bt, e, ph);
    tree_add_phase(bt, e, ph, true);
}

bool pr_phase_exists(pr_engine *e, pr_phase_type t, int battler)
{
    for (int i = 0; i < e->n_dyn[t]; i++)
        if (battler < 0 || e->pool[e->dyn[t][i]].battler == battler) return true;
    for (int l = 0; l < e->n_levels; l++)
        for (int k = 0; k < e->level_len[l]; k++) {
            pr_phase *p = &e->pool[e->levels[l][k]];
            if (p->type == t && (battler < 0 || p->battler == battler)) return true;
        }
    return false;
}

/* phaseManager.tryRemovePhase(type, phase => phase.getPokemon() === <battler>): the tree part
 * (PhaseTree.remove: the first match, searching from the top level). Dynamic types would go
 * through their priority queue first; none is removed this way yet. */
bool pr_phase_try_remove(pr_battle *bt, pr_engine *e, pr_phase_type t, int battler)
{
    if (pr_phase_is_dynamic(t)) { pr_note_unported(bt, (uint16_t)(PR_FEAT_PHASE_0 + t)); return false; }
    for (int l = e->n_levels - 1; l >= 0; l--)
        for (int k = 0; k < e->level_len[l]; k++) {
            int ph = e->levels[l][k];
            if (!phase_is(e, ph, t) || (battler >= 0 && e->pool[ph].battler != battler)) continue;
            memmove(&e->levels[l][k], &e->levels[l][k + 1], (size_t)(e->level_len[l] - k - 1));
            e->level_len[l]--;
            return true;
        }
    return false;
}

void pr_queue_message(pr_battle *bt, pr_engine *e)
{
    pr_phase_unshift(bt, e, pr_phase_new(e, PH_MESSAGE, -1));
}

void pr_queue_message_deferred(pr_battle *bt, pr_engine *e)
{
    pr_phase_push(bt, e, pr_phase_new(e, PH_MESSAGE, -1));
}

void pr_queue_ability_display(pr_battle *bt, pr_engine *e, const pr_pokemon *p, bool show)
{
    int b = pr_battler_index(bt, p);
    pr_phase_unshift(bt, e, pr_phase_new(e, show ? PH_SHOW_ABILITY : PH_HIDE_ABILITY, (int8_t)(show ? b : -1)));
}

void pr_queue_faint(pr_battle *bt, pr_engine *e, int battler, bool prevent_instant_revive, int source)
{
    int ph = pr_phase_new(e, PH_FAINT, (int8_t)battler);
    if (ph < 0) { limit(bt, e); return; }
    e->pool[ph].prevent_instant_revive = prevent_instant_revive;
    e->pool[ph].source = (int8_t)source;
    pr_phase_add_deferred(bt, e, ph);
}

void pr_targets_changed(pr_battle *bt, pr_engine *e, int list)
{
    if (list < 0) return;
    const pr_target_list *t = &e->target_lists[list];
    for (int i = 0; i < 4; i++)
        if (e->command_targets[i] == list) {
            memcpy(bt->commands[i].targets, t->v, 4);
            bt->commands[i].n_targets = t->n;
        }
    for (int k = 0; k < e->n_hist_alias; k++) {
        if (e->hist_alias[k].list != list) continue;
        pr_pokemon *p = e->hist_alias[k].p;
        int kept = p->n_move_history < PR_MAX_MOVE_HISTORY ? p->n_move_history : PR_MAX_MOVE_HISTORY;
        int at = (int)e->hist_alias[k].abs - (p->n_move_history - kept);
        if (at < 0 || at >= kept) continue;     /* fell out of the kept entries */
        memcpy(p->move_history[at].targets, t->v, 4);
        p->move_history[at].n_targets = t->n;
    }
}

void pr_push_history_shared(pr_battle *bt, pr_engine *e, pr_pokemon *p, pr_turn_move tm, int list)
{
    /* pushMoveHistory: only while the Pokemon is on the field */
    if (!p->on_field || p->switch_out_status) return;
    if (list >= 0) {
        memcpy(tm.targets, e->target_lists[list].v, 4);
        tm.n_targets = e->target_lists[list].n;
        if (e->n_hist_alias < (int)(sizeof e->hist_alias / sizeof e->hist_alias[0])) {
            e->hist_alias[e->n_hist_alias].p = p;
            e->hist_alias[e->n_hist_alias].abs = p->n_move_history;
            e->hist_alias[e->n_hist_alias].list = (int8_t)list;
            e->n_hist_alias++;
        } else {
            limit(bt, e);
        }
    }
    pr_push_move_history(p, tm);
}

int pr_targets_new(pr_engine *e, const int8_t *v, int n)
{
    if (e->n_target_lists >= PR_MAX_TARGET_LISTS || n > 4) { e->overflow = true; return -1; }
    pr_target_list *t = &e->target_lists[e->n_target_lists];
    memcpy(t->v, v, (size_t)n);
    t->n = (uint8_t)n;
    return e->n_target_lists++;
}

/* ---------------------------------------------------------------- the loop */

static bool is_stop(int type)
{
    switch (type) {
    case PH_TURN_INIT: case PH_VICTORY: case PH_SWITCH: case PH_SWITCH_SUMMON: case PH_GAME_OVER:
    case PH_BATTLE_END: case PH_SELECT_MODIFIER:
        return true;
    }
    return false;
}

/* PhaseManager.shiftPhase: the next phase, a dynamic one in place of its marker; an empty
 * tree (or a marker whose queue is empty) starts the next turn */
static int shift_phase(pr_battle *bt, pr_engine *e)
{
    int ph = tree_next(e);
    if (ph >= 0 && e->pool[ph].type == PH_MARKER) {
        int t = e->pool[ph].marker_of;
        phase_free(e, ph);
        ph = dyn_pop(bt, e, t);
    }
    if (ph < 0) {
        /* turnStart(): clear the dynamic queues, a new TurnInitPhase */
        for (int t = 0; t < PH_COUNT_; t++) {
            for (int i = 0; i < e->n_dyn[t]; i++) phase_free(e, e->dyn[t][i]);
            e->n_dyn[t] = 0;
        }
        bt->n_last_turn_order = 0;      /* MovePhasePriorityQueue.clear() also clears the order */
        pr_sync_queued_movers(bt, e);
        ph = pr_phase_new(e, PH_TURN_INIT, -1);
    }
    return ph;
}

static void trace(pr_engine *e, int ph)
{
    if (e->n_trace >= PR_MAX_TRACE) { e->overflow = true; return; }
    pr_phase *p = &e->pool[ph];
    char *out = e->trace[e->n_trace++];
    /* upstream: `${phaseName}:${battlerIndex}` for phases with getPokemon and a numeric battlerIndex */
    bool pokemon_phase = p->type == PH_MOVE || p->type == PH_MOVE_EFFECT || p->type == PH_MOVE_END
                         || p->type == PH_FAINT || p->type == PH_DAMAGE_ANIM || p->type == PH_COMMON_ANIM
                         || p->type == PH_WEATHER_EFFECT || p->type == PH_SHOW_ABILITY
                         || p->type == PH_STAT_STAGE_CHANGE || p->type == PH_OBTAIN_STATUS_EFFECT
                         || p->type == PH_POKEMON_HEAL || p->type == PH_POST_TURN_STATUS_EFFECT
                         || p->type == PH_MOVE_CHARGE;
    pokemon_phase = pokemon_phase || p->type == PH_MOVE_REFLECT;
    if (pokemon_phase && p->battler >= 0) snprintf(out, 40, "%s:%d", pr_phase_name[p->type], p->battler);
    else snprintf(out, 40, "%s", pr_phase_name[p->type]);
}

void pr_run_turn(pr_battle *bt, pr_engine *e)
{
    memset(e, 0, sizeof *e);
    e->n_levels = 1;
    e->current = -1;
    for (int i = 0; i < 4; i++) e->command_targets[i] = -1;
    bt->engine = e;
    /* the caller's pushNew("TurnStartPhase") + shiftPhase() */
    pr_phase_push(bt, e, pr_phase_new(e, PH_TURN_START, -1));
    int ph = shift_phase(bt, e);
    for (;;) {
        if (ph < 0 || e->overflow) { pr_note_unported(bt, PR_FEAT_ENGINE_LIMIT); break; }
        if (is_stop(e->pool[ph].type)) {
            e->stop = e->pool[ph].type;
            phase_free(e, ph);
            break;
        }
        trace(e, ph);
        e->current = ph;
        pr_phase_run(bt, e, ph);
        e->current = -1;
        phase_free(e, ph);
        if (e->n_trace >= PR_MAX_TRACE) { e->overflow = true; continue; }
        ph = shift_phase(bt, e);
    }
    bt->engine = NULL;
}
