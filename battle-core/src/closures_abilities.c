/* Hand ports of upstream ability closures: attribute conditions, extraConditions and
 * ability conditions. Same scheme as src/closures_moves.c: each registry entry names the
 * closure it was written against (owner + hash), so an upstream change reads as
 * unported. Argument order follows the closure: (user, target, move) for attack
 * conditions, (target, user, move) for defend conditions, (pokemon) otherwise.
 * Named helpers the closures call (anyTypeMoveConversionCondition) are not part of the
 * hash; check them by hand after an upstream bump. */
#include "pr/battle.h"

#define UNPORTED(cls) do { pr_note_unported(bt, (cls)); return 0; } while (0)
#define ARGS pr_battle *bt, const pr_closure_info *self, const pr_pokemon *a, const pr_pokemon *b, int m
#define UNUSED (void)bt; (void)self; (void)a; (void)b; (void)m

/* The single value a capture was probed at (boostedType, moveType...). */
static int cap_value(const pr_closure_info *self)
{
    for (int v = -1; v < 31; v++)
        if (pr_cap_has(self, v)) return v;
    return -99;
}

/* () => !arena.weather?.isEffectSuppressed() && weatherTypes.includes(arena.weatherType) */
static double weather_in(ARGS)
{
    UNUSED;
    if (pr_weather_effect_suppressed(bt)) return 0;
    return pr_cap_has(self, bt->weather);
}

/* (_pokemon) => !!terrainType && terrainTypes.indexOf(terrainType) > -1 */
static double terrain_in(ARGS)
{
    UNUSED;
    return bt->terrain != 0 && pr_cap_has(self, bt->terrain);
}

/* (pokemon, _defender, move) => pokemon?.getMoveType(move) === boostedType */
static double user_move_type_is(ARGS)
{
    UNUSED;
    return a && pr_get_move_type(bt, a, m) == cap_value(self);
}

/* (_target, user, move) => user.getMoveType(move) === moveType */
static double attacker_move_type_is(ARGS)
{
    UNUSED;
    return pr_get_move_type(bt, b, m) == cap_value(self);
}

/* (_pokemon, _defender, move) => boostedCategories.includes(move.category) */
static double category_in(ARGS)
{
    UNUSED;
    return pr_cap_has(self, pr_moves[m].category);
}

/* move.category === 0 / === 1 (either argument order) */
static double physical(ARGS) { UNUSED; return pr_moves[m].category == MOVE_CATEGORY_PHYSICAL; }
static double special(ARGS) { UNUSED; return pr_moves[m].category == MOVE_CATEGORY_SPECIAL; }

/* (_user, _target, move) => move.hasFlag(F) */
#define HAS_FLAG(name, F) static double name(ARGS) { UNUSED; return (pr_moves[m].flags & (F)) != 0; }
HAS_FLAG(flag_punching, MOVE_FLAGS_PUNCHING_MOVE)
HAS_FLAG(flag_reckless, MOVE_FLAGS_RECKLESS_MOVE)
HAS_FLAG(flag_biting, MOVE_FLAGS_BITING_MOVE)
HAS_FLAG(flag_pulse, MOVE_FLAGS_PULSE_MOVE)
HAS_FLAG(flag_contact, MOVE_FLAGS_MAKES_CONTACT)
HAS_FLAG(flag_sound, MOVE_FLAGS_SOUND_BASED)
HAS_FLAG(flag_slicing, MOVE_FLAGS_SLICING_MOVE)
HAS_FLAG(flag_wind, MOVE_FLAGS_WIND_MOVE)

/* (pokemon, attacker, move) => pokemon !== attacker && move.hasFlag(F) */
#define IMMUNE_FLAG(name, F) static double name(ARGS) { UNUSED; return a != b && (pr_moves[m].flags & (F)) != 0; }
IMMUNE_FLAG(immune_sound, MOVE_FLAGS_SOUND_BASED)
IMMUNE_FLAG(immune_powder, MOVE_FLAGS_POWDER_MOVE)
IMMUNE_FLAG(immune_ballbomb, MOVE_FLAGS_BALLBOMB_MOVE)

/* Wind Rider: pokemon !== attacker && move.hasFlag(WIND_MOVE) && move.category !== STATUS */
static double immune_wind(ARGS)
{
    UNUSED;
    return a != b && (pr_moves[m].flags & MOVE_FLAGS_WIND_MOVE) && pr_moves[m].category != MOVE_CATEGORY_STATUS;
}

