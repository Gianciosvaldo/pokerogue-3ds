#include "football_live_systems.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static FootballRoster roster;
static FootballLive m;
static void begin(uint32_t seed,FootballSide offense,unsigned ball,unsigned to_go,unsigned down)
{
    assert(football_live_begin(&m,&roster,seed,1));
    m.rules.offense=(uint8_t)offense;m.rules.ball=(uint8_t)ball;m.rules.down=(uint8_t)down;m.rules.first_down_at=(uint8_t)(ball+to_go>100?100:ball+to_go);
    m.phase=FB_LIVE_CALL;m.special=FB_SPECIAL_NONE;
    /* Rebuild the formation for the new possession. */
    assert(football_live_call(&m,FB_PASS_SLANTS));m.phase=FB_LIVE_CALL;m.special=FB_SPECIAL_NONE;
}
static void tick(unsigned frames,FootballLiveInput input){for(unsigned i=0;i<frames;++i)football_live_tick(&m,17,input);}
/* Plays a kick from the call to the dead ball. The human only snaps and, when `steer` is set, runs the controlled player to the ball. */
typedef struct { bool returned,flight,fair_pressed;unsigned frames;float peak; } Trace;
static Trace play(bool human_snaps,bool steer,bool press_fair)
{
    Trace t={0};unsigned guard=0;
    while(m.phase!=FB_LIVE_DEAD && m.phase!=FB_LIVE_END){
        assert(++guard<4000);
        FootballLiveInput in={.receiver=-1};
        if(m.phase==FB_LIVE_SET && human_snaps)in.pressed=FB_LIVE_ACTION;
        if(m.phase==FB_LIVE_PLAY && m.stage==FB_STAGE_FLIGHT && m.special==FB_SPECIAL_PUNT && m.controlled<22){
            const FootballLiveActor *c=&m.actors[m.controlled];
            if(steer){float dx=m.to_x-c->x,dy=m.to_y-c->y,len=hypotf(dx,dy);if(len>.3f){in.x=dx/len;in.y=dy/len;}}
            if(press_fair){in.pressed|=FB_LIVE_ACTION;t.fair_pressed=true;}
        }
        football_live_tick(&m,16,in);t.frames++;
        if(m.stage==FB_STAGE_FLIGHT)t.flight=true;
        if(m.stage==FB_STAGE_RETURN && m.phase==FB_LIVE_PLAY)t.returned=true;
        if(m.ball_z>t.peak)t.peak=m.ball_z;
    }
    return t;
}
int main(void)
{
    assert(football_roster_init(&roster));roster.count=22;
    for(unsigned i=0;i<22;++i)roster.player[i].attributes=(FootballAttributes){60,60,60,60,60,60,60,60};

    /* A punt is always allowed at the call, a field goal only inside kick range, and nothing mid-play. */
    begin(11,FB_HOME,30,10,4);
    assert(football_live_special_allowed(&m,FB_SPECIAL_PUNT) && !football_live_special_allowed(&m,FB_SPECIAL_FIELD_GOAL));
    assert(football_live_fg_distance(&m)==87 && !football_live_special(&m,FB_SPECIAL_FIELD_GOAL) && m.phase==FB_LIVE_CALL);
    begin(11,FB_HOME,85,10,4);
    assert(football_live_fg_distance(&m)==32 && football_live_special_allowed(&m,FB_SPECIAL_FIELD_GOAL));
    begin(11,FB_HOME,55,10,4);assert(football_live_fg_distance(&m)==62 && football_live_special_allowed(&m,FB_SPECIAL_FIELD_GOAL));
    begin(11,FB_HOME,54,10,4);assert(!football_live_special_allowed(&m,FB_SPECIAL_FIELD_GOAL));
    assert(!football_live_special(&m,FB_SPECIAL_NONE) && !football_live_special(&m,(FootballSpecial)9));
    assert(football_live_special(&m,FB_SPECIAL_PUNT) && m.phase==FB_LIVE_SET && !football_live_special_allowed(&m,FB_SPECIAL_PUNT));

    /* The punt formation: punter deep behind the line, line on the ball, returners deep and the coverage men wide. */
    begin(12,FB_HOME,30,10,4);assert(football_live_special(&m,FB_SPECIAL_PUNT));
    assert(m.actors[0].x<m.los-13 && fabsf(m.actors[0].y-26.67f)<.1f && m.carrier==0 && m.controlled==0);
    assert(m.actors[11+9].x>m.los+35 && m.actors[2].y<5 && m.actors[3].y>48);
    unsigned line=0;for(unsigned i=6;i<11;++i)if(fabsf(m.actors[i].x-m.los)<1)line++;
    assert(line==5);
    /* Nothing happens until the human snaps; then the kick comes after the snap time, not before. */
    float before=m.actors[0].x;tick(120,(FootballLiveInput){.receiver=-1});assert(m.phase==FB_LIVE_SET && fabsf(m.actors[0].x-before)<.01f);
    tick(1,(FootballLiveInput){.pressed=FB_LIVE_ACTION,.receiver=-1});tick(40,(FootballLiveInput){.receiver=-1});
    assert(m.phase==FB_LIVE_PLAY && m.stage==FB_STAGE_SNAP && !m.flying);
    tick(60,(FootballLiveInput){.receiver=-1});assert(m.stage>=FB_STAGE_FLIGHT);

    /* Human punts: the ball flies a real distance, the possession flips and the rules agree with the dead ball. */
    unsigned events[16]={0},returns=0,flights=0;int net_min=1000,net_max=-1000;
    for(uint32_t seed=1;seed<=120;++seed){
        begin(seed,FB_HOME,30,10,4);FootballRules start=m.rules;
        assert(football_live_special(&m,FB_SPECIAL_PUNT));
        Trace t=play(true,false,false);flights+=t.flight;returns+=t.returned;
        assert(m.phase==FB_LIVE_DEAD && m.special==FB_SPECIAL_PUNT && m.event!=FB_EVENT_NONE);
        events[m.event]++;
        assert(m.kick_yards>=22 && m.kick_yards<=68 && t.peak>3);
        assert(m.rules.offense==FB_AWAY || m.event==FB_EVENT_MUFF || m.event==FB_EVENT_FUMBLE);
        assert(m.rules.down==1 && m.rules.ball>=1 && m.rules.ball<=99 && m.rules.first_down_at>m.rules.ball);
        assert(m.rules.seconds_left<start.seconds_left && m.rules.seconds_left+30>=start.seconds_left && m.snaps==1);
        assert(m.rules.score[0]==start.score[0] || m.event==FB_EVENT_TOUCHDOWN);
        if(m.event==FB_EVENT_TOUCHBACK)assert(m.rules.ball==20 && m.rules.offense==FB_AWAY);
        if(m.event==FB_EVENT_PUNT || m.event==FB_EVENT_FAIR_CATCH){if(m.result.yards<net_min)net_min=m.result.yards;if(m.result.yards>net_max)net_max=m.result.yards;}
        /* The next call arrives with the possession it earned. */
        tick(70,(FootballLiveInput){.receiver=-1});assert(m.phase==FB_LIVE_CALL && m.special==FB_SPECIAL_NONE);
    }
    assert(flights==120 && returns>10);
    assert(events[FB_EVENT_FAIR_CATCH]+events[FB_EVENT_PUNT]>60 && events[FB_EVENT_TOUCHDOWN]<=3);
    assert(net_min>=-10 && net_max<=68);

    /* A returned punt hands the ball to the receiving team: rules.offense flips, the returner carries it, the kicking team chases. */
    bool saw_return=false;
    for(uint32_t seed=1;seed<=60 && !saw_return;++seed){
        begin(seed,FB_HOME,30,10,4);assert(football_live_special(&m,FB_SPECIAL_PUNT));
        unsigned guard=0;
        while(m.phase!=FB_LIVE_DEAD && ++guard<4000){
            football_live_tick(&m,16,(FootballLiveInput){.pressed=m.phase==FB_LIVE_SET?FB_LIVE_ACTION:0u,.receiver=-1});
            if(m.stage==FB_STAGE_RETURN && m.phase==FB_LIVE_PLAY){
                saw_return=true;assert(m.rules.offense==FB_AWAY && m.carrier==11+9 && m.caught && !m.flying);
                assert(football_live_cpu(&m,FB_AWAY) && m.controlled<11); /* the human covers the kick */
                break;
            }
        }
        if(saw_return){while(m.phase!=FB_LIVE_DEAD && ++guard<8000)football_live_tick(&m,16,(FootballLiveInput){.receiver=-1});
            assert(m.phase==FB_LIVE_DEAD && (m.event==FB_EVENT_PUNT || m.event==FB_EVENT_TOUCHDOWN || m.event==FB_EVENT_FUMBLE));}
    }
    assert(saw_return);

    /* A human returner steers under the ball and can call a fair catch with A. */
    unsigned fair=0,runs=0,catches=0;
    for(uint32_t seed=1;seed<=80;++seed){
        begin(seed,FB_AWAY,25,10,4);
        assert(football_live_defend(&m,FB_DEF_MAN) && m.special==FB_SPECIAL_PUNT && m.controlled==9);
        Trace t=play(false,true,true);(void)t;
        if(m.event==FB_EVENT_FAIR_CATCH){fair++;assert(m.rules.offense==FB_HOME);}
        else if(m.event==FB_EVENT_PUNT || m.event==FB_EVENT_MUFF)catches++;
        runs++;
    }
    assert(fair>runs/2 && fair+catches>=runs*3/4);
    /* Without the fair catch the same returner gets the ball and runs it back. */
    unsigned returned=0;
    for(uint32_t seed=1;seed<=60;++seed){
        begin(seed,FB_AWAY,25,10,4);assert(football_live_defend(&m,FB_DEF_MAN));
        returned+=play(false,true,false).returned;
    }
    assert(returned>20);

    /* A rusher who reaches the punter first blocks it, and the return team takes over there. */
    begin(5,FB_HOME,40,10,4);assert(football_live_special(&m,FB_SPECIAL_PUNT));
    tick(1,(FootballLiveInput){.pressed=FB_LIVE_ACTION,.receiver=-1});
    m.actors[11].x=m.actors[0].x+1.0f;m.actors[11].y=m.actors[0].y;
    tick(4,(FootballLiveInput){.receiver=-1});
    /* The punter stood on HOME's 25 (x = 35.5), which is AWAY's 75. */
    assert(m.phase==FB_LIVE_DEAD && m.event==FB_EVENT_BLOCKED && m.rules.offense==FB_AWAY && m.rules.ball>=74 && m.rules.ball<=76 && m.rules.down==1);

    /* Field goals: the make rate falls with distance and the rules score three or give the ball back. */
    unsigned good_short=0,good_long=0,tries=300;
    for(uint32_t seed=1;seed<=tries;++seed){
        begin(seed,FB_HOME,90,10,4);FootballRules start=m.rules;assert(football_live_special(&m,FB_SPECIAL_FIELD_GOAL));
        Trace t=play(true,false,false);assert(t.flight && m.special==FB_SPECIAL_FIELD_GOAL);
        if(m.event==FB_EVENT_FIELD_GOAL){good_short++;assert(m.rules.score[0]==start.score[0]+3 && m.rules.offense==FB_AWAY && m.rules.ball==25 && m.rules.last.field_goal);}
        else{assert(m.event==FB_EVENT_FIELD_GOAL_MISSED || m.event==FB_EVENT_BLOCKED);assert(m.rules.score[0]==start.score[0] && m.rules.offense==FB_AWAY && m.rules.ball==20);}
    }
    for(uint32_t seed=1;seed<=tries;++seed){
        begin(seed,FB_HOME,57,10,4);assert(football_live_special(&m,FB_SPECIAL_FIELD_GOAL));
        play(true,false,false);
        if(m.event==FB_EVENT_FIELD_GOAL)good_long++;
        else assert(m.rules.ball==43); /* missed from the 43-yard line: the ball goes back to the line of scrimmage */
    }
    assert(good_short>tries*8/10 && good_short<tries && good_long>tries/10 && good_long<tries*6/10 && good_short>good_long+tries/5);
    printf("special: fg 32yd %u/%u, 60yd %u/%u\n",good_short,tries,good_long,tries);

    /* A field goal as the CPU's defender: the human steers a rusher and can dive at the kicker. A rush through the line
     * almost never gets there in time, but the kick always resolves and a block is possible. */
    unsigned resolved=0;
    for(uint32_t seed=1;seed<=100;++seed){
        begin(seed,FB_AWAY,88,10,4);assert(football_live_defend(&m,FB_DEF_MAN) && m.special==FB_SPECIAL_FIELD_GOAL && m.controlled==5);
        unsigned guard=0;
        while(m.phase!=FB_LIVE_DEAD && ++guard<3000){
            const FootballLiveActor *k=&m.actors[11],*c=&m.actors[m.controlled];float dx=k->x-c->x,dy=k->y-c->y,len=hypotf(dx,dy);
            FootballLiveInput in={.x=len>.1f?dx/len:0,.y=len>.1f?dy/len:0,.receiver=-1,.pressed=len<2.2f?FB_LIVE_ACTION:0u};
            football_live_tick(&m,16,in);
        }
        resolved+=m.event==FB_EVENT_FIELD_GOAL || m.event==FB_EVENT_FIELD_GOAL_MISSED || m.event==FB_EVENT_BLOCKED;
        assert(m.rules.offense==FB_HOME || m.rules.offense==FB_AWAY);
    }
    assert(resolved==100);
    begin(7,FB_HOME,88,10,4);assert(football_live_special(&m,FB_SPECIAL_FIELD_GOAL));
    tick(1,(FootballLiveInput){.pressed=FB_LIVE_ACTION,.receiver=-1});
    m.actors[11+5].x=m.actors[0].x+1.0f;m.actors[11+5].y=m.actors[0].y;
    tick(4,(FootballLiveInput){.receiver=-1});
    assert(m.phase==FB_LIVE_DEAD && m.event==FB_EVENT_BLOCKED && m.rules.offense==FB_AWAY && m.rules.score[0]==0);

    /* The CPU's fourth-down choices. */
    unsigned punt=0,field=0,go=0;
    for(uint32_t seed=1;seed<=200;++seed){
        begin(seed,FB_AWAY,30,10,4);if(football_live_ai_special(&m)==FB_SPECIAL_PUNT)punt++;
        begin(seed,FB_AWAY,85,6,4);if(football_live_ai_special(&m)==FB_SPECIAL_FIELD_GOAL)field++;
        begin(seed,FB_AWAY,60,1,4);if(football_live_ai_special(&m)==FB_SPECIAL_NONE)go++;
        begin(seed,FB_AWAY,30,10,3);assert(football_live_ai_special(&m)==FB_SPECIAL_NONE);
        begin(seed,FB_AWAY,30,10,1);assert(football_live_ai_special(&m)==FB_SPECIAL_NONE);
    }
    assert(punt==200 && field==200 && go>200*3/10 && go<200*8/10);
    /* Overtime never punts, and a team that must score late does not either. */
    begin(3,FB_AWAY,30,10,4);m.rules.quarter=5;assert(football_live_ai_special(&m)==FB_SPECIAL_NONE);
    begin(3,FB_AWAY,55,6,4);m.rules.quarter=4;m.rules.seconds_left=50;m.rules.score[FB_HOME]=10;m.rules.score[FB_AWAY]=0;
    assert(football_live_ai_special(&m)==FB_SPECIAL_NONE);
    begin(3,FB_AWAY,85,6,4);m.rules.quarter=4;m.rules.seconds_left=50;m.rules.score[FB_HOME]=2;m.rules.score[FB_AWAY]=0;
    assert(football_live_ai_special(&m)==FB_SPECIAL_FIELD_GOAL); /* a field goal wins it */
    begin(3,FB_AWAY,85,6,4);m.rules.quarter=4;m.rules.seconds_left=50;m.rules.score[FB_HOME]=10;m.rules.score[FB_AWAY]=0;
    assert(football_live_ai_special(&m)==FB_SPECIAL_NONE); /* a field goal does not */

    /* Whole CPU-vs-CPU games with kicks in them finish, keep legal rules state and stay deterministic. */
    uint32_t signature[2]={0,0};
    for(unsigned pass=0;pass<2;++pass){
        unsigned punts=0,goals=0;
        for(uint32_t seed=1;seed<=30;++seed){
            assert(football_live_begin(&m,&roster,seed,1));m.autoplay=1;unsigned guard=0;
            while(m.phase!=FB_LIVE_END){
                assert(++guard<200000);
                if(m.phase==FB_LIVE_CALL){
                    assert(m.rules.ball>=1 && m.rules.ball<=99 && m.rules.down>=1 && m.rules.down<=4 && m.special==FB_SPECIAL_NONE);
                    assert(football_live_defend(&m,FB_DEF_MAN));
                }
                unsigned snaps=m.snaps;football_live_tick(&m,16,(FootballLiveInput){.receiver=-1});
                if(m.snaps!=snaps){
                    if(m.special==FB_SPECIAL_PUNT)punts++;else if(m.special==FB_SPECIAL_FIELD_GOAL)goals++;
                    signature[pass]=signature[pass]*31u+m.rules.ball*7u+m.event+m.special*3u+m.rules.score[0]*5u+m.rules.score[1];
                }
            }
        }
        assert(punts>30*3 && punts<30*14 && goals>10 && goals<30*5);
        if(pass==0)printf("special: 30 CPU games, %u punts, %u field goals\n",punts,goals);
    }
    assert(signature[0]==signature[1]);
    puts("football special teams tests passed");
    return 0;
}
