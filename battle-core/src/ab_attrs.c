/* Ability attribute dispatch: ports of applyAbAttrs (src/data/abilities/apply-ab-attrs.ts),
 * Pokemon.canApplyAbility, and the canApply/apply pairs of the AbAttr classes on the
 * damage path (src/data/abilities/ab-attrs.ts). A class with no case here is reported
 * unported, never skipped silently. */
#include "pr/engine.h"
#include <math.h>
#include <stddef.h>
#include <string.h>

#define AB_FLAG_BYPASS_FAINT (1u << 0)
#define AB_FLAG_IGNORABLE (1u << 1)
#define AB_FLAG_UNSUPPRESSABLE (1u << 2)
#define AB_FLAG_UNCOPIABLE (1u << 3)
#define AB_FLAG_UNREPLACEABLE (1u << 4)

#define P(T, a) ((const pr_attr_##T *)(a)->p)
/* Nullable closure field: PR_NO_FN when absent. Structs with a `nulls` mask always
 * store PR_NO_FN for a null fn field, so the id alone is enough. */
#define FN(T, a, f) (P(T, a)->f)

static double to_dmg_value(double v)
{
    v = floor(v);
    return v > 1 ? v : 1;
}

bool pr_is_opponent(const pr_battle *bt, const pr_pokemon *a, const pr_pokemon *b)
{
    (void)bt;
    return a->is_player != b->is_player;
}

const pr_pokemon *pr_get_ally(const pr_battle *bt, const pr_pokemon *p)
{
    if (!bt->double_battle) return NULL;
    for (int i = 0; i < 4; i++)
        if (bt->field[i] == p) return bt->field[i ^ 1];
    return NULL;
}

const pr_attr *pr_ability_first_attr(uint16_t ability, uint16_t cls)
{
    if (ability >= PR_ABILITY_COUNT) return NULL;
    const pr_ability *a = &pr_abilities[ability];
    for (unsigned i = 0; i < a->nattr; i++)
        if (pr_attr_is(pr_attrs[a->attr0 + i].cls, cls)) return &pr_attrs[a->attr0 + i];
    return NULL;
}

bool pr_move_is_attack(int move)
{
    /* Move.is("AttackMove"): AttackMove and ChargingAttackMove (the mixin keeps AttackMove.is) */
    const char *k = pr_moves[move].kind;
    return !strcmp(k, "AttackMove") || !strcmp(k, "ChargingAttackMove");
}

/* Move.getPriority(user) */
int pr_move_get_priority(pr_battle *bt, int move, const pr_pokemon *user)
{
    double prio = pr_moves[move].priority;
    /* applyMoveAttrs("IncrementMovePriorityAttr", user, null, move, priority): Grassy Glide */
    const pr_move *mv = &pr_moves[move];
    for (unsigned i = 0; i < mv->nattr; i++) {
        const pr_attr *a = &pr_attrs[mv->attr0 + i];
        if (!pr_attr_is(a->cls, PR_AC_IncrementMovePriorityAttr)) continue;
        const pr_attr_IncrementMovePriorityAttr *x = P(IncrementMovePriorityAttr, a);
        if (pr_closure_call(bt, x->moveIncrementFunc, a->cls, 0, user, NULL, move)) prio += x->increaseAmount;
    }
    pr_ab_params pp = {.pokemon = user, .move = move, .simulated = true, .value = &prio};
    pr_apply_ab_attrs(bt, PR_AC_ChangeMovePriorityAbAttr, &pp);
    return (int)prio;
}

bool pr_has_ability_active(pr_battle *bt, const pr_pokemon *p, int ability)
{
    if (p->ability == ability && pr_can_apply_ability(bt, p, p->ability)) return true;
    return p->passive && p->passive == ability && pr_can_apply_ability(bt, p, p->passive);
}

/* PostSummonAbAttr.shouldActivateOnGain(): activateOnGain, after the fields every attr starts with
 * (nulls, showAbility, extraCondition); extraCondition is a closure id where an instance has one */
static bool activate_on_gain(const pr_attr *a)
{
    switch (a->cls) {
    case PR_AC_PostSummonAddBattlerTagAbAttr: return P(PostSummonAddBattlerTagAbAttr, a)->activateOnGain;
    case PR_AC_PostSummonFormChangeAbAttr: return P(PostSummonFormChangeAbAttr, a)->activateOnGain;
    case PR_AC_PostSummonMessageAbAttr: return P(PostSummonMessageAbAttr, a)->activateOnGain;
    }
    return ((const struct { uint32_t nulls; bool showAbility; uint8_t extraCondition; bool activateOnGain; } *)a->p)
        ->activateOnGain;
}
_Static_assert(offsetof(pr_attr_PostSummonTransformAbAttr, activateOnGain) == offsetof(pr_attr_DownloadAbAttr, activateOnGain),
               "PostSummon attr layout");

/* Pokemon.setTempAbility(ability) (passive = false): the ability's lose effects, summonData.ability,
 * then the new ability's PostSummon attrs that activate on gain */
void pr_set_temp_ability(pr_battle *bt, pr_pokemon *p, uint16_t ability)
{
    pr_ab_params lose = {.pokemon = p, .move = -1, .only = 1};
    pr_apply_ab_attrs(bt, PR_AC_PreLeaveFieldAbAttr, &lose);            /* applyOnLoseAbAttrs (no ability has an
                                                                         * IllusionBreakAbAttr in the dump) */
    p->ability_override = ability;
    /* getAbility ignores a zero summonData.ability in the pin. An ordinary
     * actor therefore gains the permanent ability's effects again. */
    p->ability = ability ? ability : p->base_ability;
    pr_refresh_ability(p);
    if (!ability && p->transformed && !p->ability_lookup_known) pr_note_unported(bt, PR_FEAT_FORM_CHANGE);
    pr_ab_params gain = {.pokemon = p, .move = -1, .only = 1, .on_gain = true};
    pr_apply_ab_attrs(bt, PR_AC_PostSummonAbAttr, &gain);               /* applyOnGainAbAttrs */
}

/* applyAbAttrs("PostDamageAbAttr", {pokemon, damage, source}) */
void pr_post_damage_ab(pr_battle *bt, pr_pokemon *p, double damage, pr_pokemon *source)
{
    pr_ab_params pp = {.pokemon = p, .opponent = source, .move = -1, .damage = damage};
    pr_apply_ab_attrs(bt, PR_AC_PostDamageAbAttr, &pp);
}

bool pr_has_ability_with_attr(pr_battle *bt, const pr_pokemon *p, uint16_t cls)
{
    if (pr_can_apply_ability(bt, p, p->ability) && pr_ability_first_attr(p->ability, cls)) return true;
    return p->passive && pr_can_apply_ability(bt, p, p->passive) && pr_ability_first_attr(p->passive, cls);
}

bool pr_move_hits_substitute(pr_battle *bt, int move, const pr_pokemon *user, const pr_pokemon *target)
{
    int tgt = pr_moves[move].target;
    if (tgt == MOVE_TARGET_USER || tgt == MOVE_TARGET_USER_SIDE || tgt == MOVE_TARGET_ENEMY_SIDE
        || tgt == MOVE_TARGET_BOTH_SIDES || !target || !pr_has_battler_tag(bt, target, BATTLER_TAG_TYPE_SUBSTITUTE))
        return false;
    bool bypassed = false;
    pr_ab_params pp = {.pokemon = user, .move = -1, .flag = &bypassed};
    pr_apply_ab_attrs(bt, PR_AC_InfiltratorAbAttr, &pp);
    unsigned fl = pr_moves[move].flags;
    return !bypassed && !(fl & MOVE_FLAGS_SOUND_BASED) && !(fl & MOVE_FLAGS_IGNORE_SUBSTITUTE);
}

/* Move.doesFlagEffectApply({flag, user, target, isFollowUp}) */
bool pr_move_does_flag_effect_apply_ex(pr_battle *bt, int move, unsigned flag, const pr_pokemon *user,
                                       const pr_pokemon *target, bool is_follow_up)
{
    switch (flag) {
    case MOVE_FLAGS_MAKES_CONTACT:
        if (pr_has_ability_with_attr(bt, user, PR_AC_IgnoreContactAbAttr) || pr_move_hits_substitute(bt, move, user, target))
            return false;
        break;
    case MOVE_FLAGS_IGNORE_ABILITIES:
        if (pr_has_ability_with_attr(bt, user, PR_AC_MoveAbilityBypassAbAttr)) {
            bool ignored = false;
            pr_ab_params pp = {.pokemon = user, .move = move, .flag = &ignored};
            pr_apply_ab_attrs(bt, PR_AC_MoveAbilityBypassAbAttr, &pp);
            if (ignored) return true;
        }
        return (pr_moves[move].flags & flag) && !is_follow_up;
    case MOVE_FLAGS_IGNORE_PROTECT:
        if (pr_has_ability_with_attr(bt, user, PR_AC_IgnoreProtectOnContactAbAttr)
            && pr_move_does_flag_effect_apply_ex(bt, move, MOVE_FLAGS_MAKES_CONTACT, user, NULL, false))
            return true;
        break;
    }
    return (pr_moves[move].flags & flag) != 0;
}

bool pr_move_does_flag_effect_apply(pr_battle *bt, int move, unsigned flag, const pr_pokemon *user,
                                    const pr_pokemon *target)
{
    return pr_move_does_flag_effect_apply_ex(bt, move, flag, user, target, false);
}

/* Weather.isEffectSuppressed(): reads the first SuppressWeatherEffectAbAttr straight off
 * each active Pokemon's ability (or passive), with no canApplyAbility check. */
bool pr_weather_immutable(int weather)
{
    return weather == WEATHER_TYPE_HEAVY_RAIN || weather == WEATHER_TYPE_HARSH_SUN || weather == WEATHER_TYPE_STRONG_WINDS;
}

bool pr_weather_effect_suppressed(pr_battle *bt)
{
    if (bt->weather == WEATHER_TYPE_NONE) return false;
    bool immutable = pr_weather_immutable(bt->weather);
    for (int i = 0; i < 4; i++) {
        const pr_pokemon *p = bt->field[i];
        if (!p || p->hp <= 0) continue;
        const pr_attr *a = pr_ability_first_attr(p->ability, PR_AC_SuppressWeatherEffectAbAttr);
        if (!a && p->passive) a = pr_ability_first_attr(p->passive, PR_AC_SuppressWeatherEffectAbAttr);
        if (a && (!immutable || P(SuppressWeatherEffectAbAttr, a)->affectsImmutable)) return true;
    }
    return false;
}

/* Pokemon.canApplyAbility(passive). Fusion, Transform, Mold Breaker (arena.ignoreAbilities) and
 * summonData.abilitySuppressed are not modelled; none can occur in the current state. */
bool pr_can_apply_ability(pr_battle *bt, const pr_pokemon *p, uint16_t ability)
{
    if (p->fusion_species && pr_ability_first_attr(ability, PR_AC_NoFusionAbilityAbAttr)) return false;
    const pr_ability *ab = &pr_abilities[ability];
    /* Mold Breaker, Sunsteel Strike...: the move in progress ignores ignorable abilities
     * of every Pokemon but its user */
    if (bt->ignore_abilities && bt->ignoring_effect_source != pr_battler_index(bt, p) && (ab->flags & AB_FLAG_IGNORABLE))
        return false;
    /* Gastro Acid and co. */
    if (p->ability_suppressed && !(ab->flags & AB_FLAG_UNSUPPRESSABLE)) return false;
    /* Neutralizing Gas: arena.getTag(NEUTRALIZING_GAS) suppresses the abilities of Pokemon on the field
     * (and PreSummonAbAttr abilities off it), except unsuppressable ones and the gas user's own: its
     * other ability too while it is the only source (sourceCount 1). isOnField() goes by on_field, not
     * the field slot: a Pokemon that has left the field keeps its slot until the switch swaps it. */
    const pr_arena_tag *gas = pr_arena_get_tag_on_side(bt, ARENA_TAG_TYPE_NEUTRALIZING_GAS, ARENA_TAG_SIDE_BOTH);
    if (gas && !gas->being_removed
        && ((p->on_field && !p->switch_out_status) || pr_ability_first_attr(ability, PR_AC_PreSummonAbAttr))) {
        uint16_t src_attr = PR_AC_PreLeaveFieldRemoveSuppressAbilitiesSourceAbAttr;
        bool this_suppressing = pr_ability_first_attr(ability, src_attr) != NULL;
        /* hasAbilityWithAttr(..., canApply = false) */
        bool has_suppressing = pr_ability_first_attr(p->ability, src_attr) || (p->passive && pr_ability_first_attr(p->passive, src_attr));
        bool unsuppressable = (ab->flags & AB_FLAG_UNSUPPRESSABLE) || this_suppressing
                              || (has_suppressing && !(gas->source_count > 1));
        if (!unsuppressable) return false;
    }
    if (!(p->hp > 0 || (ab->flags & AB_FLAG_BYPASS_FAINT))) return false;
    for (unsigned i = 0; i < ab->conditions.n; i++)
        if (!pr_closure_call(bt, ab->conditions.v[i], PR_AC_AbAttr, 1, p, NULL, -1)) return false;
    return true;
}

static bool call_bool(pr_battle *bt, pr_fn_id fn, uint16_t cls, const pr_pokemon *a, const pr_pokemon *b, int move)
{
    return pr_closure_call(bt, fn, cls, 0, a, b, move) != 0;
}

/* attr.getCondition(): extraCondition, except for classes that override it. */
static bool condition_ok(pr_battle *bt, const pr_attr *a, const pr_pokemon *p)
{
    switch (a->cls) {
    case PR_AC_LowHpMoveTypePowerBoostAbAttr:
        return pr_get_hp_ratio(p, false) <= 0.33;
    case PR_AC_TypeImmunityAbAttr:
    case PR_AC_TypeImmunityHealAbAttr:
    case PR_AC_TypeImmunityStatStageChangeAbAttr:
    case PR_AC_TypeImmunityAddBattlerTagAbAttr:
    case PR_AC_NonSuperEffectiveImmunityAbAttr:
        /* getCondition() returns this.condition, which no instance sets */
        return true;
    case PR_AC_PostWeatherLapseHealAbAttr:
    case PR_AC_PostWeatherLapseDamageAbAttr: {
        /* getCondition(): getWeatherCondition(...weatherTypes) */
        const pr_intlist *l = a->cls == PR_AC_PostWeatherLapseHealAbAttr
                                  ? &P(PostWeatherLapseHealAbAttr, a)->weatherTypes
                                  : &P(PostWeatherLapseDamageAbAttr, a)->weatherTypes;
        if (pr_weather_effect_suppressed(bt)) return false;
        for (unsigned i = 0; i < l->n; i++)
            if (l->v[i] == bt->weather) return true;
        return false;
    }
    }
    if (a->cond == PR_NO_FN) return true;
    return call_bool(bt, a->cond, a->cls, p, NULL, -1);
}

static bool type_in(const pr_intlist *l, int t)
{
    for (unsigned i = 0; i < l->n; i++)
        if (l->v[i] == t) return true;
    return false;
}

/* What applySingleAbAttrs does between canApply and apply outside a simulation: show the
 * ability, queue its trigger message. The turn engine keeps these phases (NULL: no engine). */