/* Good as Gold: pokemon !== attacker && move.category === STATUS
 *   && ![ENEMY_SIDE, BOTH_SIDES, USER_SIDE].includes(move.moveTarget) */
static double good_as_gold(ARGS)
{
    UNUSED;
    int t = pr_moves[m].target;
    return a != b && pr_moves[m].category == MOVE_CATEGORY_STATUS && t != MOVE_TARGET_ENEMY_SIDE
           && t != MOVE_TARGET_BOTH_SIDES && t != MOVE_TARGET_USER_SIDE;
}

/* Telepathy: pokemon.getAlly() === attacker && move.is("AttackMove") */
static double telepathy(ARGS)
{
    UNUSED;
    return pr_get_ally(bt, a) == b && pr_move_is_attack(m);
}

/* Plus / Minus: currentBattle.double && [PLUS, MINUS].some(a => p.getAlly()?.hasAbility(a)) */
static double plus_minus(ARGS)
{
    UNUSED;
    if (!bt->double_battle) return 0;
    const pr_pokemon *ally = pr_get_ally(bt, a);
    return ally && (pr_has_ability_active(bt, ally, ABILITY_ID_PLUS) || pr_has_ability_active(bt, ally, ABILITY_ID_MINUS));
}

/* Guts, Marvel Scale, Quick Feet: !!pokemon.status || pokemon.hasAbility(COMATOSE) */
static double status_or_comatose(ARGS)
{
    UNUSED;
    return a->status != STATUS_EFFECT_NONE || pr_has_ability_active(bt, a, ABILITY_ID_COMATOSE);
}

/* Quick Feet: pokemon.status ? pokemon.status.effect === PARALYSIS : false */
static double paralyzed(ARGS) { UNUSED; return a->status == STATUS_EFFECT_PARALYSIS; }

/* Tangled Feet: !!pokemon.getTag("CONFUSED") */
static double confused(ARGS) { UNUSED; return pr_has_battler_tag(bt, a, BATTLER_TAG_TYPE_CONFUSED); }

static bool set_has(const uint16_t *v, unsigned n, int x)
{
    for (unsigned i = 0; i < n; i++)
        if (v[i] == x) return true;
    return false;
}

/* anyTypeMoveConversionCondition(user, target, move) */
static bool any_type_conversion(const pr_pokemon *user, int m)
{
    if (set_has(pr_set_no_ability_type_override_moves, pr_set_no_ability_type_override_moves_n, m)) return false;
    if (!user->terastallized) return true;
    if (m == MOVE_ID_TERA_BLAST) return false;
    if (m == MOVE_ID_TERA_STARSTORM && pr_get_tera_type(user) == POKEMON_TYPE_STELLAR
        && user->species == SPECIES_ID_TERAPAGOS)
        return false;
    return true;
}

/* Normalize: anyTypeMoveConversionCondition written out inline */
static double normalize(ARGS) { UNUSED; return any_type_conversion(a, m); }
/* Refrigerate, Pixilate, Aerilate, Galvanize, Dragonize:
 * move.type === NORMAL && anyTypeMoveConversionCondition(user, target, move) */
static double ate(ARGS) { UNUSED; return pr_moves[m].type == POKEMON_TYPE_NORMAL && any_type_conversion(a, m); }
/* Liquid Voice: move.hasFlag(SOUND_BASED) && anyTypeMoveConversionCondition(...) */
static double liquid_voice(ARGS)
{
    UNUSED;
    return (pr_moves[m].flags & MOVE_FLAGS_SOUND_BASED) && any_type_conversion(a, m);
}

/* Technician: power after VariablePowerAttr <= 60 */
static double technician(ARGS)
{
    UNUSED;
    double power = pr_moves[m].power;
    pr_apply_variable_power(bt, a, b, m, &power);
    return power <= 60;
}

/* Tinted Lens: (target?.getMoveEffectiveness(user, move) ?? 1) <= .5 */
static double tinted_lens(ARGS)
{
    UNUSED;
    return (b ? pr_get_move_effectiveness(bt, b, a, m) : 1) <= .5;
}

