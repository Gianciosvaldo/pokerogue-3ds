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
    printf("fused name test: %d checks, 0 failures\n",checks);
    return 0;
}
