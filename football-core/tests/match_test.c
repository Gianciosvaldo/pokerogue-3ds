#include "football_match.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static FootballTeam team(unsigned count,unsigned rating)
{
    FootballTeam t={0};t.count=(uint8_t)count;
    for (unsigned i=0;i<count;++i) t.players[i].attributes=(FootballAttributes){rating,rating,rating,rating,rating,rating,rating,rating};
    return t;
}
static PlayInfo call(unsigned play)
{
    return (PlayInfo){(FootballFormation)(play/2),(FootballPlayType)(play%2),(FootballPlayId)play,255};
}
static void invariant(const FootballMatch *m)
{
    assert(m->rules.offense<=1 && m->rules.down>=1 && m->rules.down<=4);
    assert(m->rules.ball>0 && m->rules.ball<100);
    assert(m->rules.first_down_at>m->rules.ball && m->rules.first_down_at<=100);
    assert(m->rules.quarter>=1 && m->rules.quarter<=5);
    assert(m->phase!=FB_MATCH_ERROR);
    for (unsigned s=0;s<2;++s) for (unsigned i=0;i<m->teams[s].count;++i) assert(m->energy[s][i]<=100);
}
static void game(FootballMatch *m,unsigned dt)
{
    unsigned guard=0;
    while (m->phase!=FB_MATCH_FINISHED && ++guard<20000) {
        if (football_match_waiting_for_home(m)) assert(football_match_call(m,call(m->plays%4)));
        football_match_tick(m,dt); invariant(m);
    }
    assert(m->phase==FB_MATCH_FINISHED && m->rules.score[0]!=m->rules.score[1]);
    assert(m->plays<512);
}
int main(void)
{
    FootballTeam home=team(6,60),away=team(6,60);
    FootballMatch m={0};FootballMatch saved=m;
    FootballTeam invalid=home;invalid.count=7;
    assert(!football_match_begin(&m,&invalid,&away,0,256));
    assert(memcmp(&m,&saved,sizeof m)==0);
    assert(!football_match_begin(&m,&home,&away,0,255));
    assert(football_match_begin(&m,&home,&away,0,256));
    saved=m;
    PlayInfo bad=call(0);bad.id=FB_PASS_SLANTS;
    assert(!football_match_call(&m,bad));
    assert(!football_match_tick(&m,0));
    assert(memcmp(&m,&saved,sizeof m)==0);
    assert(football_match_call(&m,call(1)));
    assert(m.carrier>0 && m.carrier<home.count); /* QB cannot throw to self on a multi-player team. */
    saved=m;assert(!football_match_call(&m,call(0)));assert(memcmp(&m,&saved,sizeof m)==0);
    assert(m.rules.down==1 && m.plays==0);
    football_match_tick(&m,UINT32_MAX);assert(m.animation_ms==250 && m.plays==0);
    football_match_tick(&m,250);football_match_tick(&m,250);assert(m.plays==1);
    for (unsigned count=1;count<=6;++count) for (unsigned seed=0;seed<120;++seed) {
        home=team(count,60);
        FootballMatch replay;
        assert(football_match_begin(&m,&home,&away,seed,256+(seed%7)*64));
        assert(football_match_begin(&replay,&home,&away,seed,256+(seed%7)*64));
        game(&m,250);game(&replay,100);
        assert(m.plays==replay.plays && m.rng==replay.rng);
        assert(m.rules.score[0]==replay.rules.score[0] && m.rules.score[1]==replay.rules.score[1]);
        assert(m.rules.winner==replay.rules.winner);
        assert(!football_match_call(&m,call(0)) && !football_match_tick(&m,250));
    }
    /* A sack animates the QB rather than the intended receiver. */
    bool saw_sack=false;
    home=team(6,60);
    for (unsigned seed=1;seed<200 && !saw_sack;++seed) {
        assert(football_match_begin(&m,&home,&away,seed,256));
        assert(football_match_call(&m,call(FB_PASS_SLANTS)));
        if (m.resolved.kind==FB_RESULT_SACK) {
            assert(m.carrier==football_match_quarterback(&m,FB_HOME));saw_sack=true;
        }
    }
    assert(saw_sack);
    FootballDexMaxima maxima;
    assert(football_stats_build_maxima(&maxima,pr_forms,PR_FORM_COUNT));
    for (unsigned round=1;round<=7;++round) {
        FootballTeam opponent,copy;
        assert(football_match_opponent(&opponent,&maxima,123,round));
        assert(football_match_opponent(&copy,&maxima,123,round));
        assert(opponent.count==6);
        for (unsigned i=0;i<6;++i) {
            assert(opponent.players[i].species_id==copy.players[i].species_id);
            assert(opponent.players[i].form_index==0);
            for (unsigned j=0;j<i;++j) assert(opponent.players[i].species_id!=opponent.players[j].species_id);
        }
    }
    puts("football match: 1,440 full games, frame-independent replay, short rosters, atomic guards and seven opponent pools passed");
}
