/* Real run controller: native encounter width, both commands, targeting and restore. */
#include "../src/internal.h"
#include <stdio.h>
#include <stdlib.h>
static gc_run r,back;static unsigned char save[GC_SAVE_MAX];static int checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"doubles host line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
static void drain(gc_run *run){gc_event e;while(gc_run_next_event(run,&e)){}CHECK(!run->ev_overflow);}
static void choose(gc_run *run,gc_choice c){const char *why=NULL;if(!gc_run_choose(run,&c,&why)){fprintf(stderr,"choice%d: %s\n",c.kind,why);CHECK(false);}drain(run);}
static void setup(int seed){
 char key[64];snprintf(key,sizeof key,"doubles-controller-%d",seed);gc_run_new(&r,GAME_MODES_ENDLESS,key,seed);r.wave=1;
 r.n_items=3;const char *keys[]={"LURE","SUPER_LURE","MAX_LURE"};
 for(int k=0;k<3;k++)r.items[k]=(gc_item){.key=keys[k],.stack=1,.arg=-1,.battle_count=(int[]){10,15,30}[k]};
 gc_starter team[3]={{.species=1,.tera_type=-1},{.species=4,.tera_type=-1},{.species=7,.tera_type=-1}};
 for(int k=0;k<3;k++)memset(team[k].ivs,15,6);
 const char *why=NULL;CHECK(gc_run_set_starters(&r,team,3,&why));drain(&r);
}
static bool rngsame(pr_rng a,pr_rng b){return a.c==b.c&&a.s0==b.s0&&a.s1==b.s1&&a.s2==b.s2;}
static void turn(gc_run *run){
 CHECK(run->screen.kind==GC_SCREEN_COMMAND&&run->screen.slot==0);
 choose(run,(gc_choice){GC_CH_FIGHT,0,-1,0});
 CHECK(run->screen.kind==GC_SCREEN_TARGET&&run->screen.slot==0);
 CHECK(run->screen.target_ok[1]&&run->screen.target_ok[2]&&run->screen.target_ok[3]&&!run->screen.target_ok[0]);
 const char *why=NULL;CHECK(!gc_run_choose(run,&(gc_choice){GC_CH_TARGET,0,0,0},&why));
 choose(run,(gc_choice){GC_CH_CANCEL,0,0,0});CHECK(run->screen.kind==GC_SCREEN_COMMAND&&run->screen.slot==0);
 choose(run,(gc_choice){GC_CH_FIGHT,0,-1,0});choose(run,(gc_choice){GC_CH_TARGET,3,0,0});
 CHECK(run->screen.kind==GC_SCREEN_COMMAND&&run->screen.slot==1&&run->bt.turn==1);
 CHECK(run->bt.commands[0].n_targets==1&&run->bt.commands[0].targets[0]==3);
 choose(run,(gc_choice){GC_CH_FIGHT,0,-1,0});
 CHECK(run->screen.kind==GC_SCREEN_COMMAND&&run->screen.slot==0&&run->bt.turn==2);
 CHECK(run->bt.field[0]->pp_used[0]==1&&run->bt.field[1]->pp_used[0]==1);
}
static void release_active_ally(void){
 setup(1);drain(&r);
 for(int k=3;k<6;k++){r.party[k]=r.party[2];r.party[k].b.id+=k;r.party_order[k]=k;}
 r.n_party=6;r.bt.n_party[0]=6;for(int k=0;k<6;k++)r.bt.party[0][k]=&r.party[k].b;
 pr_pokemon *lead=r.bt.field[0];int released=r.party_order[1];uint32_t caught=r.enemy[0].b.id;
 gc_rphase phase=gc_rp(RP_CATCH,0,0,0);CHECK(!gc_catch_start(&r,&phase));drain(&r);
 bool ok=false;const char *why=NULL;gc_choice c={GC_CH_PICK,released,0,0};CHECK(gc_catch_choose(&r,&phase,&c,&why,&ok)&&ok);
 CHECK(r.bt.field[0]==lead&&r.bt.field[1]==NULL&&r.party[released].b.id==caught&&!r.party[released].b.on_field);
 gc_event ev;int recalls=0;while(gc_run_next_event(&r,&ev))if(ev.kind==GC_EV_RECALL){CHECK(ev.a==0&&ev.b==released&&ev.c==1);recalls++;}CHECK(recalls==1);
}
static void restore_fainted_field(void){
 setup(2);pr_pokemon *ally=r.bt.field[1];CHECK(ally);ally->hp=0;ally->on_field=false;ally->status=STATUS_EFFECT_FAINT;
 const char *why=NULL;size_t n=gc_run_save(&r,save,sizeof save);CHECK(n&&gc_run_load(&back,save,n,&why));
 CHECK(back.bt.field[1]&&back.bt.field[1]==back.bt.party[0][1]&&back.bt.field[1]->id==ally->id&&!back.bt.field[1]->on_field&&back.bt.field[1]->hp==0);
}
static void restore_disguise(void){
 setup(3);pr_pokemon *ally=r.bt.field[1];CHECK(ally);ally->custom_ability=ABILITY_ID_ILLUSION;pr_refresh_ability(ally);pr_set_illusion(ally,&r.party[2].b);
 const char *why=NULL;size_t n=gc_run_save(&r,save,sizeof save);CHECK(n&&gc_run_load(&back,save,n,&why));
 CHECK(back.bt.field[1]&&back.bt.field[1]->illusion.active&&!memcmp(&back.bt.field[1]->illusion,&ally->illusion,sizeof ally->illusion));
}
int main(void){
 release_active_ally();restore_fainted_field();restore_disguise();
 int played=0;
 for(int seed=1;seed<=24;seed++){
  setup(seed);if(r.battle_type!=BATTLE_TYPE_WILD)continue;
  CHECK(r.double_battle&&r.bt.double_battle&&r.n_enemy==2);
  for(int k=0;k<4;k++)CHECK(r.bt.field[k]&&r.bt.field[k]->on_field);
  CHECK(r.stubs[GC_STUB_DOUBLE_AS_SINGLE]==0);
  for(int k=0;k<4;k++){
   pr_pokemon *p=r.bt.field[k];p->hp=p->stats[STAT_HP]=100000;p->moves[0]=k==0?MOVE_ID_TACKLE:MOVE_ID_SPLASH;
   p->n_moves=1;p->pp_used[0]=0;p->max_pp_override[0]=-1;
  }
  const char *why=NULL;CHECK(!strcmp(gc_catch_ball_refusal(&r,0),"battle:noPokeballMulti"));
  size_t n=gc_run_save(&r,save,sizeof save);CHECK(n&&gc_run_load(&back,save,n,&why)&&!back.load_dropped);
  CHECK(back.double_battle&&back.bt.double_battle&&back.n_enemy==2);
  for(int k=0;k<4;k++)CHECK(back.bt.field[k]&&back.bt.field[k]->id==r.bt.field[k]->id);
  drain(&back);turn(&r);turn(&back);
  CHECK(rngsame(r.bt.rng,back.bt.rng)&&rngsame(r.bt.battle_rng,back.bt.battle_rng));
  for(int k=0;k<4;k++)CHECK(r.bt.field[k]->hp==back.bt.field[k]->hp);
  played++;
 }
 CHECK(played>0);printf("Doubles controller: %d wild encounters, %d checks passed\n",played,checks);return 0;
}
