#include "football_live_systems.h"
#include <math.h>
/* The CPU's thinking, in one place: what to call, how a carrier picks a lane, when the QB throws, and
 * how a receiver gets open. Everything reads the live state and consumes only the match's own RNG. */
static float clampf(float v,float a,float b){return v<a?a:v>b?b:v;}
static float gap(const FootballLiveActor *a,const FootballLiveActor *b){return hypotf(a->x-b->x,a->y-b->y);}
static unsigned off_base(const FootballLive *m){return m->rules.offense==FB_HOME?0:11;}
static float dir_of(const FootballLive *m){return m->rules.offense==FB_HOME?1:-1;}
FootballPlayId football_live_ai_call(FootballLive *m)
{
    const FootballRules *r=&m->rules;unsigned me=r->offense,to_go=football_rules_distance(r);
    int lead=(int)r->score[me]-(int)r->score[1-me];
    bool late=r->quarter>=4 && (r->quarter>4 || r->seconds_left<=60);
    float weight[FB_PLAY_COUNT],total=0;
    for(unsigned id=0;id<FB_PLAY_COUNT;++id){
        const FootballPlayDefinition *p=football_playbook_get((FootballPlayId)id);
        bool pass=p->type==FB_PLAY_PASS,deep=id==FB_PASS_VERTICALS || id==FB_PASS_POST;
        float v=pass?10:16;
        /* Down and distance: pound it on short yardage, throw it when the sticks are far. */
        if(to_go<=2)v*=pass?.55f:2.6f;
        else if(to_go>=9)v*=pass?2.0f:id==FB_RUN_DRAW?1.2f:.6f;
        else if(to_go>=6)v*=pass?1.4f:.85f;
        if(r->down>=3 && to_go>=5)v*=pass?1.6f:id==FB_RUN_DRAW?1:.5f;
        if(to_go<=5 && deep)v*=.8f;
        /* Field position: no room for long routes near the goal, no risky pitches from the shadow of it. */
        if(r->ball>=88 && deep)v*=.2f;
        if(r->ball>=88 && id==FB_PASS_SLANTS)v*=1.3f;
        if(r->ball>=96 && !pass)v*=1.4f;
        if(r->ball<=8 && (deep || id==FB_RUN_TOSS))v*=.3f;
        /* Late in the game: chase the score, or run the clock. */
        if(late && lead<0)v*=pass?(lead<=-8?3.f:2.f):.6f;
        if(late && lead>0)v*=pass?.5f:(lead>=8?3.f:1.8f);
        /* Variety: a play it just ran, or ran twice back, is a poor call. */
        for(unsigned k=0;k<3;++k)if(m->recent[k]==id+1)v*=k==0?.12f:.4f;
        weight[id]=v;total+=v;
    }
    float pick=(football_live_random(m)%10000)/10000.0f*total;
    unsigned id=0;
    for(;id<FB_PLAY_COUNT-1;++id){if(pick<weight[id])break;pick-=weight[id];}
    m->recent[2]=m->recent[1];m->recent[1]=m->recent[0];m->recent[0]=(uint8_t)(id+1);
    return (FootballPlayId)id;
}
FootballSpecial football_live_ai_special(const FootballLive *m)
{
    const FootballRules *r=&m->rules;
    if(r->down<4)return FB_SPECIAL_NONE;
    unsigned me=r->offense,to_go=football_rules_distance(r);
    int lead=(int)r->score[me]-(int)r->score[1-me];
    bool overtime=r->quarter>=5,last_minutes=r->quarter>=4 && !overtime && r->seconds_left<=150,desperate=r->quarter>=4 && !overtime && r->seconds_left<=60;
    /* The kicker is the QB slot of this offense: a stronger leg kicks from further out. */
    const FootballLiveActor *kicker=&m->actors[off_base(m)];
    unsigned reach=(unsigned)(40+kicker->attributes.thp*.18f);
    bool kick_ok=football_live_special_allowed(m,FB_SPECIAL_FIELD_GOAL) && football_live_fg_distance(m)<=reach;
    unsigned roll=(unsigned)((m->rng>>7)%100u);
    /* Time is running out and a field goal does not help: only a touchdown will. */
    bool need_touchdown=last_minutes && lead<-3 && !(desperate && lead>=-3);
    if(overtime || need_touchdown){
        if(kick_ok && !(need_touchdown && lead<-3))return FB_SPECIAL_FIELD_GOAL;
        return FB_SPECIAL_NONE;
    }
    /* Inside the red zone with a yard to go, take the touchdown more often than not. */
    if(kick_ok && to_go<=1 && r->ball>=96 && roll<60u)return FB_SPECIAL_NONE;
    if(kick_ok)return FB_SPECIAL_FIELD_GOAL;
    /* Short yardage past midfield: go for it. Nearer the goal line is worth even more. */
    if(to_go<=1 && r->ball>=45 && roll<(r->ball>=70?70u:45u))return FB_SPECIAL_NONE;
    if(to_go<=3 && r->ball>=65 && roll<35u)return FB_SPECIAL_NONE;
    /* Behind late and short of the goal: punting only hands the ball over for good. */
    if(last_minutes && lead<0 && r->ball>=40 && to_go<=8)return FB_SPECIAL_NONE;
    return FB_SPECIAL_PUNT;
}
bool football_live_ai_goes_for_two(const FootballLive *m)
{
    const FootballRules *r=&m->rules;unsigned me=m->after_side;
    /* The score includes the touchdown just scored. Late, go for two where it changes what the other team needs. */
    int lead=(int)r->score[me]-(int)r->score[1-me];
    if(r->quarter>=4)return lead==-2 || lead==-5 || lead==-8 || lead==-10 || lead==-13;
    /* Earlier it is a rare gamble, a little likelier when far behind. */
    return (m->rng>>9)%100u<(lead<=-9?12u:4u);
}
FootballDefenseId football_live_ai_defense(FootballLive *m)
{
    const FootballRules *r=&m->rules;unsigned to_go=football_rules_distance(r);
    float blitz=.12f,zone=.30f;
    if(r->down>=3 && to_go>=5){blitz+=.28f;zone+=.2f;}   /* obvious pass: pressure it or sit on the routes */
    if(to_go<=2){blitz+=.1f;zone-=.2f;}                 /* short yardage: stay tight */
    if(r->ball>=90)zone-=.1f;
    float roll=(football_live_random(m)%1000)/1000.0f;
    return roll<blitz?FB_DEF_BLITZ:roll<blitz+zone?FB_DEF_ZONE:FB_DEF_MAN;
}
void football_live_ai_carrier(const FootballLive *m,unsigned i,float *x,float *y)
{
    const FootballLiveActor *a=&m->actors[i];unsigned off=off_base(m),def=11-off;float dir=dir_of(m);
    const FootballPlayDefinition *p=football_playbook_get(m->play);
    float depth=(a->x-m->los)*dir,tx=dir,ty=0;
    bool lane=p && p->type==FB_PLAY_RUN && p->route_count && i==off+1u && !m->caught && depth<p->route[p->route_count-1].x;
    if(lane){
        /* The called run's path, a few yards at a time: route y is inside-positive around the middle of the field. */
        FootballRoutePoint point=p->route[p->route_count-1];
        for(unsigned k=0;k<p->route_count;++k)if(p->route[k].x>depth+2.5f){point=p->route[k];break;}
        tx=clampf((m->los+dir*point.x-a->x)*.6f,-1,1);ty=clampf((26.67f-m->run_side*point.y-a->y)*.5f,-1,1);
    }
    /* Read what is in front: lean away from defenders in the lane, ease off when one stands square in it. */
    float push=0,block=0;
    for(unsigned d=def;d<def+11;++d){const FootballLiveActor *b=&m->actors[d];float ahead=(b->x-a->x)*dir,side=b->y-a->y;
        if(ahead<-1.2f || ahead>8 || fabsf(side)>6)continue;
        float weight=7/(ahead*ahead+side*side+.6f);
        push+=(fabsf(side)<.15f?(float)m->run_side:side>0?-1.f:1.f)*weight;
        if(fabsf(side)<1.6f && ahead<4)block+=weight*.25f;
    }
    ty=clampf(ty+push,-1,1);tx*=1-clampf(block,0,.55f);
    if(a->y<3.5f)ty+=.5f;else if(a->y>49.8f)ty-=.5f;
    *x=tx;*y=clampf(ty,-1,1);
}
int football_live_ai_qb(FootballLive *m)
{
    unsigned off=off_base(m),def=11-off;float dir=dir_of(m);const FootballLiveActor *qb=&m->actors[off];
    if(m->thrown || m->flying || m->scramble || m->carrier!=off)return FB_AI_HOLD;
    /* The window opens when the called routes have had time to break: quick ones early, deep and play-action ones late. */
    const FootballPlayDefinition *play=football_playbook_get(m->play);
    float opens=.85f+(play?fmaxf(0,play->gain)*.11f+(play->formation==FB_FORM_UNDER_CENTER?.4f:0):0);
    static const uint8_t slot[5]={2,3,4,5,1};
    float to_go=(float)football_rules_distance(&m->rules),arm=12+qb->attributes.thp*.36f,pressure=100,ahead=100;
    for(unsigned d=def;d<def+11;++d){const FootballLiveActor *b=&m->actors[d];
        pressure=fminf(pressure,gap(qb,b));
        if((b->x-qb->x)*dir>0 && (b->x-qb->x)*dir<6 && fabsf(b->y-qb->y)<3)ahead=0;
    }
    int pick=-1;float best=-1;
    for(unsigned t=0;t<5;++t){const FootballLiveActor *r=&m->actors[off+slot[t]];
        float depth=(r->x-m->los)*dir,range=gap(qb,r),near=100;
        if(depth<-2 || range>arm+8)continue;
        for(unsigned d=def;d<def+11;++d)near=fminf(near,gap(r,&m->actors[d]));
        /* A QB misjudges how open a receiver is, more the poorer his reads (tha). */
        near+=(((int)(football_live_random(m)%2001)-1000)/1000.0f)*(1.4f-qb->attributes.tha*.011f);
        /* Open is what matters; a receiver past the sticks on 3rd/4th down is worth waiting for. */
        float value=fminf(near,5)-fmaxf(0,depth-10)*.07f+(m->rules.down>=3 && depth>=to_go?1.5f:0)-fmaxf(0,range-arm)*.25f;
        if(value>best){best=value;pick=(int)t;}
    }
    /* Patience runs out with the clock in the pocket; a sharp QB takes smaller windows. */
    float need=fmaxf(1.6f,3.4f-(m->elapsed-opens)*1.0f-(qb->attributes.tha-50)*.012f);
    if(m->elapsed<opens && pressure>=2.0f)return FB_AI_HOLD;
    /* Pressure is felt a beat late: a rusher can arrive before the QB reacts, and that is a sack. */
    bool felt=football_live_random(m)%100<9u+qb->attributes.tha/10u;
    if(pick>=0 && (best>=need || (pressure<2.4f && felt && best>=1.6f)))return pick;
    if(m->elapsed>=3.6f || (pressure<1.0f+qb->attributes.agi*.008f && felt)){
        if(qb->attributes.agi>=45 && ahead>50 && m->elapsed<3.2f)return FB_AI_SCRAMBLE;
        return FB_AI_THROW_AWAY;
    }
    return FB_AI_HOLD;
}
float football_live_ai_separation(const FootballLive *m,unsigned i)
{
    unsigned def=11-off_base(m);const FootballLiveActor *a=&m->actors[i];float best=3,push=0;
    for(unsigned d=def;d<def+11;++d){float g=gap(a,&m->actors[d]);if(g<best){best=g;push=a->y>=m->actors[d].y?1.f:-1.f;}}
    return push*(3-best)/3*(.25f+a->attributes.agi/250.0f);
}

