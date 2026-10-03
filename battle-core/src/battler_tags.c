/* Battler tags being added, lapsed and removed: Pokemon.addTag, lapseTag, lapseTags, removeTag,
 * removeTagsBySourceId (src/field/pokemon.ts) and, per tag type, what getBattlerTag() builds
 * (src/data/battler-tags.ts): its constructor's fields, canAdd, onAdd, onOverlap, lapse and onRemove.
 *
 * Only some tag types have those ported (pr_tag_type_ported). The others still take part where
 * upstream would reach them: a lapse or removal that would run one of their methods reports
 * the tag type (PR_FEAT_TAG_0 + type) and leaves the tag as it is. Which lapses reach a tag
 * comes from the lapse types upstream builds each tag with (pr_battler_tags_info, dumped). */
#include "pr/engine.h"
#include <math.h>
#include <string.h>

/* The tag types whose constructor, canAdd, onAdd, onOverlap, lapse and onRemove are ported. */
bool pr_tag_type_ported(int type)
{
    switch (type) {
    case BATTLER_TAG_TYPE_CHARGING:     /* SerializableBattlerTag(CHARGING, CUSTOM, 1, sourceMove, sourceId) */
    case BATTLER_TAG_TYPE_FLYING:       /* SemiInvulnerableTag: onAdd / onRemove only hide and show the sprite */
    case BATTLER_TAG_TYPE_UNDERGROUND:
    case BATTLER_TAG_TYPE_UNDERWATER:
    case BATTLER_TAG_TYPE_HIDDEN:
    case BATTLER_TAG_TYPE_FLINCHED:     /* FlinchedTag */
    case BATTLER_TAG_TYPE_CONFUSED:     /* ConfusedTag */
    case BATTLER_TAG_TYPE_FRENZY:       /* FrenzyTag (MoveLockTag): Outrage, Thrash, Petal Dance, Raging Fury */
    case BATTLER_TAG_TYPE_TRAPPED:      /* TrappedTag: Mean Look, Block, Spider Web... */
    case BATTLER_TAG_TYPE_NO_RETREAT:   /* NoRetreatTag */
    case BATTLER_TAG_TYPE_ALWAYS_GET_HIT:          /* Glaive Rush: SerializableBattlerTag(PRE_MOVE, 1) */
    case BATTLER_TAG_TYPE_RECEIVE_DOUBLE_DAMAGE:
    case BATTLER_TAG_TYPE_MINIMIZED:    /* MinimizeTag */
    case BATTLER_TAG_TYPE_CHARGED:      /* TypeBoostTag(CHARGED, sourceMove, ELECTRIC, 2, oneUse) */
    case BATTLER_TAG_TYPE_FIRE_BOOST:   /* TypeBoostTag(FIRE_BOOST, sourceMove, FIRE, 1.5): Flash Fire */
    case BATTLER_TAG_TYPE_TAR_SHOT:     /* TarShotTag */
    case BATTLER_TAG_TYPE_AUTOTOMIZED:  /* AutotomizedTag */
    case BATTLER_TAG_TYPE_RECHARGING:   /* RechargingTag: Hyper Beam and co. */
    case BATTLER_TAG_TYPE_SEEDED:       /* SeedTag: Leech Seed */
    case BATTLER_TAG_TYPE_SUBSTITUTE:   /* SubstituteTag: Substitute, Shed Tail */
    case BATTLER_TAG_TYPE_ENDURING:     /* EnduringTag: Endure */
    case BATTLER_TAG_TYPE_IGNORE_FLYING:  /* GroundedTag: Smack Down, Thousand Arrows (CUSTOM, 1 turn) */
    case BATTLER_TAG_TYPE_IGNORE_GHOST:   /* ExposedTag: Foresight, Odor Sleuth (CUSTOM, 1 turn) */
    case BATTLER_TAG_TYPE_IGNORE_DARK:    /* ExposedTag: Miracle Eye */
    case BATTLER_TAG_TYPE_SHELL_TRAP:     /* ShellTrapTag: TURN_END and AFTER_HIT, 1 turn */
    case BATTLER_TAG_TYPE_BEAK_BLAST_CHARGING:  /* BeakBlastChargingTag: PRE_MOVE, TURN_END, AFTER_HIT, 1 turn */
    case BATTLER_TAG_TYPE_IGNORE_ACCURACY:  /* Lock-On, Mind Reader: SerializableBattlerTag(TURN_END, 2) */
    case BATTLER_TAG_TYPE_ALWAYS_CRIT:      /* Laser Focus: the same */
    case BATTLER_TAG_TYPE_PERISH_SONG:      /* PerishSongTag */
    case BATTLER_TAG_TYPE_ROOSTED:          /* RoostedTag */
    case BATTLER_TAG_TYPE_THROAT_CHOPPED:   /* ThroatChoppedTag (MoveRestrictionBattlerTag) */
    case BATTLER_TAG_TYPE_HEAL_BLOCK:       /* HealBlockTag (MoveRestrictionBattlerTag) */
    case BATTLER_TAG_TYPE_AQUA_RING:        /* AquaRingTag */
    case BATTLER_TAG_TYPE_INGRAIN:          /* IngrainTag (TrappedTag) */
    case BATTLER_TAG_TYPE_OCTOLOCK:         /* OctolockTag (TrappedTag) */
    case BATTLER_TAG_TYPE_RAGE:             /* RageTag: PRE_MOVE and AFTER_HIT, 1 turn */
    case BATTLER_TAG_TYPE_PSYCHO_SHIFT:     /* PsychoShiftTag: AFTER_MOVE, 1 turn */
    case BATTLER_TAG_TYPE_DISABLED:         /* DisabledTag (MoveRestrictionBattlerTag) */
    case BATTLER_TAG_TYPE_TAUNT:            /* TauntTag (MoveRestrictionBattlerTag) */
    case BATTLER_TAG_TYPE_TORMENT:          /* TormentTag (MoveRestrictionBattlerTag) */
    case BATTLER_TAG_TYPE_ENCORE:           /* EncoreTag (MoveRestrictionBattlerTag) */
    case BATTLER_TAG_TYPE_INFATUATED:       /* InfatuatedTag: Attract, Cute Charm */
    case BATTLER_TAG_TYPE_SALT_CURED:       /* SaltCuredTag */
    case BATTLER_TAG_TYPE_CURSED:           /* CursedTag: a Ghost's Curse */
    case BATTLER_TAG_TYPE_NIGHTMARE:        /* NightmareTag */
    case BATTLER_TAG_TYPE_DROWSY:           /* DrowsyTag: Yawn */
    case BATTLER_TAG_TYPE_POWDER:           /* PowderTag */
    case BATTLER_TAG_TYPE_FLOATING:         /* FloatingTag: Magnet Rise */
    case BATTLER_TAG_TYPE_TELEKINESIS:      /* TelekinesisTag */
    case BATTLER_TAG_TYPE_CRIT_BOOST:       /* CritBoostTag: Focus Energy */
    case BATTLER_TAG_TYPE_DRAGON_CHEER:     /* CritBoostTag: Dragon Cheer */
    case BATTLER_TAG_TYPE_SYRUP_BOMB:       /* SyrupBombTag */
    case BATTLER_TAG_TYPE_MAGIC_COAT:       /* MagicCoatTag: the MoveReflectPhase reads it */
    case BATTLER_TAG_TYPE_ELECTRIFIED:      /* ElectrifiedTag: the move type reads it */
    case BATTLER_TAG_TYPE_DESTINY_BOND:     /* DestinyBondTag: the FaintPhase lapses it */
    case BATTLER_TAG_TYPE_GRUDGE:           /* GrudgeTag: the same */
    case BATTLER_TAG_TYPE_STOCKPILING:      /* StockpilingTag */
    case BATTLER_TAG_TYPE_BURNED_UP:        /* RemovedTypeTag: Burn Up (Roost reads it) */
    case BATTLER_TAG_TYPE_DOUBLE_SHOCKED:   /* RemovedTypeTag: Double Shock */
    case BATTLER_TAG_TYPE_POWER_TRICK:      /* PowerTrickTag */
    case BATTLER_TAG_TYPE_INTERRUPTED:      /* InterruptedTag: Smack Down or Gravity on a flying Pokemon */
    case BATTLER_TAG_TYPE_TRUANT:           /* TruantTag */
    case BATTLER_TAG_TYPE_SLOW_START:       /* SlowStartTag */
    case BATTLER_TAG_TYPE_STURDY:           /* SturdyTag: Sturdy's endure, set by the damage calculation */
    case BATTLER_TAG_TYPE_ENDURE_TOKEN:     /* EnduringTag(ENDURE_TOKEN, AFTER_HIT) */
    case BATTLER_TAG_TYPE_UNBURDEN:         /* UnburdenTag (the Speed doubling reads it) */
    case BATTLER_TAG_TYPE_GORILLA_TACTICS:  /* GorillaTacticsTag (MoveRestrictionBattlerTag) */
    case BATTLER_TAG_TYPE_SUPREME_OVERLORD: /* SupremeOverlordTag (the damage calculation reads it) */
    case BATTLER_TAG_TYPE_PROTOSYNTHESIS:   /* WeatherHighestStatBoostTag (getEffectiveStat reads it) */
    case BATTLER_TAG_TYPE_QUARK_DRIVE:      /* TerrainHighestStatBoostTag */
    case BATTLER_TAG_TYPE_BYPASS_SPEED:     /* BypassSpeedTag */
        return true;
    }
    return pr_damaging_trap_move(type) != 0 || pr_battler_tag_is(type, PR_TAGCLASS_PROTECTED);
}

/* DamagingTrapTag's subclasses (Bind, Wrap, Fire Spin...): the move each one names as its
 * sourceMove, whatever move added it; 0 for any other tag type */
int pr_damaging_trap_move(int type)
{
    switch (type) {
    case BATTLER_TAG_TYPE_BIND: return MOVE_ID_BIND;
    case BATTLER_TAG_TYPE_WRAP: return MOVE_ID_WRAP;
    case BATTLER_TAG_TYPE_FIRE_SPIN: return MOVE_ID_FIRE_SPIN;
    case BATTLER_TAG_TYPE_WHIRLPOOL: return MOVE_ID_WHIRLPOOL;
    case BATTLER_TAG_TYPE_CLAMP: return MOVE_ID_CLAMP;
    case BATTLER_TAG_TYPE_SAND_TOMB: return MOVE_ID_SAND_TOMB;
    case BATTLER_TAG_TYPE_MAGMA_STORM: return MOVE_ID_MAGMA_STORM;
    case BATTLER_TAG_TYPE_SNAP_TRAP: return MOVE_ID_SNAP_TRAP;
    case BATTLER_TAG_TYPE_THUNDER_CAGE: return MOVE_ID_THUNDER_CAGE;
    case BATTLER_TAG_TYPE_INFESTATION: return MOVE_ID_INFESTATION;
    }
    return 0;
}

static bool lapses_on(int type, int lapse_type)
{
    return (pr_battler_tags_info[type].lapse_mask >> lapse_type) & 1u;
}

static void note_tag(pr_battle *bt, int type)
{
    pr_note_unported(bt, (uint16_t)(PR_FEAT_TAG_0 + type));
}

/* the ported TrappedTag subclasses; each is source-linked (isSourceLinked) */
static bool is_trap(int type)
{
    return type == BATTLER_TAG_TYPE_TRAPPED || type == BATTLER_TAG_TYPE_NO_RETREAT || type == BATTLER_TAG_TYPE_INGRAIN
           || type == BATTLER_TAG_TYPE_OCTOLOCK || pr_damaging_trap_move(type);
}

/* isSourceLinked(): TrappedTag and InfatuatedTag */
static bool source_linked(int type)
{
    return is_trap(type) || type == BATTLER_TAG_TYPE_INFATUATED;
}

/* isActive(true): not fainted, and on the field */
static bool active_on_field(const pr_pokemon *p)
{
    return p->hp > 0 && p->on_field && !p->switch_out_status;
}

/* the moveset slot holding `move` with PP left (moveset.find(m => m.moveId === move &&
 * !m.isOutOfPp())), -1 when none; isOutOfPp: getMovePp() !== -1 && ppUsed >= getMovePp() */
int pr_moveset_slot_with_pp(const pr_pokemon *p, int move)
{
    for (int s = 0; s < p->n_moves; s++) {
        if (p->moves[s] != move) continue;
        double step = floor(pr_moves[move].pp / 5.0);                     /* toDmgValue(pp / 5) */
        int max = p->max_pp_override[s] > 0 ? p->max_pp_override[s] : pr_moves[move].pp + p->pp_up[s] * (int)(step < 1 ? 1 : step);
        if (max == -1 || p->pp_used[s] < max) return s;
    }
    return -1;
}

/* toDmgValue(pokemon.getMaxHp() / div) */
static double max_hp_part(const pr_pokemon *p, double div)
{
    double v = floor(pr_get_stat(p, STAT_HP, false) / div);
    return v < 1 ? 1 : v;
}

static int find_tag(const pr_pokemon *p, int type)
{
    for (int i = 0; i < p->n_tags; i++)
        if (p->tags[i].type == type) return i;
    return -1;
}

static void splice_tag(pr_pokemon *p, int i)
{
    memmove(&p->tags[i], &p->tags[i + 1], (size_t)(p->n_tags - i - 1) * sizeof p->tags[0]);
    p->n_tags--;
}

