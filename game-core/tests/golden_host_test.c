/* Inventory mapping, non-consumption and real command/save/load boundaries.
 * Full upstream turn/state/message answers are in golden_diff_test. */
#include "../src/internal.h"
#include <stdio.h>
#include <stdlib.h>

static gc_run run, loaded;
static unsigned char saved[GC_SAVE_MAX];
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"golden host line%d: %s\n",__LINE__,#x); exit(1); } } while (0)
static void setup(void)
{
    gc_run_new(&run, GAME_MODES_CLASSIC, "golden-host", 777);
    gc_starter starter = {.species=SPECIES_ID_BULBASAUR,.tera_type=-1};
    memset(starter.ivs,15,6);
    const char *why;
    CHECK(gc_run_set_starters(&run,&starter,1,&why));
    CHECK(run.screen.kind == GC_SCREEN_COMMAND);
    for (int side=0; side<2; side++) {
        pr_pokemon *p=run.bt.field[2*side];
        CHECK(p != NULL);
        p->species=SPECIES_ID_RATTATA; p->form=0;
        p->ability=p->base_ability=ABILITY_ID_NONE; p->ability_override=-1;
        p->custom_passive=-1; p->passive_flag=false; p->passive=ABILITY_ID_NONE;
        p->status=STATUS_EFFECT_NONE; p->status_toxic_turns=0;
        p->n_moves=1; p->moves[0]=MOVE_ID_SPLASH;
        memset(p->pp_used,0,sizeof p->pp_used); memset(p->pp_up,0,sizeof p->pp_up);
        pr_calculate_stats(&run.bt,p); p->hp=p->stats[STAT_HP];
    }
    gc_event ev; while(gc_run_next_event(&run,&ev)) {}
}

