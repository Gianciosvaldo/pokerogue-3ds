/* game-core internals shared by the files in src/ */
#ifndef GC_INTERNAL_H
#define GC_INTERNAL_H
#include "gc/run.h"
#include "gc/custom.h"
#include "gc/ports.h"
#include "gc/data.h"
#include "pr/ai.h"
#include "pr/reward.h"
#include <math.h>
#include <string.h>

/* ---- the run's phase queue: upstream's PhaseTree for the run-level phases (src/phase-tree.ts). A push
 * goes to the bottom level (runs last); a phase added while another runs (unshiftNew) goes to the level
 * above, so the phases one phase adds run right after it, in the order it added them. */
enum {
    RP_NONE,
    RP_NEW_BATTLE,       /* NewBattlePhase + the EncounterPhase that follows it */
    RP_TURN_INIT,        /* TurnInitPhase, then the COMMAND screen */
    RP_COMMAND,          /* CommandPhase: waits on the COMMAND screen */
    RP_TURN,             /* battle-core's turn, from TurnStartPhase to its next stop */
    RP_SWITCH,           /* battle-core's SwitchPhase: waits on the SWITCH screen */
    RP_VICTORY,          /* VictoryPhase: a = enemy party index */
    RP_EXP,              /* ExpPhase / ShowPartyExpBarPhase: a = party index, d = exp */
    RP_LEVEL_UP,         /* LevelUpPhase: a = party index, b = last level, c = new level */
    RP_LEARN_MOVE,       /* LearnMovePhase: a = party index, b = move; may wait on LEARN_MOVE */
    RP_EVOLUTION,        /* EvolutionPhase: a = party index, b = gc_evo_data index, c = last level, d = secondary half */
    RP_CONTINUE,         /* the rest of the turn after a VictoryPhase's experience (battle-core's pr_continue) */
    RP_CHECK_SWITCH,     /* CheckSwitchPhase: waits on CHECK_SWITCH (only with run->ask_switch) */
    RP_BATTLE_END,       /* BattleEndPhase: a = victory */
    RP_TRAINER_VICTORY,  /* TrainerVictoryPhase */
    RP_MONEY_REWARD,     /* MoneyRewardPhase: d = money multiplier */
    RP_MODIFIER_REWARD,  /* ModifierRewardPhase: key */
    RP_SELECT_MODIFIER,  /* SelectModifierPhase: waits on REWARD */
    RP_SELECT_BIOME,     /* SelectBiomePhase: may wait on BIOME */
    RP_SWITCH_BIOME,     /* SwitchBiomePhase: a = biome */
    RP_PARTY_HEAL,       /* PartyHealPhase */
    RP_GAME_OVER,        /* GameOverPhase: a = victory; waits on GAME_OVER / CLEAR */
    RP_CATCH,            /* AttemptCapturePhase.catch() after the roll: a = enemy index, b = ball; waits on SWITCH when the party is full */
    RP_MYSTERY,          /* MysteryEncounterPhase: a = selected type, b = selected tier */
    RP_REWARD_FORM,      /* bounded FormChangePhase: a = storage party index, b = target form */
    RP_ME_POST,          /* PostMysteryEncounterPhase's onPostOptionPhase: a = encounter type, b = party index, c = option */
    RP_CUSTOM_OFFER,     /* custom content: the app's rematch offer, waits on CUSTOM_OFFER (gc/custom.h) */
    RP_FG_KO,            /* Fun and Games!: MysteryEncounterRewardsPhase running doContinueEncounter (handleLoseMinigame) */
    RP_EGG_LAPSE,        /* EggLapsePhase: wave progress and ready hatch/profile rewards */
    RP_ME_BEFORE_REWARDS, /* a mystery encounter's callback before the rewards (Training Session: the Pokemon comes back changed; Berries Abound: the berries) */
    RP_COUNT_
};
typedef struct { uint8_t type; int32_t a, b, c; double d; const char *key; } gc_rphase;
#define GC_RQ_LEVELS 12
#define GC_RQ_CAP 24
typedef struct {
    gc_rphase q[GC_RQ_LEVELS][GC_RQ_CAP];
    int n[GC_RQ_LEVELS];
    int n_levels, current_level;
    gc_rphase current;          /* the phase running now (type RP_NONE = none) */
    bool waiting;               /* it waits on a screen */
    bool overflow;
} gc_rqueue;

/* ---- the reward screen (src/gc_reward.c): what a SelectModifierPhase keeps between its calls ---- */
typedef struct {
    pr_reward_option options[PR_REWARD_MAX_OPTIONS];   /* typeOptions of the open phase */
    int n_options;
    bool has_custom, is_copy;
    pr_reward_custom custom;    /* its customModifierSettings: the no-victory screen's, or a copy's old options */
    pr_reward_thresholds th;    /* modifierPoolThresholds: global upstream, kept so a copy rolls with the old ones */
    /* copy(): the options, rerollCount and rerollMultiplier a copy of the screen is made from, until it starts */
    bool copy_pending;
    int copy_reroll_count;
    double copy_reroll_multiplier;
    pr_reward_option copy_options[PR_REWARD_MAX_OPTIONS];
    int n_copy_options;
    pr_reward_custom me_custom; /* the customModifierSettings of a mystery encounter's setEncounterRewards, until its screen opens */
} gc_reward_state;

