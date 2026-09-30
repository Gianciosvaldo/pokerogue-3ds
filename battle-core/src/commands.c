/* The command screen's checks (src/phases/command-phase.ts): whether a move can be picked, whether
 * the Pokemon can be switched out, whether the player can run, and the text upstream shows when
 * it can't. They only read the battle: nothing is queued and no ability shows (upstream asks
 * with simulated calls). */
#include "pr/engine.h"
#include <math.h>

/* PokemonMove.getMovePp(): maxPpOverride, else pp + ppUp * toDmgValue(pp / 5) */
static int move_max_pp(const pr_pokemon *p, int slot)
{
    if (p->max_pp_override[slot] > 0) return p->max_pp_override[slot];
    int pp = pr_moves[p->moves[slot]].pp;
    double step = floor(pp / 5.0);
    return pp + p->pp_up[slot] * (int)(step > 1 ? step : 1);
}

static bool out_of_pp(const pr_pokemon *p, int slot)
{
    int max = move_max_pp(p, slot);
    return max != -1 && p->pp_used[slot] >= max;
}

static bool in_moveset(const pr_pokemon *p, int move)
{
    for (int i = 0; i < p->n_moves; i++)
        if (p->moves[i] == move) return true;
    return false;
}

/* MoveRestrictionBattlerTag.isMoveRestricted(move, user) for p's tag t; false for other tags */
static bool tag_restricts(pr_battle *bt, const pr_pokemon *p, const pr_battler_tag *t, int move)
{
    switch (t->type) {
    case BATTLER_TAG_TYPE_THROAT_CHOPPED: return (pr_moves[move].flags & MOVE_FLAGS_SOUND_BASED) != 0;
    case BATTLER_TAG_TYPE_DISABLED: return move == t->move_id;
    case BATTLER_TAG_TYPE_GORILLA_TACTICS:
    case BATTLER_TAG_TYPE_ENCORE: return move != t->move_id;
    case BATTLER_TAG_TYPE_TAUNT: return pr_moves[move].category == MOVE_CATEGORY_STATUS;
    case BATTLER_TAG_TYPE_HEAL_BLOCK:
        for (unsigned i = 0; i < pr_set_heal_blocked_moves_n; i++)
            if (pr_set_heal_blocked_moves[i] == move) return true;
        return false;
    case BATTLER_TAG_TYPE_TORMENT: {
        const pr_turn_move *last = pr_last_move(bt, p, 0);
        if (!last) return false;
        bool unaffected = pr_move_has_attr(last->move, PR_AC_ConsecutiveUseDoublePowerAttr)
                          || pr_get_battler_tag(p, BATTLER_TAG_TYPE_FRENZY);
        bool valid = last->result == MOVE_RESULT_SUCCESS || last->result == MOVE_RESULT_MISS;
        return last->move == move && valid && last->move != MOVE_ID_STRUGGLE && !unaffected;
    }
    case BATTLER_TAG_TYPE_IMPRISON: {
        const pr_pokemon *src = pr_pokemon_by_id(bt, t->source_id);
        return src && in_moveset(src, move) && src->hp > 0 && src->on_field && !src->switch_out_status;
    }
    }
    return false;
}

/* the tag's selectionDeniedText(pokemon, move) */
static pr_msg denied_text(const pr_battle *bt, const pr_pokemon *p, const pr_battler_tag *t, int move)
{
    pr_msg_arg name = pr_arg_pokemon(bt, "pokemonNameWithAffix", p);
    switch (t->type) {
    case BATTLER_TAG_TYPE_THROAT_CHOPPED: return pr_msg1("battle:moveCannotBeSelected", pr_arg_move("moveName", move));
    case BATTLER_TAG_TYPE_GORILLA_TACTICS:
        return pr_msg2("battle:canOnlyUseMove", pr_arg_move("moveName", t->move_id),
                       pr_arg_pokemon(bt, "pokemonName", p));
    case BATTLER_TAG_TYPE_HEAL_BLOCK: return pr_msg2("battle:moveDisabledHealBlock", name, pr_arg_move("moveName", move));
    case BATTLER_TAG_TYPE_TORMENT: return pr_msg1("battle:moveDisabledTorment", name);
    case BATTLER_TAG_TYPE_TAUNT: return pr_msg2("battle:moveDisabledTaunt", name, pr_arg_move("moveName", move));
    case BATTLER_TAG_TYPE_IMPRISON: return pr_msg2("battle:moveDisabledImprison", name, pr_arg_move("moveName", move));
    default: return pr_msg1("battle:moveDisabled", pr_arg_move("moveName", move));   /* Disable, Encore */
    }
}

