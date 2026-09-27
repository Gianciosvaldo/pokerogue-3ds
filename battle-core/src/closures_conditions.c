/* Hand ports of the move conditions MovePhase checks before a move runs (move.applyConditions):
 * the closures in pr_moves[m].conditions / conditions_seq2 / conditions_seq3.
 *
 * Most attribute conditions are made by attr.getCondition() and read the attribute through
 * `this`, and every attribute of a class gives the same source, so they are registered by hash
 * for any owner (owner NULL). own_attr() finds the attribute a call belongs to: Move.attr()
 * pushes attribute conditions onto `conditions` in attribute order, so the k-th closure of a
 * hash in a move's list belongs to the k-th attribute that makes that hash. When the counts
 * disagree (a class this file doesn't know makes the same condition) the call reports itself
 * unported rather than guess.
 *
 * A condition whose inputs aren't modelled yet reports itself unported (PR_FEAT_MOVE_CONDITION)
 * and returns true; a turn with an unported condition is not claimed faithful. */
#include "pr/engine.h"
#include <math.h>
#include <string.h>

#define UNPORTED(id) do { pr_note_unported(bt, (id)); return 1; } while (0)
#define ARGS pr_battle *bt, const pr_closure_info *self, const pr_pokemon *u, const pr_pokemon *t, int m
#define UNUSED (void)bt; (void)self; (void)u; (void)t; (void)m
/* upstream would throw reading a property of the missing target */
#define NEED_TARGET do { if (!t) UNPORTED(PR_FEAT_MOVE_CONDITION); } while (0)
#define P(T, a) ((const pr_attr_##T *)(a)->p)

/* Ability flags (src/data/abilities/ability.ts) */
#define AB_FLAG_UNSUPPRESSABLE (1u << 2)
#define AB_FLAG_UNCOPIABLE (1u << 3)
#define AB_FLAG_UNREPLACEABLE (1u << 4)
static bool ab_replaceable(int ab) { return !(pr_abilities[ab].flags & AB_FLAG_UNREPLACEABLE); }
static bool ab_copiable(int ab) { return !(pr_abilities[ab].flags & AB_FLAG_UNCOPIABLE); }
static bool ab_suppressable(int ab) { return !(pr_abilities[ab].flags & AB_FLAG_UNSUPPRESSABLE); }
/* AB_FLAG_UNSWAPPABLE = UNCOPIABLE | UNREPLACEABLE */
static bool ab_swappable(int ab) { return !(pr_abilities[ab].flags & (AB_FLAG_UNCOPIABLE | AB_FLAG_UNREPLACEABLE)); }

static double to_dmg_value(double v) { double f = floor(v); return f < 1 ? 1 : f; }
static bool is_full_hp(const pr_pokemon *p) { return p->hp >= pr_get_stat(p, STAT_HP, true); }
/* isActive(): isAllowedInBattle (not fainted; challenges are not modelled) */
static bool is_active(const pr_pokemon *p) { return p && p->hp > 0; }
static bool is_on_field(const pr_pokemon *p) { return p && p->on_field && !p->switch_out_status; }
/* PlayerPokemon.hasTrainer() is true; EnemyPokemon's is !!trainerSlot */
static bool has_trainer(const pr_pokemon *p) { return p->is_player || p->has_trainer; }
/* hasAbility(ability, canApply = false): the active ability or the passive, usable or not */
static bool has_ability_any(const pr_pokemon *p, int ab) { return p->ability == ab || (p->passive && p->passive == ab); }

/* isOfType(type, {returnOriginalTypesIfStellar: true}) */
static bool is_of_type_orig(const pr_pokemon *p, int type)
{
    int8_t ty[3];
    int n = pr_get_types(p, ty, PR_TYPES_ORIGINAL_IF_STELLAR);
    for (int i = 0; i < n; i++)
        if (ty[i] == type) return true;
    return false;
}

/* pokemonMove.getMovePp(): maxPpOverride || pp + ppUp * toDmgValue(pp / 5) */
static int move_pp(int move, int pp_up, int max_pp_override)
{
    if (max_pp_override > 0) return max_pp_override;
    return pr_moves[move].pp + pp_up * (int)to_dmg_value(pr_moves[move].pp / 5.0);
}

/* The attribute of move m whose getCondition() made closure `self` (see the top of the file) */
static const pr_attr *own_attr(const pr_closure_info *self, int m, bool (*makes)(const pr_attr *))
{
    if (m < 0 || m >= PR_MOVE_COUNT) return NULL;
    const pr_move *mv = &pr_moves[m];
    int id = (int)(self - pr_closures_meta), k = -1, same = 0;
    for (int i = 0; i < mv->conditions.n; i++) {
        if (strcmp(pr_closures_meta[mv->conditions.v[i]].hash, self->hash)) continue;
        if (mv->conditions.v[i] == id) k = same;
        same++;
    }
    if (k < 0) return NULL;
    const pr_attr *found = NULL;
    int n = 0;
    for (unsigned i = 0; i < mv->nattr; i++) {
        const pr_attr *a = &pr_attrs[mv->attr0 + i];
        if (!makes(a)) continue;
        if (n++ == k) found = a;
    }
    return n == same ? found : NULL;
}
#define OWN(makes) const pr_attr *a = own_attr(self, m, makes); if (!a) UNPORTED(PR_FEAT_MOVE_CONDITION)

/* The party slots of one side ([0] player, [1] enemy), read through the field Pokemon when on it */
static const pr_pokemon *party_field_mon(const pr_battle *bt, int side, int k)
{
    for (int i = 0; i < 4; i++)
        if (bt->field[i] && bt->field[i]->id == bt->party[side][k].id) return bt->field[i];
    return NULL;
}
static int32_t party_hp(const pr_battle *bt, int side, int k)
{
    const pr_pokemon *f = party_field_mon(bt, side, k);
    return f ? f->hp : bt->party[side][k].hp;
}
static bool party_on_field(const pr_battle *bt, int side, int k)
{
    return is_on_field(party_field_mon(bt, side, k));
}

