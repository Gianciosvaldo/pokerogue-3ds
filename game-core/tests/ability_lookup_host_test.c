/* Legal command saves preserve indexed abilities and temporary lookup inputs. */
#include "../src/internal.h"
#include <stdio.h>
#include <stdlib.h>
static gc_run run,loaded;static unsigned char saved[GC_SAVE_MAX],mutated[GC_SAVE_MAX];static int checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"ability lookup save line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
static uint32_t hashname(const char *s){uint32_t h=2166136261u;while(*s)h=(h^(unsigned char)*s++)*16777619u;return h;}
static uint32_t crc32(const unsigned char *p,size_t n){uint32_t c=~0u;for(size_t i=0;i<n;i++){c^=p[i];for(int k=0;k<8;k++)c=c>>1^(0xedb88320u&(0u-(c&1)));}return ~c;}
static int hide(size_t n,const char *field){uint32_t h=hashname(field),unknown=hashname("unknown-ability-lookup-field");int hits=0;for(size_t i=20;i+4<=n;i++)if(!memcmp(mutated+i,&h,4)){memcpy(mutated+i,&unknown,4);hits++;}uint32_t crc=crc32(mutated+20,n-20);memcpy(mutated+16,&crc,4);return hits;}
int main(void){
 const char *why=NULL;gc_run_new(&run,GAME_MODES_CLASSIC,"ability-lookup-save",777);
 gc_starter starter={.species=SPECIES_ID_BULBASAUR,.tera_type=-1,.moves={MOVE_ID_SPLASH}};
 memset(starter.ivs,15,sizeof starter.ivs);CHECK(gc_run_set_starters(&run,&starter,1,&why));
 gc_event ev;while(gc_run_next_event(&run,&ev)){}CHECK(gc_run_can_save(&run));pr_pokemon *p=run.bt.field[0];CHECK(p&&p->ability_lookup_known);
 p->custom_ability=0;pr_refresh_ability(p);CHECK(p->ability==0&&p->base_ability==0);
 p->ability_override=ABILITY_ID_BALL_FETCH;pr_refresh_ability(p);CHECK(p->ability==ABILITY_ID_BALL_FETCH&&p->base_ability==0);
 size_t n=gc_run_save(&run,saved,sizeof saved);CHECK(n>0);CHECK(gc_run_load(&loaded,saved,n,&why));pr_pokemon *q=loaded.bt.field[0];
 CHECK(q&&q->ability_lookup_known&&q->custom_ability==0&&pr_get_ability(q,true)==0&&q->ability==ABILITY_ID_BALL_FETCH);
 pr_reset_summon_data(q);CHECK(q->ability==0&&q->ability_override==-1);
 const char *fields[]={"ability_lookup_known","ability_index","custom_ability","fusion_custom_ability","lookup_ability_override","daily_boss_ability","temporary_fusion_species","temporary_fusion_form"};
 for(int i=0;i<8;i++){memcpy(mutated,saved,n);CHECK(hide(n,fields[i])>0);CHECK(!gc_run_load(&loaded,mutated,n,&why));}
 memcpy(mutated,saved,n);for(int i=0;i<8;i++)CHECK(hide(n,fields[i])>0);
 CHECK(gc_run_load(&loaded,mutated,n,&why));q=loaded.bt.field[0];CHECK(!q->ability_lookup_known&&q->ability==ABILITY_ID_BALL_FETCH&&q->base_ability==0);
 pr_reset_summon_data(q);CHECK(q->ability==0);
 p->fusion_species=SPECIES_ID_RATTATA;p->fusion_form=0;p->fusion_ability_index=0;p->fusion_custom_ability=ABILITY_ID_SERENE_GRACE;
 p->temporary_fusion_species=SPECIES_ID_PIDGEY;p->temporary_fusion_form=0;p->lookup_ability_override=ABILITY_ID_BALL_FETCH;p->daily_boss_ability=0;p->ability_override=0;pr_refresh_ability(p);
 n=gc_run_save(&run,saved,sizeof saved);CHECK(n>0);CHECK(gc_run_load(&loaded,saved,n,&why));q=loaded.bt.field[0];
 CHECK(q->temporary_fusion_species==SPECIES_ID_PIDGEY&&q->lookup_ability_override==ABILITY_ID_BALL_FETCH&&q->daily_boss_ability==0&&q->fusion_custom_ability==ABILITY_ID_SERENE_GRACE);
 pr_reset_summon_data(q);CHECK(q->temporary_fusion_species==0&&q->ability==ABILITY_ID_BALL_FETCH);
 p->ability_index=3;n=gc_run_save(&run,saved,sizeof saved);CHECK(n>0);CHECK(!gc_run_load(&loaded,saved,n,&why));p->ability_index=0;
 p->custom_ability=PR_ABILITY_COUNT;n=gc_run_save(&run,saved,sizeof saved);CHECK(n>0);CHECK(!gc_run_load(&loaded,saved,n,&why));
 printf("Ability lookup command/save/reset: %d checks passed\n",checks);return 0;
}