/* run-level state that isn't part of the public struct */
typedef struct {
    gc_rqueue rq;
    uint32_t stub_rng;          /* the stand-ins' own RNG, so they never move upstream's */
    int reroll_count;
    int battle_item_count[2];   /* represented inventory prefix; rebuilt on load/setup */
    /* ai-core's view of the battle: enemySwitchCounter, each Pokemon's AI entry (initialTeamIndex,
     * waveData.abilityRevealed); reset at each new battle. ai.trainer is set at each call, as it
     * points into this struct */
    pr_ai ai;
    pr_ai_trainer ai_trainer;
    bool lock_rarities;         /* globalScene.lockModifierTiers (the Lock Capsule's switch) */
    gc_reward_state reward;
    int last_battle_type;       /* the finished battle's BattleType, -1 = none yet */
    bool last_final_boss;       /* it was the classic final boss */
    double last_max_exp_level;  /* getMaxExpLevel() when it ended, for LevelCapPhase */
    bool pending_trainer_victory;
    bool battle_won;            /* a VictoryPhase has run in this battle: its BattleEndPhase is the victory's */
    uint8_t thrown_ball;        /* the PokeballType of the last ball command: the one a catch is made with */
    bool pending_me_miss;       /* EncounterPhase increments spawn chance after the save point */
    pr_msg refusal_message;     /* structured reason for the last refused choice; never saved */
    bool undo_first, undo_save_point, undo_rng_live, undo_battle_rng_live;
    pr_rng undo_rng, undo_battle_rng;
    struct pr_turn_command undo_commands[2], undo_pre_commands[2];
    bool at_save_point;         /* from GC_EV_SAVE_POINT until the battle's first turn runs (gc_run_can_save) */
    /* gc_trainer_names' ring: run->trainer_names' texts, gc_m_trainer's key for each and whether that
     * key is the whole name (bit per GC_TNAME_*) */
    uint8_t name_ring, name_exact[3];
    char name_text[3][GC_TNAME_COUNT_][128];
    char name_arg[3][GC_TNAME_COUNT_][72];
    /* custom content (gc/custom.h): the app's hooks (not saved, the app installs them again after a load) and the offer a wave is waiting on */
    const gc_custom_hooks *custom;
    char custom_trainer_name[48];      /* a team's plain-text trainer name (not saved: gc_custom_set_trainer_name after a load) */
    bool custom_team_on;               /* the battle's party came from a team: enemy_items gives its members the team's items */
    gc_custom_team custom_team;
    bool custom_offering, custom_offer_reset, custom_offer_new_biome;
    uint16_t enemy_start_species[6];   /* the enemy party's species and forms when the battle began (gc_custom_actor.start_*); not saved */
    uint8_t enemy_start_form[6];
    /* Fun and Games!: the minigame is running (the encounter's misc.turnsRemaining and its handlers). Not saved: the only
     * save point is the encounter's start, before the option is chosen */
    struct { bool active; int8_t turns_remaining; } fg;
    /* the Pillar Men's fight is running (option 0..2): its victory is the encounter's rewards. Not saved, for the same reason as fg */
    struct { bool active; int8_t option; } pillar;
    /* an encounter's fight that is not allowed to flee (withFleeAllowed(false)) and, for a fight with a guaranteed reward (Fight or Flight), what its
     * victory opens: setEncounterRewards({guaranteedModifierTypeOptions: [item], fillRemaining: false}). Not saved, for the same reason as fg */
    bool me_no_flee;
    struct { bool active, fill, before; int16_t func; bool has_item; pr_reward_option item; uint8_t n_more; pr_reward_option more[5]; } me_reward;   /* the shop: one item (has_item) or one reward function (func >= 0) */
    /* the encounter's startOfBattleEffects (source battler, its target, the move): copied into the battle at its setup, played once by the
     * first TurnInitPhase */
    struct { int8_t source, target; uint16_t move; uint8_t use_mode; } me_start[4];
    uint8_t n_me_start;
    /* withPreventGameStatsUpdates(true): the enemy is not counted as seen (Training Session: it is the player's own Pokemon) */
    bool me_no_stats;
    /* Training Session: the Pokemon picked is out of the party for the fight (removePokemonFromPlayerParty(.., false)), with the held items
     * it will get back and what the rewards phase does to it (onBeforeRewardsPhase). Not saved, for the same reason as fg */
    struct { bool pending; uint8_t n; } berries;   /* Berries Abound: the berries the fight's win gives */
    struct { bool away; int8_t kind, nature, ability; int n_kept; gc_mon mon; gc_item kept[64]; } training;
    /* A football run (gc_football_new): its members evolve by the football rules (gc_mon_football_evolutions). Never set on a classic or
     * custom run, and not saved */
    bool football;
} gc_internal;
gc_internal *gc_int(gc_run *run);