/* ------------------------------------------------------------ attribute conditions */

/* AddBattlerTagAttr (and ExposedMoveAttr) with failOnOverlap:
 * (user, target) => !(this.selfTarget ? user : target).getTag(this.tagType) */
static bool makes_tag_overlap(const pr_attr *a)
{
    if (a->cls == PR_AC_AddBattlerTagAttr) return P(AddBattlerTagAttr, a)->failOnOverlap;
    if (a->cls == PR_AC_ExposedMoveAttr) return P(ExposedMoveAttr, a)->failOnOverlap;
    return false;
}
static double tag_overlap(ARGS)
{
    OWN(makes_tag_overlap);
    bool self_target = a->cls == PR_AC_AddBattlerTagAttr ? P(AddBattlerTagAttr, a)->selfTarget
                                                         : P(ExposedMoveAttr, a)->selfTarget;
    int tag = a->cls == PR_AC_AddBattlerTagAttr ? P(AddBattlerTagAttr, a)->tagType : P(ExposedMoveAttr, a)->tagType;
    const pr_pokemon *p = self_target ? u : t;
    if (!p) UNPORTED(PR_FEAT_MOVE_CONDITION);
    return !pr_get_battler_tag(p, tag);
}

/* HealAttr / VariableHealAttr:
 * (user, target) => !(this.failOnFullHp && (this.selfTarget ? user : target).isFullHp()) */
static bool makes_heal(const pr_attr *a) { return a->cls == PR_AC_HealAttr || a->cls == PR_AC_VariableHealAttr; }
static bool heal_ok(const pr_attr *a, const pr_pokemon *u, const pr_pokemon *t, bool *need_target)
{
    bool fail_full, self_target;
    if (a->cls == PR_AC_HealAttr) { fail_full = P(HealAttr, a)->failOnFullHp; self_target = P(HealAttr, a)->selfTarget; }
    else if (a->cls == PR_AC_VariableHealAttr) {
        fail_full = P(VariableHealAttr, a)->failOnFullHp; self_target = P(VariableHealAttr, a)->selfTarget;
    } else { fail_full = P(HealOnAllyAttr, a)->failOnFullHp; self_target = P(HealOnAllyAttr, a)->selfTarget; }
    if (!fail_full) return true;
    const pr_pokemon *p = self_target ? u : t;
    if (!p) { *need_target = true; return true; }
    return !is_full_hp(p);
}
static double heal_full_hp(ARGS)
{
    OWN(makes_heal);
    bool nt = false;
    bool ok = heal_ok(a, u, t, &nt);
    if (nt) UNPORTED(PR_FEAT_MOVE_CONDITION);
    return ok;
}

/* HealOnAllyAttr (Pollen Puff): (user, target, move) => user.isOpponent(target) || super.getCondition()(...) */
static bool makes_heal_on_ally(const pr_attr *a) { return a->cls == PR_AC_HealOnAllyAttr; }
static double heal_on_ally(ARGS)
{
    OWN(makes_heal_on_ally);
    NEED_TARGET;
    if (pr_is_opponent(bt, u, t)) return 1;
    bool nt = false;
    bool ok = heal_ok(a, u, t, &nt);
    if (nt) UNPORTED(PR_FEAT_MOVE_CONDITION);
    return ok;
}

/* AddArenaTagAttr with failOnOverlap:
 * (_user, target) => !arena.getTagOnSide(this.tagType, target.isPlayer() ? PLAYER : ENEMY) */
static bool makes_arena_overlap(const pr_attr *a)
{
    return a->cls == PR_AC_AddArenaTagAttr && P(AddArenaTagAttr, a)->failOnOverlap;
}
static double arena_tag_overlap(ARGS)
{
    OWN(makes_arena_overlap);
    NEED_TARGET;
    int side = t->is_player ? ARENA_TAG_SIDE_PLAYER : ARENA_TAG_SIDE_ENEMY;
    return !pr_arena_get_tag_on_side(bt, P(AddArenaTagAttr, a)->tagType, side);
}

/* AddArenaTrapTagAttr: side = this.selfSideTarget === user.isPlayer() ? PLAYER : ENEMY;
 * no tag there ? true : tag.canAdd() (the layer count isn't modelled) */
static bool makes_arena_trap(const pr_attr *a) { return a->cls == PR_AC_AddArenaTrapTagAttr; }
static double arena_trap_tag(ARGS)
{
    OWN(makes_arena_trap);
    int side = P(AddArenaTrapTagAttr, a)->selfSideTarget == u->is_player ? ARENA_TAG_SIDE_PLAYER : ARENA_TAG_SIDE_ENEMY;
    if (!pr_arena_get_tag_on_side(bt, P(AddArenaTrapTagAttr, a)->tagType, side)) return 1;
    UNPORTED(PR_FEAT_MOVE_CONDITION);
}

/* WeatherChangeAttr: !arena.weather || (weather.weatherType !== this.weatherType && !weather.isImmutable())
 * (arena.weather is null for NONE; HEAVY_RAIN, HARSH_SUN and STRONG_WINDS are immutable) */
static bool makes_weather(const pr_attr *a) { return a->cls == PR_AC_WeatherChangeAttr; }
static double weather_change(ARGS)
{
    OWN(makes_weather);
    int w = bt->weather;
    if (w == WEATHER_TYPE_NONE) return 1;
    bool immutable = w == WEATHER_TYPE_HEAVY_RAIN || w == WEATHER_TYPE_HARSH_SUN || w == WEATHER_TYPE_STRONG_WINDS;
    return w != P(WeatherChangeAttr, a)->weatherType && !immutable;
}

