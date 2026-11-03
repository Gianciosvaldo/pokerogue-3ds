#include "football_live.h"
#include "football_live_systems.h"
#include <math.h>
#include <string.h>
static float limit(float v,float a,float b){return v<a?a:v>b?b:v;}
#define random32 football_live_random
bool football_live_cpu(const FootballLive *m,FootballSide side){return m && (side==FB_AWAY || (m->autoplay&1));}
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
    m->credit_count=0;m->credited=FB_NO_ACTOR;m->return_yards=0;m->turnover=false;
    m->snap_offense=m->rules.offense;m->event=FB_EVENT_NONE;m->snap_score[0]=m->rules.score[0];m->snap_score[1]=m->rules.score[1];
    float dir=direction(m);m->los=m->rules.offense==FB_HOME?10+m->rules.ball:110-m->rules.ball;
    unsigned off=offense_base(m),def=defense_base(m);
    const FootballPlayDefinition *play=football_playbook_get(m->play);
    for(unsigned i=0;i<11;++i){
        FootballLiveActor *a=&m->actors[off+i],*b=&m->actors[def+i];
        unsigned os=m->lineup[m->rules.offense][i],ds=m->lineup[1-m->rules.offense][i+11];
        float depth=ox[i];
        if(play && play->formation==FB_FORM_UNDER_CENTER){if(i==0)depth=-1.5f;else if(i==1)depth=-5;}
        *a=(FootballLiveActor){.attributes=m->units[m->rules.offense][os],.x=m->los+dir*depth,.y=oy[i],.roster=(uint8_t)os,.block_peer=255,.facing=(int8_t)dir,.home=off==0};
        *b=(FootballLiveActor){.attributes=m->units[1-m->rules.offense][ds],.x=m->los+dir*dx[i],.y=dy[i],.roster=(uint8_t)ds,.block_peer=255,.facing=(int8_t)-dir,.home=def==0};
    }
    m->hand_to=FB_NO_ACTOR;m->align=FB_ALIGN_SPREAD;m->flip=0;
    int run_side=random32(m)&1?1:-1;
    if(!m->special && play){
        /* Where the five eligible players stand depends on the play (its usual alignment, or its alternate a third of the time)
         * and is mirrored half the time, so the same call does not always start from the same picture. */
        FootballAlignmentId id=football_playbook_alignment_for(play,random32(m)%100<35?1:0);
        const FootballAlignment *al=football_playbook_alignment(id);
        m->align=(uint8_t)id;m->flip=random32(m)&1;
        FootballLiveActor *rb=&m->actors[off+1];
        rb->x=m->los+dir*(ox[1]+al->rb_back+(play->formation==FB_FORM_UNDER_CENTER?2:0));rb->y=26.667f+al->rb_dy;
        for(unsigned k=0;k<4;++k){FootballLiveActor *w=&m->actors[off+2+k];w->x=m->los+dir*al->wr[k].depth;w->y=al->wr[k].y;}
        if(m->flip)for(unsigned k=1;k<6;++k)m->actors[off+k].y=53.333f-m->actors[off+k].y;
        /* The runs with a lead blocker go to the strong side, the one the tight end lines up on: run_side is set so that the end of the
         * called path (route y, inside-positive around the middle of the field) lands there. */
        if(football_playbook_strong_side(play) && play->route_count){
            bool high=m->actors[off+5].y>26.667f;int sign=play->route[play->route_count-1].y>0?1:-1;
            run_side=high?-sign:sign;
        }
    }
    for(unsigned i=0;i<11;++i){m->snap_x[i]=m->actors[off+i].x;m->snap_y[i]=m->actors[off+i].y;}
    football_live_defense_setup(m);
    m->target=(uint8_t)(off+2);
    /* The human always plays HOME: the QB with the ball, or a linebacker without it. */
    m->carrier=(uint8_t)off;m->controlled=!football_live_cpu(m,(FootballSide)m->rules.offense)?0:!football_live_cpu(m,(FootballSide)(1-m->rules.offense))?5:FB_NO_ACTOR;
    m->run_side=(int8_t)run_side;
    m->ball_x=m->actors[off].x;m->ball_y=m->actors[off].y;m->ball_z=0;
    m->flying=m->thrown=m->caught=m->loose=m->scramble=false;m->elapsed=m->juke=m->cooldown=0;
    memset(m->fx,0,sizeof m->fx);m->fx_next=0;
    if(m->special)football_live_special_arrange(m);
}
bool football_live_begin(FootballLive *m,const FootballRoster *r,uint32_t seed,unsigned round)
{
    if(!m || !r || r->count!=22 || round<1 || round>7)return false;
    unsigned seen=0;
    for(unsigned i=0;i<22;++i){unsigned member=r->lineup[i];if(member>=22 || (seen&(1u<<member)))return false;seen|=1u<<member;}
    memset(m,0,sizeof *m);m->rng=seed?seed:1;
    if(!football_rules_begin(&m->rules,(FootballSide)(random32(m)%2)))return false;
    m->rules.td_points=6; /* a touchdown is six; the try after it is played */
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
/* The human snaps the ball, so the human may change his mind until he does. */
static void remember_call(FootballLive *m)
{
    m->undo_valid=!football_live_cpu(m,(FootballSide)m->rules.offense);
    m->undo_rules=m->rules;m->undo_rng=m->rng;m->undo_play=(uint8_t)m->play;m->undo_defense=(uint8_t)m->defense;
}
bool football_live_can_undo(const FootballLive *m){return m && m->phase==FB_LIVE_SET && m->undo_valid && !football_live_cpu(m,(FootballSide)m->rules.offense);}
bool football_live_undo(FootballLive *m)
{
    if(!football_live_can_undo(m))return false;
    m->rules=m->undo_rules;m->play=(FootballPlayId)m->undo_play;m->defense=(FootballDefenseId)m->undo_defense;
    m->special=FB_SPECIAL_NONE;m->phase=FB_LIVE_CALL;
    arrange(m); /* the picture the playcall shows, as before the call; its random draws do not count */
    m->rng=m->undo_rng;m->undo_valid=false;m->pending=0;
    return true;
}
bool football_live_call(FootballLive *m,FootballPlayId play)
{
    if(!m || m->phase!=FB_LIVE_CALL || m->conversion==1 || !football_playbook_get(play))return false;
    remember_call(m);
    if(football_live_cpu(m,(FootballSide)(1-m->rules.offense))){
        /* Scheme depends on situation/seed, never the selected offensive play. */
        m->defense=football_live_ai_defense(m);
    }
    m->special=FB_SPECIAL_NONE;m->play=play;m->phase=FB_LIVE_SET;arrange(m);return true;
}
bool football_live_special(FootballLive *m,FootballSpecial kind)
{
    if(!football_live_special_allowed(m,kind))return false;
    remember_call(m);
    /* The extra point is kicked from the 15, where the formation and the kick distance are those of a field goal. */
    if(kind==FB_SPECIAL_EXTRA_POINT){m->rules.ball=85;m->rules.first_down_at=100;}
    m->special=(uint8_t)kind;m->play=FB_RUN_TOSS;m->phase=FB_LIVE_SET;arrange(m);return true;
}
bool football_live_go_for_two(FootballLive *m)
{
    if(!m || m->phase!=FB_LIVE_CALL || m->conversion!=1)return false;
    m->conversion=2;return true;
}
bool football_live_defend(FootballLive *m,FootballDefenseId defense)
{
    if(!m || m->phase!=FB_LIVE_CALL || !football_live_cpu(m,(FootballSide)m->rules.offense) || (unsigned)defense>=FB_DEF_COUNT)return false;
    /* CPU call never reads the human scheme. */
    m->defense=defense;
    if(m->conversion==1){
        if(!football_live_ai_goes_for_two(m))return football_live_special(m,FB_SPECIAL_EXTRA_POINT);
        football_live_go_for_two(m);
    }
    FootballSpecial kick=m->conversion?FB_SPECIAL_NONE:football_live_ai_special(m);
    if(kick!=FB_SPECIAL_NONE && football_live_special(m,kick))return true;
    return football_live_call(m,football_live_ai_call(m));
}
void football_live_cycle_controlled(FootballLive *m,int step)
{
    /* The human's eleven are actors 0..10. Nearest the ball first; equal distances keep their order. */
    unsigned order[11],here=11;float far[11];
    for(unsigned i=0;i<11;++i){far[i]=distance(m->actors[i].x,m->actors[i].y,m->ball_x,m->ball_y);order[i]=i;}
    for(unsigned i=1;i<11;++i)for(unsigned j=i;j>0 && far[order[j]]<far[order[j-1]];--j){unsigned t=order[j];order[j]=order[j-1];order[j-1]=t;}
    for(unsigned i=0;i<11;++i)if(order[i]==m->controlled)here=i;
    if(here==11){m->controlled=(uint8_t)order[0];return;} /* nobody steered: the one nearest the ball */
    m->controlled=(uint8_t)order[(here+11u+(unsigned)(step<0?10:1))%11];
}
void football_live_switch_step(FootballLive *m,int step)
{
    if(!m || m->phase!=FB_LIVE_PLAY || !football_live_cpu(m,(FootballSide)m->rules.offense) || football_live_cpu(m,(FootballSide)(1-m->rules.offense)))return;
    football_live_cycle_controlled(m,step); /* never hand control back to the AI */
}
void football_live_switch(FootballLive *m){football_live_switch_step(m,1);}
void football_live_credit(FootballLive *m,unsigned actor,FootballCreditKind kind)
{
    if(!m || actor>=22 || kind<FB_CREDIT_INTERCEPTION || kind>FB_CREDIT_TOUCHDOWN)return;
    unsigned side=actor<11?FB_HOME:FB_AWAY,member=m->actors[actor].roster;
    if(member>=22)return;
    uint8_t *stat=&m->stats[side][member][kind-1];
    if(*stat<255)++*stat;
    if(m->credit_count<4)m->credits[m->credit_count++]=(FootballCredit){(uint8_t)side,(uint8_t)member,(uint8_t)kind};
}
void football_live_fx(FootballLive *m,FootballFxKind kind,unsigned actor,unsigned other,int side)
{
    if(!m || actor>=22 || kind<=FB_FX_NONE || kind>FB_FX_TOUCHDOWN)return;
    bool at_ball=kind==FB_FX_INTERCEPTION;
    m->fx[m->fx_next]=(FootballFx){(uint8_t)kind,(uint8_t)actor,(uint8_t)(other<22?other:22),(int8_t)(side<0?-1:1),m->steps,at_ball?m->ball_x:m->actors[actor].x,at_ball?m->ball_y:m->actors[actor].y};
    m->fx_next=(uint8_t)((m->fx_next+1)%FB_FX_COUNT);
}
/* After the ball is dead and the rules have run: a touchdown is followed by its try, a field goal, a safety or the
 * half by a kickoff. Nothing follows in overtime, where a touchdown is worth the full seven, or at the end. */
void football_live_after_play(FootballLive *m)
{
    const FootballRules *r=&m->rules;
    m->after=FB_AFTER_NONE;
    if(m->phase==FB_LIVE_END || r->finished || r->quarter>=5)return;
    if(r->last.touchdown){m->after=FB_AFTER_CONVERSION;m->after_side=r->score[FB_HOME]>m->snap_score[FB_HOME]?FB_HOME:FB_AWAY;}
    else if(r->last.field_goal || r->last.safety || r->last.halftime){m->after=FB_AFTER_KICKOFF;m->after_side=(uint8_t)(1-r->offense);}
}
/* The try is over (a kick or a play): points to the scorer, the other team takes the ball at its 25 for now and
 * the scorer kicks it off. The clock never runs on a try. */
void football_live_conversion_done(FootballLive *m,unsigned points,FootballLiveEvent event)
{
    unsigned scorer=m->after_side;
    bool ok=football_rules_conversion(&m->rules,(FootballSide)scorer,points,(FootballSide)m->next_offense);
    if(!ok){m->rules.finished=true;m->rules.winner=FB_AWAY;}
    m->conversion=0;m->event=(uint8_t)event;m->flying=false;m->ball_z=0;m->caught=false;
    ++m->snaps;m->phase=m->rules.finished?FB_LIVE_END:FB_LIVE_DEAD;m->dead_time=0;
    m->after=m->rules.finished?FB_AFTER_NONE:FB_AFTER_KICKOFF;m->after_side=(uint8_t)scorer;
}
/* The dead ball after a run-back: the team that took the ball scores, or gets it where its carrier went down; a fumble on the return
 * goes back to the team that threw. Yards are counted from where the ball was taken. */
static void finish_turnover(FootballLive *m,FootballResultKind kind,float x)
{
    unsigned taker=m->rules.offense,thrower=1-taker;float rd=taker==FB_HOME?1.f:-1.f;
    unsigned seconds=(unsigned)limit(ceilf(m->elapsed)+4,1,30);
    bool score=(rd>0 && x>=110) || (rd<0 && x<=10),lost=kind==FB_RESULT_FUMBLE;
    m->turnover=false;
    football_live_credit(m,m->interceptor,FB_CREDIT_INTERCEPTION);
    m->return_yards=(int16_t)limit(roundf((x-m->catch_x)*rd),-100,100);
    m->result=(FootballResolvedPlay){FB_RESULT_INTERCEPTION,m->return_yards<0?0:m->return_yards,(uint8_t)seconds};
    bool ok;
    if(score && !lost){
        football_live_credit(m,m->carrier,FB_CREDIT_TOUCHDOWN);football_live_fx(m,FB_FX_TOUCHDOWN,m->carrier,22,rd<0?-1:1);
        ok=football_rules_touchdown(&m->rules,(FootballSide)taker,seconds);m->event=FB_EVENT_TOUCHDOWN;
    }else{
        unsigned side=lost?thrower:taker;
        int spot=(int)roundf(side==FB_HOME?x-10:110-x);
        ok=football_rules_possession(&m->rules,(FootballSide)side,(unsigned)limit((float)spot,1,99),seconds);
        m->event=lost?FB_EVENT_FUMBLE:FB_EVENT_INTERCEPTION;
    }
    if(!ok){m->rules.finished=true;m->rules.winner=FB_AWAY;}
    m->flying=false;m->ball_z=0;m->caught=false;
    ++m->snaps;m->phase=m->rules.finished?FB_LIVE_END:FB_LIVE_DEAD;m->dead_time=0;
    football_live_after_play(m);
}
static void finish(FootballLive *m,FootballResultKind kind,float x)
{
    if(m->special && m->stage==FB_STAGE_RETURN){football_live_special_finish(m,kind,x);return;}
    if(m->turnover){finish_turnover(m,kind,x);return;}
    int yards=(int)roundf((x-m->los)*direction(m));
    if(kind==FB_RESULT_INCOMPLETE)yards=0;
    if(kind==FB_RESULT_INTERCEPTION && yards<0)yards=0;
    m->result=(FootballResolvedPlay){kind,(int16_t)limit((float)yards,-100,100),(uint8_t)limit(ceilf(m->elapsed)+4,1,30)};
    if(m->conversion==2){
        /* The play from the 4: two points if the ball is carried or caught across the goal line, nothing otherwise. */
        bool good=(kind==FB_RESULT_RUN || kind==FB_RESULT_COMPLETE) && ((direction(m)>0 && x>=110) || (direction(m)<0 && x<=10));
        football_live_conversion_done(m,good?2:0,good?FB_EVENT_TWO_POINT:FB_EVENT_TWO_POINT_FAILED);return;
    }
    if(!football_rules_advance(&m->rules,m->result)){m->rules.finished=true;m->rules.winner=FB_AWAY;}
    const FootballRuleEvents *last=&m->rules.last;
    /* The scorer is the carrier, or the defender who took the ball away and ran it in. */
    if(last->touchdown){
        unsigned scorer=(kind==FB_RESULT_INTERCEPTION || kind==FB_RESULT_FUMBLE) && m->credited<22?m->credited:m->carrier;
        football_live_credit(m,scorer,FB_CREDIT_TOUCHDOWN);football_live_fx(m,FB_FX_TOUCHDOWN,scorer,22,m->actors[scorer].facing<0?-1:1);
    }
    m->event=last->touchdown?FB_EVENT_TOUCHDOWN:last->safety?FB_EVENT_SAFETY:kind==FB_RESULT_INTERCEPTION?FB_EVENT_INTERCEPTION:
        kind==FB_RESULT_SACK?FB_EVENT_SACK:kind==FB_RESULT_FUMBLE?FB_EVENT_FUMBLE:FB_EVENT_NONE;
    ++m->snaps;m->phase=m->rules.finished?FB_LIVE_END:FB_LIVE_DEAD;m->dead_time=0;
    football_live_after_play(m);
}
void football_live_move(FootballLiveActor *a,float tx,float ty,bool sprint)
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
/* The QB gives up on the play: the ball goes out of bounds, away from every receiver and defender. */
static void throw_away(FootballLive *m)
{
    unsigned off=offense_base(m);FootballLiveActor *qb=&m->actors[off];
    if(m->flying || m->thrown || m->carrier!=off)return;
    m->from_x=qb->x;m->from_y=qb->y;m->to_x=limit(qb->x+direction(m)*3,0,120);m->to_y=qb->y<26.67f?-6:59.3f;
    m->flight_time=.55f;m->flight=0;m->flying=m->thrown=true;m->target=(uint8_t)off;
}
/* The defender who has the ball runs it back, from where he caught it. False where the old dead-ball interception stands: on a try (a
 * two-point play) and in the defender's own end zone or out of bounds, which is a touchback. */
static bool begin_turnover(FootballLive *m,unsigned defender)
{
    unsigned taker=defender<11?FB_HOME:FB_AWAY,thrower=1-taker;
    bool deep=taker==FB_HOME?m->ball_x<=10:m->ball_x>=110;
    if(m->conversion || deep || m->ball_y<=0 || m->ball_y>=53.333f)return false;
    for(unsigned i=0;i<22;++i){FootballLiveActor *a=&m->actors[i];a->block_peer=255;a->block_time=0;a->block_cooldown=0;}
    m->turnover=true;m->interceptor=(uint8_t)defender;m->rules.offense=(uint8_t)taker;m->carrier=m->target=(uint8_t)defender;m->catch_x=m->ball_x;
    m->caught=m->thrown=true;m->flying=false;m->scramble=false;m->ball_z=0;m->play=FB_RUN_TOSS;m->hand_to=FB_NO_ACTOR;m->juke=0;m->cooldown=0;
    if(!football_live_cpu(m,(FootballSide)taker))m->controlled=(uint8_t)defender;
    else if(!football_live_cpu(m,(FootballSide)thrower)){
        /* The human chases with the player of his own team nearest the ball. */
        unsigned best=thrower==FB_HOME?0:11,start=best;float near=1000;
        for(unsigned i=start;i<start+11;++i){float g=distance(m->actors[i].x,m->actors[i].y,m->ball_x,m->ball_y);if(g<near){near=g;best=i;}}
        m->controlled=(uint8_t)best;
    }else m->controlled=FB_NO_ACTOR;
    return true;
}
static void ball_step(FootballLive *m)
{
    if(!m->flying){m->ball_x=m->actors[m->carrier].x;m->ball_y=m->actors[m->carrier].y;return;}
    m->flight+=FB_LIVE_STEP;float t=limit(m->flight/m->flight_time,0,1);
    m->ball_x=m->from_x+(m->to_x-m->from_x)*t;m->ball_y=m->from_y+(m->to_y-m->from_y)*t;m->ball_z=4*t*(1-t)*5;
    if(t<1)return;
    m->flying=false;m->ball_z=0;
    /* The ball comes down among whoever is in reach: the closest offense and defense player contest it. */
    unsigned receiver=22,defender=22;float near_receiver=2.4f,near_defender=2.4f;
    for(unsigned i=0;i<22;++i){float d=distance(m->ball_x,m->ball_y,m->actors[i].x,m->actors[i].y);
        if((i<11)==(m->rules.offense==FB_HOME)){if(d<near_receiver){near_receiver=d;receiver=i;}}
        else if(d<near_defender){near_defender=d;defender=i;}}
    if(defender<22){
        /* Alone under it a defender usually takes it; with a receiver there it is a fight. */
        float chance=receiver<22?3+m->actors[defender].attributes.cth/20.0f:58+m->actors[defender].attributes.cth/3.0f;
        if(random32(m)%100<chance){m->credited=(uint8_t)defender;football_live_fx(m,FB_FX_INTERCEPTION,defender,receiver,(m->ball_x>=m->from_x)?1:-1);
            /* The credit is made with the dead ball, as for every play, so a run-back that ends in a fumble is still his interception. */
            if(!begin_turnover(m,defender)){football_live_credit(m,defender,FB_CREDIT_INTERCEPTION);finish(m,FB_RESULT_INTERCEPTION,m->ball_x);}return;}
    }
    if(receiver==22 || (defender<22 && random32(m)%100<12u+m->actors[defender].attributes.cth/8u) || random32(m)%100>55u+m->actors[receiver].attributes.cth/3){finish(m,FB_RESULT_INCOMPLETE,m->los);return;}
    m->carrier=(uint8_t)receiver;m->caught=true;football_live_fx(m,FB_FX_CATCH,receiver,22,direction(m)>0?1:-1);
    if(!football_live_cpu(m,(FootballSide)m->rules.offense))m->controlled=m->carrier;
}
/* The dead ball is over: the next playcall, the try after a touchdown, or a kickoff. */
static void begin_next(FootballLive *m)
{
    unsigned after=m->after;FootballRules *r=&m->rules;
    m->after=FB_AFTER_NONE;m->special=FB_SPECIAL_NONE;m->conversion=0;m->phase=FB_LIVE_CALL;m->undo_valid=false;
    if(after==FB_AFTER_CONVERSION){
        /* The scorer lines up on the 4 for the try (a straight run from the 2 scored 93% of the time here, from the 4
         * 62%); the other team's drive waits in next_offense. */
        m->next_offense=r->offense;r->offense=m->after_side;r->ball=96;r->down=1;r->first_down_at=100;m->conversion=1;
    }else if(after==FB_AFTER_KICKOFF){
        r->offense=m->after_side;r->ball=35;r->down=1;r->first_down_at=45;
        m->special=FB_SPECIAL_KICKOFF;m->play=FB_RUN_TOSS;m->phase=FB_LIVE_SET;
    }
    arrange(m);
}
static void step(FootballLive *m,FootballLiveInput input)
{
    ++m->steps;
    if(m->phase==FB_LIVE_CALL || m->phase==FB_LIVE_END)return;
    if(m->phase==FB_LIVE_DEAD){
        float hold=m->event==FB_EVENT_TOUCHDOWN?FB_LIVE_HOLD_TOUCHDOWN:m->event==FB_EVENT_INTERCEPTION || m->event==FB_EVENT_SACK || m->event==FB_EVENT_FUMBLE?FB_LIVE_HOLD_TURNOVER:1;
        m->dead_time+=FB_LIVE_STEP;if(m->dead_time>=hold)begin_next(m);return;
    }
    m->elapsed+=FB_LIVE_STEP;
    unsigned off=offense_base(m),def=defense_base(m);float dir=direction(m);
    const FootballPlayDefinition *p=football_playbook_get(m->play);
    bool cpu=football_live_cpu(m,(FootballSide)m->rules.offense);
    if(m->phase==FB_LIVE_SET){
        if((input.pressed&FB_LIVE_UNDO) && football_live_undo(m))return;
        if((!cpu && (input.pressed&FB_LIVE_ACTION)) || (cpu && m->elapsed>=1.2f)){
            m->phase=FB_LIVE_PLAY;m->elapsed=0;m->undo_valid=false;
            if(!m->special && p->type==FB_PLAY_RUN)m->hand_to=(uint8_t)(off+football_playbook_runner(p)); /* the ball is the QB's until the handoff */
        }
        return;
    }
    if(m->special && m->stage!=FB_STAGE_RETURN){football_live_special_step(m,input);return;}
    if(input.pressed&FB_LIVE_SWITCH)football_live_switch_step(m,1);
    if(input.pressed&FB_LIVE_SWITCH_BACK)football_live_switch_step(m,-1);
    if(!cpu && input.receiver>=0)throw_ball(m,(unsigned)input.receiver);
    if(m->cooldown>0)m->cooldown-=FB_LIVE_STEP;
    if(m->juke>0)m->juke-=FB_LIVE_STEP;
    if(!cpu && m->hand_to>=22 && (input.pressed&FB_LIVE_JUKE) && m->cooldown<=0){m->juke=.35f;m->cooldown=1.6f;football_live_fx(m,FB_FX_JUKE,m->carrier,22,m->actors[m->carrier].vy<0?-1:1);}
    if(cpu && p->type==FB_PLAY_PASS && !m->thrown && !m->scramble){
        int read=football_live_ai_qb(m);
        if(read>=0 && read<5)throw_ball(m,(unsigned)read);
        else if(read==FB_AI_THROW_AWAY)throw_away(m);
        else if(read==FB_AI_SCRAMBLE)m->scramble=true;
    }
    float mesh_x=0,mesh_y=0;
    if(m->hand_to<22){
        /* The handoff is done when the runner reaches the mesh point beside the QB (or, if bodies in the way keep him from it, a
         * moment later anyway); the human gets the runner then. */
        football_live_mesh_point(m,&mesh_x,&mesh_y);
        const FootballLiveActor *runner=&m->actors[m->hand_to];
        if(m->elapsed>=football_playbook_handoff(p).delay && (distance(runner->x,runner->y,mesh_x,mesh_y)<=1.2f || m->elapsed>=FB_LIVE_HANDOFF_MAX)){
            m->carrier=m->hand_to;m->hand_to=FB_NO_ACTOR;if(!cpu)m->controlled=m->carrier;
        }
    }
    bool meshing=m->hand_to<22;
    FootballLiveActor *carrier=&m->actors[m->carrier];
    float intent[22][2];
    for(unsigned i=0;i<22;++i){
        FootballLiveActor *a=&m->actors[i];float tx=0,ty=0;
        bool attack=i>=off && i<off+11;
        if(i==m->controlled && !meshing){tx=limit(input.x,-1,1);ty=limit(input.y,-1,1);}
        else if(attack){
            unsigned rank=i-off;
            if(meshing && i==m->hand_to){if(m->elapsed>=football_playbook_handoff(p).delay){tx=mesh_x-a->x;ty=mesh_y-a->y;}}
            else if(i==m->carrier){
                if(meshing){tx=ty=0;} /* the QB turns with the ball and waits for the runner */
                else if(rank==0 && p->type==FB_PLAY_PASS && !m->caught && !m->scramble){
                    /* Hold the pocket, sliding away from the closest rusher. */
                    tx=0;ty=(26.67f-a->y)*.2f;float near=5;
                    for(unsigned d=def;d<def+11;++d){float g=distance(a->x,a->y,m->actors[d].x,m->actors[d].y);
                        if(g<near){near=g;ty+=(a->y>=m->actors[d].y?1:-1)*.45f;tx=(a->x-m->los)*dir>-9?-dir*.2f:0;}}
                }else football_live_ai_carrier(m,i,&tx,&ty);
            }
            else if(football_live_is_blocker(m,i)){
                float wx,wy;bool wall=football_live_screen_wall(m,rank,&wx,&wy);
                unsigned mark=wall?255:football_live_block_target(m,i);
                if(wall){tx=wx-a->x;ty=wy-a->y;} /* the screen's wall leaves its men and sets up ahead of the back */
                else if(mark<22){tx=m->actors[mark].x-a->x;ty=m->actors[mark].y-a->y;
                    /* Pass protection sets and mirrors; it does not charge into the rushers. */
                    if(p->type==FB_PLAY_PASS && !m->caught){float len=hypotf(tx,ty);if(len>.55f){tx*=.55f/len;ty*=.55f/len;}}}
                /* A run's eligible blockers release downfield when no defender
                 * is in reach; an empty assignment is not an order to stand still. */
                else if((p->type==FB_PLAY_RUN || m->caught) && rank>=1 && rank<=5)tx=dir;
            }else if(rank>=1){
                if(m->flying && i==m->target){tx=m->to_x-a->x;ty=m->to_y-a->y;} /* the target goes to the ball */
                else if(m->caught){tx=dir;ty=(carrier->y-a->y)*.1f;}
                else football_live_ai_receiver(m,i,&tx,&ty);
            }
        }else football_live_defense_intent(m,i,&tx,&ty);
        intent[i][0]=tx;intent[i][1]=ty;
    }
    for(unsigned i=0;i<22;++i)football_live_move(&m->actors[i],intent[i][0],intent[i][1],i==m->controlled || (m->flying && i==m->target));
    football_live_physics(m);
    football_live_update_facing(m);
    if(!m->thrown && m->carrier==off && (carrier->x-m->los)*dir>0)m->thrown=true; /* QB scramble past LOS */
    ball_step(m);if(m->phase!=FB_LIVE_PLAY)return;
    carrier=&m->actors[m->carrier];
    if(!m->flying){
        if(carrier->x<=0 || carrier->x>=120 || carrier->y<=0 || carrier->y>=53.333f || (carrier->x>=110 && dir>0) || (carrier->x<=10 && dir<0)){
            finish(m,m->caught?FB_RESULT_COMPLETE:FB_RESULT_RUN,carrier->x);return;}
        if(cpu && m->cooldown<=0){
            float near=100;for(unsigned d=def;d<def+11;++d)near=fminf(near,distance(carrier->x,carrier->y,m->actors[d].x,m->actors[d].y));
            if(near<2.4f && random32(m)%100<8u+carrier->attributes.agi/12u){m->juke=.35f;m->cooldown=1.6f;football_live_fx(m,FB_FX_JUKE,m->carrier,22,carrier->vy<0?-1:1);} /* the CPU carrier jukes */
        }
        for(unsigned d=def;d<def+11;++d){FootballLiveActor *a=&m->actors[d];bool dive=d==m->controlled && (input.pressed&FB_LIVE_ACTION);
            if(dive)football_live_fx(m,FB_FX_DIVE,d,m->carrier,carrier->x>=a->x?1:-1);
            if(a->contact>0 || distance(a->x,a->y,carrier->x,carrier->y)>(dive?2.6f:1.7f))continue;
            a->contact=.35f;float chance=limit(53+(a->attributes.tak-carrier->attributes.agi)*.35f+(dive?15:0)-(m->juke>0?35:0),15,95);
            if(random32(m)%100<chance){
                if(random32(m)%1000<8){
                    /* Recovery belongs to the nearest actor at the actual loose-ball spot. */
                    unsigned recover=m->carrier;float nearest=2.5f;
                    for(unsigned j=0;j<22;++j)if(j!=m->carrier){float dist=distance(carrier->x,carrier->y,m->actors[j].x,m->actors[j].y);if(dist<nearest){nearest=dist;recover=j;}}
                    bool same=(recover<11)==(m->carrier<11);m->credited=(uint8_t)recover;football_live_fx(m,FB_FX_FUMBLE,m->carrier,d,carrier->x>=a->x?1:-1);finish(m,same?FB_RESULT_FUMBLE_RECOVERED:FB_RESULT_FUMBLE,carrier->x);return;
                }
                FootballResultKind kind=!m->thrown && p->type==FB_PLAY_PASS?FB_RESULT_SACK:m->caught?FB_RESULT_COMPLETE:FB_RESULT_RUN;
                if(kind==FB_RESULT_SACK)football_live_credit(m,d,FB_CREDIT_SACK);
                football_live_fx(m,kind==FB_RESULT_SACK?FB_FX_SACK:FB_FX_TACKLE,d,m->carrier,carrier->x>=a->x?1:-1);
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