/* ---------------------------------------------------------------- messages */

/* queueMessage(i18next.t(key, {pokemonNameWithAffix: getPokemonNameWithAffix(p)})): most tag texts */
static void say(pr_battle *bt, pr_engine *e, const char *key, const pr_pokemon *p)
{
    pr_queue_msg(bt, e, pr_msg_p(bt, key, p));
}

/* getSourcePokemon() where upstream only names it in a text (no report: the tag's own logic
 * already looked it up) */
static const pr_pokemon *msg_source(const pr_battle *bt, const pr_battler_tag *t)
{
    return pr_pokemon_by_id(bt, t->source_id);
}

/* getMoveName(): the source move's name, null (an empty interpolation) without one */
static pr_msg_arg move_name_arg(const pr_battler_tag *t)
{
    return pr_arg_move("moveName", t->source_move);
}

/* tag.getTrapMessage(pokemon), which TrappedTag.onAdd queues: TrappedTag's own (Mean Look, No
 * Retreat, Octolock), Ingrain's, and each DamagingTrapTag subclass's */
static pr_msg trap_message(const pr_battle *bt, const pr_pokemon *p, const pr_battler_tag *t)
{
    pr_msg_arg name = pr_arg_pokemon(bt, "pokemonNameWithAffix", p);
    const pr_pokemon *source = msg_source(bt, t);
    switch (t->type) {
    case BATTLER_TAG_TYPE_INGRAIN: return pr_msg1("battlerTags:ingrainOnTrap", name);
    case BATTLER_TAG_TYPE_FIRE_SPIN:                  /* VortexTrapTag */
    case BATTLER_TAG_TYPE_WHIRLPOOL: return pr_msg1("battlerTags:vortexOnTrap", name);
    case BATTLER_TAG_TYPE_SAND_TOMB: return pr_msg2("battlerTags:sandTombOnTrap", name, move_name_arg(t));
    case BATTLER_TAG_TYPE_MAGMA_STORM: return pr_msg1("battlerTags:magmaStormOnTrap", name);
    case BATTLER_TAG_TYPE_SNAP_TRAP: return pr_msg1("battlerTags:snapTrapOnTrap", name);
    case BATTLER_TAG_TYPE_BIND:
    case BATTLER_TAG_TYPE_WRAP:
    case BATTLER_TAG_TYPE_CLAMP:
    case BATTLER_TAG_TYPE_THUNDER_CAGE:
    case BATTLER_TAG_TYPE_INFESTATION:
        /* without its source upstream returns an untranslated "ERROR - ... REPORT" string, which
         * no locale key holds: not ported */
        if (!source) return pr_msg0(NULL);
        break;
    default:
        return pr_msg1("battlerTags:trappedOnAdd", name);
    }
    switch (t->type) {
    case BATTLER_TAG_TYPE_BIND:
        return pr_msg3("battlerTags:bindOnTrap", name, pr_arg_pokemon(bt, "sourcePokemonName", source), move_name_arg(t));
    case BATTLER_TAG_TYPE_WRAP:
        return pr_msg3("battlerTags:wrapOnTrap", name, pr_arg_pokemon(bt, "sourcePokemonName", source), move_name_arg(t));
    case BATTLER_TAG_TYPE_CLAMP:
        return pr_msg2("battlerTags:clampOnTrap", pr_arg_pokemon(bt, "sourcePokemonNameWithAffix", source),
                       pr_arg_pokemon(bt, "pokemonName", p));
    case BATTLER_TAG_TYPE_THUNDER_CAGE:
        return pr_msg2("battlerTags:thunderCageOnTrap", name, pr_arg_pokemon(bt, "sourcePokemonNameWithAffix", source));
    default:                                          /* INFESTATION */
        return pr_msg2("battlerTags:infestationOnTrap", name, pr_arg_pokemon(bt, "sourcePokemonNameWithAffix", source));
    }
}

/* the CommonAnim a DamagingTrapTag subclass hands its constructor, played at each lapse */
static int trap_anim(int type)
{
    switch (type) {
    case BATTLER_TAG_TYPE_WRAP: return COMMON_ANIM_WRAP;
    case BATTLER_TAG_TYPE_FIRE_SPIN: return COMMON_ANIM_FIRE_SPIN;
    case BATTLER_TAG_TYPE_WHIRLPOOL: return COMMON_ANIM_WHIRLPOOL;
    case BATTLER_TAG_TYPE_CLAMP: return COMMON_ANIM_CLAMP;
    case BATTLER_TAG_TYPE_SAND_TOMB: return COMMON_ANIM_SAND_TOMB;
    case BATTLER_TAG_TYPE_MAGMA_STORM: return COMMON_ANIM_MAGMA_STORM;
    case BATTLER_TAG_TYPE_SNAP_TRAP: return COMMON_ANIM_SNAP_TRAP;
    case BATTLER_TAG_TYPE_THUNDER_CAGE: return COMMON_ANIM_THUNDER_CAGE;
    case BATTLER_TAG_TYPE_INFESTATION: return COMMON_ANIM_INFESTATION;
    }
    return COMMON_ANIM_BIND;
}

/* ---------------------------------------------------------------- per tag type */

/* getBattlerTag(tagType, turnCount, sourceMove, sourceId): the fields its constructor sets
 * (`if (sourceMove)`: 0 stays none; a sourceId only where the class takes one) */
static pr_battler_tag construct(int type, int turn_count, int source_move, int64_t source_id)
{
    pr_battler_tag t;
    memset(&t, 0, sizeof t);
    t.type = (uint8_t)type;
    t.turn_count = (int16_t)turn_count;
    t.source_move = (uint16_t)source_move;
    t.source_id = -1;
    t.source_index = -1;
    switch (type) {
    case BATTLER_TAG_TYPE_CHARGING:
        t.turn_count = 1;
        t.source_id = source_id;
        break;
    case BATTLER_TAG_TYPE_TRAPPED:                /* TrappedTag(tagType, CUSTOM, turnCount, sourceMove, sourceId) */
        t.source_id = source_id;
        break;
    case BATTLER_TAG_TYPE_NO_RETREAT:             /* NoRetreatTag(sourceId) */
        t.turn_count = 0;
        t.source_move = MOVE_ID_NO_RETREAT;
        t.source_id = source_id;
        break;
    case BATTLER_TAG_TYPE_FLINCHED:               /* FlinchedTag(sourceMove) */
    case BATTLER_TAG_TYPE_IGNORE_FLYING:          /* GroundedTag(tagType, CUSTOM, sourceMove) */
    case BATTLER_TAG_TYPE_IGNORE_GHOST:           /* ExposedTag(tagType, sourceMove, ...) */
    case BATTLER_TAG_TYPE_IGNORE_DARK:
    case BATTLER_TAG_TYPE_ALWAYS_GET_HIT:
    case BATTLER_TAG_TYPE_RECEIVE_DOUBLE_DAMAGE:
    case BATTLER_TAG_TYPE_CHARGED:
    case BATTLER_TAG_TYPE_FIRE_BOOST:
        t.turn_count = 1;
        break;
    case BATTLER_TAG_TYPE_MINIMIZED:              /* MinimizeTag() */
        t.turn_count = 1;
        t.source_move = MOVE_ID_MINIMIZE;
        break;
    case BATTLER_TAG_TYPE_TAR_SHOT:               /* TarShotTag() */
        t.turn_count = 0;
        t.source_move = 0;
        break;
    case BATTLER_TAG_TYPE_RECHARGING:             /* RechargingTag(sourceMove) */
        t.turn_count = 2;
        break;
    case BATTLER_TAG_TYPE_AUTOTOMIZED:            /* AutotomizedTag(): sourceMove = AUTOTOMIZE */
        t.turn_count = 1;
        t.source_move = MOVE_ID_AUTOTOMIZE;
        break;
    case BATTLER_TAG_TYPE_SEEDED:                 /* SeedTag(sourceId): TURN_END, 1 turn, LEECH_SEED */
        t.turn_count = 1;
        t.source_move = MOVE_ID_LEECH_SEED;
        t.source_id = source_id;
        break;
    case BATTLER_TAG_TYPE_SUBSTITUTE:             /* SubstituteTag(sourceMove, sourceId) */
        t.turn_count = 0;
        t.source_id = source_id;
        break;
    case BATTLER_TAG_TYPE_ENDURING:               /* EnduringTag(tagType, TURN_END, sourceMove) */
        t.turn_count = 0;
        break;
    case BATTLER_TAG_TYPE_SHELL_TRAP:             /* ShellTrapTag(): no source move */
        t.turn_count = 1;
        t.source_move = 0;
        break;
    case BATTLER_TAG_TYPE_BEAK_BLAST_CHARGING:    /* BeakBlastChargingTag() */
        t.turn_count = 1;
        t.source_move = MOVE_ID_BEAK_BLAST;
        break;
    case BATTLER_TAG_TYPE_IGNORE_ACCURACY:        /* SerializableBattlerTag(tagType, TURN_END, 2, sourceMove) */
    case BATTLER_TAG_TYPE_ALWAYS_CRIT:
        t.turn_count = 2;
        break;
    case BATTLER_TAG_TYPE_PERISH_SONG:            /* PerishSongTag(turnCount) */
        t.source_move = MOVE_ID_PERISH_SONG;
        break;
    case BATTLER_TAG_TYPE_ROOSTED:                /* RoostedTag() */
        t.turn_count = 1;
        t.source_move = MOVE_ID_ROOST;
        break;
    case BATTLER_TAG_TYPE_THROAT_CHOPPED:         /* ThroatChoppedTag() */
        t.turn_count = 2;
        t.source_move = MOVE_ID_THROAT_CHOP;
        break;
    case BATTLER_TAG_TYPE_HEAL_BLOCK:             /* HealBlockTag(turnCount): no source move */
        t.source_move = 0;
        break;
    case BATTLER_TAG_TYPE_AQUA_RING:              /* AquaRingTag() */
        t.turn_count = 1;
        t.source_move = MOVE_ID_AQUA_RING;
        break;
    case BATTLER_TAG_TYPE_INGRAIN:                /* IngrainTag(sourceId) */
        t.turn_count = 1;
        t.source_move = MOVE_ID_INGRAIN;
        t.source_id = source_id;
        break;
    case BATTLER_TAG_TYPE_OCTOLOCK:               /* OctolockTag(sourceId) */
        t.turn_count = 1;
        t.source_move = MOVE_ID_OCTOLOCK;
        t.source_id = source_id;
        break;
    case BATTLER_TAG_TYPE_RAGE:                   /* RageTag(): no source id */
        t.turn_count = 1;
        t.source_move = MOVE_ID_RAGE;
        break;
    case BATTLER_TAG_TYPE_PSYCHO_SHIFT:           /* PsychoShiftTag() */
        t.turn_count = 1;
        t.source_move = MOVE_ID_PSYCHO_SHIFT;
        break;
    case BATTLER_TAG_TYPE_DISABLED:               /* DisabledTag(sourceId): TURN_END, 4 turns; moveId NONE */
        t.turn_count = 4;
        t.source_move = MOVE_ID_DISABLE;
        t.source_id = source_id;
        break;
    case BATTLER_TAG_TYPE_TAUNT:                  /* TauntTag(): AFTER_MOVE, 4 turns, no source id */
        t.turn_count = 4;
        t.source_move = MOVE_ID_TAUNT;
        break;
    case BATTLER_TAG_TYPE_TORMENT:                /* TormentTag(sourceId): AFTER_MOVE, 1 turn */
        t.turn_count = 1;
        t.source_move = MOVE_ID_TORMENT;
        t.source_id = source_id;
        break;
    case BATTLER_TAG_TYPE_ENCORE:                 /* EncoreTag(sourceId): TURN_END, 4 turns; moveId set by canAdd */
        t.turn_count = 4;
        t.source_move = MOVE_ID_ENCORE;
        t.source_id = source_id;
        t.added_this_turn = true;
        break;
    case BATTLER_TAG_TYPE_INFATUATED:             /* InfatuatedTag(sourceMove, sourceId): CUSTOM, 1 turn */
        t.turn_count = 1;
        t.source_id = source_id;
        break;
    case BATTLER_TAG_TYPE_SALT_CURED:             /* SaltCuredTag(sourceId): TURN_END, 1 turn */
        t.turn_count = 1;
        t.source_move = MOVE_ID_SALT_CURE;
        t.source_id = source_id;
        break;
    case BATTLER_TAG_TYPE_MAGIC_COAT:             /* MagicCoatTag(): TURN_END, 1 turn */
        t.turn_count = 1;
        t.source_move = MOVE_ID_MAGIC_COAT;
        break;
    case BATTLER_TAG_TYPE_STOCKPILING:            /* StockpilingTag(sourceMove): CUSTOM, 1 turn */
    case BATTLER_TAG_TYPE_BURNED_UP:              /* RemovedTypeTag(tagType, CUSTOM, sourceMove): 1 turn */
    case BATTLER_TAG_TYPE_DOUBLE_SHOCKED:
        t.turn_count = 1;
        break;
    case BATTLER_TAG_TYPE_POWER_TRICK:            /* PowerTrickTag(sourceMove, sourceId): CUSTOM, 0 turns */
        t.turn_count = 0;
        t.source_id = source_id;
        break;
    case BATTLER_TAG_TYPE_INTERRUPTED:            /* InterruptedTag(sourceMove): PRE_MOVE, 0 turns */
    case BATTLER_TAG_TYPE_STURDY:                 /* SturdyTag(sourceMove): TURN_END, 0 turns */
    case BATTLER_TAG_TYPE_ENDURE_TOKEN:           /* EnduringTag(ENDURE_TOKEN, AFTER_HIT, sourceMove): 0 turns */
        t.turn_count = 0;
        break;
    /* AbilityBattlerTag(tagType, ability, lapseType, turnCount): no source move */
    case BATTLER_TAG_TYPE_TRUANT:                 /* CUSTOM, 1 turn */
    case BATTLER_TAG_TYPE_UNBURDEN:               /* CUSTOM, 1 turn */
        t.turn_count = 1;
        t.source_move = 0;
        break;
    case BATTLER_TAG_TYPE_SLOW_START:             /* TURN_END, 5 turns */
        t.turn_count = 5;
        t.source_move = 0;
        break;
    case BATTLER_TAG_TYPE_PROTOSYNTHESIS:         /* HighestStatBoostTag: CUSTOM, 1 turn, Attack x1.3 */
    case BATTLER_TAG_TYPE_QUARK_DRIVE:
        t.turn_count = 1;
        t.source_move = 0;
        t.stat = STAT_ATK;
        t.multiplier = 1.3;
        break;
    case BATTLER_TAG_TYPE_BYPASS_SPEED:           /* BypassSpeedTag: TURN_END, 1 turn */
        t.turn_count = 1;
        t.source_move = 0;
        break;
    case BATTLER_TAG_TYPE_GORILLA_TACTICS:        /* MoveRestrictionBattlerTag(GORILLA_TACTICS, CUSTOM, 0) */
    case BATTLER_TAG_TYPE_SUPREME_OVERLORD:       /* FAINT, 0 turns */
        t.turn_count = 0;
        t.source_move = 0;
        break;
    case BATTLER_TAG_TYPE_ELECTRIFIED:            /* ElectrifiedTag(): TURN_END, 1 turn */
        t.turn_count = 1;
        t.source_move = MOVE_ID_ELECTRIFY;
        break;
    case BATTLER_TAG_TYPE_DESTINY_BOND:           /* DestinyBondTag(sourceMove, sourceId): PRE_MOVE, 1 turn */
        t.turn_count = 1;
        t.source_id = source_id;
        break;
    case BATTLER_TAG_TYPE_GRUDGE:                 /* GrudgeTag(): PRE_MOVE, 1 turn, no source move */
        t.turn_count = 1;
        t.source_move = 0;
        break;
    case BATTLER_TAG_TYPE_SYRUP_BOMB:             /* SyrupBombTag(sourceId): TURN_END, 3 turns */
        t.turn_count = 3;
        t.source_move = MOVE_ID_SYRUP_BOMB;
        t.source_id = source_id;
        break;
    case BATTLER_TAG_TYPE_CURSED:                 /* CursedTag(sourceId): TURN_END, 1 turn */
        t.turn_count = 1;
        t.source_move = MOVE_ID_CURSE;
        t.source_id = source_id;
        break;
    case BATTLER_TAG_TYPE_NIGHTMARE:              /* NightmareTag(): TURN_END, 1 turn */
        t.turn_count = 1;
        t.source_move = MOVE_ID_NIGHTMARE;
        break;
    case BATTLER_TAG_TYPE_DROWSY:                 /* DrowsyTag(): TURN_END, 2 turns */
        t.turn_count = 2;
        t.source_move = MOVE_ID_YAWN;
        break;
    case BATTLER_TAG_TYPE_POWDER:                 /* PowderTag(): TURN_END, 1 turn, no source move */
    case BATTLER_TAG_TYPE_TELEKINESIS:            /* TelekinesisTag(): TURN_END, 3 turns, no source move */
        t.turn_count = type == BATTLER_TAG_TYPE_POWDER ? 1 : 3;
        t.source_move = 0;
        break;
    case BATTLER_TAG_TYPE_FLOATING:               /* FloatingTag(turnCount): no source move */
        t.source_move = 0;
        break;
    case BATTLER_TAG_TYPE_CRIT_BOOST:             /* CritBoostTag(tagType, sourceMove): TURN_END, 1 turn */
    case BATTLER_TAG_TYPE_DRAGON_CHEER:
        t.turn_count = 1;
        t.crit_stages = 1;
        break;
    default:                                      /* CONFUSED, FRENZY, the semi-invulnerable tags */
        if (pr_damaging_trap_move(type)) {        /* BindTag(turnCount, sourceId) and co. */
            t.source_move = (uint16_t)pr_damaging_trap_move(type);
            t.source_id = source_id;
        }
        if (pr_battler_tag_is(type, PR_TAGCLASS_PROTECTED)) t.turn_count = 0;   /* ProtectedTag(sourceMove, tagType) */
        break;
    }
    return t;
}

