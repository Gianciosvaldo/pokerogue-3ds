/* The phase engine: a port of upstream's PhaseManager (src/phase-manager.ts), PhaseTree
 * (src/phase-tree.ts) and the dynamic, speed-ordered queues (src/queues/, dynamic-queue-manager.ts,
 * utils/speed-order.ts), plus the phases of a turn (src/phases.c).
 *
 * Upstream runs a battle as a queue of Phase objects. The order they run in follows from
 * where each one is inserted (the tree's levels, deferral, the speed-sorted queues), so the
 * engine keeps upstream's exact structure, including phases that only show something
 * (messages, animations): where they sit changes where later phases land. */
#ifndef PR_ENGINE_H
#define PR_ENGINE_H
#include "pr/battle.h"

/* Upstream phase classes the engine knows. Names (pr_phase_name) are upstream's phaseName. */
typedef enum {
    PH_NONE,
    PH_MARKER,                 /* DynamicPhaseMarker: stands in the tree for a dynamic phase */
    /* phases of a turn, ported */
    PH_TURN_START, PH_MOVE, PH_MOVE_EFFECT, PH_MOVE_END, PH_FAINT, PH_CHECK_INTERLUDE,
    PH_WEATHER_EFFECT, PH_POSITIONAL_TAG, PH_BERRY, PH_CHECK_STATUS_EFFECT, PH_POST_TURN_STATUS_EFFECT,
    PH_TURN_END, PH_RESET_STATUS, PH_STAT_STAGE_CHANGE, PH_OBTAIN_STATUS_EFFECT, PH_MOVE_CHARGE, PH_POKEMON_HEAL,
    /* presentation: no battle state of their own */
    PH_MESSAGE, PH_DAMAGE_ANIM, PH_COMMON_ANIM, PH_SHOW_ABILITY, PH_HIDE_ABILITY, PH_POKEMON_ANIM,
    /* where a turn hands control back to the caller */
    PH_TURN_INIT, PH_VICTORY, PH_SWITCH, PH_SWITCH_SUMMON, PH_GAME_OVER, PH_BATTLE_END, PH_SELECT_MODIFIER,
    /* not ported yet: created where upstream creates them (so the order stays right), and
     * the turn is reported unported */
    PH_MOVE_HEADER,
    PH_MOVE_REFLECT, PH_DANCER, PH_TERA, PH_ATTEMPT_CAPTURE, PH_ATTEMPT_RUN, PH_TOGGLE_DOUBLE_POSITION,
    PH_COUNT_
} pr_phase_type;
#define PH_FIRST_UNPORTED PH_MOVE_HEADER
extern const char *const pr_phase_name[PH_COUNT_];

/* An upstream PokemonMove the phase holds: a moveset slot, or a detached one
 * (`new PokemonMove(id)`, when the command's move isn't usable from the moveset) */
typedef struct {
    uint16_t id;
    int8_t slot;               /* moveset index, -1 = detached */
    uint8_t pp_used;           /* detached only */
} pr_move_ref;

typedef struct pr_phase {
    uint8_t type;              /* pr_phase_type */
    uint8_t marker_of;         /* PH_MARKER: the dynamic phase type it stands for */
    int8_t battler;            /* PokemonPhase.battlerIndex (0-3), -1 = none */
    pr_pokemon *pokemon;       /* MovePhase.pokemon (fixed at creation) */
    /* MovePhase / MoveEffectPhase: the targets array, shared upstream between the turn
     * command, these phases and the move history entry (see pr_targets) */
    int8_t targets;            /* index into engine->target_lists, -1 = none */
    pr_move_ref move;          /* MovePhase */
    uint16_t move_id;          /* MoveEffectPhase.move */
    uint8_t use_mode;
    uint8_t timing;            /* MovePhase.timingModifier */
    bool prevent_instant_revive;       /* FaintPhase */
    int8_t source;             /* FaintPhase.source battler, -1 = none */
    int8_t end_targets[4];     /* MoveEndPhase.targets (battler indexes) */
    uint8_t n_end_targets;
    bool was_follow_up;        /* MoveEndPhase */
    pr_pokemon *source_p;      /* FaintPhase.source (undefined = NULL) */
    /* MovePhase / MoveEffectPhase.moveHistoryEntry: its targets are the shared list (targets) */
    pr_turn_move entry;
    bool failed, cancelled, thaw;          /* MovePhase */
    /* MoveEffectPhase.hitChecks (result, effectiveness), firstHit / lastHit */
    uint8_t hit_check[4];
    double hit_eff[4];
    uint8_t n_hit_checks;
    bool first_hit, last_hit;
    /* StatStageChangePhase options: the changes, sourcePokemon (source_p), whether a message was
     * given, ignoreAbilities, sourceEffectType (StatChangeSource, 0 = none), processed; and
     * selfTarget, which the constructor fixes */
    int8_t ssc_stat[7], ssc_stages[7];
    uint8_t n_ssc;
    bool ssc_message, ssc_ignore_abilities, ssc_processed, ssc_self_target;
    uint8_t ssc_source_type;
    /* ObtainStatusEffectPhase: the effect, sourcePokemon (source_p), sleepTurnsRemaining (-1 = undefined) */
    uint8_t ose_effect;
    int8_t ose_sleep_turns;
    /* MoveChargePhase: targetIndex (the MovePhase's first target) */
    int8_t charge_target;
    /* PokemonHealPhase: hpHealed and its options (PR_HEAL_*) */
    double heal_hp;
    uint8_t heal_flags;
    uint32_t seq;              /* creation order, for debugging */
} pr_phase;

