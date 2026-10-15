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
#include "pr/events.h"

/* Upstream phase classes the engine knows. Names (pr_phase_name) are upstream's phaseName. */
typedef enum {
    PH_NONE,
    PH_MARKER,                 /* DynamicPhaseMarker: stands in the tree for a dynamic phase */
    /* phases of a turn, ported */
    PH_TURN_START, PH_MOVE, PH_MOVE_EFFECT, PH_MOVE_END, PH_FAINT, PH_CHECK_INTERLUDE,
    PH_WEATHER_EFFECT, PH_POSITIONAL_TAG, PH_BERRY, PH_CHECK_STATUS_EFFECT, PH_POST_TURN_STATUS_EFFECT,
    PH_TURN_END, PH_RESET_STATUS, PH_STAT_STAGE_CHANGE, PH_OBTAIN_STATUS_EFFECT, PH_MOVE_CHARGE, PH_POKEMON_HEAL,
    PH_MOVE_HEADER, PH_SWITCH_SUMMON, PH_POST_SUMMON, PH_MOVE_REFLECT, PH_ATTEMPT_RUN,
    PH_SWITCH,                 /* hands control back mid-phase for the player's pick (pr_resume_switch) */
    /* the start of a battle (pr_start_battle) */
    PH_SUMMON, PH_TOGGLE_DOUBLE_POSITION, PH_INIT_ENCOUNTER,
    PH_CHECK_SWITCH,           /* hands control back mid-phase for the player's answer (pr_answer_check_switch) */
    /* presentation: no battle state of their own */
    PH_MESSAGE, PH_DAMAGE_ANIM, PH_COMMON_ANIM, PH_SHOW_ABILITY, PH_HIDE_ABILITY, PH_POKEMON_ANIM, PH_LOAD_MOVE_ANIM, PH_MOVE_ANIM,
    PH_SHINY_SPARKLE,
   /* where a turn hands control back to the caller */
    PH_TURN_INIT, PH_VICTORY, PH_GAME_OVER, PH_BATTLE_END, PH_SELECT_MODIFIER,
    PH_SELECT_BIOME, PH_NEW_BATTLE,
    PH_ATTEMPT_CAPTURE,
    /* not ported yet: created where upstream creates them (so the order stays right), and
     * the turn is reported unported */
    PH_QUIET_FORM_CHANGE,
    PH_DANCER, PH_TERA,
    PH_TRANSFORM,              /* appended: keep existing phase identifiers stable */
    PH_RETURN,                 /* Scene.newBattle reset-arena return, no replacement */
    PH_COUNT_
} pr_phase_type;
#define PH_FIRST_UNPORTED PH_TERA
extern const char *const pr_phase_name[PH_COUNT_];

/* An upstream PokemonMove the phase holds: a moveset slot, or a detached one
 * (`new PokemonMove(id)`, when the command's move isn't usable from the moveset) */
typedef struct {
    uint16_t id;
    int8_t slot;               /* moveset index, -1 = detached */
    uint8_t pp_used;           /* detached only */
    int8_t pp_up;              /* final boss Recover uses -4 */
    int16_t max_pp_override;   /* detached only */
    bool from_transform;      /* selected PokemonMove belonged to summonData.moveset */
} pr_move_ref;

