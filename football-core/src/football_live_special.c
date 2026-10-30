#include "football_live_systems.h"
#include <math.h>
/* Punts, field goals, extra points and kickoffs on the live field. The kicking team is rules.offense until the ball
 * is caught; a return then flips rules.offense to the receiving team, so its carrier, blockers and the kicking
 * team's pursuit are the ordinary run-play systems. Every rules change goes through a football_rules_* call. */
#define PUNT_TIME 1.45f
#define FG_TIME 1.05f
#define KICKOFF_TIME 1.1f
#define FG_MAX_DISTANCE 62u
static float limit(float v,float a,float b){return v<a?a:v>b?b:v;}
static unsigned base(unsigned side){return side==FB_HOME?0:11;}
static float dir(unsigned side){return side==FB_HOME?1:-1;}
static float jitter(FootballLive *m){return ((int)(football_live_random(m)%2001)-1000)/1000.0f;}
static float gap(const FootballLiveActor *a,float x,float y){return hypotf(a->x-x,a->y-y);}
/* A goal kick goes through the posts (field goal, extra point); a free kick comes down in the field (punt, kickoff). */
static bool goal_kick(const FootballLive *m){return m->special==FB_SPECIAL_FIELD_GOAL || m->special==FB_SPECIAL_EXTRA_POINT;}
static bool kickoff(const FootballLive *m){return m->special==FB_SPECIAL_KICKOFF;}
static float kick_time(const FootballLive *m){return m->special==FB_SPECIAL_PUNT?PUNT_TIME:kickoff(m)?KICKOFF_TIME:FG_TIME;}
/* Yards from `side`'s own goal line (1..99) of a field x. */
static int spot_for(unsigned side,float x)
{
    int s=(int)roundf(side==FB_HOME?x-10:110-x);
    return s<1?1:s>99?99:s;
}
unsigned football_live_fg_distance(const FootballLive *m){return m?(unsigned)(100-m->rules.ball)+17u:0;}
static unsigned make_chance(const FootballLive *m,float distance)
{
    const FootballLiveActor *k=&m->actors[base(m->rules.offense)];
    float p=.98f-fmaxf(0,distance-20)*.0155f+((int)k->attributes.thp-50)*.004f+((int)k->attributes.tha-50)*.003f;
    return (unsigned)limit(p*100,3,99);
}
unsigned football_live_fg_chance(const FootballLive *m){return m?make_chance(m,(float)football_live_fg_distance(m)):0;}
/* The extra point is a short kick: it never loses to distance, only to a weak or wild leg. */
unsigned football_live_extra_point_chance(const FootballLive *m){return m?make_chance(m,20):0;}
bool football_live_special_allowed(const FootballLive *m,FootballSpecial kind)
{
    if(!m || m->phase!=FB_LIVE_CALL || m->rules.finished)return false;
    if(kind==FB_SPECIAL_EXTRA_POINT)return m->conversion==1;
    if(m->conversion)return false;
    if(kind==FB_SPECIAL_PUNT)return true;
    return kind==FB_SPECIAL_FIELD_GOAL && football_live_fg_distance(m)<=FG_MAX_DISTANCE;
}
void football_live_special_arrange(FootballLive *m)
{
    unsigned ks=m->rules.offense,rs=1-ks,kb=base(ks),rb=base(rs);float d=dir(ks);
    bool punt=m->special==FB_SPECIAL_PUNT,off=kickoff(m);
    m->kick_side=(uint8_t)ks;m->stage=FB_STAGE_SNAP;m->fair_catch=false;m->kick_good=false;m->kick_yards=m->return_yards=0;m->event=FB_EVENT_NONE;
    /* Offsets from the line of scrimmage, toward the kicking team's own goal for the kickers (rank 0 is the kicker,
     * 1 the protector/holder, 2-5 wings and gunners, 6-10 the line) and toward the opponent's goal for the others. */
    static const float punt_kx[11]={-14.5f,-7.5f,-.4f,-.4f,-.4f,-.4f,-.5f,-.5f,-.5f,-.5f,-.5f};
    static const float punt_ky[11]={26.67f,26.67f,1.5f,52,14,39.5f,21.5f,24.2f,26.67f,29.1f,31.8f};
    static const float fg_kx[11]={-8.4f,-7,-.5f,-.5f,-.5f,-.5f,-.5f,-.5f,-.5f,-.5f,-.5f};
    static const float fg_ky[11]={24.9f,26.67f,15.2f,38.2f,18.4f,35,21.5f,24.2f,26.67f,29.1f,31.8f};
    static const float punt_rx[11]={1.6f,1.6f,1.6f,1.6f,3.2f,3.2f,3.2f,1.5f,1.5f,0,0};
    static const float punt_ry[11]={20.5f,24.5f,28.8f,32.8f,13,26.67f,40,3.5f,50,26.67f,40};
    static const float fg_rx[11]={1.5f,1.5f,1.5f,1.5f,1.6f,1.6f,1.6f,1.5f,1.5f,0,0};
    static const float fg_ry[11]={20,24.5f,29,33.5f,15,26.67f,38.5f,11.5f,42,26.67f,26.67f};
    /* A kickoff: the kicker a few steps behind the ball, the other ten abreast; the front line stands ten yards off, a
     * second line behind it, two returners deep. */
    static const float ko_kx[11]={-4,-.6f,-.6f,-.6f,-.6f,-.6f,-.6f,-.6f,-.6f,-.6f,-.6f};
    static const float ko_ky[11]={26.67f,4.5f,9.5f,14.5f,19.5f,24,29.5f,34.5f,39.5f,44.5f,49.2f};
    static const float ko_rx[11]={10.5f,10.5f,10.5f,10.5f,21,21,21,33,33,0,0};
    static const float ko_ry[11]={12,21,32.5f,41.5f,15,26.67f,38.5f,6,47.3f,26.67f,34};
    for(unsigned i=0;i<11;++i){
        FootballLiveActor *k=&m->actors[kb+i],*r=&m->actors[rb+i];
        if(off){k->x=m->los+d*ko_kx[i];k->y=ko_ky[i];r->x=m->los+d*ko_rx[i];r->y=ko_ry[i];}
        else{
            k->x=m->los+d*(punt?punt_kx[i]:fg_kx[i]);k->y=punt?punt_ky[i]:fg_ky[i];
            r->x=m->los+d*(punt?punt_rx[i]:fg_rx[i]);r->y=punt?punt_ry[i]:fg_ry[i];
        }
        k->vx=k->vy=r->vx=r->vy=0;k->facing=(int8_t)d;r->facing=(int8_t)-d;
    }
    /* The returner waits deep, but not past the back line. A field-goal safety stands near the end line. */
    FootballLiveActor *back=&m->actors[rb+9],*second=&m->actors[rb+10];
    if(off){
        back->x=d>0?105:15;back->y=26.67f;second->x=d>0?100:20;second->y=34;
    }else if(punt){
        back->x=d>0?fminf(m->los+42,105):fmaxf(m->los-42,15);back->y=26.67f;
        second->x=back->x-d*9;second->y=40;
    }else{
        back->x=d>0?fminf(m->los+30,112):fmaxf(m->los-30,8);back->y=26.67f;
        second->x=d>0?fminf(m->los+12,108):fmaxf(m->los-12,12);second->y=26.67f;
    }
    /* The kicker holds the ball. The human steers a returner, or a rusher on a field goal, and kicks by pressing A. */
    m->carrier=(uint8_t)kb;m->target=(uint8_t)kb;
    bool human_kicks=!football_live_cpu(m,(FootballSide)ks),human_receives=!football_live_cpu(m,(FootballSide)rs);
    m->controlled=human_kicks?(uint8_t)kb:human_receives?(uint8_t)(rb+(punt || off?9:5)):FB_NO_ACTOR;
    m->ball_x=off?m->los:m->actors[kb].x;m->ball_y=off?26.67f:m->actors[kb].y;m->ball_z=0;
    if(off){m->carrier=(uint8_t)kb;m->target=(uint8_t)kb;}
}
/* The ball is dead and the rules have been told: banner, snap count and the next phase. */
static void settle(FootballLive *m,bool ok,FootballLiveEvent event,float final_x,unsigned seconds)
{
    int net=(int)roundf((final_x-m->los)*dir(m->kick_side));
    m->result=(FootballResolvedPlay){FB_RESULT_RUN,(int16_t)limit((float)net,-100,100),(uint8_t)seconds};
    if(!ok){m->rules.finished=true;m->rules.winner=FB_AWAY;}
    m->event=(uint8_t)event;m->flying=false;m->ball_z=0;m->caught=false;
    ++m->snaps;m->phase=m->rules.finished?FB_LIVE_END:FB_LIVE_DEAD;m->dead_time=0;
    football_live_after_play(m);
}
/* A punt costs its whole play; a kickoff only runs the clock from the catch. */
static unsigned clock_seconds(const FootballLive *m)
{
    if(kickoff(m))return (unsigned)limit(ceilf(m->elapsed-m->catch_time)+1,1,30);
    return (unsigned)limit(ceilf(m->elapsed)+2,1,30);
}
/* Possession goes to `receiver` at `spot` yards from its own goal. */
static void conclude(FootballLive *m,unsigned receiver,int spot,FootballLiveEvent event,float final_x)
{
    unsigned seconds=clock_seconds(m);
    m->return_yards=m->stage==FB_STAGE_RETURN?(int16_t)limit(roundf((final_x-m->catch_x)*dir(1-m->kick_side)),-100,100):0;
    settle(m,football_rules_possession(&m->rules,(FootballSide)receiver,(unsigned)limit((float)spot,1,99),seconds),event,final_x,seconds);
}
void football_live_special_finish(FootballLive *m,FootballResultKind kind,float x)
{
    unsigned ks=m->kick_side,rs=1-ks;float rd=dir(rs);unsigned seconds=clock_seconds(m);
    bool score=(rd>0 && x>=110) || (rd<0 && x<=10);
    if(kind==FB_RESULT_FUMBLE)conclude(m,ks,spot_for(ks,x),FB_EVENT_FUMBLE,x);
    else if(score){
        football_live_credit(m,m->carrier,FB_CREDIT_TOUCHDOWN);football_live_fx(m,FB_FX_TOUCHDOWN,m->carrier,22,rd<0?-1:1);
        m->return_yards=(int16_t)limit(roundf((x-m->catch_x)*rd),-100,100);
        settle(m,football_rules_touchdown(&m->rules,(FootballSide)rs,seconds),FB_EVENT_TOUCHDOWN,x,seconds);
    }
    else conclude(m,rs,spot_for(rs,x),kickoff(m)?FB_EVENT_KICKOFF:FB_EVENT_PUNT,x);
}
static void begin_return(FootballLive *m,unsigned returner)
{
    unsigned ks=m->kick_side,rs=1-ks;
    m->catch_x=m->actors[returner].x;m->catch_time=m->elapsed;
    for(unsigned i=0;i<22;++i){FootballLiveActor *a=&m->actors[i];a->block_peer=255;a->block_time=0;a->block_cooldown=0;}
    m->rules.offense=(uint8_t)rs;m->stage=FB_STAGE_RETURN;m->carrier=(uint8_t)returner;m->target=(uint8_t)returner;
    m->caught=m->thrown=true;m->flying=false;m->ball_z=0;m->play=FB_RUN_TOSS;m->juke=0;m->cooldown=0;
    if(!football_live_cpu(m,(FootballSide)rs))m->controlled=(uint8_t)returner;
    else if(!football_live_cpu(m,(FootballSide)ks)){
        /* The human coverage player closest to the catch. */
        unsigned best=base(ks)+1;float near=1000;const FootballLiveActor *r=&m->actors[returner];
        for(unsigned i=base(ks)+1;i<base(ks)+11;++i){float g=gap(&m->actors[i],r->x,r->y);if(g<near){near=g;best=i;}}
        m->controlled=(uint8_t)best;
    }else m->controlled=FB_NO_ACTOR;
}
static void kick_punt(FootballLive *m,FootballLiveInput input)
{
    unsigned ks=m->kick_side,kb=base(ks);float d=dir(ks);const FootballLiveActor *k=&m->actors[kb];
    float length=limit(34+k->attributes.thp*.25f+jitter(m)*4,22,68);
    /* A punter near the goal line kicks for the corner of the field rather than the end zone, unless the kick gets away from him. */
    float room=(d>0?110-m->los:m->los-10)-7;
    if(length>room && room>=12 && football_live_random(m)%100>=12u)length=limit(room+jitter(m)*3,12,length);
    float aim=!football_live_cpu(m,(FootballSide)ks)?limit(input.y,-1,1)*12:0;
    float land_x=limit(m->los+d*length,0,120);
    /* The CPU punts for the corner once the ball would come down near the goal line. */
    if(football_live_cpu(m,(FootballSide)ks) && fabsf(land_x-(d>0?110:10))<20)aim=(k->y<26.67f?-1:1)*19;
    float error=(1-k->attributes.tha/110.0f)*5*jitter(m);
    m->from_x=k->x;m->from_y=k->y;m->to_x=land_x;m->to_y=26.67f+aim+error;
    m->flight=0;m->flight_time=2.2f+length*.022f;m->flying=true;m->stage=FB_STAGE_FLIGHT;
    m->kick_yards=(int16_t)roundf(length);
    if(!football_live_cpu(m,(FootballSide)ks)){
        /* The human coverage starts with the gunner on the ball's side. */
        unsigned gunner=kb+(m->to_y<26.67f?2:3);m->controlled=(uint8_t)gunner;
    }
}
static void kick_goal(FootballLive *m)
{
    unsigned ks=m->kick_side,kb=base(ks);float d=dir(ks);const FootballLiveActor *k=&m->actors[kb];
    m->kick_good=football_live_random(m)%100<(m->special==FB_SPECIAL_EXTRA_POINT?football_live_extra_point_chance(m):football_live_fg_chance(m));
    float spread=jitter(m);
    m->from_x=k->x;m->from_y=k->y;m->to_x=d>0?116:4;
    /* Good kicks split the posts; a miss goes wide, left or right. */
    m->to_y=26.67f+(m->kick_good?spread*1.8f:(spread>=0?1:-1)*(4.2f+fabsf(spread)*3));
    m->flight=0;m->flight_time=1.1f+football_live_fg_distance(m)*.012f;m->flying=true;m->stage=FB_STAGE_FLIGHT;
    m->kick_yards=(int16_t)football_live_fg_distance(m);
}
/* The kickoff goes deep, toward the returners; the human aims it across the field, the CPU mixes it up. */
static void kick_off(FootballLive *m,FootballLiveInput input)
{
    unsigned ks=m->kick_side,rs=1-ks,kb=base(ks),rb=base(rs);float d=dir(ks);const FootballLiveActor *k=&m->actors[kb];
    bool cpu=football_live_cpu(m,(FootballSide)ks);
    float length=limit(49+k->attributes.thp*.18f+jitter(m)*7,34,80);
    float aim=cpu?jitter(m)*11:limit(input.y,-1,1)*25;
    float error=(1-k->attributes.tha/110.0f)*4*jitter(m);
    m->from_x=m->los;m->from_y=26.67f;m->to_x=limit(m->los+d*length,0,120);m->to_y=26.67f+aim+error;
    m->flight=0;m->flight_time=2.6f+length*.02f;m->flying=true;m->stage=FB_STAGE_FLIGHT;
    m->kick_yards=(int16_t)roundf(length);
    /* The deep man closer to the ball goes to meet it; a human returner decides for himself unless he is nowhere near. */
    unsigned best=rb+9;float near=1000;
    for(unsigned rank=9;rank<11;++rank){
        unsigned i=rb+rank;float g=gap(&m->actors[i],m->to_x,m->to_y);
        if(i==m->controlled && g<12)continue;
        if(g<near){near=g;best=i;}
    }
    m->target=(uint8_t)best;
    if(!cpu){
        /* The human coverage starts with the man across from where the ball is coming down. */
        unsigned pick=kb+1;float across=1000;
        for(unsigned i=kb+1;i<kb+11;++i){float g=fabsf(m->actors[i].y-m->to_y);if(g<across){across=g;pick=i;}}
        m->controlled=(uint8_t)pick;
    }
}
static void punt_lands(FootballLive *m)
{
    unsigned ks=m->kick_side,rs=1-ks,rb=base(rs);float d=dir(ks);
    float lx=m->to_x,ly=m->to_y;m->flying=false;m->ball_x=lx;m->ball_y=ly;m->ball_z=0;
    if(ly<0 || ly>53.333f){conclude(m,rs,spot_for(rs,lx),FB_EVENT_OUT_OF_BOUNDS,lx);return;}
    if((d>0 && lx>=110) || (d<0 && lx<=10)){conclude(m,rs,20,FB_EVENT_TOUCHBACK,lx);return;}
    const FootballLiveActor *back=&m->actors[rb+9];
    if(gap(back,lx,ly)<=2.4f){
        float cover=1000;
        for(unsigned i=base(ks)+1;i<base(ks)+11;++i)cover=fminf(cover,gap(&m->actors[i],back->x,back->y));
        bool cpu=football_live_cpu(m,(FootballSide)rs);
        /* The CPU signals for a fair catch when a gunner is close, or when a return would start in its own shadow. */
        bool fair=cpu?(cover<6.5f && football_live_random(m)%100<90u) || (spot_for(rs,back->x)<=10 && football_live_random(m)%100<70u):m->fair_catch;
        if(fair){conclude(m,rs,spot_for(rs,back->x),FB_EVENT_FAIR_CATCH,back->x);return;}
        /* A fumbled catch: worse hands and a gunner on top of the returner make it likelier. */
        float muff=2+fmaxf(0,50-(int)back->attributes.cth)*.12f+(cover<2.2f?7:0);
        if(football_live_random(m)%1000<muff*10){
            bool kicker_recovers=football_live_random(m)%2==0;
            conclude(m,kicker_recovers?ks:rs,spot_for(kicker_recovers?ks:rs,back->x),FB_EVENT_MUFF,back->x);return;
        }
        begin_return(m,rb+9);return;
    }
    /* Nobody under it: it bounces on, and the kicking team downs it. */
    m->from_x=lx;m->from_y=ly;m->to_x=limit(lx+d*(3+football_live_random(m)%40/10.0f),0,120);m->to_y=ly;
    m->flight=0;m->flight_time=.9f;m->stage=FB_STAGE_ROLL;
}
/* A kickoff comes down: the end zone is a touchback to the 25, the sideline gives the ball at the 40, and the
 * receiving player nearest the spot catches it and brings it out. There is no fair catch on a kickoff. */