typedef struct {
    pr_engine *e; bool shown; uint16_t ability; bool passive;
    /* what the attr's canApply keeps on it for getTriggerMessage, set by its case before GATE:
     * Color Change's type, Trace's target */
    int msg_type;
    const pr_pokemon *msg_target;
} ab_ctx;
/* findModifiers(PokemonHeldItemModifier held by p): the item stealers stop at a non-empty list */
static bool has_held_items(const pr_battle *bt, const pr_pokemon *p)
{
    int side = p->is_player ? 0 : 1;
    for (int i = 0; i < bt->n_mods[side]; i++)
        if (pr_mod_is(&bt->mods[side][i], PR_MOD_HELD_ITEM) && bt->mods[side][i].pokemon_id == (int64_t)p->id) return true;
    return false;
}
static bool trigger_message(pr_battle *bt, const pr_attr *a, const pr_ab_params *pp, const ab_ctx *c, pr_msg *out);
static bool shows_ability(const pr_attr *a);
static void before_apply(pr_battle *bt, const pr_attr *a, const pr_ab_params *pp, ab_ctx *c)
{
    if (!c->e) return;
    c->shown = shows_ability(a);
    if (c->shown) pr_queue_ability_display(bt, c->e, pp->pokemon, c->passive, true);
    pr_msg m;
    if (trigger_message(bt, a, pp, c, &m)) pr_queue_msg(bt, c->e, m);
}
/* canApply(params) false: skip this attr; true: the ability applies, apply follows */
#define GATE(can) do { if (!(can)) return 0; before_apply(bt, a, pp, c); } while (0)

/* PostSummonMessageAbAttr's messageFunc(pokemon) and PostSummonUnnamedMessageAbAttr's message, by the
 * ability they belong to (init-abilities.ts) */
static pr_msg post_summon_message(const pr_battle *bt, const pr_attr *a, uint16_t ability, const pr_pokemon *p)
{
    if (a->cls == PR_AC_PostSummonUnnamedMessageAbAttr)                /* Cloud Nine, Air Lock */
        return pr_msg0(ability == ABILITY_ID_CLOUD_NINE || ability == ABILITY_ID_AIR_LOCK
                           ? "abilityTriggers:weatherEffectDisappeared" : NULL);
    const char *key = NULL;
    int stat = -1;
    switch (ability) {
    case ABILITY_ID_PRESSURE: key = "abilityTriggers:postSummonPressure"; break;
    case ABILITY_ID_MOLD_BREAKER: key = "abilityTriggers:postSummonMoldBreaker"; break;
    case ABILITY_ID_ANTICIPATION: key = "abilityTriggers:postSummonAnticipation"; break;
    case ABILITY_ID_TURBOBLAZE: key = "abilityTriggers:postSummonTurboblaze"; break;
    case ABILITY_ID_TERAVOLT: key = "abilityTriggers:postSummonTeravolt"; break;
    case ABILITY_ID_DARK_AURA: key = "abilityTriggers:postSummonDarkAura"; break;
    case ABILITY_ID_FAIRY_AURA: key = "abilityTriggers:postSummonFairyAura"; break;
    case ABILITY_ID_AURA_BREAK: key = "abilityTriggers:postSummonAuraBreak"; break;
    case ABILITY_ID_AS_ONE_GLASTRIER: key = "abilityTriggers:postSummonAsOneGlastrier"; break;
    case ABILITY_ID_AS_ONE_SPECTRIER: key = "abilityTriggers:postSummonAsOneSpectrier"; break;
    /* (its messageFunc also marks zeroToHeroFormChangeMessage, which its condition reads: not modelled) */
    case ABILITY_ID_ZERO_TO_HERO: key = "abilityTriggers:postSummonZeroToHero"; break;
    case ABILITY_ID_VESSEL_OF_RUIN: key = "abilityTriggers:postSummonVesselOfRuin"; stat = STAT_SPATK; break;
    case ABILITY_ID_SWORD_OF_RUIN: key = "abilityTriggers:postSummonSwordOfRuin"; stat = STAT_DEF; break;
    case ABILITY_ID_TABLETS_OF_RUIN: key = "abilityTriggers:postSummonTabletsOfRuin"; stat = STAT_ATK; break;
    case ABILITY_ID_BEADS_OF_RUIN: key = "abilityTriggers:postSummonBeadsOfRuin"; stat = STAT_SPDEF; break;
    default: return pr_msg0(NULL);                                       /* a messageFunc not listed here */
    }
    if (stat >= 0) return pr_msg2(key, pr_arg_pokemon(bt, "pokemonNameWithAffix", p), pr_arg_stat("statName", stat));
    return pr_msg_p(bt, key, p);
}