typedef struct pr_phase {
    uint8_t type;              /* pr_phase_type */
    uint8_t marker_of;         /* PH_MARKER: the dynamic phase type it stands for */
    int8_t battler;            /* PokemonPhase.battlerIndex (0-3), -1 = none */
    int64_t user_id;           /* MoveEffectPhase of a delayed attack: the battlerIndex it was given is
                                * the source's Pokemon.id (getPokemonById); -1 = none */
    pr_pokemon *pokemon;       /* MovePhase.pokemon (fixed at creation) */
    /* MovePhase / MoveEffectPhase: the targets array, shared upstream between the turn
     * command, these phases and the move history entry (see pr_targets) */
    int8_t targets;            /* index into engine->target_lists, -1 = none */
    pr_move_ref move;          /* MovePhase */
    uint16_t move_id;          /* MoveEffectPhase.move */
    uint8_t use_mode;
    uint8_t timing;            /* MovePhase.timingModifier */
    bool prevent_instant_revive;       /* FaintPhase */
    bool affect_confusion;             /* ResetStatusPhase */
    int8_t source;             /* FaintPhase.source battler, -1 = none */
    pr_pokemon *end_targets[4]; /* MoveEndPhase.targets */
    uint8_t n_end_targets;
    bool was_follow_up;        /* MoveEndPhase */
    bool upcoming_interlude;   /* TurnEndPhase: skip Pokemon callbacks and arena tag lapse */
    pr_pokemon *source_p;      /* FaintPhase.source (undefined = NULL) */
    /* MovePhase / MoveEffectPhase.moveHistoryEntry: its targets are the shared list (targets),
     * or entry_targets once Encore gave the MovePhase new targets (the entry keeps the old array) */
    pr_turn_move entry;
    int8_t entry_targets;      /* -2 = the same list as targets */
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
    bool ssc_stockpile;            /* onChange: StockpilingTag.onStatStagesChanged (the only caller) */
    uint8_t ssc_source_type;
    /* ObtainStatusEffectPhase: the effect, sourcePokemon (source_p), sleepTurnsRemaining (-1 = undefined) */
    uint8_t ose_effect;
    int8_t ose_sleep_turns;
    /* MoveChargePhase: targetIndex (the MovePhase's first target) */
    int8_t charge_target;
    /* SwitchPhase(switchType, fieldIndex, isModal, doReturn), where a turn hands control back to
     * the caller for the player's pick, and SwitchSummonPhase(switchType, fieldIndex, slotIndex,
     * doReturn, player) */
    int8_t sw_type, sw_field_index, sw_slot;
    bool sw_modal, sw_do_return, sw_player;
    /* PostSummonPhase; PostSummonActivateAbilityPhase (the same phaseName) is one with
     * ps_activate set: getPriority() and which ability it applies */
    int8_t ps_priority;
    bool ps_activate, ps_passive;
    /* PokemonHealPhase: hpHealed and its options (PR_HEAL_*) */
    double heal_hp;
    uint8_t heal_flags;
    /* MessagePhase: its text (key == NULL: a message whose text isn't ported yet) */
    pr_msg msg;
    /* presentation phases: CommonAnimPhase's CommonAnim and target, PokemonAnimPhase's
     * PokemonAnimType, DamageAnimPhase's amount, result and critical flag, MoveAnimPhase's move */
    uint16_t anim;
    int8_t anim_target;
    int8_t transform_target;
    bool transform_sound;
    int32_t anim_amount;
    uint8_t anim_result;
    bool anim_critical;
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
    pr_phase stop_phase;       /* that phase, with its arguments (the switch prompt's) */
    /* PH_SWITCH stop: the SwitchPhase opened the party screen and waits in the pool for the
     * player's pick; pr_resume_switch finishes it and runs on */
    bool awaiting_pick;
    int pick_phase;            /* its pool index */
    /* PH_CHECK_SWITCH stop: the CheckSwitchPhase asked "Will you switch?" and waits in the pool
     * (pick_phase) for pr_answer_check_switch */
    bool awaiting_confirm;
    bool overflow;             /* a fixed-size table ran out: the run is not faithful */
    /* what the turn shows (include/pr/events.h): started afresh by pr_run_turn and
     * pr_resume_switch */
    pr_event events[PR_MAX_EVENTS];
    int n_events;
    bool events_overflow;      /* more than PR_MAX_EVENTS: the later ones were dropped */
} pr_engine;

/* Run one turn from TurnStartPhase, as upstream's PhaseManager would, until the turn is over
 * (the next TurnInitPhase) or a phase outside the turn (victory, a switch prompt...). The turn
 * commands in bt->commands / bt->pre_commands must be set. */
void pr_run_turn(pr_battle *bt, pr_engine *e);

