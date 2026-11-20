/* A fused Pokemon (DNA Splicers, Spliced Endless) is named as both halves on the battle info box, like upstream's
 * getNameToRender -> getFusedSpeciesName; an unfused one and a disguise keep their own names. */
#include "battle_events.h"
#include "battle_setup.h"
#include "fused_name.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int checks;
#define CHECK(x) do {checks++;if(!(x)){fprintf(stderr,"Fused name line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
const char *i18n_language(void){return "en";}
const char *i18n_species_name(int id){
    static char ring[4][40];static int at;char *n=ring[at++&3];
    switch(id){
        case SPECIES_ID_PURRLOIN:return strcpy(n,"Purrloin");
        case SPECIES_ID_TENTACRUEL:return strcpy(n,"Tentacruel");
        case SPECIES_ID_DITTO:return strcpy(n,"Ditto");
        case SPECIES_ID_ZORUA:return strcpy(n,"Zorua");
        default:snprintf(n,40,"Species_%d",id);return n;
    }
}
char *pak_read(const char *key,size_t *len){(void)key;if(len)*len=0;return NULL;}
void bv_play(const bv_event *ev,int n,void (*done)(void *),void *arg){(void)ev;(void)n;(void)done;(void)arg;}
int main(void){
    static bs_battle b;bs_begin(&b,BIOME_ID_GRASS,3,"fused-name");
    int moves[]={MOVE_ID_SPLASH};
    pr_pokemon *plain=bs_add(&b,0,SPECIES_ID_PURRLOIN,20,moves,1);
    pr_pokemon *fused=bs_add(&b,1,SPECIES_ID_PURRLOIN,20,moves,1);
    fused->fusion_species=SPECIES_ID_TENTACRUEL;
    char want[128];pr_fused_species_name("Purrloin","Tentacruel",want,sizeof want);
    CHECK(strcmp(want,"Purrloin")&&strcmp(want,"Tentacruel")&&strlen(want)>8); /* a blend, neither half alone */
    bv_mon m;
    be_mon(&b.bt,plain,&m);CHECK(!strcmp(m.name,"Purrloin"));
    be_mon(&b.bt,fused,&m);CHECK(!strcmp(m.name,want));
    CHECK(m.species==SPECIES_ID_PURRLOIN); /* the sprite species is still the first half */
    char direct[40];fused_mon_name(direct,sizeof direct,SPECIES_ID_PURRLOIN,SPECIES_ID_TENTACRUEL);CHECK(!strcmp(direct,want));
    fused_mon_name(direct,sizeof direct,SPECIES_ID_ZORUA,0);CHECK(!strcmp(direct,"Zorua"));
    /* nicknames (getNameToRender: the nickname when there is one): it replaces a fusion's blended name, a disguise hides it and the real Pokemon is
     * named by it again when the disguise breaks, and the species name is back once it is cleared */
    char tiny[6];
    mon_display_name(direct,sizeof direct,SPECIES_ID_PURRLOIN,0,NULL);CHECK(!strcmp(direct,"Purrloin"));
    mon_display_name(direct,sizeof direct,SPECIES_ID_PURRLOIN,0,"");CHECK(!strcmp(direct,"Purrloin"));
    mon_display_name(direct,sizeof direct,SPECIES_ID_PURRLOIN,SPECIES_ID_TENTACRUEL,"");CHECK(!strcmp(direct,want));
    mon_display_name(direct,sizeof direct,SPECIES_ID_PURRLOIN,SPECIES_ID_TENTACRUEL,"Sparky");CHECK(!strcmp(direct,"Sparky"));
    mon_display_name(tiny,sizeof tiny,SPECIES_ID_PURRLOIN,0,"Sparky Jr");CHECK(!strcmp(tiny,"Spark"));   /* a small buffer is cut, never overrun */
    strcpy(plain->nickname,"Sparky");
    be_mon(&b.bt,plain,&m);CHECK(!strcmp(m.name,"Sparky")&&m.species==SPECIES_ID_PURRLOIN);
    strcpy(fused->nickname,"Sparky Jr");
    be_mon(&b.bt,fused,&m);CHECK(!strcmp(m.name,"Sparky Jr")&&m.fusion_species==SPECIES_ID_TENTACRUEL);
    memset(plain->nickname,'a',PR_NICKNAME_BYTES-1);plain->nickname[PR_NICKNAME_BYTES-1]=0;   /* the whole field, 80 bytes */
    be_mon(&b.bt,plain,&m);CHECK(strlen(m.name)==PR_NICKNAME_BYTES-1&&sizeof m.name>PR_NICKNAME_BYTES-1);
    memset(plain->nickname,0,sizeof plain->nickname);
    for(int i=0;i<10;i++)strcat(plain->nickname,"\xF0\x9F\x94\xA5");                                  /* ten emoji, 20 UTF-16 units, 40 bytes */
    be_mon(&b.bt,plain,&m);CHECK(!strcmp(m.name,plain->nickname));
    plain->nickname[0]=0;fused->nickname[0]=0;
    be_mon(&b.bt,plain,&m);CHECK(!strcmp(m.name,"Purrloin"));
    be_mon(&b.bt,fused,&m);CHECK(!strcmp(m.name,want));
    {
        strcpy(plain->nickname,"Sparky");
        pr_set_illusion_species(plain,SPECIES_ID_ZORUA,0);
        pr_battle disguised=b.bt;disguised.deviations|=PR_DEV_ILLUSION;
        be_mon(&disguised,plain,&m);CHECK(m.illusion&&m.species==SPECIES_ID_ZORUA&&!strcmp(m.name,"Zorua"));   /* the disguise's name, not the nickname */
        be_view view={0};bv_event out[8];pr_event events[1]={0};
        view.present[0]=true;view.mon[0]=m;
        events[0].type=PR_EV_ILLUSION_BREAK;events[0].battler=0;
        events[0].field[0]=(pr_event_field){.species=SPECIES_ID_PURRLOIN,.pokemon_id=plain->id,.pokemon_id_known=true,.hp=plain->hp,.max_hp=plain->stats[STAT_HP],.visible=true};
        int n=be_convert_events(&disguised,events,1,false,&view,out,8);
        bool found=false;
        for(int i=0;i<n;i++)if(out[i].type==BEV_ILLUSION){found=true;CHECK(!out[i].mon->illusion&&!strcmp(out[i].mon->name,"Sparky"));}
        CHECK(found);
        /* without a nickname the real Pokemon is named by its species, as before */
        plain->nickname[0]=0;
        view.mon[0]=m;
        n=be_convert_events(&disguised,events,1,false,&view,out,8);
        found=false;
        for(int i=0;i<n;i++)if(out[i].type==BEV_ILLUSION){found=true;CHECK(!out[i].mon->illusion&&!strcmp(out[i].mon->name,"Purrloin"));}
        CHECK(found);
    }
    {   /* sent out as a form it changed from later in the same events: the nickname stays, a plain Pokemon is named by that form's species */
        be_view view={0};bv_event out[16];pr_event events[1]={0};
        events[0].type=PR_EV_SWITCH_IN;events[0].battler=0;
        events[0].field[0]=(pr_event_field){.species=SPECIES_ID_ZORUA,.pokemon_id=plain->id,.pokemon_id_known=true,.hp=plain->hp,.max_hp=plain->stats[STAT_HP],.visible=true};
        strcpy(plain->nickname,"Sparky");
        int n=be_convert_events(&b.bt,events,1,false,&view,out,16);
        bool found=false;
        for(int i=0;i<n;i++)if(out[i].type==BEV_SUMMON&&out[i].battler==0){found=true;CHECK(out[i].mon->species==SPECIES_ID_ZORUA&&!strcmp(out[i].mon->name,"Sparky"));}
        CHECK(found);
        plain->nickname[0]=0;memset(&view,0,sizeof view);
        n=be_convert_events(&b.bt,events,1,false,&view,out,16);
        found=false;
        for(int i=0;i<n;i++)if(out[i].type==BEV_SUMMON&&out[i].battler==0){found=true;CHECK(out[i].mon->species==SPECIES_ID_ZORUA&&!strcmp(out[i].mon->name,"Zorua"));}
        CHECK(found);
        /* under a disguise the same form change is named by the species, not the nickname */
        strcpy(plain->nickname,"Sparky");
        pr_set_illusion_species(plain,SPECIES_ID_ZORUA,0);
        pr_battle disguised=b.bt;disguised.deviations|=PR_DEV_ILLUSION;
        events[0].field[0].species=SPECIES_ID_DITTO;memset(&view,0,sizeof view);
        n=be_convert_events(&disguised,events,1,false,&view,out,16);
        found=false;
        for(int i=0;i<n;i++)if(out[i].type==BEV_SUMMON&&out[i].battler==0){found=true;CHECK(out[i].mon->species==SPECIES_ID_DITTO&&!strcmp(out[i].mon->name,"Ditto"));}
        CHECK(found);
        pr_break_illusion(plain);plain->nickname[0]=0;
    }
    printf("fused name test: %d checks, 0 failures\n",checks);
    return 0;
}
