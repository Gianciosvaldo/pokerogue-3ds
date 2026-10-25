/* Hardware-sized guarded stack: real Fight/Ball command dispatch into enemy Taunt. */
#define _GNU_SOURCE
#include "../src/internal.h"
#include "../../game/include/runtime_limits.h"
#include <pthread.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
static gc_run run;
static gc_choice choice;
static bool accepted;
static const char *reason;
static int checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"command stack line%d: %s (%s)\n",__LINE__,#x,reason?reason:"");exit(1);}}while(0)
static void *choose_on_stack(void *unused){(void)unused;accepted=gc_run_choose(&run,&choice,&reason);return NULL;}
static void setup(int seed){
 char text[40];snprintf(text,sizeof text,"command-stack-%d",seed);gc_run_new(&run,GAME_MODES_CLASSIC,text,777);
 gc_starter starter={.species=SPECIES_ID_BULBASAUR,.tera_type=-1,.moves={MOVE_ID_TACKLE}};memset(starter.ivs,15,6);CHECK(gc_run_set_starters(&run,&starter,1,&reason));gc_event event;while(gc_run_next_event(&run,&event)){}
 CHECK(run.screen.kind==GC_SCREEN_COMMAND);
 pr_pokemon *p=run.bt.field[0],*enemy=run.bt.field[2];CHECK(p&&enemy);
 /* Fight/Ball must execute through Taunt on the guarded stack; a double
  * encounter has already lost its other opponent before these throws. */
 if(run.bt.field[3]){run.bt.field[3]->hp=0;run.bt.field[3]->on_field=false;run.bt.field[3]->status=STATUS_EFFECT_FAINT;}
 enemy->species=SPECIES_ID_MEWTWO;enemy->form=0;enemy->level=100;enemy->custom_ability=ABILITY_ID_NONE;pr_refresh_ability(enemy);pr_calculate_stats(&run.bt,enemy);enemy->hp=enemy->stats[STAT_HP];enemy->n_moves=1;enemy->moves[0]=MOVE_ID_TAUNT;enemy->pp_used[0]=0;enemy->pp_up[0]=0;enemy->max_pp_override[0]=-1;
 p->custom_ability=ABILITY_ID_NONE;pr_refresh_ability(p);p->n_moves=1;p->moves[0]=MOVE_ID_TACKLE;p->pp_used[0]=0;
 run.pokeballs[POKEBALL_TYPE_POKEBALL]=5;gc_ai_enemy_party(&run);gc_battle_command_screen(&run);
}
int main(int argc,char **argv){
 struct rlimit core={0,0};setrlimit(RLIMIT_CORE,&core);
 size_t size=POKEROGUE_MAIN_STACK_BYTES;if(argc==2)size=strtoul(argv[1],NULL,10);CHECK(size>=16384);
 size_t page=(size_t)sysconf(_SC_PAGESIZE);CHECK(size%page==0);size_t high=0;int taunts=0,balls=0;
 for(int i=0;i<16;i++){
  setup(i);choice=(gc_choice){.kind=i&1?GC_CH_BALL:GC_CH_FIGHT,.a=0,.b=-1};accepted=false;reason=NULL;
  unsigned char *mapping=mmap(NULL,size+2*page,PROT_NONE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);CHECK(mapping!=MAP_FAILED);unsigned char *stack=mapping+page;CHECK(mprotect(stack,size,PROT_READ|PROT_WRITE)==0);memset(stack,0xa5,size);
  pthread_attr_t attr;CHECK(pthread_attr_init(&attr)==0);CHECK(pthread_attr_setguardsize(&attr,0)==0);CHECK(pthread_attr_setstack(&attr,stack,size)==0);pthread_t thread;
  CHECK(pthread_create(&thread,&attr,choose_on_stack,NULL)==0);CHECK(pthread_join(thread,NULL)==0);CHECK(pthread_attr_destroy(&attr)==0);CHECK(accepted);
  size_t untouched=0;while(untouched<size&&stack[untouched]==0xa5)untouched++;size_t used=size-untouched;if(used>high)high=used;
  CHECK(munmap(mapping,size+2*page)==0);taunts+=pr_get_battler_tag(&run.party[0].b,BATTLER_TAG_TYPE_TAUNT)!=NULL;balls+=!!(i&1);
  CHECK(run.screen.kind==GC_SCREEN_COMMAND||run.screen.kind==GC_SCREEN_REWARD||run.screen.kind==GC_SCREEN_SWITCH);
 }
 CHECK(taunts==16&&balls==8);CHECK(high<size);
 printf("Guarded real commands: %d checks, Fight8 Ball8 Taunt%d, stack%zu high-water%zu bytes passed\n",checks,taunts,size,high);return 0;
}