/* Neuroforce: (target?.getMoveEffectiveness(user, move) ?? 1) >= 2 */
static double neuroforce(ARGS)
{
    UNUSED;
    return (b ? pr_get_move_effectiveness(bt, b, a, m) : 1) >= 2;
}

/* Filter, Solid Rock, Prism Armor: target.getMoveEffectiveness(user, move) >= 2 */
static double super_effective_on_me(ARGS)
{
    UNUSED;
    return pr_get_move_effectiveness(bt, a, b, m) >= 2;
}

/* Sheer Force: move.chance >= 1 */
static double has_secondary(ARGS) { UNUSED; return pr_moves[m].chance >= 1; }

/* Defeatist: pokemon.getHpRatio() <= .5 */
static double defeatist(ARGS) { UNUSED; return pr_get_hp_ratio(a, false) <= .5; }

/* Multiscale, Shadow Shield: target.isFullHp() */
static double full_hp(ARGS) { UNUSED; return a->hp >= pr_get_stat(a, STAT_HP, true); }

/* Toxic Boost: move.category === PHYSICAL && (user.status?.effect === POISON || TOXIC) */
static double toxic_boost(ARGS)
{
    UNUSED;
    return pr_moves[m].category == MOVE_CATEGORY_PHYSICAL
           && (a->status == STATUS_EFFECT_POISON || a->status == STATUS_EFFECT_TOXIC);
}

/* Flare Boost: move.category === SPECIAL && user.status?.effect === BURN */
static double flare_boost(ARGS)
{
    UNUSED;
    return pr_moves[m].category == MOVE_CATEGORY_SPECIAL && a->status == STATUS_EFFECT_BURN;
}

/* Merciless: target?.status?.effect === TOXIC || target?.status?.effect === POISON */
static double merciless(ARGS)
{
    UNUSED;
    return b && (b->status == STATUS_EFFECT_TOXIC || b->status == STATUS_EFFECT_POISON);
}

/* Stakeout: !!target?.turnData.switchedInThisTurn */
static double stakeout(ARGS) { UNUSED; return b && b->switched_in_this_turn; }

/* Parental Bond: user.turnData.hitCount > 1 && user.turnData.hitsLeft === 1
 *   && move.canBeMultiStrikeEnhanced(user, true, target) */
static double parental_bond(ARGS)
{
    UNUSED;
    return a->hit_count > 1 && a->hits_left == 1 && pr_move_can_be_multi_strike_enhanced_ex(bt, m, a, true, b);
}

/* Analytic: !globalScene.phaseManager.hasPhaseOfType("MovePhase", phase => phase.pokemon.id !== user.id),
 * i.e. no other Pokemon still has a move queued this turn */
static double analytic(ARGS)
{
    UNUSED;
    for (int i = 0; i < bt->n_queued_movers; i++)
        if (bt->queued_movers[i] != a->id) return 0;
    return 1;
}

/* Prankster: (_pokemon, move) => move.category === STATUS */
static double prankster(ARGS) { UNUSED; return pr_moves[m].category == MOVE_CATEGORY_STATUS; }

/* Gale Wings: pokemon.isFullHp() && pokemon.getMoveType(move) === FLYING */
static double gale_wings(ARGS)
{
    UNUSED;
    return a->hp >= pr_get_stat(a, STAT_HP, true) && pr_get_move_type(bt, a, m) == POKEMON_TYPE_FLYING;
}

/* Triage: move.hasFlag(HEALING_MOVE) */
static double triage(ARGS) { UNUSED; return (pr_moves[m].flags & MOVE_FLAGS_HEALING_MOVE) != 0; }

/* Unseen Fist, Piercing Drill: !!target && target has a ProtectedTag && contact applies. */
static double protected_contact(ARGS)
{
    UNUSED;
    return b && pr_has_battler_tag_class(bt, b, PR_TAGCLASS_PROTECTED)
           && pr_move_does_flag_effect_apply(bt, m, MOVE_FLAGS_MAKES_CONTACT, a, NULL);
}

/* Fluffy, Aura Guard: move.doesFlagEffectApply({flag: MAKES_CONTACT, user, target}) */
static double contact_on_me(ARGS)
{
    UNUSED;
    return pr_move_does_flag_effect_apply(bt, m, MOVE_FLAGS_MAKES_CONTACT, b, a);
}