void gc_rq_reset(gc_rqueue *q);
void gc_rq_push(gc_run *run, gc_rphase p);        /* pushNew */
void gc_rq_unshift(gc_run *run, gc_rphase p);     /* unshiftNew */
bool gc_rq_has(gc_run *run, int type);
bool gc_rq_remove(gc_run *run, int type);          /* tryRemovePhase */
void gc_rq_remove_all(gc_run *run, int type);
void gc_rq_clear(gc_run *run);
bool gc_rq_next(gc_rqueue *q, gc_rphase *out);
void gc_run_until_screen(gc_run *run);            /* runs phases until one waits on a screen */
static inline gc_rphase gc_rp(int type, int a, int b, int c) { gc_rphase p = {(uint8_t)type, a, b, c, 0, NULL}; return p; }

/* ---- events (src/gc_run.c) ---- */
gc_event *gc_ev_new(gc_run *run, int kind);
void gc_ev_money(gc_run *run, double change);
gc_event *gc_ev_msg(gc_run *run, const char *key, int n, ...);   /* n pr_msg_arg values follow */
pr_msg_arg gc_m_mon(gc_run *run, const char *name, int side, int idx);   /* getPokemonNameWithAffix */
pr_msg_arg gc_m_mon_name(gc_run *run, const char *name, int side, int idx);   /* getNameToRender */
pr_msg_arg gc_m_trainer(gc_run *run, const char *name, int which);       /* trainer.getName (GC_TNAME_*) */
pr_msg_arg gc_m_item(const char *name, const char *item_key);            /* a modifier type's name */
pr_msg_arg gc_m_item_arg(const char *name, const char *item_key, int arg); /* generated modifier's concrete type name */
void gc_ev_battle_chunk(gc_run *run, const pr_event *events, int count);
void gc_ev_battle(gc_run *run);                   /* battle-core's events of the last call, as GC_EV_BATTLE */
void gc_stub(gc_run *run, int which);

/* ---- Pokemon (src/gc_mon.c) ---- */
const pr_species *gc_species(int id);
const pr_form *gc_form(int species, int form);
int gc_form_ability(int species, const pr_form *f, int ability_index);
void gc_mon_new_player(gc_run *run, gc_mon *m, const gc_starter *s, int level);
void gc_mon_new_enemy(gc_run *run, gc_mon *m, int species, int level, const pr_enemy_gen *g, int trainer_slot);
const pr_species_gen *gc_species_gen(int id);                          /* run-core's per-species facts (by SpeciesId), NULL = none */
/* new PlayerPokemon(species, level, abilityIndex, formIndex, undefined, shiny, undefined) as PokemonData.toPokemon()
 * makes an EnemyPokemon of it (is_player false, the stats of the player's constructor); the rolls are on `rng`.
 * form < 0 = getSpeciesFormIndex() (only for a species without forms), shiny < 0 = trySetShiny(). The moveset is
 * not rolled (Pokemon.generateAndPopulateMoveset isn't ported for a player's Pokemon: the stand-in is the caller's).
 * false = a species the roll can't be made for; the mon is then unusable. */
bool gc_egg_generate(gc_run *run, const gc_egg *egg, gc_mon *out);
int gc_system_hatch_ready_eggs(gc_run *run);
bool gc_mon_roll_player(gc_run *run, pr_rng *rng, gc_mon *m, int species, int level, int ability_index, int form, int shiny);
/* Clean factory followed by validated ordinary wild level pools; false leaves RNG/mon untouched. */
bool gc_enemy_moveset(gc_run *run, gc_mon *m, const pr_enemy_gen *g);
/* generateMoveset for a wild, non-boss Pokemon on `rng` (src/gc_movegen.c); false = a Pokemon it does not cover, left untouched */
bool gc_generate_moveset(gc_run *run, pr_rng *rng, gc_mon *m);
/* Move.calculateEffectivePower(pokemon) of each of `n` moves for a Pokemon (what the differential compares); false = a passive */
bool gc_mg_effective_powers(gc_run *run, const gc_mon *m, const uint16_t *moves, int n, double *out);
void gc_mon_set_move(gc_mon *m, int slot, int move);                  /* setMove */
int gc_move_max_pp(const pr_pokemon *p, int slot);                     /* PokemonMove.getMovePp */
bool gc_move_usable(const pr_pokemon *p, int slot, const char **why);  /* the PP and implementation checks */
/* the moves LevelUpPhase queues for levels last+1..level (own level moves, no prevolution ones) */
int gc_level_up_moves(const gc_mon *m, int starting_level, uint16_t *out, int max);
/* the level-0 (EVOLVE_MOVE) moves EvolutionPhase queues */
void gc_mon_add_exp(gc_run *run, gc_mon *m, double exp);              /* addExp */
double gc_level_total_exp(int level, int growth_rate);
/* EvolutionItemModifier.apply's evolution: the gc_evo_data index, -1 = none */
int gc_mon_football_evolutions(gc_run *run, const gc_mon *m, int from_level, int *out, int cap);
int gc_mon_item_evolution(gc_run *run, const gc_mon *m, int item);
int gc_mon_item_evolution_target(gc_run *run, const gc_mon *m, int item, bool *fusion);
int gc_mon_item_evolutions(gc_run *run, const gc_mon *m, int *out, int cap);
/* getEvolution(): the gc_evo_data index, -1 = none */
int gc_mon_get_evolution(gc_run *run, const gc_mon *m); /* primary-only compatibility helper */
int gc_mon_get_evolution_target(gc_run *run, const gc_mon *m, bool *fusion);
void gc_mon_evolve(gc_run *run, gc_mon *m, int evo);
void gc_mon_evolve_target(gc_run *run, gc_mon *m, int evo, bool fusion);
void gc_mon_recalc(gc_run *run, gc_mon *m);                            /* calculateStats on a living Pokemon */
bool gc_mon_fainted(const gc_mon *m);
bool gc_mon_allowed(const gc_mon *m);                                  /* isAllowedInBattle */

