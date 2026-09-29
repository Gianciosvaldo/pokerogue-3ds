/* ai-core internals shared by its source files */
#ifndef PR_AI_INTERNAL_H
#define PR_AI_INTERNAL_H
#include "pr/ai.h"
#include "pr/ai_data.h"
#include "pr/engine.h"

/* p's pr_ai_mon values, or the defaults when it has no entry (never adds one) */
bool ai_ability_revealed(const pr_ai *ai, const pr_pokemon *p);
int ai_type_of(const pr_ai *ai, const pr_pokemon *p);

/* getMaxHp(), isActive(onField), isOnField(), getHpRatio() */
double ai_max_hp(const pr_pokemon *p);
bool ai_is_on_field(const pr_pokemon *p);
bool ai_is_active(const pr_pokemon *p, bool on_field);
/* PokemonMove.getMovePp() / getPpRatio() for p's moveset slot */
double ai_move_pp(const pr_pokemon *p, int slot);
double ai_pp_ratio(const pr_pokemon *p, int slot);
/* A read with useIllusion: Illusion isn't modelled, so it is reported when p could have one */
void ai_note_illusion(pr_battle *bt, const pr_pokemon *p);
/* randBattleSeedInt(range) with a range that may not be a whole number (getNextTargets' total weight) */
double ai_battle_rand_real(pr_battle *bt, double range);

/* arena.isMoveWeatherCancelled(user, move), arena.isMoveTerrainCancelled(user, targets, move) */
bool ai_move_weather_cancelled(pr_battle *bt, const pr_pokemon *user, int move);
bool ai_move_terrain_cancelled(pr_battle *bt, const pr_pokemon *user, const int8_t *targets, int n, int move);
/* MoveEffectAttr.getMoveChance(user, target, move, selfEffect, showAbility) for pr_attrs[attr_index] */
double ai_move_chance(pr_battle *bt, int attr_index, const pr_pokemon *user, const pr_pokemon *target, int move,
                      bool self_effect, bool show_ability);

/* src/ai_damage.c: getMoveEffectiveness(source, move, ignoreAbility, simulated = true, cancelled,
 * useIllusion) and getAttackDamage({..., ignoreAbility, ignoreAllyAbility, isCritical, simulated: true})
 * with the ignore flags battle-core's versions don't take (the source's abilities always count) */
double ai_move_effectiveness(pr_battle *bt, const pr_pokemon *def, const pr_pokemon *src, int move,
                             bool ignore_ability, bool use_illusion, bool *cancelled_out);
pr_damage_result ai_attack_damage(pr_battle *bt, pr_pokemon *def, pr_pokemon *src, int move, bool ignore_ability,
                                  bool ignore_ally_ability, bool is_critical);

#endif
