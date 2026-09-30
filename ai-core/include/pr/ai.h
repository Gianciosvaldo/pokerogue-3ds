/* ai-core: what an enemy Pokemon does each turn, ported from upstream PokeRogue @ 8555c08:
 * EnemyCommandPhase (src/phases/enemy-command-phase.ts), EnemyPokemon.getNextMove and
 * getNextTargets (src/field/pokemon.ts), the moves' benefit scores (getUserBenefitScore /
 * getTargetBenefitScore in src/data/moves/move.ts), Pokemon.getMatchupScore and the trainer's
 * switch choice (src/field/trainer.ts). Function names follow upstream so each C function can be
 * diffed against the TS it replaces.
 *
 * It reads battle-core's state (pr_battle, pr_pokemon) and never changes battle-core. The game
 * calls pr_ai_enemy_command once per enemy on the field after the player's command and before
 * pr_run_turn, as upstream runs EnemyCommandPhase after CommandPhase. */
#ifndef PR_AI_H
#define PR_AI_H
#include "pr/battle.h"

/* What the AI reads about a Pokemon that pr_pokemon doesn't hold (yet). One entry per Pokemon in
 * either party, found by Pokemon.id; a Pokemon with no entry reads as the defaults below. */
typedef struct pr_ai_mon {
    uint32_t id;                   /* Pokemon.id */
    uint8_t ai_type;               /* EnemyPokemon.aiType (AI_TYPE_*): SMART for a boss or a trainer's
                                    * Pokemon, SMART_RANDOM otherwise; default pr_ai_default_type(is_boss, has_trainer) */
    bool ability_revealed;         /* waveData.abilityRevealed: its ability has been shown this wave; default false */
    int8_t initial_team_index;     /* EnemyPokemon.initialTeamIndex (its place when the party was made); default -1 */
    uint8_t trainer_slot;          /* EnemyPokemon.trainerSlot (TRAINER_SLOT_*); default NONE */
} pr_ai_mon;

/* TrainerAI.teraMode (src/enums/tera-ai-mode.ts) */
enum { PR_TERA_AI_NO_TERA, PR_TERA_AI_INSTANT_TERA, PR_TERA_AI_SMART_TERA };

/* currentBattle.trainer, as far as the AI reads it */
typedef struct pr_ai_trainer {
    bool is_boss;                  /* trainer.config.isBoss (gym leaders, Elite Four, champions, evil team
                                    * bosses, the last rivals): switches at 2x the matchup instead of 3x */
    bool is_double;                /* trainer.isDouble() */
    uint8_t tera_mode;             /* config.trainerAI.teraMode (PR_TERA_AI_*) */
    int8_t instant_teras[6];       /* config.trainerAI.instantTeras: initialTeamIndex values whose condition
                                    * holds (the caller evaluates the conditions) */
    uint8_t n_instant_teras;
} pr_ai_trainer;

/* Unported cases the AI reports in bt->unported, after battle-core's own ids */
enum {
    PR_AI_FEAT_ILLUSION = PR_FEAT_COUNT_,   /* a read with useIllusion of a Pokemon that has Illusion */
    PR_AI_FEAT_ATTR_SCORE,                  /* a move attribute or condition whose benefit score has no port */
    PR_AI_FEAT_RESTRICTION,                 /* a move restriction with no port (or Stuff Cheeks beside held items) */
    PR_AI_FEAT_TABLES,                      /* gen/out/ai_data.c doesn't match battle-core's move tables */
    PR_AI_FEAT_COUNT_
};
const char *pr_ai_unported_name(uint16_t id);   /* battle-core's names, and these */
/* Whether gen/out/ai_data.c was dumped from the same upstream moves as battle-core's tables (every
 * move's attribute count and classes agree). The scores report PR_AI_FEAT_TABLES while it doesn't. */
bool pr_ai_tables_match(void);

/* The session-wide state upstream keeps on move attribute objects that the AI's scores read.
 * Upstream changes them whenever any Pokemon uses the move, and the change lasts until the next
 * use (across battles too). battle-core doesn't keep them yet: until it does, the caller may set
 * them, and they read as upstream's initial values otherwise. */
void pr_ai_session_reset(void);
/* MultiStatusEffectAttr.effect (Tri Attack, Dire Claw): the status its last use rolled */
void pr_ai_session_set_multi_status(int move, int effect);
int pr_ai_session_multi_status(int move);
/* VariableHealAttr.healRatio (Moonlight, Morning Sun, Synthesis, Shore Up, Swallow, Floral Healing...):
 * the ratio of its last use; 1 before */
void pr_ai_session_set_heal_ratio(int move, double ratio);
double pr_ai_session_heal_ratio(int move);