/* ---- learnsets (src/gc_learnset.c): field/learnsets.ts getLevelMoves, as the reward screen reads it ---- */
/* One row of PlayerPokemon.getLevelMoves: its level (-1 RELEARN_MOVE, 0 EVOLVE_MOVE), the move and its
 * LearnableMoveSource (PR_LEARN_SOURCE_*) */
typedef struct { int16_t level; uint16_t move; int8_t source; } gc_learn_row;
/* Pokemon.getLevelMoves({startingLevel, includeEvolutionMoves, includePrevolutionMoves, includeRelearnerMoves}):
 * the rows, or -1 when they don't fit in `cap` */
int gc_level_moves(const gc_mon *m, int starting_level, bool evo, bool prevo, bool relearn, gc_learn_row *out, int cap);
/* the level-0 (EVOLVE_MOVE) moves postEvolve queues for the mon's current species and form, one LearnMovePhase each */
int gc_evolution_moves(const gc_mon *m, uint16_t *out, int max);
/* PlayerPokemon.getUnlockedEggMoves(): how many (out holds 4) */
int gc_unlocked_egg_moves(const gc_run *run, const gc_mon *m, uint16_t out[4]);
int gc_egg_moves(const gc_mon *m, uint16_t out[4]);
int gc_mystery_primary_pokemon(const gc_run *run, int type, int option);   /* the option's primaryPokemon (storage index) or -1 */
int gc_root_species(int species);   /* PokemonSpeciesForm.getRootSpeciesId(): the first stage of the species' evolution line */
void gc_party_remove(gc_run *run, int idx);   /* removePokemonFromPlayerParty: storage index; the storage behind it moves down */
int gc_offer_terms(const gc_run *run, double *price);   /* An Offer You Can't Refuse: the strongest Pokemon (storage index) and what it is worth */
bool gc_salesman_offer(gc_run *run, gc_mon *mon, double *price);   /* The Pokemon Salesman: what he sells (an enemy-shaped level 5 Pokemon, no moveset) and its price; false = a species not ported */
double gc_part_timer_multiplier(const gc_mon *m, int option);   /* Part Timer's money multiplier for a Pokemon: option 0 by Speed, 1 by HP/Attack/Defense, 2 fixed */
bool gc_can_learn_move(const gc_mon *m, int move);   /* CanLearnMoveRequirement: level (now), TM or egg move */
void gc_exp_party_build(gc_run *run, pr_exp_party *pp, const uint32_t *participants, int n_participants);
void gc_exp_party_queue(gc_run *run, const pr_exp_party *pp, double exp_value, bool use_wave_index_multiplier);

/* ---- rewards (src/gc_reward.c): the adapter between game-core's run and reward-core ---- */
/* getPartyLuckValue(party) */
int gc_party_luck(gc_run *run);
/* A pr_reward_ctx made from the run as it is now (a static: valid until the next call). Its party is in
 * upstream's party order (run->party_order), so ctx party index k is run->party[run->party_order[k]]. */
pr_reward_ctx *gc_reward_ctx(gc_run *run);
/* the same without the learnsets: what the application of a reward reads (the party, the items, the unlocks, the balls, the arena) */
pr_reward_ctx *gc_reward_ctx_base(gc_run *run);
/* The types an attack move counts as for the type-boosting items: VariableMoveTypeAttr.getTypesForItemSpawn
 * or Pokemon.getMoveTypeForItemSpawn. Returns how many (out holds 2). */
int gc_types_for_item_spawn(gc_run *run, const gc_mon *m, int move, int8_t out[2]);
/* SelectModifierPhase.copy(): queue the screen again, with the options it showed (what a TM or the Memory
 * Mushroom does when it is taken) */