/* canApply, then apply, for one attr: 1 applied, 0 canApply was false, -1 no C port. */
static int apply_one(pr_battle *bt, const pr_attr *a, pr_ab_params *pp, ab_ctx *c)
{
    const pr_pokemon *p = pp->pokemon, *o = pp->opponent;
    int move = pp->move;
    switch (a->cls) {
    case PR_AC_PokemonTypeChangeAbAttr: {
        if (move <= MOVE_ID_NONE || move >= PR_MOVE_COUNT) return -1;
        if (p->terastallized || move == MOVE_ID_STRUGGLE
            || pr_move_has_attr((uint16_t)move, PR_AC_CallMoveAttr)
            || pr_move_has_attr((uint16_t)move, PR_AC_NaturePowerAttr)) return 0;
        int type = pr_get_move_type(bt, p, move);
        int8_t types[PR_MAX_TYPES];
        int n = pr_get_types(p, types, 0);
        bool same = true;
        for (int i = 0; i < n; i++) if (types[i] != type) same = false;
        if (same) return 0;
        if (!pp->simulated && !c->e) return -1;
        c->msg_type = type; /* Source canApply sets the trigger message's moveType. */
        GATE(true);
        if (!pp->simulated) {
            int8_t one = (int8_t)pr_get_move_type(bt, p, move);
            pr_set_summon_types(bt, (pr_pokemon *)p, &one, 1);
        }
        return 1;
    }
    /* ---- stats ---- */
    case PR_AC_AddSecondStrikeAbAttr:
        if (move <= MOVE_ID_NONE || move >= PR_MOVE_COUNT) return -1;
        GATE(pr_move_can_be_multi_strike_enhanced_ex(bt, move, p, true, o));
        *pp->value += 1;
        return 1;
    case PR_AC_MaxMultiHitAbAttr:
        GATE(true);
        *pp->value = 0; /* Source replaces the rolled bucket, not the final hit count. */
        return 1;
    case PR_AC_ChangeMovePriorityInBracketAbAttr: {
        const pr_attr_ChangeMovePriorityInBracketAbAttr *x = P(ChangeMovePriorityInBracketAbAttr, a);
        GATE(call_bool(bt, x->moveFunc, a->cls, p, NULL, move));
        *pp->value = x->newModifier;
        return 1;
    }
    case PR_AC_BypassSpeedChanceAbAttr: {
        if (pp->simulated) return 0;
        if (!c->e) return -1;
        int battler = pr_battler_index(bt, p);
        int chosen = battler >= 0 ? bt->commands[battler].move : MOVE_ID_NONE;
        bool damage = chosen > MOVE_ID_NONE && chosen < PR_MOVE_COUNT
            && (pr_moves[chosen].category == MOVE_CATEGORY_PHYSICAL || pr_moves[chosen].category == MOVE_CATEGORY_SPECIAL);
        /* Source draws before checking the move category or existing bypass tag. */
        GATE(pr_battle_rand_seed_int(bt, 100, 0) < P(BypassSpeedChanceAbAttr, a)->chance
             && damage && pr_can_add_tag(bt, (pr_pokemon *)p, BATTLER_TAG_TYPE_BYPASS_SPEED));
        pr_add_tag(bt, c->e, (pr_pokemon *)p, BATTLER_TAG_TYPE_BYPASS_SPEED, 0, 0, -1);
        return 1;
    }
    case PR_AC_PreventBypassSpeedChanceAbAttr: {
        int battler = pr_battler_index(bt, p);
        if (battler < 0 || bt->commands[battler].command != COMMAND_FIGHT) return 0;
        int chosen = bt->commands[battler].move;
        if (chosen <= MOVE_ID_NONE || chosen >= PR_MOVE_COUNT) return -1;
        GATE(call_bool(bt, P(PreventBypassSpeedChanceAbAttr, a)->condition, a->cls, p, NULL, chosen));
        *pp->flag = false;
        return 1;
    }
    case PR_AC_WonderSkinAbAttr:
        GATE(move >= 0 && move < PR_MOVE_COUNT
             && pr_moves[move].category == MOVE_CATEGORY_STATUS && *pp->value >= 50);
        *pp->value = 50; /* Source also modifies the holder during simulation. */
        return 1;
    case PR_AC_StatMultiplierAbAttr: {
        const pr_attr_StatMultiplierAbAttr *x = P(StatMultiplierAbAttr, a);
        GATE(pp->stat == x->stat
             && (x->condition == PR_NO_FN || call_bool(bt, x->condition, a->cls, p, NULL, move < 0 ? 0 : move)));
        *pp->value *= x->multiplier;
        return 1;
    }
    case PR_AC_AllyStatMultiplierAbAttr: {
        const pr_attr_AllyStatMultiplierAbAttr *x = P(AllyStatMultiplierAbAttr, a);
        GATE(pp->stat == x->stat && !(pp->ignore_ability && x->ignorable));
        *pp->value *= x->multiplier;
        return 1;
    }
    case PR_AC_FieldMultiplyStatAbAttr: {
        const pr_attr_FieldMultiplyStatAbAttr *x = P(FieldMultiplyStatAbAttr, a);
        bool ok = x->canStack;
        if (!ok && !*pp->flag && x->stat == pp->stat) {
            /* target.getAbilityAttrs("FieldMultiplyStatAbAttr").every(attr => attr.stat !== stat) */
            ok = true;
            uint16_t abs[2] = {o->ability, o->passive};
            for (int k = 0; k < 2; k++) {
                if (k == 1 && !o->passive) continue;
                if (!pr_can_apply_ability(bt, o, abs[k])) continue;
                const pr_ability *ab = &pr_abilities[abs[k]];
                for (unsigned i = 0; i < ab->nattr; i++) {
                    const pr_attr *t = &pr_attrs[ab->attr0 + i];
                    if (t->cls == PR_AC_FieldMultiplyStatAbAttr && P(FieldMultiplyStatAbAttr, t)->stat == pp->stat) ok = false;
                }
            }
        }
        GATE(ok);
        *pp->value *= x->multiplier;
        *pp->flag = true;
        return 1;
    }
    case PR_AC_IgnoreOpponentStatStagesAbAttr:
        GATE(type_in(&P(IgnoreOpponentStatStagesAbAttr, a)->stats, pp->stat));
        *pp->flag = true;
        return 1;

    /* ---- move type and power ---- */
    case PR_AC_MoveTypeChangeAbAttr: {
        const pr_attr_MoveTypeChangeAbAttr *x = P(MoveTypeChangeAbAttr, a);
        GATE(call_bool(bt, x->condition, a->cls, p, o, move));
        *pp->holder = x->newType;
        return 1;
    }
    case PR_AC_MovePowerBoostAbAttr:
    case PR_AC_MoveTypePowerBoostAbAttr:
    case PR_AC_LowHpMoveTypePowerBoostAbAttr: {
        /* skipDuringMovesetGen only matters while the AI generates movesets */
        const pr_attr_MovePowerBoostAbAttr *x = P(MovePowerBoostAbAttr, a);
        pr_fn_id cond; double mult;
        if (a->cls == PR_AC_MovePowerBoostAbAttr) { cond = x->condition; mult = x->powerMultiplier; }
        else if (a->cls == PR_AC_MoveTypePowerBoostAbAttr) {
            cond = P(MoveTypePowerBoostAbAttr, a)->condition; mult = P(MoveTypePowerBoostAbAttr, a)->powerMultiplier;
        } else {
            cond = P(LowHpMoveTypePowerBoostAbAttr, a)->condition; mult = P(LowHpMoveTypePowerBoostAbAttr, a)->powerMultiplier;
        }
        GATE(call_bool(bt, cond, a->cls, p, o, move));
        *pp->value *= mult;
        return 1;
    }
    case PR_AC_AllyMoveCategoryPowerBoostAbAttr: {
        /* FieldMovePowerBoostAbAttr: canApply is always true, apply checks the condition */
        const pr_attr_AllyMoveCategoryPowerBoostAbAttr *x = P(AllyMoveCategoryPowerBoostAbAttr, a);
        GATE(true);
        if (call_bool(bt, x->condition, a->cls, p, o, move)) *pp->value *= x->powerMultiplier;
        return 1;
    }
    case PR_AC_UserFieldMoveTypePowerBoostAbAttr: {
        const pr_attr_UserFieldMoveTypePowerBoostAbAttr *x = P(UserFieldMoveTypePowerBoostAbAttr, a);
        GATE(true);
        if (call_bool(bt, x->condition, a->cls, p, o, move)) *pp->value *= x->powerMultiplier;
        return 1;
    }
    case PR_AC_PreAttackWeatherOverrideAbAttr: {
        int w = P(PreAttackWeatherOverrideAbAttr, a)->weatherType;
        GATE(*pp->holder != w);
        *pp->holder = w;
        return 1;
    }
    case PR_AC_WeightMultiplierAbAttr:
        GATE(true);
        *pp->value *= P(WeightMultiplierAbAttr, a)->multiplier;
        return 1;
    case PR_AC_UngroundedAbAttr:
        GATE(true);
        *pp->flag = true;
        return 1;

    /* ---- effectiveness and immunities ---- */
    case PR_AC_IgnoreTypeImmunityAbAttr: {
        const pr_attr_IgnoreTypeImmunityAbAttr *x = P(IgnoreTypeImmunityAbAttr, a);
        GATE(!*pp->flag && x->defenderType == pp->defender_type && type_in(&x->allowedMoveTypes, pp->move_type));
        *pp->flag = true;
        return 1;
    }
    case PR_AC_TypeImmunityHealAbAttr:
    case PR_AC_TypeImmunityStatStageChangeAbAttr:
    case PR_AC_TypeImmunityAddBattlerTagAbAttr: {
        int immune;
        switch (a->cls) {
        case PR_AC_TypeImmunityHealAbAttr: immune = P(TypeImmunityHealAbAttr, a)->immuneType; break;
        case PR_AC_TypeImmunityStatStageChangeAbAttr: immune = P(TypeImmunityStatStageChangeAbAttr, a)->immuneType; break;
        default: immune = P(TypeImmunityAddBattlerTagAbAttr, a)->immuneType; break;
        }
        int tgt = pr_moves[move].target;
        GATE(tgt != MOVE_TARGET_BOTH_SIDES && tgt != MOVE_TARGET_ENEMY_SIDE && tgt != MOVE_TARGET_USER_SIDE
             && o != p && pr_get_move_type(bt, o, move) == immune);
        *pp->value = 0;
        if (a->cls == PR_AC_TypeImmunityHealAbAttr) {
            if (p->hp < pr_get_stat(p, STAT_HP, true) && !pp->simulated) {
                /* unshiftNew("PokemonHealPhase", toDmgValue(maxHp / 4), {message}); the damage-only
                 * test has no engine and records it as deferred */
                if (c->e)
                    pr_unshift_heal_msg(bt, c->e, pr_battler_index(bt, p), to_dmg_value(pr_get_stat(p, STAT_HP, true) / 4.0),
                                        PR_HEAL_MESSAGE, pr_msg2("abilityTriggers:typeImmunityHeal",
                                        pr_arg_pokemon(bt, "pokemonNameWithAffix", p), pr_arg_ability("abilityName", c->ability)));
                else pr_note_deferred(bt, a->cls);
                *pp->flag = true;
            }
        } else {
            *pp->flag = true;
            if (pp->simulated) return 1;
            if (!c->e) {
                pr_note_deferred(bt, a->cls);                /* StatStageChangePhase / addTag */
            } else if (a->cls == PR_AC_TypeImmunityStatStageChangeAbAttr) {
                /* Motor Drive, Sap Sipper, Lightning Rod...: the stat change, from the Pokemon itself */
                const pr_attr_TypeImmunityStatStageChangeAbAttr *x = P(TypeImmunityStatStageChangeAbAttr, a);
                const int32_t st = x->stat;
                pr_unshift_stat_stage_change(bt, c->e, pr_battler_index(bt, p), &st, 1, x->stages, (pr_pokemon *)p);
            } else {
                /* Flash Fire: addTag(tagType, turnCount, undefined, pokemon.id) */
                const pr_attr_TypeImmunityAddBattlerTagAbAttr *x = P(TypeImmunityAddBattlerTagAbAttr, a);
                pr_add_tag(bt, c->e, (pr_pokemon *)p, x->tagType, x->turnCount, 0, (int64_t)p->id);
            }
        }
        return 1;
    }
    case PR_AC_NonSuperEffectiveImmunityAbAttr:
        GATE(pr_move_is_attack(move) && *pp->value < 2);
        *pp->flag = true;
        *pp->value = 0;
        return 1;
    case PR_AC_MoveImmunityAbAttr:
    case PR_AC_MoveImmunityStatStageChangeAbAttr: {
        pr_fn_id cond = a->cls == PR_AC_MoveImmunityAbAttr ? P(MoveImmunityAbAttr, a)->immuneCondition
                                                           : P(MoveImmunityStatStageChangeAbAttr, a)->immuneCondition;
        GATE(!(a->cls == PR_AC_MoveImmunityStatStageChangeAbAttr && pp->simulated) && !*pp->flag
             && call_bool(bt, cond, a->cls, p, o, move));
        *pp->flag = true;
        if (a->cls == PR_AC_MoveImmunityStatStageChangeAbAttr) pr_note_deferred(bt, a->cls);
        return 1;
    }
    case PR_AC_ChangeMovePriorityAbAttr: {
        /* Prankster, Gale Wings, Triage: moveFunc(pokemon, move) */
        const pr_attr_ChangeMovePriorityAbAttr *x = P(ChangeMovePriorityAbAttr, a);
        GATE(call_bool(bt, x->moveFunc, a->cls, p, NULL, move));
        *pp->value += x->changeAmount;
        return 1;
    }
    case PR_AC_FieldPriorityMoveImmunityAbAttr: {
        int tgt = pr_moves[move].target;
        bool ally_target = tgt == MOVE_TARGET_USER || tgt == MOVE_TARGET_NEAR_ALLY || tgt == MOVE_TARGET_ALLY
                           || tgt == MOVE_TARGET_USER_OR_NEAR_ALLY || tgt == MOVE_TARGET_USER_AND_ALLIES
                           || tgt == MOVE_TARGET_USER_SIDE;
        GATE(!*pp->flag && pr_move_get_priority(bt, move, o) > 0 && !ally_target && pr_is_opponent(bt, o, p));
        *pp->flag = true;
        return 1;
    }
    case PR_AC_FullHpResistTypeAbAttr:
        GATE(!pr_move_has_attr((uint16_t)move, PR_AC_FixedDamageAttr) && p->hp >= pr_get_stat(p, STAT_HP, true)
             && *pp->value > 0.5);
        *pp->value = 0.5;
        ((pr_pokemon *)p)->turn_move_effectiveness = 0.5; /* pokemon.turnData.moveEffectiveness */
        return 1;

    /* ---- damage ---- */
    case PR_AC_StabBoostAbAttr:
        GATE(*pp->value > 1);
        *pp->value += 0.5;
        return 1;
    case PR_AC_MultCritAbAttr:
        GATE(*pp->value > 1);
        *pp->value *= P(MultCritAbAttr, a)->multAmount;
        return 1;
    /* ---- status effects ---- */
    case PR_AC_StatusEffectImmunityAbAttr: {                            /* Limber, Insomnia, Water Veil... */
        const pr_attr_StatusEffectImmunityAbAttr *x = P(StatusEffectImmunityAbAttr, a);
        GATE(!*pp->flag && ((x->immuneEffects.n == 0 && pp->effect != STATUS_EFFECT_FAINT)
                            || type_in(&x->immuneEffects, pp->effect)));
        *pp->flag = true;
        return 1;
    }
    case PR_AC_UserFieldStatusEffectImmunityAbAttr: {                   /* Sweet Veil, Pastel Veil */
        const pr_attr_UserFieldStatusEffectImmunityAbAttr *x = P(UserFieldStatusEffectImmunityAbAttr, a);
        GATE((!*pp->flag && x->immuneEffects.n == 0 && pp->effect != STATUS_EFFECT_FAINT)
             || type_in(&x->immuneEffects, pp->effect));
        *pp->flag = true;
        return 1;
    }
    case PR_AC_ConditionalUserFieldStatusEffectImmunityAbAttr: {        /* Flower Veil */
        const pr_attr_ConditionalUserFieldStatusEffectImmunityAbAttr *x = P(ConditionalUserFieldStatusEffectImmunityAbAttr, a);
        GATE(!*pp->flag && call_bool(bt, x->condition, a->cls, pp->target, o, -1)
             && ((!*pp->flag && x->immuneEffects.n == 0 && pp->effect != STATUS_EFFECT_FAINT)
                 || type_in(&x->immuneEffects, pp->effect)));
        *pp->flag = true;
        return 1;
    }
    case PR_AC_IgnoreTypeStatusEffectImmunityAbAttr: {                  /* Corrosion */
        const pr_attr_IgnoreTypeStatusEffectImmunityAbAttr *x = P(IgnoreTypeStatusEffectImmunityAbAttr, a);
        GATE(!*pp->flag && type_in(&x->statusEffect, pp->effect) && type_in(&x->defenderType, pp->defender_type));
        *pp->flag = true;
        return 1;
    }
    case PR_AC_SynchronizeStatusAbAttr: {
        if (!pp->simulated && !c->e) return -1;
        int ef = pp->effect;
        GATE(o && (ef == STATUS_EFFECT_BURN || ef == STATUS_EFFECT_PARALYSIS || ef == STATUS_EFFECT_POISON
                   || ef == STATUS_EFFECT_TOXIC));
        if (!pp->simulated) pr_try_set_status(bt, c->e, (pr_pokemon *)o, ef, (pr_pokemon *)p, -1, false, true);
        return 1;
    }
    case PR_AC_PostSummonHealStatusAbAttr: {                            /* Limber, Insomnia, Water Veil... */
        const pr_attr_PostSummonHealStatusAbAttr *x = P(PostSummonHealStatusAbAttr, a);
        if (!(p->status && (x->immuneEffects.n == 0 || type_in(&x->immuneEffects, p->status)))) return 0;
        if (!c->e) return -1;                         /* apply ignores simulated: it needs the queue */
        GATE(true);                                   /* its trigger message names the status cured last time */
        pr_status_healed_set(c->ability, p->status);
        pr_reset_status_phase(bt, c->e, (pr_pokemon *)p, false);
        return 1;
    }
    case PR_AC_PostSummonTransformAbAttr: {                            /* Imposter */
        const pr_pokemon *targets[2]; int n = 0;
        for (int k = 0; k < 4; k++) {
            const pr_pokemon *q = bt->field[k];
            if (q && q->is_player != p->is_player && q->hp > 0 && q->on_field && !q->switch_out_status
                && pr_can_transform_into(p, q)) targets[n++] = q;
        }
        if (!n) return 0;
        /* canApply selects once; apply uses that target without another RNG draw. */
        const pr_pokemon *target = targets[pr_battle_rand_seed_int(bt, n, 0)];
        if (!c->e) return -1;
        GATE(true);
        int ph = pr_phase_new(c->e, PH_TRANSFORM, (int8_t)pr_battler_index(bt, p));
        if (ph < 0) return -1;
        c->e->pool[ph].transform_target = (int8_t)pr_battler_index(bt, target);
        c->e->pool[ph].transform_sound = true;
        pr_phase_unshift(bt, c->e, ph);
        return 1;
    }
    case PR_AC_PostVictoryStatStageChangeAbAttr: {
        /* Moxie/Neigh: fixed changes; Beast Boost/Eelevate: raw highest stat. */
        GATE(true);
        if (pp->simulated) return 1;
        if (!c->e) return -1;
        const pr_stat_change_source *changes = &P(PostVictoryStatStageChangeAbAttr, a)->changes;
        int8_t st[7], sg[7];
        int n = 0;
        if (changes->fn != PR_NO_FN) {
            pr_fn_id fn = changes->fn;
            const char *hash = fn < PR_CLOSURE_COUNT ? pr_closures_meta[fn].hash : "";
            if (strcmp(hash, "6297309b617b")) {
                if (fn < PR_CLOSURE_COUNT) pr_closure_misses[fn]++;
                return -1;
            }
            static const int stats[] = {STAT_ATK, STAT_DEF, STAT_SPATK, STAT_SPDEF, STAT_SPD};
            double highest = 0;
            int best = -1;
            for (unsigned i = 0; i < sizeof(stats) / sizeof(stats[0]); i++) {
                double value = pr_get_stat(p, stats[i], false);
                if (value > highest) { highest = value; best = stats[i]; }
            }
            if (best < 0) return -1;
            pr_closure_hits[fn]++;
            st[0] = (int8_t)best; sg[0] = 1; n = 1;
        } else {
            if (changes->fixed.n > 7) return -1;
            for (unsigned i = 0; i < changes->fixed.n; i++) {
                st[n] = changes->fixed.v[i].stat;
                sg[n++] = changes->fixed.v[i].stages;
            }
        }
        return pr_unshift_stat_stage_change_ex(bt, c->e, pr_battler_index(bt, p), st, sg, n,
                                               (pr_pokemon *)p, 0) < 0 ? -1 : 1;
    }
    case PR_AC_CopyFaintedAllyAbilityAbAttr: {
        const pr_pokemon *victim = pp->victim;
        if (!victim || victim->ability >= PR_ABILITY_COUNT) return -1;
        GATE(p->is_player == victim->is_player
             && !(pr_abilities[victim->ability].flags & AB_FLAG_UNCOPIABLE));
        if (pp->simulated) return 1;
        if (!c->e) return -1;
        pr_set_temp_ability(bt, (pr_pokemon *)p, victim->ability);
        /* Source reads the victim again after the copied ability's gain effects. */
        pr_queue_msg(bt, c->e, pr_msg2("abilityTriggers:copyFaintedAllyAbility",
                                     pr_arg_pokemon(bt, "pokemonNameWithAffix", victim),
                                     pr_arg_ability("abilityName", victim->ability)));
        return 1;
    }
    case PR_AC_PostKnockOutStatStageChangeAbAttr: {                     /* Soul-Heart */
        const pr_attr_PostKnockOutStatStageChangeAbAttr *x = P(PostKnockOutStatStageChangeAbAttr, a);
        GATE(true);
        if (pp->simulated) return 1;
        if (!c->e) return -1;
        int8_t st = (int8_t)x->stat, sg = (int8_t)x->stages;
        return pr_unshift_stat_stage_change_ex(bt, c->e, pr_battler_index(bt, p), &st, &sg, 1,
                                               (pr_pokemon *)p, 0) < 0 ? -1 : 1;
    }
    case PR_AC_PreLeaveFieldClearWeatherAbAttr: {
        int weather = P(PreLeaveFieldClearWeatherAbAttr, a)->weatherType;
        bool another = false;
        for (int b = 0; b < 4 && !another; b++) {
            const pr_pokemon *q = bt->field[b];
            /* getField(true) checks legal/faint eligibility, not isOnField(). */
            if (!q || q == p || q->hp <= 0) continue;
            for (int passive = 0; passive < 2 && !another; passive++) {
                int ability = passive ? q->passive : q->ability;
                if (!ability || !pr_can_apply_ability(bt, q, ability)) continue;
                const pr_ability *ab = &pr_abilities[ability];
                for (unsigned i = 0; i < ab->nattr; i++) {
                    const pr_attr *other = &pr_attrs[ab->attr0 + i];
                    if (other->cls == PR_AC_PreLeaveFieldClearWeatherAbAttr
                        && P(PreLeaveFieldClearWeatherAbAttr, other)->weatherType == weather) another = true;
                }
            }
        }
        GATE(bt->weather == weather && !another);
        if (pp->simulated) return 1;
        if (!c->e) return -1;
        pr_try_set_weather(bt, c->e, WEATHER_TYPE_NONE, NULL);
        return 1;
    }
    /* ---- coming in: PostSummonActivateAbilityPhase ---- */
    case PR_AC_PostSummonMessageAbAttr:                                 /* Pressure, Mold Breaker, Anticipation... */
    case PR_AC_PostSummonUnnamedMessageAbAttr:                          /* Air Lock, Cloud Nine */
        GATE(true);
        if (pp->simulated) return 1;
        if (!c->e) return -1;
        pr_queue_msg(bt, c->e, post_summon_message(bt, a, c->ability, p));
        return 1;
    case PR_AC_PostSummonAddBattlerTagAbAttr: {                         /* Protosynthesis, Quark Drive... */
        const pr_attr_PostSummonAddBattlerTagAbAttr *x = P(PostSummonAddBattlerTagAbAttr, a);
        if (!c->e) return -1;
        GATE(pr_can_add_tag(bt, (pr_pokemon *)p, x->tagType));
        if (!pp->simulated) pr_add_tag(bt, c->e, (pr_pokemon *)p, x->tagType, x->turnCount, 0, -1);
        return 1;
    }
    case PR_AC_PostSummonStatStageChangeAbAttr:                         /* Intimidate, Intrepid Sword... */
    case PR_AC_PostSummonStatStageChangeOnArenaAbAttr: {                /* Wind Rider with a Tailwind */
        const pr_statchanges *ch;
        bool self_target, intimidate;
        if (a->cls == PR_AC_PostSummonStatStageChangeAbAttr) {
            const pr_attr_PostSummonStatStageChangeAbAttr *x = P(PostSummonStatStageChangeAbAttr, a);
            ch = &x->changes, self_target = x->selfTarget, intimidate = x->intimidate;
            if (!c->e) return -1;
            GATE(true);
        } else {
            const pr_attr_PostSummonStatStageChangeOnArenaAbAttr *x = P(PostSummonStatStageChangeOnArenaAbAttr, a);
            ch = &x->changes, self_target = x->selfTarget, intimidate = x->intimidate;
            if (!c->e) return -1;
            GATE(pr_arena_get_tag_on_side(bt, x->arenaTagType, p->is_player ? ARENA_TAG_SIDE_PLAYER : ARENA_TAG_SIDE_ENEMY));
        }
        if (pp->simulated) return 1;
        int8_t st[7], sg[7];
        int n = 0;
        for (unsigned i = 0; i < ch->n && n < 7; i++, n++) { st[n] = ch->v[i].stat; sg[n] = ch->v[i].stages; }
        if (self_target) {
            pr_unshift_stat_stage_change_ex(bt, c->e, pr_battler_index(bt, p), st, sg, n, (pr_pokemon *)p, 0);
            return 1;
        }
        /* each opponent in speed order: Intimidate checks its immunities and reactions (Rattled,
         * Guard Dog) and a Substitute */
        pr_speed_iter it;
        pr_pokemon *opp;
        pr_speed_iter_init(bt, &it, p->is_player ? ARENA_TAG_SIDE_ENEMY : ARENA_TAG_SIDE_PLAYER);
        while ((opp = pr_speed_iter_next(bt, &it))) {
            bool cancelled = false;
            if (intimidate) {
                pr_ab_params ip = {.pokemon = opp, .move = -1, .flag = &cancelled};
                pr_apply_ab_attrs(bt, PR_AC_IntimidateImmunityAbAttr, &ip);
                pr_apply_ab_attrs(bt, PR_AC_PostIntimidateStatStageChangeAbAttr, &ip);
                if (pr_get_battler_tag(opp, BATTLER_TAG_TYPE_SUBSTITUTE)) cancelled = true;
            }
            if (!cancelled)
                pr_unshift_stat_stage_change_ex(bt, c->e, pr_battler_index(bt, opp), st, sg, n, (pr_pokemon *)p, 0);
        }
        return 1;
    }
    case PR_AC_PostIntimidateStatStageChangeAbAttr: {                   /* Rattled, Guard Dog */
        const pr_attr_PostIntimidateStatStageChangeAbAttr *x = P(PostIntimidateStatStageChangeAbAttr, a);
        if (!c->e) return -1;
        GATE(true);
        if (!pp->simulated) {
            int8_t st[7], sg[7];
            int n = 0;
            for (unsigned i = 0; i < x->changes.n && n < 7; i++, n++) { st[n] = x->changes.v[i].stat; sg[n] = x->changes.v[i].stages; }
            pr_unshift_stat_stage_change_ex(bt, c->e, pr_battler_index(bt, p), st, sg, n, (pr_pokemon *)p, 0);
        }
        *pp->flag = x->overwrites;
        return 1;
    }
    case PR_AC_PostSummonWeatherChangeAbAttr: {                         /* Drizzle, Drought, the primal weathers */
        int w = P(PostSummonWeatherChangeAbAttr, a)->weatherType;
        if (!c->e) return -1;
        bool replaceable = w == WEATHER_TYPE_HEAVY_RAIN || w == WEATHER_TYPE_HARSH_SUN || w == WEATHER_TYPE_STRONG_WINDS
                           || w == WEATHER_TYPE_NONE || !pr_weather_immutable(bt->weather);
        GATE(replaceable && bt->weather != w);                          /* canSetWeather */
        if (!pp->simulated) pr_try_set_weather(bt, c->e, w, p);
        return 1;
    }
    case PR_AC_PostSummonTerrainChangeAbAttr: {                         /* the Surges */
        int t = P(PostSummonTerrainChangeAbAttr, a)->terrainType;
        if (!c->e) return -1;
        GATE(bt->terrain != t);                                         /* canSetTerrain */
        if (!pp->simulated) pr_try_set_terrain(bt, c->e, t, false, p);
        return 1;
    }
    case PR_AC_PostSummonAddArenaTagAbAttr: {                           /* Neutralizing Gas */
        const pr_attr_PostSummonAddArenaTagAbAttr *x = P(PostSummonAddArenaTagAbAttr, a);
        if (!c->e) return -1;
        GATE(true);
        if (!pp->simulated) pr_arena_add_tag(bt, c->e, x->tagType, x->turnCount, -1, p->id, ARENA_TAG_SIDE_BOTH, false);
        return 1;
    }
    case PR_AC_PostSummonRemoveArenaTagAbAttr: {                        /* Screen Cleaner */
        const pr_attr_PostSummonRemoveArenaTagAbAttr *x = P(PostSummonRemoveArenaTagAbAttr, a);
        if (!c->e) return -1;
        bool any = false;                                               /* arena.hasTag(tagTypes): any side */
        for (int i = 0; i < bt->n_arena_tags; i++) any |= type_in(&x->arenaTags, bt->arena_tags[i].type);
        GATE(any);
        if (!pp->simulated) pr_arena_remove_tags_on_side(bt, c->e, x->arenaTags.v, x->arenaTags.n, ARENA_TAG_SIDE_BOTH, false);
        return 1;
    }
    case PR_AC_PostFaintUnsuppressedWeatherFormChangeAbAttr: {
        bool any = false;
        for (int k = 0; k < 4; k++) {
            const pr_pokemon *q = bt->field[k];
            if (!q || q->hp <= 0 || !q->on_field || q->switch_out_status) continue;
            any |= (q->species == SPECIES_ID_CASTFORM && pr_has_ability_active(bt, q, ABILITY_ID_FORECAST))
                || (q->species == SPECIES_ID_CHERRIM && pr_has_ability_active(bt, q, ABILITY_ID_FLOWER_GIFT));
        }
        GATE(any);
        if (!pp->simulated) pr_weather_form_changes(bt, false, NULL);
        return 1;
    }
    case PR_AC_PostSummonWeatherSuppressedFormChangeAbAttr: {           /* Air Lock, Cloud Nine */
        /* getPokemonWithWeatherBasedForms(): Castform with Forecast, Cherrim with Flower Gift */
        const pr_pokemon *forms[4];
        int nf = 0;
        for (int k = 0; k < 4; k++) {
            const pr_pokemon *q = bt->field[k];
            if (!q || !(q->hp > 0 && q->on_field && !q->switch_out_status)) continue;   /* getField(true) */
            if ((q->species == SPECIES_ID_CASTFORM && pr_has_ability_active(bt, q, ABILITY_ID_FORECAST))
                || (q->species == SPECIES_ID_CHERRIM && pr_has_ability_active(bt, q, ABILITY_ID_FLOWER_GIFT)))
                forms[nf++] = q;
        }
        if (!c->e) return -1;
        GATE(nf > 0);
        if (!pp->simulated)                                             /* triggerWeatherBasedFormChangesToNormal */
            for (int k = 0; k < nf; k++) pr_form_change(bt, forms[k], PR_FCT_SpeciesFormChangeRevertWeatherFormTrigger);
        return 1;
    }
    case PR_AC_DownloadAbAttr: {                                        /* Download */
        /* the opponents' average effective Defense and Sp. Def, each rounded (Math.round) */
        double def = 0, spdef = 0;
        int count = 0;
        for (int k = 0; k < 4; k++) {
            const pr_pokemon *q = bt->field[k];
            if (!q || q->is_player == p->is_player || !(q->hp > 0 && q->on_field && !q->switch_out_status)) continue;
            count++;
            def += pr_get_effective_stat(bt, q, STAT_DEF, NULL, -1, false, false);
            spdef += pr_get_effective_stat(bt, q, STAT_SPDEF, NULL, -1, false, false);
        }
        if (!c->e) return -1;
        def = count ? floor(def / count + 0.5) : NAN;
        spdef = count ? floor(spdef / count + 0.5) : NAN;
        GATE(def > 0 && spdef > 0);
        if (!pp->simulated) {
            int8_t st = def < spdef ? STAT_ATK : STAT_SPATK, sg = 1;
            pr_unshift_stat_stage_change_ex(bt, c->e, pr_battler_index(bt, p), &st, &sg, 1, (pr_pokemon *)p, 0);
        }
        return 1;
    }
    case PR_AC_ForewarnAbAttr: {                                        /* Forewarn */
        const pr_pokemon *opps[4];
        int no = 0;
        bool any_moves = false;
        for (int k = 0; k < 4; k++) {
            const pr_pokemon *q = bt->field[k];
            if (!q || q->is_player == p->is_player || !(q->hp > 0 && q->on_field && !q->switch_out_status)) continue;
            opps[no++] = q;
            any_moves |= PR_N_MOVES(q) > 0;
        }
        if (!c->e) return -1;
        GATE(any_moves);
        if (pp->simulated) return 1;
        /* the strongest moves by getForewarnPower; one of the ties on the battle RNG */
        int max_power = 0, n_at_max = 0;
        uint16_t at_max[4 * 4];                                         /* movesAtMaxPower's moves */
        for (int k = 0; k < no; k++)
            for (int j = 0; j < PR_N_MOVES(opps[k]); j++) {
                int mv = PR_MOVES(opps[k])[j];
                if (!mv) continue;
                int power = pr_moves[mv].category == MOVE_CATEGORY_STATUS ? 1
                            : pr_move_has_attr((uint16_t)mv, PR_AC_OneHitKOAttr) ? 150
                            : pr_move_has_attr((uint16_t)mv, PR_AC_CounterDamageAttr) ? 120
                            : pr_moves[mv].power == -1 ? 80 : pr_moves[mv].power;
                if (power < max_power) continue;
                if (power == max_power) { at_max[n_at_max++] = (uint16_t)mv; continue; }
                max_power = power;
                n_at_max = 1;
                at_max[0] = (uint16_t)mv;
            }
        if (n_at_max == 0) return 1;
        int chosen = at_max[pr_battle_rand_seed_int(bt, n_at_max, 0)];
        pr_queue_msg(bt, c->e, pr_msg2("abilityTriggers:forewarn", pr_arg_pokemon(bt, "pokemonNameWithAffix", p),
                                       pr_arg_move("moveName", chosen)));
        return 1;
    }
    case PR_AC_FriskAbAttr: {                                           /* Frisk: a message per opponent */
        if (!c->e) return -1;
        GATE(true);
        if (pp->simulated) return 1;
        pr_speed_iter it;
        pr_speed_iter_init(bt, &it, p->is_player ? ARENA_TAG_SIDE_ENEMY : ARENA_TAG_SIDE_PLAYER);
        for (pr_pokemon *q; (q = pr_speed_iter_next(bt, &it));)
            pr_queue_msg(bt, c->e, pr_msg3("abilityTriggers:frisk", pr_arg_pokemon(bt, "pokemonNameWithAffix", p),
                                           pr_arg_pokemon_name(bt, "opponentName", q),
                                           pr_arg_ability("opponentAbilityName", q->ability)));
        return 1;
    }
    /* ---- leaving the field: SwitchSummonPhase.switchAndSummon ---- */
    case PR_AC_PreSwitchOutResetStatusAbAttr:                           /* Natural Cure */
        GATE(p->status != 0);
        if (pp->simulated) return 1;
        if (!c->e) return -1;
        pr_reset_status_phase(bt, c->e, (pr_pokemon *)p, true);
        return 1;
    case PR_AC_PreSwitchOutHealAbAttr: {                                /* Regenerator */
        double max = pr_get_stat(p, STAT_HP, true);
        GATE(p->hp < max);                                              /* !isFullHp() */
        if (!pp->simulated) ((pr_pokemon *)p)->hp += (int32_t)fmin(to_dmg_value(max * 0.33), max - p->hp);
        return 1;
    }
    case PR_AC_IncreasePpUsedAbAttr:                                    /* Pressure (shows nothing) */
        GATE(p->is_player != o->is_player);                             /* isOpponent */
        *pp->value += P(IncreasePpUsedAbAttr, a)->ppIncrease;
        return 1;
    case PR_AC_PostSummonUserFieldRemoveStatusEffectAbAttr: {           /* Pastel Veil */
        const pr_attr_PostSummonUserFieldRemoveStatusEffectAbAttr *x = P(PostSummonUserFieldRemoveStatusEffectAbAttr, a);
        /* getPlayerField() / getEnemyField(): the first one (two in doubles) of the party, allowed in battle */
        int side = p->is_player ? 0 : 1, n = bt->double_battle ? 2 : 1;
        bool any = false;
        for (int k = 0; k < n; k++) {
            const pr_pokemon *q = bt->n_party[side] ? (k < bt->n_party[side] ? bt->party[side][k] : NULL)
                                                    : bt->field[side * 2 + k];
            if (q && q->hp > 0) any = true;
        }
        if (!c->e && !pp->simulated) return -1;
        GATE(any);
        if (pp->simulated) return 1;
        /* getAlliesGenerator(): its side in speed order; the heal text, then resetStatus(false) as a phase */
        pr_speed_iter it;
        pr_pokemon *q;
        pr_speed_iter_init(bt, &it, p->is_player ? ARENA_TAG_SIDE_PLAYER : ARENA_TAG_SIDE_ENEMY);
        while ((q = pr_speed_iter_next(bt, &it)))
            if (q->status && type_in(&x->statusEffect, q->status)) {
                pr_queue_msg(bt, c->e, pr_status_msg(bt, q->status, PR_ST_HEAL, q));   /* getStatusEffectHealText */
                pr_reset_status_phase(bt, c->e, q, false);
            }
        return 1;
    }
    case PR_AC_PostSummonCopyAbilityAbAttr: {                           /* Trace */
        /* getOpponents().filter(t => t.getAbility().copiable || t.getAbility().id === WONDER_GUARD) */
        const pr_pokemon *tg[2];
        int n = 0;
        for (int k = 0; k < 2; k++) {
            const pr_pokemon *t = bt->field[(p->is_player ? 2 : 0) + k];
            if (!t || t->hp <= 0 || !t->on_field || t->switch_out_status) continue;
            if (!(pr_abilities[t->ability].flags & AB_FLAG_UNCOPIABLE) || t->ability == ABILITY_ID_WONDER_GUARD) tg[n++] = t;
        }
        if (n == 0) return 0;
        if (!c->e && !pp->simulated) return -1;
        /* two to pick from (doubles), not simulated: randSeedInt(targets.length), the global generator */
        int pick = 0;
        if (n > 1 && !pp->simulated) {
            if (!bt->rng_live) { pr_note_unported(bt, PR_FEAT_GLOBAL_RNG); return -1; }
            pick = pr_rng_seed_int(&bt->rng, n, 0);
        }
        c->msg_target = tg[pick];                                       /* this.target */
        GATE(true);
        if (!pp->simulated) pr_set_temp_ability(bt, (pr_pokemon *)p, tg[pick]->ability);   /* revealAbility: UI */
        return 1;
    }
    case PR_AC_PostDamageForceSwitchAbAttr: {                           /* Wimp Out, Emergency Exit */
        if (!c->e) return -1;
        int r = pr_wimp_out_can_apply(bt, p, o, pp->damage, P(PostDamageForceSwitchAbAttr, a)->hpRatio);
        if (r < 0) return -1;
        GATE(r);
        pr_wimp_out_apply(bt, c->e, (pr_pokemon *)p);                   /* the simulated flag isn't read */
        return 1;
    }
    case PR_AC_PostSummonRemoveBattlerTagAbAttr: {                      /* Oblivious, Own Tempo */
        const pr_attr_PostSummonRemoveBattlerTagAbAttr *x = P(PostSummonRemoveBattlerTagAbAttr, a);
        for (unsigned i = 0; i < x->immuneTags.n; i++)
            if (pr_get_battler_tag(p, x->immuneTags.v[i])) return -1;       /* removeTag: onRemove */
        return 0;
    }
    case PR_AC_MoodyAbAttr: {
        GATE(true);
        if (pp->simulated) return 1;
        if (!c->e) return -1;
        /* Source EFFECTIVE_STATS order; choices use pre-queued stage values. */
        static const int32_t stats[] = {STAT_ATK, STAT_DEF, STAT_SPATK, STAT_SPDEF, STAT_SPD};
        int32_t raise[5], lower[5];
        int nr = 0, nl = 0;
        for (int j = 0; j < 5; j++) {
            if (p->stat_stages[stats[j] - 1] < 6) raise[nr++] = stats[j];
            if (p->stat_stages[stats[j] - 1] > -6) lower[nl++] = stats[j];
        }
        if (nr) {
            int32_t chosen = raise[pr_battle_rand_seed_int(bt, nr, 0)];
            int keep = 0;
            for (int j = 0; j < nl; j++) if (lower[j] != chosen) lower[keep++] = lower[j];
            nl = keep;
            if (pr_unshift_stat_stage_change(bt, c->e, pr_battler_index(bt, p), &chosen, 1, 2, (pr_pokemon *)p) < 0)
                c->e->overflow = true;
        }
        if (nl) {
            int32_t chosen = lower[pr_battle_rand_seed_int(bt, nl, 0)];
            if (pr_unshift_stat_stage_change(bt, c->e, pr_battler_index(bt, p), &chosen, 1, -1, (pr_pokemon *)p) < 0)
                c->e->overflow = true;
        }
        return 1;
    }
    case PR_AC_SpeedBoostAbAttr: {
        /* Source canApply has no minimum turn-count condition. The existing
         * ability gate supplies faint/suppression checks before this leaf. */
        GATE(pp->simulated || (!p->switched_in_this_turn && !p->failed_run_away));
        if (!c->e) return -1;                           /* queue requires an actual turn engine */
        static const int32_t stat = STAT_SPD;
        if (pr_unshift_stat_stage_change(bt, c->e, pr_battler_index(bt, p), &stat, 1, 1, (pr_pokemon *)p) < 0)
            c->e->overflow = true;
        return 1;
    }
    case PR_AC_PostTurnStatusHealAbAttr: {                              /* Poison Heal */
        const pr_attr_PostTurnStatusHealAbAttr *x = P(PostTurnStatusHealAbAttr, a);
        if (!(p->status && type_in(&x->effects, p->status) && p->hp < pr_get_stat(p, STAT_HP, true))) return 0;
        if (!c->e) return -1;
        GATE(true);
        pr_unshift_heal_msg(bt, c->e, pr_battler_index(bt, p), to_dmg_value(pr_get_stat(p, STAT_HP, true) / 8.0), PR_HEAL_MESSAGE,
                            pr_msg2("abilityTriggers:poisonHeal", pr_arg_pokemon(bt, "pokemonName", p),
                                    pr_arg_ability("abilityName", c->ability)));
        return 1;
    }
    /* ---- weather and terrain: WeatherEffectPhase, and arena.trySetWeather / trySetTerrain ---- */
    case PR_AC_SuppressWeatherEffectAbAttr:                             /* Cloud Nine, Air Lock */
        GATE(bt->weather != WEATHER_TYPE_NONE && !*pp->flag
             && (P(SuppressWeatherEffectAbAttr, a)->affectsImmutable || pr_weather_immutable(bt->weather)));
        *pp->flag = true;
        return 1;
    case PR_AC_BlockWeatherDamageAttr: {                                /* Overcoat, Sand Veil, Snow Cloak... */
        const pr_attr_BlockWeatherDamageAttr *x = P(BlockWeatherDamageAttr, a);
        GATE(bt->weather != WEATHER_TYPE_NONE && !*pp->flag
             && (x->weatherTypes.n == 0 || type_in(&x->weatherTypes, bt->weather)));
        *pp->flag = true;
        return 1;
    }
    case PR_AC_PostWeatherLapseHealAbAttr: {                            /* Rain Dish, Ice Body, Dry Skin */
        const pr_attr_PostWeatherLapseHealAbAttr *x = P(PostWeatherLapseHealAbAttr, a);
        if (!c->e) return -1;
        GATE(p->hp < pr_get_stat(p, STAT_HP, true));                     /* !isFullHp() */
        pr_unshift_heal_msg(bt, c->e, pr_battler_index(bt, p),
                            to_dmg_value(pr_get_stat(p, STAT_HP, true) / (16.0 / x->healFactor)), PR_HEAL_MESSAGE,
                            pr_msg2("abilityTriggers:postWeatherLapseHeal", pr_arg_pokemon(bt, "pokemonNameWithAffix", p),
                                    pr_arg_ability("abilityName", c->ability)));
        return 1;
    }
    case PR_AC_PostWeatherLapseDamageAbAttr: {                          /* Solar Power, Dry Skin */
        const pr_attr_PostWeatherLapseDamageAbAttr *x = P(PostWeatherLapseDamageAbAttr, a);
        if (!c->e) return -1;
        GATE(!pr_has_ability_with_attr(bt, p, PR_AC_BlockNonDirectDamageAbAttr));
        /* abilityName: (passive ? getPassiveAbility() : getAbility()).name */
        pr_queue_msg(bt, c->e, pr_msg2("abilityTriggers:postWeatherLapseDamage",
                                       pr_arg_pokemon(bt, "pokemonNameWithAffix", p), pr_arg_ability("abilityName", c->ability)));
        pr_damage_and_update(bt, c->e, (pr_pokemon *)p,
                             to_dmg_value(pr_get_stat(p, STAT_HP, true) / (16.0 / x->damageFactor)),
                             HIT_RESULT_INDIRECT, false, NULL);
        return 1;
    }
    case PR_AC_PostSummonFormChangeByWeatherAbAttr: {
        int wanted = p->species == SPECIES_ID_CASTFORM ? ABILITY_ID_FORECAST
                   : p->species == SPECIES_ID_CHERRIM ? ABILITY_ID_FLOWER_GIFT : -1;
        GATE(wanted >= 0 && !p->ability_suppressed && !pr_weather_effect_suppressed(bt)
             && pr_has_ability_active(bt, p, wanted));
        int form = bt->weather == WEATHER_TYPE_SUNNY || bt->weather == WEATHER_TYPE_HARSH_SUN ? 1
                 : p->species == SPECIES_ID_CASTFORM && (bt->weather == WEATHER_TYPE_RAIN || bt->weather == WEATHER_TYPE_HEAVY_RAIN) ? 2
                 : p->species == SPECIES_ID_CASTFORM && (bt->weather == WEATHER_TYPE_HAIL || bt->weather == WEATHER_TYPE_SNOW) ? 3 : -1;
        GATE(form >= 0 && form != p->form);
        if (!pp->simulated) pr_form_change(bt, p, PR_FCT_SpeciesFormChangeWeatherTrigger);
        return 1;
    }
    case PR_AC_PostSummonFormChangeAbAttr:
    case PR_AC_PostTurnFormChangeAbAttr:
    case PR_AC_PostBattleInitFormChangeAbAttr:
    case PR_AC_PostVictoryFormChangeAbAttr:
    case PR_AC_PostFaintFormChangeAbAttr: {
        pr_fn_id fn = a->cls == PR_AC_PostSummonFormChangeAbAttr
            ? P(PostSummonFormChangeAbAttr, a)->formFunc
            : a->cls == PR_AC_PostTurnFormChangeAbAttr
                ? P(PostTurnFormChangeAbAttr, a)->formFunc
                : a->cls == PR_AC_PostVictoryFormChangeAbAttr
                    ? P(PostVictoryFormChangeAbAttr, a)->formFunc
                    : a->cls == PR_AC_PostFaintFormChangeAbAttr
                        ? P(PostFaintFormChangeAbAttr, a)->formFunc
                        : P(PostBattleInitFormChangeAbAttr, a)->formFunc;
        GATE(pr_closure_call(bt, fn, a->cls, p->form, p, NULL, -1) != p->form);
        if (a->cls == PR_AC_PostBattleInitFormChangeAbAttr) GATE(!pp->simulated);
        if (!pp->simulated) pr_form_change(bt, p, PR_FCT_SpeciesFormChangeAbilityTrigger);
        return 1;
    }
    case PR_AC_PostWeatherChangeFormChangeAbAttr: {                     /* Forecast, Flower Gift */
        const pr_attr_PostWeatherChangeFormChangeAbAttr *x = P(PostWeatherChangeFormChangeAbAttr, a);
        GATE((p->species == SPECIES_ID_CASTFORM && x->ability == ABILITY_ID_FORECAST)
             || (p->species == SPECIES_ID_CHERRIM && x->ability == ABILITY_ID_FLOWER_GIFT));
        if (!pp->simulated) {
            bool revert = bt->weather != WEATHER_TYPE_NONE && type_in(&x->formRevertingWeathers, bt->weather);
            pr_weather_form_changes(bt, revert, NULL);
        }
        return 1;
    }
    case PR_AC_IceFaceFormChangeAbAttr:                                 /* Ice Face in Hail or Snow */
        GATE(p->form == P(IceFaceFormChangeAbAttr, a)->formIndex
             && (pp->field_type == WEATHER_TYPE_HAIL || pp->field_type == WEATHER_TYPE_SNOW));
        if (!pp->simulated) pr_form_change(bt, p, PR_FCT_SpeciesFormChangeAbilityTrigger);
        return 1;
    case PR_AC_PostWeatherChangeAddBattlerTagAbAttr: {                  /* Protosynthesis */
        const pr_attr_PostWeatherChangeAddBattlerTagAbAttr *x = P(PostWeatherChangeAddBattlerTagAbAttr, a);
        if (!type_in(&x->weatherTypes, pp->field_type)) return 0;
        if (!c->e) return -1;
        GATE(pr_can_add_tag(bt, (pr_pokemon *)p, x->tagType));
        if (!pp->simulated) pr_add_tag(bt, c->e, (pr_pokemon *)p, x->tagType, x->turnCount, 0, -1);
        return 1;
    }
    case PR_AC_PostTerrainChangeAddBattlerTagAttr: {                    /* Quark Drive */
        const pr_attr_PostTerrainChangeAddBattlerTagAttr *x = P(PostTerrainChangeAddBattlerTagAttr, a);
        if (!type_in(&x->terrainTypes, pp->field_type)) return 0;
        if (!c->e) return -1;
        GATE(pr_can_add_tag(bt, (pr_pokemon *)p, x->tagType));
        if (!pp->simulated) pr_add_tag(bt, c->e, (pr_pokemon *)p, x->tagType, x->turnCount, 0, -1);
        return 1;
    }
    case PR_AC_TerrainEventTypeChangeAbAttr:                            /* Mimicry: its type change isn't ported */
        return p->terastallized ? 0 : -1;
    /* ---- on-hit abilities: PostDefend (the Pokemon hit), PostReceiveCrit, PostAttack (the attacker).
     * The ones whose effect adds a tag, weather, terrain, an ability or an item run their canApply
     * (random rolls included, so the RNG stays in step) and report themselves when it holds. */
    case PR_AC_PostDefendStatStageChangeAbAttr: {                       /* Justified, Weak Armor, Gooey... */
        const pr_attr_PostDefendStatStageChangeAbAttr *x = P(PostDefendStatStageChangeAbAttr, a);
        if (!c->e) return -1;
        GATE(call_bool(bt, x->condition, a->cls, p, o, move));
        int8_t st = (int8_t)x->stat, sg = (int8_t)x->stages;
        if (x->allOthers) {
            /* getOpponents() (active on the field, field order), then getAlly() */
            for (int k = 0; k < 4; k++) {
                pr_pokemon *other = bt->field[k];
                if (other && other->is_player != p->is_player && other->hp > 0 && other->on_field && !other->switch_out_status)
                    pr_unshift_stat_stage_change_ex(bt, c->e, k, &st, &sg, 1, (pr_pokemon *)p, 0);
            }
            const pr_pokemon *ally = pr_get_ally(bt, p);
            if (ally) pr_unshift_stat_stage_change_ex(bt, c->e, pr_battler_index(bt, ally), &st, &sg, 1, (pr_pokemon *)p, 0);
        } else {
            pr_unshift_stat_stage_change_ex(bt, c->e, pr_battler_index(bt, x->selfTarget ? p : o), &st, &sg, 1,
                                            (pr_pokemon *)p, 0);
        }
        return 1;
    }
    case PR_AC_PostDefendHpGatedStatStageChangeAbAttr: {                /* Berserk, Anger Shell */
        const pr_attr_PostDefendHpGatedStatStageChangeAbAttr *x = P(PostDefendHpGatedStatStageChangeAbAttr, a);
        if (!c->e) return -1;
        double threshold = to_dmg_value(pr_get_stat(p, STAT_HP, true) * x->hpGate);
        GATE(pr_moves[move].category != MOVE_CATEGORY_STATUS && p->hp <= threshold && p->hp + pp->damage > threshold);
        int8_t st[7], sg[7];
        int n = 0;
        for (unsigned i = 0; i < x->changes.n && n < 7; i++, n++) { st[n] = x->changes.v[i].stat; sg[n] = x->changes.v[i].stages; }
        pr_unshift_stat_stage_change_ex(bt, c->e, pr_battler_index(bt, x->selfTarget ? p : o), st, sg, n, (pr_pokemon *)p, 0);
        return 1;
    }
    case PR_AC_FlinchStatStageChangeAbAttr: {                           /* Steadfast */
        const pr_attr_FlinchStatStageChangeAbAttr *x = P(FlinchStatStageChangeAbAttr, a);
        if (!c->e) return -1;
        GATE(true);
        int8_t st[7], sg[7];
        int n = 0;
        for (unsigned i = 0; i < x->changes.n && n < 7; i++, n++) { st[n] = x->changes.v[i].stat; sg[n] = x->changes.v[i].stages; }
        pr_unshift_stat_stage_change_ex(bt, c->e, pr_battler_index(bt, p), st, sg, n, (pr_pokemon *)p, 0);
        return 1;
    }
    case PR_AC_PostReceiveCritStatStageChangeAbAttr: {                  /* Anger Point */
        const pr_attr_PostReceiveCritStatStageChangeAbAttr *x = P(PostReceiveCritStatStageChangeAbAttr, a);
        if (!c->e) return -1;
        GATE(true);
        int8_t st = (int8_t)x->stat, sg = (int8_t)x->stages;
        pr_unshift_stat_stage_change_ex(bt, c->e, pr_battler_index(bt, p), &st, &sg, 1, (pr_pokemon *)p, 0);
        return 1;
    }
    case PR_AC_EffectSporeAbAttr:
    case PR_AC_PostDefendApplyStatusEffectAbAttr: {                     /* Static, Flame Body, Poison Point */
        const pr_attr_PostDefendApplyStatusEffectAbAttr *x = P(PostDefendApplyStatusEffectAbAttr, a);
        if (!c->e) return -1;
        /* Effect Spore: not against Grass types or Overcoat, before anything is rolled */
        if (a->cls == PR_AC_EffectSporeAbAttr
            && (pr_is_of_type(o, POKEMON_TYPE_GRASS) || pr_has_ability_active(bt, o, ABILITY_ID_OVERCOAT)))
            return 0;
        /* canApply picks the effect first (a roll when there are several), then contact, the
         * attacker's status, the chance roll and canSetStatus */
        int ef = x->effects.n == 1 ? x->effects.v[0] : x->effects.v[pr_battle_rand_seed_int(bt, x->effects.n, 0)];
        bool can = (x->contactRequired ? pr_move_does_flag_effect_apply(bt, move, MOVE_FLAGS_MAKES_CONTACT, o, p)
                                       : pr_moves[move].category != MOVE_CATEGORY_STATUS)
                   && !o->status && (x->chance == -1 || pr_battle_rand_seed_int(bt, 100, 0) < x->chance)
                   && pr_can_set_status(bt, c->e, (pr_pokemon *)o, ef, true, false, p, false);
        GATE(can);
        /* apply picks again, independently of the effect canApply checked */
        ef = x->effects.n == 1 ? x->effects.v[0] : x->effects.v[pr_battle_rand_seed_int(bt, x->effects.n, 0)];
        pr_try_set_status(bt, c->e, (pr_pokemon *)o, ef, (pr_pokemon *)p, -1, false, true);
        return 1;
    }
    case PR_AC_PostDefendContactDamageAbAttr: {                         /* Rough Skin, Iron Barbs */
        const pr_attr_PostDefendContactDamageAbAttr *x = P(PostDefendContactDamageAbAttr, a);
        if (!c->e) return -1;
        GATE(!pp->simulated && pr_move_does_flag_effect_apply(bt, move, MOVE_FLAGS_MAKES_CONTACT, o, p)
             && !pr_has_ability_with_attr(bt, o, PR_AC_BlockNonDirectDamageAbAttr));
        pr_pokemon *w = (pr_pokemon *)o;
        pr_damage_and_update(bt, c->e, w, to_dmg_value(pr_get_stat(w, STAT_HP, true) * (1.0 / x->damageRatio)),
                             HIT_RESULT_INDIRECT, false, NULL);
        w->damage_taken += (int32_t)to_dmg_value(pr_get_stat(w, STAT_HP, true) * (1.0 / x->damageRatio));
        return 1;
    }
    case PR_AC_PostDefendTypeChangeAbAttr: {                            /* Color Change */
        if (pp->hit_result >= HIT_RESULT_NO_EFFECT || p->terastallized
            || pr_move_has_attr((uint16_t)move, PR_AC_TypelessAttr) || o->hits_left > 1)
            return 0;
        int type = pr_get_move_type(bt, o, move);
        int8_t ty[PR_MAX_TYPES];
        int n = pr_get_types(p, ty, PR_TYPES_ORIGINAL_IF_STELLAR);
        for (int i = 0; i < n; i++)
            if (ty[i] == type) return 0;
        c->msg_type = type;                                       /* this.type */
        GATE(true);
        if (!pp->simulated) {
            pr_pokemon *w = (pr_pokemon *)p;
            int8_t one = (int8_t)type;                            /* summonData.types = [type] */
            pr_set_summon_types(bt, w, &one, 1);
        }
        return 1;
    }
    case PR_AC_PostDefendMoveDisableAbAttr: {                           /* Cursed Body */
        const pr_attr_PostDefendMoveDisableAbAttr *x = P(PostDefendMoveDisableAbAttr, a);
        if (!c->e) return -1;
        GATE(!pr_get_battler_tag(o, BATTLER_TAG_TYPE_DISABLED)
             && pr_move_does_flag_effect_apply(bt, move, MOVE_FLAGS_MAKES_CONTACT, o, p)
             && (x->chance == -1 || pr_battle_rand_seed_int(bt, 100, 0) < x->chance));
        if (!pp->simulated)                                     /* the attacker's last move, 4 turns */
            pr_add_tag(bt, c->e, (pr_pokemon *)o, BATTLER_TAG_TYPE_DISABLED, 4, 0, (int64_t)p->id);
        return 1;
    }
    case PR_AC_PostDefendContactApplyTagChanceAbAttr: {                 /* Cute Charm */
        const pr_attr_PostDefendContactApplyTagChanceAbAttr *x = P(PostDefendContactApplyTagChanceAbAttr, a);
        if (!c->e) return -1;
        GATE(pr_move_does_flag_effect_apply(bt, move, MOVE_FLAGS_MAKES_CONTACT, o, p)
             && pr_battle_rand_seed_int(bt, 100, 0) < x->chance
             && pr_can_add_tag(bt, (pr_pokemon *)o, x->tagType));
        if (!pp->simulated)                                     /* addTag(tagType, turnCount, move.id, pokemon.id) */
            pr_add_tag(bt, c->e, (pr_pokemon *)o, x->tagType, 0, move, (int64_t)p->id);
        return 1;
    }
    case PR_AC_PostDefendPerishSongAbAttr: {                            /* Perish Body */
        if (!c->e) return -1;
        GATE(pr_move_does_flag_effect_apply(bt, move, MOVE_FLAGS_MAKES_CONTACT, o, p)
             && !pr_get_battler_tag(o, BATTLER_TAG_TYPE_PERISH_SONG));
        if (!pp->simulated) {                                   /* the attacker, then the Pokemon */
            int turns = P(PostDefendPerishSongAbAttr, a)->turns;
            pr_add_tag(bt, c->e, (pr_pokemon *)o, BATTLER_TAG_TYPE_PERISH_SONG, turns, 0, -1);
            pr_add_tag(bt, c->e, (pr_pokemon *)p, BATTLER_TAG_TYPE_PERISH_SONG, turns, 0, -1);
        }
        return 1;
    }
    case PR_AC_PostTurnResetStatusAbAttr: {                             /* Shed Skin, Hydration, Healer */
        /* the Pokemon, or its ally; any status but FAINT: the heal text, then resetStatus(false) */
        if (!c->e) return -1;
        const pr_pokemon *tg = P(PostTurnResetStatusAbAttr, a)->allyTarget ? pr_get_ally(bt, p) : p;
        GATE(tg && tg->status && tg->status != STATUS_EFFECT_FAINT);
        if (!pp->simulated) {
            pr_queue_msg(bt, c->e, pr_status_msg(bt, tg->status, PR_ST_HEAL, tg));   /* getStatusEffectHealText */
            pr_reset_status_phase(bt, c->e, (pr_pokemon *)tg, false);
        }
        return 1;
    }
    case PR_AC_PostDefendAbilitySwapAbAttr: {                           /* Wandering Spirit */
        /* contact, and the attacker's ability swappable (neither uncopiable nor unreplaceable) */
        GATE(pr_move_does_flag_effect_apply(bt, move, MOVE_FLAGS_MAKES_CONTACT, o, p)
             && !(pr_abilities[o->ability].flags & (AB_FLAG_UNCOPIABLE | AB_FLAG_UNREPLACEABLE)));
        if (!pp->simulated) {
            uint16_t theirs = o->ability;
            pr_set_temp_ability(bt, (pr_pokemon *)o, p->ability);
            pr_set_temp_ability(bt, (pr_pokemon *)p, theirs);
        }
        return 1;
    }
    case PR_AC_PostDefendAbilityGiveAbAttr: {                           /* Mummy, Lingering Aroma */
        /* contact, the attacker's ability suppressable and not one of these itself */
        GATE(pr_move_does_flag_effect_apply(bt, move, MOVE_FLAGS_MAKES_CONTACT, o, p)
             && !(pr_abilities[o->ability].flags & AB_FLAG_UNSUPPRESSABLE)
             && !pr_ability_first_attr(o->ability, PR_AC_PostDefendAbilityGiveAbAttr));
        if (!pp->simulated) pr_set_temp_ability(bt, (pr_pokemon *)o, (uint16_t)P(PostDefendAbilityGiveAbAttr, a)->ability);
        return 1;
    }
    case PR_AC_PostDefendWeatherChangeAbAttr: {                         /* Sand Spit */
        const pr_attr_PostDefendWeatherChangeAbAttr *x = P(PostDefendWeatherChangeAbAttr, a);
        if (!c->e) return -1;
        GATE(call_bool(bt, x->condition, a->cls, p, o, move) && !pr_weather_immutable(bt->weather)
             && bt->weather != x->weatherType);
        if (!pp->simulated) pr_try_set_weather(bt, c->e, x->weatherType, p);
        return 1;
    }
    case PR_AC_PostDefendApplyArenaTrapTagAbAttr: {                     /* Toxic Debris */
        /* the tag on either side (getTag looks at both) must have a layer left */
        const pr_attr_PostDefendApplyArenaTrapTagAbAttr *x = P(PostDefendApplyArenaTrapTagAbAttr, a);
        if (!c->e) return -1;
        const pr_arena_tag *tag = pr_arena_get_tag_on_side(bt, x->arenaTagType, ARENA_TAG_SIDE_BOTH);
        GATE(call_bool(bt, x->condition, a->cls, p, o, move) && (!tag || tag->layers < pr_arena_max_layers(tag->type)));
        if (!pp->simulated)
            pr_arena_add_tag(bt, c->e, x->arenaTagType, 0, -1, p->id,
                             p->is_player ? ARENA_TAG_SIDE_ENEMY : ARENA_TAG_SIDE_PLAYER, false);
        return 1;
    }
    case PR_AC_PostDefendApplyBattlerTagAbAttr: {                       /* Wind Power, Electromorphosis */
        const pr_attr_PostDefendApplyBattlerTagAbAttr *x = P(PostDefendApplyBattlerTagAbAttr, a);
        if (!c->e) return -1;
        GATE(call_bool(bt, x->condition, a->cls, p, o, move));
        if (!pr_get_battler_tag(p, x->tagType) && !pp->simulated) {
            pr_add_tag(bt, c->e, (pr_pokemon *)p, x->tagType, 0, 0, p->id);   /* addTag(tagType, undefined, undefined, id) */
            pr_queue_msg(bt, c->e, pr_msg2("abilityTriggers:windPowerCharged", pr_arg_pokemon(bt, "pokemonName", p),
                                           pr_arg_move("moveName", move)));
        }
        return 1;
    }
    case PR_AC_PostDefendTerrainChangeAbAttr: {                         /* Seed Sower: with its animation */
        const pr_attr_PostDefendTerrainChangeAbAttr *x = P(PostDefendTerrainChangeAbAttr, a);
        if (!c->e) return -1;
        GATE(pp->hit_result < HIT_RESULT_NO_EFFECT && bt->terrain != x->terrainType);
        if (!pp->simulated) pr_try_set_terrain(bt, c->e, x->terrainType, false, p);
        return 1;
    }
    case PR_AC_ReverseDrainAbAttr: {                                    /* Liquid Ooze */
        /* canApply applies the attacker's BlockNonDirectDamageAbAttr (Magic Guard) as it is simulated */
        bool cancelled = false;
        pr_ab_params bp = {.pokemon = o, .move = -1, .flag = &cancelled, .simulated = pp->simulated};
        pr_apply_ab_attrs(bt, PR_AC_BlockNonDirectDamageAbAttr, &bp);
        return !cancelled && pr_move_has_attr((uint16_t)move, PR_AC_HitHealAttr) ? -1 : 0;
    }
    case PR_AC_PostDefendStealHeldItemAbAttr:                           /* Pickpocket */
        if (pp->simulated || pp->hit_result >= HIT_RESULT_NO_EFFECT
            || !call_bool(bt, P(PostDefendStealHeldItemAbAttr, a)->condition, a->cls, p, o, move))
            return 0;
        return has_held_items(bt, o) ? -1 : 0;                  /* the roll for the item, the transfer */
    case PR_AC_PostAttackApplyStatusEffectAbAttr:
    case PR_AC_PostAttackContactApplyStatusEffectAbAttr: {              /* Poison Touch, Toxic Chain */
        const pr_attr_PostAttackApplyStatusEffectAbAttr *x = P(PostAttackApplyStatusEffectAbAttr, a);
        if (!c->e) return -1;
        if (!call_bool(bt, x->attackCondition, a->cls, p, o, move)) return 0;
        if (!pp->simulated
            && !(!pr_has_ability_with_attr(bt, o, PR_AC_IgnoreMoveEffectsAbAttr) && p != o
                 && (!x->contactRequired || pr_move_does_flag_effect_apply(bt, move, MOVE_FLAGS_MAKES_CONTACT, p, o))
                 && pr_battle_rand_seed_int(bt, 100, 0) < x->chance && !p->status))   /* the attacker's own status */
            return 0;
        int ef = x->effects.n == 1 ? x->effects.v[0] : x->effects.v[pr_battle_rand_seed_int(bt, x->effects.n, 0)];
        GATE(pp->simulated || pr_can_set_status(bt, c->e, (pr_pokemon *)o, ef, true, false, p, false));
        ef = x->effects.n == 1 ? x->effects.v[0] : x->effects.v[pr_battle_rand_seed_int(bt, x->effects.n, 0)];
        pr_try_set_status(bt, c->e, (pr_pokemon *)o, ef, (pr_pokemon *)p, -1, false, true);
        return 1;
    }
    case PR_AC_PostAttackApplyBattlerTagAbAttr: {                       /* Stench */
        const pr_attr_PostAttackApplyBattlerTagAbAttr *x = P(PostAttackApplyBattlerTagAbAttr, a);
        if (!c->e) return -1;
        /* the contact check and the chance take (user = the one hit, target = the attacker) */
        if (!call_bool(bt, x->attackCondition, a->cls, p, o, move) || pr_has_ability_with_attr(bt, o, PR_AC_IgnoreMoveEffectsAbAttr)
            || p == o || (x->contactRequired && !pr_move_does_flag_effect_apply(bt, move, MOVE_FLAGS_MAKES_CONTACT, o, p)))
            return 0;
        int roll = pr_battle_rand_seed_int(bt, 100, 0);
        if (!(roll < pr_closure_call(bt, x->chance, a->cls, 0, o, p, move)) || p->status) return 0;
        return -1;                                              /* addTag(FLINCHED) */
    }
    case PR_AC_PostAttackStealHeldItemAbAttr:                           /* Magician */
        if (!call_bool(bt, P(PostAttackStealHeldItemAbAttr, a)->attackCondition, a->cls, p, o, move) || pp->simulated
            || pp->hit_result >= HIT_RESULT_NO_EFFECT)
            return 0;
        return has_held_items(bt, o) ? -1 : 0;
    /* ---- battler tag immunities (addTag): BaseBattlerTagImmunityAbAttr's canApply */
    case PR_AC_BattlerTagImmunityAbAttr: {                              /* Oblivious, Own Tempo, Inner Focus... */
        const pr_attr_BattlerTagImmunityAbAttr *x = P(BattlerTagImmunityAbAttr, a);
        GATE(!*pp->flag && type_in(&x->immuneTagTypes, pp->tag_type));
        *pp->flag = true;
        return 1;
    }
    case PR_AC_UserFieldBattlerTagImmunityAbAttr: {                     /* Sweet Veil, Aroma Veil */
        const pr_attr_UserFieldBattlerTagImmunityAbAttr *x = P(UserFieldBattlerTagImmunityAbAttr, a);
        GATE(!*pp->flag && type_in(&x->immuneTagTypes, pp->tag_type));
        *pp->flag = true;
        return 1;
    }
    case PR_AC_ConditionalUserFieldBattlerTagImmunityAbAttr: {          /* Flower Veil */
        const pr_attr_ConditionalUserFieldBattlerTagImmunityAbAttr *x = P(ConditionalUserFieldBattlerTagImmunityAbAttr, a);
        GATE(!*pp->flag && type_in(&x->immuneTagTypes, pp->tag_type) && call_bool(bt, x->condition, a->cls, pp->target, o, -1));
        *pp->flag = true;
        return 1;
    }
    case PR_AC_ReduceStatusEffectDurationAbAttr:                        /* Early Bird */
        GATE(pp->effect == P(ReduceStatusEffectDurationAbAttr, a)->statusEffect);
        *pp->value -= 1;
        return 1;
    case PR_AC_BlockStatusDamageAbAttr: {                               /* Poison Heal */
        const pr_attr_BlockStatusDamageAbAttr *x = P(BlockStatusDamageAbAttr, a);
        GATE(!*pp->flag && p->status && type_in(&x->effects, p->status));
        *pp->flag = true;
        return 1;
    }
    case PR_AC_ReduceBurnDamageAbAttr:                                  /* Heatproof */
        GATE(true);
        *pp->value = to_dmg_value(*pp->value * P(ReduceBurnDamageAbAttr, a)->multiplier);
        return 1;
    case PR_AC_PostDancingMoveAbAttr: {
        if (!c->e) return -1;
        GATE(move >= 0 && (pr_moves[move].flags & MOVE_FLAGS_DANCE_MOVE)
             && !pr_find_battler_tag_class(p, PR_TAGCLASS_SEMI_INVULNERABLE));
        int8_t targets[4];
        int n = 0;
        if (pr_move_is_multi_target(move)) {
            bool multiple;
            n = pr_get_move_targets(bt, p, move, -1, targets, &multiple);
        } else if (!strcmp(pr_moves[move].kind, "SelfStatusMove")
                   || !strcmp(pr_moves[move].kind, "ChargingSelfStatusMove")) {
            /* ChargeMove mixin retains SelfStatusMove.is upstream. */
            targets[n++] = (int8_t)pr_battler_index(bt, p);
        } else {
            bool targeted = false;
            int b = pr_battler_index(bt, p);
            for (int i = 0; i < pp->n_move_targets; i++) targeted |= pp->move_targets[i] == b;
            if (o && (p->is_player != o->is_player || targeted))
                targets[n++] = (int8_t)pr_battler_index(bt, o);
            else for (int i = 0; i < pp->n_move_targets && i < 4; i++) targets[n++] = pp->move_targets[i];
            if (bt->double_battle && n > 0) {
                pr_pokemon *first = bt->field[targets[0]];
                pr_pokemon *ally = bt->field[targets[0] ^ 1];
                if (first && first->hp <= 0 && first->is_player != p->is_player
                    && ally && ally->hp > 0 && ally->on_field && !ally->switch_out_status) {
                    targets[0] ^= 1; n = 1;
                }
            }
        }
        pr_unshift_move_phase(bt, c->e, (pr_pokemon *)p, move, -1, targets, n,
                              MOVE_USE_MODE_INDIRECT, MOVE_PHASE_TIMING_MODIFIER_FIRST);
        return 1;
    }
    case PR_AC_PostItemLostApplyBattlerTagAbAttr: {
        int tag = P(PostItemLostApplyBattlerTagAbAttr, a)->tagType;
        GATE(!pp->simulated && !pr_get_battler_tag(p, tag));
        pr_add_tag(bt, c->e, (pr_pokemon *)p, tag, 0, 0, -1);
        return 1;
    }
    case PR_AC_BlockNonDirectDamageAbAttr:     /* Magic Guard */
    case PR_AC_BlockRecoilDamageAttr:          /* Rock Head */
    case PR_AC_BypassBurnDamageReductionAbAttr:
    case PR_AC_BlockOneHitKOAbAttr:            /* Sturdy against OHKO moves */
    case PR_AC_FieldPreventExplosiveMovesAbAttr: /* Damp */
    case PR_AC_BlockItemTheftAbAttr:           /* Sticky Hold */
    case PR_AC_ForceSwitchOutImmunityAbAttr:   /* Suction Cups, Guard Dog */
    case PR_AC_IntimidateImmunityAbAttr:       /* Inner Focus, Oblivious, Own Tempo, Scrappy */
        GATE(!*pp->flag); /* CancelInteractionAbAttr */
        *pp->flag = true;
        return 1;
    case PR_AC_MoveDamageBoostAbAttr: {
        const pr_attr_MoveDamageBoostAbAttr *x = P(MoveDamageBoostAbAttr, a);
        GATE(call_bool(bt, x->condition, a->cls, p, o, move));
        *pp->value = to_dmg_value(*pp->value * x->damageMultiplier);
        return 1;
    }
    case PR_AC_ReceivedMoveDamageMultiplierAbAttr:
    case PR_AC_ReceivedTypeDamageMultiplierAbAttr: {
        pr_fn_id cond; double mult;
        if (a->cls == PR_AC_ReceivedMoveDamageMultiplierAbAttr) {
            cond = P(ReceivedMoveDamageMultiplierAbAttr, a)->condition;
            mult = P(ReceivedMoveDamageMultiplierAbAttr, a)->damageMultiplier;
        } else {
            cond = P(ReceivedTypeDamageMultiplierAbAttr, a)->condition;
            mult = P(ReceivedTypeDamageMultiplierAbAttr, a)->damageMultiplier;
        }
        /* PokemonDefendCondition: (target, user, move) */
        GATE(call_bool(bt, cond, a->cls, p, o, move));
        *pp->value = to_dmg_value(*pp->value * mult);
        return 1;
    }
    case PR_AC_FormBlockDamageAbAttr: {
        /* Disguise, Ice Face. apply does nothing when simulated. */
        const pr_attr_FormBlockDamageAbAttr *x = P(FormBlockDamageAbAttr, a);
        GATE(*pp->value > 0 && p->form == x->formIndex && call_bool(bt, x->condition, a->cls, p, o, move)
             && !pr_move_hits_substitute(bt, move, o, p));
        if (pp->simulated) return 1;
        *pp->value = 0;
        /* Disguise's recoil hits at once (a later passive then sees the lower HP); the form change is queued */
        if (x->recoil > 0) pr_damage_indirect(bt, (pr_pokemon *)p, to_dmg_value(pr_get_stat(p, STAT_HP, true) * x->recoil));
        pr_note_deferred(bt, a->cls);
        return 1;
    }
    case PR_AC_AlliedFieldDamageReductionAbAttr:
        GATE(true);
        *pp->value = to_dmg_value(*pp->value * P(AlliedFieldDamageReductionAbAttr, a)->damageMultiplier);
        return 1;
    case PR_AC_RedirectTypeMoveAbAttr: {                                /* Lightning Rod, Storm Drain */
        /* canRedirect: a NEAR_OTHER or OTHER move of the ability's type (the user's getMoveType);
         * and the target isn't this Pokemon already. The target becomes this Pokemon */
        int tgt = pr_moves[move].target, self = pr_battler_index(bt, p);
        GATE((tgt == MOVE_TARGET_NEAR_OTHER || tgt == MOVE_TARGET_OTHER)
             && pr_get_move_type(bt, o, move) == P(RedirectTypeMoveAbAttr, a)->type && *pp->holder != self);
        *pp->holder = self;
        return 1;
    }
    case PR_AC_RunSuccessAbAttr:
        GATE(true);
        if (pp->value) *pp->value = 256;
        return 1;
    case PR_AC_GorillaTacticsAbAttr:                                    /* ExecutedMoveAbAttr */
        if (!c->e) return -1;
        GATE(!pr_get_battler_tag(p, BATTLER_TAG_TYPE_GORILLA_TACTICS));
        if (!pp->simulated) pr_add_tag(bt, c->e, (pr_pokemon *)p, BATTLER_TAG_TYPE_GORILLA_TACTICS, 0, 0, -1);
        return 1;
    case PR_AC_PreDefendFullHpEndureAbAttr:
        GATE(p->hp >= pr_get_stat(p, STAT_HP, true) && pr_get_stat(p, STAT_HP, true) > 1 && *pp->value >= p->hp
             && !pr_has_battler_tag(bt, p, BATTLER_TAG_TYPE_STURDY));
        /* addTag(STURDY, 1); the damage itself is capped later, in Pokemon.damage */
        if (!pp->simulated) {
            if (c->e) pr_add_tag(bt, c->e, (pr_pokemon *)p, BATTLER_TAG_TYPE_STURDY, 1, 0, -1);
            else pr_note_deferred(bt, a->cls);
        }
        return 1;

    /* ---- crits ---- */
    case PR_AC_ConditionalCritAbAttr:
        GATE(!*pp->flag && call_bool(bt, P(ConditionalCritAbAttr, a)->condition, a->cls, p, o, move));
        *pp->flag = true;
        return 1;
    case PR_AC_BonusCritAbAttr:
        GATE(true);
        *pp->holder += 1;
        return 1;
    case PR_AC_BlockCritAbAttr:
        GATE(true);
        *pp->flag = true;
        return 1;

    /* ---- screens and Substitute ---- */
    case PR_AC_InfiltratorAbAttr:
        GATE(!*pp->flag);
        *pp->flag = true;
        return 1;

    /* ---- Mold Breaker and co. ---- */
    case PR_AC_MoveAbilityBypassAbAttr:
        GATE(!*pp->flag && call_bool(bt, P(MoveAbilityBypassAbAttr, a)->moveIgnoreFunc, a->cls, p, NULL, move));
        *pp->flag = true;
        return 1;

    /* ---- berries ---- */
    case PR_AC_CudChewRecordBerryAbAttr: {
        if (p->n_turn_berries > PR_MAX_BERRY_HISTORY) {pr_note_unported(bt, PR_FEAT_ENGINE_LIMIT); return -1;}
        GATE(true);
        /* Pinned apply intentionally also runs during simulated dispatch. */
        pr_pokemon *q = (pr_pokemon *)p;
        q->n_last_berries = q->n_turn_berries;
        memcpy(q->last_berries, q->turn_berries, q->n_last_berries);
        return 1;
    }
    case PR_AC_CudChewConsumeBerryAbAttr: {
        if (p->n_last_berries > PR_MAX_BERRY_HISTORY) {pr_note_unported(bt, PR_FEAT_ENGINE_LIMIT); return -1;}
        GATE(p->n_last_berries > 0);
        if (!c->e) return -1;
        pr_unshift_common_anim(bt, c->e, pr_battler_index(bt, p), pr_battler_index(bt, p), COMMON_ANIM_USE_ITEM);
        for (int i = 0; i < p->n_last_berries; i++)
            pr_berry_effect(bt, c->e, (pr_pokemon *)p, p->last_berries[i], NULL, NULL, NULL);
        return 1;
    }
    case PR_AC_PostTurnRestoreBerryAbAttr: {
        if (p->n_battle_berries > PR_MAX_BERRY_HISTORY) {pr_note_unported(bt, PR_FEAT_ENGINE_LIMIT); return -1;}
        int side = p->is_player ? 0 : 1;
        uint8_t eligible[PR_MAX_BERRY_HISTORY];
        int n = 0;
        for (int i = 0; i < p->n_battle_berries; i++) {
            int berry = p->battle_berries[i];
            int cap = (berry <= BERRY_TYPE_ENIGMA || berry == BERRY_TYPE_LEPPA) ? 2 : 3;
            bool capped = false;
            for (int j = 0; j < bt->n_mods[side]; j++) {
                const pr_modifier *m = &bt->mods[side][j];
                if (m->cls == PR_MOD_BERRY && m->pokemon_id == (int64_t)p->id && m->berry_type == berry
                    && pr_mod_get_stack_count(m) >= cap) capped = true;
            }
            if (!capped) eligible[n++] = (uint8_t)berry;
        }
        if (!n) return 0;
        double pass = pr_rng_seed_float(&bt->rng);
        double chance = pr_closure_call(bt, P(PostTurnRestoreBerryAbAttr, a)->procChance, a->cls, 0, p, NULL, -1);
        GATE(chance >= pass);
        if (pp->simulated) return 1;
        if (!c->e) return -1;
        int idx = pr_rng_seed_int(&bt->rng, n, 0), berry = eligible[idx];
        pr_pokemon *q = (pr_pokemon *)p;
        /* Pin splices the FILTERED index in the original list. Preserve that quirk. */
        memmove(q->battle_berries + idx, q->battle_berries + idx + 1, q->n_battle_berries - idx - 1);
        q->n_battle_berries--;
        pr_modifier *restored = NULL;
        for (int j = 0; j < bt->n_mods[side]; j++) {
            pr_modifier *m = &bt->mods[side][j];
            if (m->cls == PR_MOD_BERRY && m->pokemon_id == (int64_t)p->id && m->berry_type == berry) {restored = m; break;}
        }
        if (restored) restored->stack_count++;
        else {
            if (bt->n_mods[side] >= PR_MAX_MODIFIERS) { pr_note_unported(bt, PR_FEAT_ENGINE_LIMIT); return -1; }
            restored = &bt->mods[side][bt->n_mods[side]++];
            *restored = (pr_modifier){.cls = PR_MOD_BERRY, .pokemon_id = p->id, .stack_count = 1, .berry_type = berry, .is_transferable = true};
        }
        pr_refresh_berry_modifiers(bt, side);
        static const char *keys[] = {"berry:sitrus.name", "berry:lum.name", "berry:enigma.name", "berry:liechi.name", "berry:ganlon.name", "berry:petaya.name", "berry:apicot.name", "berry:salac.name", "berry:lansat.name", "berry:starf.name", "berry:leppa.name"};
        if (berry < 0 || berry >= (int)(sizeof keys / sizeof keys[0])) return -1;
        pr_queue_msg(bt, c->e, pr_msg2("abilityTriggers:postTurnLootCreateEatenBerry", pr_arg_pokemon(bt, "pokemonNameWithAffix", p), pr_arg_key("berryName", keys[berry])));
        return 1;
    }
    case PR_AC_ReduceBerryUseThresholdAbAttr:
        GATE(pr_has_ability_with_attr(bt, p, PR_AC_ReduceBerryUseThresholdAbAttr));
        if (pp->value) {
            double hp_ratio = js_round(((double)p->hp / p->stats[STAT_HP]) * 100.0) / 100.0;
            if (*pp->value < hp_ratio) {
                *pp->value *= 2.0;
            }
        }
        return 1;
    case PR_AC_DoubleBerryEffectAbAttr:
        GATE(pr_has_ability_with_attr(bt, p, PR_AC_DoubleBerryEffectAbAttr));
        if (pp->value) *pp->value *= 2.0;
        return 1;
    case PR_AC_HealFromBerryUseAbAttr:
        GATE(true);
        if (pp->simulated) return 1;
        if (c->e) {
            double hp_healed = to_dmg_value(pr_get_stat(p, STAT_HP, true) * P(HealFromBerryUseAbAttr, a)->healPercent);
            pr_msg msg = pr_msg2("abilityTriggers:healFromBerryUse",
                                 pr_arg_pokemon(bt, "pokemonNameWithAffix", p),
                                 pr_arg_ability("abilityName", c->ability));
            pr_unshift_heal_msg(bt, c->e, pr_battler_index(bt, p), hp_healed, PR_HEAL_MESSAGE, msg);
        }
        return 1;
    case PR_AC_PreventBerryUseAbAttr:
        GATE(pr_has_ability_with_attr(bt, p, PR_AC_PreventBerryUseAbAttr));
        if (pp->flag) *pp->flag = true;
        return 1;

    /* ---- end of turn ---- */
    case PR_AC_PostTurnHurtIfSleepingAbAttr: {
        if(bt->double_battle)return -1;
        bool eligible=false;
        for(int j=0;j<4;j++){
            const pr_pokemon *v=bt->field[j];
            if(v && pr_is_opponent(bt,p,v) && v->hp>0 && v->on_field && !v->switch_out_status
                && (v->status==STATUS_EFFECT_SLEEP || pr_has_ability_active(bt,v,ABILITY_ID_COMATOSE))
                && !pr_has_ability_with_attr(bt,v,PR_AC_BlockNonDirectDamageAbAttr))eligible=true;
        }
        GATE(eligible);
        if(pp->simulated)return 1;
        if(!c->e)return -1;
        for(int j=0;j<4;j++){
            pr_pokemon *v=bt->field[j];
            if(!v || !pr_is_opponent(bt,p,v) || v->hp<=0 || !v->on_field || v->switch_out_status
                || (v->status!=STATUS_EFFECT_SLEEP && !pr_has_ability_active(bt,v,ABILITY_ID_COMATOSE)))continue;
            bool cancelled=false;
            /* Pinned source deliberately invokes the blocker on the ability HOLDER. */
            pr_ab_params bp={.pokemon=p,.move=-1,.simulated=pp->simulated,.flag=&cancelled};
            pr_apply_ab_attrs(bt,PR_AC_BlockNonDirectDamageAbAttr,&bp);
            if(!cancelled){
                pr_damage_and_update(bt,c->e,v,to_dmg_value(pr_get_stat(v,STAT_HP,true)/8.0),HIT_RESULT_INDIRECT,false,NULL);
                pr_queue_msg(bt,c->e,pr_msg1("abilityTriggers:badDreams",pr_arg_pokemon(bt,"pokemonName",v)));
            }
        }
        return 1;
    }
    case PR_AC_FetchBallAbAttr:
        GATE(!pp->simulated && bt->last_used_pokeball >= 0 && p->is_player);
        pr_note_unported(bt, a->cls); /* globalScene.pokeballCounts isn't modelled */
        bt->last_used_pokeball = -1;
        return 1;

    /* ---- secondary effect chance (MoveEffectAttr.getMoveChance) ---- */
    /* ---- StatStageChangePhase: protection from drops, reactions ---- */
    case PR_AC_ProtectStatAbAttr: {                                     /* Clear Body, Hyper Cutter, Keen Eye... */
        const pr_attr_ProtectStatAbAttr *x = P(ProtectStatAbAttr, a);
        int ps = (x->nulls & (1u << 2)) ? -1 : x->protectedStat;          /* null: every stat */
        bool any = false;
        for (int i = 0; i < pp->n_ch; i++)
            any |= (pp->ch_stat[i] == ps || ps < 0) && !(*pp->cancelled_stats & (1u << pp->ch_stat[i]));
        GATE(any);
        if (ps >= 0) *pp->cancelled_stats |= (uint16_t)(1u << ps);
        else for (int i = 0; i < pp->n_ch; i++) *pp->cancelled_stats |= (uint16_t)(1u << pp->ch_stat[i]);
        return 1;
    }
    case PR_AC_ConditionalUserFieldProtectStatAbAttr: {                 /* Flower Veil */
        /* the constructor drops its protectedStat argument, so every stat is protected */
        const pr_attr_ConditionalUserFieldProtectStatAbAttr *x = P(ConditionalUserFieldProtectStatAbAttr, a);
        if (!pp->target) return 0;
        bool any = false;
        for (int i = 0; i < pp->n_ch; i++) any |= !(*pp->cancelled_stats & (1u << pp->ch_stat[i]));
        GATE(any && call_bool(bt, x->condition, a->cls, pp->target, NULL, -1));
        for (int i = 0; i < pp->n_ch; i++) *pp->cancelled_stats |= (uint16_t)(1u << pp->ch_stat[i]);
        return 1;
    }
    case PR_AC_ReflectStatStageChangeAbAttr: {                          /* Mirror Armor */
        if (!pp->simulated && !c->e) return -1;
        int8_t rs[7], rg[7];
        int nr = 0;
        for (int i = 0; i < pp->n_ch && nr < 7; i++)
            if (!(*pp->cancelled_stats & (1u << pp->ch_stat[i]))) { rs[nr] = pp->ch_stat[i]; rg[nr++] = pp->ch_stages[i]; }
        GATE(o && nr > 0);
        /* StatStageChangePhase({battlerIndex: source, changes: reflected, sourcePokemon: undefined,
         * ignoreAbilities: false, sourceEffectType: MIRROR_ARMOR}) */
        if (!pp->simulated
            && pr_unshift_stat_stage_change_ex(bt, c->e, pr_battler_index(bt, o), rs, rg, nr, NULL,
                                               STAT_CHANGE_SOURCE_MIRROR_ARMOR) < 0)
            return -1;
        for (int i = 0; i < nr; i++) *pp->cancelled_stats |= (uint16_t)(1u << rs[i]);
        return 1;
    }
    case PR_AC_StatStageChangeCopyAbAttr:                               /* Opportunist */
        if (!pp->simulated && !c->e) return -1;
        GATE(true);
        if (!pp->simulated
            && pr_unshift_stat_stage_change_ex(bt, c->e, pr_battler_index(bt, p), pp->ch_stat, pp->ch_stages, pp->n_ch,
                                               (pr_pokemon *)p, STAT_CHANGE_SOURCE_OPPORTUNIST) < 0)
            return -1;
        return 1;
    case PR_AC_PostStatStageChangeStatStageChangeAbAttr: {              /* Defiant, Competitive */
        /* changeFunc(pokemon, changes) returns a StatChange; the two upstream closures are
         * {stat: ATK or SPATK, stages: changes[0].stages < 0 ? 2 * changes.length : 0} */
        pr_fn_id fn = P(PostStatStageChangeStatStageChangeAbAttr, a)->changeFunc;
        const char *h = fn < PR_CLOSURE_COUNT ? pr_closures_meta[fn].hash : "";
        int8_t st = !strcmp(h, "f798b2762ab2") ? STAT_ATK : !strcmp(h, "8d8ced84c6c1") ? STAT_SPATK : -1;
        if (st < 0) { if (fn < PR_CLOSURE_COUNT) pr_closure_misses[fn]++; return -1; }
        if (!pp->simulated && !c->e) return -1;
        pr_closure_hits[fn]++;
        int8_t sg = (int8_t)(pp->n_ch && pp->ch_stages[0] < 0 ? 2 * pp->n_ch : 0);
        GATE(!pp->self_target && sg != 0);
        if (!pp->simulated
            && pr_unshift_stat_stage_change_ex(bt, c->e, pr_battler_index(bt, p), &st, &sg, 1, (pr_pokemon *)p, 0) < 0)
            return -1;
        return 1;
    }
    case PR_AC_StatStageChangeMultiplierAbAttr:                 /* Simple x2, Contrary x-1 */
        GATE(true);
        *pp->value *= P(StatStageChangeMultiplierAbAttr, a)->multiplier;
        return 1;
    case PR_AC_MoveEffectChanceMultiplierAbAttr:
        GATE(!(*pp->value <= 0 || move == MOVE_ID_ORDER_UP || move == MOVE_ID_ELECTRO_SHOT));
        *pp->value *= P(MoveEffectChanceMultiplierAbAttr, a)->chanceMultiplier;
        if (*pp->value > 100) *pp->value = 100;
        return 1;
    case PR_AC_ReflectStatusMoveAbAttr:                                 /* Magic Bounce: no canApply of its own */
        if (!c->e) return -1;
        GATE(true);
        if (!pp->simulated) pr_reflect_move(bt, c->e, (pr_pokemon *)p, o, move);
        return 1;
    case PR_AC_IgnoreMoveEffectsAbAttr:
        GATE(*pp->value > 0);
        *pp->value = 0;
        return 1;
    }
    return -1;
}
#undef GATE