/* newTag.canAdd(pokemon) */
static pr_pokemon *tag_source(pr_battle *bt, const pr_battler_tag *t);

/* isOppositeGender: this.gender !== GENDERLESS && other.gender === (this.gender === MALE ? FEMALE : MALE)
 * (Gender: GENDERLESS -1, MALE 0, FEMALE 1) */
static bool opposite_gender(const pr_pokemon *a, const pr_pokemon *b)
{
    return a->gender != -1 && b->gender == (a->gender == 0 ? 1 : 0);
}

/* EncoreTag.canAdd without the tag: the move it would lock the Pokemon into (its last non-virtual
 * move, not one of invalidEncoreMoves, still in its moveset with PP left, and no Shell Trap
 * set), 0 when it can't be added; -1 when the history it needs isn't kept (reported) */
int pr_encore_move(pr_battle *bt, const pr_pokemon *p)
{
    bool unknown;
    const pr_turn_move *last = pr_last_non_virtual_move(bt, p, false, true, &unknown);
    if (unknown) return -1;
    if (!last) return 0;
    for (unsigned i = 0; i < pr_set_invalid_encore_moves_n; i++)
        if (pr_set_invalid_encore_moves[i] == last->move) return 0;
    if (pr_moveset_slot_with_pp(p, last->move) < 0 || pr_get_battler_tag(p, BATTLER_TAG_TYPE_SHELL_TRAP)) return 0;
    return last->move;
}

/* EncoreTag.tryOverrideMove's first half: an Encore added this turn whose move still has PP
 * takes a turn off its count and is no longer new; the moveset slot, -1 for no override */
int pr_encore_try_override(pr_pokemon *p)
{
    int i = find_tag(p, BATTLER_TAG_TYPE_ENCORE);
    if (i < 0 || !p->tags[i].added_this_turn) return -1;
    int slot = pr_moveset_slot_with_pp(p, p->tags[i].move_id);
    if (slot < 0) return -1;
    p->tags[i].turn_count--;
    p->tags[i].added_this_turn = false;
    return slot;
}

/* newTag.canAdd(pokemon); Encore's sets its moveId */
static bool can_add(pr_battle *bt, pr_engine *e, pr_pokemon *p, pr_battler_tag *t)
{
    switch (t->type) {
    case BATTLER_TAG_TYPE_ENCORE: {
        int move = pr_encore_move(bt, p);
        if (move <= 0) return false;
        t->move_id = (uint16_t)move;
        return true;
    }
    case BATTLER_TAG_TYPE_INFATUATED: {
        pr_pokemon *source = tag_source(bt, t);
        return source && opposite_gender(p, source);
    }
    case BATTLER_TAG_TYPE_DROWSY:                         /* no Yawn on the ground in Electric Terrain */
        return bt->terrain != TERRAIN_ELECTRIC || !pr_is_grounded(bt, p, false);
    case BATTLER_TAG_TYPE_PERISH_SONG:
        return !p->is_boss;                                   /* !isBossImmune() */
    case BATTLER_TAG_TYPE_ROOSTED:
        return pr_is_of_type(p, POKEMON_TYPE_FLYING) && !p->terastallized;
    case BATTLER_TAG_TYPE_INGRAIN:
        return !pr_get_battler_tag(p, BATTLER_TAG_TYPE_TRAPPED);
    case BATTLER_TAG_TYPE_CONFUSED:
        /* blocked by Misty Terrain on the ground; isGrounded() runs first, and applies Levitate (not
         * simulated). The terrain's message only when the running phase is a MoveEffectPhase of a
         * status move (queueStatusImmuneMessage(false, MISTY)) */
        if (pr_is_grounded(bt, p, false) && bt->terrain == TERRAIN_MISTY) {
            if (e && e->current >= 0 && e->pool[e->current].type == PH_MOVE_EFFECT
                && pr_moves[e->pool[e->current].move_id].category == MOVE_CATEGORY_STATUS)
                pr_queue_msg(bt, e, pr_terrain_block_msg(bt, TERRAIN_MISTY, p));   /* getTerrainBlockMessage */
            return false;
        }
        return true;
    case BATTLER_TAG_TYPE_TRAPPED:
    case BATTLER_TAG_TYPE_OCTOLOCK: {                     /* TrappedTag.canAdd */
        /* getSourcePokemon() searches both parties; only the field is modelled */
        pr_pokemon *source = pr_field_pokemon_by_id(bt, t->source_id);
        if (!source) {
            pr_note_unported(bt, PR_FEAT_PARTY);
            return false;
        }
        if (t->source_move && pr_move_hits_substitute(bt, t->source_move, source, p)) return false;
        return !pr_find_battler_tag_class(p, PR_TAGCLASS_TRAPPED) && !pr_is_of_type(p, POKEMON_TYPE_GHOST);
    }
    case BATTLER_TAG_TYPE_NO_RETREAT:
        return !pr_find_battler_tag_class(p, PR_TAGCLASS_TRAPPED);
    case BATTLER_TAG_TYPE_GORILLA_TACTICS: {              /* a last non-virtual move to lock into, not Struggle */
        bool unknown;
        const pr_turn_move *last = pr_last_non_virtual_move(bt, p, false, true, &unknown);
        return last && last->move != MOVE_ID_STRUGGLE;
    }
    case BATTLER_TAG_TYPE_INTERRUPTED:                    /* only a Pokemon up in the air (Fly, Bounce) */
        return pr_get_battler_tag(p, BATTLER_TAG_TYPE_FLYING) != NULL;
    case BATTLER_TAG_TYPE_TAR_SHOT:
        return !p->terastallized;
    case BATTLER_TAG_TYPE_SEEDED:
        return !pr_is_of_type(p, POKEMON_TYPE_GRASS);
    }
    /* DamagingTrapTag: !getTag(TrappedTag) && !getTag(SUBSTITUTE); no Ghost check, no source */
    if (pr_damaging_trap_move(t->type))
        return !pr_find_battler_tag_class(p, PR_TAGCLASS_TRAPPED) && !pr_get_battler_tag(p, BATTLER_TAG_TYPE_SUBSTITUTE);
    return true;
}

static bool has_type(const int8_t *types, int n, int type)
{
    for (int i = 0; i < n; i++)
        if (types[i] == type) return true;
    return false;
}

/* RoostedTag.onAdd: Flying comes off the current types; a Pokemon purely Flying by its species
 * becomes Normal instead (isBasePureFlying), and one a RemovedTypeTag (Burn Up, Double Shock)
 * left with one type of its two becomes typeless. summonData.types = the result. */
static void roost_on_add(pr_battle *bt, pr_pokemon *p, pr_battler_tag *t)
{
    int8_t cur[PR_MAX_TYPES], base[PR_MAX_TYPES], out[PR_MAX_TYPES];
    int nc = pr_get_types(p, cur, 0);
    int nb = pr_get_types(p, base, PR_TYPES_NO_TERA | PR_TYPES_BYPASS_SUMMON | PR_TYPES_IGNORE_THIRD);
    t->base_pure_flying = base[0] == POKEMON_TYPE_FLYING && nb == 1;
    int n = 0;
    if (t->base_pure_flying && nc != 2) {
        out[n++] = POKEMON_TYPE_NORMAL;
    } else if ((pr_get_battler_tag(p, BATTLER_TAG_TYPE_BURNED_UP) || pr_get_battler_tag(p, BATTLER_TAG_TYPE_DOUBLE_SHOCKED))
               && nb == 2 && nc != 2) {
        out[n++] = POKEMON_TYPE_UNKNOWN;
    } else {
        for (int i = 0; i < nc; i++)
            if (cur[i] != POKEMON_TYPE_FLYING) out[n++] = cur[i];
    }
    pr_set_summon_types(bt, p, out, n);
}

/* RoostedTag.onRemove: Flying goes back on top of the current types; for a pure Flying one, the
 * Normal it got comes off only when Forest's Curse or Trick-or-Treat added a type (guessed from
 * the types, as upstream does), else it is just Flying again */
static void roost_on_remove(pr_battle *bt, pr_pokemon *p, const pr_battler_tag *t)
{
    int8_t cur[PR_MAX_TYPES], base[PR_MAX_TYPES], out[PR_MAX_TYPES + 1];
    int nc = pr_get_types(p, cur, 0);
    int nb = pr_get_types(p, base, PR_TYPES_NO_TERA | PR_TYPES_BYPASS_SUMMON | PR_TYPES_IGNORE_THIRD);
    bool forests_curse = has_type(cur, nc, POKEMON_TYPE_GRASS) && !has_type(base, nb, POKEMON_TYPE_GRASS);
    bool trick_or_treat = has_type(cur, nc, POKEMON_TYPE_GHOST) && !has_type(base, nb, POKEMON_TYPE_GHOST);
    int n = 0;
    if (t->base_pure_flying && !forests_curse && !trick_or_treat) {
        out[n++] = POKEMON_TYPE_FLYING;
    } else {
        for (int i = 0; i < nc; i++)
            if (!t->base_pure_flying || cur[i] != POKEMON_TYPE_NORMAL) out[n++] = cur[i];
        out[n++] = POKEMON_TYPE_FLYING;
    }
    pr_set_summon_types(bt, p, out, n);
}

