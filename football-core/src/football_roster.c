#include "football_roster.h"
#include <string.h>
const char *const football_slot_names[22]={"QB","RB","WR1","WR2","WR3","TE","LT","LG","C","RG","RT","DE1","DT1","DT2","DE2","LB1","LB2","LB3","CB1","CB2","FS","SS"};
const char *const football_slot_titles[22]={"Quarterback","Running back","Wide receiver 1","Wide receiver 2","Wide receiver 3","Tight end","Left tackle","Left guard","Center","Right guard","Right tackle","Defensive end 1","Defensive tackle 1","Defensive tackle 2","Defensive end 2","Linebacker 1","Linebacker 2","Linebacker 3","Cornerback 1","Cornerback 2","Free safety","Strong safety"};
const char *football_position_label(FootballPosition p)
{
    static const char *const labels[FB_POSITION_COUNT]={"QB","WR","RB","OL/DL","LB","TE/S"};
    return (unsigned)p<FB_POSITION_COUNT?labels[p]:"";
}
FootballPosition football_slot_position(unsigned s)
{
    if(s==0)return FB_QB;
    if(s==1)return FB_RB;
    if(s>=2 && s<=4)return FB_WR;
    if(s==5 || s>=18)return FB_TE_S;
    if(s>=15 && s<=17)return FB_LB;
    return FB_OL_DL;
}
bool football_roster_init(FootballRoster *r)
{
    if(!r)return false;
    memset(r,0,sizeof *r);
    for(unsigned i=0;i<22;++i)r->lineup[i]=(uint8_t)i;
    return football_stats_build_maxima(&r->maxima,pr_forms,PR_FORM_COUNT);
}
bool football_roster_refresh(FootballRoster *r,unsigned i)
{
    if(!r || r->count>22 || i>=22 || i>=r->count)return false;
    const pr_form *f=NULL;
    for(unsigned j=0;j<PR_FORM_COUNT;++j)if(pr_forms[j].species==r->mon[i].b.species && pr_forms[j].form_index==r->mon[i].b.form){f=&pr_forms[j];break;}
    if(!f || !football_stats_generate(&r->player[i],r->mon[i].b.id,f,&r->maxima))return false;
    unsigned a=r->player[i].attributes.spd+r->speed[i],b=r->player[i].attributes.str+r->strength[i];
    r->player[i].attributes.spd=(uint8_t)(a>99?99:a);r->player[i].attributes.str=(uint8_t)(b>99?99:b);
    return true;
}
bool football_roster_starter(const gc_system *profile,unsigned species,gc_starter *out)
{
    if(!profile || !out || species>GC_MAX_SPECIES_ID || !profile->dex[species].caught_attr)return false;
    const pr_species *sp=NULL;for(unsigned i=0;i<PR_SPECIES_COUNT;++i)if(pr_species_table[i].id==species)sp=&pr_species_table[i];
    if(!sp || sp->prevolution>0 || gc_starter_cost(species)<0)return false;
    gc_starter starter={.species=(uint16_t)species,.tera_type=-1};
    uint64_t caught=profile->dex[species].caught_attr;
    starter.female=sp->male_permille==0 || (!(caught&4) && (caught&8));
    if(!(caught&1)){starter.shiny=true;while(starter.variant<2 && !(caught&(UINT64_C(1)<<(4+starter.variant))))++starter.variant;}
    bool nature=false;for(unsigned n=0;n<25;++n)if(profile->dex[species].nature_attr&(1u<<(n+1))){starter.nature=(uint8_t)n;nature=true;break;}
    if(!nature)return false;
    memcpy(starter.ivs,profile->dex[species].ivs,sizeof starter.ivs);
    for(unsigned ability=0;ability<3;++ability){starter.ability_index=(uint8_t)ability;
        if(gc_starter_is_available(profile,&starter,NULL,NULL)){*out=starter;return true;}
    }
    return false;
}
bool football_roster_add(FootballRoster *r,gc_run *scratch,const gc_starter *s,const gc_system *profile,const char **why)
{
    const char *error="Draft needs 22 base-form Pokemon within 55 points.";
    if(!r || !scratch || !s || !profile || r->count>=22)goto bad;
    const pr_species *species=NULL;
    for(unsigned i=0;i<PR_SPECIES_COUNT;++i)if(pr_species_table[i].id==s->species)species=&pr_species_table[i];
    if(!species || species->prevolution>0 || s->form)goto bad;
    for(unsigned i=0;i<r->count;++i)if(r->mon[i].b.species==s->species){error="That species is already drafted.";goto bad;}
    double cost;
    if(!gc_starter_is_available(profile,s,&cost,why))return false;
    if(cost<0 || r->cost+cost>FB_ROSTER_BUDGET+1e-9)goto bad;
    if(!gc_football_create_member(scratch,s,why))return false;
    unsigned i=r->count++;r->mon[i]=scratch->party[0];
    if(!football_roster_refresh(r,i)){--r->count;goto bad;}
    r->cost+=cost;return true;
bad: if(why)*why=error;return false;
}
bool football_roster_assign(FootballRoster *r,unsigned slot,unsigned member)
{
    if(!r || r->count>22 || slot>=r->count || member>=r->count)return false;
    for(unsigned i=0;i<r->count;++i)if(r->lineup[i]==member){uint8_t old=r->lineup[slot];r->lineup[slot]=(uint8_t)member;r->lineup[i]=old;return true;}
    return false;
}
void football_roster_autofill(FootballRoster *r)
{
    if(!r || r->count>22)return;
    for(unsigned s=0;s<r->count;++s){
        unsigned best=s;float rating=-1;
        for(unsigned i=s;i<r->count;++i){const FootballPlayer *p=&r->player[r->lineup[i]];const FootballAttributes *a=&p->attributes;
            FootballPosition pos=football_slot_position(s);float value=0;
            switch(pos){case FB_QB:value=a->tha*.55f+a->thp*.45f;break;case FB_WR:value=a->cth*.6f+a->spd*.4f;break;case FB_RB:value=a->agi*.5f+a->spd*.3f+a->str*.2f;break;case FB_OL_DL:value=a->str*.6f+a->tak*.4f;break;case FB_LB:value=a->tak*.6f+a->agi*.4f;break;default:value=a->cth*.4f+a->tak*.3f+a->spd*.3f;break;}
            float affinity=football_stats_type_affinity(p->primary_type,pos);
            if(p->secondary_type>=0 && p->secondary_type!=p->primary_type)affinity=(affinity+football_stats_type_affinity(p->secondary_type,pos))*.5f;
            value*=affinity;
            if(value>rating){rating=value;best=i;}
        }
        uint8_t t=r->lineup[s];r->lineup[s]=r->lineup[best];r->lineup[best]=t;
    }
}
