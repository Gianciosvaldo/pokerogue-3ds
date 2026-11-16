#include "football_sound.h"
#include <string.h>
/* Read-only: nothing here writes to the match. Sounds come from three kinds of change between two looks:
 *  - state edges: the snap (SET to PLAY), the ball leaving a hand or a foot (flying turns on), the end of the match;
 *  - new fx records (the engine already stamps every tackle, sack, catch, ... with the step it happened on, so a record is
 *    new when its slot holds another kind or step than last look; the ring is cleared with the next play);
 *  - a new dead ball (`snaps` counts them): what the rules and the event say it was. */
static bool home_actor(unsigned actor){return actor<11;}
void football_sound_reset(FootballSoundWatch *w,const FootballLive *m)
{
    if(!w)return;
    memset(w,0,sizeof *w);
    if(!m)return;
    w->seen=true;w->flying=m->flying;w->ended=m->phase==FB_LIVE_END;
    w->phase=(uint8_t)m->phase;w->offense=m->rules.offense;w->quarter=m->rules.quarter;w->snaps=m->snaps;
    w->score[0]=m->rules.score[0];w->score[1]=m->rules.score[1];
    for(unsigned i=0;i<FB_FX_COUNT;++i){w->fx_kind[i]=m->fx[i].kind;w->fx_step[i]=m->fx[i].step;}
}
static FootballSound fx_sound(const FootballFx *f,FootballSound *then)
{
    *then=FB_SND_NONE;
    switch(f->kind){
    case FB_FX_JUKE:return FB_SND_JUKE;
    case FB_FX_DIVE:return FB_SND_DIVE;
    case FB_FX_TACKLE:return FB_SND_TACKLE;
    case FB_FX_SACK:return FB_SND_SACK;
    case FB_FX_FUMBLE:return FB_SND_FUMBLE;
    case FB_FX_CATCH:return FB_SND_CATCH;
    case FB_FX_INCOMPLETE:return FB_SND_INCOMPLETE;
    case FB_FX_INTERCEPTION:*then=home_actor(f->actor)?FB_SND_TURNOVER_GAIN:FB_SND_TURNOVER_LOSS;return FB_SND_INTERCEPTION;
    case FB_FX_TOUCHDOWN:return home_actor(f->actor)?FB_SND_TOUCHDOWN_FOR:FB_SND_TOUCHDOWN_AGAINST;
    default:return FB_SND_NONE;
    }
}
unsigned football_sound_scan(FootballSoundWatch *w,const FootballLive *m,FootballSound out[FB_SOUND_MAX])
{
    if(!w || !m || !out)return 0;
    if(!w->seen){football_sound_reset(w,m);return 0;}
    unsigned n=0;
#define PUSH(s) do{if(n<FB_SOUND_MAX && (s)!=FB_SND_NONE)out[n++]=(s);}while(0)
    /* The snap of a scrimmage play (a kick makes its own sound when the ball leaves the foot). */
    if(w->phase!=FB_LIVE_PLAY && m->phase==FB_LIVE_PLAY && !m->special && !m->turnover)PUSH(FB_SND_SNAP);
    /* The ball leaves the passer's hand or the kicker's foot. */
    if(!w->flying && m->flying)PUSH(m->special?FB_SND_KICK:FB_SND_THROW);
    /* New fx records, oldest first (ring order breaks a tie within one step). */
    unsigned fresh[FB_FX_COUNT],count=0;
    for(unsigned i=0;i<FB_FX_COUNT;++i){
        if(m->fx[i].kind && (m->fx[i].kind!=w->fx_kind[i] || m->fx[i].step!=w->fx_step[i]))fresh[count++]=i;
        w->fx_kind[i]=m->fx[i].kind;w->fx_step[i]=m->fx[i].step;
    }
    for(unsigned a=1;a<count;++a){unsigned v=fresh[a],b=a;while(b>0 && m->fx[fresh[b-1]].step>m->fx[v].step){fresh[b]=fresh[b-1];--b;}fresh[b]=v;}
    for(unsigned i=0;i<count;++i){FootballSound then;FootballSound s=fx_sound(&m->fx[fresh[i]],&then);PUSH(s);PUSH(then);}
    /* A new dead ball. */
    if(m->snaps!=w->snaps){
        const FootballRuleEvents *last=&m->rules.last;
        bool gain_home=m->rules.score[FB_HOME]>w->score[FB_HOME],gain_away=m->rules.score[FB_AWAY]>w->score[FB_AWAY];
        switch(m->event){
        case FB_EVENT_FIELD_GOAL:case FB_EVENT_EXTRA_POINT:case FB_EVENT_TWO_POINT:case FB_EVENT_SAFETY:
            PUSH(gain_home?FB_SND_SCORE_FOR:gain_away?FB_SND_SCORE_AGAINST:FB_SND_NONE);break;
        case FB_EVENT_FIELD_GOAL_MISSED:case FB_EVENT_EXTRA_POINT_MISSED:case FB_EVENT_TWO_POINT_FAILED:PUSH(FB_SND_NO_GOOD);break;
        case FB_EVENT_BLOCKED:PUSH(FB_SND_BLOCKED);break;
        case FB_EVENT_MUFF:PUSH(FB_SND_MUFF);break;
        case FB_EVENT_FAIR_CATCH:PUSH(FB_SND_CATCH);break;
        default:break;
        }
        /* A first down for the human; a ball lost on downs or to a fumble, told by who has it now. */
        if(m->event==FB_EVENT_NONE && last->first_down && m->rules.offense==FB_HOME)PUSH(FB_SND_FIRST_DOWN);
        if((m->event==FB_EVENT_NONE || m->event==FB_EVENT_FUMBLE) && last->turnover)PUSH(m->rules.offense==FB_HOME?FB_SND_TURNOVER_GAIN:FB_SND_TURNOVER_LOSS);
        if(last->quarter_end && !m->rules.finished)PUSH(FB_SND_PERIOD_END);
    }
    /* The final whistle, once. */
    if(m->phase==FB_LIVE_END && !w->ended){PUSH(m->rules.winner==FB_HOME?FB_SND_WIN:FB_SND_LOSS);w->ended=true;}
#undef PUSH
    w->flying=m->flying;w->phase=(uint8_t)m->phase;w->offense=m->rules.offense;w->quarter=m->rules.quarter;w->snaps=m->snaps;
    w->score[0]=m->rules.score[0];w->score[1]=m->rules.score[1];
    return n;
}
const char *football_sound_key(FootballSound s)
{
    /* Only keys the app already plays from upstream's pack (battle, hatching, results), so every one is a sound the pack is known to hold. */
    switch(s){
    case FB_SND_SNAP:return "se/hit_weak";
    case FB_SND_THROW:return "se/pb_throw";
    case FB_SND_KICK:return "se/hit_strong";
    case FB_SND_CATCH:return "se/pb_catch";
    case FB_SND_INCOMPLETE:return "se/pb_bounce_1";
    case FB_SND_JUKE:return "se/pb_move";
    case FB_SND_DIVE:return "se/pb_move";
    case FB_SND_TACKLE:return "se/hit";
    case FB_SND_SACK:return "se/hit_strong";
    case FB_SND_FUMBLE:return "se/hit_strong";
    case FB_SND_INTERCEPTION:return "se/pb_lock";
    case FB_SND_BLOCKED:return "se/hit_strong";
    case FB_SND_MUFF:return "se/pb_bounce_1";
    case FB_SND_TOUCHDOWN_FOR:return "se/level_up_fanfare";
    case FB_SND_TOUCHDOWN_AGAINST:return "se/stat_down";
    case FB_SND_SCORE_FOR:return "se/level_up";
    case FB_SND_SCORE_AGAINST:return "se/stat_down";
    case FB_SND_NO_GOOD:return "ui/error";
    case FB_SND_FIRST_DOWN:return "se/stat_up";
    case FB_SND_TURNOVER_GAIN:return "se/stat_up";
    case FB_SND_TURNOVER_LOSS:return "se/stat_down";
    case FB_SND_PERIOD_END:return "ui/menu_open";
    case FB_SND_WIN:return "se/achv";
    case FB_SND_LOSS:return "se/faint";
    default:return NULL;
    }
}