#define PR_MAX_PHASES 160
#define PR_MAX_LEVELS 24
#define PR_LEVEL_CAP 64
#define PR_DYN_CAP 32
#define PR_MAX_TARGET_LISTS 24
#define PR_MAX_TRACE 512

typedef struct { int8_t v[4]; uint8_t n; } pr_target_list;

typedef struct pr_engine {
    pr_phase pool[PR_MAX_PHASES];
    bool used[PR_MAX_PHASES];
    uint32_t next_seq;
    /* PhaseTree: levels[0] is the bottom (runs last) */
    uint8_t levels[PR_MAX_LEVELS][PR_LEVEL_CAP];
    uint8_t level_len[PR_MAX_LEVELS];
    int n_levels;
    int current_level;
    bool deferred_active;
    /* DynamicQueueManager: one FIFO per dynamic phase type, sorted on every pop */
    uint8_t dyn[PH_COUNT_][PR_DYN_CAP];
    uint8_t n_dyn[PH_COUNT_];
    int current;               /* pool index of the running phase, -1 = none */
    /* shared target arrays (see pr_phase.targets) */
    pr_target_list target_lists[PR_MAX_TARGET_LISTS];
    int n_target_lists;
    int8_t command_targets[4];         /* turn command i's list, -1 = none */
    /* move history entries pushed this turn with a shared list: a change to the list shows in them */
    struct { pr_pokemon *p; uint16_t abs; int8_t list; } hist_alias[16];
    int n_hist_alias;
    /* the phases that ran (upstream's phaseName, ":battler" for PokemonPhases) */
    char trace[PR_MAX_TRACE][40];
    int n_trace;
    uint8_t stop;              /* the phase type the turn stopped at */
    bool overflow;             /* a fixed-size table ran out: the run is not faithful */
} pr_engine;

/* Run one turn from TurnStartPhase, as upstream's PhaseManager would, until the turn is over
 * (the next TurnInitPhase) or a phase outside the turn (victory, a switch prompt...). The turn
 * commands in bt->commands / bt->pre_commands must be set. */
void pr_run_turn(pr_battle *bt, pr_engine *e);

/* ---- used by the phases (src/phases.c) ---- */
int pr_phase_new(pr_engine *e, pr_phase_type type, int8_t battler);
void pr_phase_push(pr_battle *bt, pr_engine *e, int ph);           /* pushPhase */
void pr_phase_unshift(pr_battle *bt, pr_engine *e, int ph);        /* unshiftPhase */
void pr_phase_add_deferred(pr_battle *bt, pr_engine *e, int ph);   /* addPhase(phase, true) */
bool pr_phase_is_dynamic(pr_phase_type t);
/* phaseManager.hasPhaseOfType / tryRemovePhase for a type (optionally one battler) */
bool pr_phase_exists(pr_engine *e, pr_phase_type t, int battler);
bool pr_phase_try_remove(pr_battle *bt, pr_engine *e, pr_phase_type t, int battler);
/* queueMessage(...), queueAbilityDisplay(...): presentation phases */
void pr_queue_message(pr_battle *bt, pr_engine *e);
void pr_queue_message_deferred(pr_battle *bt, pr_engine *e);
void pr_queue_ability_display(pr_battle *bt, pr_engine *e, const pr_pokemon *p, bool show);
/* queueFaintPhase(battlerIndex, preventInstantRevive, source) */
void pr_queue_faint(pr_battle *bt, pr_engine *e, int battler, bool prevent_instant_revive, int source);
/* ---- battler tags being added, lapsed and removed (src/battler_tags.c) ---- */
bool pr_tag_type_ported(int type);
/* the move a DamagingTrapTag subclass (Bind, Wrap, Fire Spin...) names as its source; 0 otherwise */
int pr_damaging_trap_move(int type);
/* pokemon.addTag(type, turnCount, sourceMove, sourceId): whether it was added (0 / -1 = none) */
bool pr_add_tag(pr_battle *bt, pr_engine *e, pr_pokemon *p, int type, int turn_count, int source_move,
                int64_t source_id);