void gc_reward_queue_copy(gc_run *run);
/* SelectModifierPhase.getRerollCost(lockModifierTiers) for the screen that is open */
double gc_reward_reroll_cost(gc_run *run);
/* the reward-core item (enum pr_item) of a modifierTypes key, -1 = none */
int gc_reward_item_index(const char *key);
/* what a reward is used on (GC_TARGET_*): the type's own PokemonModifierType / PokemonMoveModifierType class */
uint8_t gc_reward_target(const char *key);
/* Task 60: the application of healing, revive, PP restore and PP Up rewards (Potions, Full Restore, Revives,
 * Sacred Ash, Ether, Elixir, PP Up, PP Max) by reward-core's pr_reward_apply, on a context made from the run,
 * its result written back. `party` is an index into run->party[], `move_slot` a moveset slot. GC_APPLY_REFUSED
 * (and *why) = it can't be used there; GC_APPLY_UNHANDLED = not one of these items, the
 * caller's own code applies it. A free reward (offer cost 0) is taken even when it did nothing and has no target
 * (Sacred Ash with nobody fainted), as SelectModifierPhase.applyModifier does with cost -1.
 * Vitamin exception: an applied purchase returns REFUSED with an empty reason,
 * preserving upstream's persistent-add error/no-payment result after mutation. */
enum { GC_APPLY_UNHANDLED, GC_APPLY_DONE, GC_APPLY_REFUSED };
int gc_reward_apply(gc_run *run, const gc_offer *o, int party, int move_slot, const char **why);
int gc_reward_lifecycle_apply(gc_run *run, const gc_offer *o, int party, const char **why);
/* DNA Splicers (FusePokemonModifierType) taken from the reward screen: party[primary] gets party[donor] as its fusion half and the donor
 * leaves the party (src/gc_fusion.c). Both are party indices (run->party[]); GC_APPLY_DONE / GC_APPLY_REFUSED with *why */
int gc_fusion_scene_apply(gc_run *run, const gc_offer *o, int primary, int donor, const char **why);
/* Task 96: Poke Ball rewards. add_pokeballs = AddPokeballModifier.apply (+5, Master Ball +1, at most 99) for a reward
 * key, false when the key is not a Poke Ball; gc_reward_gain = ModifierRewardPhase.doReward: give the reward, then
 * queue battle:rewardGain naming it. */
bool gc_reward_add_pokeballs(gc_run *run, const char *key);
void gc_reward_gain(gc_run *run, const char *key);
void gc_reward_change_form(gc_run *run, int party, int form);

/* ---- items (src/gc_items.c) ---- */
int gc_item_stack(const gc_run *run, const char *key);                 /* stack count of a player item, 0 = none */
bool gc_item_key_form_change(const char *key);
bool gc_form_items_context_known(const gc_run *run, const pr_pokemon *pokemon, int candidate);
bool gc_item_key_locked(const char *key); /* intrinsic pinned constructor property */
void gc_item_add(gc_run *run, const char *key, int16_t arg, uint32_t pokemon_id, int stack);
void gc_item_remove_at(gc_run *run, int idx);
/* The enemy's held items (the modifiers with a Pokemon) and the player's, as catching moves them. Each rebuilds
 * battle-core's modifier lists (gc_items_to_battle) from the lists as they stand. */
void gc_items_take_enemy_held(gc_run *run);      /* addToParty: findModifiers(held, false) -> addModifier: onto the player's list */
void gc_items_drop_enemy_held(gc_run *run);      /* clearEnemyHeldItemModifiers() */
void gc_items_drop_held_of(gc_run *run, uint32_t pokemon_id);   /* removePartyMemberModifiers: a released member's */
void gc_items_fuse_transfer(gc_run *run, uint32_t from_id, int to);   /* PlayerPokemon.fuse: all of `from_id`'s held items onto party[to] */
int gc_item_remove_stacks(gc_run *run, const char *key, int count);
void gc_items_from_battle(gc_run *run); /* persist consumed berries/White Herb */
void gc_items_to_battle(gc_run *run);                                  /* the player's and the enemy's lists into bt->mods */
void gc_cache_generated_item_tiers(gc_run *run);
bool gc_item_stock_ported(const char *key, int arg, int stack);   /* the battle layer knows this held item (gc_custom_team_check) */
double gc_apply_exp_boosters(const gc_run *run, double exp);          /* ExpBoosterModifier */

