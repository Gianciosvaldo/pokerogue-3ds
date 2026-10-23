#ifndef FOOTBALL_PASS_H
#define FOOTBALL_PASS_H
#include "football_stats.h"
#include "football_playbook.h"
#include "football_defense.h"
#include "football_rules.h"
typedef struct {
    FootballAttributes quarterback,receiver,coverer,rusher;
    FootballPlayId play;
    FootballDefenseId defense;
    int8_t protection;
} FootballPassInput;
typedef struct {
    bool attempted;
    uint8_t depth_yards,separation,pressure,completion_chance,sack_chance,interception_chance;
    uint16_t release_ms;
    FootballResolvedPlay resolved;
} FootballPassReport;
/* Ratings/protection estimates, not real-time distances or velocities.
 * Explicit independent 0..99 rolls, no allocation or owned RNG. */
bool football_pass_resolve(FootballPassReport *out,const FootballPassInput *input,
                            unsigned pressure_roll,unsigned catch_roll,unsigned gain_roll);
#endif