void pr_lapse_tags(pr_battle *bt, pr_engine *e, pr_pokemon *p, int lapse_type);   /* lapseTags(type) */
/* lapseTag(type, lapseType): whether the Pokemon had the tag (upstream's default lapse type is CUSTOM) */
bool pr_lapse_tag(pr_battle *bt, pr_engine *e, pr_pokemon *p, int type, int lapse_type);
void pr_remove_tag(pr_battle *bt, pr_engine *e, pr_pokemon *p, int type);         /* removeTag(type) */
/* After a weather or terrain change to `now`: Protosynthesis's tag (weather) or Quark Drive's
 * (terrain) goes with its message unless it lists the new one */
void pr_remove_field_boost_tag(pr_battle *bt, pr_engine *e, pr_pokemon *p, bool terrain, int now);
/* arena.trySetWeather(weather, user) / trySetTerrain(terrain, ignoreAnim, user): whether it changed.
 * The start or clear message, then per Pokemon in speed order the boost tag it ends and the
 * abilities told about the change */
bool pr_try_set_weather(pr_battle *bt, pr_engine *e, int weather, const pr_pokemon *user);   /* user may be NULL */
bool pr_try_set_terrain(pr_battle *bt, pr_engine *e, int terrain, bool ignore_anim, const pr_pokemon *user);
void pr_remove_tags_by_source_id(pr_battle *bt, pr_engine *e, pr_pokemon *p, int64_t source_id);
/* findAndRemoveTags(t => types.includes(t.tagType)) */
void pr_find_and_remove_tags(pr_battle *bt, pr_engine *e, pr_pokemon *p, const int32_t *types, int n_types);
/* ---- arena tags being added, lapsed and removed (src/arena_tags.c) ---- */
/* arena.addTag(tagType, turnCount, sourceMove, sourceId, side, quiet): whether a new tag was added
 * (an existing one on that side overlaps instead) */
bool pr_arena_add_tag(pr_battle *bt, pr_engine *e, int type, int turn_count, int source_move, int64_t source_id,
                      int side, bool quiet);
/* EntryHazardTag.maxLayers (canAdd: layers < maxLayers) */
int pr_arena_max_layers(int type);
bool pr_arena_remove_tag_on_side(pr_battle *bt, pr_engine *e, int type, int side, bool quiet);
void pr_arena_remove_tags_on_side(pr_battle *bt, pr_engine *e, const int32_t *types, int n_types, int side, bool quiet);
void pr_arena_lapse_tags(pr_battle *bt, pr_engine *e);
/* arena.applyTagsForSide(ConditionalProtectTag, side, false, isProtected, attacker, defender, move,
 * ignoresProtectBypass) */
void pr_arena_conditional_protect(pr_battle *bt, pr_engine *e, int side, const pr_pokemon *defender, int move,
                                  bool *is_protected, bool *bypass);
/* phaseManager.unshiftNew(<PokemonPhase type>, battlerIndex) */
void pr_unshift_new(pr_battle *bt, pr_engine *e, pr_phase_type t, int battler);
/* the running phase, when it is a MovePhase (of Pokemon p, unless p is NULL): phase.cancel() */
void pr_cancel_current_move(pr_engine *e, const pr_pokemon *p);
/* inSpeedOrder(side): the active Pokemon of a side (ArenaTagSide) in speed order, as upstream's
 * generator yields them (the queue is re-sorted, with its seeded shuffle, before each one) */
typedef struct { pr_pokemon *queue[4]; int n; } pr_speed_iter;
void pr_speed_iter_init(pr_battle *bt, pr_speed_iter *it, int side);
pr_pokemon *pr_speed_iter_next(pr_battle *bt, pr_speed_iter *it);   /* NULL when done */
/* sortInSpeedOrder over n Pokemon (payload follows its Pokemon); returns nothing, sorts in place */
void pr_sort_in_speed_order(pr_battle *bt, pr_pokemon **mons, int *payload, int n);
/* shared target arrays */
int pr_targets_new(pr_engine *e, const int8_t *v, int n);
/* The list changed in place (targets[0] = x, splice): update everything that shares it */
void pr_targets_changed(pr_battle *bt, pr_engine *e, int list);
/* pokemon.pushMoveHistory(entry) where entry.targets is shared list `list` (-1 = own copy) */
void pr_push_history_shared(pr_battle *bt, pr_engine *e, pr_pokemon *p, pr_turn_move tm, int list);
void pr_sync_queued_movers(pr_battle *bt, pr_engine *e);

