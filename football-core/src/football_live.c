#include "football_live.h"
#include <math.h>
#include <string.h>
static float limit(float v,float a,float b){return v<a?a:v>b?b:v;}
static uint32_t random32(FootballLive *m){uint32_t x=m->rng;x^=x<<13;x^=x>>17;x^=x<<5;m->rng=x;return x;}
static float distance(float x,float y,float a,float b){float dx=x-a,dy=y-b;return sqrtf(dx*dx+dy*dy);}
static float direction(const FootballLive *m){return m->rules.offense==FB_HOME?1:-1;}
static unsigned offense_base(const FootballLive *m){return m->rules.offense==FB_HOME?0:11;}
static unsigned defense_base(const FootballLive *m){return m->rules.offense==FB_HOME?11:0;}
static void arrange(FootballLive *m)
{
    static const float ox[11]={-5,-7,-1,-1,-1,-1,0,0,0,0,0};
    static const float oy[11]={26.67f,28,4,49,13,39,21,24,26.67f,29,32};
    static const float dx[11]={2,2,2,2,7,7,7,2,2,15,15};
    static const float dy[11]={20,24,29,33,15,26.67f,38,5,48,18,36};
    float dir=direction(m);m->los=m->rules.offense==FB_HOME?10+m->rules.ball:110-m->rules.ball;
    unsigned off=offense_base(m),def=defense_base(m);
    const FootballPlayDefinition *play=football_playbook_get(m->play);
    for(unsigned i=0;i<11;++i){
        FootballLiveActor *a=&m->actors[off+i],*b=&m->actors[def+i];
        unsigned os=m->lineup[m->rules.offense][i],ds=m->lineup[1-m->rules.offense][i+11];
        float depth=ox[i];
        if(play && play->formation==FB_FORM_UNDER_CENTER){if(i==0)depth=-1.5f;else if(i==1)depth=-5;}
        *a=(FootballLiveActor){.attributes=m->units[m->rules.offense][os],.x=m->los+dir*depth,.y=oy[i],.roster=(uint8_t)os,.home=off==0};
        *b=(FootballLiveActor){.attributes=m->units[1-m->rules.offense][ds],.x=m->los+dir*dx[i],.y=dy[i],.roster=(uint8_t)ds,.home=def==0};
    }
    m->carrier=(uint8_t)off;m->controlled=m->rules.offense==FB_HOME?0:5;
    m->ball_x=m->actors[off].x;m->ball_y=m->actors[off].y;m->ball_z=0;
    m->flying=m->thrown=m->caught=m->loose=false;m->elapsed=m->juke=m->cooldown=0;
}
bool football_live_begin(FootballLive *m,const FootballRoster *r,uint32_t seed,unsigned round)
{
    if(!m || !r || r->count!=22 || round<1 || round>7)return false;
    unsigned seen=0;
    for(unsigned i=0;i<22;++i){unsigned member=r->lineup[i];if(member>=22 || (seen&(1u<<member)))return false;seen|=1u<<member;}
    memset(m,0,sizeof *m);m->rng=seed?seed:1;
    if(!football_rules_begin(&m->rules,(FootballSide)(random32(m)%2)))return false;
    for(unsigned s=0;s<2;++s)for(unsigned i=0;i<22;++i){
        m->lineup[s][i]=s? (uint8_t)i:r->lineup[i];
        m->units[s][i]=r->player[i].attributes;
        if(s){
            const pr_form *form=&pr_forms[random32(m)%PR_FORM_COUNT];FootballPlayer opponent;
            if(!football_stats_generate(&opponent,i,form,&r->maxima))return false;
            m->away_species[i]=form->species;m->away_form[i]=form->form_index;m->units[s][i]=opponent.attributes;
            uint8_t *a=(uint8_t *)&m->units[s][i];
            for(unsigned k=0;k<sizeof(FootballAttributes);++k)a[k]=(uint8_t)limit(a[k]*(1+(round-1)*.0625f),1,99);
        }
    }
    m->pending_receiver=-1;m->phase=FB_LIVE_CALL;m->play=FB_PASS_SLANTS;arrange(m);return true;
}
bool football_live_call(FootballLive *m,FootballPlayId play)
{
    if(!m || m->phase!=FB_LIVE_CALL || !football_playbook_get(play))return false;
    m->play=play;m->phase=FB_LIVE_SET;arrange(m);return true;
}
bool football_live_defend(FootballLive *m,FootballDefenseId defense)
{
    if(!m || m->phase!=FB_LIVE_CALL || m->rules.offense!=FB_AWAY || (unsigned)defense>=FB_DEF_COUNT)return false;
    /* CPU call never reads the human scheme. */
    FootballPlayId play=(FootballPlayId)(random32(m)%FB_PLAY_COUNT);
    if(!football_live_call(m,play))return false;
    m->defense=defense;return true;
}
void football_live_switch(FootballLive *m)
{
    if(!m || m->phase!=FB_LIVE_PLAY || m->rules.offense==FB_HOME)return;
    /* Cycle through all eleven defenders, never hand control back to the AI. */
    m->controlled=(uint8_t)((m->controlled+1)%11);
}
static void finish(FootballLive *m,FootballResultKind kind,float x)
{
    int yards=(int)roundf((x-m->los)*direction(m));
    if(kind==FB_RESULT_INCOMPLETE)yards=0;
    if(kind==FB_RESULT_INTERCEPTION && yards<0)yards=0;
    m->result=(FootballResolvedPlay){kind,(int16_t)limit((float)yards,-100,100),(uint8_t)limit(ceilf(m->elapsed)+4,1,30)};
    if(!football_rules_advance(&m->rules,m->result)){m->rules.finished=true;m->rules.winner=FB_AWAY;}
    ++m->snaps;m->phase=m->rules.finished?FB_LIVE_END:FB_LIVE_DEAD;m->dead_time=0;
}
static void move(FootballLiveActor *a,float tx,float ty,bool sprint)
{
    float length=sqrtf(tx*tx+ty*ty);if(length>1){tx/=length;ty/=length;}
    float speed=(4.3f+a->attributes.spd*.045f)*(sprint?1.08f:1);
    if(a->stun>0){a->stun-=FB_LIVE_STEP;tx=ty=0;}
    float accel=(10+a->attributes.agi*.1f)*FB_LIVE_STEP;
    a->vx+=limit(tx*speed-a->vx,-accel,accel);a->vy+=limit(ty*speed-a->vy,-accel,accel);
    a->x=limit(a->x+a->vx*FB_LIVE_STEP,0,120);a->y+=a->vy*FB_LIVE_STEP;
    if(a->contact>0)a->contact-=FB_LIVE_STEP;
}
static void throw_ball(FootballLive *m,unsigned receiver)
{
    unsigned off=offense_base(m);if((m->actors[off].x-m->los)*direction(m)>0 || receiver>=5 || m->flying || m->thrown || m->carrier!=off)return;
    const FootballPlayDefinition *p=football_playbook_get(m->play);if(!p || p->type!=FB_PLAY_PASS)return;
    static const uint8_t targets[5]={2,3,4,5,1};
    unsigned index=off+targets[receiver];FootballLiveActor *qb=&m->actors[off],*target=&m->actors[index];
    float range=distance(qb->x,qb->y,target->x,target->y),arm=12+qb->attributes.thp*.36f;
    float duration=limit(range/(18+qb->attributes.thp*.1f),.25f,2.4f);
    float error=(1-qb->attributes.tha/110.0f)*(.4f+range*.04f)+fmaxf(0,range-arm)*.4f;
    float jitter=((int)(random32(m)%2001)-1000)/1000.0f;
    m->from_x=qb->x;m->from_y=qb->y;
    m->to_x=limit(target->x+target->vx*duration+jitter*error,0,120);
    m->to_y=target->y+target->vy*duration+jitter*error;
    m->flight_time=duration;m->flight=0;m->flying=m->thrown=true;m->target=(uint8_t)index;
}
static void ball_step(FootballLive *m)
{
    if(!m->flying){m->ball_x=m->actors[m->carrier].x;m->ball_y=m->actors[m->carrier].y;return;}
    m->flight+=FB_LIVE_STEP;float t=limit(m->flight/m->flight_time,0,1);
    m->ball_x=m->from_x+(m->to_x-m->from_x)*t;m->ball_y=m->from_y+(m->to_y-m->from_y)*t;m->ball_z=4*t*(1-t)*5;
    if(t<1)return;
    m->flying=false;m->ball_z=0;
    unsigned nearest=22;float best=2.4f;
    for(unsigned i=0;i<22;++i){float d=distance(m->ball_x,m->ball_y,m->actors[i].x,m->actors[i].y);if(d<best){best=d;nearest=i;}}
    if(nearest==22){finish(m,FB_RESULT_INCOMPLETE,m->los);return;}
    if((nearest<11)!=(m->rules.offense==FB_HOME)){finish(m,FB_RESULT_INTERCEPTION,m->ball_x);return;}
    if(random32(m)%100>55u+m->actors[nearest].attributes.cth/3){finish(m,FB_RESULT_INCOMPLETE,m->los);return;}
    m->carrier=(uint8_t)nearest;m->caught=true;
    if(m->rules.offense==FB_HOME)m->controlled=m->carrier;
}
static void step(FootballLive *m,FootballLiveInput input)
{
    ++m->steps;
    if(m->phase==FB_LIVE_CALL || m->phase==FB_LIVE_END)return;
    if(m->phase==FB_LIVE_DEAD){m->dead_time+=FB_LIVE_STEP;if(m->dead_time>=1){m->phase=FB_LIVE_CALL;arrange(m);}return;}
    m->elapsed+=FB_LIVE_STEP;
    unsigned off=offense_base(m),def=defense_base(m);float dir=direction(m);
    const FootballPlayDefinition *p=football_playbook_get(m->play);
    if(m->phase==FB_LIVE_SET){
        if((m->rules.offense==FB_HOME && (input.pressed&FB_LIVE_ACTION)) || (m->rules.offense==FB_AWAY && m->elapsed>=1.2f)){
            m->phase=FB_LIVE_PLAY;m->elapsed=0;
            if(p->type==FB_PLAY_RUN){m->carrier=(uint8_t)(off+1);if(m->rules.offense==FB_HOME)m->controlled=m->carrier;}
        }
        return;
    }
    if(input.pressed&FB_LIVE_SWITCH)football_live_switch(m);
    if(m->rules.offense==FB_HOME && input.receiver>=0)throw_ball(m,(unsigned)input.receiver);
    if(m->cooldown>0)m->cooldown-=FB_LIVE_STEP;
    if(m->juke>0)m->juke-=FB_LIVE_STEP;
    if(m->rules.offense==FB_HOME && (input.pressed&FB_LIVE_JUKE) && m->cooldown<=0){m->juke=.35f;m->cooldown=1.6f;}
    FootballLiveActor *carrier=&m->actors[m->carrier];
    for(unsigned i=0;i<22;++i){
        FootballLiveActor *a=&m->actors[i];float tx=0,ty=0;
        bool attack=i>=off && i<off+11;
        if(i==m->controlled){tx=limit(input.x,-1,1);ty=limit(input.y,-1,1);}
        else if(attack){
            unsigned rank=i-off;
            if(i==m->carrier){tx=dir;ty=(26.67f-a->y)*.05f;
                if(rank==0 && p->type==FB_PLAY_PASS && !m->caught){tx=0;ty=(26.67f-a->y)*.2f;}
            }
            else if(rank>=6){
                unsigned mark=def+(rank-6)%4;tx=m->actors[mark].x-a->x;ty=m->actors[mark].y-a->y;
            }else if(rank>=1){
                FootballRoutePoint route=football_playbook_route(p,limit(m->elapsed/3,0,1));
                float target_x=m->los+dir*(route.x+rank*2),target_y=rank==1?30:rank==2?6:rank==3?47:rank==4?16:37;
                target_y+=route.y*(rank%2?1:-1);
                tx=target_x-a->x;ty=limit(target_y,2,51)-a->y;
                if(m->caught){tx=dir;ty=(carrier->y-a->y)*.1f;}
            }
        }else{
            unsigned rank=i-def;
            if(m->flying){tx=m->to_x-a->x;ty=m->to_y-a->y;}
            else if(!m->thrown && p->type==FB_PLAY_PASS && rank>=4 && m->elapsed<3 && !(m->rules.offense==FB_AWAY && m->defense==FB_DEF_BLITZ)){
                unsigned mark=off+1+(rank-4)%5;tx=m->actors[mark].x+dir-a->x;ty=m->actors[mark].y-a->y;
                if(m->rules.offense==FB_AWAY && m->defense==FB_DEF_ZONE){tx=m->los+dir*(rank>8?15:8)-a->x;ty=6+(rank-4)*6.8f-a->y;}
            }else {tx=carrier->x+carrier->vx*.15f-a->x;ty=carrier->y+carrier->vy*.15f-a->y;}
            /* Five line blockers engage nearby rushers; strength sets hold time. */
            if(rank<4)for(unsigned b=off+6;b<off+11;++b)if(distance(a->x,a->y,m->actors[b].x,m->actors[b].y)<1.5f && m->elapsed<.6f+m->actors[b].attributes.str*.018f){a->stun=.02f;tx=ty=0;}
        }
        move(a,tx,ty,i==m->controlled);
        a->y=limit(a->y,-1,54.333f);
    }
    if(m->rules.offense==FB_AWAY && p->type==FB_PLAY_PASS && !m->thrown && m->elapsed>=1.7f){
        unsigned best=0;float separation=-1;
        static const unsigned targets[5]={2,3,4,5,1};
        for(unsigned t=0;t<5;++t){FootballLiveActor *a=&m->actors[off+targets[t]];float near=100;
            for(unsigned d=def;d<def+11;++d)near=fminf(near,distance(a->x,a->y,m->actors[d].x,m->actors[d].y));
            if(near>separation){separation=near;best=t;}}
        throw_ball(m,best);
    }
    if(!m->thrown && m->carrier==off && (carrier->x-m->los)*dir>0)m->thrown=true; /* QB scramble past LOS */
    ball_step(m);if(m->phase!=FB_LIVE_PLAY)return;
    carrier=&m->actors[m->carrier];
    if(!m->flying){
        if(carrier->x<=0 || carrier->x>=120 || carrier->y<=0 || carrier->y>=53.333f || (carrier->x>=110 && dir>0) || (carrier->x<=10 && dir<0)){
            finish(m,m->caught?FB_RESULT_COMPLETE:FB_RESULT_RUN,carrier->x);return;}
        for(unsigned d=def;d<def+11;++d){FootballLiveActor *a=&m->actors[d];bool dive=d==m->controlled && (input.pressed&FB_LIVE_ACTION);
            if(a->contact>0 || distance(a->x,a->y,carrier->x,carrier->y)>(dive?2.6f:1.2f))continue;
            a->contact=.35f;float chance=limit(53+(a->attributes.tak-carrier->attributes.agi)*.35f+(dive?15:0)-(m->juke>0?35:0),15,95);
            if(random32(m)%100<chance){
                if(random32(m)%1000<8){
                    /* Recovery belongs to the nearest actor at the actual loose-ball spot. */
                    unsigned recover=m->carrier;float nearest=2.5f;
                    for(unsigned j=0;j<22;++j)if(j!=m->carrier){float dist=distance(carrier->x,carrier->y,m->actors[j].x,m->actors[j].y);if(dist<nearest){nearest=dist;recover=j;}}
                    bool same=(recover<11)==(m->carrier<11);finish(m,same?FB_RESULT_FUMBLE_RECOVERED:FB_RESULT_FUMBLE,carrier->x);return;
                }
                FootballResultKind kind=!m->thrown && p->type==FB_PLAY_PASS?FB_RESULT_SACK:m->caught?FB_RESULT_COMPLETE:FB_RESULT_RUN;
                float spot=kind==FB_RESULT_SACK ? m->los+dir*fminf(0,(carrier->x-m->los)*dir):carrier->x;
                finish(m,kind,spot);return;
            }
            a->stun=dive?1:.35f;
        }
    }
    if(m->elapsed>=25)finish(m,m->caught?FB_RESULT_COMPLETE:FB_RESULT_RUN,carrier->x);
}
void football_live_tick(FootballLive *m,unsigned ms,FootballLiveInput input)
{
    if(!m || !isfinite(input.x) || !isfinite(input.y))return;
    if(ms>100)ms=100;
    m->pending|=input.pressed;
    if(input.receiver>=0 && input.receiver<5)m->pending_receiver=input.receiver;
    m->accumulator+=ms*.001f;
    while(m->accumulator+0.000001f>=FB_LIVE_STEP){m->accumulator-=FB_LIVE_STEP;input.pressed=m->pending;input.receiver=m->pending_receiver;step(m,input);m->pending=0;m->pending_receiver=-1;}
}
