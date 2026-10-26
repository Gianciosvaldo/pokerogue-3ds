#include "football_live_systems.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static FootballRoster roster;
static FootballLive m;
static FootballLiveInput idle(void){return (FootballLiveInput){.receiver=-1};}
static unsigned count(FootballFxKind kind)
{
    unsigned n=0;
    for(unsigned i=0;i<FB_FX_COUNT;++i)n+=m.fx[i].kind==kind;
    return n;
}
static unsigned fresh(FootballFxKind kind)
{
    unsigned n=0;
    for(unsigned i=0;i<FB_FX_COUNT;++i)n+=m.fx[i].kind==kind && m.fx[i].step==m.steps;
    return n;
}
static const FootballFx *newest(FootballFxKind kind)
{
    const FootballFx *found=NULL;
    for(unsigned i=0;i<FB_FX_COUNT;++i)if(m.fx[i].kind==kind && (!found || m.fx[i].step>=found->step))found=&m.fx[i];
    return found;
}
static void snap(FootballPlayId play,bool human_ball)
{
    unsigned guard=0;
    assert(football_live_call(&m,play));
    while(m.phase!=FB_LIVE_PLAY){assert(++guard<400);football_live_tick(&m,16,(FootballLiveInput){.pressed=human_ball?FB_LIVE_ACTION:0u,.receiver=-1});}
}
int main(void)
{
    assert(football_roster_init(&roster));roster.count=22;
    for(unsigned i=0;i<22;++i)roster.player[i].attributes=(FootballAttributes){60,60,60,60,60,60,60,60};
    assert(football_live_begin(&m,&roster,3,1));
    /* The record: stamped with the clock and the spot, a ring of eight that overwrites the oldest, bad calls ignored. */
    for(unsigned i=0;i<8;++i)assert(m.fx[i].kind==FB_FX_NONE);
    m.steps=500;m.actors[4].x=33;m.actors[4].y=12;football_live_fx(&m,FB_FX_TACKLE,4,15,-7);
    assert(m.fx[0].kind==FB_FX_TACKLE && m.fx[0].actor==4 && m.fx[0].other==15 && m.fx[0].side==-1 && m.fx[0].step==500 && m.fx[0].x==33 && m.fx[0].y==12 && m.fx_next==1);
    m.ball_x=60;m.ball_y=20;football_live_fx(&m,FB_FX_INTERCEPTION,16,3,5);
    assert(m.fx[1].x==60 && m.fx[1].y==20 && m.fx[1].side==1);
    football_live_fx(&m,FB_FX_NONE,4,15,1);football_live_fx(&m,(FootballFxKind)99,4,15,1);football_live_fx(&m,FB_FX_JUKE,22,15,1);football_live_fx(NULL,FB_FX_JUKE,4,15,1);
    assert(m.fx_next==2);
    football_live_fx(&m,FB_FX_JUKE,4,99,1);assert(m.fx[2].other==22);
    for(unsigned i=0;i<10;++i){m.steps=600+i;football_live_fx(&m,FB_FX_CATCH,5,22,1);}
    assert(m.fx_next==(2+1+10)%FB_FX_COUNT && count(FB_FX_CATCH)==8 && !count(FB_FX_TACKLE) && !count(FB_FX_INTERCEPTION) && newest(FB_FX_CATCH)->step==609);

    /* A human runner's juke is recorded once per press and cooldown, for the ball carrier, with the way he is cutting. */
    assert(football_live_begin(&m,&roster,5,1));snap(FB_RUN_DIVE,true);
    assert(m.carrier==m.controlled && m.rules.offense==FB_HOME);
    for(unsigned i=0;i<20;++i)football_live_tick(&m,16,(FootballLiveInput){.y=1,.receiver=-1});
    float vy=m.actors[m.carrier].vy;
    football_live_tick(&m,16,(FootballLiveInput){.y=1,.pressed=FB_LIVE_JUKE,.receiver=-1});
    assert(count(FB_FX_JUKE)==1 && newest(FB_FX_JUKE)->actor==m.carrier && newest(FB_FX_JUKE)->side==(vy<0?-1:1) && m.juke>0);
    for(unsigned i=0;i<10;++i)football_live_tick(&m,16,(FootballLiveInput){.pressed=FB_LIVE_JUKE,.receiver=-1});
    assert(count(FB_FX_JUKE)==1); /* the cooldown keeps a second one away */

    /* A human defender's dive is recorded whenever he dives, whether or not it reaches the runner. */
    assert(football_live_begin(&m,&roster,6,1));m.phase=FB_LIVE_CALL;m.rules.offense=FB_AWAY;m.rules.ball=50;m.rules.first_down_at=60;
    snap(FB_RUN_DIVE,false);
    assert(m.controlled<11);
    unsigned guard=0;bool dived=false;
    while(m.phase==FB_LIVE_PLAY && ++guard<300){
        football_live_tick(&m,16,(FootballLiveInput){.pressed=guard==20?FB_LIVE_ACTION:0u,.receiver=-1});
        if(guard==20){assert(count(FB_FX_DIVE)==1 && newest(FB_FX_DIVE)->actor==m.controlled && newest(FB_FX_DIVE)->other==m.carrier);dived=true;}
    }
    assert(dived);

    /* Whole games: every score, sack and takeaway has its moment, naming the player the credits name, and the dead ball
     * waits as long as the moment needs. */
    unsigned long tackles=0,touchdowns=0,sacks=0,interceptions=0,fumbles=0,catches=0,jukes=0;
    for(uint32_t seed=1;seed<=30;++seed){
        assert(football_live_begin(&m,&roster,seed,1));m.autoplay=1;guard=0;unsigned dead_since=0,dead_event=0;bool was_dead=false;
        while(m.phase!=FB_LIVE_END){
            assert(++guard<300000);
            if(m.phase==FB_LIVE_CALL)assert(football_live_defend(&m,FB_DEF_MAN));
            unsigned before=m.snaps;football_live_tick(&m,16,idle());
            if(was_dead && m.phase!=FB_LIVE_DEAD){
                float held=(float)(m.steps-dead_since)/60.0f,want=dead_event==FB_EVENT_TOUCHDOWN?FB_LIVE_HOLD_TOUCHDOWN:dead_event==FB_EVENT_INTERCEPTION || dead_event==FB_EVENT_SACK || dead_event==FB_EVENT_FUMBLE?FB_LIVE_HOLD_TURNOVER:1;
                assert(held>=want-.001f && held<want+.05f);
                for(unsigned i=0;i<FB_FX_COUNT;++i)assert(m.fx[i].kind==FB_FX_NONE); /* the next play starts clean */
            }
            was_dead=m.phase==FB_LIVE_DEAD;
            if(m.snaps==before){
                catches+=fresh(FB_FX_CATCH);jukes+=fresh(FB_FX_JUKE);
                continue;
            }
            dead_since=m.steps;dead_event=m.event;
            unsigned td=0,intc=0,sack=0;
            for(unsigned i=0;i<m.credit_count;++i){
                const FootballCredit *c=&m.credits[i];FootballFxKind kind=c->kind==FB_CREDIT_TOUCHDOWN?FB_FX_TOUCHDOWN:c->kind==FB_CREDIT_SACK?FB_FX_SACK:FB_FX_INTERCEPTION;
                const FootballFx *f=newest(kind);
                assert(f && f->step==m.steps && m.actors[f->actor].roster==c->member && (f->actor<11)==(c->side==FB_HOME));
                td+=kind==FB_FX_TOUCHDOWN;intc+=kind==FB_FX_INTERCEPTION;sack+=kind==FB_FX_SACK;
            }
            assert(fresh(FB_FX_TOUCHDOWN)==td && fresh(FB_FX_INTERCEPTION)==intc && fresh(FB_FX_SACK)==sack);
            assert(m.event!=FB_EVENT_FUMBLE || fresh(FB_FX_FUMBLE)==1);
            assert(fresh(FB_FX_SACK)+fresh(FB_FX_TACKLE)<=1); /* one tackle at most ends a play */
            tackles+=fresh(FB_FX_TACKLE);touchdowns+=td;sacks+=sack;interceptions+=intc;fumbles+=fresh(FB_FX_FUMBLE);
        }
    }
    assert(tackles>100 && touchdowns>20 && sacks>10 && interceptions>5 && catches>100 && jukes>5);
    printf("fx: 30 games recorded %lu tackles, %lu sacks, %lu interceptions, %lu fumbles, %lu touchdowns, %lu catches, %lu CPU jukes\n",tackles,sacks,interceptions,fumbles,touchdowns,catches,jukes);
    puts("football motion record tests passed");
    return 0;
}
