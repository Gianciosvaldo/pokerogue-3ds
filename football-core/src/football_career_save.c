#include "football_career.h"
#include <stdlib.h>
#include <string.h>
typedef struct { unsigned char *p;size_t n,cap;bool read,ok; } Stream;
static uint32_t crc(const unsigned char *p,size_t n){uint32_t v=~0u;while(n--){v^=*p++;for(unsigned i=0;i<8;++i)v=(v>>1)^(0xedb88320u & (0u-(v&1)));}return ~v;}
static uint32_t number(Stream *s,uint32_t v,uint32_t max)
{
    if(!s->ok || s->n>s->cap || s->cap-s->n<4){s->ok=false;return 0;}
    if(s->read){v=0;for(unsigned i=0;i<4;++i)v|=(uint32_t)s->p[s->n+i]<<(8*i);}else for(unsigned i=0;i<4;++i)s->p[s->n+i]=(unsigned char)(v>>(8*i));
    s->n+=4;if(v>max){s->ok=false;return 0;}return v;
}
#define N(x,max) do{(x)=number(s,(uint32_t)(x),max);}while(0)
static void state(Stream *s,FootballCareer *c)
{
    N(c->phase,FB_CAREER_END);N(c->round,7);N(c->wins,7);N(c->wallet,5600);N(c->seed,UINT32_MAX);
    unsigned mega=(unsigned)(c->mega+1);N(mega,22);c->mega=(int)mega-1;
    N(c->roster.count,22);unsigned cost=(unsigned)(c->roster.cost*1000+.5);N(cost,FB_ROSTER_BUDGET*1000);c->roster.cost=cost/1000.0;
    unsigned seen=0;
    for(unsigned i=0;i<c->roster.count;++i){
        N(c->roster.lineup[i],21);N(c->roster.speed[i],20);N(c->roster.strength[i],20);N(c->roster.mega_item[i],INT16_MAX);
        if(seen&(1u<<c->roster.lineup[i]))s->ok=false;
        seen|=1u<<c->roster.lineup[i];
        size_t length=0;
        if(!s->read){
            gc_football_new(c->scratch,NULL,"live-save",c->seed+i);c->scratch->party[0]=c->roster.mon[i];c->scratch->n_party=1;c->scratch->party_order[0]=0;c->scratch->screen.kind=GC_SCREEN_NONE;
            if(s->cap-s->n>=4)length=gc_football_roster_save(c->scratch,s->p+s->n+4,s->cap-s->n-4);
        }
        unsigned packet=number(s,(uint32_t)length,65536);
        if(!packet || packet>s->cap-s->n){s->ok=false;return;}
        if(s->read){if(!gc_football_roster_load(c->scratch,NULL,s->p+s->n,packet) || c->scratch->n_party!=1){s->ok=false;return;}c->roster.mon[i]=c->scratch->party[0];}
        s->n+=packet;
    }
    for(unsigned i=0;i<3;++i){N(c->prices[i],1000);N(c->sold[i],1);}
    unsigned target=(unsigned)(c->stone_target+1);N(target,22);c->stone_target=(int)target-1;N(c->stone_item,INT16_MAX);
    if(c->phase==FB_CAREER_MATCH){
        FootballLive *m=&c->live;FootballRules *r=&m->rules;
        N(m->rng,UINT32_MAX);N(m->snaps,4096);
        N(r->score[0],10000);N(r->score[1],10000);N(r->seconds_left,120);N(r->quarter,5);N(r->offense,1);N(r->down,4);N(r->ball,99);N(r->first_down_at,100);N(r->overtime_drives,7);N(r->tiebreak_side,1);
        if(!m->rng || !r->quarter || !r->down || !r->ball || r->first_down_at<=r->ball || (r->quarter<5 && !r->seconds_left))s->ok=false;
        for(unsigned team=0;team<2;++team)for(unsigned i=0;i<22;++i){
            uint8_t *a=(uint8_t *)&m->units[team][i];for(unsigned k=0;k<sizeof(FootballAttributes);++k){N(a[k],99);if(!a[k])s->ok=false;}
            m->lineup[team][i]=team?(uint8_t)i:c->roster.lineup[i];
        }
        for(unsigned i=0;i<22;++i){N(m->away_species[i],UINT16_MAX);N(m->away_form[i],255);
            if(s->read){bool found=false;for(unsigned j=0;j<PR_FORM_COUNT;++j)if(pr_forms[j].species==m->away_species[i] && pr_forms[j].form_index==m->away_form[i])found=true;if(!found)s->ok=false;}
        }
        m->phase=FB_LIVE_CALL;m->pending_receiver=-1;
        if(s->read){if(!football_live_call(m,FB_PASS_SLANTS))s->ok=false;m->phase=FB_LIVE_CALL;}
    }
}
#undef N
bool football_career_save_ready(const FootballCareer *c)
{
    return c && c->scratch && c->roster.count==22 && (c->phase==FB_CAREER_LINEUP || c->phase==FB_CAREER_READY || c->phase==FB_CAREER_SHOP || c->phase==FB_CAREER_END || (c->phase==FB_CAREER_MATCH && c->live.phase==FB_LIVE_CALL));
}
size_t football_career_save(FootballCareer *c,void *buffer,size_t capacity)
{
    if(!football_career_save_ready(c) || !buffer || capacity<4096 || capacity>FB_CAREER_SAVE_MAX)return 0;
    Stream s={buffer,20,capacity,false,true};state(&s,c);if(!s.ok)return 0;
    Stream header={buffer,8,20,false,true};number(&header,1,1);number(&header,(uint32_t)s.n,FB_CAREER_SAVE_MAX);number(&header,crc((unsigned char *)buffer+20,s.n-20),UINT32_MAX);memcpy(buffer,"PRFBLIVE",8);c->scratch->sys=c->profile;return s.n;
}
bool football_career_load(FootballCareer *c,gc_run *scratch,gc_system *profile,const void *buffer,size_t length)
{
    if(!c || !scratch || !profile || !buffer || length<20 || length>FB_CAREER_SAVE_MAX || memcmp(buffer,"PRFBLIVE",8))return false;
    Stream s={(unsigned char *)buffer,8,length,true,true};unsigned version=number(&s,0,1),size=number(&s,0,FB_CAREER_SAVE_MAX),sum=number(&s,0,UINT32_MAX);
    if(!s.ok || version!=1 || size!=length || sum!=crc((const unsigned char *)buffer+20,length-20))return false;
    FootballCareer *copy=calloc(1,sizeof *copy);if(!copy)return false;
    bool valid=football_career_new(copy,scratch,NULL,1);state(&s,copy);
    valid=valid && s.ok && s.n==length && copy->round>0 && copy->seed && football_career_save_ready(copy) && copy->wins<=copy->round;
    if((copy->phase==FB_CAREER_READY || copy->phase==FB_CAREER_LINEUP || copy->phase==FB_CAREER_MATCH) && copy->wins+1!=copy->round)valid=false;
    if(copy->phase==FB_CAREER_SHOP && copy->wins!=copy->round)valid=false;
    for(unsigned i=0;valid && i<22;++i){
        if(!football_roster_refresh(&copy->roster,i))valid=false;
        gc_starter original;
        if(profile && !football_roster_starter(profile,copy->roster.mon[i].met_species,&original))valid=false;
        if(copy->roster.mega_item[i]){gc_football_new(scratch,NULL,"check-mega",1);scratch->party[0]=copy->roster.mon[i];scratch->n_party=1;if(!gc_football_mega_form(scratch,0,copy->roster.mega_item[i]))valid=false;}
    }
    if(valid && copy->mega>=0 && !copy->roster.mega_item[copy->mega])valid=false;
    if(valid && copy->stone_target>=0){
        gc_football_new(scratch,NULL,"check-offer",1);scratch->party[0]=copy->roster.mon[copy->stone_target];scratch->n_party=1;
        if(!gc_football_mega_form(scratch,0,copy->stone_item))valid=false;
    }
    if(valid){copy->profile=profile;*c=*copy;scratch->sys=profile;}
    free(copy);return valid;
}