static void kickoff_lands(FootballLive *m)
{
    unsigned ks=m->kick_side,rs=1-ks,rb=base(rs);float d=dir(ks);
    float lx=m->to_x,ly=m->to_y;m->flying=false;m->ball_x=lx;m->ball_y=ly;m->ball_z=0;m->catch_time=m->elapsed;
    if(ly<0 || ly>53.333f){conclude(m,rs,40,FB_EVENT_OUT_OF_BOUNDS,lx);return;}
    if((d>0 && lx>=110) || (d<0 && lx<=10)){conclude(m,rs,25,FB_EVENT_TOUCHBACK,lx);return;}
    unsigned catcher=22;float near=2.8f;
    for(unsigned i=rb;i<rb+11;++i){float g=gap(&m->actors[i],lx,ly);if(g<near){near=g;catcher=i;}}
    if(catcher<22){
        const FootballLiveActor *c=&m->actors[catcher];float cover=1000;
        for(unsigned i=base(ks)+1;i<base(ks)+11;++i)cover=fminf(cover,gap(&m->actors[i],c->x,c->y));
        float muff=1+fmaxf(0,45-(int)c->attributes.cth)*.1f+(cover<2.2f?4:0);
        if(football_live_random(m)%1000<muff*10){
            bool kicker_recovers=football_live_random(m)%2==0;
            conclude(m,kicker_recovers?ks:rs,spot_for(kicker_recovers?ks:rs,c->x),FB_EVENT_MUFF,c->x);return;
        }
        begin_return(m,catcher);return;
    }
    m->from_x=lx;m->from_y=ly;m->to_x=limit(lx+d*(3+football_live_random(m)%40/10.0f),0,120);m->to_y=ly;
    m->flight=0;m->flight_time=.9f;m->stage=FB_STAGE_ROLL;
}
static void kick_lands(FootballLive *m)
{
    unsigned rs=1-m->kick_side,seconds=clock_seconds(m);
    m->flying=false;m->ball_x=m->to_x;m->ball_y=m->to_y;m->ball_z=0;
    if(m->special==FB_SPECIAL_EXTRA_POINT){
        football_live_conversion_done(m,m->kick_good?1:0,m->kick_good?FB_EVENT_EXTRA_POINT:FB_EVENT_EXTRA_POINT_MISSED);return;
    }
    if(m->kick_good){settle(m,football_rules_field_goal(&m->rules,seconds),FB_EVENT_FIELD_GOAL,m->to_x,seconds);return;}
    /* A miss gives the ball back where it was kicked from, or at the 20 for a long one. */
    int spot=100-m->rules.ball;if(spot<20)spot=20;
    conclude(m,rs,spot,FB_EVENT_FIELD_GOAL_MISSED,m->los);
}
void football_live_special_step(FootballLive *m,FootballLiveInput input)
{
    unsigned ks=m->kick_side,rs=1-ks,kb=base(ks),rb=base(rs);float d=dir(ks);
    bool punt=m->special==FB_SPECIAL_PUNT,off=kickoff(m),goal=goal_kick(m);
    bool cpu_kick=football_live_cpu(m,(FootballSide)ks),cpu_catch=football_live_cpu(m,(FootballSide)rs);
    const FootballLiveActor *kicker=&m->actors[kb];
    /* The human can cycle the player they steer once there is more than one worth steering. */
    if((input.pressed&FB_LIVE_SWITCH) && ((!cpu_kick && m->stage!=FB_STAGE_SNAP) || (!cpu_catch && !punt)))
        m->controlled=(uint8_t)((m->controlled+1)%11);
    if(!cpu_catch && punt && m->stage==FB_STAGE_FLIGHT && (input.pressed&FB_LIVE_ACTION))m->fair_catch=true;
    float intent[22][2];bool sprint[22];
    for(unsigned i=0;i<22;++i){intent[i][0]=intent[i][1]=0;sprint[i]=false;}
    float land_x=m->to_x,land_y=m->to_y;bool after=m->stage!=FB_STAGE_SNAP;
    /* Kickoff return team: the front line and the second line fall back into a wall in front of the catch. */
    static const float wall_x[9]={14,13,13,14,9,8,9,5,5};
    static const float wall_y[9]={-10,-3.5f,3.5f,10,-6.5f,0,6.5f,-17,17};
    static const float lane_y[11]={26.67f,4.5f,9.5f,14.5f,19.5f,24,29.5f,34.5f,39.5f,44.5f,49.2f};
    for(unsigned rank=0;rank<11;++rank){
        unsigned ki=kb+rank,ri=rb+rank;FootballLiveActor *k=&m->actors[ki],*r=&m->actors[ri];
        float *kt=intent[ki],*rt=intent[ri];
        if(off && !after){
            /* The run-up: the kicker steps to the ball, the line starts to move just before the foot meets it. */
            if(rank==0){if((m->los-k->x)*d>.9f)kt[0]=d*.5f;}
            else if(m->elapsed>=.6f){kt[0]=d;sprint[ki]=true;}
        }
        else if(ki==m->controlled){kt[0]=limit(input.x,-1,1);kt[1]=limit(input.y,-1,1);if(m->stage==FB_STAGE_SNAP && ki==kb)kt[0]=kt[1]=0;sprint[ki]=true;}
        else if(!after){
            /* Before the kick the line blocks the closest rushers; everyone else holds. */
            if(rank>=6 || (goal && rank>=2)){unsigned mark=football_live_block_target(m,ki);
                if(mark<22){float x=m->actors[mark].x-k->x,y=m->actors[mark].y-k->y,len=hypotf(x,y);if(len>.55f){x*=.55f/len;y*=.55f/len;}kt[0]=x;kt[1]=y;}}
        }else if(punt && rank>=1){
            /* Coverage: everyone runs down to where the ball is coming down, fanned out across the field. */
            kt[0]=land_x-k->x;kt[1]=land_y+(rank-5.5f)*2.4f-k->y;sprint[ki]=rank<=5;
        }else if(off && rank>=1){
            /* Kickoff coverage runs its lanes down the field, closing on the ball as it comes down. */
            kt[0]=land_x-d*4-k->x;kt[1]=lane_y[rank]*.65f+land_y*.35f-k->y;sprint[ki]=true;
        }
        if(ri==m->controlled){rt[0]=limit(input.x,-1,1);rt[1]=limit(input.y,-1,1);sprint[ri]=true;}
        else if(!after){
            bool rush=!off && (rank<4 || (goal && rank<9));
            if(rush){rt[0]=kicker->x-r->x;rt[1]=kicker->y-r->y;sprint[ri]=true;}
            else if(punt && (rank==7 || rank==8)){const FootballLiveActor *gunner=&m->actors[kb+(rank==7?2:3)];rt[0]=gunner->x-r->x;rt[1]=gunner->y-r->y;}
        }else if(off){
            if(ri==m->target && m->stage==FB_STAGE_FLIGHT){rt[0]=land_x-r->x;rt[1]=land_y-r->y;sprint[ri]=true;}
            else if(rank<9 && m->stage==FB_STAGE_FLIGHT){
                rt[0]=land_x-d*wall_x[rank]-r->x;rt[1]=limit(land_y+wall_y[rank],3,50)-r->y;sprint[ri]=true;
            }
        }else if(punt){
            if(rank==9){
                if(m->stage==FB_STAGE_FLIGHT){rt[0]=land_x-r->x;rt[1]=land_y-r->y;sprint[ri]=true;}
            }else{
                /* The rest of the return team looks for someone to block. */
                float near=1000;unsigned mark=22;
                for(unsigned j=1;j<11;++j){float g=gap(&m->actors[kb+j],r->x,r->y);if(g<near){near=g;mark=kb+j;}}
                if(mark<22){rt[0]=m->actors[mark].x-r->x;rt[1]=m->actors[mark].y-r->y;}
            }
        }
    }
    for(unsigned i=0;i<22;++i)football_live_move(&m->actors[i],intent[i][0],intent[i][1],sprint[i]);
    football_live_physics(m);
    football_live_update_facing(m);
    if(m->stage==FB_STAGE_SNAP){
        FootballLiveActor *k=&m->actors[kb];
        /* The ball waits on the tee for a kickoff; a punter or holder has it in his hands. */
        if(!off){m->ball_x=k->x;m->ball_y=k->y;m->ball_z=0;}
        /* A rusher who reaches the kicker before the foot meets the ball blocks it; the human dives with A. */
        for(unsigned i=rb;!off && i<rb+11;++i){bool dive=i==m->controlled && (input.pressed&FB_LIVE_ACTION);
            float reach=dive?2.5f:1.35f;
            if(gap(&m->actors[i],k->x,k->y)<reach && (!dive || football_live_random(m)%100<55u)){
                if(m->special==FB_SPECIAL_EXTRA_POINT)football_live_conversion_done(m,0,FB_EVENT_BLOCKED);
                else conclude(m,rs,spot_for(rs,k->x),FB_EVENT_BLOCKED,k->x);
                return;}
        }
        if(m->elapsed>=kick_time(m)){
            if(punt)kick_punt(m,input);else if(off)kick_off(m,input);else kick_goal(m);
        }
        return;
    }
    if(m->stage==FB_STAGE_FLIGHT){
        m->flight+=FB_LIVE_STEP;float t=limit(m->flight/m->flight_time,0,1);
        m->ball_x=m->from_x+(m->to_x-m->from_x)*t;m->ball_y=m->from_y+(m->to_y-m->from_y)*t;
        m->ball_z=4*t*(1-t)*(off?14.0f:punt?11.0f:6.0f);
        if(t>=1){if(punt)punt_lands(m);else if(off)kickoff_lands(m);else kick_lands(m);}
        return;
    }
    if(m->stage==FB_STAGE_ROLL){
        m->flight+=FB_LIVE_STEP;float t=limit(m->flight/m->flight_time,0,1);
        m->ball_x=m->from_x+(m->to_x-m->from_x)*t*(2-t);m->ball_y=m->from_y;m->ball_z=0;
        if(t>=1){
            if((d>0 && m->ball_x>=110) || (d<0 && m->ball_x<=10))conclude(m,rs,off?25:20,FB_EVENT_TOUCHBACK,m->ball_x);
            else conclude(m,rs,spot_for(rs,m->ball_x),off?FB_EVENT_KICKOFF:FB_EVENT_PUNT,m->ball_x);
        }
    }
}
void football_live_focus(const FootballLive *m,float *x,float *y,float *vx)
{
    unsigned actor=m->controlled<22?m->controlled:m->carrier;
    *x=m->actors[actor].x;*y=m->actors[actor].y;*vx=m->actors[actor].vx;
    /* The human must see the player they steer under a kick, including its
     * flight and bounce. Test this before the general flying-ball view. */
    bool returner_view=(m->special==FB_SPECIAL_KICKOFF || m->special==FB_SPECIAL_PUNT)
        && m->controlled<22 && !football_live_cpu(m,(FootballSide)(1-m->kick_side));
    if(returner_view && m->phase!=FB_LIVE_DEAD && m->phase!=FB_LIVE_END)return;
    if(m->flying){*x=m->ball_x;*y=m->ball_y;*vx=0;return;}
    if(!m->special)return;
    if(m->stage==FB_STAGE_SNAP && m->phase!=FB_LIVE_DEAD && !returner_view){const FootballLiveActor *k=&m->actors[base(m->kick_side)];*x=k->x;*y=k->y;*vx=0;}
    else if(m->stage==FB_STAGE_ROLL || m->phase==FB_LIVE_DEAD){*x=m->ball_x;*y=m->ball_y;*vx=0;}
}