/* Fluffy: user.getMoveType(move) === FIRE */
static double attacker_fire(ARGS) { UNUSED; return pr_get_move_type(bt, b, m) == POKEMON_TYPE_FIRE; }

/* Aura Break: getField(true).some(p => p.hasAbility(DARK_AURA / FAIRY_AURA)) */
static bool field_has_ability(pr_battle *bt, int ability)
{
    for (int i = 0; i < 4; i++) {
        const pr_pokemon *p = bt->field[i];
        if (p && p->hp > 0 && pr_has_ability_active(bt, p, ability)) return true;
    }
    return false;
}
static double field_dark_aura(ARGS) { UNUSED; return field_has_ability(bt, ABILITY_ID_DARK_AURA); }
static double field_fairy_aura(ARGS) { UNUSED; return field_has_ability(bt, ABILITY_ID_FAIRY_AURA); }

/* Rivalry: user.gender !== -1 && target?.gender !== -1 && user.gender === / !== target?.gender */
static double same_gender(ARGS) { UNUSED; return b && a->gender != -1 && b->gender != -1 && a->gender == b->gender; }
static double opposite_gender(ARGS) { UNUSED; return b && a->gender != -1 && b->gender != -1 && a->gender != b->gender; }

/* Disguise: () => true */
static double always(ARGS) { UNUSED; return 1; }

/* Hunger Switch: (pokemon) => !pokemon.isTerastallized */
static double not_terastallized(ARGS) { UNUSED; return !a->terastallized; }

/* Flower Veil: (target) => target.isOfType(GRASS, {returnOriginalTypesIfStellar: true}) */
static double grass_target(ARGS)
{
    UNUSED;
    int8_t ty[3];
    int n = pr_get_types(a, ty, PR_TYPES_ORIGINAL_IF_STELLAR);
    for (int i = 0; i < n; i++)
        if (ty[i] == POKEMON_TYPE_GRASS) return 1;
    return 0;
}

/* Flower Veil status: (target, source) => !!source && target.id !== source.id
 *   && target.isOfType(GRASS, {returnOriginalTypesIfStellar: true}) */
static double flower_veil_status(ARGS)
{
    UNUSED;
    return b && a->id != b->id && grass_target(bt, self, a, NULL, -1) != 0;
}

/* Shields Down: (p) => p.formIndex < 7 (Meteor Form) */
static double meteor_form(ARGS) { UNUSED; return a->form < 7; }

/* (_user, _target, move) => move.category !== STATUS: PostAttackAbAttr's default attackCondition and
 * several on-hit conditions */
static double not_status(ARGS) { UNUSED; return pr_moves[m].category != MOVE_CATEGORY_STATUS; }

/* Stench's chance, called as chance(the one hit, the attacker, move):
 * (user, target, move) => !move.hasAttr("FlinchAttr") && !move.hitsSubstitute(user, target) ? 10 : 0 */
static double stench_chance(ARGS)
{
    UNUSED;
    return !pr_move_has_attr((uint16_t)m, PR_AC_FlinchAttr) && !pr_move_hits_substitute(bt, m, a, b) ? 10 : 0;
}

/* On-hit stat abilities: (_target, user, move) => the move's type for the attacker is one of the
 * listed ones && move.category !== STATUS (Justified, Rattled, Water Compaction, Steam Engine,
 * Thermal Exchange) */
static bool attacker_type_in(pr_battle *bt, const pr_pokemon *user, int m, int t1, int t2, int t3)
{
    int t = pr_get_move_type(bt, user, m);
    return t == t1 || t == t2 || t == t3;
}
static double justified(ARGS)
{
    UNUSED;
    return pr_get_move_type(bt, b, m) == POKEMON_TYPE_DARK && pr_moves[m].category != MOVE_CATEGORY_STATUS;
}
static double rattled(ARGS)
{
    UNUSED;
    return pr_moves[m].category != MOVE_CATEGORY_STATUS
           && attacker_type_in(bt, b, m, POKEMON_TYPE_DARK, POKEMON_TYPE_BUG, POKEMON_TYPE_GHOST);
}
static double water_compaction(ARGS)
{
    UNUSED;
    return pr_get_move_type(bt, b, m) == POKEMON_TYPE_WATER && pr_moves[m].category != MOVE_CATEGORY_STATUS;
}
static double steam_engine(ARGS)
{
    UNUSED;
    return pr_moves[m].category != MOVE_CATEGORY_STATUS
           && attacker_type_in(bt, b, m, POKEMON_TYPE_FIRE, POKEMON_TYPE_WATER, POKEMON_TYPE_WATER);
}
static double thermal_exchange(ARGS)
{
    UNUSED;
    return pr_get_move_type(bt, b, m) == POKEMON_TYPE_FIRE && pr_moves[m].category != MOVE_CATEGORY_STATUS;
}