/* Project onto the called polyline, then look ahead by distance along it.
 * Advancing by x alone skips the nearly sideways legs of PA Cross. */
static FootballRoutePoint receiver_route(const FootballPlayDefinition *p,float x,float y)
{
    FootballRoutePoint start={0,0},point={0,0};unsigned segment=0;float nearest=1e9f;
    for(unsigned k=0;k<p->route_count;++k){
        FootballRoutePoint end=p->route[k];float dx=end.x-start.x,dy=end.y-start.y;
        float len2=dx*dx+dy*dy,t=len2>0?clampf(((x-start.x)*dx+(y-start.y)*dy)/len2,0,1):0;
        FootballRoutePoint q={start.x+t*dx,start.y+t*dy};float gap2=(q.x-x)*(q.x-x)+(q.y-y)*(q.y-y);
        if(gap2<nearest){nearest=gap2;point=q;segment=k;}
        start=end;
    }
    float left=1.5f;
    for(unsigned k=segment;k<p->route_count;++k){
        FootballRoutePoint end=p->route[k];float dx=end.x-point.x,dy=end.y-point.y,len=hypotf(dx,dy);
        if(len>left)return (FootballRoutePoint){point.x+dx*left/len,point.y+dy*left/len};
        left-=len;point=end;
    }
    point.x=fmaxf(point.x,x)+left;return point;
}