/* ---- the encounter and the battle (src/gc_encounter.c, src/gc_battle.c) ---- */
void gc_new_battle(gc_run *run);                  /* NewBattlePhase + EncounterPhase */
void gc_new_battle_start(gc_run *run, bool reset, bool new_biome);   /* the part of EncounterPhase from the save point on (after a custom offer) */
/* custom hooks (src/gc_custom.c) */
void gc_custom_install_observer(gc_run *run);                        /* puts the faint observer into the battle when hooks want it */
bool gc_custom_report(gc_run *run, const gc_custom_encounter *enc);  /* encounter(): true = the player is asked */
void gc_custom_answer(gc_run *run, bool accept);                     /* the player's answer: builds the enemy for an accept, then the battle starts */
bool gc_custom_build_enemy(gc_run *run, const gc_custom_enemy *spec, const char **why);   /* the ordinary enemy replaced by spec's identity */
bool gc_custom_build_trainer(gc_run *run, const gc_custom_team *team);                    /* the ordinary trainer party replaced by the team (unfinished: gc_enemy_finish_party follows) */
bool gc_custom_trainer_wave(gc_run *run, int wave);                                      /* the wave just generated may become a team's */
bool gc_custom_trainer(gc_run *run, const gc_custom_encounter *enc);                      /* trainer(): an eligible wave asks the app for a team and builds it */
void gc_custom_team_items(gc_run *run);                                                   /* the team's items instead of the generated ones (enemy_items) */
void gc_enemy_finish_party(gc_run *run);                             /* passives, stats, seen flags, items, AI, trainer names of the enemy party */
void gc_enemy_finish_party_me(gc_run *run);                          /* the same for an initBattleWithEnemyConfig without item configs: the item rolls, then updateModifiers' stats */
void gc_enemy_ctx(gc_run *run, pr_enemy_gen_ctx *ec);                /* the scene addEnemyPokemon sees (run-core's context) */
void gc_refresh_enemy(gc_run *run, gc_mon *m, int species, int level, const pr_enemy_gen *g, int slot);   /* gc_mon_new_enemy again, keeping the moveset */
/* Task 45: getMysteryEncounter() inside executeWithSeedOffset(wave * 16). */
bool gc_mystery_select(gc_run *run, int *type, int *tier);
void gc_mystery_record(gc_run *run, int type, int tier);
void gc_mystery_increment_miss(gc_run *run);
void gc_mystery_setup_screen(gc_run *run);
bool gc_mystery_execute(gc_run *run, int option, int b, int c, const char **why, bool *ok);
bool gc_mystery_pick_ok(const gc_run *run, int type, int option, int idx);   /* the option's party screen filter accepts the Pokemon */
void gc_mystery_post(gc_run *run, const gc_rphase *p);
bool gc_gen_wild(gc_run *run, int level);
bool gc_trainer_is_double(const gc_run *run);
void gc_trainer_names(gc_run *run);               /* run->trainer_names for the current trainer */
void gc_ev_trainer_dialogue(gc_run *run, int kind);   /* GC_EV_DIALOGUE for the current trainer (0 before the fight, 1 after it is won) */
bool gc_is_new_biome(const gc_run *run, int wave);   /* isNewBiome for the battle of `wave` */
int gc_roll_biome_weather(gc_run *run);            /* Arena.setBiomeWeather's pick (consumes the run RNG) */
int gc_roll_biome_terrain(gc_run *run);            /* Arena.setBiomeTerrain's pick */
void gc_arena_reset_effects(gc_run *run);          /* Arena.resetArenaEffects: weather and terrain */
void gc_arena_new_battle(gc_run *run, bool reset, bool roll);   /* ...then the biome's rolls */
void gc_battle_note_unported(gc_run *run); /* drain battle diagnostics into persistent counters */
bool gc_set_encounter_nature(pr_battle *bt, pr_pokemon *p, int nature, void *data);
void gc_battle_cleanup_players(gc_run *run);   /* outgoing field, before generating the encounter */
bool gc_battle_setup_field(gc_run *run, bool reset_arena);   /* bt from the parties, before the first turn */
void gc_battle_restore_field(gc_run *run); /* pointers only; summon effects already saved */
void gc_ai_reset(gc_run *run);                    /* ai-core's state for a new battle */
void gc_ai_enemy_party(gc_run *run);              /* ...and the enemy party's entries once it is made */
void gc_battle_turn_init(gc_run *run);            /* TurnInitPhase's part: participants, resetTurnData */
/* The player's command for field slot 0: fills bt->commands[0]; false (and why) if not allowed */
bool gc_battle_player_command(gc_run *run, const gc_choice *c, const char **why);
void gc_battle_command_screen(gc_run *run);       /* fills run->screen for COMMAND */
/* battle-core's turn from TurnStartPhase (or the pick after a SwitchPhase, pick >= -1), to its next
 * stop: returns the stop's phase type */
int gc_battle_run(gc_run *run, int pick);
/* After a stop at PH_VICTORY and the experience: the rest of the turn (the other moves, the end-of-turn
 * effects, the trainer's next Pokemon) to the next stop (pr_continue) */