/* TerrainChangeAttr: !arena.terrain || arena.terrain.terrainType !== this.terrainType */
static bool makes_terrain(const pr_attr *a) { return a->cls == PR_AC_TerrainChangeAttr; }
static double terrain_change(ARGS)
{
    OWN(makes_terrain);
    return bt->terrain == TERRAIN_NONE || bt->terrain != P(TerrainChangeAttr, a)->terrainType;
}

/* CutHpStatStageBoostAttr (Belly Drum and co.):
 * user => user.getHpRatio() > 1 / this.cutRatio && this.stats.some(s => user.getStatStage(s) < 6) */
static bool makes_cut_hp(const pr_attr *a) { return a->cls == PR_AC_CutHpStatStageBoostAttr; }
static double cut_hp_boost(ARGS)
{
    OWN(makes_cut_hp);
    const pr_attr_CutHpStatStageBoostAttr *x = P(CutHpStatStageBoostAttr, a);
    if (!(pr_get_hp_ratio(u, false) > 1.0 / x->cutRatio)) return 0;
    for (unsigned i = 0; i < x->stats.n; i++)
        if (u->stat_stages[x->stats.v[i] - 1] < 6) return 1;
    return 0;
}

/* AddSubstituteAttr: user => !user.getTag(SubstituteTag) && user.hp > this.getHpCost(user),
 * getHpCost = (this.roundUp ? Math.ceil : toDmgValue)(user.getMaxHp() * this.hpCost) */
static bool makes_substitute(const pr_attr *a) { return a->cls == PR_AC_AddSubstituteAttr; }
static double add_substitute(ARGS)
{
    OWN(makes_substitute);
    const pr_attr_AddSubstituteAttr *x = P(AddSubstituteAttr, a);
    if (pr_find_battler_tag_class(u, PR_TAGCLASS_SUBSTITUTE)) return 0;
    double c = pr_get_stat(u, STAT_HP, true) * x->hpCost;
    return u->hp > (x->roundUp ? ceil(c) : to_dmg_value(c));
}

/* AddTypeAttr (Trick-or-Treat, Forest's Curse): !target.isTerastallized && !target.isOfType(this.type) */
static bool makes_add_type(const pr_attr *a) { return a->cls == PR_AC_AddTypeAttr; }
static double add_type(ARGS)
{
    OWN(makes_add_type);
    NEED_TARGET;
    return !t->terastallized && !pr_is_of_type(t, P(AddTypeAttr, a)->type);
}

/* ChangeTypeAttr (Soak, Magic Powder): !target.isTerastallized && !target.hasAbility(MULTITYPE)
 * && !target.hasAbility(RKS_SYSTEM) && !(target.getTypes().length === 1 && target.getTypes()[0] === this.type) */
static bool makes_change_type(const pr_attr *a) { return a->cls == PR_AC_ChangeTypeAttr; }
static double change_type(ARGS)
{
    OWN(makes_change_type);
    NEED_TARGET;
    if (t->terastallized) return 0;
    if (pr_has_ability_active(bt, t, ABILITY_ID_MULTITYPE) || pr_has_ability_active(bt, t, ABILITY_ID_RKS_SYSTEM)) return 0;
    int8_t ty[3];
    int n = pr_get_types(t, ty, 0);
    return !(n == 1 && ty[0] == P(ChangeTypeAttr, a)->type);
}

/* AbilityChangeAttr (Worry Seed, Simple Beam): (this.selfTarget ? user : target).getAbility().replaceable
 * && (this.selfTarget ? user : target).getAbility().id !== this.ability */
static bool makes_ability_change(const pr_attr *a) { return a->cls == PR_AC_AbilityChangeAttr; }
static double ability_change(ARGS)
{
    OWN(makes_ability_change);
    const pr_pokemon *p = P(AbilityChangeAttr, a)->selfTarget ? u : t;
    if (!p) UNPORTED(PR_FEAT_MOVE_CONDITION);
    return ab_replaceable(p->ability) && p->ability != P(AbilityChangeAttr, a)->ability;
}

/* AbilityCopyAttr (Role Play, Doodle): target.getAbility().copiable && user.getAbility().replaceable, then
 * with copyToPartner in a double battle: (!ally?.hp || ally.getAbility().replaceable),
 * otherwise: user.getAbility().id !== target.getAbility().id */
static bool makes_ability_copy(const pr_attr *a) { return a->cls == PR_AC_AbilityCopyAttr; }
static double ability_copy(ARGS)
{
    OWN(makes_ability_copy);
    NEED_TARGET;
    bool ret = ab_copiable(t->ability) && ab_replaceable(u->ability);
    if (P(AbilityCopyAttr, a)->copyToPartner && bt->double_battle) {
        const pr_pokemon *ally = pr_get_ally(bt, u);
        return ret && (!ally || !ally->hp || ab_replaceable(ally->ability));
    }
    return ret && u->ability != t->ability;
}

/* ForceSwitchOutAttr: move.category !== STATUS || this.getSwitchOutCondition()(user, target, move)
 * (the switch-out condition isn't ported) */
static double force_switch_out(ARGS)
{
    UNUSED;
    if (pr_moves[m].category != MOVE_CATEGORY_STATUS) return 1;
    UNPORTED(PR_FEAT_MOVE_CONDITION);
}

/* ChillyReceptionAttr: arena.weather?.weatherType !== SNOW || super.getSwitchOutCondition()(...) */
static double chilly_reception(ARGS)
{
    UNUSED;
    if (bt->weather != WEATHER_TYPE_SNOW) return 1;
    UNPORTED(PR_FEAT_MOVE_CONDITION);
}