/* Move.checkRestrictions(user): the moves' own MoveRestrictions, which upstream writes as closures
 * on four moves plus gravity()'s. Stuff Cheeks needs a berry on the user's side: berries aren't a
 * modifier class here yet, so a held item of a class the port doesn't know counts as one. */
static bool move_restriction(pr_battle *bt, const pr_pokemon *p, int move, pr_msg *why)
{
    pr_msg_arg name = pr_arg_pokemon(bt, "pokemonNameWithAffix", p), mv = pr_arg_move("moveName", move);
    if (move == MOVE_ID_BELCH && !p->has_eaten_berry) {
        *why = pr_msg2("battle:moveDisabledBelch", name, mv);
        return true;
    }
    if (move == MOVE_ID_STUFF_CHEEKS) {
        int side = p->is_player ? 0 : 1;
        bool berry = false;
        for (int i = 0; i < bt->n_mods[side]; i++)
            berry |= pr_mod_is(&bt->mods[side][i], PR_MOD_HELD_ITEM) && bt->mods[side][i].unknown_subclass;
        if (!berry) {
            *why = pr_msg2("battle:moveDisabledNoBerry", name, mv);
            return true;
        }
    }
    if (move == MOVE_ID_GIGATON_HAMMER || move == MOVE_ID_BLOOD_MOON) {       /* consecutiveUseRestriction */
        const pr_turn_move *last = pr_last_move(bt, p, 0);
        if (last && last->move == move) {
            *why = pr_msg2("battle:moveDisabledConsecutive", name, mv);
            return true;
        }
    }
    if ((pr_moves[move].flags & MOVE_FLAGS_GRAVITY)
        && pr_arena_get_tag_on_side(bt, ARENA_TAG_TYPE_GRAVITY, ARENA_TAG_SIDE_BOTH)) {   /* gravityUseRestriction */
        *why = pr_msg2("battle:moveDisabledGravity", name, mv);
        return true;
    }
    return false;
}

bool pr_move_selectable(pr_battle *bt, const pr_pokemon *p, int move, pr_msg *why)
{
    pr_msg m = {0};
    bool ok = true;
    for (int i = 0; i < p->n_tags && ok; i++)
        if (tag_restricts(bt, p, &p->tags[i], move)) {
            m = denied_text(bt, p, &p->tags[i], move);
            ok = false;
        }
    if (ok && move_restriction(bt, p, move, &m)) ok = false;
    if (why) *why = m;
    return ok;
}

bool pr_move_usable(pr_battle *bt, const pr_pokemon *p, int slot, bool ignore_pp, bool for_selection, pr_msg *why)
{
    pr_msg m = {0};
    if (why) *why = m;
    if (slot < 0 || slot >= 4) return false;
    int move = p->moves[slot];
    if (move == MOVE_ID_NONE || pr_moves[move].unimplemented) {
        /* moveName.replace(" (N)", "") */
        if (why) *why = pr_msg1("battle:moveNotImplemented", pr_arg_move("moveName", move | PR_MSG_MOVE_BARE));
        return false;
    }
    if (!ignore_pp && out_of_pp(p, slot)) {
        if (why) *why = pr_msg1("battle:moveNoPp", pr_arg_move("moveName", move));
        return false;
    }
    if (for_selection && !pr_move_selectable(bt, p, move, why)) return false;
    return true;   /* the challenges' POKEMON_MOVE rule for the player isn't modelled */
}