/* The start of a battle, from where upstream's EncounterPhase ends to the first turn: the
 * trainer sends out its Pokemon (SummonPhase), the player sends out theirs, and the PostSummon
 * phases run (entry abilities such as Intimidate and Drizzle, hazards). The game sets up bt first:
 * the parties, with the Pokemon to send out first in slot 0; a wild Pokemon already on the field
 * (EncounterPhase put it there), a trainer's and the player's not yet. Stops at PH_TURN_INIT, at
 * PH_CHECK_SWITCH (a wild battle asks whether the player will switch; bt->battle_style_set skips
 * it, the "Set" battle style), or at a switch pick after a "yes". The game shows the encounter
 * text and the trainer's intro before calling this. Singles only. */
void pr_start_battle(pr_battle *bt, pr_engine *e);
/* Scene.newBattle's player cleanup queue, before Encounter/Return clears summon
 * data. The caller resets the arena itself and removes the old enemy field. */
void pr_prepare_player_cleanup(pr_battle *bt, pr_engine *e, bool reset_arena);
void pr_run_player_cleanup(pr_battle *bt, pr_engine *e);

/* After a stop at PH_CHECK_SWITCH: the player's answer. Yes opens the party screen (a PH_SWITCH
 * stop, answered with pr_resume_switch); no goes on. Runs on to the next stop. */
void pr_answer_check_switch(pr_battle *bt, pr_engine *e, bool yes);

/* After a turn stopped at PH_SWITCH (the party screen of a SwitchPhase), the player's pick: the
 * party slot to send in, or -1 for none. Runs on to the next stop, as pr_run_turn. */
void pr_resume_switch(pr_battle *bt, pr_engine *e, int slot);

/* After a turn stopped at PH_VICTORY (an enemy fainted): the game applies the experience
 * first (upstream's applyPartyExp unshifts the ExpPhases, which run next), then calls this.
 * It does the rest of VictoryPhase (when no enemy is left, a BattleEndPhase at the end of
 * the queue) and runs the rest of the turn to the next stop: PH_BATTLE_END once the battle
 * is won, PH_TURN_INIT, another PH_VICTORY, or a switch pick. The events start afresh.
 * What upstream queues after that BattleEndPhase is the game's to do, in this order:
 * TrainerVictoryPhase (trainer battles), then on the final wave GameOverPhase(victory), else
 * EggLapsePhase, the wave's ModifierRewardPhases, SelectModifierPhase (waves not a multiple
 * of 10), AddEnemyBuffModifierPhase (endless), SelectBiomePhase (new biome), NewBattlePhase. */
void pr_continue(pr_battle *bt, pr_engine *e);

/* ---- the command screen's checks (CommandPhase, src/commands.c) ----
 * They read the battle between turns and change nothing. Each says whether the command is
 * accepted and, when it isn't, puts in *why (may be NULL) the text upstream shows then: key
 * NULL when upstream shows nothing, "" for its empty text box. */
/* pokemon.isMoveSelectable(move): a restricting battler tag (Disable, Encore, Taunt, Torment,
 * Imprison, Heal Block, Throat Chop, Gorilla Tactics), then the move's own restrictions (Belch,
 * Stuff Cheeks, Gigaton Hammer and Blood Moon twice in a row, Gravity) */
bool pr_move_selectable(pr_battle *bt, const pr_pokemon *p, int move, pr_msg *why);
/* PokemonMove.isUsable(p, ignorePp, forSelection) for moveset slot `slot`: not implemented, out
 * of PP, then (forSelection) pr_move_selectable. The player's challenge rules aren't modelled. */
bool pr_move_usable(pr_battle *bt, const pr_pokemon *p, int slot, bool ignore_pp, bool for_selection, pr_msg *why);
/* CommandPhase.handleFightCommand's check for the FIGHT command on slot `slot` (-1: the queued
 * MoveId.NONE of a recharge turn): the move the turn command gets (the slot's move, or Struggle
 * when no move at all is usable), or -1 when the pick is refused (*why says why) */
int pr_fight_move(pr_battle *bt, const pr_pokemon *p, int slot, bool ignore_pp, pr_msg *why);
/* pokemon.isTrapped(trappedAbMessages, simulated): Commander, Shadow Tag / Arena Trap / Magnet
 * Pull (the first trigger message in *ab_msg), a trapping tag, Fairy Lock; Ghost types and Run
 * Away are never trapped */