/* ProtectAttr: the chance to keep protecting falls to 1/3^n after n successful protections
 * in a row (Quick Guard and Wide Guard count without being ProtectAttrs):
 * timesUsed === 0 || user.randBattleSeedInt(Math.pow(3, timesUsed)) === 0 */
static double protect_streak(ARGS)
{
    UNUSED;
    int times = 0;
    for (int k = 0; k < u->n_move_history; k++) {
        const pr_turn_move *tm = pr_last_move(bt, u, k);
        if (!tm) return 1;                                   /* older than the kept entries (reported) */
        bool counts = pr_move_has_attr(tm->move, PR_AC_ProtectAttr) || tm->move == MOVE_ID_QUICK_GUARD
                      || tm->move == MOVE_ID_WIDE_GUARD;
        if (!counts || tm->result != MOVE_RESULT_SUCCESS) break;
        times++;
    }
    if (times == 0) return 1;
    if (times > 19) UNPORTED(PR_FEAT_MOVE_CONDITION);         /* 3^20 leaves int32 */
    int32_t range = 1;
    for (int i = 0; i < times; i++) range *= 3;
    return pr_battle_rand_seed_int(bt, range, 0) == 0;
}

/* OneHitKOAttr moves: BlockOneHitKOAbAttr (Sturdy) on the target, not simulated, then
 * !cancelled.value && user.level >= target.level */
static double one_hit_ko(ARGS)
{
    UNUSED;
    NEED_TARGET;
    bool cancelled = false;
    pr_ab_params pp = {.pokemon = t, .move = -1, .flag = &cancelled};
    pr_apply_ab_attrs(bt, PR_AC_BlockOneHitKOAbAttr, &pp);
    return !cancelled && u->level >= t->level;
}

/* Counter (physical), Mirror Coat (special), Metal Burst and Comeuppance (both): the singleton
 * CounterAttackConditon(damageCategory): getCounterAttackTarget(user, damageCategory) !== null */
static double counter_physical(ARGS) { UNUSED; return pr_counter_attack_target(bt, u, MOVE_CATEGORY_PHYSICAL) != -2; }
static double counter_special(ARGS) { UNUSED; return pr_counter_attack_target(bt, u, MOVE_CATEGORY_SPECIAL) != -2; }
static double counter_both(ARGS) { UNUSED; return pr_counter_attack_target(bt, u, -1) != -2; }

/* FailIfInsufficientHpCondition(cutRatio): user.getHpRatio() > 1 / cutRatio
 * (Clangorous Soul 3, Fillet Away 2; the ratio is a constructor argument the dump can't see) */
static double insufficient_hp_3(ARGS) { UNUSED; return pr_get_hp_ratio(u, false) > 1.0 / 3; }
static double insufficient_hp_2(ARGS) { UNUSED; return pr_get_hp_ratio(u, false) > 1.0 / 2; }

/* ------------------------------------------------------------ move-specific conditions */

/* Protect and co.: () => phaseManager.hasPhaseOfType("MovePhase") */
static double move_phase_queued(ARGS)
{
    UNUSED;
    if (!bt->engine) UNPORTED(PR_FEAT_MOVE_CONDITION);
    return pr_phase_exists(bt->engine, PH_MOVE, -1);
}

/* () => currentBattle.double */
static double battle_is_double(ARGS) { UNUSED; return bt->double_battle; }

/* Fake Out, Mat Block, First Impression: user.tempSummonData.waveTurnCount === 1 */
static double first_turn(ARGS) { UNUSED; return u->temp_wave_turn_count == 1; }

/* Spider Web, Mean Look, Block...: !target.isOfType(GHOST) */
static double target_not_ghost(ARGS) { UNUSED; NEED_TARGET; return !pr_is_of_type(t, POKEMON_TYPE_GHOST); }

/* Leech Seed: !target.getTag("SEEDED") && !target.isOfType(GRASS) */
static double leech_seed(ARGS)
{
    UNUSED;
    NEED_TARGET;
    return !pr_get_battler_tag(t, BATTLER_TAG_TYPE_SEEDED) && !pr_is_of_type(t, POKEMON_TYPE_GRASS);
}

/* Dream Eater, Nightmare: target.status?.effect === SLEEP || target.hasAbility(COMATOSE) */
static double target_asleep(ARGS)
{
    UNUSED;
    NEED_TARGET;
    return t->status == STATUS_EFFECT_SLEEP || pr_has_ability_active(bt, t, ABILITY_ID_COMATOSE);
}

/* Snore, Sleep Talk: user.status?.effect === SLEEP || user.hasAbility(COMATOSE) */
static double user_asleep(ARGS)
{
    UNUSED;
    return u->status == STATUS_EFFECT_SLEEP || pr_has_ability_active(bt, u, ABILITY_ID_COMATOSE);
}

/* isOppositeGender: this.gender !== GENDERLESS && other.gender === (this.gender === MALE ? FEMALE : MALE)
 * (Gender: GENDERLESS -1, MALE 0, FEMALE 1) */
static bool opposite_gender(const pr_pokemon *a, const pr_pokemon *b)
{
    return a->gender != -1 && b->gender == (a->gender == 0 ? 1 : 0);
}
/* Attract: user.isOppositeGender(target) */
static double attract(ARGS) { UNUSED; NEED_TARGET; return opposite_gender(u, t); }
/* Captivate: target.isOppositeGender(user) */
static double captivate(ARGS) { UNUSED; NEED_TARGET; return opposite_gender(t, u); }

/* Refresh: !!user.status && effect is PARALYSIS, POISON, TOXIC or BURN */
static double refresh(ARGS)
{
    UNUSED;
    int s = u->status;
    return s == STATUS_EFFECT_PARALYSIS || s == STATUS_EFFECT_POISON || s == STATUS_EFFECT_TOXIC || s == STATUS_EFFECT_BURN;
}