/* BattlerTag.getDescriptor() of the tag a BaseBattlerTagImmunityAbAttr blocks ("" for most tags) */
static const char *tag_descriptor_key(int type)
{
    switch (type) {
    case BATTLER_TAG_TYPE_FLINCHED: return "battlerTags:flinchedDesc";
    case BATTLER_TAG_TYPE_CONFUSED: return "battlerTags:confusedDesc";
    case BATTLER_TAG_TYPE_INFATUATED: return "battlerTags:infatuatedDesc";
    case BATTLER_TAG_TYPE_SEEDED: return "battlerTags:seedDesc";
    case BATTLER_TAG_TYPE_NIGHTMARE: return "battlerTags:nightmareDesc";
    case BATTLER_TAG_TYPE_INGRAIN: return "battlerTags:ingrainDesc";      /* before TrappedTag's */
    case BATTLER_TAG_TYPE_DROWSY: return "battlerTags:drowsyDesc";
    }
    return pr_battler_tag_is(type, PR_TAGCLASS_TRAPPED) ? "battlerTags:trappedDesc" : "";
}

/* attr.getTriggerMessage(params, ability.name), which applySingleAbAttrs queues (when it isn't null
 * or "") between showing the ability and apply: the text as upstream builds it then. abilityName is
 * the name of the ability the attr belongs to (the passive's when it is the passive). */