/* tag.onAdd(pokemon); t is the tag in p->tags */
/* PowerTrickTag.swapStat: Attack and Defense trade places in summonData.stats (getStat(stat,
 * false) reads through to the real stats where no override is set; setStat ignores a value
 * of 0 or less) */
void pr_power_trick_swap(pr_pokemon *p)
{
    double atk = pr_get_stat(p, STAT_ATK, false), def = pr_get_stat(p, STAT_DEF, false);
    if (def > 0) p->summon_stats[STAT_ATK] = def;
    if (atk > 0) p->summon_stats[STAT_DEF] = atk;
}

/* StockpilingTag.onAdd (onOverlap too): below 3 stacks, one more with its message, then Defense
 * and Sp. Def +1 from the Pokemon itself, the tag counting each raise that applies */
static void stockpile_on_add(pr_battle *bt, pr_engine *e, pr_pokemon *p, pr_battler_tag *t)
{
    if (t->stockpiled_count >= 3) return;
    t->stockpiled_count++;
    pr_queue_msg(bt, e, pr_msg2("battlerTags:stockpilingOnAdd", pr_arg_pokemon(bt, "pokemonNameWithAffix", p),
                                pr_arg_int("stockpiledCount", t->stockpiled_count)));
    static const int32_t stats[] = {STAT_SPDEF, STAT_DEF};
    int ph = pr_unshift_stat_stage_change(bt, e, pr_battler_index(bt, p), stats, 2, 1, p);
    if (ph >= 0) e->pool[ph].ssc_stockpile = true;
}

static void on_add(pr_battle *bt, pr_engine *e, pr_pokemon *p, pr_battler_tag *t)
{
    switch (t->type) {
    case BATTLER_TAG_TYPE_CONFUSED:
        pr_unshift_common_anim(bt, e, pr_battler_index(bt, p), -1, COMMON_ANIM_CONFUSION);
        say(bt, e, "battlerTags:confusedOnAdd", p);
        return;
    case BATTLER_TAG_TYPE_TRAPPED:
    case BATTLER_TAG_TYPE_NO_RETREAT:
    case BATTLER_TAG_TYPE_INGRAIN:               /* TrappedTag.onAdd: getTrapMessage() */
    case BATTLER_TAG_TYPE_OCTOLOCK:
        pr_queue_msg(bt, e, trap_message(bt, p, t));
        return;
    case BATTLER_TAG_TYPE_CHARGED:               /* TypeBoostTag.onAdd: its boostedType */
    case BATTLER_TAG_TYPE_FIRE_BOOST:
        pr_queue_msg(bt, e, pr_msg2("abilityTriggers:typeImmunityPowerBoost", pr_arg_pokemon(bt, "pokemonNameWithAffix", p),
                                    pr_arg_type("typeName", t->type == BATTLER_TAG_TYPE_CHARGED ? POKEMON_TYPE_ELECTRIC
                                                                                                : POKEMON_TYPE_FIRE)));
        return;
    case BATTLER_TAG_TYPE_SHELL_TRAP:            /* "set a shell trap" */
        pr_queue_msg(bt, e, pr_msg1("moveTriggers:setUpShellTrap", pr_arg_pokemon(bt, "pokemonName", p)));
        return;
    case BATTLER_TAG_TYPE_BEAK_BLAST_CHARGING:   /* the charge animation plays outside the phases, then the message */
        pr_queue_msg(bt, e, pr_msg1("moveTriggers:startedHeatingUpBeak", pr_arg_pokemon(bt, "pokemonName", p)));
        return;
    case BATTLER_TAG_TYPE_INFATUATED:            /* canAdd found the source */
        pr_queue_msg(bt, e, pr_msg2("battlerTags:infatuatedOnAdd", pr_arg_pokemon(bt, "pokemonNameWithAffix", p),
                                    pr_arg_pokemon(bt, "sourcePokemonName", msg_source(bt, t))));
        return;
    case BATTLER_TAG_TYPE_TAR_SHOT: say(bt, e, "battlerTags:tarShotOnAdd", p); return;
    case BATTLER_TAG_TYPE_ENDURING:              /* EnduringTag */
    case BATTLER_TAG_TYPE_ENDURE_TOKEN: say(bt, e, "battlerTags:enduringOnAdd", p); return;
    case BATTLER_TAG_TYPE_AQUA_RING: say(bt, e, "battlerTags:aquaRingOnAdd", p); return;
    case BATTLER_TAG_TYPE_HEAL_BLOCK: say(bt, e, "battlerTags:healBlockOnAdd", p); return;
    case BATTLER_TAG_TYPE_TAUNT: say(bt, e, "battlerTags:tauntOnAdd", p); return;
    case BATTLER_TAG_TYPE_TORMENT: say(bt, e, "battlerTags:tormentOnAdd", p); return;
    case BATTLER_TAG_TYPE_ENCORE: say(bt, e, "battlerTags:encoreOnAdd", p); return;
    case BATTLER_TAG_TYPE_NIGHTMARE: say(bt, e, "battlerTags:nightmareOnAdd", p); return;
    case BATTLER_TAG_TYPE_DROWSY: say(bt, e, "battlerTags:drowsyOnAdd", p); return;
    case BATTLER_TAG_TYPE_POWDER: say(bt, e, "battlerTags:powderOnAdd", p); return;
    case BATTLER_TAG_TYPE_TELEKINESIS: say(bt, e, "battlerTags:telekinesisOnAdd", p); return;
    case BATTLER_TAG_TYPE_SYRUP_BOMB: say(bt, e, "battlerTags:syrupBombOnAdd", p); return;
    case BATTLER_TAG_TYPE_MAGIC_COAT: say(bt, e, "battlerTags:magicCoatOnAdd", p); return;
    case BATTLER_TAG_TYPE_ELECTRIFIED: say(bt, e, "battlerTags:electrifiedOnAdd", p); return;
    case BATTLER_TAG_TYPE_GRUDGE: say(bt, e, "battlerTags:grudgeOnAdd", p); return;
    case BATTLER_TAG_TYPE_SLOW_START: say(bt, e, "battlerTags:slowStartOnAdd", p); return;
    case BATTLER_TAG_TYPE_SALT_CURED:                /* the message only with its source */
        if (tag_source(bt, t)) say(bt, e, "battlerTags:saltCuredOnAdd", p);
        return;
    case BATTLER_TAG_TYPE_CRIT_BOOST:
    case BATTLER_TAG_TYPE_DRAGON_CHEER: {
        /* 2 stages; Dragon Cheer only 1 for a Pokemon that isn't Dragon type (its original types
         * when Stellar), then the message */
        int8_t ty[PR_MAX_TYPES];
        int n = pr_get_types(p, ty, PR_TYPES_ORIGINAL_IF_STELLAR);
        t->crit_stages = (int8_t)(t->type == BATTLER_TAG_TYPE_DRAGON_CHEER && !has_type(ty, n, POKEMON_TYPE_DRAGON) ? 1 : 2);
        say(bt, e, "battlerTags:critBoostOnAdd", p);
        return;
    }
    case BATTLER_TAG_TYPE_DISABLED: {
        /* the Pokemon's last non-virtual move becomes moveId, with the message; nothing against
         * no move or Struggle */
        bool unknown;
        const pr_turn_move *move = pr_last_non_virtual_move(bt, p, false, true, &unknown);
        if (!move || move->move == MOVE_ID_STRUGGLE) return;
        t->move_id = move->move;
        pr_queue_msg(bt, e, pr_msg2("battlerTags:disabledOnAdd", pr_arg_pokemon(bt, "pokemonNameWithAffix", p),
                                    pr_arg_move("moveName", t->move_id)));
        return;
    }
    case BATTLER_TAG_TYPE_ROOSTED:
        roost_on_add(bt, p, t);
        return;
    case BATTLER_TAG_TYPE_STOCKPILING:
        stockpile_on_add(bt, e, p, t);
        return;
    case BATTLER_TAG_TYPE_POWER_TRICK:
        pr_power_trick_swap(p);
        say(bt, e, "battlerTags:powerTrickActive", p);
        return;
    case BATTLER_TAG_TYPE_GORILLA_TACTICS: {
        /* locked into the last non-virtual move (canAdd found one); Attack x1.5 in summonData.stats */
        bool unknown;
        const pr_turn_move *last = pr_last_non_virtual_move(bt, p, false, true, &unknown);
        t->move_id = last ? last->move : MOVE_ID_NONE;
        double atk = pr_get_stat(p, STAT_ATK, false) * 1.5;
        if (atk > 0) p->summon_stats[STAT_ATK] = atk;
        return;
    }
    case BATTLER_TAG_TYPE_PROTOSYNTHESIS:
    case BATTLER_TAG_TYPE_QUARK_DRIVE: {
        /* the highest effective stat without abilities or held items, Attack first on a tie; the
         * tag is already on the Pokemon, so its default Attack x1.3 counts in that comparison */
        const pr_stat_params sp = {.move = -1, .ignore_ability = true, .ignore_opp_ability = true,
                                   .ignore_ally_ability = true, .ignore_held_items = true, .simulated = true};
        int best = STAT_ATK;
        int32_t best_value = 0;
        for (int stat = STAT_ATK; stat <= STAT_SPD; stat++) {
            int32_t v = pr_get_effective_stat_p(bt, p, stat, &sp);
            if (v > best_value) best = stat, best_value = v;
        }
        t->stat = (int8_t)best;
        t->multiplier = best == STAT_SPD ? 1.5 : 1.3;
        pr_queue_msg(bt, e, pr_msg2("battlerTags:highestStatBoostOnAdd", pr_arg_pokemon(bt, "pokemonNameWithAffix", p),
                                    pr_arg_stat("statName", best)));
        return;
    }
    case BATTLER_TAG_TYPE_SUPREME_OVERLORD: {
        /* faintCount: the side's faints so far (the arena's for the player, the battle's for the
         * enemy), at most 5, then the message */
        int faints = p->is_player ? bt->player_faints : bt->enemy_faints;
        t->faint_count = (int8_t)(faints < 5 ? faints : 5);
        say(bt, e, "battlerTags:supremeOverlordOnAdd", p);
        return;
    }
    case BATTLER_TAG_TYPE_INTERRUPTED: {
        /* the queued second turn of Fly or Bounce goes, and the history gets a NONE entry (OTHER) */
        if (p->n_move_queue) {
            memmove(&p->move_queue[0], &p->move_queue[1], (size_t)(p->n_move_queue - 1) * sizeof p->move_queue[0]);
            p->n_move_queue--;
        }
        pr_turn_move tm;
        memset(&tm, 0, sizeof tm);
        tm.move = MOVE_ID_NONE;
        tm.result = MOVE_RESULT_OTHER;
        tm.use_mode = MOVE_USE_MODE_NORMAL;
        pr_push_history_shared(bt, e, p, tm, -1);
        return;
    }
    case BATTLER_TAG_TYPE_SEEDED: {
        /* SeedTag.onAdd: nothing without its source (getSourcePokemon searches both parties; only
         * the field is modelled), else the message, then sourceIndex = the source's battler index */
        pr_pokemon *source = t->source_id >= 0 ? pr_field_pokemon_by_id(bt, t->source_id) : NULL;
        if (!source) {
            if (t->source_id >= 0) pr_note_unported(bt, PR_FEAT_PARTY);
            return;
        }
        say(bt, e, "battlerTags:seededOnAdd", p);
        t->source_index = (int8_t)pr_battler_index(bt, source);
        return;
    }
    case BATTLER_TAG_TYPE_SUBSTITUTE: {
        /* SubstituteTag.onAdd: the doll gets a quarter of its source's max HP, its animation
         * (PokemonAnimPhase), the message, then the Pokemon's damaging traps go */
        pr_pokemon *source = t->source_id >= 0 ? pr_field_pokemon_by_id(bt, t->source_id) : NULL;
        if (!source) {
            if (t->source_id >= 0) pr_note_unported(bt, PR_FEAT_PARTY);
            return;
        }
        t->hp = (int32_t)floor(pr_get_stat(source, STAT_HP, false) / 4.0);
        t->in_focus = false;
        pr_unshift_pokemon_anim(bt, e, pr_battler_index(bt, p), POKEMON_ANIM_TYPE_SUBSTITUTE_ADD);
        say(bt, e, t->source_move == MOVE_ID_SHED_TAIL ? "battlerTags:shedTailOnAdd" : "battlerTags:substituteOnAdd", p);
        static const int32_t traps[] = {
            BATTLER_TAG_TYPE_BIND, BATTLER_TAG_TYPE_WRAP, BATTLER_TAG_TYPE_FIRE_SPIN, BATTLER_TAG_TYPE_WHIRLPOOL,
            BATTLER_TAG_TYPE_CLAMP, BATTLER_TAG_TYPE_SAND_TOMB, BATTLER_TAG_TYPE_MAGMA_STORM,
            BATTLER_TAG_TYPE_SNAP_TRAP, BATTLER_TAG_TYPE_THUNDER_CAGE, BATTLER_TAG_TYPE_INFESTATION,
        };
        /* findAndRemoveTags(tag => tag instanceof DamagingTrapTag); t may move */
        pr_find_and_remove_tags(bt, e, p, traps, (int)(sizeof traps / sizeof traps[0]));
        return;
    }
    case BATTLER_TAG_TYPE_AUTOTOMIZED:
        if (pr_get_weight(bt, p) > 0.1) say(bt, e, "battlerTags:autotomizeOnAdd", p);
        t->autotomize_count++;
        return;
    case BATTLER_TAG_TYPE_RECHARGING:
        /* pushMoveQueue({move: NONE, targets: [], useMode: NORMAL}): next turn's placeholder */
        if (p->n_move_queue < PR_MAX_MOVE_QUEUE) {
            pr_turn_move q;
            memset(&q, 0, sizeof q);
            q.move = MOVE_ID_NONE;
            q.use_mode = MOVE_USE_MODE_NORMAL;
            q.result = PR_MOVE_RESULT_UNDEFINED;
            p->move_queue[p->n_move_queue++] = q;
        } else {
            pr_note_unported(bt, PR_FEAT_ENGINE_LIMIT);
            if (e) e->overflow = true;
        }
        return;
    }
    if (pr_damaging_trap_move(t->type)) pr_queue_msg(bt, e, trap_message(bt, p, t));   /* TrappedTag.onAdd */
    if (pr_battler_tag_is(t->type, PR_TAGCLASS_PROTECTED)) say(bt, e, "battlerTags:protectedOnAdd", p);   /* ProtectedTag.onAdd */
}