void football_live_ai_receiver(FootballLive *m,unsigned i,float *x,float *y)
{
    unsigned off=off_base(m),def=11-off,rank=i-off;
    FootballLiveActor *a=&m->actors[i];
    const FootballPlayDefinition *p=football_playbook_get(m->play);
    /* Receiver-relative routes start at the snap position. Inside is toward the
     * middle of the field, independent of slot parity and direction of play. */
    float dir=dir_of(m),origin=rank==1?(p->formation==FB_FORM_UNDER_CENTER?-5.f:-7.f):-1.f;
    float lane=rank==1?28:rank==2?4:rank==3?49:rank==4?13:39,inside=lane<26.67f?1.f:-1.f;
    float depth=(a->x-m->los)*dir-origin;
    FootballRoutePoint route=receiver_route(p,depth,(a->y-lane)*inside);
    float tx=dir*fmaxf(.1f,route.x-depth),ty=clampf(lane+route.y*inside,2,51)-a->y;
    float length=hypotf(tx,ty);if(length>1){tx/=length;ty/=length;}
    /* Hold a sidestep lane until past the obstructing body, rather than
     * continuously running sideways or turning straight back into contact. */
    if(a->route_peer){const FootballLiveActor *b=&m->actors[a->route_peer-1];
        if((b->x-a->x)*dir<-FB_BODY_DIAMETER || gap(a,b)>6)a->route_peer=0;
    }
    if(!a->route_peer){
        float nearest=3;
        for(unsigned d=def;d<def+11;++d){const FootballLiveActor *b=&m->actors[d];
            float ahead=(b->x-a->x)*dir,side=b->y-a->y,range=gap(a,b);
            float closing=(a->vx-b->vx)*dir;
            if(ahead<0 || ahead>2.5f || fabsf(side)>FB_BODY_DIAMETER || range>=nearest ||
               (ahead>FB_BODY_DIAMETER+.2f && closing<.5f))continue;
            nearest=range;
            float release=fabsf(side)<.3f?(fabsf(ty)>.2f?(ty>0?1.f:-1.f):inside):(side>0?-1.f:1.f);
            if(b->y+release*(FB_BODY_DIAMETER+.4f)<2 || b->y+release*(FB_BODY_DIAMETER+.4f)>51)release=-release;
            a->route_peer=(uint8_t)(d+1);a->route_side=(int8_t)release;
            a->route_y=clampf(b->y+release*(FB_BODY_DIAMETER+.4f),2,51);
        }
    }
    if(a->route_peer){const FootballLiveActor *b=&m->actors[a->route_peer-1];
        /* If coverage follows the first step, keep enough lateral clearance. */
        if(fabsf(b->y-a->y)<FB_BODY_DIAMETER && (b->x-a->x)*dir>0)
            a->route_y=clampf(b->y+a->route_side*(FB_BODY_DIAMETER+.4f),2,51);
        float center=clampf(lane+route.y*inside,2,51);
        a->route_y=clampf(a->route_y,center-1.7f,center+1.7f);
        tx=dir*fmaxf(.1f,route.x-depth);ty=a->route_y-a->y;
    }
    /* Settle before the back of the end zone, as routeThink does in the
     * supplied JS AI. Four yards of braking also covers high-speed, low-agility
     * players. Apply after release steering so contact cannot cancel it. */
    float rear=dir>0?116.5f:3.5f;
    float forward=clampf((rear-a->x)*dir/4.f,-.3f,1.f);
    if(tx*dir>forward)tx=dir*forward;
    *x=tx;*y=ty;
}