/* the phases' start() (src/phases.c) */
void pr_phase_run(pr_battle *bt, pr_engine *e, int ph);
/* the running phase, when it is a MoveEffectPhase: its user (getUserPokemon) and move; else NULL */
pr_pokemon *pr_move_effect_user(pr_battle *bt, pr_engine *e, int *move);
/* MoveEffectPhase.stopMultiHit(target) on the running MoveEffectPhase */
void pr_stop_multi_hit(pr_battle *bt, pr_engine *e, pr_pokemon *target);

/* One MoveEffectAttr's apply(user, target, move) for MoveEffectPhase `ph` (src/move_effects.c):
 * pr_attrs[attr_index] is one of the move's attributes. target is NULL for POST_TARGET effects. */
void pr_apply_move_effect(pr_battle *bt, pr_engine *e, int ph, int attr_index, pr_pokemon *user, pr_pokemon *target);
/* unshiftNew("StatStageChangePhase", {battlerIndex, changes: groupStatChange(stats, stages),
 * sourcePokemon}) with no message and no other option; returns the phase (-1 when out of room) */
int pr_unshift_stat_stage_change(pr_battle *bt, pr_engine *e, int battler, const int32_t *stats, int n_stats,
                                 int stages, pr_pokemon *source);
/* Pokemon.damageAndUpdate(damage, {result, ignoreFaintPhase, source}): the DamageAnimPhase, the
 * damage, PostDamage abilities; returns the damage dealt */
double pr_damage_and_update(pr_battle *bt, pr_engine *e, pr_pokemon *p, double damage, int result,
                            bool ignore_faint_phase, pr_pokemon *source);
/* unshiftNew("PokemonHealPhase", battler, hpHealed, options): options as PR_HEAL_* flags, upstream's
 * defaults being showFullHpMessage and nothing else */
enum {
    PR_HEAL_MESSAGE = 1,            /* a message was given */
    PR_HEAL_NO_FULL_HP_MESSAGE = 2, /* showFullHpMessage: false */
    PR_HEAL_SKIP_ANIM = 4,
    PR_HEAL_REVIVE = 8,
    PR_HEAL_STATUS = 16,
    PR_HEAL_PREVENT_FULL = 32,
    PR_HEAL_RESTORE_PP = 64,
};
void pr_unshift_heal(pr_battle *bt, pr_engine *e, int battler, double hp, unsigned flags);
/* Pokemon.resetStatus(revive, false, false, asPhase=true): a ResetStatusPhase, unshifted */
void pr_reset_status_phase(pr_battle *bt, pr_engine *e, pr_pokemon *p, bool revive);
/* Pokemon.trySetStatus(effect, sourcePokemon, sleepTurnsRemaining (-1 = undefined), sourceText,
 * overrideStatus, quiet): canSetStatus, then an ObtainStatusEffectPhase is unshifted */
bool pr_try_set_status(pr_battle *bt, pr_engine *e, pr_pokemon *p, int effect, pr_pokemon *source, int sleep_turns,
                       bool override_status, bool quiet);
/* Pokemon.canSetStatus(effect, quiet, overrideStatus, sourcePokemon, ignoreField) */
bool pr_can_set_status(pr_battle *bt, pr_engine *e, pr_pokemon *p, int effect, bool quiet, bool override_status,
                       const pr_pokemon *source, bool ignore_field);
/* unshiftNew("StatStageChangePhase", {battlerIndex, changes, sourcePokemon, sourceEffectType}) as the
 * stat-change abilities queue it (Mirror Armor, Opportunist, Defiant); -1 when out of room */
int pr_unshift_stat_stage_change_ex(pr_battle *bt, pr_engine *e, int battler, const int8_t *stat,
                                    const int8_t *stages, int n, pr_pokemon *source, int source_type);
/* move.applyConditions(user, target, sequence 2, 3 or 4): every condition of that list holds */
bool pr_move_apply_conditions(pr_battle *bt, int move, const pr_pokemon *user, const pr_pokemon *target, int seq);
/* getCounterAttackTarget(user, damageCategory): the battler index, or -2 for null (category -1 = any) */
int pr_counter_attack_target(pr_battle *bt, const pr_pokemon *user, int category);

#endif