/* Purify: target.status && isNonVolatileStatusEffect(target.status.effect) */
static double purify(ARGS)
{
    UNUSED;
    NEED_TARGET;
    return t->status >= STATUS_EFFECT_POISON && t->status <= STATUS_EFFECT_BURN;
}

/* Sucker Punch, Thunderclap (TurnCommand of the target):
 * turnCommand && turnCommand.move && command === FIGHT && !target.turnData.acted
 * && allMoves[turnCommand.move.move].category !== STATUS
 * (a command whose move object holds NONE reads as no move here) */
static double target_attacking(ARGS)
{
    UNUSED;
    NEED_TARGET;
    int b = pr_battler_index(bt, t);
    if (b < 0 || b > 3) UNPORTED(PR_FEAT_MOVE_CONDITION);
    const struct pr_turn_command *c = &bt->commands[b];
    if (c->command < 0 || !c->move) return 0;
    return c->command == COMMAND_FIGHT && !t->acted && pr_moves[c->move].category != MOVE_CATEGORY_STATUS;
}

/* Upper Hand: the same, and the move's getPriority(target) > 0 */
static double upper_hand(ARGS)
{
    UNUSED;
    NEED_TARGET;
    int b = pr_battler_index(bt, t);
    if (b < 0 || b > 3) UNPORTED(PR_FEAT_MOVE_CONDITION);
    const struct pr_turn_command *c = &bt->commands[b];
    return c->command == COMMAND_FIGHT && !t->acted && c->move
           && pr_moves[c->move].category != MOVE_CATEGORY_STATUS && pr_move_get_priority(bt, c->move, t) > 0;
}

/* !target.turnData.acted */
static double target_not_acted(ARGS) { UNUSED; NEED_TARGET; return !t->acted; }

/* Magnet Rise, Telekinesis: !target.getTag("IGNORE_FLYING") */
static double no_ignore_flying(ARGS)
{
    UNUSED;
    NEED_TARGET;
    return !pr_get_battler_tag(t, BATTLER_TAG_TYPE_IGNORE_FLYING);
}
/* Focus Energy: !target.getTag("DRAGON_CHEER") */
static double no_dragon_cheer(ARGS) { UNUSED; NEED_TARGET; return !pr_get_battler_tag(t, BATTLER_TAG_TYPE_DRAGON_CHEER); }
/* Dragon Cheer: !target.getTag("CRIT_BOOST") */
static double no_crit_boost(ARGS) { UNUSED; NEED_TARGET; return !pr_get_battler_tag(t, BATTLER_TAG_TYPE_CRIT_BOOST); }
/* !target.getTag("SUBSTITUTE") */
static double no_substitute(ARGS) { UNUSED; NEED_TARGET; return !pr_get_battler_tag(t, BATTLER_TAG_TYPE_SUBSTITUTE); }

/* Strength Sap: target.getStatStage(ATK) > -6 */
static double atk_not_min(ARGS) { UNUSED; NEED_TARGET; return t->stat_stages[STAT_ATK - 1] > -6; }

/* Burn Up: user.isOfType(FIRE, {returnOriginalTypesIfStellar}); Double Shock: ELECTRIC */
static double user_fire(ARGS) { UNUSED; return is_of_type_orig(u, POKEMON_TYPE_FIRE); }
static double user_electric(ARGS) { UNUSED; return is_of_type_orig(u, POKEMON_TYPE_ELECTRIC); }

/* Aurora Veil: getEffectiveWeatherForMove(user) is HAIL or SNOW */
static double hail_or_snow(ARGS)
{
    UNUSED;
    int w = pr_get_effective_weather_for_move(bt, u);
    return w == WEATHER_TYPE_HAIL || w == WEATHER_TYPE_SNOW;
}

/* Steel Roller: !!arena.terrain */
static double any_terrain(ARGS) { UNUSED; return bt->terrain != TERRAIN_NONE; }

/* Endeavor: user.hp <= target.hp */
static double endeavor(ARGS) { UNUSED; NEED_TARGET; return u->hp <= t->hp; }

/* Perish Song, ...: !target.isBossImmune() (isBoss()) */
static double not_boss(ARGS) { UNUSED; NEED_TARGET; return !t->is_boss; }

/* Pollen Puff: target == null || target.isOpponent(user) || !target.getTag("HEAL_BLOCK") */
static double pollen_puff_heal_block(ARGS)
{
    UNUSED;
    if (!t) return 1;
    return pr_is_opponent(bt, t, u) || !pr_get_battler_tag(t, BATTLER_TAG_TYPE_HEAL_BLOCK);
}

/* Destiny Bond: the last move isn't a successful Destiny Bond:
 * lastTurnMove = user.getLastXMoves(1).at(0); !(lastTurnMove?.move === move.id && lastTurnMove.result === SUCCESS) */
static double not_repeated_success(ARGS)
{
    UNUSED;
    if (!u->n_move_history) return 1;
    const pr_turn_move *tm = pr_last_move(bt, u, 0);
    if (!tm) return 1;
    return !(tm->move == m && tm->result == MOVE_RESULT_SUCCESS);
}

/* Destiny Bond: !(target.isEnemy() && (gameMode.isBattleClassicFinalBoss(wave) || isEndlessMinorBoss(wave)))
 * Those need wave 200 (classic, challenge) or a multiple of 250 (endless); the game mode isn't modelled,
 * so only other waves are answered. */
static double not_final_boss(ARGS)
{
    UNUSED;
    NEED_TARGET;
    if (t->is_player) return 1;
    if (bt->wave_index != 200 && bt->wave_index % 250 != 0) return 1;
    UNPORTED(PR_FEAT_FINAL_BOSS);
}

/* Yawn: !target.status && !target.isSafeguarded(user). isSafeguarded(attacker): Safeguard on the
 * target's side, unless the attacker's InfiltratorAbAttr (applied, not simulated) bypasses it */