/* existingTag.onOverlap(pokemon) */
static void on_overlap(pr_battle *bt, pr_engine *e, pr_pokemon *p, pr_battler_tag *t)
{
    switch (t->type) {
    case BATTLER_TAG_TYPE_CONFUSED: say(bt, e, "battlerTags:confusedOnOverlap", p); return;
    case BATTLER_TAG_TYPE_CHARGED:                        /* TypeBoostTag.onOverlap */
    case BATTLER_TAG_TYPE_FIRE_BOOST: say(bt, e, "abilityTriggers:moveImmunity", p); return;
    case BATTLER_TAG_TYPE_INFATUATED: say(bt, e, "battlerTags:infatuatedOnOverlap", p); return;
    case BATTLER_TAG_TYPE_NIGHTMARE: say(bt, e, "battlerTags:nightmareOnOverlap", p); return;
    case BATTLER_TAG_TYPE_AUTOTOMIZED:
        on_add(bt, e, p, t);
        return;
    case BATTLER_TAG_TYPE_STOCKPILING:
        stockpile_on_add(bt, e, p, t);
        return;
    case BATTLER_TAG_TYPE_POWER_TRICK:                    /* a second Power Trick undoes the first */
        pr_remove_tag(bt, e, p, BATTLER_TAG_TYPE_POWER_TRICK);
        return;
    }
}

/* ConfusedTag.lapse's hit on itself: 40 power, the effective Attack against the effective Defense */
static void confusion_self_hit(pr_battle *bt, pr_engine *e, pr_pokemon *p)
{
    double atk = pr_get_effective_stat(bt, p, STAT_ATK, NULL, -1, false, false);
    double def = pr_get_effective_stat(bt, p, STAT_DEF, NULL, -1, false, false);
    double roll = pr_battle_rand_seed_int(bt, 16, 85) / 100.0;       /* randBattleSeedIntRange(85, 100) */
    double damage = floor((((2.0 * p->level) / 5 + 2) * 40 * atk / def / 50 + 2) * roll);
    if (damage < 1) damage = 1;                                        /* toDmgValue */
    pr_queue_msg(bt, e, pr_msg0("battlerTags:confusedLapseHurtItself"));
    pr_damage_and_update(bt, e, p, damage, HIT_RESULT_CONFUSION, false, NULL);
    pr_cancel_current_move(e, p);
}

/* MoveLockTag.getNextTargets: a random near enemy again for RANDOM_NEAR_ENEMY moves
 * (getMoveTargets), else the last targets */
static void next_targets(pr_battle *bt, const pr_pokemon *p, int move, const pr_turn_move *last, pr_turn_move *out)
{
    if (pr_moves[move].target != MOVE_TARGET_RANDOM_NEAR_ENEMY) {
        memcpy(out->targets, last->targets, sizeof out->targets);
        out->n_targets = last->n_targets;
        return;
    }
    if (pr_move_has_attr((uint16_t)move, PR_AC_VariableTargetAttr)) pr_note_unported(bt, PR_AC_VariableTargetAttr);
    if (bt->double_battle) pr_note_unported(bt, PR_FEAT_DOUBLES);
    /* set = [opponents[randBattleSeedInt(opponents.length)]], opponents = getOpponents(false) (not
     * fainted, field order); then only those active on the field */
    const pr_pokemon *opp[2];
    int n = 0;
    for (int k = 0; k < 4; k++) {
        const pr_pokemon *o = bt->field[k];
        if (o && o->is_player != p->is_player && o->hp > 0 && n < 2) opp[n++] = o;
    }
    int pick = pr_battle_rand_seed_int(bt, n, 0);
    out->n_targets = 0;
    if (pick < n) {
        const pr_pokemon *o = opp[pick];
        if (o->on_field && !o->switch_out_status) out->targets[out->n_targets++] = (int8_t)pr_battler_index(bt, o);
    }
}

/* SeedTag.lapse past its countdown: the opponent standing at sourceIndex gets what the seeded
 * Pokemon loses (an eighth of its max HP), or loses it instead against Liquid Ooze */
static bool seed_drain(pr_battle *bt, pr_engine *e, pr_pokemon *p, const pr_battler_tag *t)
{
    /* pokemon.getOpponents().find(o => o.getBattlerIndex() === sourceIndex): isActive(true) */
    pr_pokemon *source = NULL;
    for (int i = 0; i < 4 && !source; i++) {
        pr_pokemon *o = bt->field[i];
        if (o && o->is_player != p->is_player && o->hp > 0 && o->on_field && !o->switch_out_status
            && t->source_index >= 0 && pr_battler_index(bt, o) == t->source_index)
            source = o;
    }
    if (!source) return true;
    bool cancelled = false;
    pr_ab_params ap = {.pokemon = p, .move = -1, .flag = &cancelled};
    pr_apply_ab_attrs(bt, PR_AC_BlockNonDirectDamageAbAttr, &ap);
    if (cancelled) return true;
    pr_unshift_common_anim(bt, e, pr_battler_index(bt, source), pr_battler_index(bt, p), COMMON_ANIM_LEECH_SEED);
    double hp = floor(pr_get_stat(p, STAT_HP, false) / 8.0);
    double damage = pr_damage_and_update(bt, e, p, hp < 1 ? 1 : hp, HIT_RESULT_INDIRECT, false, NULL);
    /* hasAbilityWithAttr("ReverseDrainAbAttr", false): no canApplyAbility check */
    bool reverse = pr_ability_first_attr(p->ability, PR_AC_ReverseDrainAbAttr)
                   || (p->passive && pr_ability_first_attr(p->passive, PR_AC_ReverseDrainAbAttr));
    pr_unshift_heal_msg(bt, e, pr_battler_index(bt, source), reverse ? -damage : damage,
                        PR_HEAL_MESSAGE | PR_HEAL_NO_FULL_HP_MESSAGE | PR_HEAL_SKIP_ANIM,
                        pr_msg_p(bt, reverse ? "battlerTags:seededLapseShed" : "battlerTags:seededLapse", p));
    return true;
}

/* SubstituteTag.onHit: the running MoveEffectPhase's first strike, if it hits the doll, says so */
static void substitute_on_hit(pr_battle *bt, pr_engine *e, pr_pokemon *p)
{
    int move;
    pr_pokemon *attacker = pr_move_effect_user(bt, e, &move);
    if (!attacker) return;
    if (attacker->hit_count == attacker->hits_left && pr_move_hits_substitute(bt, move, attacker, p))
        say(bt, e, "battlerTags:substituteOnHit", p);
}

/* ContactProtectedTag.onContact(attacker, user = the protected Pokemon) */
static void protect_on_contact(pr_battle *bt, pr_engine *e, pr_pokemon *p, const pr_battler_tag *t, pr_pokemon *attacker)
{
    int32_t stat = STAT_ATK;
    int stages = -1;
    switch (t->type) {
    case BATTLER_TAG_TYPE_SPIKY_SHIELD: {
        /* the protected Pokemon's Magic Guard (sic) decides; an eighth of the attacker's max HP */
        bool cancelled = false;
        pr_ab_params ap = {.pokemon = p, .move = -1, .flag = &cancelled};
        pr_apply_ab_attrs(bt, PR_AC_BlockNonDirectDamageAbAttr, &ap);
        if (!cancelled) {
            double d = floor(pr_get_stat(attacker, STAT_HP, false) * (1.0 / 8));
            pr_damage_and_update(bt, e, attacker, d < 1 ? 1 : d, HIT_RESULT_INDIRECT, false, NULL);
        }
        return;
    }
    case BATTLER_TAG_TYPE_BANEFUL_BUNKER:
        pr_try_set_status(bt, e, attacker, STATUS_EFFECT_POISON, p, -1, false, true);
        return;
    case BATTLER_TAG_TYPE_BURNING_BULWARK:
        pr_try_set_status(bt, e, attacker, STATUS_EFFECT_BURN, p, -1, false, true);
        return;
    case BATTLER_TAG_TYPE_KINGS_SHIELD: stat = STAT_ATK; stages = -1; break;
    case BATTLER_TAG_TYPE_OBSTRUCT: stat = STAT_DEF; stages = -2; break;
    case BATTLER_TAG_TYPE_SILK_TRAP: stat = STAT_SPD; stages = -1; break;
    default: return;
    }
    /* StatStageChangePhase({battlerIndex: attacker, changes, sourcePokemon: getSourcePokemon()}):
     * a protection tag has no sourceId, so no source */
    pr_unshift_stat_stage_change(bt, e, pr_battler_index(bt, attacker), &stat, 1, stages, NULL);
}

/* getSourcePokemon(): getPokemonById(sourceId), reported when it isn't found and a side's party
 * isn't modelled (it could be there) */
static pr_pokemon *tag_source(pr_battle *bt, const pr_battler_tag *t)
{
    pr_pokemon *source = pr_pokemon_by_id(bt, t->source_id);
    if (!source && t->source_id >= 0 && (!bt->n_party[0] || !bt->n_party[1])) pr_note_unported(bt, PR_FEAT_PARTY);
    return source;
}

/* MoveRestrictionBattlerTag.isMoveRestricted (Torment's, which reads the user's history, is
 * only asked at move selection) */
static bool move_restricted(const pr_battler_tag *t, int move)
{
    switch (t->type) {
    case BATTLER_TAG_TYPE_THROAT_CHOPPED: return (pr_moves[move].flags & MOVE_FLAGS_SOUND_BASED) != 0;
    case BATTLER_TAG_TYPE_DISABLED: return move == t->move_id;
    case BATTLER_TAG_TYPE_TAUNT: return pr_moves[move].category == MOVE_CATEGORY_STATUS;
    case BATTLER_TAG_TYPE_ENCORE: return move != t->move_id;
    case BATTLER_TAG_TYPE_GORILLA_TACTICS: return move != t->move_id;
    }
    for (unsigned i = 0; i < pr_set_heal_blocked_moves_n; i++)           /* HEAL_BLOCK: healBlockedMoves */
        if (pr_set_heal_blocked_moves[i] == move) return true;
    return false;
}

/* interruptedText(pokemon, move) of a MoveRestrictionBattlerTag; false where it is "" (the base
 * class's, which Encore keeps), and then nothing is queued */
static bool interrupted_text(const pr_battle *bt, const pr_pokemon *p, const pr_battler_tag *t, int move, pr_msg *out)
{
    pr_msg_arg name = pr_arg_pokemon(bt, "pokemonNameWithAffix", p);
    switch (t->type) {
    case BATTLER_TAG_TYPE_THROAT_CHOPPED:
        *out = pr_msg1("battle:throatChopInterruptedMove", pr_arg_pokemon(bt, "pokemonName", p));
        return true;
    case BATTLER_TAG_TYPE_DISABLED:
        *out = pr_msg2("battle:disableInterruptedMove", name, pr_arg_move("moveName", move));
        return true;
    case BATTLER_TAG_TYPE_HEAL_BLOCK:                 /* its selectionDeniedText */
        *out = pr_msg2("battle:moveDisabledHealBlock", name, pr_arg_move("moveName", move));
        return true;
    case BATTLER_TAG_TYPE_TAUNT:
        *out = pr_msg2("battle:moveDisabledTaunt", name, pr_arg_move("moveName", move));
        return true;
    }
    return false;
}

/* MoveRestrictionBattlerTag.lapse: on PRE_MOVE (the MovePhase's check), a restricted move is
 * interrupted (interruptedText when it has one, then cancel()) and the tag stays; any other
 * lapse is BattlerTag.lapse */