int gc_battle_continue(gc_run *run);
/* After the wild battle's start stopped at PH_CHECK_SWITCH: the answer, run to the next stop */
int gc_battle_answer_switch(gc_run *run, bool yes);
void gc_battle_switch_screen(gc_run *run);        /* fills run->screen for SWITCH */
void gc_pick_up_scattered_money(gc_run *run);     /* pickUpScatteredMoney (src/battle.ts:204) */
void gc_battle_end_cleanup(gc_run *run);          /* after a battle: the party off the field */
int gc_enemy_active_index(const gc_run *run);     /* the enemy party index on the field, -1 = none */
int gc_player_active_index(const gc_run *run);
bool gc_battle_queued_command(gc_run *run);       /* a move locked in by the last turn: no COMMAND screen */
int gc_battle_slot_of(const gc_run *run, int party_index);   /* upstream party slot of party[i] */
void gc_sync_party_order(gc_run *run);            /* party_order from battle-core's party list */

/* ---- catching (src/gc_catch.c) ---- */
/* CommandPhase.checkCanUseBall + handleBallCommand's boss rule: NULL = this ball may be thrown now, else the locale
 * key of the text upstream shows instead */
const char *gc_catch_ball_refusal(const gc_run *run, int ball);
bool gc_catch_is_catch(const gc_run *run, int enemy_idx);   /* at a stop for a VictoryPhase: the enemy was caught, not fainted */
bool gc_catch_start(gc_run *run, gc_rphase *p);
bool gc_catch_choose(gc_run *run, gc_rphase *p, const gc_choice *c, const char **why, bool *ok);

/* ---- Fun and Games! (src/gc_fungames.c) ---- */
bool gc_fg_active(const gc_run *run);
bool gc_fg_pick_ok(const gc_run *run, int idx);                 /* the party screen's filter: allowed in battle */
void gc_fg_start(gc_run *run, int pick);                        /* the "Play the Game" option, with the party screen's pick (a storage index) */
bool gc_fg_turn_init(gc_run *run);                              /* handleNextTurn from TurnInitPhase: true = skip the rest of the phase */
void gc_fg_lose(gc_run *run);                                   /* handleLoseMinigame, in MysteryEncounterRewardsPhase (RP_FG_KO) */
void gc_fg_battle_end(gc_run *run, bool victory);               /* handleMysteryEncounterVictory's phases, after the minigame's battle ended */
bool gc_fg_wobbuffet_moveset(gc_run *run, gc_mon *m);           /* generateMoveset for a wild Wobbuffet, on the run's RNG */
/* mystery-encounter exits for gc_fungames.c (src/gc_mystery.c) */
void gc_enemy_finish_party_items(gc_run *run, const gc_item *items, int n);   /* gc_enemy_finish_party with the first Pokemon's items given (no item rolls) */
void gc_me_begin_battle(gc_run *run, int type);                /* the encounter's battle starts: the field, the first turn (an EnemyPartyConfig's enemy party is in place) */
void gc_dark_deal_execute(gc_run *run, int option);
bool gc_me_new_boss(gc_run *run, pr_rng *r, int species, int level, bool second_iv, gc_mon *out);   /* new EnemyPokemon(.., boss) on a seed-offset stream */
void gc_fof_execute(gc_run *run, int option);                    /* Fight or Flight (src/gc_fof.c) */
bool gc_fof_init(gc_run *run, gc_mon *boss, int *level, pr_reward_option *item);   /* onInit, worked out from the run (the oracle's test reads it) */
bool gc_fof_tokens(gc_run *run, pr_msg_arg *enemy_name, pr_msg_arg *item_name);   /* the tokens onInit sets: enemyPokemon, itemName */
void gc_me_win_rewards(gc_run *run);                            /* an encounter's fight is won with setEncounterRewards in place: the shop it set, then the next wave */
void gc_snorlax_execute(gc_run *run, int option);                /* Slumbering Snorlax (src/gc_snorlax.c) */
void gc_snorlax_tokens(gc_run *run, pr_msg_arg *name);
void gc_me_add_start_effect(gc_run *run, int source, int target, int move);   /* startOfBattleEffects.push({sourceBattlerIndex, targets: [target], move, useMode: IGNORE_PP}) */
int gc_battle_start_effects(gc_run *run);                       /* handleMysteryEncounterBattleStartEffects: the engine runs them to its next stop */
void gc_me_leave(gc_run *run);                                   /* leaveEncounterWithoutBattle(): no heal shop */
bool gc_me_init_boss(gc_run *run, gc_mon *boss, int *level, pr_rng *after);   /* onInit's boss (Fight or Flight, Berries Abound), src/gc_fof.c */
void gc_me_start_boss_battle(gc_run *run, const gc_mon *boss, int level, int post_summon, pr_rng *stream, int type);
void gc_berries_execute(gc_run *run, int option);                  /* Berries Abound (src/gc_berries.c) */
void gc_berries_reward(gc_run *run);
bool gc_berries_tokens(gc_run *run, pr_msg_arg *enemy_name, pr_msg_arg *fastest_name, int *grab);
bool gc_berries_race_lost(gc_run *run);
int gc_berries_fastest(const gc_run *run);                          /* getHighestStatPlayerPokemon(SPD, true, false): a storage index */
void gc_training_execute(gc_run *run, int option, int member, int choice);
void gc_training_return(gc_run *run);
bool gc_training_pick_ok(const gc_run *run, int idx);
bool gc_training_nature_new(const gc_run *run, int member, int nature);
int gc_training_abilities(const gc_run *run, int member, int slots[3], bool is_new[3]);
const char *gc_nature_key(int nature);
void gc_me_steal_exp(gc_run *run, int primary, int species);    /* setEncounterExp(primary.id, species.baseExp) */                               /* the fight is won: the item's shop, then the next wave */
bool gc_fof_preview(gc_run *run, int *species, int *form, bool *shiny, int *variant, const char **item_key);   /* what onInit made, for the question screen; false when it could not */
void gc_me_leave_with_heal_shop(gc_run *run);                   /* leaveEncounterWithoutBattle(true) */
void gc_me_leave_with_shop(gc_run *run, const char *key);       /* setEncounterRewards({guaranteedModifierTypeFuncs: [key], fillRemaining: false}), leaveEncounterWithoutBattle() */

