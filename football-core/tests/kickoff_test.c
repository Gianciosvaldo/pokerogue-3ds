#include "football_live_systems.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static FootballRoster roster;
static FootballLive m;
static void begin(uint32_t seed)
{
    assert(football_live_begin(&m,&roster,seed,1));
    m.phase=FB_LIVE_CALL;m.special=FB_SPECIAL_NONE;
}
static void tick(unsigned frames,FootballLiveInput input){for(unsigned i=0;i<frames;++i)football_live_tick(&m,17,input);}
static FootballLiveInput idle(void){return (FootballLiveInput){.receiver=-1};}
/* Plays whatever is on the field to the dead ball. The human snaps and kicks with A, and when `steer` is set runs a
 * returner under the ball. */
static float push_x;
static void drive(bool steer)
{
    unsigned guard=0;
    while(m.phase!=FB_LIVE_DEAD && m.phase!=FB_LIVE_END){
        assert(++guard<6000);
        FootballLiveInput in=idle();
        if(m.phase==FB_LIVE_SET)in.pressed=FB_LIVE_ACTION;
        if(m.phase==FB_LIVE_PLAY && !m.special)in.x=push_x;
        if(steer && m.special && m.stage==FB_STAGE_FLIGHT && m.controlled<11){
            const FootballLiveActor *c=&m.actors[m.controlled];float dx=m.to_x-c->x,dy=m.to_y-c->y,len=hypotf(dx,dy);
            if(len>.3f){in.x=dx/len;in.y=dy/len;}
        }
        football_live_tick(&m,16,in);
    }
}
/* The offense scores a touchdown on a run from the 3: the carrier is put over the goal line on the first live step. */
static void touchdown(uint32_t seed,FootballSide side)
{
    begin(seed);m.rules.offense=(uint8_t)side;m.rules.ball=97;m.rules.down=1;m.rules.first_down_at=100;
    assert(football_live_call(&m,FB_RUN_DIVE));
    unsigned guard=0;
    while(m.phase!=FB_LIVE_PLAY){assert(++guard<400);football_live_tick(&m,16,(FootballLiveInput){.pressed=FB_LIVE_ACTION,.receiver=-1});}
    m.actors[m.carrier].x=side==FB_HOME?110.5f:9.5f;
    tick(2,idle());
}
static void next_call(void){tick(70,idle());}
int main(void)
{
    assert(football_roster_init(&roster));roster.count=22;
    for(unsigned i=0;i<22;++i)roster.player[i].attributes=(FootballAttributes){60,60,60,60,60,60,60,60};

    /* A touchdown is six points, and the scorer is asked for the try before anything else happens. */
    begin(3);assert(m.rules.td_points==6 && m.conversion==0 && m.after==FB_AFTER_NONE);
    touchdown(3,FB_HOME);
    assert(m.phase==FB_LIVE_DEAD && m.event==FB_EVENT_TOUCHDOWN && m.rules.score[FB_HOME]==6 && m.after==FB_AFTER_CONVERSION && m.after_side==FB_HOME);
    next_call();
    assert(m.phase==FB_LIVE_CALL && m.conversion==1 && m.rules.offense==FB_HOME && m.rules.ball==96 && m.next_offense==FB_AWAY && !m.special);
    assert(!football_live_call(&m,FB_RUN_DIVE) && !football_live_special(&m,FB_SPECIAL_PUNT) && !football_live_special(&m,FB_SPECIAL_FIELD_GOAL) && !football_live_special(&m,FB_SPECIAL_KICKOFF));
    assert(!football_live_special_allowed(&m,FB_SPECIAL_KICKOFF) && football_live_special_allowed(&m,FB_SPECIAL_EXTRA_POINT));
    assert(football_live_extra_point_chance(&m)>=90 && football_live_extra_point_chance(&m)<=99);
    /* Nothing but the try is allowed outside the try. */
    begin(3);assert(!football_live_special_allowed(&m,FB_SPECIAL_EXTRA_POINT) && !football_live_go_for_two(&m));

    /* The extra point: a kick from the 15 through the same formation as a field goal; +1, then the scorer kicks off. */
    unsigned made=0,missed=0,blocked=0;
    for(uint32_t seed=1;seed<=200;++seed){
        touchdown(seed,FB_HOME);next_call();assert(m.conversion==1);unsigned clock=m.rules.seconds_left;
        assert(football_live_special(&m,FB_SPECIAL_EXTRA_POINT) && m.phase==FB_LIVE_SET && m.rules.ball==85 && m.special==FB_SPECIAL_EXTRA_POINT);
        assert(m.actors[0].x<m.los-8 && fabsf(m.los-95)<.01f); /* the holder and kicker behind the line, from the 15 */
        drive(false);
        assert(m.phase==FB_LIVE_DEAD && m.conversion==0 && m.snaps==2 && m.rules.seconds_left==clock); /* a try never runs the clock */
        if(m.event==FB_EVENT_EXTRA_POINT){made++;assert(m.rules.score[FB_HOME]==7 && m.rules.last.conversion);}
        else if(m.event==FB_EVENT_BLOCKED){blocked++;assert(m.rules.score[FB_HOME]==6);}
        else{missed++;assert(m.event==FB_EVENT_EXTRA_POINT_MISSED && m.rules.score[FB_HOME]==6);}
        assert(m.rules.score[FB_AWAY]==0 && m.after==FB_AFTER_KICKOFF && m.after_side==FB_HOME && m.rules.offense==FB_AWAY && m.rules.ball==25);
    }
    assert(made>=170 && made<200 && missed>0 && made+missed+blocked==200);
    printf("kickoff: extra points %u/200 good, %u missed, %u blocked\n",made,missed,blocked);

    /* The kickoff follows: the scorer kicks from its own 35, ten men abreast, with the receiving team set up in two lines and two returners. */
    touchdown(5,FB_HOME);next_call();assert(football_live_special(&m,FB_SPECIAL_EXTRA_POINT));drive(false);
    assert(m.phase==FB_LIVE_DEAD);next_call();
    assert(m.phase==FB_LIVE_SET && m.special==FB_SPECIAL_KICKOFF && m.rules.offense==FB_HOME && m.rules.ball==35 && fabsf(m.los-45)<.01f && m.controlled==0);
    assert(m.actors[0].x<m.los-3 && fabsf(m.actors[0].y-26.67f)<.1f && m.carrier==0 && fabsf(m.ball_x-m.los)<.01f);
    unsigned abreast=0,front=0,deep=0;
    for(unsigned i=1;i<11;++i)if(fabsf(m.actors[i].x-(m.los-.6f))<.1f)abreast++;
    for(unsigned i=11;i<15;++i)if(fabsf(m.actors[i].x-(m.los+10.5f))<.1f)front++;
    for(unsigned i=20;i<22;++i)if(m.actors[i].x>m.los+50)deep++;
    assert(abreast==10 && front==4 && deep==2);
    float spread_low=100,spread_high=0;for(unsigned i=1;i<11;++i){if(m.actors[i].y<spread_low)spread_low=m.actors[i].y;if(m.actors[i].y>spread_high)spread_high=m.actors[i].y;}
    assert(spread_low<6 && spread_high>48);
    /* Nothing happens until the human kicks, and the kick comes after the run-up, not before. */
    tick(120,idle());assert(m.phase==FB_LIVE_SET && m.actors[0].x<m.los-3);
    tick(1,(FootballLiveInput){.pressed=FB_LIVE_ACTION,.receiver=-1});tick(30,idle());
    assert(m.phase==FB_LIVE_PLAY && m.stage==FB_STAGE_SNAP && !m.flying && m.actors[0].x>m.los-3.9f);
    tick(60,idle());assert(m.stage>=FB_STAGE_FLIGHT);

    /* Human kickoffs: the ball flies a real distance, the clock only runs from the catch, and the rules agree with the dead ball. */
    unsigned events[24]={0},returns=0,sum_return=0;int ret_min=1000,ret_max=-1000;
    for(uint32_t seed=1;seed<=160;++seed){
        touchdown(seed,FB_HOME);next_call();assert(football_live_special(&m,FB_SPECIAL_EXTRA_POINT));drive(false);next_call();
        assert(m.special==FB_SPECIAL_KICKOFF);FootballRules start=m.rules;
        drive(false);
        assert(m.phase==FB_LIVE_DEAD && m.special==FB_SPECIAL_KICKOFF && m.event!=FB_EVENT_NONE && m.kick_yards>=34 && m.kick_yards<=80);
        events[m.event]++;
        assert(m.rules.down==1 && m.rules.ball>=1 && m.rules.ball<=99 && m.rules.first_down_at>m.rules.ball);
        assert(m.rules.seconds_left<start.seconds_left && m.rules.seconds_left+30>=start.seconds_left);
        if(m.event==FB_EVENT_TOUCHBACK){assert(m.rules.ball==25 && m.rules.offense==FB_AWAY && start.seconds_left-m.rules.seconds_left==1);}
        else if(m.event==FB_EVENT_KICKOFF){
            assert(m.rules.offense==FB_AWAY);
            if(m.stage==FB_STAGE_RETURN){
                returns++;sum_return+=(unsigned)(m.return_yards>0?m.return_yards:0);
                /* The spot is the catch plus the return. */
                assert(fabsf((float)m.return_yards-(m.catch_x-(110-m.rules.ball)))<=1.6f);
                if(m.return_yards<ret_min)ret_min=m.return_yards;
                if(m.return_yards>ret_max)ret_max=m.return_yards;
            }else assert(m.return_yards==0);
        }else if(m.event==FB_EVENT_TOUCHDOWN){assert(m.rules.score[FB_AWAY]>=6 && m.after==FB_AFTER_CONVERSION && m.after_side==FB_AWAY);}
        else assert(m.event==FB_EVENT_MUFF || m.event==FB_EVENT_FUMBLE || m.event==FB_EVENT_OUT_OF_BOUNDS);
    }
    assert(returns>110 && events[FB_EVENT_TOUCHDOWN]<=6 && ret_min>=-12 && ret_max>20);
    assert(sum_return/returns>=8 && sum_return/returns<=32);
    printf("kickoff: 160 human kickoffs, %u returned (avg %u yds, best %d), %u touchbacks, %u muffs, %u touchdowns\n",returns,sum_return/returns,ret_max,events[FB_EVENT_TOUCHBACK],events[FB_EVENT_MUFF],events[FB_EVENT_TOUCHDOWN]);

    /* A strong leg boots it out of the end zone, and the ball comes out at the 25. */
    unsigned boomed=0;
    for(uint32_t seed=1;seed<=60;++seed){
        touchdown(seed,FB_HOME);next_call();assert(football_live_special(&m,FB_SPECIAL_EXTRA_POINT));drive(false);next_call();
        m.actors[0].attributes.thp=99;m.units[FB_HOME][m.lineup[FB_HOME][0]].thp=99;
        drive(false);
        if(m.event==FB_EVENT_TOUCHBACK)boomed++;
    }
    assert(boomed>=20 && boomed<=58);
    /* Pushing the kick to the sideline with a weak aim can run it out of bounds: the receiving team then takes it at the 40. */
    unsigned out=0,aimed=0;
    for(uint32_t seed=1;seed<=120;++seed){
        touchdown(seed,FB_HOME);next_call();assert(football_live_special(&m,FB_SPECIAL_EXTRA_POINT));drive(false);next_call();
        for(unsigned i=0;i<22;++i)m.actors[i].attributes.tha=1;
        unsigned guard=0;
        while(m.phase!=FB_LIVE_DEAD){assert(++guard<6000);football_live_tick(&m,16,(FootballLiveInput){.y=1,.pressed=m.phase==FB_LIVE_SET?FB_LIVE_ACTION:0u,.receiver=-1});}
        aimed++;
        if(m.event==FB_EVENT_OUT_OF_BOUNDS){out++;assert(m.rules.ball==40 && m.rules.offense==FB_AWAY);}
    }
    assert(out>5 && out<aimed);

    /* The human receives: the CPU kicks, the returner is under the ball, and a returned kick starts a drive at the spot. */
    unsigned caught=0,human_returns=0;
    for(uint32_t seed=1;seed<=120;++seed){
        touchdown(seed,FB_AWAY);next_call();assert(m.conversion==1 && m.rules.offense==FB_AWAY);
        assert(football_live_defend(&m,FB_DEF_MAN) && (m.special==FB_SPECIAL_EXTRA_POINT || m.conversion==2));
        if(m.conversion==2){drive(false);}else drive(false);
        assert(m.phase==FB_LIVE_DEAD && m.after==FB_AFTER_KICKOFF && m.after_side==FB_AWAY);next_call();
        assert(m.special==FB_SPECIAL_KICKOFF && m.rules.offense==FB_AWAY && m.controlled==9 && m.actors[m.controlled].home);
        drive(true);
        assert(m.phase==FB_LIVE_DEAD);
        if(m.event==FB_EVENT_KICKOFF || m.event==FB_EVENT_TOUCHDOWN){human_returns++;if(m.event==FB_EVENT_KICKOFF)assert(m.rules.offense==FB_HOME);}
        if(m.event!=FB_EVENT_TOUCHBACK && m.event!=FB_EVENT_OUT_OF_BOUNDS)caught++;
    }
    assert(caught>=100 && human_returns>=90);
    printf("kickoff: human returner caught %u of 120 kickoffs and ran back %u\n",caught,human_returns);

    /* The CPU's choice at the try: kick early, and go for two only where it changes what the other team needs. */
    unsigned early_two=0;
    for(uint32_t seed=1;seed<=400;++seed){
        touchdown(seed,FB_AWAY);next_call();assert(m.conversion==1);
        if(football_live_ai_goes_for_two(&m))early_two++;
    }
    assert(early_two>0 && early_two<400/6);
    touchdown(9,FB_AWAY);next_call();m.rules.quarter=4;m.rules.seconds_left=60;m.rules.score[FB_AWAY]=6;m.rules.score[FB_HOME]=8;assert(football_live_ai_goes_for_two(&m)); /* down two: two ties it */
    m.rules.score[FB_HOME]=9;assert(!football_live_ai_goes_for_two(&m));
    m.rules.score[FB_HOME]=11;assert(football_live_ai_goes_for_two(&m)); /* down five */
    m.rules.score[FB_HOME]=0;assert(!football_live_ai_goes_for_two(&m)); /* up six: kick it */

    /* Two points: a play from the 4. Carried or caught across the line it is worth two, anything else nothing. */
    touchdown(11,FB_HOME);next_call();assert(football_live_go_for_two(&m) && m.conversion==2 && m.phase==FB_LIVE_CALL);
    assert(football_live_call(&m,FB_RUN_DIVE) && fabsf(m.los-106)<.01f);
    unsigned guard=0;while(m.phase!=FB_LIVE_PLAY){assert(++guard<400);football_live_tick(&m,16,(FootballLiveInput){.pressed=FB_LIVE_ACTION,.receiver=-1});}
    m.actors[m.carrier].x=110.5f;tick(2,idle());
    assert(m.phase==FB_LIVE_DEAD && m.event==FB_EVENT_TWO_POINT && m.rules.score[FB_HOME]==8 && m.conversion==0 && m.rules.last.conversion);
    assert(m.after==FB_AFTER_KICKOFF && m.after_side==FB_HOME && m.rules.offense==FB_AWAY && m.rules.ball==25 && m.rules.seconds_left<120);
    unsigned two_good=0,two_bad=0;
    for(uint32_t seed=1;seed<=120;++seed){
        touchdown(seed,FB_HOME);next_call();unsigned clock=m.rules.seconds_left;assert(football_live_go_for_two(&m) && football_live_call(&m,FB_RUN_DIVE));
        push_x=1;drive(false);push_x=0;assert(m.phase==FB_LIVE_DEAD && m.conversion==0 && m.rules.seconds_left==clock); /* a try never runs the clock */
        if(m.event==FB_EVENT_TWO_POINT){two_good++;assert(m.rules.score[FB_HOME]==8);}
        else{two_bad++;assert(m.event==FB_EVENT_TWO_POINT_FAILED && m.rules.score[FB_HOME]==6);}
        assert(m.after==FB_AFTER_KICKOFF && m.after_side==FB_HOME && m.rules.offense==FB_AWAY);
    }
    assert(two_good>8 && two_bad>8);
    printf("kickoff: two-point tries %u good, %u stopped\n",two_good,two_bad);

    /* A touchdown that cannot be followed is worth the full seven: overtime and the last play of the game. */
    begin(4);m.rules.quarter=5;m.rules.ball=97;m.rules.first_down_at=100;m.rules.overtime_drives=0;m.rules.offense=FB_HOME;
    assert(football_live_call(&m,FB_RUN_DIVE));
    guard=0;while(m.phase!=FB_LIVE_PLAY){assert(++guard<400);football_live_tick(&m,16,(FootballLiveInput){.pressed=FB_LIVE_ACTION,.receiver=-1});}
    m.actors[m.carrier].x=110.5f;tick(2,idle());
    assert(m.phase==FB_LIVE_DEAD && m.rules.score[FB_HOME]==7 && m.after==FB_AFTER_NONE);next_call();assert(m.phase==FB_LIVE_CALL && m.conversion==0 && m.rules.quarter==5);
    begin(4);m.rules.quarter=4;m.rules.seconds_left=2;m.rules.score[FB_AWAY]=3;m.rules.ball=97;m.rules.first_down_at=100;assert(football_live_call(&m,FB_RUN_DIVE));
    guard=0;while(m.phase!=FB_LIVE_PLAY){assert(++guard<400);football_live_tick(&m,16,(FootballLiveInput){.pressed=FB_LIVE_ACTION,.receiver=-1});}
    m.actors[m.carrier].x=110.5f;tick(2,idle());
    assert(m.phase==FB_LIVE_END && m.rules.finished && m.rules.score[FB_HOME]==7 && m.rules.winner==FB_HOME && m.after==FB_AFTER_NONE);

    /* The second half starts with a kickoff, whichever play ends the half; a quarter break alone does not. */
    unsigned half_kicks=0,quarter_calls=0;
    for(uint32_t seed=1;seed<=40;++seed){
        begin(seed);m.rules.quarter=2;m.rules.seconds_left=1;m.rules.ball=50;m.rules.first_down_at=60;assert(football_live_call(&m,FB_RUN_DIVE));
        drive(false);
        if(m.event==FB_EVENT_TOUCHDOWN || m.event==FB_EVENT_INTERCEPTION || m.event==FB_EVENT_FUMBLE)continue;
        assert(m.rules.quarter==3 && m.rules.last.halftime && m.after==FB_AFTER_KICKOFF && m.after_side==FB_HOME && m.rules.offense==FB_AWAY);
        next_call();assert(m.special==FB_SPECIAL_KICKOFF && m.rules.offense==FB_HOME && m.phase==FB_LIVE_SET);half_kicks++;
        begin(seed);m.rules.quarter=1;m.rules.seconds_left=1;m.rules.ball=50;m.rules.first_down_at=60;assert(football_live_call(&m,FB_RUN_DIVE));drive(false);
        if(m.event!=FB_EVENT_TOUCHDOWN && m.event!=FB_EVENT_INTERCEPTION && m.event!=FB_EVENT_FUMBLE && m.rules.quarter==2){assert(m.after==FB_AFTER_NONE);next_call();assert(m.phase==FB_LIVE_CALL && !m.special);quarter_calls++;}
    }
    assert(half_kicks>=10 && quarter_calls>=10);
    /* A field goal and a safety both lead to a kickoff from the team that gave up the points. */
    bool field_goal=false;
    for(uint32_t seed=2;!field_goal && seed<60;++seed){begin(seed);m.rules.ball=88;m.rules.down=4;m.rules.first_down_at=100;assert(football_live_special(&m,FB_SPECIAL_FIELD_GOAL));drive(false);field_goal=m.event==FB_EVENT_FIELD_GOAL;}
    assert(field_goal && m.after==FB_AFTER_KICKOFF && m.after_side==FB_HOME && m.rules.offense==FB_AWAY);next_call();
    assert(m.special==FB_SPECIAL_KICKOFF && m.rules.offense==FB_HOME);
    begin(6);m.rules.ball=3;assert(football_live_call(&m,FB_RUN_DIVE));
    guard=0;while(m.phase!=FB_LIVE_PLAY){assert(++guard<400);football_live_tick(&m,16,(FootballLiveInput){.pressed=FB_LIVE_ACTION,.receiver=-1});}
    m.actors[m.carrier].x=0;tick(2,(FootballLiveInput){.x=-1,.receiver=-1});
    assert(m.phase==FB_LIVE_DEAD && m.rules.last.safety && m.after==FB_AFTER_KICKOFF && m.after_side==FB_HOME && m.rules.offense==FB_AWAY);

    /* A return can score, and the team that scored then kicks the try like any other. */
    bool saw_return_td=false;
    for(uint32_t seed=1;seed<=60 && !saw_return_td;++seed){
        touchdown(seed,FB_HOME);next_call();assert(football_live_special(&m,FB_SPECIAL_EXTRA_POINT));drive(false);next_call();
        guard=0;
        while(m.phase!=FB_LIVE_DEAD && !(m.stage==FB_STAGE_RETURN && m.phase==FB_LIVE_PLAY)){
            assert(++guard<6000);football_live_tick(&m,16,(FootballLiveInput){.pressed=m.phase==FB_LIVE_SET?FB_LIVE_ACTION:0u,.receiver=-1});
        }
        if(m.phase==FB_LIVE_DEAD)continue;
        assert(m.rules.offense==FB_AWAY && m.carrier>=11 && m.caught);
        m.actors[m.carrier].x=9.5f;tick(2,idle());
        assert(m.phase==FB_LIVE_DEAD && m.event==FB_EVENT_TOUCHDOWN && m.rules.score[FB_AWAY]==6 && m.after==FB_AFTER_CONVERSION && m.after_side==FB_AWAY && m.return_yards>40);
        next_call();assert(m.conversion==1 && m.rules.offense==FB_AWAY && m.next_offense==FB_HOME);
        saw_return_td=true;
    }
    assert(saw_return_td);

    /* Whole CPU-vs-CPU games: every point is one of the known kinds, every game finishes in legal states and replays identically. */
    uint32_t signature[2]={0,0};
    for(unsigned pass=0;pass<2;++pass){
        unsigned kicks=0,extra=0,twos=0,tds=0,returns_cpu=0;
        for(uint32_t seed=1;seed<=60;++seed){
            assert(football_live_begin(&m,&roster,seed,1));m.autoplay=1;unsigned guard2=0;
            while(m.phase!=FB_LIVE_END){
                assert(++guard2<300000);
                if(m.phase==FB_LIVE_CALL){
                    assert(m.rules.ball>=1 && m.rules.ball<=99 && m.special==FB_SPECIAL_NONE);
                    assert(m.conversion==0 || (m.conversion==1 && m.rules.ball==96 && m.rules.offense==m.after_side));
                    assert(football_live_defend(&m,FB_DEF_MAN));
                }
                unsigned before=m.snaps,points=(unsigned)m.rules.score[0]+m.rules.score[1];
                football_live_tick(&m,16,idle());
                if(m.snaps==before)continue;
                unsigned gained=(unsigned)m.rules.score[0]+m.rules.score[1]-points;
                switch(m.event){
                case FB_EVENT_TOUCHDOWN:assert(gained==6 || (gained==7 && m.after==FB_AFTER_NONE));tds++;break;
                case FB_EVENT_EXTRA_POINT:assert(gained==1);extra++;break;
                case FB_EVENT_TWO_POINT:assert(gained==2);twos++;break;
                case FB_EVENT_FIELD_GOAL:assert(gained==3);break;
                case FB_EVENT_SAFETY:assert(gained==2);break;
                default:assert(gained==0 || (m.rules.last.tiebreak && gained==2));
                }
                if(m.special==FB_SPECIAL_KICKOFF){kicks++;if(m.event==FB_EVENT_KICKOFF)returns_cpu++;}
                assert(m.event!=FB_EVENT_TOUCHDOWN || m.rules.last.touchdown);
                signature[pass]=signature[pass]*31u+m.rules.ball*7u+m.event+m.special*3u+m.rules.score[0]*5u+m.rules.score[1]+(unsigned)m.return_yards*11u;
            }
        }
        assert(kicks>60*2 && extra>60 && tds>=extra && returns_cpu>kicks/2);
        if(pass==0)printf("kickoff: 60 CPU games, %u kickoffs (%u returned), %u touchdowns, %u extra points, %u two-point conversions\n",kicks,returns_cpu,tds,extra,twos);
    }
    assert(signature[0]==signature[1]);
    puts("football kickoff and try tests passed");
    return 0;
}