bool pr_is_trapped(pr_battle *bt, const pr_pokemon *p, pr_msg *ab_msg);
/* The POKEMON command (tryLeaveField with isSwitch): p may switch out. baton: the party screen's
 * Baton Pass switch, which skips the trap check. Which party member may come in is the party
 * screen's business, not checked here. */
bool pr_can_switch(pr_battle *bt, const pr_pokemon *p, bool baton, pr_msg *why);
/* The RUN command (handleRunCommand): not in the End biome, not from a trainer, and p not
 * trapped (an ability's trap shows no text there, upstream's quirk). Whether the escape
 * succeeds is the AttemptRunPhase's roll, not ported yet. */
bool pr_can_run(pr_battle *bt, const pr_pokemon *p, pr_msg *why);

/* SwitchPhase.start: the early exits, or the party screen (sets e->awaiting_pick) */
void pr_switch_phase(pr_battle *bt, pr_engine *e, int ph);

/* ---- used by the phases (src/phases.c) ---- */
int pr_phase_new(pr_engine *e, pr_phase_type type, int8_t battler);
void pr_phase_push(pr_battle *bt, pr_engine *e, int ph);           /* pushPhase */
void pr_phase_unshift(pr_battle *bt, pr_engine *e, int ph);        /* unshiftPhase */
void pr_phase_add_deferred(pr_battle *bt, pr_engine *e, int ph);   /* addPhase(phase, true) */
bool pr_phase_is_dynamic(pr_phase_type t);
/* phaseManager.hasPhaseOfType / tryRemovePhase for a type (optionally one battler) */
bool pr_phase_exists(pr_engine *e, pr_phase_type t, int battler);
bool pr_phase_try_remove(pr_battle *bt, pr_engine *e, pr_phase_type t, int battler);
/* queueMessage(...), queueAbilityDisplay(...): presentation phases. pr_queue_message is a
 * message whose text isn't ported yet; pr_queue_msg carries it. */
void pr_queue_message_at(pr_battle *bt, pr_engine *e, const char *site);
void pr_queue_message_deferred_at(pr_battle *bt, pr_engine *e, const char *site);
#define PR_STR2_(x) #x
#define PR_STR_(x) PR_STR2_(x)
/* the site ("src/phases.c:904") rides in msg.args[0].s, so the turn test can list what is left */
#define pr_queue_message(bt, e) pr_queue_message_at((bt), (e), __FILE__ ":" PR_STR_(__LINE__))
#define pr_queue_message_deferred(bt, e) pr_queue_message_deferred_at((bt), (e), __FILE__ ":" PR_STR_(__LINE__))
void pr_queue_msg(pr_battle *bt, pr_engine *e, pr_msg m);
void pr_queue_msg_deferred(pr_battle *bt, pr_engine *e, pr_msg m);
/* ---- messages and events (src/events.c) ---- */
/* i18next.t(key, {args}) as a message; up to PR_MSG_MAX_ARGS arguments */
pr_msg pr_msg0(const char *key);
pr_msg pr_msg1(const char *key, pr_msg_arg a);
pr_msg pr_msg2(const char *key, pr_msg_arg a, pr_msg_arg b);
pr_msg pr_msg3(const char *key, pr_msg_arg a, pr_msg_arg b, pr_msg_arg c);
pr_msg pr_msg4(const char *key, pr_msg_arg a, pr_msg_arg b, pr_msg_arg c, pr_msg_arg d);
pr_msg_arg pr_arg_pokemon(const pr_battle *bt, const char *name, const pr_pokemon *p);   /* with affix */
pr_msg_arg pr_arg_pokemon_name(const pr_battle *bt, const char *name, const pr_pokemon *p);
pr_msg_arg pr_arg_int(const char *name, int32_t v);
pr_msg_arg pr_arg_move(const char *name, int move);
pr_msg_arg pr_arg_ability(const char *name, int ability);
pr_msg_arg pr_arg_type(const char *name, int type);
pr_msg_arg pr_arg_stat(const char *name, int stat);
pr_msg_arg pr_arg_stats(const char *name, const int8_t *stats, int n);
pr_msg_arg pr_arg_key(const char *name, const char *key);
/* shorthand for the most common shape: {pokemonNameWithAffix} */
pr_msg pr_msg_p(const pr_battle *bt, const char *key, const pr_pokemon *p);
/* keys picked by an enum value (upstream's get...Message helpers); "" where they give "" */
enum { PR_ST_OBTAIN, PR_ST_OBTAIN_SOURCE, PR_ST_ACTIVATION, PR_ST_OVERLAP, PR_ST_HEAL, PR_ST_DESCRIPTION };
const char *pr_status_key(int effect, int kind);
pr_msg pr_status_msg(const pr_battle *bt, int effect, int kind, const pr_pokemon *p);   /* {pokemonNameWithAffix} */
enum { PR_WEATHER_START, PR_WEATHER_LAPSE, PR_WEATHER_CLEAR, PR_WEATHER_CONTINUE, PR_WEATHER_BLOCK, PR_WEATHER_DAMAGE };
const char *pr_weather_key(int weather, int kind);
enum { PR_TERRAIN_START, PR_TERRAIN_CLEAR, PR_TERRAIN_NAME };
const char *pr_terrain_key(int terrain, int kind);
pr_msg pr_terrain_block_msg(const pr_battle *bt, int terrain, const pr_pokemon *p);
/* the text a phase shows itself (ui.showText), as a PR_EV_TEXT event now */
void pr_show_text(pr_battle *bt, pr_engine *e, pr_msg m);
/* append an event with the field as it stands */
void pr_event_add(pr_battle *bt, pr_engine *e, int type, int battler, int target, int id, int32_t value,
                  const pr_msg *m);
