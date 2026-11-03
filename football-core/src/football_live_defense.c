#include "football_live_systems.h"
#include <math.h>
static float clamp(float v,float a,float b){return v<a?a:v>b?b:v;}
static void overload_lanes(FootballLive *m,unsigned def,unsigned off);
void football_live_defense_setup(FootballLive *m)
{
    if(!m)return;
    unsigned off=m->rules.offense==FB_HOME?0:11,def=11-off;
    float dir=off? -1:1;
    const FootballPlayDefinition *play=football_playbook_get(m->play);
    bool passing=play && play->type==FB_PLAY_PASS;
    bool zone=football_defense_is_zone(m->defense);
    /* DL rush, LB covers WR3/RB/TE, corners cover WR1/WR2, two deep safeties.
     * A five-man blitz rotates the free safety onto the RB and keeps one high. */
    static const uint8_t man[11]={255,255,255,255,4,1,5,2,3,255,255};
    /* Hook defenders stay inside; flat corners own the two sidelines.
     * Starting the corners in the seams made them duplicate the inside marks. */
    static const float depth[11]={2,2,2,2,7,8,7,5,5,18,18};
    static const float lane[11]={20,24,29,33,15,26.67f,38,5,48,18,36};
    for(unsigned rank=0;rank<11;++rank){
        m->coverage[rank]=man[rank]==255?255:(uint8_t)(off+man[rank]);
        m->zone_x[rank]=clamp(m->los+dir*depth[rank],1,119);m->zone_y[rank]=lane[rank];
        if(m->defense==FB_DEF_BLITZ){if(rank==5)m->coverage[rank]=255;if(rank==9)m->coverage[rank]=(uint8_t)(off+1);}
        if(m->defense==FB_DEF_COVER0){if(rank==9)m->coverage[rank]=(uint8_t)(off+4);if(rank==10)m->coverage[rank]=(uint8_t)(off+1);}
        if(football_defense_is_rusher(m->defense,rank))m->coverage[rank]=255;
        if(m->defense==FB_DEF_COVER3 || m->defense==FB_DEF_ZONE_BLITZ){
            if(rank==7 || rank==8){m->zone_x[rank]=m->los+dir*18;m->zone_y[rank]=rank==7?5:48;}
            if(rank==9){m->zone_x[rank]=m->los+dir*24;m->zone_y[rank]=26.67f;}
            if(m->defense==FB_DEF_COVER3){if(rank==4)m->zone_y[rank]=5;if(rank==5)m->zone_y[rank]=20;if(rank==6)m->zone_y[rank]=34;if(rank==10){m->zone_x[rank]=m->los+dir*7;m->zone_y[rank]=48;}}
            else {if(rank==5)m->zone_y[rank]=26.67f;if(rank==6)m->zone_y[rank]=42;if(rank==10){m->zone_x[rank]=m->los+dir*7;m->zone_y[rank]=12;}}
        }
        if(m->defense==FB_DEF_COVER4){if(rank==4)m->zone_y[rank]=12;if(rank==6)m->zone_y[rank]=41;if(rank==7 || rank==8)m->zone_x[rank]=m->los+dir*16;if(rank>=9)m->zone_x[rank]=m->los+dir*22;}
        if(m->defense==FB_DEF_COVER1_ROBBER){if(rank==9)m->zone_y[rank]=26.67f;if(rank==10){m->zone_x[rank]=m->los+dir*8;m->zone_y[rank]=26.67f;}}
        m->zone_x[rank]=clamp(m->zone_x[rank],1,119);
        if(rank>=4){
            FootballLiveActor *a=&m->actors[def+rank];unsigned target=m->coverage[rank];
            if(!zone && target!=255){
                a->x=m->los+dir*(rank==7 || rank==8?2:5);
                float leverage=m->snap_y[target-off]<26.67f?1.f:-1.f;
                a->y=clamp(m->actors[target].y+(passing?leverage*(FB_BODY_DIAMETER+.4f):0),2,51);
            }else if(zone || m->defense==FB_DEF_COVER1_ROBBER){a->x=m->zone_x[rank];a->y=m->zone_y[rank];}
            if((m->defense==FB_DEF_ZONE_BLITZ || m->defense==FB_DEF_COVER0) && football_defense_is_rusher(m->defense,rank)){a->x=m->los+dir*3;a->y=rank==4?18:35;}
        }
        if(football_defense_squat_mask(m->defense)&(1u<<rank))m->actors[def+rank].x=m->los+dir*FB_LIVE_SQUAT_START;
    }
    if(football_defense_matches_overload(m->defense))overload_lanes(m,def,off);
    /* The goal line squeezes the defense: nobody starts, or drops, deeper than a couple of yards into the end zone he is defending (the
     * deep players 2.5 yd past the goal line, the rest 1 yd), instead of at the depths of an open field, which put the safeties at the
     * back line (and, in the man calls, outside the field) from the 3-yard line. Nothing changes until the line of scrimmage is within
     * about 21 yd of the goal line. */
    for(unsigned rank=0;rank<11;++rank){
        FootballLiveActor *a=&m->actors[def+rank];
        float cap=football_live_goal_room(m)+((football_defense_deep_mask(m->defense)&(1u<<rank))?FB_LIVE_DEEP_BEHIND:FB_LIVE_SHALLOW_BEHIND);
        if((a->x-m->los)*dir>cap)a->x=m->los+dir*cap;
        if((m->zone_x[rank]-m->los)*dir>cap)m->zone_x[rank]=m->los+dir*cap;
    }
}
/* Overload: in Cover 2 and Cover 3 three receivers on one side (Trips, Tight) met two underneath defenders and the third was alone (the slot
 * was open in 86% and 84% of the Trips snaps). The underneath defenders (the flats and hooks, not the deep players, who drop 16 yd or more) closest to that
 * sideline now line up on the receivers' spots, one each, nearest the sideline first. Two and two (Spread, Jet): the fixed lanes. */