int pr_fight_move(pr_battle *bt, const pr_pokemon *p, int slot, bool ignore_pp, pr_msg *why)
{
    if (why) *why = (pr_msg){0};
    if (slot == -1) return MOVE_ID_NONE;
    pr_msg reason;
    if (slot < p->n_moves && pr_move_usable(bt, p, slot, ignore_pp, true, &reason)) return p->moves[slot];
    if (slot >= p->n_moves) reason = pr_msg0("");               /* trySelectMove: no move there ("") */
    for (int i = 0; i < p->n_moves; i++)
        if (pr_move_usable(bt, p, i, ignore_pp, true, NULL)) {
            if (why) *why = reason;
            return -1;
        }
    return MOVE_ID_STRUGGLE;                                     /* nothing usable: Struggle */
}

/* ArenaTrapAbAttr's arenaTrapCondition closures (Shadow Tag, Magnet Pull, Arena Trap), checked
 * against the upstream source they were written for; any other goes through the closure table */
static bool arena_trap_condition(pr_battle *bt, uint16_t fn, const pr_attr *a, const pr_pokemon *user,
                                 const pr_pokemon *target)
{
    if (pr_closure_matches(fn, "ability:23", "2f8bc1c25e68"))       /* !target.hasAbility(SHADOW_TAG) */
        return !pr_has_ability_active(bt, target, ABILITY_ID_SHADOW_TAG);
    if (pr_closure_matches(fn, "ability:42", "4ef22f394ce6")) {     /* target.isOfType(STEEL, {returnOriginalTypesIfStellar}) */
        int8_t t[PR_MAX_TYPES];
        int n = pr_get_types(target, t, PR_TYPES_ORIGINAL_IF_STELLAR);
        for (int i = 0; i < n; i++)
            if (t[i] == POKEMON_TYPE_STEEL) return true;
        return false;
    }
    if (pr_closure_matches(fn, "ability:71", "8e2c012e822a"))       /* target.isGrounded() */
        return pr_is_grounded(bt, target, false);
    return pr_closure_call(bt, fn, a->cls, 0, user, target, -1) != 0;
}

/* applyAbAttrs("CheckTrappedAbAttr", {pokemon: opp, trapped, opponent: p, simulated: true}, messages):
 * the ability, then the passive; the first trigger message goes to *msg */
static void check_trapped(pr_battle *bt, const pr_pokemon *opp, const pr_pokemon *p, bool *trapped, pr_msg *msg)
{
    for (int passive = 0; passive < 2; passive++) {
        uint16_t ability = passive ? opp->passive : opp->ability;
        if (passive && (!opp->passive || opp->passive == opp->ability)) continue;
        const pr_ability *ab = &pr_abilities[ability];
        bool checked = false, can = false;
        for (unsigned i = 0; i < ab->nattr; i++) {
            const pr_attr *a = &pr_attrs[ab->attr0 + i];
            if (!pr_attr_is(a->cls, PR_AC_CheckTrappedAbAttr)) continue;
            if (!checked) { can = pr_can_apply_ability(bt, opp, ability); checked = true; }
            if (!can) break;
            if (a->cond != PR_NO_FN && !pr_closure_call(bt, a->cond, a->cls, 1, opp, NULL, -1)) continue;
            if (a->cls != PR_AC_ArenaTrapAbAttr) continue;   /* CheckTrappedAbAttr itself does nothing */
            uint16_t fn = ((const pr_attr_ArenaTrapAbAttr *)a->p)->arenaTrapCondition;
            int8_t t[PR_MAX_TYPES];
            int n = pr_get_types(p, t, PR_TYPES_ORIGINAL_IF_STELLAR);
            bool ghost = false;
            for (int k = 0; k < n; k++) ghost |= t[k] == POKEMON_TYPE_GHOST;
            if (!arena_trap_condition(bt, fn, a, opp, p) || ghost || pr_has_ability_active(bt, p, ABILITY_ID_RUN_AWAY))
                continue;
            if (msg && !msg->key)
                *msg = pr_msg2("abilityTriggers:arenaTrap", pr_arg_pokemon(bt, "pokemonNameWithAffix", opp),
                               pr_arg_ability("abilityName", ability));
            *trapped = true;
        }
    }
}

static bool commanded(const pr_battle *bt, const pr_pokemon *p)
{
    const pr_battler_tag *cmd = pr_get_battler_tag(p, BATTLER_TAG_TYPE_COMMANDED);
    if (!cmd) return false;
    const pr_pokemon *src = pr_pokemon_by_id(bt, cmd->source_id);
    return src && src->hp > 0 && src->on_field && !src->switch_out_status;   /* getSourcePokemon()?.isActive(true) */
}