void pr_queue_ability_display(pr_battle *bt, pr_engine *e, const pr_pokemon *p, bool passive, bool show);
/* queueFaintPhase(battlerIndex, preventInstantRevive, source) */
void pr_queue_faint(pr_battle *bt, pr_engine *e, int battler, bool prevent_instant_revive, int source);
/* ---- battler tags being added, lapsed and removed (src/battler_tags.c) ---- */
bool pr_tag_type_ported(int type);
/* the move a DamagingTrapTag subclass (Bind, Wrap, Fire Spin...) names as its source; 0 otherwise */
int pr_damaging_trap_move(int type);
/* pokemon.canAddTag(type) */
bool pr_can_add_tag(pr_battle *bt, pr_pokemon *p, int type);
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

/* ---- moves that call moves (src/called_moves.c) ---- */
/* CallMoveAttr.getMove for attribute `a` of `move` (MOVE_ID_NONE when nothing can be called);
 * e may be NULL outside a turn (Nature Power's message is then not queued) */
int pr_call_move_get(pr_battle *bt, pr_engine *e, int move, const pr_attr *a, const pr_pokemon *user,
                     const pr_pokemon *target);
/* getMoveTargets(user, move, replaceTarget): the battler indexes (at most 4) and whether the move
 * hits them all; replace_target -1 = the move's own target */
int pr_get_move_targets(pr_battle *bt, const pr_pokemon *user, int move, int replace_target, int8_t out[4],
                        bool *multiple);
bool pr_move_is_multi_target(int move);              /* Move.isMultiTarget() */
bool pr_instruct_invalid(int move);                  /* invalidInstructMoves.has(move) */
void pr_instruct_keep(const pr_pokemon *p, int slot);   /* RepeatMoveAttr.movesetMove = p's moveset[slot] */
int pr_instruct_slot(const pr_pokemon *p);              /* that slot when it is p's, else -1 */
int pr_unshift_move_phase(pr_battle *bt, pr_engine *e, pr_pokemon *p, int move, int slot, const int8_t *targets,
                          int n_targets, int use_mode, int timing);
/* Pokemon.setMove replaces permanent and temporary slots with one new object. */
void pr_set_move(pr_battle *bt, pr_engine *e, pr_pokemon *p, int slot, uint16_t move);
void pr_detach_temporary_moves(pr_engine *e, pr_pokemon *p);
bool pr_copy_last_move(pr_battle *bt, pr_engine *e, pr_pokemon *user,
                       const pr_pokemon *target, int move, bool permanent);