static bool trigger_message(pr_battle *bt, const pr_attr *a, const pr_ab_params *pp, const ab_ctx *c, pr_msg *out)
{
    const pr_pokemon *p = pp->pokemon;
    pr_msg_arg name = pr_arg_pokemon(bt, "pokemonNameWithAffix", p);
    pr_msg_arg ability = pr_arg_ability("abilityName", c->ability);
    switch (a->cls) {
    case PR_AC_BlockItemTheftAbAttr: *out = pr_msg2("abilityTriggers:blockItemTheft", name, ability); return true;
    case PR_AC_NonSuperEffectiveImmunityAbAttr:
        *out = pr_msg2("abilityTriggers:nonSuperEffectiveImmunity", name, ability);
        return true;
    case PR_AC_FullHpResistTypeAbAttr: *out = pr_msg1("abilityTriggers:fullHpResistType", name); return true;
    case PR_AC_MoveImmunityAbAttr:
    case PR_AC_MoveImmunityStatStageChangeAbAttr:                       /* inherits MoveImmunityAbAttr's */
        *out = pr_msg1("abilityTriggers:moveImmunity", name);
        return true;
    case PR_AC_ReverseDrainAbAttr:                                      /* names the attacker */
        *out = pr_msg_p(bt, "abilityTriggers:reverseDrain", pp->opponent);
        return true;
    case PR_AC_PostDefendTypeChangeAbAttr:
        *out = pr_msg3("abilityTriggers:postDefendTypeChange", name, ability, pr_arg_type("typeName", c->msg_type));
        return true;
    case PR_AC_PostDefendContactDamageAbAttr:
        *out = pr_msg2("abilityTriggers:postDefendContactDamage", name, ability);
        return true;
    case PR_AC_PostDefendPerishSongAbAttr:
        *out = pr_msg2("abilityTriggers:perishBody", pr_arg_pokemon(bt, "pokemonName", p), ability);
        return true;
    case PR_AC_PostDefendAbilitySwapAbAttr: *out = pr_msg1("abilityTriggers:postDefendAbilitySwap", name); return true;
    case PR_AC_PostDefendAbilityGiveAbAttr:
        *out = pr_msg2("abilityTriggers:postDefendAbilityGive", name, ability);
        return true;
    case PR_AC_PokemonTypeChangeAbAttr:
        *out = pr_msg2("abilityTriggers:pokemonTypeChange", name, pr_arg_type("moveType", c->msg_type));
        return true;
    case PR_AC_IntimidateImmunityAbAttr:
        *out = pr_msg2("abilityTriggers:intimidateImmunity", name, ability);
        return true;
    case PR_AC_PostSummonHealStatusAbAttr: {
        /* getStatusEffectHealText(this.statusHealed): the status this attr cured last time, if any */
        int healed = pr_status_healed_get(c->ability);
        if (!healed) return false;
        *out = pr_status_msg(bt, healed, PR_ST_HEAL, p);
        return true;
    }
    case PR_AC_PostSummonCopyAbilityAbAttr:                             /* Trace: this.target */
        *out = pr_msg3("abilityTriggers:trace", pr_arg_pokemon(bt, "pokemonName", p),
                       pr_arg_pokemon(bt, "targetName", c->msg_target),
                       pr_arg_ability("abilityName", c->msg_target ? c->msg_target->ability : 0));
        return true;
    case PR_AC_PostSummonCopyAllyStatsAbAttr:                           /* Costar: this.ally = getAlly() */
        *out = pr_msg2("abilityTriggers:costar", pr_arg_pokemon(bt, "pokemonName", p),
                       pr_arg_pokemon(bt, "allyName", pr_get_ally(bt, p)));
        return true;
    case PR_AC_ReflectStatStageChangeAbAttr: {                          /* Mirror Armor */
        /* this.reflectedChanges: the changes not cancelled yet (apply hasn't cancelled them) */
        int n = 0, stat = 0;
        for (int i = 0; i < pp->n_ch; i++)
            if (!(*pp->cancelled_stats & (1u << pp->ch_stat[i]))) { stat = pp->ch_stat[i]; n++; }
        *out = pr_msg3("abilityTriggers:protectStat", name, ability,
                       n == 1 ? pr_arg_stat("statName", stat) : pr_arg_key("statName", "battle:stats"));
        return true;
    }
    case PR_AC_ProtectStatAbAttr: {
        const pr_attr_ProtectStatAbAttr *x = P(ProtectStatAbAttr, a);
        bool one = !(x->nulls & (1u << 2)) && x->protectedStat;        /* `this.protectedStat ?` */
        *out = pr_msg3("abilityTriggers:protectStat", name, ability,
                       one ? pr_arg_stat("statName", x->protectedStat) : pr_arg_key("statName", "battle:stats"));
        return true;
    }
    case PR_AC_StatusEffectImmunityAbAttr: {                            /* PreSetStatusEffectImmunityAbAttr's */
        const pr_attr_StatusEffectImmunityAbAttr *x = P(StatusEffectImmunityAbAttr, a);
        if (x->immuneEffects.n > 0)
            *out = pr_msg3("abilityTriggers:statusEffectImmunityWithName", name, ability,
                           pr_arg_key("statusEffectName", pr_status_key(pp->effect, PR_ST_DESCRIPTION)));
        else
            *out = pr_msg2("abilityTriggers:statusEffectImmunity", name, ability);
        return true;
    }
    case PR_AC_ArenaTrapAbAttr: *out = pr_msg2("abilityTriggers:arenaTrap", name, ability); return true;
    case PR_AC_PostFaintContactDamageAbAttr:
        *out = pr_msg2("abilityTriggers:postFaintContactDamage", name, ability);
        return true;
    case PR_AC_PostFaintHPDamageAbAttr: *out = pr_msg2("abilityTriggers:postFaintHpDamage", name, ability); return true;
    case PR_AC_FormBlockDamageAbAttr:                                   /* Disguise, Ice Face: this.i18nKey */
        *out = pr_msg2(P(FormBlockDamageAbAttr, a)->i18nKey, name, ability);
        return true;
    case PR_AC_BypassSpeedChanceAbAttr:
        *out = pr_msg1("abilityTriggers:quickDraw", pr_arg_pokemon(bt, "pokemonName", p));
        return true;
    case PR_AC_TerrainEventTypeChangeAbAttr: {                          /* Mimicry */
        /* determineTypeChange(pokemon, terrain)[0] for a terrain */
        int t = bt->terrain == TERRAIN_ELECTRIC ? POKEMON_TYPE_ELECTRIC
              : bt->terrain == TERRAIN_MISTY ? POKEMON_TYPE_FAIRY
              : bt->terrain == TERRAIN_GRASSY ? POKEMON_TYPE_GRASS
              : bt->terrain == TERRAIN_PSYCHIC ? POKEMON_TYPE_PSYCHIC : -2;
        if (bt->terrain == TERRAIN_NONE) *out = pr_msg1("abilityTriggers:pokemonTypeChangeRevert", name);
        else if (t != -2) *out = pr_msg2("abilityTriggers:pokemonTypeChange", name, pr_arg_type("moveType", t));
        else *out = pr_msg0(NULL);
        return true;
    }
    }
    /* BaseBattlerTagImmunityAbAttr: BattlerTagImmunity, UserFieldBattlerTagImmunity and its Conditional */
    if (pr_attr_is(a->cls, PR_AC_PreApplyBattlerTagAbAttr)) {
        *out = pr_msg3("abilityTriggers:battlerTagImmunity", name, ability,
                       pr_arg_key("battlerTagName", tag_descriptor_key(pp->tag_type)));
        return true;
    }
    return false;
}