static void command_wallet(void) {
 const char *why;for(int capped=0;capped<2;capped++) {
  setup();run.money=capped?9007199254740991.0-1:1000.125;
  run.items[0]=(gc_item){.key="GOLDEN_PUNCH",.arg=-1,.stack=2,.pokemon_id=run.bt.field[0]->id};
  run.items[1]=(gc_item){.key="AMULET_COIN",.arg=0,.stack=1};run.items[2]=(gc_item){.key="AMULET_COIN",.arg=0,.stack=2};run.n_items=3;gc_items_to_battle(&run);
  run.bt.field[0]->moves[0]=MOVE_ID_DRAGON_RAGE;run.bt.field[2]->level=100;pr_calculate_stats(&run.bt,run.bt.field[2]);run.bt.field[2]->hp=run.bt.field[2]->stats[STAT_HP];
  double before=run.money;gc_system sys,old;gc_system_init_defaults(&sys);old=sys;run.sys=&sys;
  size_t n=gc_run_save(&run,saved,sizeof saved);CHECK(n&&gc_run_load(&loaded,saved,n,&why));CHECK(loaded.money==before&&loaded.bt.money==before);
  gc_choice fight={GC_CH_FIGHT,0,-1,0};CHECK(gc_run_choose(&run,&fight,&why));CHECK(gc_run_choose(&loaded,&fight,&why));
  /* damage40, Golden2 =>40, Coin1 adds8, Coin2 adds19, nominal67. */
  CHECK(run.money==(capped?9007199254740991.0:before+67));CHECK(loaded.money==run.money&&run.bt.money==run.money);
  CHECK(run.bt.money_scattered==0&&loaded.bt.money_scattered==0);CHECK(run.n_items==3&&loaded.n_items==3);
  CHECK(!memcmp(&sys,&old,sizeof sys));CHECK(!memcmp(&run.run.rng,&loaded.run.rng,sizeof run.run.rng));
  gc_wallet_to_battle(&run);CHECK(run.bt.money==run.money);
 }
}
static void guards(void) {
 for(int guard=0;guard<10;guard++) {
  setup();run.money=100;run.items[0]=(gc_item){.key="GOLDEN_PUNCH",.arg=-1,.stack=1,.pokemon_id=run.bt.field[0]->id};run.items[1]=(gc_item){.key="AMULET_COIN",.arg=0,.stack=1};run.n_items=2;
  if(guard==0)run.items[1].stack=6;
  if(guard==1)run.items[1].arg=2;
  if(guard==2)run.items[1].pokemon_id=run.party[0].b.id;
  if(guard==3)run.items[0].stack=6;
  if(guard==4)run.items[0].virtual_stack=-1;
  if(guard==5)run.money=-1;
  if(guard==6)run.money=9007199254740992.0;
  if(guard==7)run.items[0].arg=2;
  if(guard==8){run.items[0].stack=5;run.items[0].virtual_stack=1;}
  if(guard==9)run.money=NAN;
  gc_items_to_battle(&run);gc_wallet_to_battle(&run);double old=run.money;pr_apply_damage_money_modifiers(&run.bt,run.bt.field[0],40);gc_wallet_from_battle(&run);
  CHECK((run.money==old||(isnan(run.money)&&isnan(old)))&&run.bt.n_unported>0);gc_items_from_battle(&run);CHECK(run.n_items==2);
 }
 setup();run.money=10;run.bt.money_scattered=1234;gc_wallet_to_battle(&run);run.bt.money=19.5;gc_wallet_from_battle(&run);CHECK(run.money==19.5&&run.bt.money_scattered==1234);
}
static void mapping(void) {
 for(int side=0;side<2;side++)for(int stack=1;stack<=5;stack++)for(int owner=0;owner<3;owner++) {
  setup();gc_item *it=side?run.enemy_items:run.items;
  *it=(gc_item){.key="GOLDEN_PUNCH",.arg=-1,.stack=stack,.pokemon_id=owner?123:0,.detached=owner==2,.virtual_stack=stack==5?0:1};
  if(side)run.n_enemy_items=1;else run.n_items=1;gc_items_to_battle(&run);
  const pr_modifier *m=&run.bt.mods[side][0];CHECK(!m->unknown_subclass&&m->cls==PR_MOD_DAMAGE_MONEY_REWARD);
  CHECK(m->pokemon_id==(owner==2?0:owner?123:-1)&&m->stack_count==stack&&m->virtual_stack_count==it->virtual_stack);
  gc_items_from_battle(&run);gc_items_from_battle(&run);CHECK((side?run.n_enemy_items:run.n_items)==1);
 }
 setup();run.items[0]=(gc_item){.key="AMULET_COIN",.arg=-1,.stack=1,.virtual_stack=1};run.n_items=1;
 gc_items_to_battle(&run);CHECK(run.bt.mods[0][0].unknown_subclass);
 run.items[1]=(gc_item){.key="GOLDEN_PUNCH",.arg=-1,.stack=1,.pokemon_id=run.bt.field[0]->id};run.n_items=2;
 gc_items_to_battle(&run);CHECK(!run.bt.mods[0][0].unknown_subclass&&run.bt.mods[0][0].virtual_stack_count==1);
 /* An unrepresented Golden must not unlock a represented virtual Coin. */
 run.items[GC_MAX_ITEMS-1]=run.items[1];for(int i=1;i<GC_MAX_ITEMS-1;i++)run.items[i]=(gc_item){.key="UNPORTED",.stack=1};run.n_items=GC_MAX_ITEMS;
 gc_items_to_battle(&run);CHECK(run.bt.n_mods[0]==0&&run.bt.n_unported);gc_items_from_battle(&run);CHECK(run.n_items==GC_MAX_ITEMS&&!strcmp(run.items[GC_MAX_ITEMS-1].key,"GOLDEN_PUNCH"));
 /* At the supported maximum the final Golden is represented and unlocks Coin. */
 run.n_items=PR_MAX_MODIFIERS;run.items[PR_MAX_MODIFIERS-1]=run.items[GC_MAX_ITEMS-1];run.bt.n_unported=0;
 gc_items_to_battle(&run);CHECK(run.bt.n_mods[0]==PR_MAX_MODIFIERS&&!run.bt.mods[0][0].unknown_subclass);
}
static void yielded_wallet(void) {
 setup();
 /* A real knockout hands VictoryPhase back to game-core. A caller balance
  * change between calls must not be overwritten by the old battle wallet. */
 run.money=100;run.bt.field[0]->moves[0]=MOVE_ID_DRAGON_RAGE;run.bt.field[2]->hp=1;
 run.bt.commands[0].command=COMMAND_FIGHT;run.bt.commands[0].move=MOVE_ID_DRAGON_RAGE;run.bt.commands[0].targets[0]=2;run.bt.commands[0].n_targets=1;
 run.bt.commands[2].command=COMMAND_FIGHT;run.bt.commands[2].move=MOVE_ID_SPLASH;run.bt.commands[2].targets[0]=2;run.bt.commands[2].n_targets=1;
 CHECK(gc_battle_run(&run,-2)==PH_VICTORY);CHECK(run.money==100);
 run.money+=17.25;gc_battle_continue(&run);CHECK(run.money==117.25&&run.bt.money==117.25);
 /* CheckSwitchPhase really asks with a live bench; answer resumes that phase. */
 setup();run.party[1]=run.party[0];run.party[1].b.id+=1000;run.n_party=2;run.party_order[1]=1;
 gc_battle_setup_field(&run,false);CHECK(run.bt.n_party[0]==2);run.bt.battle_style_set=false;
 run.bt.field[0]->on_field=true;
 int ph=pr_phase_new(&run.eng,PH_CHECK_SWITCH,-1);CHECK(ph>=0);
 run.eng.pool[ph].sw_field_index=0;pr_check_switch_phase(&run.bt,&run.eng,ph);
 CHECK(run.eng.awaiting_confirm);run.eng.pick_phase=ph;
 run.money=99.125;run.bt.money=1;gc_battle_answer_switch(&run,false);
 CHECK(!run.eng.awaiting_confirm&&run.money==99.125&&run.bt.money==99.125);
 /* Rebuilding a subsequent battle and save-load start from current run money. */
 run.money=150.5;CHECK(gc_battle_setup_field(&run,true));CHECK(run.bt.money==150.5);
}
static void storage_source(const char *path) {
 FILE *f=fopen(path,"r");CHECK(f!=NULL);char header[80];CHECK(fgets(header,sizeof header,f)!=NULL&&strcmp(header,"GOLDEN_STORAGE 1 10\n")==0);const char *why;
 for(int ci=0;ci<10;ci++) {
  int side,stack,virtual,stored_stack,stored_virtual,before,after;
  CHECK(fscanf(f," P %d %d %d %d %d %d %d",&side,&stack,&virtual,&stored_stack,&stored_virtual,&before,&after)==7);
  CHECK(side==ci/5&&stack==ci%5+1&&virtual==(stack==5?0:1)&&stored_stack==stack&&stored_virtual==0);
  setup();run.money=0;gc_item *it=side?run.enemy_items:run.items;
  *it=(gc_item){.key="GOLDEN_PUNCH",.arg=-1,.stack=stack,.pokemon_id=run.bt.field[side*2]->id,.virtual_stack=virtual};
  if(side)run.n_enemy_items=1;else run.n_items=1;gc_items_to_battle(&run);
  const pr_modifier *m=&run.bt.mods[side][0];CHECK(20*(m->stack_count+m->virtual_stack_count)==before);
  gc_wallet_to_battle(&run);pr_apply_damage_money_modifiers(&run.bt,run.bt.field[side*2],40);CHECK(run.bt.money==(side?0:before));
  size_t n=gc_run_save(&run,saved,sizeof saved);CHECK(n&&gc_run_load(&loaded,saved,n,&why));
  const gc_item *out=side?loaded.enemy_items:loaded.items;CHECK(out->stack==stored_stack&&out->virtual_stack==stored_virtual&&!out->detached);
  m=&loaded.bt.mods[side][0];CHECK(m->cls==PR_MOD_DAMAGE_MONEY_REWARD&&!m->unknown_subclass&&20*(m->stack_count+m->virtual_stack_count)==after);
  pr_apply_damage_money_modifiers(&loaded.bt,loaded.bt.field[side*2],40);CHECK(loaded.bt.money==(side?0:after));
 }
 int n;CHECK(fscanf(f," Z %d",&n)==1&&n==10);int c;while((c=fgetc(f))!='\n'&&c!=EOF)CHECK(c==' '||c=='\t');CHECK(c=='\n'&&fgetc(f)==EOF);fclose(f);
}
int main(int argc,char **argv){command_wallet();guards();mapping();yielded_wallet();if(argc==3&&!strcmp(argv[1],"--storage"))storage_source(argv[2]);printf("Golden host: %u checks passed\n",checks);return 0;}
