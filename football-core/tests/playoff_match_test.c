#include "football_playoff_match.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static gc_run roster;
static FootballSeason season,other;
static FootballPlayoffMatch session;
int main(void)
{
    bool saw_win=false,saw_loss=false;
    for (unsigned seed=1;seed<=100;++seed) {
        memset(&session,0,sizeof session);
        assert(football_season_new(&season,&roster,NULL,"match",seed));
        gc_starter starter={.species=SPECIES_ID_BULBASAUR,.tera_type=-1};
        assert(!football_playoff_match_begin(&session,&season));
        assert(football_season_draft(&season,&starter,1,NULL));
        assert(football_playoff_match_begin(&session,&season));
        assert(session.active && season.phase==FB_SEASON_MATCH);
        assert(!football_playoff_match_begin(&session,&season));
        /* Drive both real score outcomes without caller-supplied win/loss flags. */
        for (unsigned side=0;side<2;++side) for (unsigned i=0;i<session.match.teams[side].count;++i) {
            uint8_t rating=(uint8_t)(((side==FB_HOME)==(seed%2==0)) ? 99 : 1);
            session.match.teams[side].players[i].attributes=(FootballAttributes){rating,rating,rating,rating,rating,rating,rating,rating};
        }
        other.phase=FB_SEASON_MATCH;other.round=season.round;
        assert(!football_playoff_match_tick(&session,&other,250,NULL));
        unsigned guard=0;
        while (session.active && ++guard<10000) {
            if (football_match_waiting_for_home(&session.match)) {
                PlayInfo p={FB_FORM_UNDER_CENTER,FB_PLAY_RUN,FB_RUN_DIVE,255};
                assert(football_match_call(&session.match,p));
            }
            assert(football_playoff_match_tick(&session,&season,250,NULL));
        }
        assert(guard<10000 && session.reported);
        bool won=session.match.rules.winner==FB_HOME;
        assert(season.phase==(won ? FB_SEASON_EXP : FB_SEASON_ELIMINATED));
        assert(season.wins==(won ? 1u : 0u));
        assert(season.wallet==(won ? 500u : 0u));
        uint32_t wallet=season.wallet;unsigned wins=season.wins;
        assert(!football_playoff_match_tick(&session,&season,250,NULL));
        assert(wallet==season.wallet && wins==season.wins);
        saw_win|=won;saw_loss|=!won;
    }
    assert(saw_win && saw_loss);
    puts("football playoff bridge: 100 real match results, correct EXP/elimination and exactly-once reporting passed");
}