bool pr_is_trapped(pr_battle *bt, const pr_pokemon *p, pr_msg *ab_msg)
{
    if (ab_msg) *ab_msg = (pr_msg){0};
    if (commanded(bt, p)) return true;
    if (pr_is_of_type(p, POKEMON_TYPE_GHOST) || pr_has_ability_active(bt, p, ABILITY_ID_RUN_AWAY)) return false;
    bool trapped = false;
    pr_speed_iter it;
    pr_pokemon *opp;
    pr_speed_iter_init(bt, &it, p->is_player ? ARENA_TAG_SIDE_ENEMY : ARENA_TAG_SIDE_PLAYER);
    while ((opp = pr_speed_iter_next(bt, &it)))
        if (!opp->switch_out_status) check_trapped(bt, opp, p, &trapped, ab_msg);
    int side = p->is_player ? ARENA_TAG_SIDE_PLAYER : ARENA_TAG_SIDE_ENEMY;
    return trapped || pr_find_battler_tag_class(p, PR_TAGCLASS_TRAPPED)
           || pr_arena_get_tag_on_side(bt, ARENA_TAG_TYPE_FAIRY_LOCK, side);
}

/* showNoEscapeText(tag, false): upstream always says "fleeing", on the switch command too */
static pr_msg no_escape_text(const pr_battle *bt, int64_t source_id, int source_move)
{
    const pr_pokemon *src = source_id >= 0 ? pr_pokemon_by_id(bt, source_id) : NULL;
    pr_msg_arg name = src ? pr_arg_pokemon(bt, "pokemonName", src) : (pr_msg_arg){"pokemonName", PR_ARG_STR, 0, ""};
    return pr_msg3("battle:noEscapePokemon", name, pr_arg_move("moveName", source_move),
                   pr_arg_key("escapeVerb", "battle:escapeVerbFlee"));
}

/* tryLeaveField's checks for p (handleTrap): the text shown when p can't leave */
static bool can_leave(pr_battle *bt, const pr_pokemon *p, bool is_switch, bool baton, pr_msg *why)
{
    if (commanded(bt, p)) {
        if (is_switch) *why = pr_msg0("battle:noEscapeForce");
        return false;
    }
    if (baton) return true;
    pr_msg ab_msg;
    if (!pr_is_trapped(bt, p, &ab_msg)) return true;
    if (ab_msg.key) {
        if (is_switch) *why = ab_msg;                    /* on the run command, nothing is shown */
        return false;
    }
    const pr_battler_tag *trap = pr_find_battler_tag_class(p, PR_TAGCLASS_TRAPPED);
    const pr_arena_tag *lock = pr_arena_get_tag_on_side(bt, ARENA_TAG_TYPE_FAIRY_LOCK,
                                                        p->is_player ? ARENA_TAG_SIDE_PLAYER : ARENA_TAG_SIDE_ENEMY);
    if (trap) *why = no_escape_text(bt, trap->source_id, trap->source_move);
    else if (lock) *why = no_escape_text(bt, lock->source_id, lock->source_move);
    return false;
}

bool pr_can_switch(pr_battle *bt, const pr_pokemon *p, bool baton, pr_msg *why)
{
    pr_msg m = {0};
    bool ok = can_leave(bt, p, true, baton, &m);
    if (why) *why = m;
    return ok;
}

bool pr_can_run(pr_battle *bt, const pr_pokemon *p, pr_msg *why)
{
    pr_msg m = {0};
    bool ok;
    if (bt->battle_type == BATTLE_TYPE_MYSTERY_ENCOUNTER) pr_note_unported(bt, PR_FEAT_TRAINER);   /* fleeAllowed */
    if (bt->has_biome && bt->biome == BIOME_ID_END) {
        m = pr_msg0("battle:noEscapeForce");
        ok = false;
    } else if (bt->battle_type == BATTLE_TYPE_TRAINER) {
        m = pr_msg0("battle:noEscapeTrainer");
        ok = false;
    } else {
        ok = can_leave(bt, p, false, false, &m);
    }
    if (why) *why = m;
    return ok;
}