/* Ball Fetch: (pokemon) => !pokemon.waveData.abilitiesApplied.has(ability) */
static double ball_fetch_unused(ARGS) { UNUSED; return !pr_wave_ability_applied(a, ABILITY_ID_BALL_FETCH); }

/* Color Change, Pickpocket, Wimp Out, Emergency Exit, Berserk, Anger Shell (sheerForceHitDisableAbCondition):
 *   const lastReceivedAttack = pokemon.turnData.attacksReceived[0];
 *   if (!lastReceivedAttack) return true;
 *   const lastAttacker = pokemon.getOpponents().find(p => p.id === lastReceivedAttack.sourceId);
 *   if (!lastAttacker) return true;
 *   return !(allMoves[lastReceivedAttack.move].chance >= 0 && lastAttacker.hasAbility(SHEER_FORCE));
 * getOpponents() is the other side's field filtered by isActive(true): on the field and not
 * fainted (isAllowedInChallenge is true without challenges, which the port doesn't model). */
static double not_sheer_force_hit(ARGS)
{
    UNUSED;
    if (a->n_attacks_received == 0) return 1;
    const pr_attack_record *r = &a->attacks_received[0];
    const pr_pokemon *attacker = pr_field_pokemon_by_id(bt, r->source_id);
    if (!attacker || attacker->is_player == a->is_player || attacker->hp <= 0) return 1;
    return !(pr_moves[r->move].chance >= 0 && pr_has_ability_active(bt, attacker, ABILITY_ID_SHEER_FORCE));
}