static bool restriction_lapse(pr_battle *bt, pr_engine *e, const pr_pokemon *p, pr_battler_tag *t, int lapse_type)
{
    if (lapse_type != BATTLER_TAG_LAPSE_TYPE_PRE_MOVE) return --t->turn_count > 0;
    if (e && e->current >= 0 && e->pool[e->current].type == PH_MOVE
        && move_restricted(t, e->pool[e->current].move.id)) {
        pr_msg m;
        if (interrupted_text(bt, p, t, e->pool[e->current].move.id, &m)) pr_queue_msg(bt, e, m);
        pr_cancel_current_move(e, NULL);
    }
    return true;
}

/* tag.lapse(pokemon, lapseType): whether the tag stays */
static bool lapse(pr_battle *bt, pr_engine *e, pr_pokemon *p, pr_battler_tag *t, int lapse_type)
{
    switch (t->type) {
    case BATTLER_TAG_TYPE_FLINCHED:
        if (lapse_type == BATTLER_TAG_LAPSE_TYPE_PRE_MOVE) {
            pr_cancel_current_move(e, NULL);
            say(bt, e, "battlerTags:flinchedLapse", p);
            pr_ab_params ap = {.pokemon = p, .move = -1};
            pr_apply_ab_attrs(bt, PR_AC_FlinchEffectAbAttr, &ap);
            return true;
        }
        return lapse_type == BATTLER_TAG_LAPSE_TYPE_TURN_END && --t->turn_count > 0;
    case BATTLER_TAG_TYPE_CONFUSED:
        /* the duration only ticks down on PRE_MOVE */
        if (!(lapse_type == BATTLER_TAG_LAPSE_TYPE_PRE_MOVE && --t->turn_count > 0)) return false;
        say(bt, e, "battlerTags:confusedLapse", p);
        pr_unshift_common_anim(bt, e, pr_battler_index(bt, p), -1, COMMON_ANIM_CONFUSION);
        if (pr_battle_rand_seed_int(bt, 3, 0) == 0) confusion_self_hit(bt, e, p);
        return true;
    case BATTLER_TAG_TYPE_INTERRUPTED:
        /* whatever MovePhase is running is cancelled, then BattlerTag.lapse (0 turns: it goes) */
        pr_cancel_current_move(e, NULL);
        return --t->turn_count > 0;
    case BATTLER_TAG_TYPE_RECHARGING:
        if (lapse_type == BATTLER_TAG_LAPSE_TYPE_PRE_MOVE) {
            /* "must recharge": the move is cancelled and the placeholder leaves the queue */
            say(bt, e, "battlerTags:rechargingLapse", p);
            pr_cancel_current_move(e, NULL);
            if (p->n_move_queue) {
                memmove(&p->move_queue[0], &p->move_queue[1], (size_t)(p->n_move_queue - 1) * sizeof p->move_queue[0]);
                p->n_move_queue--;
            }
        }
        return --t->turn_count > 0;
    case BATTLER_TAG_TYPE_MINIMIZED:
    case BATTLER_TAG_TYPE_CHARGED:
    case BATTLER_TAG_TYPE_FIRE_BOOST:
        return lapse_type != BATTLER_TAG_LAPSE_TYPE_CUSTOM || --t->turn_count > 0;
    case BATTLER_TAG_TYPE_SEEDED:
        /* lapseType !== CUSTOM || BattlerTag.lapse, then the drain */
        if (lapse_type == BATTLER_TAG_LAPSE_TYPE_CUSTOM && !(--t->turn_count > 0)) return false;
        return seed_drain(bt, e, p, t);
    case BATTLER_TAG_TYPE_SUBSTITUTE:
        /* MOVE and AFTER_MOVE bring the Pokemon in and out of focus (PokemonAnimPhase); HIT: onHit.
         * Only a CUSTOM lapse removes it */
        if (lapse_type == BATTLER_TAG_LAPSE_TYPE_MOVE || lapse_type == BATTLER_TAG_LAPSE_TYPE_AFTER_MOVE) {
            pr_unshift_pokemon_anim(bt, e, pr_battler_index(bt, p),
                                    lapse_type == BATTLER_TAG_LAPSE_TYPE_MOVE ? POKEMON_ANIM_TYPE_SUBSTITUTE_PRE_MOVE
                                                                              : POKEMON_ANIM_TYPE_SUBSTITUTE_POST_MOVE);
            t->in_focus = lapse_type == BATTLER_TAG_LAPSE_TYPE_MOVE;
        } else if (lapse_type == BATTLER_TAG_LAPSE_TYPE_HIT) {
            substitute_on_hit(bt, e, p);
        }
        return lapse_type != BATTLER_TAG_LAPSE_TYPE_CUSTOM;
    case BATTLER_TAG_TYPE_ENDURING:
    case BATTLER_TAG_TYPE_ENDURE_TOKEN:
        if (lapse_type == BATTLER_TAG_LAPSE_TYPE_CUSTOM) {
            say(bt, e, "battlerTags:enduringLapse", p);
            return true;
        }
        return --t->turn_count > 0;
    case BATTLER_TAG_TYPE_STURDY:
        /* CUSTOM (Pokemon.damage leaving it at 1 HP): the message, and it goes */
        if (lapse_type == BATTLER_TAG_LAPSE_TYPE_CUSTOM) {
            say(bt, e, "battlerTags:sturdyLapse", p);
            return false;
        }
        return --t->turn_count > 0;
    case BATTLER_TAG_TYPE_SLOW_START:
        /* without the ability any more it ends at this lapse */
        if (!pr_has_ability_active(bt, p, ABILITY_ID_SLOW_START)) t->turn_count = 1;
        return --t->turn_count > 0;
    case BATTLER_TAG_TYPE_TRUANT: {
        /* without the ability: BattlerTag.lapse. Otherwise nothing after no move or a NONE entry
         * (getLastXMoves(1)), else the running MovePhase is cancelled with the ability shown
         * around the message; the tag stays */
        if (!pr_has_ability_active(bt, p, ABILITY_ID_TRUANT)) return --t->turn_count > 0;
        const pr_turn_move *last = pr_last_move(bt, p, 0);
        if (!last || last->move == MOVE_ID_NONE) return true;
        pr_cancel_current_move(e, NULL);
        pr_queue_ability_display(bt, e, p, p->ability != ABILITY_ID_TRUANT, true);
        say(bt, e, "battlerTags:truantLapse", p);
        pr_queue_ability_display(bt, e, p, p->ability != ABILITY_ID_TRUANT, false);
        return true;
    }
    case BATTLER_TAG_TYPE_BEAK_BLAST_CHARGING:
        /* AFTER_HIT: a contact move burns its user (trySetStatus(BURN, pokemon)); the tag stays */
        if (lapse_type == BATTLER_TAG_LAPSE_TYPE_AFTER_HIT) {
            int move;
            pr_pokemon *attacker = pr_move_effect_user(bt, e, &move);
            if (attacker && pr_move_does_flag_effect_apply_ex(bt, move, MOVE_FLAGS_MAKES_CONTACT, attacker, p, false))
                pr_try_set_status(bt, e, attacker, STATUS_EFFECT_BURN, p, -1, false, true);
            return true;
        }
        return --t->turn_count > 0;
    case BATTLER_TAG_TYPE_SHELL_TRAP:
        /* AFTER_HIT: an opponent's physical move (by the move's own category) makes the owner move
         * next and activates the trap; the tag stays */
        if (lapse_type == BATTLER_TAG_LAPSE_TYPE_AFTER_HIT) {
            int move;
            pr_pokemon *attacker = pr_move_effect_user(bt, e, &move);
            if (attacker && pr_moves[move].category == MOVE_CATEGORY_PHYSICAL && pr_is_opponent(bt, p, attacker)) {
                pr_force_move_next(e, p);
                t->activated = true;
            }
            return true;
        }
        return --t->turn_count > 0;
    case BATTLER_TAG_TYPE_THROAT_CHOPPED:
    case BATTLER_TAG_TYPE_HEAL_BLOCK:
    case BATTLER_TAG_TYPE_DISABLED:
    case BATTLER_TAG_TYPE_TAUNT:
        return restriction_lapse(bt, e, p, t, lapse_type);
    case BATTLER_TAG_TYPE_TORMENT:                /* only leaving the field ends it */
        return active_on_field(p);
    case BATTLER_TAG_TYPE_ENCORE:
        /* no longer added this turn; it ends early once the move has no PP left */
        t->added_this_turn = false;
        return pr_moveset_slot_with_pp(p, t->move_id) >= 0 && restriction_lapse(bt, e, p, t, lapse_type);
    case BATTLER_TAG_TYPE_SALT_CURED:
    case BATTLER_TAG_TYPE_CURSED:
    case BATTLER_TAG_TYPE_NIGHTMARE: {
        /* lapseType !== CUSTOM || BattlerTag.lapse; while it stays: (Nightmare's message first) an
         * animation (Nightmare's is CURSE on the Pokemon; Salt Cure's and Curse's are both SALT_CURE,
         * from the Pokemon to itself), then unless Magic Guard blocks it the damage (a quarter of
         * max HP; Salt Cure an eighth on Steel or Water types, else a sixteenth) and (Salt Cure's,
         * Curse's) message */
        bool ret = lapse_type != BATTLER_TAG_LAPSE_TYPE_CUSTOM || --t->turn_count > 0;
        if (!ret) return false;
        int idx = pr_battler_index(bt, p);
        if (t->type == BATTLER_TAG_TYPE_NIGHTMARE) {
            say(bt, e, "battlerTags:nightmareLapse", p);
            pr_unshift_common_anim(bt, e, idx, -1, COMMON_ANIM_CURSE);
        } else {
            pr_unshift_common_anim(bt, e, idx, idx, COMMON_ANIM_SALT_CURE);
        }
        bool cancelled = false;
        pr_ab_params ap = {.pokemon = p, .move = -1, .flag = &cancelled};
        pr_apply_ab_attrs(bt, PR_AC_BlockNonDirectDamageAbAttr, &ap);
        if (cancelled) return true;
        double div = 4;
        if (t->type == BATTLER_TAG_TYPE_SALT_CURED)
            div = pr_is_of_type(p, POKEMON_TYPE_STEEL) || pr_is_of_type(p, POKEMON_TYPE_WATER) ? 8 : 16;
        pr_damage_and_update(bt, e, p, max_hp_part(p, div), HIT_RESULT_INDIRECT, false, NULL);
        if (t->type == BATTLER_TAG_TYPE_SALT_CURED)
            pr_queue_msg(bt, e, pr_msg2("battlerTags:saltCuredLapse", pr_arg_pokemon(bt, "pokemonNameWithAffix", p),
                                        move_name_arg(t)));
        else if (t->type == BATTLER_TAG_TYPE_CURSED)
            say(bt, e, "battlerTags:cursedLapse", p);
        return true;
    }
    case BATTLER_TAG_TYPE_CRIT_BOOST:
    case BATTLER_TAG_TYPE_DRAGON_CHEER:
        return lapse_type != BATTLER_TAG_LAPSE_TYPE_CUSTOM || --t->turn_count > 0;
    case BATTLER_TAG_TYPE_GRUDGE:                 /* its CUSTOM lapse is the FaintPhase's (pr_grudge_lapse) */
        return false;
    case BATTLER_TAG_TYPE_SYRUP_BOMB: {
        /* ends at once when its source is gone or off the field; else the message and Speed -1
         * (the Pokemon as its own source), then BattlerTag.lapse */
        pr_pokemon *source = tag_source(bt, t);
        if (!source || !active_on_field(source)) return false;
        say(bt, e, "battlerTags:syrupBombLapse", p);
        int32_t spd = STAT_SPD;
        if (pr_unshift_stat_stage_change(bt, e, pr_battler_index(bt, p), &spd, 1, -1, p) < 0 && e) e->overflow = true;
        return --t->turn_count > 0;
    }
    case BATTLER_TAG_TYPE_DROWSY:
        /* at the end of its count the Pokemon falls asleep: trySetStatus(SLEEP) */
        if (--t->turn_count > 0) return true;
        pr_try_set_status(bt, e, p, STATUS_EFFECT_SLEEP, NULL, -1, false, true);
        return false;
    case BATTLER_TAG_TYPE_POWDER: {
        /* TURN_END ends it. Else, in a MovePhase of a Fire-type move (unless unsuppressed Heavy Rain
         * blocks it first): the move fails, the animation, a quarter of max HP (rounded down)
         * unless Magic Guard blocks it, then the message */
        if (lapse_type == BATTLER_TAG_LAPSE_TYPE_TURN_END) return false;
        if (!e || e->current < 0 || e->pool[e->current].type != PH_MOVE) return true;
        int move = e->pool[e->current].move.id;
        if (pr_get_move_type(bt, p, move) != POKEMON_TYPE_FIRE
            || (bt->weather == WEATHER_TYPE_HEAVY_RAIN && !pr_weather_effect_suppressed(bt)))
            return true;
        pr_fail_current_move(e);
        pr_unshift_common_anim(bt, e, pr_battler_index(bt, p), pr_battler_index(bt, p), COMMON_ANIM_POWDER);
        bool cancelled = false;
        pr_ab_params ap = {.pokemon = p, .move = -1, .flag = &cancelled};
        pr_apply_ab_attrs(bt, PR_AC_BlockNonDirectDamageAbAttr, &ap);
        if (!cancelled) pr_damage_and_update(bt, e, p, floor(pr_get_stat(p, STAT_HP, false) / 4.0), HIT_RESULT_INDIRECT, false, NULL);
        pr_queue_msg(bt, e, pr_msg1("battlerTags:powderLapse", pr_arg_move("moveName", move)));   /* move.name */
        return true;
    }
    case BATTLER_TAG_TYPE_INFATUATED: {
        /* lapseType !== CUSTOM || BattlerTag.lapse; no source ends it. Else the message, the
         * ATTRACT animation, and half the time the immobilized message and the running
         * MovePhase cancelled */
        if (!(lapse_type != BATTLER_TAG_LAPSE_TYPE_CUSTOM || --t->turn_count > 0)) return false;
        const pr_pokemon *source = tag_source(bt, t);
        if (!source) return false;
        pr_queue_msg(bt, e, pr_msg2("battlerTags:infatuatedLapse", pr_arg_pokemon(bt, "pokemonNameWithAffix", p),
                                    pr_arg_pokemon(bt, "sourcePokemonName", source)));
        pr_unshift_common_anim(bt, e, pr_battler_index(bt, p), -1, COMMON_ANIM_ATTRACT);
        if (pr_battle_rand_seed_int(bt, 2, 0)) {
            say(bt, e, "battlerTags:infatuatedLapseImmobilize", p);
            pr_cancel_current_move(e, NULL);
        }
        return true;
    }
    case BATTLER_TAG_TYPE_PERISH_SONG: {
        /* the count, or at its end the Pokemon's remaining HP (INDIRECT_KO, ignoreSegments) */
        bool ret = --t->turn_count > 0;
        if (ret)
            pr_queue_msg(bt, e, pr_msg2("battlerTags:perishSongLapse", pr_arg_pokemon(bt, "pokemonNameWithAffix", p),
                                        pr_arg_int("turnCount", t->turn_count)));
        else pr_damage_and_update(bt, e, p, p->hp, HIT_RESULT_INDIRECT_KO, false, NULL);
        return ret;
    }
    case BATTLER_TAG_TYPE_AQUA_RING:
    case BATTLER_TAG_TYPE_INGRAIN: {
        /* lapseType !== CUSTOM || BattlerTag.lapse; while it stays, a sixteenth of max HP back
         * (PokemonHealPhase with a message) */
        bool ret = lapse_type != BATTLER_TAG_LAPSE_TYPE_CUSTOM || --t->turn_count > 0;
        if (ret)
            pr_unshift_heal_msg(bt, e, pr_battler_index(bt, p), max_hp_part(p, 16), PR_HEAL_MESSAGE,
                                t->type == BATTLER_TAG_TYPE_INGRAIN ? pr_msg_p(bt, "battlerTags:ingrainLapse", p)
                                : pr_msg2("battlerTags:aquaRingLapse", pr_arg_move("moveName", t->source_move),
                                          pr_arg_pokemon(bt, "pokemonName", p)));
        return ret;
    }
    case BATTLER_TAG_TYPE_OCTOLOCK: {
        /* the same test; while it stays, Defense and Sp. Def drop a stage (its source as the
         * StatStageChangePhase's sourcePokemon) */
        bool ret = lapse_type != BATTLER_TAG_LAPSE_TYPE_CUSTOM || --t->turn_count > 0;
        if (ret) {
            static const int32_t stats[2] = {STAT_DEF, STAT_SPDEF};
            if (pr_unshift_stat_stage_change(bt, e, pr_battler_index(bt, p), stats, 2, -1, tag_source(bt, t)) < 0 && e)
                e->overflow = true;
        }
        return ret;
    }
    case BATTLER_TAG_TYPE_PSYCHO_SHIFT:
        /* heals the user of the status it passed on once its move is over (isActive(true)):
         * the message, then resetStatus() as a phase (revive = true) */
        if (p->status && p->hp > 0 && p->on_field && !p->switch_out_status) {
            pr_queue_msg(bt, e, pr_status_msg(bt, p->status, PR_ST_HEAL, p));   /* getStatusEffectHealText */
            pr_reset_status_phase(bt, e, p, true);
        }
        return false;
    case BATTLER_TAG_TYPE_RAGE: {
        /* AFTER_HIT: when attacksReceived.at(-1) (the turn's first record, as they are unshifted)
         * dealt damage, Attack rises a stage (the Pokemon as its own source); the tag stays.
         * PRE_MOVE: BattlerTag.lapse */
        if (lapse_type != BATTLER_TAG_LAPSE_TYPE_AFTER_HIT) return --t->turn_count > 0;
        if (!p->n_attacks_received) return true;
        if (p->n_attacks_received > pr_attacks_received_kept(p)) {
            pr_attacks_received_exhausted(bt, p);
            return true;
        }
        if (p->attacks_received[p->n_attacks_received - 1].damage > 0) {
            int32_t atk = STAT_ATK;
            if (pr_unshift_stat_stage_change(bt, e, pr_battler_index(bt, p), &atk, 1, 1, p) < 0 && e) e->overflow = true;
        }
        return true;
    }
    case BATTLER_TAG_TYPE_FRENZY: {
        /* MoveLockTag: only a use of the locked move (or none) advances it */
        const pr_turn_move *last = pr_last_move(bt, p, 0);
        if (!t->source_move || !last || (last->move != t->source_move && last->move != MOVE_ID_NONE)) return true;
        bool ret = --t->turn_count > 0 && last->n_targets > 0 && last->result == MOVE_RESULT_SUCCESS;
        if (ret) {
            /* pushMoveQueue({move: sourceMove, targets: nextTargets, useMode: IGNORE_PP}) */
            pr_turn_move q;
            memset(&q, 0, sizeof q);
            q.move = t->source_move;
            q.use_mode = MOVE_USE_MODE_IGNORE_PP;
            q.result = PR_MOVE_RESULT_UNDEFINED;
            pr_turn_move lt = *last;
            next_targets(bt, p, t->source_move, &lt, &q);
            if (p->n_move_queue < PR_MAX_MOVE_QUEUE) p->move_queue[p->n_move_queue++] = q;
            else { pr_note_unported(bt, PR_FEAT_ENGINE_LIMIT); if (e) e->overflow = true; }
        }
        return ret;
    }
    }
    if (pr_damaging_trap_move(t->type)) {
        /* DamagingTrapTag.lapse (TURN_END): BattlerTag.lapse, then while the tag lasts its message,
         * its animation and an eighth of max HP, unless BlockNonDirectDamageAbAttr (Magic Guard) */
        if (--t->turn_count <= 0) return false;
        pr_queue_msg(bt, e, pr_msg2("battlerTags:damagingTrapLapse", pr_arg_pokemon(bt, "pokemonNameWithAffix", p),
                                    move_name_arg(t)));
        pr_unshift_common_anim(bt, e, pr_battler_index(bt, p), -1, trap_anim(t->type));
        bool cancelled = false;
        pr_ab_params ap = {.pokemon = p, .move = -1, .flag = &cancelled};
        pr_apply_ab_attrs(bt, PR_AC_BlockNonDirectDamageAbAttr, &ap);
        if (!cancelled) {
            double damage = floor(pr_get_stat(p, STAT_HP, false) / 8.0);
            pr_damage_and_update(bt, e, p, damage < 1 ? 1 : damage, HIT_RESULT_INDIRECT, false, NULL);
        }
        return true;
    }
    if (pr_battler_tag_is(t->type, PR_TAGCLASS_PROTECTED)) {
        /* ProtectedTag.lapse: on CUSTOM (the hit check lapsing it) the message and stopMultiHit on
         * the running MoveEffectPhase; ContactProtectedTag.lapse then, for a contact move, onContact */
        if (lapse_type != BATTLER_TAG_LAPSE_TYPE_CUSTOM) return --t->turn_count > 0;
        /* (CommonBattleAnim(PROTECT).play() first: an animation outside the phases, no event yet) */
        say(bt, e, "battlerTags:protectedLapse", p);
        int move;
        pr_pokemon *attacker = pr_move_effect_user(bt, e, &move);
        if (attacker) pr_stop_multi_hit(bt, e, p);
        if (attacker && t->type != BATTLER_TAG_TYPE_PROTECTED
            && pr_move_does_flag_effect_apply_ex(bt, move, MOVE_FLAGS_MAKES_CONTACT, attacker, p, false)) {
            pr_battler_tag copy = *t;
            protect_on_contact(bt, e, p, &copy, attacker);
        }
        return true;
    }
    return --t->turn_count > 0;                   /* BattlerTag.lapse */
}