/* ---- Global Trade System (src/gc_gts.c) ---- */
typedef struct { const char *key; bool gendered; struct { int n; const char *parts; } list[3]; } gc_trader_class;
extern const gc_trader_class gc_trader_classes[];
extern const int gc_trader_class_count;
bool gc_gts_trade_options(gc_run *run, int party_idx, gc_mon out[3]);    /* the 3 offers of a party member (generateTradeOption x3, then the 3 EnemyPokemon) */
bool gc_gts_item_transferable(const gc_item *it);
int gc_gts_held_items(const gc_run *run, int idx, int out[GC_MAX_ITEMS]);
bool gc_gts_pick_ok(const gc_run *run, int option, int idx);
int gc_gts_allowed(const gc_run *run);
bool gc_gts_execute(gc_run *run, int option, int b, int c, const char **why);
/* the Pillar Men (src/gc_pillar.c, custom runs) */
bool gc_pillar_offered(const gc_run *run);                  /* the hook says the encounter is on offer */
void gc_pillar_execute(gc_run *run, int option);            /* the option chosen: the fight starts (or the encounter leaves with the heal shop) */
void gc_pillar_victory(gc_run *run);                        /* the fight is won: the encounter's rewards, then the next wave */
void gc_me_end_with_shop(gc_run *run);                      /* the shop in me_custom, then on to the next wave (end_mystery_encounter_shop) */
const pr_trainer_info *gc_trainer_info_of(int type);
const pr_party_template *gc_party_template_of(const gc_run *run, int trainer_type, int index, int prev_wave);
bool gc_gen_trainer_party(gc_run *run, const pr_party_template *tpl, int trainer_type, int variant, const int *levels, int n_levels);
/* mystery-encounter internals for gc_gts.c (src/gc_mystery.c) */
pr_rng gc_me_option_rng(const gc_run *run);
void gc_me_prune_items(gc_run *run);
void gc_me_update_modifiers(gc_run *run);
void gc_me_end_shop(gc_run *run);

/* run-phase handlers (src/gc_phases.c): return true when the phase is done, false when it waits on a screen */
bool gc_phase_start(gc_run *run, gc_rphase *p);
bool gc_phase_choose(gc_run *run, gc_rphase *p, const gc_choice *c, const char **why, bool *ok);

uint32_t gc_stub_rand(gc_run *run, uint32_t n);

/* GlobalScene wallet transport, separate from currentBattle Pay Day money. */
void gc_wallet_to_battle(gc_run *run);
void gc_wallet_from_battle(gc_run *run);

int gc_temp_add(gc_run *run, int item, int arg);
int gc_temp_apply(gc_run *run, const gc_offer *o, const char **why);
bool gc_temp_lapse(gc_run *run, int *expired);

bool gc_classic_finish(gc_run *run,bool victory,const char **why);

int gc_multi_add(gc_run *run,int storage);
int gc_multi_scene_apply(gc_run *run,const gc_offer *o,int storage,const char **why,bool selection);
int gc_wide_add(gc_run *run,int storage);
int gc_wide_scene_apply(gc_run *run,const gc_offer *o,int storage,const char **why,bool selection);

#endif

int gc_orb_add(gc_run *run,int storage,int item);
int gc_orb_scene_apply(gc_run *run,const gc_offer *o,int storage,const char **why,bool selection);

int gc_lure_add(gc_run *run,int item);
bool gc_lure_lapse(gc_run *run,int *expired);
bool gc_lure_count(gc_run *run,int *count);
int gc_lure_apply(gc_run *run,const gc_offer *o,const char **why);
int gc_soothe_add(gc_run *run,int storage);
bool gc_soothe_battle_gain(gc_run *run,int storage);
int gc_soothe_scene_apply(gc_run *run,const gc_offer *o,int storage,const char **why,bool selection);