static double yawn(ARGS)
{
    UNUSED;
    NEED_TARGET;
    if (t->status) return 0;
    int side = t->is_player ? ARENA_TAG_SIDE_PLAYER : ARENA_TAG_SIDE_ENEMY;
    if (!pr_arena_get_tag_on_side(bt, ARENA_TAG_TYPE_SAFEGUARD, side)) return 1;
    bool bypassed = false;
    pr_ab_params pp = {.pokemon = u, .move = -1, .flag = &bypassed};
    pr_apply_ab_attrs(bt, PR_AC_InfiltratorAbAttr, &pp);
    return bypassed;
}

/* Disable: lastNonVirtualMove = target.getLastNonVirtualMove(); != null && .move !== STRUGGLE
 * getLastNonVirtualMove(): moveHistory.findLast(m => m.move !== NONE && !isVirtual(m.useMode)) */
static const pr_turn_move *last_non_virtual_move(pr_battle *bt, const pr_pokemon *p, bool *unknown)
{
    *unknown = false;
    for (int k = 0; k < p->n_move_history; k++) {
        const pr_turn_move *tm = pr_last_move(bt, p, k);
        if (!tm) { *unknown = true; return NULL; }             /* older than the kept entries (reported) */
        if (tm->move != MOVE_ID_NONE && tm->use_mode < MOVE_USE_MODE_INDIRECT) return tm;
    }
    return NULL;
}
static double disable(ARGS)
{
    UNUSED;
    NEED_TARGET;
    bool unknown;
    const pr_turn_move *tm = last_non_virtual_move(bt, t, &unknown);
    if (unknown) return 1;
    return tm && tm->move != MOVE_ID_STRUGGLE;
}

/* Spite: movesetMove = target.getMoveset().find(m => m.moveId === lastMove?.move); !!movesetMove?.getPpRatio() */
static double spite(ARGS)
{
    UNUSED;
    NEED_TARGET;
    bool unknown;
    const pr_turn_move *tm = last_non_virtual_move(bt, t, &unknown);
    if (unknown) return 1;
    if (!tm) return 0;                                         /* no moveset move has moveId undefined */
    for (int s = 0; s < t->n_moves; s++)
        if (t->moves[s] == tm->move) {
            double r = 1.0 - (double)t->pp_used[s] / move_pp(t->moves[s], t->pp_up[s], t->max_pp_override[s]);
            return r == r && r != 0;                           /* !!NaN is false */
        }
    return 0;
}

/* Conversion: !user.isTerastallized && !user.isOfType(user.getMoveset()[0].getMove().type) */
static double conversion(ARGS)
{
    UNUSED;
    if (u->terastallized) return 0;
    if (!u->n_moves) UNPORTED(PR_FEAT_MOVE_CONDITION);         /* getMoveset()[0] is undefined: upstream throws */
    return !pr_is_of_type(u, pr_moves[u->moves[0]].type);
}

/* Conversion 2: !user.isTerastallized && target.getLastXMoves(-1).some(tm => tm.move !== NONE) */
static double conversion_2(ARGS)
{
    UNUSED;
    NEED_TARGET;
    if (u->terastallized) return 0;
    for (int k = 0; k < t->n_move_history; k++) {
        const pr_turn_move *tm = pr_last_move(bt, t, k);
        if (!tm) return 1;                                     /* older than the kept entries (reported) */
        if (tm->move != MOVE_ID_NONE) return 1;
    }
    return 0;
}

/* Last Resort: the moveset has the move and another one, and every other move is in the
 * non-virtual move history */
static double last_resort(ARGS)
{
    UNUSED;
    bool has_self = false;
    int others[4], n_others = 0;
    for (int s = 0; s < u->n_moves; s++) {
        if (u->moves[s] == m) { has_self = true; continue; }
        bool dup = false;
        for (int j = 0; j < n_others; j++) dup |= others[j] == u->moves[s];
        if (!dup) others[n_others++] = u->moves[s];
    }
    if (!has_self || !n_others) return 0;
    int kept = u->n_move_history < PR_MAX_MOVE_HISTORY ? u->n_move_history : PR_MAX_MOVE_HISTORY;
    for (int j = 0; j < n_others; j++) {
        bool seen = false;
        for (int k = 0; k < kept && !seen; k++) {
            const pr_turn_move *tm = &u->move_history[k];
            seen = tm->move == others[j] && tm->use_mode < MOVE_USE_MODE_INDIRECT;
        }
        if (!seen) {
            if (u->n_move_history > kept) UNPORTED(PR_FEAT_MOVE_HISTORY);
            return 0;
        }
    }
    return 1;
}

/* Magnetic Flux, Gear Up:
 * [user, user.getAlly()].filter(p => p?.isActive()).find(p => [PLUS, MINUS].some(a => p.hasAbility(a, false))) */
static double plus_minus_on_side(ARGS)
{
    UNUSED;
    const pr_pokemon *ps[2] = {u, pr_get_ally(bt, u)};
    for (int i = 0; i < 2; i++)
        if (is_active(ps[i]) && (has_ability_any(ps[i], ABILITY_ID_PLUS) || has_ability_any(ps[i], ABILITY_ID_MINUS)))
            return 1;
    return 0;
}

/* Skill Swap: [user, target].every(p => p.getAbility().swappable) */
static double skill_swap(ARGS) { UNUSED; NEED_TARGET; return ab_swappable(u->ability) && ab_swappable(t->ability); }

/* Entrainment: user.getAbility().copiable && target.getAbility().replaceable && user ability !== target ability */
static double entrainment(ARGS)
{
    UNUSED;
    NEED_TARGET;
    return ab_copiable(u->ability) && ab_replaceable(t->ability) && u->ability != t->ability;
}