/* AbAttr.showAbility (every AbAttr struct starts with nulls and showAbility, but one) */
static bool shows_ability(const pr_attr *a)
{
    if (a->cls == PR_AC_PostVictoryFormChangeAbAttr) return ((const pr_attr_PostVictoryFormChangeAbAttr *)a->p)->showAbility;
    return ((const struct { uint32_t nulls; bool showAbility; } *)a->p)->showAbility;
}

static void set_add(pr_battle *bt, uint16_t *v, uint8_t *n, uint16_t x)
{
    for (int i = 0; i < *n; i++)
        if (v[i] == x) return;
    if (*n < 4) v[(*n)++] = x;
    else pr_note_unported(bt, PR_FEAT_ENGINE_LIMIT);
}

static struct { uint16_t ability; uint8_t effect; } status_healed[16];
static int n_status_healed;

int pr_status_healed_get(uint16_t ability)
{
    for (int i = 0; i < n_status_healed; i++)
        if (status_healed[i].ability == ability) return status_healed[i].effect;
    return 0;
}

void pr_status_healed_set(uint16_t ability, int effect)
{
    int i = 0;
    while (i < n_status_healed && status_healed[i].ability < ability) i++;
    if (i < n_status_healed && status_healed[i].ability == ability) {
        status_healed[i].effect = (uint8_t)effect;
        return;
    }
    if (n_status_healed == (int)(sizeof status_healed / sizeof status_healed[0])) return;
    memmove(&status_healed[i + 1], &status_healed[i], (size_t)(n_status_healed - i) * sizeof status_healed[0]);
    status_healed[i].ability = ability;
    status_healed[i].effect = (uint8_t)effect;
    n_status_healed++;
}

