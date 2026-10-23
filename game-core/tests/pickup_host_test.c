/* Real turn -> Faint loot -> BattleEnd callbacks -> inventory -> next-wave save. */
#define main retained_doubles_main
#include "doubles_host_test.c"
#undef main
static void probe(bool capped){
 setup(1);int start_wave=r.wave;pr_pokemon *receiver=&r.party[2].b;
 for(int k=0;k<3;k++){
  pr_pokemon *p=&r.party[k].b;p->ability=p->base_ability=ABILITY_ID_NONE;p->custom_ability=ABILITY_ID_NONE;p->passive=0;
  p->hp=p->stats[STAT_HP]=100000;p->stats[STAT_ATK]=p->stats[STAT_SPATK]=100000;p->stat_stages[STAT_ACC-STAT_ATK]=6;
  p->n_moves=1;p->moves[0]=k?MOVE_ID_SPLASH:MOVE_ID_SURF;
 }
 receiver->ability=receiver->base_ability=ABILITY_ID_PICKUP;
 for(int k=0;k<2;k++){
  pr_pokemon *p=&r.enemy[k].b;p->hp=1;p->ability=p->base_ability=ABILITY_ID_NONE;p->custom_ability=ABILITY_ID_NONE;p->passive=0;p->n_moves=1;p->moves[0]=MOVE_ID_SPLASH;
 }
 r.n_items=0;if(capped)r.items[r.n_items++]=(gc_item){.key="GOLDEN_EGG",.arg=-1,.stack=99,.pokemon_id=receiver->id};
 r.n_enemy_items=1;r.enemy_items[0]=(gc_item){.key="GOLDEN_EGG",.arg=-1,.stack=2,.pokemon_id=r.enemy[0].b.id};
 gc_items_to_battle(&r);int phase_count=0;bool saw_reward=false;
 while(r.wave==start_wave&&phase_count++<50){
  gc_choice c={0};const gc_screen *s=gc_run_screen(&r);
  switch(s->kind){
  case GC_SCREEN_COMMAND:c=(gc_choice){GC_CH_FIGHT,0,-1,0};break;
  case GC_SCREEN_TARGET:c=(gc_choice){GC_CH_TARGET,s->target_ok[2]?2:3,0,0};break;
  case GC_SCREEN_REWARD:saw_reward=true;c.kind=GC_CH_SKIP;break;
  case GC_SCREEN_LEARN_MOVE:c=(gc_choice){GC_CH_FORGET,-1,0,0};break;
  case GC_SCREEN_EVOLVE:c.kind=GC_CH_CANCEL;break;
  case GC_SCREEN_CHECK_SWITCH:c.kind=GC_CH_CONFIRM;break;
  default:fprintf(stderr,"unexpected screen%d\n",s->kind);CHECK(false);
  }
  choose(&r,c);
 }
 CHECK(saw_reward&&r.wave==start_wave+1&&r.n_enemy_items>=0);
 int count=0,stacks=0;for(int i=0;i<r.n_items;i++)if(!strcmp(r.items[i].key,"GOLDEN_EGG")&&r.items[i].pokemon_id==receiver->id){count++;stacks+=r.items[i].stack;CHECK(!r.items[i].detached&&!r.items[i].loot_ref);}
 CHECK(count==1&&stacks==(capped?99:1)&&r.stubs[GC_STUB_POST_BATTLE]==0);
 CHECK(r.party[2].b.hp>0&&!r.party[2].b.on_field);
 const char *why=NULL;size_t n=gc_run_save(&r,save,sizeof save);CHECK(n&&gc_run_load(&back,save,n,&why));
 count=stacks=0;for(int i=0;i<back.n_items;i++)if(!strcmp(back.items[i].key,"GOLDEN_EGG")&&back.items[i].pokemon_id==receiver->id){count++;stacks+=back.items[i].stack;}
 CHECK(count==1&&stacks==(capped?99:1));
}
int main(void){probe(false);probe(true);printf("Pickup host: whole-turn faint loot, bench holder, cap refusal, next wave and save; %d checks passed\n",checks);return 0;}