/* Gastro Acid: !target.summonData.abilitySuppressed
 * && (target.getAbility().suppressable || (target.hasPassive() && target.getPassiveAbility().suppressable)) */
static double gastro_acid(ARGS)
{
    UNUSED;
    NEED_TARGET;
    if (t->ability_suppressed) return 0;
    return ab_suppressable(t->ability) || (t->passive && ab_suppressable(t->passive));
}

/* Reflect Type: !user.isTerastallized && (!target.isOfType(UNKNOWN, {returnOriginalTypesIfStellar})
 * || target.summonData.addedType !== null) (addedType isn't modelled) */
static double reflect_type(ARGS)
{
    UNUSED;
    NEED_TARGET;
    if (u->terastallized) return 0;
    if (!is_of_type_orig(t, POKEMON_TYPE_UNKNOWN)) return 1;
    UNPORTED(PR_FEAT_MOVE_CONDITION);
}

/* No Retreat: user.getTag(TrappedTag)?.tagType !== "NO_RETREAT" */
static double no_retreat(ARGS)
{
    UNUSED;
    const pr_battler_tag *tag = pr_find_battler_tag_class(u, PR_TAGCLASS_TRAPPED);
    return !tag || tag->type != BATTLER_TAG_TYPE_NO_RETREAT;
}

/* Stockpile: (user.getTag("STOCKPILING")?.stockpiledCount ?? 0) < 3 */
static double stockpile(ARGS)
{
    UNUSED;
    const pr_battler_tag *tag = pr_get_battler_tag(u, BATTLER_TAG_TYPE_STOCKPILING);
    return (tag ? tag->stockpiled_count : 0) < 3;
}

/* Spit Up, Swallow: tag = user.getTag(StockpilingTag); !!tag && tag.stockpiledCount > 0 */
static double has_stockpile(ARGS)
{
    UNUSED;
    const pr_battler_tag *tag = pr_find_battler_tag_class(u, PR_TAGCLASS_STOCKPILING);
    return tag && tag->stockpiled_count > 0;
}

/* target.getHeldItems().filter(i => i.isTransferable).length > 0 */
static double target_has_transferable_item(ARGS)
{
    UNUSED;
    NEED_TARGET;
    return pr_held_item_count(bt, t, true) > 0;
}

/* Telekinesis: invalidTelekinesisSpecies.has(target.species.speciesId) ? false
 * : speciesId !== GENGAR || !(formKey === "mega" || formKey === "gigantamax") */
static double telekinesis(ARGS)
{
    UNUSED;
    NEED_TARGET;
    for (unsigned i = 0; i < pr_set_invalid_telekinesis_species_n; i++)
        if (pr_set_invalid_telekinesis_species[i] == t->species) return 0;
    if (t->species != SPECIES_ID_GENGAR) return 1;
    const char *fk = pr_pokemon_form(t)->form_key;
    return !(fk && (!strcmp(fk, "mega") || !strcmp(fk, "gigantamax")));
}

/* Flower Shield: getField(true).some(p => p.isOfType(GRASS, {returnOriginalTypesIfStellar})
 * && !p.getTag(SemiInvulnerableTag)) */
static double flower_shield(ARGS)
{
    UNUSED;
    for (int i = 0; i < 4; i++) {
        const pr_pokemon *p = bt->field[i];
        if (is_active(p) && is_of_type_orig(p, POKEMON_TYPE_GRASS)
            && !pr_find_battler_tag_class(p, PR_TAGCLASS_SEMI_INVULNERABLE))
            return 1;
    }
    return 0;
}

/* Rototiller: [...getEnemyParty(), ...getPlayerParty()].some(p => p.isOfType(GRASS) && p.isGrounded())
 * (the bench's types aren't modelled: answered only when a Pokemon on the field settles it) */
static double rototiller(ARGS)
{
    UNUSED;
    for (int i = 0; i < 4; i++) {
        const pr_pokemon *p = bt->field[i];
        if (p && pr_is_of_type(p, POKEMON_TYPE_GRASS) && pr_is_grounded(bt, p, false)) return 1;
    }
    for (int side = 0; side < 2; side++) {
        if (!bt->n_party[side]) UNPORTED(PR_FEAT_PARTY);
        for (int k = 0; k < bt->n_party[side]; k++)
            if (!party_field_mon(bt, side, k)) UNPORTED(PR_FEAT_MOVE_CONDITION);
    }
    return 0;
}

/* Healing Wish, Lunar Dance: getPlayerParty().filter(p => p.isActive()).length > currentBattle.getBattlerCount()
 * (the player's party whoever the user is) */
static double player_bench_left(ARGS)
{
    UNUSED;
    if (!bt->n_party[0]) UNPORTED(PR_FEAT_PARTY);
    int n = 0;
    for (int k = 0; k < bt->n_party[0]; k++) n += party_hp(bt, 0, k) > 0;
    return n > (bt->double_battle ? 2 : 1);
}

/* Baton Pass and co.: party = user's side; party.some(p => p.isActive() && !p.isOnField()) */
static double side_bench_left(ARGS)
{
    UNUSED;
    int side = u->is_player ? 0 : 1;
    if (!bt->n_party[side]) UNPORTED(PR_FEAT_PARTY);
    for (int k = 0; k < bt->n_party[side]; k++)
        if (party_hp(bt, side, k) > 0 && !party_on_field(bt, side, k)) return 1;
    return 0;
}

