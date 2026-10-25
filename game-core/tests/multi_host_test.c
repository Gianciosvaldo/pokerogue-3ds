/* Inventory mapping, non-consumption and real command/save/load boundaries.
 * Full upstream turn/state/message answers are in multi_diff_test. */
#include "../src/internal.h"
#include <stdio.h>
#include <stdlib.h>

static gc_run run, loaded;
static unsigned char saved[GC_SAVE_MAX];
static unsigned checks;
static int last_count[2], actual_count_calls;
void __real_pr_apply_multi_hit_count_modifiers(pr_battle*,const pr_pokemon*,int,int*);
void __wrap_pr_apply_multi_hit_count_modifiers(pr_battle *bt,const pr_pokemon *p,int move,int *count) {
 __real_pr_apply_multi_hit_count_modifiers(bt,p,move,count);
 if(p&&count){last_count[p->is_player?0:1]=*count;actual_count_calls++;}
}

#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"multi host line%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static void setup(void)
{
    gc_run_new(&run, GAME_MODES_CLASSIC, "multi-host", 777);
    gc_starter starter = {.species=SPECIES_ID_BULBASAUR,.tera_type=-1};
    memset(starter.ivs,15,6);
    const char *why;
    CHECK(gc_run_set_starters(&run,&starter,1,&why));
    CHECK(run.screen.kind == GC_SCREEN_COMMAND);
    for (int side=0; side<2; side++) {
        pr_pokemon *p=run.bt.field[2*side];
        CHECK(p != NULL);
        p->species=SPECIES_ID_RATTATA; p->form=0;p->level=100;
        p->ability=p->base_ability=ABILITY_ID_NONE; p->ability_override=-1;
        p->custom_passive=-1; p->passive_flag=false; p->passive=ABILITY_ID_NONE;
        p->status=STATUS_EFFECT_NONE; p->status_toxic_turns=0;
        p->n_moves=1; p->moves[0]=MOVE_ID_SPLASH;
        memset(p->pp_used,0,sizeof p->pp_used); memset(p->pp_up,0,sizeof p->pp_up);
        pr_calculate_stats(&run.bt,p); p->hp=p->stats[STAT_HP];
    }
    gc_event ev; while(gc_run_next_event(&run,&ev)) {}
}

static void mapping(void) {
 for(int side=0;side<2;side++)for(int stack=1;stack<=2;stack++)for(int virtual=0;virtual<=(stack==2?0:1);virtual++)for(int owner=0;owner<3;owner++) {
  memset(&run,0,sizeof run);gc_item *it=side?run.enemy_items:run.items;*it=(gc_item){.key="MULTI_LENS",.arg=-1,.stack=stack,.pokemon_id=owner?123:0,.virtual_stack=virtual,.detached=owner==2};
  if(side)run.n_enemy_items=1;else run.n_items=1;gc_items_to_battle(&run);const pr_modifier *m=&run.bt.mods[side][0];
  CHECK(m->cls==PR_MOD_MULTI_HIT&&!m->unknown_subclass&&m->num==0);CHECK(m->stack_count==stack&&m->virtual_stack_count==virtual);
  CHECK(m->pokemon_id==(owner==2?0:owner?123:-1)&&m->is_transferable);
 }
 for(int guard=0;guard<6;guard++) {
  memset(&run,0,sizeof run);run.n_items=1;run.items[0]=(gc_item){.key="MULTI_LENS",.arg=-1,.stack=1,.pokemon_id=123};
  if(guard==0)run.items[0].stack=3;
  if(guard==1)run.items[0].stack=0;
  if(guard==2)run.items[0].virtual_stack=-1;
  if(guard==3)run.items[0].virtual_stack=3;
  if(guard==4)run.items[0].arg=5;
  if(guard==5){run.items[0].stack=2;run.items[0].virtual_stack=1;}
  gc_items_to_battle(&run);CHECK(run.bt.mods[0][0].unknown_subclass);
  pr_pokemon p={0};p.id=123;p.is_player=true;int count=1;pr_apply_multi_hit_count_modifiers(&run.bt,&p,MOVE_ID_SWIFT,&count);CHECK(count==1);
  p.hit_count=p.hits_left=2;double mult=1;pr_apply_multi_hit_damage_modifiers(&run.bt,&p,MOVE_ID_SWIFT,&mult);CHECK(mult==1);
 }
}
static void seed_virtual_guard(void) {
 memset(&run,0,sizeof run);run.items[0]=(gc_item){.key="REVIVER_SEED",.arg=-1,.stack=1,.pokemon_id=123,.virtual_stack=1};run.n_items=1;
 gc_items_to_battle(&run);CHECK(run.bt.mods[0][0].unknown_subclass);run.bt.n_mods[0]=0;gc_items_from_battle(&run);CHECK(run.n_items==1&&run.items[0].virtual_stack==1);
}
static void reward_virtual_guard(void) {
 setup();run.items[0]=(gc_item){.key="AMULET_COIN",.arg=0,.stack=1,.virtual_stack=1};run.n_items=1;
 const char *why;double old=run.money;gc_offer money={.key="NUGGET",.arg=0};
 CHECK(gc_reward_apply(&run,&money,-1,-1,&why)==GC_APPLY_UNHANDLED&&run.money==old);
 gc_items_to_battle(&run);CHECK(run.bt.mods[0][0].unknown_subclass);
 gc_offer vitamin={.key="BASE_STAT_BOOSTER",.arg=STAT_ATK,.target=GC_TARGET_POKEMON};
 CHECK(gc_reward_apply(&run,&vitamin,0,-1,&why)==GC_APPLY_UNHANDLED&&run.n_items==1);
 run.items[0]=(gc_item){.key="MULTI_LENS",.arg=-1,.stack=1,.pokemon_id=run.party[0].b.id,.virtual_stack=1};
 gc_offer mint={.key="MINT",.arg=1,.target=GC_TARGET_POKEMON};gc_mon before=run.party[0];
 CHECK(gc_reward_apply(&run,&mint,0,-1,&why)==GC_APPLY_UNHANDLED&&!memcmp(&before,&run.party[0],sizeof before));
 CHECK(gc_reward_apply(&run,&vitamin,0,-1,&why)==GC_APPLY_UNHANDLED&&run.n_items==1);
 CHECK(gc_reward_apply(&run,&money,-1,-1,&why)==GC_APPLY_UNHANDLED&&run.money==old);
 for(int k=0;k<3;k++) {
  setup();run.items[0]=(gc_item){.key="MULTI_LENS",.arg=-1,.stack=1,.pokemon_id=run.party[0].b.id,.virtual_stack=1};run.n_items=1;
  gc_offer offered=k==0?money:k==1?vitamin:mint;gc_mon prior=run.party[0];double balance=run.money;
  CHECK(gc_port_apply_reward(&run,&offered,k==0?-1:0,-1,&why));
  CHECK(run.stubs[GC_STUB_REWARD_APPLY]==1&&run.n_items==2&&run.items[0].virtual_stack==1);
  CHECK(run.money==balance&&!memcmp(&prior,&run.party[0],sizeof prior));
 }
}

