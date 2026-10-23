#ifndef FOOTBALL_PLAYOFF_MATCH_H
#define FOOTBALL_PLAYOFF_MATCH_H
#include "football_match.h"
#include "playoff_season.h"
typedef struct {
    FootballMatch match;
    FootballSeason *owner;
    unsigned round;
    bool active, reported;
} FootballPlayoffMatch;
/* Zero-initialize the session before first use. Begin from READY, using the season's real roster and generated round opponent.
 * No caller-provided win/loss flag: the final score feeds EXP/elimination once. */
bool football_playoff_match_begin(FootballPlayoffMatch *session, FootballSeason *season);
bool football_playoff_match_tick(FootballPlayoffMatch *session, FootballSeason *season,
                                  uint32_t elapsed_ms, const char **why);
#endif
