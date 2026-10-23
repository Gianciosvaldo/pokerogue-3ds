#include "football_save.h"
#include <string.h>
typedef struct { unsigned char *bytes; size_t pos,cap; bool read,ok; } Codec;
static uint32_t checksum(const unsigned char *p,size_t n)
{
    uint32_t crc=UINT32_MAX;
    for(size_t i=0;i<n;++i) { crc^=p[i];for(unsigned bit=0;bit<8;++bit)crc=(crc>>1)^(0xedb88320u & (0u-(crc&1))); }
    return ~crc;
}
static uint32_t value(Codec *c,uint32_t v,uint32_t max)
{
    if (!c->ok || c->pos>c->cap || c->cap-c->pos<4) { c->ok=false;return 0; }
    if(c->read) { v=0;for(unsigned i=0;i<4;++i)v|=(uint32_t)c->bytes[c->pos+i]<<(8*i); }
    else for(unsigned i=0;i<4;++i)c->bytes[c->pos+i]=(unsigned char)(v>>(8*i));
    c->pos+=4;if(v>max)c->ok=false;return v;
}
#define V(field,max) do { (field)=value(c,(uint32_t)(field),(max)); } while(0)
static void player(Codec *c,FootballPlayer *p)
{
    V(p->roster_id,UINT32_MAX);V(p->species_id,UINT16_MAX);V(p->form_index,255);
    uint32_t secondary=(uint32_t)(p->secondary_type+1);V(secondary,PR_TYPE_COUNT);p->secondary_type=(int8_t)(secondary-1);
    V(p->primary_type,PR_TYPE_COUNT-1);V(p->position,FB_POSITION_COUNT-1);
    V(p->attributes.spd,99);if(!p->attributes.spd)c->ok=false;
    V(p->attributes.agi,99);if(!p->attributes.agi)c->ok=false;
    V(p->attributes.str,99);if(!p->attributes.str)c->ok=false;
    V(p->attributes.tak,99);if(!p->attributes.tak)c->ok=false;
    V(p->attributes.sta,99);if(!p->attributes.sta)c->ok=false;
    V(p->attributes.thp,99);if(!p->attributes.thp)c->ok=false;
    V(p->attributes.tha,99);if(!p->attributes.tha)c->ok=false;
    V(p->attributes.cth,99);if(!p->attributes.cth)c->ok=false;
    if(c->read) {
        bool found=false;for(unsigned i=0;i<PR_FORM_COUNT;++i)
            if(pr_forms[i].species==p->species_id && pr_forms[i].form_index==p->form_index)found=true;
        if(!found)c->ok=false;
    }
}
static void match(Codec *c,FootballMatch *m)
{
    if(c->read)memset(m->energy,100,sizeof m->energy);
    for(unsigned side=0;side<2;++side) {
        V(m->teams[side].count,6);if(!m->teams[side].count)c->ok=false;
        for(unsigned i=0;i<m->teams[side].count && i<6;++i) { player(c,&m->teams[side].players[i]);V(m->energy[side][i],100); }
    }
    V(m->rng,UINT32_MAX);
    V(m->difficulty_q8,640);
    V(m->plays,512);
    V(m->rules.score[0],UINT16_MAX);
    V(m->rules.score[1],UINT16_MAX);
    V(m->rules.seconds_left,120);
    V(m->rules.quarter,5);
    V(m->rules.offense,1);
    V(m->rules.down,4);
    V(m->rules.ball,99);
    V(m->rules.first_down_at,100);
    V(m->rules.overtime_drives,8);
    V(m->rules.tiebreak_side,1);
    if(m->difficulty_q8<256 || !m->rules.quarter || !m->rules.down || !m->rules.ball ||
        m->rules.first_down_at<=m->rules.ball || !m->rng)c->ok=false;
    m->phase=FB_MATCH_CALL;m->manual_defense=true;
}
static void state(Codec *c,FootballMode *m)
{
    V(m->phase,FB_MODE_END);V(m->season.phase,FB_SEASON_ELIMINATED);
    V(m->season.round,7);V(m->season.wins,7);V(m->season.wallet,1000000);V(m->season.rng,UINT32_MAX);
    uint32_t mega=(uint32_t)(m->season.mega_slot+1);V(mega,6);m->season.mega_slot=(int8_t)(mega-1);
    for(unsigned i=0;i<6;++i) { V(m->season.speed_boost[i],20);V(m->season.strength_boost[i],20); }
    V(m->season.shop_count,3);
    for(unsigned i=0;i<m->season.shop_count && i<3;++i) {
        FootballShopOffer *o=&m->season.shop[i];V(o->item,FB_ITEM_MEGA_STONE);V(o->price,1000);
        uint32_t arg=(uint32_t)(o->classic_arg+1),target=(uint32_t)(o->target+1);V(arg,UINT16_MAX);V(target,6);
        o->classic_arg=(int16_t)(arg-1);o->target=(int8_t)(target-1);V(o->sold,1);
    }
    if(m->phase==FB_MODE_PLAY)match(c,&m->session.match);
}
#undef V
bool football_save_ready(const FootballMode *m)
{
    return m && m->season.roster && (!m->done || m->phase==FB_MODE_END) &&
        ((m->phase==FB_MODE_ROUND && m->season.phase==FB_SEASON_READY) ||
         (m->phase==FB_MODE_SHOP && m->season.phase==FB_SEASON_SHOP) ||
         (m->phase==FB_MODE_PLAY && m->season.phase==FB_SEASON_MATCH && m->session.active &&
          m->session.match.phase==FB_MATCH_CALL && !m->session.match.rules.finished) ||
         (m->phase==FB_MODE_END && (m->season.phase==FB_SEASON_CLEAR || m->season.phase==FB_SEASON_ELIMINATED)));
}
size_t football_save_encode(const FootballMode *m,void *buffer,size_t capacity)
{
    if(!buffer || capacity<4096 || !football_save_ready(m))return 0;
    FootballMode copy=*m;
    Codec c={(unsigned char *)buffer,20,capacity,false,true};state(&c,&copy);
    if(!c.ok || c.pos>=capacity)return 0;
    size_t roster=gc_football_roster_save(m->season.roster,c.bytes+c.pos,capacity-c.pos);
    if(!roster)return 0;
    Codec header={buffer,8,20,false,true};value(&header,1,1);value(&header,(uint32_t)c.pos,4096);value(&header,checksum(c.bytes+20,c.pos+roster-20),UINT32_MAX);
    memcpy(buffer,"PRFBMODE",8);return c.pos+roster;
}
bool football_save_decode(FootballMode *m,gc_run *roster,gc_system *profile,const void *buffer,size_t length)
{
    if(!m || !roster || !buffer || length<20 || length>FB_SAVE_MAX || memcmp(buffer,"PRFBMODE",8))return false;
    Codec header={(unsigned char *)buffer,8,length,true,true};uint32_t version=value(&header,0,UINT32_MAX),offset=value(&header,0,4096),crc=value(&header,0,UINT32_MAX);
    if(version!=1 || offset<20 || offset>=length || checksum((const unsigned char *)buffer+20,length-20)!=crc)return false;
    memset(m,0,sizeof *m);m->season.mega_slot=-1;
    Codec c={(unsigned char *)buffer,20,offset,true,true};state(&c,m);
    if(!c.ok || c.pos!=offset || !m->season.round || !m->season.rng ||
       !gc_football_roster_load(roster,profile,(const unsigned char *)buffer+offset,length-offset))return false;
    m->season.roster=roster;
    if(!football_stats_build_maxima(&m->season.maxima,pr_forms,PR_FORM_COUNT))return false;
    for(unsigned i=0;i<(unsigned)roster->n_party;++i) {
        const pr_form *f=NULL;for(unsigned k=0;k<PR_FORM_COUNT;++k)
            if(pr_forms[k].species==roster->party[i].b.species && pr_forms[k].form_index==roster->party[i].b.form)f=&pr_forms[k];
        if(!f || !football_stats_generate(&m->season.players[i],roster->party[i].b.id,f,&m->season.maxima))return false;
        unsigned speed=m->season.players[i].attributes.spd+m->season.speed_boost[i],strength=m->season.players[i].attributes.str+m->season.strength_boost[i];
        m->season.players[i].attributes.spd=(uint8_t)(speed>99?99:speed);m->season.players[i].attributes.str=(uint8_t)(strength>99?99:strength);
        m->season.players[i].position=football_stats_best_position(&m->season.players[i]);
    }
    if(m->season.mega_slot>=roster->n_party || (m->season.mega_slot>=0 && !gc_football_owned_mega(roster,(unsigned)m->season.mega_slot)))return false;
    if(m->phase==FB_MODE_END && ((m->season.phase==FB_SEASON_CLEAR && (m->season.round!=7 || m->season.wins!=7)) ||
       (m->season.phase==FB_SEASON_ELIMINATED && m->season.wins!=m->season.round-1)))return false;
    if(m->phase==FB_MODE_ROUND && m->season.wins!=m->season.round-1)return false;
    if(m->phase==FB_MODE_SHOP && (m->season.wins!=m->season.round || m->season.shop_count<2))return false;
    for(unsigned i=0;i<m->season.shop_count;++i) {
        FootballShopOffer *o=&m->season.shop[i];
        if(o->item==FB_ITEM_MEGA_STONE && (o->target<0 || o->target>=roster->n_party || !gc_football_mega_form(roster,(unsigned)o->target,o->classic_arg)))return false;
        if(o->item!=FB_ITEM_MEGA_STONE && o->classic_arg!=(o->item==FB_ITEM_CARBOS?STAT_SPD:STAT_ATK))return false;
    }
    if(m->phase==FB_MODE_PLAY) {
        FootballMatch *match=&m->session.match;
        if(match->teams[0].count!=roster->n_party || m->season.wins!=m->season.round-1)return false;
        for(unsigned i=0;i<match->teams[0].count;++i) {
            FootballPlayer expected;
            if(!football_season_match_player(&m->season,i,&expected) ||
               expected.roster_id!=match->teams[0].players[i].roster_id ||
               expected.species_id!=match->teams[0].players[i].species_id || expected.form_index!=match->teams[0].players[i].form_index ||
               expected.primary_type!=match->teams[0].players[i].primary_type || expected.secondary_type!=match->teams[0].players[i].secondary_type ||
               expected.position!=match->teams[0].players[i].position || memcmp(&expected.attributes,&match->teams[0].players[i].attributes,sizeof expected.attributes))return false;
        }
        m->session.active=true;m->session.owner=&m->season;m->session.round=m->season.round;
    }
    return football_save_ready(m);
}