void pr_status_healed_clear(void) { n_status_healed = 0; }

int pr_status_healed_list(uint16_t *ability, uint8_t *effect, int max)
{
    int n = n_status_healed < max ? n_status_healed : max;
    for (int i = 0; i < n; i++) {
        ability[i] = status_healed[i].ability;
        effect[i] = status_healed[i].effect;
    }
    return n;
}

void pr_apply_ab_attrs(pr_battle *bt, uint16_t cls, pr_ab_params *pp)
{
    const pr_pokemon *p = pp->pokemon;
    if (!p) return;
    /* what applySingleAbAttrs does around apply when not simulated; only the turn engine
     * keeps the phases and sets it touches (the damage test runs without one) */
    pr_engine *e = pp->simulated ? NULL : bt->engine;
    for (int passive = 0; passive < 2; passive++) {
        uint16_t ability = passive ? p->passive : p->ability;
        if (passive && (!p->passive || p->passive == p->ability)) continue;
        if (pp->only == (passive ? 1 : 2)) continue;
        const pr_ability *ab = &pr_abilities[ability];
        bool checked = false, can = false;
        for (unsigned i = 0; i < ab->nattr; i++) {
            const pr_attr *a = &pr_attrs[ab->attr0 + i];
            if (!pr_attr_is(a->cls, cls)) continue;
            if (!checked) { can = pr_can_apply_ability(bt, p, ability); checked = true; }
            if (!can) break;
            if (pp->after_form_change && (pr_attr_is(a->cls, PR_AC_PostSummonFormChangeAbAttr)
                || pr_attr_is(a->cls, PR_AC_PostSummonFormChangeByWeatherAbAttr))) continue;
            if (pp->on_gain && !activate_on_gain(a)) continue;                 /* attrFilter */
            if (!condition_ok(bt, a, p)) continue;
            ab_ctx c = {.e = e, .ability = ability, .passive = passive != 0};
            int r = apply_one(bt, a, pp, &c);
            if (r < 0) { pr_note_unported(bt, a->cls); continue; }
            if (r == 0 || !e) continue;
            if (c.shown) pr_queue_ability_display(bt, e, p, passive != 0, false);
            pr_pokemon *w = (pr_pokemon *)p;
            set_add(bt, w->ab_applied_wave, &w->n_ab_applied_wave, ability);
            set_add(bt, w->ab_applied_summon, &w->n_ab_applied_summon, ability);
        }
    }
}