/* RandomMovesetMoveAttr.selectedMove, one per move (session state upstream keeps on the attribute) */
void pr_called_selected_reset(void);
void pr_called_selected_set(int move, int sel);
int pr_called_selected_list(uint16_t *move, uint16_t *sel, int max);
bool pr_try_set_terrain(pr_battle *bt, pr_engine *e, int terrain, bool ignore_anim, const pr_pokemon *user);
void pr_remove_tags_by_source_id(pr_battle *bt, pr_engine *e, pr_pokemon *p, int64_t source_id);
void pr_transfer_tags_by_source_id(pr_pokemon *p, int64_t from, int64_t to);
/* the FaintPhase's lapses of the fainted Pokemon's Destiny Bond and Grudge */
void pr_destiny_bond_lapse(pr_battle *bt, pr_engine *e, const pr_battler_tag *t, pr_pokemon *attacker);
void pr_grudge_lapse(pr_battle *bt, pr_engine *e, pr_pokemon *fainted, pr_pokemon *source);
/* the moveset slot holding `move` with PP left (!isOutOfPp()), -1 when none */
int pr_moveset_slot_with_pp(const pr_pokemon *p, int move);
/* the move new EncoreTag(..).canAdd(p) would lock p into; 0 when it can't be added, -1 when the
 * history it reads isn't kept (reported) */
int pr_encore_move(pr_battle *bt, const pr_pokemon *p);
/* EncoreTag.tryOverrideMove without the targets: the slot of the move to use instead, -1 = none */
int pr_encore_try_override(pr_pokemon *p);
void pr_power_trick_swap(pr_pokemon *p);
void pr_transfer_summon(pr_battle *bt, pr_pokemon *to, const pr_pokemon *from);   /* Baton Pass */
void pr_pass_substitute(pr_battle *bt, pr_pokemon *to, const pr_pokemon *from);   /* Shed Tail */
/* findAndRemoveTags(t => types.includes(t.tagType)) */
void pr_find_and_remove_tags(pr_battle *bt, pr_engine *e, pr_pokemon *p, const int32_t *types, int n_types);
/* ---- arena tags being added, lapsed and removed (src/arena_tags.c) ---- */
/* arena.addTag(tagType, turnCount, sourceMove, sourceId, side, quiet): whether a new tag was added
 * (an existing one on that side overlaps instead) */
/* positionalTagManager.canAddTag(type, targetIndex) and addTag */
bool pr_pos_can_add(const pr_battle *bt, int type, int target_index);
void pr_pos_add(pr_battle *bt, pr_engine *e, const pr_positional_tag *t);
bool pr_arena_add_tag(pr_battle *bt, pr_engine *e, int type, int turn_count, int source_move, int64_t source_id,
                      int side, bool quiet);
/* EntryHazardTag.maxLayers (canAdd: layers < maxLayers) */
int pr_arena_max_layers(int type);
bool pr_arena_remove_tag_on_side(pr_battle *bt, pr_engine *e, int type, int side, bool quiet);
void pr_arena_remove_tags_on_side(pr_battle *bt, pr_engine *e, const int32_t *types, int n_types, int side, bool quiet);
void pr_arena_remove_all_tags(pr_battle *bt, pr_engine *e);
void pr_arena_gas_source_leave(pr_battle *bt, pr_engine *e);
bool pr_tag_restricts_move(pr_battle *bt, const pr_pokemon *p, const pr_battler_tag *t, int move);
void pr_arena_lapse_tags(pr_battle *bt, pr_engine *e);
/* arena.applyTagsForSide(ConditionalProtectTag, side, false, isProtected, attacker, defender, move,
 * ignoresProtectBypass) */
void pr_arena_conditional_protect(pr_battle *bt, pr_engine *e, int side, const pr_pokemon *defender, int move,
                                  bool *is_protected, bool *bypass);
