/* Existing internal pre-command ownership, legal arena saves and reset callers.
 * The public game menu does not yet expose a Tera command. */
#include "../src/internal.h"
#include <stdio.h>
#include <stdlib.h>
static gc_run run,loaded;
static unsigned char data[GC_SAVE_MAX],changed[GC_SAVE_MAX];
static int checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"Tera host line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
static void drain(void){gc_event ev;while(gc_run_next_event(&run,&ev)){} }
static uint32_t hashname(const char *s){uint32_t h=2166136261u;while(*s)h=(h^(unsigned char)*s++)*16777619u;return h;}
static uint32_t crc32(const unsigned char *p,size_t n){uint32_t c=~0u;for(size_t i=0;i<n;i++){c^=p[i];for(int k=0;k<8;k++)c=c>>1^(0xedb88320u&(0u-(c&1)));}return ~c;}
static void hide_counter(size_t n){uint32_t h=hashname("player_teras_used"),unknown=hashname("old-save-without-tera-counter");int hits=0;for(size_t i=20;i+4<=n;i++)if(!memcmp(changed+i,&h,4)){memcpy(changed+i,&unknown,4);hits++;}CHECK(hits==1);uint32_t crc=crc32(changed+20,n-20);memcpy(changed+16,&crc,4);}
static void start(void){const char *why=NULL;gc_run_new(&run,GAME_MODES_CLASSIC,"tera-arena-save",777);gc_starter s={.species=SPECIES_ID_BULBASAUR,.tera_type=POKEMON_TYPE_STELLAR,.moves={MOVE_ID_SPLASH}};memset(s.ivs,15,sizeof s.ivs);CHECK(gc_run_set_starters(&run,&s,1,&why));drain();CHECK(gc_run_can_save(&run));CHECK(run.bt.player_teras_used_known&&run.bt.player_teras_used==0);}
int main(void){
 const char *why=NULL;start();
 const double counts[]={0,1,3,4294967296.0,9007199254740992.0};
 for(unsigned i=0;i<sizeof counts/sizeof *counts;i++){
  run.bt.player_teras_used=counts[i];size_t n=gc_run_save(&run,data,sizeof data);CHECK(n>0);CHECK(gc_run_load(&loaded,data,n,&why));CHECK(loaded.bt.player_teras_used_known&&loaded.bt.player_teras_used==counts[i]);CHECK(!loaded.load_dropped);
 }
 size_t n=gc_run_save(&run,data,sizeof data);memcpy(changed,data,n);hide_counter(n);CHECK(gc_run_load(&loaded,changed,n,&why));CHECK(loaded.bt.player_teras_used_known&&loaded.bt.player_teras_used==0);CHECK(!loaded.load_dropped);
 memcpy(changed,data,n);uint32_t counter_hash=hashname("player_teras_used");int altered=0;for(size_t i=20;i+6<n;i++)if(!memcmp(changed+i,&counter_hash,4)){changed[i+4]++;altered++;}CHECK(altered==1);uint32_t crc=crc32(changed+20,n-20);memcpy(changed+16,&crc,4);CHECK(!gc_run_load(&loaded,changed,n,&why));
 const double bad[]={-1,0.5,NAN,INFINITY,9007199254740994.0};
 for(unsigned i=0;i<sizeof bad/sizeof *bad;i++){run.bt.player_teras_used=bad[i];n=gc_run_save(&run,data,sizeof data);CHECK(n>0);CHECK(!gc_run_load(&loaded,data,n,&why));}
 run.bt.player_teras_used=7;gc_battle_restore_field(&run);CHECK(run.bt.player_teras_used_known&&run.bt.player_teras_used==7);
 /* Ordinary trainer/arena cleanup does not refresh Tera uses. */
 gc_battle_cleanup_players(&run);CHECK(run.bt.player_teras_used==7);gc_battle_setup_field(&run,true);CHECK(run.bt.player_teras_used==7);drain();
 gc_rphase heal=gc_rp(RP_PARTY_HEAL,0,0,0);CHECK(gc_phase_start(&run,&heal));CHECK(run.bt.player_teras_used_known&&run.bt.player_teras_used==0);
 run.bt.player_teras_used=8;gc_rphase biome=gc_rp(RP_SWITCH_BIOME,run.biome,0,0);CHECK(gc_phase_start(&run,&biome));CHECK(run.bt.player_teras_used==0);
 /* Source hasSpecies includes a fusion counterpart, even off the field. */
 for(int fusion=0;fusion<2;fusion++){start();pr_pokemon *p=run.bt.field[0];if(fusion)p->fusion_species=SPECIES_ID_TERAPAGOS;else p->species=SPECIES_ID_TERAPAGOS;run.bt.player_teras_used=5;gc_battle_cleanup_players(&run);CHECK(run.bt.player_teras_used_known&&run.bt.player_teras_used==0);}
 start();pr_pokemon *p=run.bt.field[0];p->added_type=POKEMON_TYPE_GRASS;run.bt.player_teras_used=3;
 run.bt.commands[0]=(struct pr_turn_command){.command=COMMAND_FIGHT,.move=MOVE_ID_SPLASH,.targets={0},.n_targets=1};run.bt.commands[2]=(struct pr_turn_command){.command=COMMAND_FIGHT,.move=MOVE_ID_SPLASH,.targets={2},.n_targets=1};run.bt.pre_commands[0]=(struct pr_turn_command){.command=COMMAND_TERA};
 gc_battle_run(&run,-2);CHECK(p->terastallized&&p->added_type==0&&run.bt.player_teras_used==4);CHECK(!run.bt.n_unported&&!run.eng.overflow);drain();CHECK(!gc_run_can_save(&run));
 printf("Tera internal turn/arena/save/reset: %d checks passed\n",checks);return 0;
}
