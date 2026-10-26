#include "football_live_systems.h"
#include <math.h>
/* Punts and field goals on the live field. The kicking team is rules.offense until the ball is caught; a
 * return then flips rules.offense to the receiving team, so its carrier, blockers and the kicking team's pursuit
 * are the ordinary run-play systems. Every rules change goes through one of three football_rules_* calls. */
#define PUNT_TIME 1.45f
#define FG_TIME 1.05f
#define FG_MAX_DISTANCE 62u
static float limit(float v,float a,float b){return v<a?a:v>b?b:v;}
static unsigned base(unsigned side){return side==FB_HOME?0:11;}
static float dir(unsigned side){return side==FB_HOME?1:-1;}
static float jitter(FootballLive *m){return ((int)(football_live_random(m)%2001)-1000)/1000.0f;}
static float gap(const FootballLiveActor *a,float x,float y){return hypotf(a->x-x,a->y-y);}
/* Yards from `side`'s own goal line (1..99) of a field x. */
static int spot_for(unsigned side,float x)
{
    int s=(int)roundf(side==FB_HOME?x-10:110-x);
    return s<1?1:s>99?99:s;
}
unsigned football_live_fg_distance(const FootballLive *m){return m?(unsigned)(100-m->rules.ball)+17u:0;}
unsigned football_live_fg_chance(const FootballLive *m)
{
    if(!m)return 0;
    const FootballLiveActor *k=&m->actors[base(m->rules.offense)];
    float distance=(float)football_live_fg_distance(m);
    float p=.98f-fmaxf(0,distance-20)*.0155f+((int)k->attributes.thp-50)*.004f+((int)k->attributes.tha-50)*.003f;
    return (unsigned)limit(p*100,3,99);
}
bool football_live_special_allowed(const FootballLive *m,FootballSpecial kind)
{
    if(!m || m->phase!=FB_LIVE_CALL || m->rules.finished)return false;
    if(kind==FB_SPECIAL_PUNT)return true;
    return kind==FB_SPECIAL_FIELD_GOAL && football_live_fg_distance(m)<=FG_MAX_DISTANCE;
}
void football_live_special_arrange(FootballLive *m)
{
    unsigned ks=m->rules.offense,rs=1-ks,kb=base(ks),rb=base(rs);float d=dir(ks);
    bool punt=m->special==FB_SPECIAL_PUNT;
    m->kick_side=(uint8_t)ks;m->stage=FB_STAGE_SNAP;m->fair_catch=false;m->kick_good=false;m->kick_yards=0;m->event=FB_EVENT_NONE;
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
    for(unsigned i=0;i<11;++i){
        FootballLiveActor *k=&m->actors[kb+i],*r=&m->actors[rb+i];
        k->x=m->los+d*(punt?punt_kx[i]:fg_kx[i]);k->y=punt?punt_ky[i]:fg_ky[i];
        r->x=m->los+d*(punt?punt_rx[i]:fg_rx[i]);r->y=punt?punt_ry[i]:fg_ry[i];
        k->vx=k->vy=r->vx=r->vy=0;k->facing=(int8_t)d;r->facing=(int8_t)-d;
    }
    /* The returner waits deep, but not past the back line. A field-goal safety stands near the end line. */
    FootballLiveActor *back=&m->actors[rb+9],*second=&m->actors[rb+10];
    if(punt){
        back->x=d>0?fminf(m->los+42,105):fmaxf(m->los-42,15);back->y=26.67f;
        second->x=back->x-d*9;second->y=40;
    }else{
        back->x=d>0?fminf(m->los+30,112):fmaxf(m->los-30,8);back->y=26.67f;
        second->x=d>0?fminf(m->los+12,108):fmaxf(m->los-12,12);second->y=26.67f;
    }
    /* The kicker holds the ball. The human steers a returner, or a rusher on a field goal, and kicks by pressing A. */
    m->carrier=(uint8_t)kb;m->target=(uint8_t)kb;
    bool human_kicks=!football_live_cpu(m,(FootballSide)ks),human_receives=!football_live_cpu(m,(FootballSide)rs);
    m->controlled=human_kicks?(uint8_t)kb:human_receives?(uint8_t)(rb+(punt?9:5)):FB_NO_ACTOR;
    m->ball_x=m->actors[kb].x;m->ball_y=m->actors[kb].y;m->ball_z=0;
}
/* The ball is dead and the rules have been told: banner, snap count and the next phase. */
static void settle(FootballLive *m,bool ok,FootballLiveEvent event,float final_x,unsigned seconds)
{
    int net=(int)roundf((final_x-m->los)*dir(m->kick_side));
    m->result=(FootballResolvedPlay){FB_RESULT_RUN,(int16_t)limit((float)net,-100,100),(uint8_t)seconds};
    if(!ok){m->rules.finished=true;m->rules.winner=FB_AWAY;}
    m->event=(uint8_t)event;m->flying=false;m->ball_z=0;m->caught=false;
    ++m->snaps;m->phase=m->rules.finished?FB_LIVE_END:FB_LIVE_DEAD;m->dead_time=0;
}
static unsigned clock_seconds(const FootballLive *m){return (unsigned)limit(ceilf(m->elapsed)+2,1,30);}
/* Possession goes to `receiver` at `spot` yards from its own goal. */
static void conclude(FootballLive *m,unsigned receiver,int spot,FootballLiveEvent event,float final_x)
{
    unsigned seconds=clock_seconds(m);
    settle(m,football_rules_possession(&m->rules,(FootballSide)receiver,(unsigned)limit((float)spot,1,99),seconds),event,final_x,seconds);
}
void football_live_special_finish(FootballLive *m,FootballResultKind kind,float x)
{
    unsigned ks=m->kick_side,rs=1-ks;float rd=dir(rs);unsigned seconds=clock_seconds(m);
    bool score=(rd>0 && x>=110) || (rd<0 && x<=10);
    if(kind==FB_RESULT_FUMBLE)conclude(m,ks,spot_for(ks,x),FB_EVENT_FUMBLE,x);
    else if(score)settle(m,football_rules_touchdown(&m->rules,(FootballSide)rs,seconds),FB_EVENT_TOUCHDOWN,x,seconds);
    else conclude(m,rs,spot_for(rs,x),FB_EVENT_PUNT,x);
}
static void begin_return(FootballLive *m,unsigned returner)
{
    unsigned ks=m->kick_side,rs=1-ks;
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
    m->kick_good=football_live_random(m)%100<football_live_fg_chance(m);
    float spread=jitter(m);
    m->from_x=k->x;m->from_y=k->y;m->to_x=d>0?116:4;
    /* Good kicks split the posts; a miss goes wide, left or right. */
    m->to_y=26.67f+(m->kick_good?spread*1.8f:(spread>=0?1:-1)*(4.2f+fabsf(spread)*3));
    m->flight=0;m->flight_time=1.1f+football_live_fg_distance(m)*.012f;m->flying=true;m->stage=FB_STAGE_FLIGHT;
    m->kick_yards=(int16_t)football_live_fg_distance(m);
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
static void kick_lands(FootballLive *m)
{
    unsigned rs=1-m->kick_side,seconds=clock_seconds(m);
    m->flying=false;m->ball_x=m->to_x;m->ball_y=m->to_y;m->ball_z=0;
    if(m->kick_good){settle(m,football_rules_field_goal(&m->rules,seconds),FB_EVENT_FIELD_GOAL,m->to_x,seconds);return;}
    /* A miss gives the ball back where it was kicked from, or at the 20 for a long one. */
    int spot=100-m->rules.ball;if(spot<20)spot=20;
    conclude(m,rs,spot,FB_EVENT_FIELD_GOAL_MISSED,m->los);
}
void football_live_special_step(FootballLive *m,FootballLiveInput input)
{
    unsigned ks=m->kick_side,rs=1-ks,kb=base(ks),rb=base(rs);float d=dir(ks);
    bool punt=m->special==FB_SPECIAL_PUNT;
    bool cpu_kick=football_live_cpu(m,(FootballSide)ks),cpu_catch=football_live_cpu(m,(FootballSide)rs);
    const FootballLiveActor *kicker=&m->actors[kb];
    /* The human can cycle the player they steer once there is more than one worth steering. */
    if((input.pressed&FB_LIVE_SWITCH) && ((!cpu_kick && m->stage!=FB_STAGE_SNAP) || (!cpu_catch && !punt)))
        m->controlled=(uint8_t)((m->controlled+1)%11);
    if(!cpu_catch && punt && m->stage==FB_STAGE_FLIGHT && (input.pressed&FB_LIVE_ACTION))m->fair_catch=true;
    float intent[22][2];bool sprint[22];
    for(unsigned i=0;i<22;++i){intent[i][0]=intent[i][1]=0;sprint[i]=false;}
    float land_x=m->to_x,land_y=m->to_y;bool after=m->stage!=FB_STAGE_SNAP;
    for(unsigned rank=0;rank<11;++rank){
        unsigned ki=kb+rank,ri=rb+rank;FootballLiveActor *k=&m->actors[ki],*r=&m->actors[ri];
        float *kt=intent[ki],*rt=intent[ri];
        if(ki==m->controlled){kt[0]=limit(input.x,-1,1);kt[1]=limit(input.y,-1,1);if(m->stage==FB_STAGE_SNAP && ki==kb)kt[0]=kt[1]=0;sprint[ki]=true;}
        else if(!after){
            /* Before the kick the line blocks the closest rushers; everyone else holds. */
            if(rank>=6 || (!punt && rank>=2)){unsigned mark=football_live_block_target(m,ki);
                if(mark<22){float x=m->actors[mark].x-k->x,y=m->actors[mark].y-k->y,len=hypotf(x,y);if(len>.55f){x*=.55f/len;y*=.55f/len;}kt[0]=x;kt[1]=y;}}
        }else if(punt && rank>=1){
            /* Coverage: everyone runs down to where the ball is coming down, fanned out across the field. */
            kt[0]=land_x-k->x;kt[1]=land_y+(rank-5.5f)*2.4f-k->y;sprint[ki]=rank<=5;
        }
        if(ri==m->controlled){rt[0]=limit(input.x,-1,1);rt[1]=limit(input.y,-1,1);sprint[ri]=true;}
        else if(!after){
            bool rush=rank<4 || (!punt && rank<9);
            if(rush){rt[0]=kicker->x-r->x;rt[1]=kicker->y-r->y;sprint[ri]=true;}
            else if(punt && (rank==7 || rank==8)){const FootballLiveActor *gunner=&m->actors[kb+(rank==7?2:3)];rt[0]=gunner->x-r->x;rt[1]=gunner->y-r->y;}
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
        m->ball_x=k->x;m->ball_y=k->y;m->ball_z=0;
        /* A rusher who reaches the kicker before the foot meets the ball blocks it; the human dives with A. */
        for(unsigned i=rb;i<rb+11;++i){bool dive=i==m->controlled && (input.pressed&FB_LIVE_ACTION);
            float reach=dive?2.5f:1.35f;
            if(gap(&m->actors[i],k->x,k->y)<reach && (!dive || football_live_random(m)%100<55u)){
                conclude(m,rs,spot_for(rs,k->x),FB_EVENT_BLOCKED,k->x);return;}
        }
        if(m->elapsed>=(punt?PUNT_TIME:FG_TIME)){
            if(punt)kick_punt(m,input);else kick_goal(m);
        }
        return;
    }
    if(m->stage==FB_STAGE_FLIGHT){
        m->flight+=FB_LIVE_STEP;float t=limit(m->flight/m->flight_time,0,1);
        m->ball_x=m->from_x+(m->to_x-m->from_x)*t;m->ball_y=m->from_y+(m->to_y-m->from_y)*t;
        m->ball_z=4*t*(1-t)*(punt?11.0f:6.0f);
        if(t>=1){if(punt)punt_lands(m);else kick_lands(m);}
        return;
    }
    if(m->stage==FB_STAGE_ROLL){
        m->flight+=FB_LIVE_STEP;float t=limit(m->flight/m->flight_time,0,1);
        m->ball_x=m->from_x+(m->to_x-m->from_x)*t*(2-t);m->ball_y=m->from_y;m->ball_z=0;
        if(t>=1){
            if((d>0 && m->ball_x>=110) || (d<0 && m->ball_x<=10))conclude(m,rs,20,FB_EVENT_TOUCHBACK,m->ball_x);
            else conclude(m,rs,spot_for(rs,m->ball_x),FB_EVENT_PUNT,m->ball_x);
        }
    }
}
void football_live_focus(const FootballLive *m,float *x,float *y,float *vx)
{
    unsigned actor=m->controlled<22?m->controlled:m->carrier;
    *x=m->actors[actor].x;*y=m->actors[actor].y;*vx=m->actors[actor].vx;
    if(m->flying){*x=m->ball_x;*y=m->ball_y;*vx=0;return;}
    if(!m->special)return;
    if(m->stage==FB_STAGE_SNAP && m->phase!=FB_LIVE_DEAD){const FootballLiveActor *k=&m->actors[base(m->kick_side)];*x=k->x;*y=k->y;*vx=0;}
    else if(m->stage==FB_STAGE_ROLL || m->phase==FB_LIVE_DEAD){*x=m->ball_x;*y=m->ball_y;*vx=0;}
}