static void save_resume(void) {
 const char *why;for(int stack=1;stack<=2;stack++) {
  setup();run.items[0]=(gc_item){.key="MULTI_LENS",.arg=-1,.stack=stack,.pokemon_id=run.bt.field[0]->id};run.n_items=1;
  run.enemy_items[0]=(gc_item){.key="MULTI_LENS",.arg=-1,.stack=stack,.pokemon_id=run.bt.field[2]->id};run.n_enemy_items=1;gc_items_to_battle(&run);
  CHECK(gc_run_can_save(&run));size_t n=gc_run_save(&run,saved,sizeof saved);CHECK(n&&gc_run_load(&loaded,saved,n,&why));
  CHECK(loaded.bt.mods[0][0].cls==PR_MOD_MULTI_HIT&&loaded.bt.mods[1][0].cls==PR_MOD_MULTI_HIT);CHECK(loaded.items[0].stack==stack&&!loaded.items[0].virtual_stack);
  for(int side=0;side<2;side++){run.bt.field[2*side]->moves[0]=MOVE_ID_SWIFT;loaded.bt.field[2*side]->moves[0]=MOVE_ID_SWIFT;}
  gc_choice fight={GC_CH_FIGHT,0,-1,0};CHECK(gc_run_choose(&run,&fight,&why));CHECK(gc_run_choose(&loaded,&fight,&why));
  CHECK(!memcmp(&run.run.rng,&loaded.run.rng,sizeof run.run.rng));
  for(int side=0;side<2;side++){pr_pokemon *p=run.bt.field[side*2],*q=loaded.bt.field[side*2];CHECK(p&&q&&p->hp==q->hp&&!memcmp(p->stat_stages,q->stat_stages,sizeof p->stat_stages));CHECK(last_count[side]==1+stack&&actual_count_calls>=4&&q->hit_count==p->hit_count&&q->hits_left==p->hits_left);}
  CHECK(run.n_items==1&&run.n_enemy_items==1&&run.items[0].stack==stack&&run.enemy_items[0].stack==stack);
  setup();run.items[0]=(gc_item){.key="MULTI_LENS",.arg=-1,.stack=stack,.pokemon_id=run.bt.field[0]->id,.virtual_stack=1};run.n_items=1;gc_items_to_battle(&run);
  CHECK(stack==2?run.bt.mods[0][0].unknown_subclass:run.bt.mods[0][0].virtual_stack_count==1);n=gc_run_save(&run,saved,sizeof saved);CHECK(n&&gc_run_load(&loaded,saved,n,&why));
  CHECK(run.items[0].virtual_stack==1);CHECK(!loaded.items[0].virtual_stack&&loaded.bt.mods[0][0].virtual_stack_count==0&&loaded.items[0].stack==stack);
 }
}
static void ownership_and_capacity(void) {
 const char *why;
 for(int owner=0;owner<4;owner++) {
  setup();pr_pokemon *p=run.bt.field[0];run.items[0]=(gc_item){.key="MULTI_LENS",.arg=-1,.stack=1,.pokemon_id=owner==0?0:owner==1?p->id:p->id+1000,.detached=owner==3};run.n_items=1;gc_items_to_battle(&run);
  int count=1;pr_apply_multi_hit_count_modifiers(&run.bt,p,MOVE_ID_SWIFT,&count);CHECK(count==(owner<2?2:1));
  p->hit_count=p->hits_left=count;double mult=1;pr_apply_multi_hit_damage_modifiers(&run.bt,p,MOVE_ID_SWIFT,&mult);CHECK(mult==(owner<2?.75:1));
  size_t n=gc_run_save(&run,saved,sizeof saved);CHECK(n&&gc_run_load(&loaded,saved,n,&why));CHECK(loaded.items[0].pokemon_id==run.items[0].pokemon_id&&loaded.items[0].detached==(owner==3));
  CHECK(loaded.bt.mods[0][0].pokemon_id==(owner==3?INT64_C(0):owner==0?INT64_C(-1):(int64_t)run.items[0].pokemon_id));
 }
 setup();for(int i=0;i<GC_MAX_ITEMS;i++)run.items[i]=(gc_item){.key="MULTI_LENS",.arg=-1,.stack=1,.pokemon_id=(uint32_t)(1000+i)};run.n_items=GC_MAX_ITEMS;
 gc_items_to_battle(&run);CHECK(run.bt.n_mods[0]==0&&run.bt.n_unported);gc_items_from_battle(&run);gc_items_from_battle(&run);CHECK(run.n_items==GC_MAX_ITEMS&&run.items[GC_MAX_ITEMS-1].pokemon_id==1000+GC_MAX_ITEMS-1);
 setup();run.enemy_items[0]=(gc_item){.key="MULTI_LENS",.arg=-1,.stack=1,.pokemon_id=run.bt.field[2]->id};run.n_enemy_items=1;gc_items_to_battle(&run);
 run.enemy[0].b.hp=0;run.bt.mods[1][0].pokemon_id=0;gc_items_from_battle(&run);gc_items_from_battle(&run);
 CHECK(run.n_enemy_items==1&&run.enemy_items[0].detached&&run.enemy_items[0].pokemon_id==run.enemy[0].b.id);gc_items_to_battle(&run);CHECK(run.bt.mods[1][0].pokemon_id==0);
}
static void storage_source(const char *path) {
 FILE *f=fopen(path,"r");CHECK(f!=NULL);char header[80];CHECK(fgets(header,sizeof header,f)!=NULL&&strcmp(header,"MULTI_STORAGE 1 4\n")==0);const char *why;
 for(int ci=0;ci<4;ci++) {
  int side,stack,virtual,stored_stack,stored_virtual,before,after;double bd,ad;
  CHECK(fscanf(f," P %d %d %d %d %d %d %d %lf %lf",&side,&stack,&virtual,&stored_stack,&stored_virtual,&before,&after,&bd,&ad)==9);
  CHECK(side==ci/2&&stack==ci%2+1&&virtual==(stack==2?0:1)&&stored_stack==stack&&stored_virtual==0);
  setup();gc_item *it=side?run.enemy_items:run.items;
  *it=(gc_item){.key="MULTI_LENS",.arg=-1,.stack=stack,.pokemon_id=run.bt.field[side*2]->id,.virtual_stack=virtual};
  if(side)run.n_enemy_items=1;else run.n_items=1;gc_items_to_battle(&run);
  int count=1;pr_pokemon *p=run.bt.field[side*2];pr_apply_multi_hit_count_modifiers(&run.bt,p,MOVE_ID_SWIFT,&count);CHECK(count==before);
  p->hit_count=p->hits_left=count;double mult=1;pr_apply_multi_hit_damage_modifiers(&run.bt,p,MOVE_ID_SWIFT,&mult);CHECK(mult==bd);
  size_t n=gc_run_save(&run,saved,sizeof saved);CHECK(n&&gc_run_load(&loaded,saved,n,&why));
  const gc_item *out=side?loaded.enemy_items:loaded.items;CHECK(out->stack==stored_stack&&out->virtual_stack==stored_virtual&&!out->detached);
  p=loaded.bt.field[side*2];count=1;pr_apply_multi_hit_count_modifiers(&loaded.bt,p,MOVE_ID_SWIFT,&count);CHECK(count==after);
  p->hit_count=p->hits_left=count;mult=1;pr_apply_multi_hit_damage_modifiers(&loaded.bt,p,MOVE_ID_SWIFT,&mult);CHECK(mult==ad);
 }
 int n;CHECK(fscanf(f," Z %d",&n)==1&&n==4);int c;while((c=fgetc(f))!='\n'&&c!=EOF)CHECK(c==' '||c=='\t');CHECK(c=='\n'&&fgetc(f)==EOF);fclose(f);
}
int main(int argc,char **argv){mapping();seed_virtual_guard();reward_virtual_guard();save_resume();ownership_and_capacity();if(argc==3&&!strcmp(argv[1],"--storage"))storage_source(argv[2]);printf("Multi host: %u checks passed\n",checks);return 0;}
