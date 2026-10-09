/* Focused diagnostic policy guards and real initial queue; legacy chooser untouched. */
#define main gcstress_entry
#include "../tools/gcstress.c"
#undef main
static gc_run test_run,before;static gc_system test_sys;static int checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"diagnostic host line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
static void setup(void){diag_setup(&test_run,&test_sys,777);gc_event ev;while(gc_run_next_event(&test_run,&ev)){};}
static void policy(void){gc_choice c;setup();CHECK(test_run.wave==1&&test_run.screen.kind==GC_SCREEN_COMMAND&&gc_int(&test_run)->rq.waiting);CHECK(test_run.n_party==6&&test_run.party[0].b.level==200&&test_run.money==10000);before=test_run;CHECK(diag_choice(&test_run,&c)&&!memcmp(&before,&test_run,sizeof before));CHECK(c.kind==GC_CH_FIGHT&&test_run.screen.move_ok[c.a]);const char *why;CHECK(gc_run_choose(&test_run,&c,&why));CHECK(test_run.wave>=1); /* actual queue performs the attack */
 setup();test_run.enemy[0].b.species=SPECIES_ID_UMBREON;for(int i=0;i<4;i++)test_run.party[0].b.moves[i]=MOVE_ID_PSYCHIC;CHECK(diag_choice(&test_run,&c)&&c.kind==GC_CH_POKEMON&&c.a>0&&test_run.party[c.a].b.hp>0);
 setup();test_run.screen.can_switch=false;test_run.party[0].b.hp=10;CHECK(diag_choice(&test_run,&c)&&c.kind==GC_CH_FIGHT&&test_run.party[0].b.moves[c.a]==MOVE_ID_RECOVER);
 setup();memset(test_run.screen.move_ok,0,sizeof test_run.screen.move_ok);test_run.screen.can_switch=false;test_run.screen.can_ball=true;CHECK(diag_choice(&test_run,&c)&&c.kind==GC_CH_BALL&&c.a==POKEBALL_TYPE_MASTER_BALL);
 test_run.screen.can_ball=false;test_run.screen.struggle=true;CHECK(diag_choice(&test_run,&c)&&c.kind==GC_CH_FIGHT);test_run.screen.struggle=false;CHECK(!diag_choice(&test_run,&c));
 setup();test_run.screen.kind=GC_SCREEN_SWITCH;memset(test_run.screen.pick_ok,0,sizeof test_run.screen.pick_ok);CHECK(!diag_choice(&test_run,&c));test_run.screen.pick_ok[2]=true;CHECK(diag_choice(&test_run,&c)&&c.a==2);test_run.screen.can_skip=true;CHECK(diag_choice(&test_run,&c)&&c.a==-1);
 setup();test_run.screen.kind=GC_SCREEN_REWARD;test_run.party[2].b.hp=0;test_run.screen.n_options=2;test_run.screen.options[0]=(gc_offer){.key="POTION",.target=GC_TARGET_POKEMON};test_run.screen.options[1]=(gc_offer){.key="REVIVE",.target=GC_TARGET_POKEMON};CHECK(diag_choice(&test_run,&c)&&c.kind==GC_CH_REWARD&&c.a==1&&c.b==2);
 test_run.party[2].b.hp=test_run.party[2].b.stats[0];test_run.party[1].b.hp=10;CHECK(diag_choice(&test_run,&c)&&c.a==0&&c.b==1);test_run.party[1].b.hp=test_run.party[1].b.stats[0];test_run.screen.n_options=0;test_run.screen.n_shop=1;test_run.screen.shop[0]=(gc_offer){.key="POTION",.cost=100,.target=GC_TARGET_POKEMON};test_run.party[1].b.hp=10;test_run.money=1;CHECK(diag_choice(&test_run,&c)&&c.kind==GC_CH_SKIP);test_run.money=100;CHECK(diag_choice(&test_run,&c)&&c.kind==GC_CH_SHOP&&c.b==1);
 setup();test_run.screen.kind=GC_SCREEN_REWARD;test_run.screen.n_options=1;test_run.screen.options[0]=(gc_offer){.key="ETHER",.target=GC_TARGET_MOVE};test_run.party[1].b.pp_used[2]=gc_move_max_pp(&test_run.party[1].b,2);CHECK(diag_choice(&test_run,&c)&&c.b==1&&c.c==2);
 setup();test_run.screen.kind=GC_SCREEN_LEARN_MOVE;CHECK(diag_choice(&test_run,&c)&&c.kind==GC_CH_FORGET&&c.a==-1);test_run.screen.kind=GC_SCREEN_EVOLVE;CHECK(diag_choice(&test_run,&c)&&c.kind==GC_CH_CANCEL&&c.a==1);test_run.screen.kind=GC_SCREEN_CHECK_SWITCH;CHECK(diag_choice(&test_run,&c)&&c.kind==GC_CH_CONFIRM&&c.a==0);
 test_run.screen.kind=GC_SCREEN_BIOME;test_run.screen.n_biomes=0;CHECK(!diag_choice(&test_run,&c));test_run.screen.n_biomes=1;CHECK(diag_choice(&test_run,&c)&&c.kind==GC_CH_BIOME);test_run.screen.kind=GC_SCREEN_MYSTERY;test_run.screen.n_options=2;memset(test_run.screen.option_ok,0,sizeof test_run.screen.option_ok);CHECK(!diag_choice(&test_run,&c));test_run.screen.option_ok[1]=true;CHECK(diag_choice(&test_run,&c)&&c.kind==GC_CH_MYSTERY&&c.a==1);
 for(int i=0;i<6;i++){setup();switch(i){case 0:test_run.n_party=0;break;case 1:test_run.n_party=7;break;case 2:test_run.screen.n_options=9;break;case 3:test_run.screen.n_shop=17;break;case 4:test_run.party[0].b.n_moves=5;break;case 5:test_run.screen.kind=255;break;}before=test_run;CHECK(!diag_choice(&test_run,&c)&&!memcmp(&before,&test_run,sizeof before));}
}
static void dirty_stock(void){
 static gc_run clean,dirty;static gc_system clean_sys,dirty_sys;diag_setup(&clean,&clean_sys,78);diag_setup(&dirty,&dirty_sys,78);
 for(int i=dirty.n_enemy_items;i<GC_MAX_ITEMS;i++){dirty.enemy_items[i].detached=true;dirty.enemy_items[i].virtual_stack=97;dirty.enemy_items[i].battle_count=99;}
 int wave=1,generated=0;const char *why;for(int step=0;step<2000&&clean.wave<100;step++){
  gc_event e;while(gc_run_next_event(&clean,&e)){}while(gc_run_next_event(&dirty,&e)){}
  CHECK(clean.wave==dirty.wave&&clean.screen.kind==dirty.screen.kind);
  if(clean.wave!=wave){wave=clean.wave;CHECK(clean.n_enemy_items==dirty.n_enemy_items);for(int i=0;i<clean.n_enemy_items;i++){gc_item *it=&dirty.enemy_items[i];CHECK(!it->detached&&!it->virtual_stack&&!it->battle_count);CHECK(!memcmp(it,&clean.enemy_items[i],sizeof *it));generated++;}}
  gc_choice a,b;CHECK(diag_choice(&clean,&a)&&diag_choice(&dirty,&b)&&!memcmp(&a,&b,sizeof a));bool ca=gc_run_choose(&clean,&a,&why),cb=gc_run_choose(&dirty,&b,&why);CHECK(ca==cb);
  if(!ca){CHECK(clean.screen.kind==GC_SCREEN_REWARD);a=(gc_choice){.kind=GC_CH_SKIP};CHECK(gc_run_choose(&clean,&a,&why)&&gc_run_choose(&dirty,&a,&why));}
 }
 CHECK(clean.wave>=100&&generated>0);
}
int main(void){policy();dirty_stock();int value;CHECK(diag_integer("100",1,100000,&value)&&value==100);CHECK(!diag_integer("100x",1,100000,&value));CHECK(!diag_integer("-1",1,100000,&value));CHECK(!diag_integer("2147483648",1,100000,&value));CHECK(!diag_integer("",1,100000,&value));printf("Endgame diagnostic host: %d checks passed\n",checks);return 0;}