/* phaseManager.unshiftNew(<PokemonPhase type>, battlerIndex) */
void pr_unshift_new(pr_battle *bt, pr_engine *e, pr_phase_type t, int battler);
void pr_unshift_common_anim(pr_battle *bt, pr_engine *e, int battler, int target, int anim);   /* CommonAnimPhase */
void pr_unshift_pokemon_anim(pr_battle *bt, pr_engine *e, int battler, int type);   /* PokemonAnimPhase */
/* the running phase, when it is a MovePhase (of Pokemon p, unless p is NULL): phase.cancel() */
void pr_cancel_current_move(pr_engine *e, const pr_pokemon *p);
/* MagicCoatTag.apply / ReflectStatusMoveAbAttr.apply: `pokemon` sends `move` back with a
 * MovePhase(pokemon, targets, new PokemonMove(move), REFLECTED, FIRST): at `opponent`, or at the
 * move's own targets for a multi-target move */
void pr_reflect_move(pr_battle *bt, pr_engine *e, pr_pokemon *pokemon, const pr_pokemon *opponent, int move);
/* the running phase's fail() when it is a MovePhase */
void pr_fail_current_move(pr_engine *e);
/* phaseManager.forceMoveNext(phase => phase.pokemon === p) (Shell Trap) */
void pr_force_move_next(pr_engine *e, const pr_pokemon *p);
/* getMovePhase(condition): the first queued MovePhase using `move` (-1: any) of a player's (1),
 * an enemy's (0) or anyone's (-1) Pokemon, in queue order */
pr_phase *pr_find_move_phase(pr_engine *e, int move, int player);
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
/* redirectPokemonMoves(removed, ally): queued single-target moves at `removed` go to `ally` */
void pr_redirect_moves(pr_battle *bt, pr_engine *e, const pr_pokemon *removed, const pr_pokemon *ally);

/* the phases' start() (src/phases.c) */
void pr_init_final_boss_phase_two(pr_battle *bt, pr_engine *e, pr_pokemon *p);
void pr_phase_run(pr_battle *bt, pr_engine *e, int ph);
/* the running phase, when it is a MoveEffectPhase: its user (getUserPokemon) and move; else NULL */
pr_pokemon *pr_move_effect_user(pr_battle *bt, pr_engine *e, int *move);
/* MoveEffectPhase.stopMultiHit(target) on the running MoveEffectPhase */
void pr_stop_multi_hit(pr_battle *bt, pr_engine *e, pr_pokemon *target);

/* One MoveEffectAttr's apply(user, target, move) for MoveEffectPhase `ph` (src/move_effects.c):
 * pr_attrs[attr_index] is one of the move's attributes. target is NULL for POST_TARGET effects. */
/* RemoveHeldItemAttr: one stack; removed receives its original identity/ownership.
 * No transfer to the user occurs. The caller may persist the resulting bt->mods. */
bool pr_remove_move_held_item(pr_battle *bt, pr_engine *e, pr_pokemon *user,
                             pr_pokemon *target, bool berries_only, pr_modifier *removed);
void pr_apply_post_victory_stat_change(pr_battle *bt, pr_engine *e, int attr, pr_pokemon *source, int move);
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
/* msg: the message option (PR_HEAL_MESSAGE); pr_unshift_heal is one whose text isn't ported yet */
void pr_unshift_heal_msg(pr_battle *bt, pr_engine *e, int battler, double hp, unsigned flags, pr_msg msg);
void pr_unshift_heal_at(pr_battle *bt, pr_engine *e, int battler, double hp, unsigned flags, const char *site);
#define pr_unshift_heal(bt, e, battler, hp, flags) \
    pr_unshift_heal_at((bt), (e), (battler), (hp), (flags), __FILE__ ":" PR_STR_(__LINE__))
/* Pokemon.leaveField(clearEffects) (src/phases.c) */
void pr_leave_field(pr_battle *bt, pr_engine *e, pr_pokemon *p, bool clear_effects);
/* Pokemon.resetTurnData(), resetSummonData(); triggerPokemonFormChange(p, <trigger class>) */
void pr_reset_turn_data(pr_pokemon *p);
void pr_reset_summon_data(pr_pokemon *p);
void pr_form_change(pr_battle *bt, const pr_pokemon *p, int trigger_class);
void pr_form_change_delayed(pr_battle *bt, const pr_pokemon *p, int trigger_class);
void pr_weather_form_changes(pr_battle *bt, bool revert, const pr_pokemon *source);
/* SwitchSummonPhase and PostSummonPhase / PostSummonActivateAbilityPhase (src/switching.c) */
void pr_switch_summon_phase(pr_battle *bt, pr_engine *e, int ph);
void pr_post_summon_phase(pr_battle *bt, pr_engine *e, int ph);
bool pr_arena_queue_heal(pr_battle *bt, pr_engine *e, int target_index,
                        int64_t source_id, int move_id, bool restore_pp, uint32_t pokemon_name);
