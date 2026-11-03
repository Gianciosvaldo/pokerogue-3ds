#include "football_live_systems.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* An interception keeps the play live: the defender who has the ball carries it for his team (rules.offense flips), and the dead ball
 * after the run-back gives his team the ball where he went down, a touchdown where he crossed the line, or hands it back on a fumble.
 * Whole games, once with the CPU on both sides and once with a scripted human, and every run-back is held to those rules. */
static FootballRoster roster;
static FootballLive m;
static unsigned checks;
#define CHECK(c) do{++checks;assert(c);}while(0)
static unsigned long returns,td_returns,fumbles,human_takes,human_chases,deep_picks,yards_total,live_frames;static long max_return=-100;
static int own_spot(unsigned side,float x){int s=(int)roundf(side==FB_HOME?x-10:110-x);return s<1?1:s>99?99:s;}
static void play(uint32_t seed,bool human)
{
    assert(football_live_begin(&m,&roster,seed,1));m.autoplay=human?0:1;
    bool turned=false;unsigned taker=0,thrower=0,snaps=0;uint16_t score_taker=0;float catch_x=0;unsigned guard=0;
    while(m.phase!=FB_LIVE_END){
        assert(++guard<600000);
        FootballLiveInput in={.receiver=-1};
        if(m.phase==FB_LIVE_CALL){
            if(football_live_cpu(&m,(FootballSide)m.rules.offense))assert(football_live_defend(&m,(FootballDefenseId)(m.snaps%FB_DEF_COUNT)));
            else if(m.conversion==1)assert(football_live_special(&m,FB_SPECIAL_EXTRA_POINT));
            else assert(football_live_call(&m,(FootballPlayId)(FB_PASS_SLANTS+m.snaps%3)));
        }
        const FootballPlayDefinition *p=football_playbook_get(m.play);
        bool human_offense=!football_live_cpu(&m,(FootballSide)m.rules.offense);
        if(m.phase==FB_LIVE_SET && human_offense)in.pressed=FB_LIVE_ACTION;
        if(m.phase==FB_LIVE_PLAY && human_offense && !m.turnover && p && p->type==FB_PLAY_PASS && !m.thrown && m.elapsed>1.2f)in.receiver=(int)(m.snaps%5);
        if(m.turnover && human)in.x=m.rules.offense==FB_HOME?1.f:-1.f; /* the human runs it back, or chases, upfield for the taker */
        unsigned before=m.snaps;bool was=m.turnover;
        football_live_tick(&m,17,in);
        if(m.turnover && !was){
            /* The moment of the interception: the ball is the defender's and the sides have swapped. */
            turned=true;taker=m.rules.offense;thrower=1-taker;snaps=before;catch_x=m.catch_x;score_taker=m.rules.score[taker];
            CHECK(m.phase==FB_LIVE_PLAY && m.caught && m.thrown && !m.flying && !m.conversion && !m.special);
            CHECK((m.carrier<11)==(taker==FB_HOME) && m.interceptor==m.carrier);
            CHECK(fabsf(m.actors[m.carrier].x-m.ball_x)<2.6f);
            CHECK(taker==FB_HOME?catch_x>10:catch_x<110); /* never from his own end zone: that is a touchback */
            CHECK(m.credit_count==0); /* credited with the dead ball */
            if(human){
                if(taker==FB_HOME){CHECK(m.controlled==m.carrier);++human_takes;}
                else{CHECK(m.controlled<11);++human_chases;}
            }else CHECK(m.controlled==FB_NO_ACTOR);
        }else if(m.turnover){
            ++live_frames;
            CHECK(m.rules.offense==taker && (m.carrier<11)==(taker==FB_HOME) && m.phase==FB_LIVE_PLAY && m.snaps==before);
        }
        if(turned && m.snaps!=before){
            /* The dead ball. */
            const FootballRules *r=&m.rules;float x=m.actors[m.carrier].x;
            CHECK(!m.turnover && m.snaps==snaps+1);
            if(m.event==FB_EVENT_TOUCHDOWN){
                CHECK(r->last.touchdown && r->score[taker]>=score_taker+6);
                CHECK(taker==FB_HOME?x>=109.5f:x<=10.5f);++td_returns;
                CHECK(m.return_yards>0 && m.credit_count>=2 && m.credits[m.credit_count-1].kind==FB_CREDIT_TOUCHDOWN);
            }else if(m.event==FB_EVENT_FUMBLE){
                CHECK(r->last.turnover && r->score[taker]==score_taker);++fumbles;
                if(!r->finished && !r->last.quarter_end && r->quarter<5)CHECK(r->offense==thrower); /* it goes back to the team that threw */
            }else if(!r->finished){
                CHECK(m.event==FB_EVENT_INTERCEPTION && r->last.turnover && r->score[taker]==score_taker);
                /* The clock can run out on the return (the half hands the ball over anyway; overtime restarts at the 75). */
                if(!r->last.quarter_end && r->quarter<5)CHECK(r->offense==taker && r->down==1 && abs((int)r->ball-own_spot(taker,x))<=1);
            }
            CHECK(m.credit_count>=1 && m.credits[0].kind==FB_CREDIT_INTERCEPTION && m.credits[0].side==(taker==FB_HOME?FB_HOME:FB_AWAY));
            CHECK(m.stats[taker==FB_HOME?FB_HOME:FB_AWAY][m.credits[0].member][FB_CREDIT_INTERCEPTION-1]>=1);
            ++returns;yards_total+=(unsigned long)(m.return_yards>0?m.return_yards:0);if(m.return_yards>max_return)max_return=m.return_yards;
            turned=false;
        }
    }
}
/* An interception in the defender's own end zone, on a try, or out of bounds is still a dead ball (a touchback, a failed try). */
static void dead_ball_cases(void)
{
    assert(football_live_begin(&m,&roster,5,1));m.autoplay=1;m.phase=FB_LIVE_CALL;
    assert(football_live_defend(&m,FB_DEF_MAN));
    unsigned guard=0;while(m.phase!=FB_LIVE_PLAY){assert(++guard<400);football_live_tick(&m,17,(FootballLiveInput){.receiver=-1});}
    /* The ball comes down on top of an AWAY defender in his own end zone, with no receiver near. */
    unsigned def=11;m.thrown=m.flying=true;m.target=0;m.from_x=m.actors[0].x;m.from_y=m.actors[0].y;m.to_x=112;m.to_y=26.67f;m.flight=m.flight_time=1;
    m.actors[def].x=112;m.actors[def].y=26.67f;m.actors[def].attributes.cth=255;
    for(unsigned i=1;i<22;++i)if(i!=def){m.actors[i].x=60;m.actors[i].y=5+i;}
    for(unsigned i=0;i<200 && m.phase==FB_LIVE_PLAY;++i){football_live_tick(&m,17,(FootballLiveInput){.receiver=-1});CHECK(!m.turnover);}
    CHECK(m.phase!=FB_LIVE_PLAY);
    deep_picks+=m.event==FB_EVENT_INTERCEPTION;
}
int main(void)
{
    assert(football_roster_init(&roster));roster.count=22;
    for(unsigned i=0;i<22;++i)roster.player[i].attributes=(FootballAttributes){70,70,70,70,70,70,70,70};
    for(uint32_t seed=1;seed<=240;++seed)play(seed*7717u+1,false);
    for(uint32_t seed=1;seed<=60;++seed)play(seed*104729u+3,true);
    dead_ball_cases();
    CHECK(returns>100 && td_returns>=10 && fumbles>=1 && live_frames>1000 && human_takes>=1 && human_chases>=3);
    printf("turnover: %lu interceptions run back (%lu for a touchdown, %lu fumbled away, avg %.1f yd, best %ld), %lu human takers, %lu human chases, %lu live frames, %u checks\n",
        returns,td_returns,fumbles,(double)yards_total/(double)(returns?returns:1),max_return,human_takes,human_chases,live_frames,checks);
    puts("football interception return tests passed");
    return 0;
}