const pr_closure_reg pr_ability_closures[] = {
    {"ability:8", "5d9de4139911", weather_in}, /* closure 460 */
    {"ability:33", "ea09345d0147", weather_in}, /* closure 464 */
    {"ability:34", "d82d9f5f2c0a", weather_in}, /* closure 465 */
    {"ability:43", "4eb3acf700c3", immune_sound}, /* closure 468 */
    {"ability:47", "84dabf19a95d", attacker_move_type_is}, /* closure 470 */
    {"ability:47", "4536aab3a2f9", attacker_move_type_is}, /* closure 471 */
    {"ability:55", "5c20143231ad", physical}, /* closure 472 */
    {"ability:57", "92b4561d36ea", plus_minus}, /* closure 474 */
    {"ability:58", "92b4561d36ea", plus_minus}, /* closure 475 */
    {"ability:62", "acdfaaf775e5", status_or_comatose}, /* closure 477 */
    {"ability:63", "acdfaaf775e5", status_or_comatose}, /* closure 478 */
    {"ability:65", "f108355fbd42", user_move_type_is}, /* closure 479 */
    {"ability:66", "0248242494c6", user_move_type_is}, /* closure 480 */
    {"ability:67", "2e0cd10e7d06", user_move_type_is}, /* closure 481 */
    {"ability:68", "870c66d29305", user_move_type_is}, /* closure 482 */
    {"ability:77", "b590762ea788", confused}, /* closure 486 */
    {"ability:81", "ea591d93bc79", weather_in}, /* closure 489 */
    {"ability:85", "84dabf19a95d", attacker_move_type_is}, /* closure 490 */
    {"ability:87", "84dabf19a95d", attacker_move_type_is}, /* closure 491 */
    {"ability:89", "9dc744cf99b3", flag_punching}, /* closure 492 */
    {"ability:94", "d82d9f5f2c0a", weather_in}, /* closure 495 */
    {"ability:95", "96431c7e279c", paralyzed}, /* closure 496 */
    {"ability:95", "acdfaaf775e5", status_or_comatose}, /* closure 497 */
    {"ability:96", "497d1fe60e56", normalize}, /* closure 498 */
    {"ability:101", "6eb4bea1f968", technician}, /* closure 502 */
    {"ability:110", "89270e5952c9", tinted_lens}, /* closure 508 */
    {"ability:111", "d098fc8f0f4a", super_effective_on_me}, /* closure 509 */
    {"ability:116", "d098fc8f0f4a", super_effective_on_me}, /* closure 510 */
    {"ability:120", "38c3300c6daa", flag_reckless}, /* closure 512 */
    {"ability:122", "7b1e78eb7155", weather_in}, /* closure 513 */
    {"ability:122", "7b1e78eb7155", weather_in}, /* closure 514 */
    {"ability:122", "7b1e78eb7155", weather_in}, /* closure 515 */
    {"ability:122", "7b1e78eb7155", weather_in}, /* closure 516 */
    {"ability:125", "98a0f5bf85f4", has_secondary}, /* closure 518 */
    {"ability:129", "3458fafd4e54", defeatist}, /* closure 520 */
    {"ability:136", "d53c3e2c5456", full_hp}, /* closure 524 */
    {"ability:137", "2e5ab55a80cb", toxic_boost}, /* closure 525 */
    {"ability:138", "8559a2dbb04f", flare_boost}, /* closure 526 */
    {"ability:140", "8966748d0c4c", telepathy}, /* closure 528 */
    {"ability:142", "7f985462f790", immune_powder}, /* closure 529 */
    {"ability:146", "5d9de4139911", weather_in}, /* closure 531 */
    {"ability:159", "80f23dd0757f", user_move_type_is}, /* closure 537 */
    {"ability:159", "f4792b9a8787", user_move_type_is}, /* closure 538 */
    {"ability:159", "58cfd3d92e64", user_move_type_is}, /* closure 539 */
    {"ability:159", "5d9de4139911", weather_in}, /* closure 540 */
    {"ability:169", "696705e9c797", physical}, /* closure 552 */
    {"ability:171", "f3155daa109a", immune_ballbomb}, /* closure 554 */
    {"ability:173", "a69a8a2263b9", flag_biting}, /* closure 556 */
    {"ability:174", "f470f2ec7c17", ate}, /* closure 557 */
    {"ability:178", "8c67880cdf7b", flag_pulse}, /* closure 559 */
    {"ability:179", "08b57ff99c93", terrain_in}, /* closure 561 */
    {"ability:181", "a6d0ede6d9db", flag_contact}, /* closure 562 */
    {"ability:185", "d186b5950f14", parental_bond}, /* closure 564 */
    {"ability:186", "365edc308c43", user_move_type_is}, /* closure 567 */
    {"ability:187", "d61b52573a47", user_move_type_is}, /* closure 569 */
    {"ability:188", "b70b1fdc1369", field_dark_aura}, /* closure 570 */
    {"ability:188", "365edc308c43", user_move_type_is}, /* closure 571 */
    {"ability:188", "1890fa55ec41", field_fairy_aura}, /* closure 572 */
    {"ability:188", "d61b52573a47", user_move_type_is}, /* closure 573 */
    {"ability:196", "4dc427d74aa6", merciless}, /* closure 580 */
    {"ability:198", "6c8a26cc37c7", stakeout}, /* closure 586 */
    {"ability:199", "84dabf19a95d", attacker_move_type_is}, /* closure 587 */
    {"ability:199", "2e0cd10e7d06", user_move_type_is}, /* closure 588 */
    {"ability:200", "58cfd3d92e64", user_move_type_is}, /* closure 589 */
    {"ability:202", "ea591d93bc79", weather_in}, /* closure 590 */
    {"ability:204", "232fef5443f6", liquid_voice}, /* closure 591 */
    {"ability:207", "98bd0df457a0", terrain_in}, /* closure 593 */
    {"ability:217", "a1461cb09481", category_in}, /* closure 615 */
    {"ability:218", "5ce535a80302", contact_on_me}, /* closure 616 */
    {"ability:218", "4e0f11be8863", attacker_fire}, /* closure 617 */
    {"ability:231", "d53c3e2c5456", full_hp}, /* closure 624 */
    {"ability:232", "d098fc8f0f4a", super_effective_on_me}, /* closure 625 */
    {"ability:233", "04f364d430b7", neuroforce}, /* closure 626 */
    {"ability:244", "8b9d34e20481", flag_sound}, /* closure 632 */
    {"ability:244", "1cf7d47a8329", flag_sound}, /* closure 633 */
    {"ability:246", "ff2f11679115", special}, /* closure 635 */
    {"ability:249", "c36a15e99421", category_in}, /* closure 640 */
    {"ability:252", "58cfd3d92e64", user_move_type_is}, /* closure 641 */
    {"ability:260", "4197cfd653a4", protected_contact}, /* closure 646 */
    {"ability:262", "7d36d7d3c34b", user_move_type_is}, /* closure 647 */
    {"ability:263", "f8e1ef8e0824", user_move_type_is}, /* closure 648 */
    {"ability:272", "da9c6f3897fd", attacker_move_type_is}, /* closure 652 */
    {"ability:274", "35c5a2c7c42c", immune_wind}, /* closure 653 */
    {"ability:276", "80f23dd0757f", user_move_type_is}, /* closure 654 */
    {"ability:283", "bea598419963", good_as_gold}, /* closure 663 */
    {"ability:288", "d82d9f5f2c0a", weather_in}, /* closure 670 */
    {"ability:289", "98bd0df457a0", terrain_in}, /* closure 673 */
    {"ability:292", "f907f78e688a", flag_slicing}, /* closure 674 */
    {"ability:311", "4197cfd653a4", protected_contact}, /* closure 682 */
    {"ability:316", "0248242494c6", user_move_type_is}, /* closure 683 */
    {"ability:319", "5ce535a80302", contact_on_me}, /* closure 684 */
    {"ability:16", "4132be35cafb", not_sheer_force_hit}, /* closure 462 */
    {"ability:79", "6c9fa5043bc9", same_gender}, /* closure 487 */
    {"ability:79", "c6599449e732", opposite_gender}, /* closure 488 */
    {"ability:93", "ea09345d0147", weather_in}, /* closure 494 */
    {"ability:102", "d82d9f5f2c0a", weather_in}, /* closure 503 */
    {"ability:209", "24044144b7ba", always}, /* closure 597 */
    {"ability:237", "8e6e1b7d5953", ball_fetch_unused}, /* closure 629 */
    {"ability:248", "696705e9c797", physical}, /* closure 638 */
    {"ability:258", "95f58e84b99d", not_terastallized}, /* closure 645 */
    {"ability:148", "1ac928f78f0f", analytic}, /* closure 532 */
    {"ability:158", "74fa5cb6b41e", prankster}, /* closure 536 */
    {"ability:177", "1b589a349668", gale_wings}, /* closure 558 */
    {"ability:205", "e8d66e7b435a", triage}, /* closure 592 */
    {"ability:166", "3adbae502f95", grass_target}, /* closure 551 */
    {"ability:166", "87c14ce5fba5", flower_veil_status}, /* closure 549 */
    {"ability:197", "193c76db9e37", meteor_form}, /* closures 584, 585 */
    /* on-hit abilities (PostDefend, PostAttack). Sources with no captures: any owner. */
    {NULL, "0a3803cd6ba8", not_status},    /* Stench, Poison Touch, Magician, Toxic Chain */
    {NULL, "ae78ec2f0596", not_status},    /* Stamina, Cotton Down, Sand Spit, Electromorphosis */
    {NULL, "5ce535a80302", contact_on_me}, /* Pickpocket, Tangling Hair */
    {NULL, "696705e9c797", physical},      /* Weak Armor, Toxic Debris */
    {"ability:1", "0440d28dc66e", stench_chance}, /* closure 457 */
    {"ability:154", "4663665ae292", justified}, /* closure 534 */
    {"ability:155", "3a028c14a4c8", rattled}, /* closure 535 */
    {"ability:183", "a74dff334353", flag_contact}, /* closure 563: Gooey, move.hasFlag(MAKES_CONTACT) */
    {"ability:195", "e5afa457a71b", water_compaction}, /* closure 579 */
    {"ability:243", "5ae0daa1e7f9", steam_engine}, /* closure 631 */
    {"ability:270", "18cb24a88a08", thermal_exchange}, /* closure 651 */
    {"ability:277", "4043a61eeca7", flag_wind}, /* closure 655: Wind Power */
};
const int pr_ability_closures_n = (int)(sizeof pr_ability_closures / sizeof pr_ability_closures[0]);