/* Teleport (failTeleportCondition) */
static double teleport(ARGS)
{
    UNUSED;
    if (has_trainer(u)) {
        int side = u->is_player ? 0 : 1;
        if (!bt->n_party[side]) UNPORTED(PR_FEAT_PARTY);
        for (int k = 0; k < bt->n_party[side]; k++)
            if (!party_on_field(bt, side, k) && party_hp(bt, side, k) > 0) return 1;
        return 0;
    }
    if (bt->double_battle) return 0;
    if (is_of_type_orig(u, POKEMON_TYPE_GHOST) || pr_has_ability_with_attr(bt, u, PR_AC_RunSuccessAbAttr)) return 1;
    if (pr_arena_has_tag(bt, ARENA_TAG_TYPE_FAIRY_LOCK, ARENA_TAG_SIDE_BOTH)
        || pr_find_battler_tag_class(u, PR_TAGCLASS_TRAPPED))
        return 0;
    return 1;
}

/* Self-Destruct, Explosion, Mind Blown, Misty Explosion: FieldPreventExplosiveMovesAbAttr (Damp) on
 * every active Pokemon on the field, not simulated outside EnemyCommandPhase: !cancelled.value */
static double no_damp(ARGS)
{
    UNUSED;
    bool cancelled = false;
    for (int i = 0; i < 4; i++) {
        if (!is_active(bt->field[i])) continue;
        pr_ab_params pp = {.pokemon = bt->field[i], .move = -1, .flag = &cancelled};
        pr_apply_ab_attrs(bt, PR_AC_FieldPreventExplosiveMovesAbAttr, &pp);
    }
    return !cancelled;
}

const pr_closure_reg pr_condition_closures[] = {
    /* attribute conditions, any owner */
    {NULL, "7680b4c661ce", tag_overlap},
    {NULL, "e088dd3b9894", heal_full_hp},
    {NULL, "0f3b7651e995", heal_on_ally},
    {NULL, "17c62d0d309e", arena_tag_overlap},
    {NULL, "398a03ca7007", arena_trap_tag},
    {NULL, "c980f6dbe046", weather_change},
    {NULL, "5d78232ee079", terrain_change},
    {NULL, "4814bba736fb", cut_hp_boost},
    {NULL, "7c2561342733", add_substitute},
    {NULL, "2578463a5d9c", add_type},
    {NULL, "45f79cc88670", change_type},
    {NULL, "1de3d66e77e0", ability_change},
    {NULL, "f3628f69b87a", ability_copy},
    {NULL, "0654450cc655", force_switch_out},
    {NULL, "c4f48d7ed249", chilly_reception},
    {NULL, "8cf51e321985", protect_streak},
    {NULL, "d747fd904def", one_hit_ko},
    /* singletons shared by several moves: the owner is the first move that uses them */
    {"move:68", "988ffaba8576", counter_physical},
    {"move:243", "988ffaba8576", counter_special},
    {"move:368", "988ffaba8576", counter_both},
    {"move:775", "2421c1f6f4bb", insufficient_hp_3},
    {"move:868", "2421c1f6f4bb", insufficient_hp_2},
    /* closures that read no `this` */
    {NULL, "8b60a06559ea", move_phase_queued},
    {NULL, "acc82e269c88", battle_is_double},
    {NULL, "01ba08ada6fe", first_turn},
    {NULL, "7534ad7b0da5", target_not_ghost},
    {NULL, "664eee3d281a", leech_seed},
    {NULL, "f3af111c9c73", target_asleep},
    {NULL, "251b1ee5e01f", user_asleep},
    {NULL, "9cae75782266", attract},
    {NULL, "fc4164dba74c", captivate},
    {NULL, "4ae772582497", refresh},
    {NULL, "4c105ac74117", purify},
    {NULL, "d4a00b56a3d8", target_attacking},
    {NULL, "b00d1e28fe0c", upper_hand},
    {NULL, "e479e07251e3", target_not_acted},
    {NULL, "30f7e3218752", no_ignore_flying},
    {NULL, "00f4e7fd63b1", no_dragon_cheer},
    {NULL, "564a1c5761d3", no_crit_boost},
    {NULL, "0a3cf7d0d45d", no_substitute},
    {NULL, "a302d17ef57c", atk_not_min},
    {NULL, "f0a123f57ab2", user_fire},
    {NULL, "1f16c84ce6e4", user_electric},
    {NULL, "302195250edf", hail_or_snow},
    {NULL, "d1c7b5a07b9d", any_terrain},
    {NULL, "33bf21e1c6db", endeavor},
    {NULL, "a47aed0a7ddf", not_boss},
    {NULL, "acff4777cd7d", pollen_puff_heal_block},
    {NULL, "3b82dbf46b74", not_repeated_success},
    {NULL, "f73e15f05b6b", not_final_boss},
    {NULL, "8ff758b28223", yawn},
    {NULL, "0a338d0d1122", disable},
    {NULL, "e781731f96ae", spite},
    {NULL, "0bef0addc6de", conversion},
    {NULL, "40bfade286cb", conversion_2},
    {NULL, "ad41a5ecc68c", last_resort},
    {NULL, "f0db5bd63f20", plus_minus_on_side},
    {NULL, "69bfa2b8c665", skill_swap},
    {NULL, "569ef05eaf81", entrainment},
    {NULL, "6194ebefe573", gastro_acid},
    {NULL, "cff0c29323e5", reflect_type},
    {NULL, "8d10d52219b0", no_retreat},
    {NULL, "f3ac317ad394", stockpile},
    {NULL, "a08257ae6829", has_stockpile},
    {NULL, "42ad58c7b206", target_has_transferable_item},
    {NULL, "2fe7d4d8b06f", telekinesis},
    {NULL, "3719da446931", flower_shield},
    {NULL, "df7cf6a239ae", rototiller},
    {NULL, "fec480e1d706", player_bench_left},
    {NULL, "f4be08971a26", side_bench_left},
    {NULL, "393096486706", teleport},
    {NULL, "286341e0a59e", no_damp},
};
const int pr_condition_closures_n = sizeof pr_condition_closures / sizeof pr_condition_closures[0];
