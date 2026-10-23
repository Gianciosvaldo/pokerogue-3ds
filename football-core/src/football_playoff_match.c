#include "football_playoff_match.h"
#include <string.h>
bool football_playoff_match_begin(FootballPlayoffMatch *session,FootballSeason *season)
{
    if (!session || session->active || !season || !season->roster || season->phase!=FB_SEASON_READY ||
        season->roster->n_party<1 || season->roster->n_party>FB_MATCH_TEAM_CAP) return false;
    FootballMatchSpec spec;
    FootballTeam home={0},away;
    home.count=(uint8_t)season->roster->n_party;
    memcpy(home.players,season->players,home.count*sizeof home.players[0]);
    if (!football_season_start_match(season,&spec)) return false;
    if (!football_match_opponent(&away,&season->maxima,spec.seed,spec.round) ||
        !football_match_begin(&session->match,&home,&away,spec.seed,spec.difficulty_q8)) {
        season->phase=FB_SEASON_ERROR; session->active=false; return false;
    }
    session->owner=season; session->round=spec.round;
    session->active=true; session->reported=false; return true;
}
bool football_playoff_match_tick(FootballPlayoffMatch *session,FootballSeason *season,
                                  uint32_t elapsed_ms,const char **why)
{
    if (!session || !season || session->owner!=season || session->round!=season->round ||
        !session->active || session->reported ||
        season->phase!=FB_SEASON_MATCH) return false;
    football_match_tick(&session->match,elapsed_ms);
    if (session->match.phase==FB_MATCH_ERROR) {
        season->phase=FB_SEASON_ERROR; session->active=false; return false;
    }
    if (session->match.phase!=FB_MATCH_FINISHED) return true;
    if (!football_season_finish_match(season,session->match.rules.winner==FB_HOME,why)) {
        session->active=false; return false;
    }
    session->reported=true; session->active=false; return true;
}