typedef struct pr_ai {
    const pr_ai_trainer *trainer;  /* NULL in a wild battle */
    pr_seedstr seed;               /* globalScene.seed (the session seed): the tie roll between equally good
                                    * switch-ins; len 0 = unknown (a tie then reports PR_FEAT_WAVE) */
    int enemy_switch_counter;      /* currentBattle.enemySwitchCounter: 0 at the start of a battle; each
                                    * pr_ai_enemy_command raises it on a switch and lowers it otherwise */
    bool skip_enemy_turns;         /* currentBattle.mysteryEncounter?.skipEnemyBattleTurns */
    pr_ai_mon mons[2 * PR_MAX_PARTY];
    uint8_t n_mons;
    /* out: the move scores of the last SMART or SMART_RANDOM choice, one per move in the pool (what
     * upstream logs as "Move Scores:"); 0 entries when the last choice didn't score moves */
    double move_scores[4];
    uint8_t n_move_scores;
} pr_ai;

/* The entry for p (adding a default one if there is room); NULL when the table is full */
pr_ai_mon *pr_ai_mon_of(pr_ai *ai, const pr_pokemon *p);
/* EnemyPokemon's constructor: boss || hasTrainer() ? SMART : SMART_RANDOM (a Pokemon without an
 * entry reads as this, from its is_boss and has_trainer) */
int pr_ai_default_type(bool boss, bool has_trainer);

/* EnemyCommandPhase(fieldIndex).start() for the enemy at bt->field[BATTLER_INDEX_ENEMY + field_index]:
 * a trainer's Pokemon may switch (bt->commands[...] = POKEMON with the party slot in cursor), otherwise
 * it picks a move (FIGHT with the move, its targets and use mode), and a trainer may terastallize it
 * (bt->pre_commands[...] = TERA). Uses the battle RNG (bt->battle_rng) exactly where upstream does.
 * A few move conditions the scores call read the phase queue (Protect and co. fail unless a
 * MovePhase is queued, which it never is at command time) or add to it as upstream does then (Damp
 * showing itself against Explosion, an immunity message): set bt->engine to the game's engine
 * between turns; with bt->engine NULL those conditions report PR_FEAT_MOVE_CONDITION. */
void pr_ai_enemy_command(pr_battle *bt, pr_ai *ai, int field_index);

/* Trainer.getNextSummonIndex(trainerSlot): the enemy party slot the trainer sends in when a Pokemon
 * of trainer_slot (TRAINER_SLOT_*) faints or is forced out (SwitchSummonPhase with slot -1): the best
 * matchup against the player's field, a tie settled by a roll on ai->seed. -1 when no Pokemon on the
 * bench can come in (upstream would throw). battle-core stops at PH_SWITCH for the enemy side; the
 * caller passes this to pr_resume_switch. */
int pr_ai_next_summon_index(pr_battle *bt, pr_ai *ai, int trainer_slot);

/* EnemyPokemon.getNextMove(): the move, its targets and use mode. Like upstream it drops unusable
 * moves from the front of p's move queue, or clears the queue. */
pr_turn_move pr_ai_get_next_move(pr_battle *bt, pr_ai *ai, pr_pokemon *p);
/* EnemyPokemon.getNextTargets(move): the battler indexes (ATTACKER = -1 for a counter move with
 * no target); returns how many */
int pr_ai_get_next_targets(pr_battle *bt, pr_ai *ai, pr_pokemon *p, int move, int8_t out[4]);

/* Move.getUserBenefitScore / getTargetBenefitScore (AttackMove's included) for user using move on target */
double pr_ai_user_benefit_score(pr_battle *bt, pr_ai *ai, pr_pokemon *user, pr_pokemon *target, int move);
double pr_ai_target_benefit_score(pr_battle *bt, pr_ai *ai, pr_pokemon *user, pr_pokemon *target, int move);

/* Pokemon.getMatchupScore(opponent): how well p fares against opponent (near 0 to 16, at most 64) */
double pr_ai_matchup_score(pr_battle *bt, pr_ai *ai, pr_pokemon *p, pr_pokemon *opponent);

/* PokemonMove.isUsable(pokemon, ignorePp, forSelection) for p's moveset slot: not NONE or
 * unimplemented, PP left unless ignored, and for selection no restricting battler tag (Disable,
 * Taunt, Torment, Encore, Throat Chop, Heal Block, Gorilla Tactics) or move restriction (Gravity,
 * Belch, Stuff Cheeks, Gigaton Hammer). The player's fight menu can use it too. */
bool pr_ai_move_usable(pr_battle *bt, const pr_pokemon *p, int slot, bool ignore_pp, bool for_selection);
/* Pokemon.isMoveSelectable(move): the restricting tags and the move's own restrictions */
bool pr_ai_move_selectable(pr_battle *bt, const pr_pokemon *p, int move);
/* Pokemon.isTrapped(): can't switch out (Arena Trap, Shadow Tag, Magnet Pull, a trapping tag, Fairy Lock) */
bool pr_ai_is_trapped(pr_battle *bt, pr_ai *ai, pr_pokemon *p);

#endif