bool pr_arena_queue_heal_name(pr_battle *bt, pr_engine *e, int target_index,
                        int64_t source_id, int move_id, bool restore_pp, pr_msg_arg name);
bool pr_arena_apply_pending_heal(pr_battle *bt, pr_engine *e, pr_pokemon *p, bool simulated);
/* SummonPhase (a battle's first send-out), InitEncounterPhase and CheckSwitchPhase (src/switching.c) */
void pr_summon_phase(pr_battle *bt, pr_engine *e, int ph);
void pr_init_encounter_phase(pr_battle *bt, pr_engine *e, int ph);
void pr_check_switch_phase(pr_battle *bt, pr_engine *e, int ph);
/* ForceSwitchOutAttr (src/switching.c): getSwitchOutCondition() for an attribute with these fields,
 * and apply(). The condition applies Suction Cups-style abilities outside a simulation. */
bool pr_switch_out_condition(pr_battle *bt, bool self_switch, int switch_type, const pr_pokemon *user,
                             const pr_pokemon *target, int move);
void pr_force_switch_out(pr_battle *bt, pr_engine *e, bool self_switch, int switch_type, pr_pokemon *user,
                         pr_pokemon *target, int move);
/* Wimp Out, Emergency Exit (PostDamageForceSwitchAbAttr): canApply (-1 = not modelled), apply */
int pr_wimp_out_can_apply(pr_battle *bt, const pr_pokemon *p, const pr_pokemon *source, double damage,
                          double hp_ratio);
void pr_wimp_out_apply(pr_battle *bt, pr_engine *e, pr_pokemon *p);
/* applyAbAttrs("PostDamageAbAttr", {pokemon, damage, source}) */
void pr_post_damage_ab(pr_battle *bt, pr_pokemon *p, double damage, pr_pokemon *source);
/* Pokemon.setTempAbility(ability): Trace and co. */
void pr_set_temp_ability(pr_battle *bt, pr_pokemon *p, uint16_t ability);
/* Pokemon.resetStatus(revive, false, false, asPhase=true): a ResetStatusPhase, unshifted */
void pr_reset_status_phase(pr_battle *bt, pr_engine *e, pr_pokemon *p, bool revive);
void pr_reset_status_phase_ex(pr_battle *bt, pr_engine *e, pr_pokemon *p, bool revive, bool confusion);
/* Move.calculateBattleAccuracy: variable accuracy, held modifiers, Fog then Gravity. */
double pr_calculate_battle_accuracy(pr_battle *bt, pr_pokemon *user, pr_pokemon *target, int move);
/* Move.getPriorityModifier(user), with its source default simulated=true. */
int pr_move_get_priority_modifier(pr_battle *bt, int move, const pr_pokemon *user);

/* Pokemon.trySetStatus(effect, sourcePokemon, sleepTurnsRemaining (-1 = undefined), sourceText,
 * overrideStatus, quiet): canSetStatus, then an ObtainStatusEffectPhase is unshifted */
bool pr_try_set_status(pr_battle *bt, pr_engine *e, pr_pokemon *p, int effect, pr_pokemon *source, int sleep_turns,
                       bool override_status, bool quiet);
bool pr_try_set_status_msg(pr_battle *bt, pr_engine *e, pr_pokemon *p, int effect, pr_pokemon *source, int sleep_turns,
                           bool override_status, bool quiet, const pr_msg *msg);
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

/* Source held-item identity for translated battle messages; NULL means unsupported. */
const char *pr_held_item_name_key(const pr_modifier *m);
bool pr_poltergeist_message(pr_battle *bt, const pr_pokemon *target, pr_msg *out);

#endif
