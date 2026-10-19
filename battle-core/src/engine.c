/* The phase engine: PhaseTree (src/phase-tree.ts), the PhaseManager's queueing
 * (src/phase-manager.ts), the DynamicQueueManager and its speed-sorted queues
 * (src/dynamic-queue-manager.ts, src/queues/, src/utils/speed-order.ts).
 * Each function names the upstream method it ports; quirks are kept on purpose. */
#include "pr/engine.h"
#include <stdio.h>
#include <string.h>

const char *const pr_phase_name[PH_COUNT_] = {
    [PH_QUIET_FORM_CHANGE] = "QuietFormChangePhase",
    [PH_NONE] = "?", [PH_MARKER] = "DynamicPhaseMarker",
    [PH_TURN_START] = "TurnStartPhase", [PH_MOVE] = "MovePhase", [PH_MOVE_EFFECT] = "MoveEffectPhase",
    [PH_MOVE_END] = "MoveEndPhase", [PH_FAINT] = "FaintPhase", [PH_CHECK_INTERLUDE] = "CheckInterludePhase",
    [PH_WEATHER_EFFECT] = "WeatherEffectPhase", [PH_POSITIONAL_TAG] = "PositionalTagPhase",
    [PH_BERRY] = "BerryPhase", [PH_CHECK_STATUS_EFFECT] = "CheckStatusEffectPhase",
    [PH_POST_TURN_STATUS_EFFECT] = "PostTurnStatusEffectPhase", [PH_TURN_END] = "TurnEndPhase", [PH_RESET_STATUS] = "ResetStatusPhase",
    [PH_MESSAGE] = "MessagePhase", [PH_DAMAGE_ANIM] = "DamageAnimPhase", [PH_COMMON_ANIM] = "CommonAnimPhase",
    [PH_SHOW_ABILITY] = "ShowAbilityPhase", [PH_HIDE_ABILITY] = "HideAbilityPhase", [PH_POKEMON_ANIM] = "PokemonAnimPhase",
    [PH_LOAD_MOVE_ANIM] = "LoadMoveAnimPhase", [PH_MOVE_ANIM] = "MoveAnimPhase",
    [PH_TURN_INIT] = "TurnInitPhase", [PH_VICTORY] = "VictoryPhase", [PH_SWITCH] = "SwitchPhase",
    [PH_SWITCH_SUMMON] = "SwitchSummonPhase", [PH_POST_SUMMON] = "PostSummonPhase",
    [PH_SHINY_SPARKLE] = "ShinySparklePhase", [PH_GAME_OVER] = "GameOverPhase", [PH_BATTLE_END] = "BattleEndPhase",
    [PH_SELECT_MODIFIER] = "SelectModifierPhase",
    [PH_STAT_STAGE_CHANGE] = "StatStageChangePhase", [PH_POKEMON_HEAL] = "PokemonHealPhase",
    [PH_OBTAIN_STATUS_EFFECT] = "ObtainStatusEffectPhase", [PH_MOVE_CHARGE] = "MoveChargePhase",
    [PH_MOVE_HEADER] = "MoveHeaderPhase", [PH_MOVE_REFLECT] = "MoveReflectPhase", [PH_DANCER] = "DancerPhase",
    [PH_TERA] = "TeraPhase", [PH_ATTEMPT_CAPTURE] = "AttemptCapturePhase", [PH_ATTEMPT_RUN] = "AttemptRunPhase",
    [PH_TOGGLE_DOUBLE_POSITION] = "ToggleDoublePositionPhase", [PH_SELECT_BIOME] = "SelectBiomePhase",
    [PH_SUMMON] = "SummonPhase", [PH_INIT_ENCOUNTER] = "InitEncounterPhase", [PH_CHECK_SWITCH] = "CheckSwitchPhase",
    [PH_NEW_BATTLE] = "NewBattlePhase",
    [PH_TRANSFORM] = "PokemonTransformPhase", [PH_RETURN] = "ReturnPhase", [PH_ILLUSION_BREAK] = "IllusionBreakPhase",
    [PH_REVIVAL_BLESSING] = "RevivalBlessingPhase",
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
    case PH_POST_TURN_STATUS_EFFECT: case PH_MOVE_CHARGE: case PH_MOVE_HEADER: case PH_SWITCH_SUMMON: case PH_RETURN:
    case PH_POST_SUMMON: case PH_SHINY_SPARKLE:
    case PH_MOVE_REFLECT: case PH_DANCER: case PH_TERA: case PH_ATTEMPT_CAPTURE: case PH_TRANSFORM:
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
            p->user_id = -1;
            p->targets = -1;
            p->entry_targets = -2;
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
    /* Native Array.sort never calls its Speed comparator for a single run.
     * This also permits consecutive phases whose getPokemon() is undefined. */
    if (ng == 1) return;
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
    double spd[PR_DYN_CAP];
    for (int g = 0; g < ng; g++) spd[g] = pr_get_effective_stat(bt, mons[start[order[g]]], STAT_SPD, NULL, -1, false, false);
    for (int i = 1; i < ng; i++) {           /* stable, like Array.prototype.sort */
        int ko = order[i];
        double ks = spd[i];
        int j = i - 1;
        while (j >= 0 && ks > spd[j]) {  /* comparator bSpeed - aSpeed > 0: key goes first */
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
    const pr_phase *p = &e->pool[ph];
    if (p->type == PH_SWITCH_SUMMON || p->type == PH_RETURN) {
        /* PartyMemberPokemonPhase.getPokemon(): the party member in slot fieldIndex */
        int side = p->sw_player ? 0 : 1;
        return p->sw_field_index >= 0 && p->sw_field_index < bt->n_party[side] ? bt->party[side][p->sw_field_index] : NULL;
    }
    /* PokemonPhase.getPokemon(): the Pokemon in the phase's field slot now (MovePhase too) */
    int b = p->battler;
    return b >= 0 && b < 4 ? bt->field[b] : NULL;
}

/* Move.getPriorityModifier(user, simulated = true) */
int pr_move_get_priority_modifier(pr_battle *bt, int move, const pr_pokemon *user)
{
    if (pr_get_battler_tag(user, BATTLER_TAG_TYPE_BYPASS_SPEED)) return MOVE_PRIORITY_IN_BRACKET_FIRST;
    double modifier = MOVE_PRIORITY_IN_BRACKET_NORMAL;
    pr_ab_params params = {.pokemon = user, .move = move, .simulated = true, .value = &modifier};
    pr_apply_ab_attrs(bt, PR_AC_ChangeMovePriorityInBracketAbAttr, &params);
    return (int)modifier;
}

/* PriorityQueue.reorder for the queue of `type` */
static void dyn_reorder(pr_battle *bt, pr_engine *e, int type)
{
    int n = e->n_dyn[type];
    if (n == 0) return;
    pr_pokemon *mons[PR_DYN_CAP];
    int pay[PR_DYN_CAP];
    bool missing = false, present = false;
    int missing_kind = -1;
    for (int i = 0; i < n; i++) {
        pay[i] = e->dyn[type][i];
        mons[i] = phase_pokemon(bt, e, pay[i]);
        missing |= mons[i] == NULL;
        present |= mons[i] != NULL;
        if (!mons[i]) {
            const pr_phase *p = &e->pool[pay[i]];
            /* getField's empty slots are null; an invalid index (or absent
             * party member) is undefined. Native grouping distinguishes them. */
            int kind = p->type != PH_SWITCH_SUMMON && p->type != PH_RETURN
                && p->battler >= 0 && p->battler < 4;
            if (missing_kind >= 0 && missing_kind != kind) {
                pr_note_unported(bt, PR_FEAT_ENGINE_LIMIT);
                return;
            }
            missing_kind = kind;
        }
    }
    /* A single undefined group sorts without a comparison. Mixing it with a
     * real Pokemon would make the native Speed comparator throw. */
    if (missing && present) { pr_note_unported(bt, PR_FEAT_ENGINE_LIMIT); return; }
    pr_sort_in_speed_order(bt, mons, pay, n);
    if (type == PH_POST_SUMMON) {
        /* PostSummonPhasePriorityQueue.reorder: then by getPriority() descending (a stable sort):
         * 0 for a PostSummonPhase, the ability's postSummonPriority for an activate phase */
        for (int i = 1; i < n; i++) {
            int kp = pay[i], kk = e->pool[kp].ps_priority;
            int j = i - 1;
            for (; j >= 0 && e->pool[pay[j]].ps_priority < kk; j--) pay[j + 1] = pay[j];
            pay[j + 1] = kp;
        }
    }
    if (type == PH_MOVE) {
        /* MovePhasePriorityQueue.sortPostSpeed: timing modifier, move priority, in-bracket
         * priority (stable sort, each key descending) */
        int key[PR_DYN_CAP][3];
        for (int i = 0; i < n; i++) {
            pr_phase *p = &e->pool[pay[i]];
            key[i][0] = p->timing;
            key[i][1] = pr_move_get_priority(bt, p->move.id, p->pokemon);
            key[i][2] = pr_move_get_priority_modifier(bt, p->move.id, p->pokemon);
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

/* phaseManager.forceMoveNext(phase => phase.pokemon === p): the first queued MovePhase of p (in
 * the queue's current order) moves first */
void pr_force_move_next(pr_engine *e, const pr_pokemon *p)
{
    for (int i = 0; i < e->n_dyn[PH_MOVE]; i++) {
        pr_phase *m = &e->pool[e->dyn[PH_MOVE][i]];
        if (m->pokemon == p) {
            m->timing = MOVE_PHASE_TIMING_MODIFIER_FIRST;
            return;
        }
    }
}

/* forceMoveLast uses the first cached owner match in the same queue order. */
void pr_force_move_last(pr_engine *e, const pr_pokemon *p)
{
    for (int i = 0; i < e->n_dyn[PH_MOVE]; i++) {
        pr_phase *m = &e->pool[e->dyn[PH_MOVE][i]];
        if (m->pokemon == p) {
            m->timing = MOVE_PHASE_TIMING_MODIFIER_LAST;
            return;
        }
    }
}

/* tryRemovePhase("MovePhase", phase => phase.pokemon === p): reorder dynamic queue first,
 * then the tree's first match. Removing a move leaves its dynamic marker in place. */
bool pr_remove_move_phase(pr_battle *bt, pr_engine *e, const pr_pokemon *p)
{
    dyn_reorder(bt, e, PH_MOVE);
    for (int i = 0; i < e->n_dyn[PH_MOVE]; i++) {
        int ph = e->dyn[PH_MOVE][i];
        if (e->pool[ph].pokemon != p) continue;
        memmove(&e->dyn[PH_MOVE][i], &e->dyn[PH_MOVE][i + 1],
                (size_t)(e->n_dyn[PH_MOVE] - i - 1));
        e->n_dyn[PH_MOVE]--;
        phase_free(e, ph);
        pr_sync_queued_movers(bt, e);
        return true;
    }
    for (int l = e->n_levels - 1; l >= 0; l--)
        for (int i = 0; i < e->level_len[l]; i++) {
            int ph = e->levels[l][i];
            if (e->pool[ph].type != PH_MOVE || e->pool[ph].pokemon != p) continue;
            memmove(&e->levels[l][i], &e->levels[l][i + 1], (size_t)(e->level_len[l] - i - 1));
            e->level_len[l]--;
            phase_free(e, ph);
            return true;
        }
    return false;
}

pr_phase *pr_find_move_phase(pr_engine *e, int move, int player)
{
    for (int i = 0; i < e->n_dyn[PH_MOVE]; i++) {
        pr_phase *m = &e->pool[e->dyn[PH_MOVE][i]];
        if (move >= 0 && m->move.id != move) continue;
        if (player >= 0 && m->pokemon->is_player != (player != 0)) continue;
        return m;
    }
    return NULL;
}

/* phaseManager.hasPhaseOfType("MovePhase") as the damage path reads it (Analytic) */
void pr_sync_queued_movers(pr_battle *bt, pr_engine *e)
{
    bt->n_queued_movers = 0;
    for (int i = 0; i < e->n_dyn[PH_MOVE] && i < PR_MAX_QUEUED_MOVERS; i++)
        bt->queued_movers[bt->n_queued_movers++] = e->pool[e->dyn[PH_MOVE][i]].pokemon->id;
}

static int check_dynamic(pr_battle *bt, pr_engine *e, int ph);

/* PostSummonPhasePriorityQueue.queueAbilityPhase: a PostSummonPhase pushed onto its queue
 * unshifts a PostSummonActivateAbilityPhase for each of getAbilityPriorities() (the ability's
 * postSummonPriority, then the passive's when it has one) */
static void queue_ability_phases(pr_battle *bt, pr_engine *e, int ph)
{
    int b = e->pool[ph].battler;
    const pr_pokemon *p = b >= 0 && b < 4 ? bt->field[b] : NULL;
    /* Source getPlayerField/getEnemyField slices party slots even before the
     * queued Summon places a sprite on the field. */
    if (!p && b >= 0 && b < 4 && b % 2 < bt->n_party[b / 2]) p = bt->party[b / 2][b % 2];
    if (!p) { limit(bt, e); return; }
    uint16_t abs[2] = {p->ability, p->passive};
    for (int k = 0; k < 2; k++) {
        /* Legacy callers may supply an effective nonzero passive directly. */
        if (k == 1 && !p->passive && !pr_has_passive(bt, p)) break;
        int a = pr_phase_new(e, PH_POST_SUMMON, (int8_t)b);
        if (a < 0) { limit(bt, e); return; }
        e->pool[a].ps_activate = true;
        e->pool[a].ps_passive = k == 1;
        e->pool[a].ps_priority = pr_abilities[abs[k]].post_summon_priority;
        pr_phase_unshift(bt, e, a);
    }
}

/* PhaseManager.checkDynamic: a dynamic phase goes to its queue and a marker takes its place */
static int check_dynamic(pr_battle *bt, pr_engine *e, int ph)
{
    int t = e->pool[ph].type;
    if (!pr_phase_is_dynamic((pr_phase_type)t)) return ph;
    if (e->n_dyn[t] >= PR_DYN_CAP) { limit(bt, e); return ph; }
    e->dyn[t][e->n_dyn[t]++] = (uint8_t)ph;
    if (t == PH_MOVE) pr_sync_queued_movers(bt, e);
    if (t == PH_POST_SUMMON && !e->pool[ph].ps_activate) queue_ability_phases(bt, e, ph);
    int m = pr_phase_new(e, PH_MARKER, -1);
    if (m < 0) { limit(bt, e); return ph; }
    e->pool[m].marker_of = (uint8_t)t;
    return m;
}

static void note_unported_phase(pr_battle *bt, pr_engine *e, int ph)
{
    int t = e->pool[ph].type;
    if (t == PH_TERA) pr_note_unported(bt, (uint16_t)(PR_FEAT_PHASE_0 + t));
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

static int message_phase(pr_engine *e, const pr_msg *m)
{
    int ph = pr_phase_new(e, PH_MESSAGE, -1);
    if (ph >= 0) e->pool[ph].msg = *m;
    return ph;
}

static pr_msg todo_msg(const char *site)
{
    pr_msg m = pr_msg0(NULL);
    m.args[0].s = site;
    return m;
}

void pr_queue_message_at(pr_battle *bt, pr_engine *e, const char *site)
{
    pr_msg m = todo_msg(site);
    pr_phase_unshift(bt, e, message_phase(e, &m));
}

void pr_queue_message_deferred_at(pr_battle *bt, pr_engine *e, const char *site)
{
    pr_msg m = todo_msg(site);
    pr_phase_push(bt, e, message_phase(e, &m));
}

void pr_queue_msg(pr_battle *bt, pr_engine *e, pr_msg m)
{
    pr_phase_unshift(bt, e, message_phase(e, &m));
}

void pr_queue_msg_deferred(pr_battle *bt, pr_engine *e, pr_msg m)
{
    pr_phase_push(bt, e, message_phase(e, &m));
}

void pr_queue_ability_display(pr_battle *bt, pr_engine *e, const pr_pokemon *p, bool passive, bool show)
{
    int b = pr_battler_index(bt, p);
    int ph = pr_phase_new(e, show ? PH_SHOW_ABILITY : PH_HIDE_ABILITY, (int8_t)(show ? b : -1));
    if (ph >= 0 && show) {
        /* the constructor fixes the name shown: getPassiveAbility() or getAbility() */
        e->pool[ph].anim_result = passive;
        e->pool[ph].anim = passive ? (uint16_t)(p->custom_passive >= 0 ? p->custom_passive : pr_pokemon_base_form(p)->passive)
                                   : p->ability;
        /* ShowAbilityPhase caches no name when its field lookup has no Pokemon. */
        if (b < 0) e->pool[ph].anim = -1;
    }
    pr_phase_unshift(bt, e, ph);
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

/* BattleScene.redirectPokemonMoves -> MovePhasePriorityQueue.redirectMoves (double battles):
 * when the ally is active on the field, every queued MovePhase with one target, the removed
 * Pokemon's battler index, whose user opposes the ally, targets the ally (targets[0] of the
 * shared array, so the command and history entries sharing it follow) */
void pr_redirect_moves(pr_battle *bt, pr_engine *e, const pr_pokemon *removed, const pr_pokemon *ally)
{
    if (!bt->double_battle || !ally || !(ally->hp > 0 && ally->on_field && !ally->switch_out_status)) return;
    int rb = pr_battler_index(bt, removed), ab = pr_battler_index(bt, ally);
    for (int i = 0; i < e->n_dyn[PH_MOVE]; i++) {
        const pr_phase *mp = &e->pool[e->dyn[PH_MOVE][i]];
        if (mp->targets < 0 || !mp->pokemon) continue;
        pr_target_list *tl = &e->target_lists[mp->targets];
        if (tl->n != 1 || tl->v[0] != rb || !pr_is_opponent(bt, mp->pokemon, ally)) continue;
        if (tl->v[0] != ab) {
            tl->v[0] = (int8_t)ab;
            pr_targets_changed(bt, e, mp->targets);
        }
    }
}

/* ---------------------------------------------------------------- the loop */

static bool is_stop(int type)
{
    switch (type) {
    case PH_TURN_INIT: case PH_VICTORY: case PH_GAME_OVER:
    case PH_BATTLE_END: case PH_SELECT_MODIFIER: case PH_SELECT_BIOME: case PH_NEW_BATTLE:
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
                         || p->type == PH_MOVE_CHARGE || p->type == PH_TRANSFORM;
    pokemon_phase = pokemon_phase || p->type == PH_MOVE_REFLECT || p->type == PH_POST_SUMMON || p->type == PH_SHINY_SPARKLE || p->type == PH_ATTEMPT_CAPTURE;
    /* SummonPhase (a PartyMemberPokemonPhase) has no battlerIndex upstream: its name alone */
    /* These constructors carry a numeric field index even for a switched-out
     * user/recipient (-1). Preserve that value instead of making it absent. */
    if (p->type == PH_MOVE_END || p->type == PH_STAT_STAGE_CHANGE) snprintf(out, 40, "%s:%d", pr_phase_name[p->type], p->battler);
    else if (pokemon_phase && p->user_id >= 0) snprintf(out, 40, "%s:%lld", pr_phase_name[p->type], (long long)p->user_id);
    else if (pokemon_phase && p->battler >= 0) snprintf(out, 40, "%s:%d", pr_phase_name[p->type], p->battler);
    else snprintf(out, 40, "%s", pr_phase_name[p->type]);
}

static void run_phases(pr_battle *bt, pr_engine *e, int ph)
{
    bt->engine = e;
    for (;;) {
        if (ph < 0 || e->overflow) { pr_note_unported(bt, PR_FEAT_ENGINE_LIMIT); break; }
        if (is_stop(e->pool[ph].type)) {
            e->stop = e->pool[ph].type;
            e->stop_phase = e->pool[ph];
            phase_free(e, ph);
            break;
        }
        trace(e, ph);
        e->current = ph;
        pr_phase_run(bt, e, ph);
        e->current = -1;
        if (e->awaiting_pick) {
            /* the party screen is open: the phase stays in the pool until the pick */
            e->stop = e->pool[ph].type;
            e->stop_phase = e->pool[ph];
            e->pick_phase = ph;
            break;
        }
        if (e->awaiting_confirm) {
            e->stop = PH_CHECK_SWITCH;
            e->stop_phase = e->pool[ph];
            e->pick_phase = ph;
            break;
        }
        phase_free(e, ph);
        if (e->n_trace >= PR_MAX_TRACE) { e->overflow = true; continue; }
        ph = shift_phase(bt, e);
    }
    bt->engine = NULL;
}

static void engine_reset(pr_battle *bt, pr_engine *e);

void pr_run_turn(pr_battle *bt, pr_engine *e)
{
    engine_reset(bt, e);
    /* the caller's pushNew("TurnStartPhase") + shiftPhase() */
    pr_phase_push(bt, e, pr_phase_new(e, PH_TURN_START, -1));
    run_phases(bt, e, shift_phase(bt, e));
}

static void engine_reset(pr_battle *bt, pr_engine *e)
{
    memset(e, 0, sizeof *e);
    e->n_levels = 1;
    e->current = -1;
    for (int i = 0; i < 4; i++) e->command_targets[i] = -1;
    bt->engine = e;
}

/* Source doPostBattleCleanup queues Return before the attribute callbacks;
 * unshifted form phases therefore execute before returning the old field. */
void pr_prepare_player_cleanup(pr_battle *bt, pr_engine *e, bool reset_arena)
{
    engine_reset(bt, e);
    if (!reset_arena) return;
    /* Arena.resetArenaEffects precedes Return and all party init callbacks.
     * A duration of zero denotes permanent biome weather/terrain. */
    if (bt->weather_turns_left != 0)
        pr_try_set_weather(bt, e, WEATHER_TYPE_NONE, NULL);
    if (bt->terrain_turns_left != 0)
        pr_try_set_terrain(bt, e, TERRAIN_NONE, true, NULL);
    bt->player_faints = 0;
    pr_arena_remove_all_tags(bt, e);
    int field_count = bt->double_battle ? 2 : 1;
    for (int i=0;i<field_count && i<bt->n_party[0];i++) {
        pr_pokemon *p=bt->party[0][i];
        pr_lapse_tag(bt,e,p,BATTLER_TAG_TYPE_COMMANDED,BATTLER_TAG_LAPSE_TYPE_CUSTOM);
        if (p->on_field && !p->switch_out_status) {
            int ph=pr_phase_new(e, PH_RETURN, i);
            if (ph<0) { e->overflow=true; continue; }
            e->pool[ph].sw_player=true;e->pool[ph].sw_field_index=i;
            pr_phase_push(bt,e,ph);
        }
    }
    for (int i=0;i<bt->n_party[0];i++) {
        pr_pokemon *p=bt->party[0][i];
        pr_reset_battle_data(p);
        bool was_tera=p->terastallized;
        p->terastallized=false;p->stellar_types_boosted=0;
        if (was_tera) pr_form_change(bt,p,PR_FCT_SpeciesFormChangeLapseTeraTrigger);
        pr_ab_params pp={.pokemon=p,.move=-1};
        pr_apply_ab_attrs(bt,PR_AC_PostBattleInitAbAttr,&pp);
    }
}

void pr_run_player_cleanup(pr_battle *bt, pr_engine *e)
{
    bt->engine=e;
    /* The owning encounter follows this queue. A stop sentinel hands it back. */
    pr_phase_push(bt,e,pr_phase_new(e,PH_TURN_INIT,-1));
    run_phases(bt,e,e->overflow ? -1 : shift_phase(bt,e));
}

/* EncounterPhase.end. Trainer prelude and queue execution belong to the caller. */
void pr_queue_encounter_end(pr_battle *bt, pr_engine *e, bool loaded)
{
    int count=bt->double_battle?2:1;
    for(int k=0;k<count&&k<bt->n_party[1];k++)
        if(bt->party[1][k]->shiny)pr_unshift_new(bt,e,PH_SHINY_SPARKLE,2+k);
    if(!loaded){
        const pr_pokemon *allowed[PR_MAX_PARTY];int n_allowed=0;
        for(int k=0;k<bt->n_party[0];k++)if(bt->party[0][k]->hp>0)allowed[n_allowed++]=bt->party[0][k];
        if(!n_allowed){pr_note_unported(bt,PR_FEAT_PARTY);return;}
        if(!allowed[0]->on_field||allowed[0]->switch_out_status){
            int s=pr_phase_new(e,PH_SUMMON,0);
            if(s>=0){e->pool[s].sw_field_index=0;e->pool[s].sw_player=true;}
            pr_phase_push(bt,e,s);
        }
        if(bt->double_battle){
            if(n_allowed>1){
                int t=pr_phase_new(e,PH_TOGGLE_DOUBLE_POSITION,-1);
                if(t>=0)e->pool[t].position_double=true;
                pr_phase_push(bt,e,t);
                if(!allowed[1]->on_field||allowed[1]->switch_out_status){
                    int s=pr_phase_new(e,PH_SUMMON,1);
                    if(s>=0){e->pool[s].sw_field_index=1;e->pool[s].sw_player=true;}
                    pr_phase_push(bt,e,s);
                }
            }
        }else{
            if(n_allowed>1&&allowed[1]->on_field&&!allowed[1]->switch_out_status){
                int s=pr_phase_new(e,PH_RETURN,1);
                if(s>=0){e->pool[s].sw_field_index=1;e->pool[s].sw_player=true;e->pool[s].sw_do_return=true;e->pool[s].sw_type=SWITCH_TYPE_SWITCH;e->pool[s].sw_slot=-1;}
                pr_phase_push(bt,e,s);
            }
            pr_phase_push(bt,e,pr_phase_new(e,PH_TOGGLE_DOUBLE_POSITION,-1));
        }
        if(bt->battle_type!=BATTLE_TYPE_TRAINER&&(bt->wave_index>1||bt->game_mode!=GAME_MODES_DAILY)
            &&n_allowed>(bt->double_battle?2:1)){
            for(int k=0;k<(bt->double_battle?2:1);k++){
                int s=pr_phase_new(e,PH_CHECK_SWITCH,-1);
                if(s>=0){e->pool[s].sw_field_index=k;e->pool[s].sw_use_name=bt->double_battle;}
                pr_phase_push(bt,e,s);
            }
        }
    }
    pr_phase_push(bt,e,pr_phase_new(e,PH_INIT_ENCOUNTER,-1));
}

/* doEncounterCommon's trainer prelude before EncounterPhase.end. */
void pr_queue_trainer_summons(pr_battle *bt, pr_engine *e)
{
    if(bt->battle_type==BATTLE_TYPE_TRAINER){
        int legal=0;for(int k=0;k<bt->n_party[1];k++)legal+=bt->party[1][k]->hp>0;
        for(int k=0;k<(bt->double_battle&&legal>1?2:1);k++){
            int s=pr_phase_new(e,PH_SUMMON,2+k);
            if(s>=0){e->pool[s].sw_field_index=k;e->pool[s].sw_player=false;}
            pr_phase_unshift(bt,e,s);
        }
    }
}

/* Fresh encounter trainer prelude, then its end queue. */
void pr_start_battle(pr_battle *bt, pr_engine *e)
{
    engine_reset(bt,e);
    if(bt->battle_type==BATTLE_TYPE_MYSTERY_ENCOUNTER)pr_note_unported(bt,PR_FEAT_TRAINER);
    /* PR_DEV_ILLUSION: the Pokemon already on the field (the player's, a wild one) take their disguise
     * here; a trainer's take it in their SummonPhase */
    for(int b=0;b<4;b++){pr_pokemon *p=bt->field[b];if(p&&p->on_field&&(b<2||bt->battle_type!=BATTLE_TYPE_TRAINER))pr_illusion_on_summon(bt,p);}
    pr_queue_trainer_summons(bt,e);
    pr_queue_encounter_end(bt,e,false);
    run_phases(bt,e,shift_phase(bt,e));
}

void pr_answer_check_switch(pr_battle *bt, pr_engine *e, bool yes)
{
    if (!e->awaiting_confirm) return;
    bt->engine = e;
    e->n_events = 0;
    e->events_overflow = false;
    int ph = e->pick_phase;
    e->awaiting_confirm = false;
    e->stop = 0;
    e->current = ph;
    if (yes) {
        /* unshiftNew("SwitchPhase", SwitchType.INITIAL_SWITCH, fieldIndex, false, true) */
        int q = pr_phase_new(e, PH_SWITCH, -1);
        if (q < 0) { pr_note_unported(bt, PR_FEAT_ENGINE_LIMIT); e->overflow = true; }
        else {
            pr_phase *n = &e->pool[q];
            n->sw_type = SWITCH_TYPE_INITIAL_SWITCH;
            n->sw_field_index = e->pool[ph].sw_field_index;
            n->sw_modal = false;
            n->sw_do_return = true;
            n->sw_player = true;
            pr_phase_unshift(bt, e, q);
        }
    }
    e->current = -1;
    phase_free(e, ph);
    run_phases(bt, e, e->overflow ? -1 : shift_phase(bt, e));
}

/* VictoryPhase.start without what belongs to the game (the Pokemon defeated count,
 * applyPartyExp) and with the reward phases it pushes after BattleEndPhase left to the game */
void pr_continue(pr_battle *bt, pr_engine *e)
{
    if (e->stop != PH_VICTORY) return;
    bt->engine = e;
    e->n_events = 0;
    e->events_overflow = false;
    e->stop = 0;
    if (e->n_trace < PR_MAX_TRACE)
        snprintf(e->trace[e->n_trace++], 40, "%s:%d", pr_phase_name[PH_VICTORY], e->stop_phase.battler);
    if (bt->battle_type == BATTLE_TYPE_MYSTERY_ENCOUNTER) pr_note_unported(bt, PR_FEAT_TRAINER);
    /* getEnemyParty().find(p => WILD ? p.isOnField() : !p?.isFainted()) finds nobody, and no
     * TrainerVictoryPhase is queued yet: that one goes in with the trainer battle's BattleEndPhase */
    bool left = false;
    bool wild = bt->battle_type == BATTLE_TYPE_WILD;
    if (bt->n_party[1]) {
        for (int k = 0; k < bt->n_party[1]; k++) {
            const pr_pokemon *p = bt->party[1][k];
            if (wild ? p->on_field : p->hp > 0) left = true;
        }
    } else {
        for (int i = 2; i < 4; i++) {
            const pr_pokemon *p = bt->field[i];
            if (p && (wild ? p->on_field : p->hp > 0)) left = true;
        }
    }
    bool trainer_victory = bt->battle_type == BATTLE_TYPE_TRAINER && pr_phase_exists(e, PH_BATTLE_END, -1);
    if (!left && !trainer_victory) pr_phase_push(bt, e, pr_phase_new(e, PH_BATTLE_END, -1));
    run_phases(bt, e, shift_phase(bt, e));
}

/* The party screen's callback: a slot past the Pokemon on the field queues the SwitchSummonPhase
 * (Baton Pass's screen hands over PASS_BATON, which keeps the Baton Pass type), then the
 * SwitchPhase ends once the screen has gone back to the message mode. */
void pr_resume_party_pick(pr_battle *bt, pr_engine *e, int slot)
{
    if (!e->awaiting_pick) return;
    bt->engine = e;
    e->n_events = 0;
    e->events_overflow = false;
    int ph = e->pick_phase;
    e->awaiting_pick = false;
    e->stop = 0;
    e->current = ph;
    const pr_phase *s = &e->pool[ph];
    if (s->type == PH_REVIVAL_BLESSING) {
        pr_finish_revival_choice(bt, e, s->pokemon, slot);
    } else if (slot >= (bt->double_battle ? 2 : 1) && slot < 6) {
        int q = pr_phase_new(e, PH_SWITCH_SUMMON, -1);
        if (q < 0) { pr_note_unported(bt, PR_FEAT_ENGINE_LIMIT); e->overflow = true; }
        else {
            pr_phase *n = &e->pool[q];
            s = &e->pool[ph];
            n->sw_type = s->sw_type;
            n->sw_field_index = s->sw_field_index;
            n->sw_slot = (int8_t)slot;
            n->sw_do_return = s->sw_do_return;
            n->sw_player = true;
            pr_phase_unshift(bt, e, q);
        }
    }
    e->current = -1;
    phase_free(e, ph);
    run_phases(bt, e, e->overflow ? -1 : shift_phase(bt, e));
}


/* Existing switch callers remain source compatible. */
void pr_resume_switch(pr_battle *bt, pr_engine *e, int slot)
{
    pr_resume_party_pick(bt, e, slot);
}

void pr_cancel_queued_move(pr_engine *e, const pr_pokemon *pokemon)
{
    for (int i=0;i<e->n_dyn[PH_MOVE];i++) {
        pr_phase *p=&e->pool[e->dyn[PH_MOVE][i]];
        if (p->pokemon!=pokemon) continue;
        p->cancelled=true;p->entry.result=MOVE_RESULT_FAIL;
        return;
    }
}

bool pr_remove_switch_summon_field(pr_battle *bt, pr_engine *e, int field_index)
{
    dyn_reorder(bt,e,PH_SWITCH_SUMMON);
    for (int i=0;i<e->n_dyn[PH_SWITCH_SUMMON];i++) {
        int ix=e->dyn[PH_SWITCH_SUMMON][i];
        if (e->pool[ix].sw_field_index!=field_index) continue;
        memmove(&e->dyn[PH_SWITCH_SUMMON][i],&e->dyn[PH_SWITCH_SUMMON][i+1],(size_t)(e->n_dyn[PH_SWITCH_SUMMON]-i-1));
        e->n_dyn[PH_SWITCH_SUMMON]--;phase_free(e,ix);return true;
    }
    for (int l=e->n_levels-1;l>=0;l--) for (int i=0;i<e->level_len[l];i++) {
        int ix=e->levels[l][i];
        if (e->pool[ix].type!=PH_SWITCH_SUMMON||e->pool[ix].sw_field_index!=field_index) continue;
        memmove(&e->levels[l][i],&e->levels[l][i+1],(size_t)(e->level_len[l]-i-1));
        e->level_len[l]--;phase_free(e,ix);return true;
    }
    return false;
}

void pr_queue_prompt_msg(pr_battle *bt, pr_engine *e, pr_msg msg)
{
    int ph=message_phase(e,&msg);
    if(ph>=0)e->pool[ph].message_prompt=true;
    pr_phase_unshift(bt,e,ph);
}