/* tag.onRemove(pokemon); t is a copy (the tag may already be out of the list) */
static void on_remove(pr_battle *bt, pr_engine *e, pr_pokemon *p, const pr_battler_tag *t)
{
    switch (t->type) {
    case BATTLER_TAG_TYPE_CONFUSED: say(bt, e, "battlerTags:confusedOnRemove", p); return;
    case BATTLER_TAG_TYPE_TRAPPED:
    case BATTLER_TAG_TYPE_NO_RETREAT:
    case BATTLER_TAG_TYPE_INGRAIN:               /* TrappedTag.onRemove */
    case BATTLER_TAG_TYPE_OCTOLOCK:
        pr_queue_msg(bt, e, pr_msg2("battlerTags:trappedOnRemove", pr_arg_pokemon(bt, "pokemonNameWithAffix", p),
                                    move_name_arg(t)));
        return;
    case BATTLER_TAG_TYPE_HEAL_BLOCK: say(bt, e, "battlerTags:healBlockOnRemove", p); return;
    case BATTLER_TAG_TYPE_DISABLED:              /* moveId: NONE when onAdd found no move to disable */
        pr_queue_msg(bt, e, pr_msg2("battlerTags:disabledOnRemove", pr_arg_pokemon(bt, "pokemonNameWithAffix", p),
                                    pr_arg_move("moveName", t->move_id)));
        return;
    case BATTLER_TAG_TYPE_TAUNT: say(bt, e, "battlerTags:tauntOnRemove", p); return;
    case BATTLER_TAG_TYPE_ENCORE: say(bt, e, "battlerTags:encoreOnRemove", p); return;
    case BATTLER_TAG_TYPE_INFATUATED: say(bt, e, "battlerTags:infatuatedOnRemove", p); return;
    case BATTLER_TAG_TYPE_TELEKINESIS: say(bt, e, "battlerTags:telekinesisOnRemove", p); return;
    case BATTLER_TAG_TYPE_CRIT_BOOST:
    case BATTLER_TAG_TYPE_DRAGON_CHEER: say(bt, e, "battlerTags:critBoostOnRemove", p); return;
    case BATTLER_TAG_TYPE_PROTOSYNTHESIS:         /* HighestStatBoostTag.onRemove: the tag's ability */
    case BATTLER_TAG_TYPE_QUARK_DRIVE:
        pr_queue_msg(bt, e, pr_msg2("battlerTags:highestStatBoostOnRemove", pr_arg_pokemon(bt, "pokemonNameWithAffix", p),
                                    pr_arg_ability("abilityName", t->type == BATTLER_TAG_TYPE_PROTOSYNTHESIS
                                                                      ? ABILITY_ID_PROTOSYNTHESIS : ABILITY_ID_QUARK_DRIVE)));
        return;
    case BATTLER_TAG_TYPE_ROOSTED:
        roost_on_remove(bt, p, t);
        return;
    case BATTLER_TAG_TYPE_POWER_TRICK:
        pr_power_trick_swap(p);
        say(bt, e, "battlerTags:powerTrickActive", p);
        return;
    case BATTLER_TAG_TYPE_SLOW_START:
        say(bt, e, "battlerTags:slowStartOnRemove", p);
        return;
    case BATTLER_TAG_TYPE_STOCKPILING:
        /* each stat drops by the raises the tag counted, from the Pokemon itself */
        if (t->stock_def > 0 || t->stock_spdef > 0) {
            const int8_t stat[] = {STAT_DEF, STAT_SPDEF};
            const int8_t stages[] = {(int8_t)-t->stock_def, (int8_t)-t->stock_spdef};
            pr_unshift_stat_stage_change_ex(bt, e, pr_battler_index(bt, p), stat, stages, 2, p, 0);
        }
        return;
    case BATTLER_TAG_TYPE_SUBSTITUTE:
        /* the removal animation (PokemonAnimPhase) unless the Pokemon is in focus (its own move
         * removed the doll: the sprite is just destroyed), then the message */
        if (!t->in_focus) pr_unshift_pokemon_anim(bt, e, pr_battler_index(bt, p), POKEMON_ANIM_TYPE_SUBSTITUTE_REMOVE);
        say(bt, e, "battlerTags:substituteOnRemove", p);
        return;
    case BATTLER_TAG_TYPE_FRENZY:
        /* a frenzy that ran its course confuses: addTag(CONFUSED, randBattleSeedIntRange(2, 5)) */
        if (t->turn_count <= 0) {
            int turns = pr_battle_rand_seed_int(bt, 4, 2);
            pr_add_tag(bt, e, p, BATTLER_TAG_TYPE_CONFUSED, turns, 0, -1);
        }
        return;
    }
    if (pr_damaging_trap_move(t->type))                                 /* TrappedTag.onRemove */
        pr_queue_msg(bt, e, pr_msg2("battlerTags:trappedOnRemove", pr_arg_pokemon(bt, "pokemonNameWithAffix", p),
                                    move_name_arg(t)));
}

/* FaintPhase: the fainted Pokemon's DestinyBondTag.lapse(source, CUSTOM), called on the tag
 * directly (it stays until resetSummonData). Once the Destiny Bond user has fainted, its
 * attacker loses its remaining HP (INDIRECT_KO), unless it is the user's ally or a boss. */