static void overload_lanes(FootballLive *m,unsigned def,unsigned off)
{
    unsigned low=0,high=0;
    for(unsigned rank=2;rank<=5;++rank){if(m->snap_y[rank]<26.667f)++low;else ++high;}
    if(low<3 && high<3)return;
    bool at_low=low>=3;
    float spot[4];unsigned n=0;
    for(unsigned rank=2;rank<=5;++rank)if((m->snap_y[rank]<26.667f)==at_low)spot[n++]=m->snap_y[rank];
    unsigned under[7],k=0;
    for(unsigned rank=4;rank<11;++rank)
        if((m->zone_x[rank]-m->los)*(off?-1.f:1.f)<=8.5f)under[k++]=rank; /* flats and hooks: the deep players drop to 16 yd and more */
    /* nearest the strong sideline first, receivers likewise */
    for(unsigned i=1;i<k;++i)for(unsigned j=i;j>0 && (at_low?m->zone_y[under[j]]<m->zone_y[under[j-1]]:m->zone_y[under[j]]>m->zone_y[under[j-1]]);--j){unsigned t=under[j];under[j]=under[j-1];under[j-1]=t;}
    for(unsigned i=1;i<n;++i)for(unsigned j=i;j>0 && (at_low?spot[j]<spot[j-1]:spot[j]>spot[j-1]);--j){float t=spot[j];spot[j]=spot[j-1];spot[j-1]=t;}
    for(unsigned i=0;i<n && i<k;++i){
        float y=clamp(spot[i],4.f,49.333f);
        m->zone_y[under[i]]=y;m->actors[def+under[i]].y=y;
    }
}
/* Yards from the line of scrimmage to the goal line the offense attacks. */
float football_live_goal_room(const FootballLive *m){return m->rules.offense==FB_HOME?110.f-m->los:m->los-10.f;}
void football_live_defense_intent(FootballLive *m,unsigned actor,float *x,float *y)
{
    unsigned off=m->rules.offense==FB_HOME?0:11,def=11-off,rank=actor-def;
    float dir=off?-1:1;FootballLiveActor *a=&m->actors[actor];const FootballLiveActor *carrier=&m->actors[m->carrier];
    bool running=!m->flying && (m->caught || (m->carrier!=off && m->elapsed>=.4f) || (m->carrier==off && (carrier->x-m->los)*dir>.5f));
    bool rushing=football_defense_is_rusher(m->defense,rank);
    bool deep=(football_defense_deep_mask(m->defense)&(1u<<rank))!=0;
    /* How far downfield a receiver is before the deep players take him over from the underneath ones: 8 yd in the open field, less when the
     * end zone is all the room there is (a receiver 4 yd deep at the 3 is already in the end zone). */
    float deep_min=fminf(8.f,football_live_goal_room(m)+1.f);
    float tx=a->x,ty=a->y;
    if(running || rushing){
        /* Pursue where the carrier will be, not where he is: lead grows with the distance to close. */
        float lead=clamp(hypotf(carrier->x-a->x,carrier->y-a->y)/(4.3f+a->attributes.spd*.045f),.1f,1.1f);
        tx=carrier->x+carrier->vx*lead;ty=carrier->y+carrier->vy*lead;
    }
    else if(football_defense_is_zone(m->defense)){
        tx=m->zone_x[rank];ty=m->zone_y[rank];
        unsigned threat=255;float best=100000;bool squat=false;
        for(unsigned receiver=1;receiver<=5;++receiver){const FootballLiveActor *r=&m->actors[off+receiver];
            float depth=(r->x-m->los)*dir,lateral=fabsf(r->y-m->zone_y[rank]);
            bool inside=deep ? depth>=deep_min && lateral<=16 : depth>=-3 && depth<=16 && lateral<=10;
            if(!inside)continue;
            float score=lateral*2+fabsf(r->x-m->zone_x[rank]);
            if(score<best){best=score;threat=off+receiver;}
        }
        /* A corner who starts close to the line and has nobody deep in his strip keeps the shorter receiver there, like an underneath
         * defender, until that receiver goes deep (and then the deep rule above takes him). */
        if(threat==255 && (football_defense_squat_mask(m->defense)&(1u<<rank))){
            for(unsigned receiver=1;receiver<=5;++receiver){const FootballLiveActor *r=&m->actors[off+receiver];
                float depth=(r->x-m->los)*dir,lateral=fabsf(r->y-m->zone_y[rank]);
                if(depth<-3 || depth>=deep_min || lateral>10)continue;
                float score=lateral*2+fabsf(r->x-m->zone_x[rank]);
                if(score<best){best=score;threat=off+receiver;squat=true;}
            }
        }
        m->coverage[rank]=(uint8_t)threat;
        if(threat!=255){const FootballLiveActor *r=&m->actors[threat];bool far=deep && !squat;
            /* Coverage can close a window without targeting the receiver's solid body. */
            tx=r->x+dir*(far?2:FB_BODY_DIAMETER+.4f);ty=clamp(r->y+r->vy*.15f,m->zone_y[rank]-(far?15:9),m->zone_y[rank]+(far?15:9));
            if(!far)tx=m->los+dir*clamp((tx-m->los)*dir,0,18);
        }
        /* Break on a thrown pass only in this defender's responsibility. */
        if(m->flying && fabsf(m->to_y-m->zone_y[rank])<=(deep?17:11) && ((m->to_x-m->los)*dir<=18 || deep)) {tx=m->to_x;ty=m->to_y;}
    }else if(m->coverage[rank]!=255){
        unsigned mark=m->coverage[rank];const FootballLiveActor *r=&m->actors[mark];
        /* Keep the coverage target outside the receiver's solid body.
         * An .8-yard lead is inside the 1.3-yard diameter and makes the DB
         * run back into the receiver instead of carrying the route downfield. */
        const FootballPlayDefinition *play=football_playbook_get(m->play);bool passing=play && play->type==FB_PLAY_PASS;
        tx=r->x+dir*(passing?FB_BODY_DIAMETER+.4f:.8f)+r->vx*.12f;
        ty=clamp(r->y+(passing?(m->snap_y[mark-off]<26.67f?1.f:-1.f)*(FB_BODY_DIAMETER+.4f):0)+r->vy*.12f,2,51);
        if(m->flying && m->target==mark){tx=m->to_x;ty=m->to_y;}
    }else {
        /* One-high help can read both sides; the robber stays underneath.
         * Choose a threat by depth/leverage instead of the last roster slot. */
        tx=m->zone_x[rank];ty=m->zone_y[rank];float best=-1e9f;
        bool robber=m->defense==FB_DEF_COVER1_ROBBER && rank==10;
        bool one_high=m->defense==FB_DEF_COVER1_ROBBER || m->defense==FB_DEF_BLITZ;
        for(unsigned receiver=1;receiver<=5;++receiver){const FootballLiveActor *r=&m->actors[off+receiver];
            float depth=(r->x-m->los)*dir;
            bool half=one_high || (rank==9?r->y<=27:r->y>=26);
            if(!half || depth<(robber?0:deep_min) || (robber&&depth>18))continue;
            float value=depth*.5f-fabsf(r->y-m->zone_y[rank])*.2f;
            if(value>best){best=value;tx=r->x+dir*(robber?FB_BODY_DIAMETER+.4f:3);ty=r->y;}
        }
        if(m->flying && (one_high || (rank==9?m->to_y<=29:m->to_y>=24))){tx=m->to_x;ty=m->to_y;}
    }
    /* Play action: the linebackers (in zone; in man the back's cover already follows him) and, half as far, the safeties step toward the
     * fake before they drop. */
    if(!rushing && football_live_fake_active(m)){
        float bite=rank>=4 && rank<=6?(football_defense_is_zone(m->defense)?1.f:0.f):rank>=9?.5f:0.f;
        if(bite>0){
            float fx,fy;football_live_fake_point(m,&fx,&fy);
            float bx=rank<=6?m->los+dir*1.5f:m->zone_x[rank]-dir*4,by=rank<=6?a->y+(fy-a->y)*.5f:m->zone_y[rank];
            tx+=(bx-tx)*bite;ty+=(by-ty)*bite;
        }
    }
    /* Until the ball is handed off the rush cannot tell a run from a pass: the line reads the QB and the backfield, then commits.
     * Only the draw is built to fool it (it looks like a pass and the rush keeps coming). */
    if(m->hand_to<22 && rushing){
        const FootballPlayDefinition *call=football_playbook_get(m->play);
        if(call && call->id!=FB_RUN_DRAW){float len=hypotf(tx-a->x,ty-a->y);if(len>0){float k=.4f;*x=(tx-a->x)/len*k;*y=(ty-a->y)/len*k;return;}}
    }
    *x=tx-a->x;*y=ty-a->y;
}