void pr_destiny_bond_lapse(pr_battle *bt, pr_engine *e, const pr_battler_tag *t, pr_pokemon *attacker)
{
    pr_pokemon *source = tag_source(bt, t);
    if (!source || source->hp > 0) return;
    if (pr_get_ally(bt, source) == attacker) return;
    if (attacker->is_boss) {                       /* isBossImmune() */
        say(bt, e, "battlerTags:destinyBondLapseIsBoss", attacker);
        return;
    }
    pr_queue_msg(bt, e, pr_msg2("battlerTags:destinyBondLapse", pr_arg_pokemon(bt, "pokemonNameWithAffix", source),
                                pr_arg_pokemon(bt, "pokemonNameWithAffix2", attacker)));
    pr_damage_and_update(bt, e, attacker, attacker->hp, HIT_RESULT_INDIRECT_KO, false, NULL);
}

/* FaintPhase: GrudgeTag.lapse(fainted, CUSTOM, source): an active opponent that knocked the
 * Pokemon out loses all the PP of its last non-virtual move (not Struggle), with the message */
void pr_grudge_lapse(pr_battle *bt, pr_engine *e, pr_pokemon *fainted, pr_pokemon *source)
{
    if (!source || source->hp <= 0 || !pr_is_opponent(bt, fainted, source)) return;
    bool unknown;
    const pr_turn_move *last = pr_last_non_virtual_move(bt, source, false, true, &unknown);
    if (!last || last->move == MOVE_ID_STRUGGLE) return;
    for (int s = 0; s < source->n_moves; s++) {
        if (source->moves[s] != last->move) continue;
        int move = source->moves[s];
        double step = floor(pr_moves[move].pp / 5.0);
        int max = source->max_pp_override[s] > 0 ? source->max_pp_override[s]
                  : pr_moves[move].pp + source->pp_up[s] * (int)(step < 1 ? 1 : step);
        int used = source->pp_used[s] + (max - source->pp_used[s]);   /* usePp(remaining) */
        source->pp_used[s] = (uint8_t)(used > max ? max : used);
        pr_queue_msg(bt, e, pr_msg2("battlerTags:grudgeLapse", pr_arg_pokemon(bt, "pokemonNameWithAffix", source),
                                    pr_arg_move("moveName", move)));   /* movesetMove.getName() */
        return;
    }
}

/* ---------------------------------------------------------------- Pokemon's tag methods */

bool pr_add_tag(pr_battle *bt, pr_engine *e, pr_pokemon *p, int type, int turn_count, int source_move,
                int64_t source_id)
{
    if (type == BATTLER_TAG_TYPE_FLINCHED && pr_is_max(p)) return false;
    if (!pr_tag_type_ported(type)) {
        note_tag(bt, type);
        return false;
    }
    int i = find_tag(p, type);
    if (i >= 0) {
        on_overlap(bt, e, p, &p->tags[i]);
        return false;
    }

    pr_battler_tag t = construct(type, turn_count, source_move, source_id);

    /* BattlerTagImmunityAbAttr on the Pokemon, then UserFieldBattlerTagImmunityAbAttr on its
     * side in speed order (the Pokemon itself included) */
    bool cancelled = false;
    pr_ab_params ip = {.pokemon = p, .move = -1, .flag = &cancelled, .tag_type = type};
    pr_apply_ab_attrs(bt, PR_AC_BattlerTagImmunityAbAttr, &ip);
    if (cancelled) return false;
    pr_speed_iter it;
    pr_pokemon *ally;
    pr_speed_iter_init(bt, &it, p->is_player ? ARENA_TAG_SIDE_PLAYER : ARENA_TAG_SIDE_ENEMY);
    while ((ally = pr_speed_iter_next(bt, &it))) {
        pr_ab_params ap = {.pokemon = ally, .target = p, .move = -1, .flag = &cancelled, .tag_type = type};
        pr_apply_ab_attrs(bt, PR_AC_UserFieldBattlerTagImmunityAbAttr, &ap);
        if (cancelled) return false;
    }

    if (!can_add(bt, e, p, &t)) return false;
    if (p->n_tags >= PR_MAX_BATTLER_TAGS) {
        pr_note_unported(bt, PR_FEAT_ENGINE_LIMIT);
        if (e) e->overflow = true;
        return false;
    }
    p->tags[p->n_tags++] = t;
    on_add(bt, e, p, &p->tags[p->n_tags - 1]);
    return true;
}

/* pokemon.canAddTag(type): not there yet, and no tag immunity (simulated, with a stub tag) */
bool pr_can_add_tag(pr_battle *bt, pr_pokemon *p, int type)
{
    if (find_tag(p, type) >= 0) return false;
    bool cancelled = false;
    pr_ab_params ip = {.pokemon = p, .move = -1, .flag = &cancelled, .tag_type = type, .simulated = true};
    pr_apply_ab_attrs(bt, PR_AC_BattlerTagImmunityAbAttr, &ip);
    pr_speed_iter it;
    pr_pokemon *ally;
    pr_speed_iter_init(bt, &it, p->is_player ? ARENA_TAG_SIDE_PLAYER : ARENA_TAG_SIDE_ENEMY);
    while ((ally = pr_speed_iter_next(bt, &it))) {
        pr_ab_params ap = {.pokemon = ally, .target = p, .move = -1, .flag = &cancelled, .tag_type = type, .simulated = true};
        pr_apply_ab_attrs(bt, PR_AC_UserFieldBattlerTagImmunityAbAttr, &ap);
    }
    return !cancelled;
}

void pr_lapse_tags(pr_battle *bt, pr_engine *e, pr_pokemon *p, int lapse_type)
{
    /* tags.filter(t => lapseType === FAINT || (t.lapseTypes has lapseType && !t.lapse(this, lapseType)))
     *     .forEach(t => { t.onRemove(this); tags.splice(tags.indexOf(t), 1); })
     * Every lapse runs before the first removal. */
    bool gone[PR_MAX_BATTLER_TAGS] = {0};
    int n = p->n_tags;
    for (int i = 0; i < n; i++) {
        int type = p->tags[i].type;
        if (lapse_type != BATTLER_TAG_LAPSE_TYPE_FAINT && !lapses_on(type, lapse_type)) continue;
        if (!pr_tag_type_ported(type)) {
            note_tag(bt, type);
            continue;
        }
        gone[i] = lapse_type == BATTLER_TAG_LAPSE_TYPE_FAINT || !lapse(bt, e, p, &p->tags[i], lapse_type);
    }
    /* in list order: onRemove, then the splice (removed counts the earlier splices; a tag onRemove
     * adds goes to the end, after every one still to go) */
    for (int i = 0, removed = 0; i < n; i++) {
        if (!gone[i]) continue;
        pr_battler_tag t = p->tags[i - removed];
        on_remove(bt, e, p, &t);
        splice_tag(p, i - removed);
        removed++;
    }
}

bool pr_lapse_tag(pr_battle *bt, pr_engine *e, pr_pokemon *p, int type, int lapse_type)
{
    int i = find_tag(p, type);
    if (i < 0) return false;
    if (!pr_tag_type_ported(type)) {
        note_tag(bt, type);
        return true;
    }
    if (!lapse(bt, e, p, &p->tags[i], lapse_type)) {
        pr_battler_tag t = p->tags[i];
        on_remove(bt, e, p, &t);
        splice_tag(p, i);
    }
    return true;
}

void pr_remove_tag(pr_battle *bt, pr_engine *e, pr_pokemon *p, int type)
{
    int i = find_tag(p, type);
    if (i < 0) return;
    if (!pr_tag_type_ported(type)) {
        note_tag(bt, type);
        return;
    }
    pr_battler_tag t = p->tags[i];
    on_remove(bt, e, p, &t);
    splice_tag(p, i);
}

void pr_find_and_remove_tags(pr_battle *bt, pr_engine *e, pr_pokemon *p, const int32_t *types, int n_types)
{
    /* findAndRemoveTags(t => types.includes(t.tagType)): the matches in list order, then each
     * one's turnCount = 0, onRemove, splice */
    pr_battler_tag match[PR_MAX_BATTLER_TAGS];
    int n = 0;
    for (int i = 0; i < p->n_tags; i++) {
        int type = p->tags[i].type;
        bool listed = false;
        for (int k = 0; k < n_types && !listed; k++) listed = types[k] == type;
        if (!listed) continue;
        if (!pr_tag_type_ported(type)) {
            note_tag(bt, type);
            continue;
        }
        match[n++] = p->tags[i];
    }
    for (int k = 0; k < n; k++) {
        int i = find_tag(p, match[k].type);
        if (i < 0) continue;
        p->tags[i].turn_count = 0;
        pr_battler_tag t = p->tags[i];
        on_remove(bt, e, p, &t);
        splice_tag(p, i);
    }
}

void pr_remove_field_boost_tag(pr_battle *bt, pr_engine *e, pr_pokemon *p, bool terrain, int now)
{
    /* findAndRemoveTags(tag => "weatherTypes" in tag && !tag.weatherTypes.find(w => w === weather)),
     * and the terrain version: only Protosynthesis's tag lists weathers (Sunny, Harsh Sun), only
     * Quark Drive's lists terrains (Electric). turnCount = 0, onRemove (HighestStatBoostTag's
     * message), splice. */
    int type = terrain ? BATTLER_TAG_TYPE_QUARK_DRIVE : BATTLER_TAG_TYPE_PROTOSYNTHESIS;
    bool listed = terrain ? now == TERRAIN_ELECTRIC : (now == WEATHER_TYPE_SUNNY || now == WEATHER_TYPE_HARSH_SUN);
    int i = find_tag(p, type);
    if (i < 0 || listed) return;
    p->tags[i].turn_count = 0;
    pr_battler_tag t = p->tags[i];
    on_remove(bt, e, p, &t);
    splice_tag(p, i);
}

/* transferTagsBySourceId: tags this Pokemon got from `from` now come from `to` (every tag type:
 * only sourceId changes) */
void pr_transfer_tags_by_source_id(pr_pokemon *p, int64_t from, int64_t to)
{
    for (int i = 0; i < p->n_tags; i++)
        if (p->tags[i].source_id == from) p->tags[i].source_id = to;
}

/* push onto summonData.tags as it is: no canAdd, no onAdd (Baton Pass, Shed Tail) */
static bool push_tag(pr_battle *bt, pr_pokemon *p, const pr_battler_tag *t)
{
    if (p->n_tags >= PR_MAX_BATTLER_TAGS) {
        pr_note_unported(bt, PR_FEAT_ENGINE_LIMIT);
        return false;
    }
    p->tags[p->n_tags++] = *t;
    return true;
}

/* Pokemon.transferSummon(source): Baton Pass hands over the stat stages and the tags that are
 * isBatonPassable for the recipient (dumped per type; Telekinesis refuses Mega and G-Max Gengar).
 * Upstream pushes the same tag objects, and the source's summon data is reset right after, so a
 * copy acts the same. */
void pr_transfer_summon(pr_battle *bt, pr_pokemon *to, const pr_pokemon *from)
{
    for (int s = 0; s < 7; s++) to->stat_stages[s] = from->stat_stages[s];   /* setStatStage: already in -6..6 */
    for (int i = 0; i < from->n_tags; i++) {
        const pr_battler_tag *t = &from->tags[i];
        bool pass = pr_battler_tags_info[t->type].baton;
        if (t->type == BATTLER_TAG_TYPE_TELEKINESIS && to->species == SPECIES_ID_GENGAR) {
            const char *fk = pr_pokemon_form(to)->form_key;
            pass = !(fk && (!strcmp(fk, "mega") || !strcmp(fk, "gigantamax")));
        }
        if (!pass) continue;
        if (t->type == BATTLER_TAG_TYPE_POWER_TRICK) pr_power_trick_swap(to);   /* swapStat(this) */
        push_tag(bt, to, t);
    }
}

/* Shed Tail: the leaving Pokemon's SubstituteTag goes onto the new one's tags */
void pr_pass_substitute(pr_battle *bt, pr_pokemon *to, const pr_pokemon *from)
{
    int i = find_tag((pr_pokemon *)from, BATTLER_TAG_TYPE_SUBSTITUTE);
    if (i >= 0) push_tag(bt, to, &from->tags[i]);
}

void pr_remove_tags_by_source_id(pr_battle *bt, pr_engine *e, pr_pokemon *p, int64_t source_id)
{
    /* findAndRemoveTags(t => t.isSourceLinked() && t.sourceId === sourceId): the matches, then
     * each one's turnCount = 0, onRemove, splice */
    pr_battler_tag match[PR_MAX_BATTLER_TAGS];
    int n = 0;
    for (int i = 0; i < p->n_tags; i++) {
        if (p->tags[i].source_id != source_id) continue;
        int type = p->tags[i].type;
        if (!pr_tag_type_ported(type)) {
            note_tag(bt, type);                   /* isSourceLinked() and what follows aren't known */
            continue;
        }
        if (source_linked(type)) match[n++] = p->tags[i];
    }
    for (int k = 0; k < n; k++) {
        int i = find_tag(p, match[k].type);
        if (i < 0) continue;
        p->tags[i].turn_count = 0;
        pr_battler_tag t = p->tags[i];
        on_remove(bt, e, p, &t);
        splice_tag(p, i);
    }
}
