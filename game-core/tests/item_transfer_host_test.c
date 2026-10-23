/* Real command, inventory projection and save/load boundaries. Source answers
 * for transfer selection/callbacks/stats are in battle-core's replay fixture. */
#include "../src/internal.h"
#include <stdio.h>
#include <stdlib.h>
static gc_run run,loaded;static unsigned char saved[GC_SAVE_MAX];static int checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"item transfer host line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
static void drain(void){gc_event e;while(gc_run_next_event(&run,&e)){};}
static void setup(void){
 const char *why=NULL;gc_run_new(&run,GAME_MODES_CLASSIC,"item-transfer",777);
 gc_starter s={.species=SPECIES_ID_BULBASAUR,.tera_type=-1,.moves={MOVE_ID_SPLASH}};memset(s.ivs,15,sizeof s.ivs);CHECK(gc_run_set_starters(&run,&s,1,&why));drain();CHECK(gc_run_can_save(&run));
 for(int side=0;side<2;side++){pr_pokemon *p=run.bt.field[side*2];CHECK(p);p->custom_ability=ABILITY_ID_NONE;p->ability_override=-1;p->passive_flag=false;pr_refresh_ability(p);p->n_moves=1;p->moves[0]=MOVE_ID_SPLASH;p->pp_used[0]=p->pp_up[0]=0;p->max_pp_override[0]=-1;}
 run.n_items=run.n_enemy_items=0;
}
static void roundtrip(void){const char *why=NULL;size_t n=gc_run_save(&run,saved,sizeof saved);CHECK(n>0);CHECK(gc_run_load(&loaded,saved,n,&why));CHECK(!loaded.load_dropped);CHECK(loaded.n_items==run.n_items&&loaded.n_enemy_items==run.n_enemy_items);for(int side=0;side<2;side++){gc_item *a=side?run.enemy_items:run.items,*b=side?loaded.enemy_items:loaded.items;int count=side?run.n_enemy_items:run.n_items;for(int i=0;i<count;i++){CHECK(!strcmp(a[i].key,b[i].key));CHECK(a[i].arg==b[i].arg&&a[i].stack==b[i].stack&&a[i].pokemon_id==b[i].pokemon_id&&a[i].detached==b[i].detached&&b[i].non_transferable==(a[i].non_transferable||!strcmp(a[i].key,"BASE_STAT_BOOSTER")));}}}
static void next_save(void){
 const char *why=NULL;pr_pokemon *p=run.bt.field[0],*enemy=run.bt.field[2];p->moves[0]=MOVE_ID_DRAGON_RAGE;enemy->hp=1;gc_choice c={GC_CH_FIGHT,0,-1,0};CHECK(gc_run_choose(&run,&c,&why));drain();
 for(int i=0;i<30&&!gc_run_can_save(&run);i++){c=(gc_choice){0};switch(run.screen.kind){case GC_SCREEN_REWARD:c.kind=GC_CH_SKIP;break;case GC_SCREEN_LEARN_MOVE:c.kind=GC_CH_FORGET;c.a=-1;break;case GC_SCREEN_EVOLVE:c.kind=GC_CH_CANCEL;c.a=1;break;case GC_SCREEN_BIOME:c.kind=GC_CH_BIOME;break;default:CHECK(false);}CHECK(gc_run_choose(&run,&c,&why));drain();}CHECK(gc_run_can_save(&run));roundtrip();
}
static void command(void){
 for(int side=0;side<2;side++)for(int blocked=0;blocked<2;blocked++){
  setup();pr_pokemon *holder=run.bt.field[2*side],*donor=run.bt.field[2*(1-side)];
  gc_item *a=side?run.enemy_items:run.items,*b=side?run.items:run.enemy_items;
  a[0]=(gc_item){.key="MINI_BLACK_HOLE",.arg=-1,.stack=1,.pokemon_id=holder->id,.non_transferable=true};
  b[0]=(gc_item){.key="ATTACK_TYPE_BOOSTER",.arg=POKEMON_TYPE_NORMAL,.stack=2,.pokemon_id=donor->id,.non_transferable=blocked};run.n_items=run.n_enemy_items=1;gc_items_to_battle(&run);
  roundtrip();const char *why=NULL;gc_choice fight={GC_CH_FIGHT,0,-1,0};CHECK(gc_run_choose(&run,&fight,&why));CHECK(gc_run_choose(&loaded,&fight,&why));drain();
  CHECK((side?run.n_enemy_items:run.n_items)==(blocked?1:2));CHECK(b[0].stack==(blocked?2:1));CHECK(a[0].non_transferable);if(!blocked)CHECK(!strcmp(a[1].key,"ATTACK_TYPE_BOOSTER")&&a[1].arg==POKEMON_TYPE_NORMAL&&a[1].stack==1&&a[1].pokemon_id==holder->id&&!a[1].non_transferable);
  CHECK(loaded.n_items==run.n_items&&loaded.n_enemy_items==run.n_enemy_items);CHECK(!memcmp(&run.run.rng,&loaded.run.rng,sizeof run.run.rng));
  int ni=run.n_items,ne=run.n_enemy_items;gc_items_from_battle(&run);gc_items_from_battle(&run);CHECK(run.n_items==ni&&run.n_enemy_items==ne);next_save();
 }
}
static void families(void){
 static const char *keys[]={"LEFTOVERS","SHELL_BELL","FOCUS_BAND","QUICK_CLAW","KINGS_ROCK","WHITE_HERB","MINI_BLACK_HOLE","BERRY","WIDE_LENS","MULTI_LENS","SCOPE_LENS","TOXIC_ORB","FLAME_ORB","SOUL_DEW","GOLDEN_PUNCH","ATTACK_TYPE_BOOSTER","REVIVER_SEED"};
 for(unsigned k=0;k<sizeof keys/sizeof *keys;k++){
  setup();gc_item it={.key=keys[k],.arg=!strcmp(keys[k],"BERRY")?BERRY_TYPE_SITRUS:!strcmp(keys[k],"ATTACK_TYPE_BOOSTER")?POKEMON_TYPE_FIRE:-1,.stack=1,.pokemon_id=run.bt.field[2]->id};run.enemy_items[0]=it;run.n_enemy_items=1;gc_items_to_battle(&run);CHECK(!run.bt.mods[1][0].unknown_subclass);
  pr_modifier m=run.bt.mods[1][0];run.bt.n_mods[1]=0;m.pokemon_id=run.bt.field[0]->id;run.bt.mods[0][run.bt.n_mods[0]++]=m;gc_items_from_battle(&run);CHECK(run.n_enemy_items==0&&run.n_items==1);CHECK(!strcmp(run.items[0].key,it.key)&&run.items[0].arg==it.arg&&run.items[0].pokemon_id==m.pokemon_id);gc_items_from_battle(&run);CHECK(run.n_items==1);roundtrip();
 }
}
static void order(void){
 setup();uint32_t id=run.bt.field[0]->id;
 run.items[0]=(gc_item){.key="LEFTOVERS",.arg=-1,.stack=1,.pokemon_id=id};
 run.items[1]=(gc_item){.key="BASE_STAT_BOOSTER",.arg=STAT_ATK,.stack=1,.pokemon_id=id};
 run.items[2]=(gc_item){.key="MINI_BLACK_HOLE",.arg=-1,.stack=1,.pokemon_id=id,.non_transferable=true};run.n_items=3;gc_items_to_battle(&run);
 pr_modifier tmp=run.bt.mods[0][0];run.bt.mods[0][0]=run.bt.mods[0][1];run.bt.mods[0][1]=run.bt.mods[0][2];run.bt.mods[0][2]=tmp;
 gc_items_from_battle(&run);CHECK(!strcmp(run.items[0].key,"BASE_STAT_BOOSTER")&&!strcmp(run.items[1].key,"MINI_BLACK_HOLE")&&!strcmp(run.items[2].key,"LEFTOVERS"));gc_items_from_battle(&run);CHECK(run.n_items==3&&!strcmp(run.items[2].key,"LEFTOVERS"));roundtrip();
}
static uint32_t hashname(const char *s){uint32_t h=2166136261u;while(*s)h=(h^(unsigned char)*s++)*16777619u;return h;}
static uint32_t crc32(const unsigned char *p,size_t n){uint32_t c=~0u;for(size_t i=0;i<n;i++){c^=p[i];for(int k=0;k<8;k++)c=c>>1^(0xedb88320u&(0u-(c&1)));}return ~c;}
static unsigned char mutated[GC_SAVE_MAX];
static void save_guards(void){
 setup();run.n_items=1;run.items[0]=(gc_item){.key="MINI_BLACK_HOLE",.arg=-1,.stack=1,.pokemon_id=run.bt.field[0]->id,.non_transferable=true};gc_items_to_battle(&run);roundtrip();
 const char *why=NULL;size_t n=gc_run_save(&run,saved,sizeof saved);CHECK(n>0);size_t header=0,value=0;
 for(size_t pos=20;pos+8<=n;){uint32_t name,len;memcpy(&name,saved+pos,4);memcpy(&len,saved+pos+4,4);CHECK(pos+8+len<=n);if(name==hashname("items")){uint16_t nf;memcpy(&nf,saved+pos+8,2);size_t off=0;for(int i=0;i<nf;i++){size_t h=pos+10+6*i;uint32_t f;uint16_t size;memcpy(&f,saved+h,4);memcpy(&size,saved+h+4,2);if(f==hashname("non_transferable")){header=h;value=pos+12+6*nf+off;}off+=size;}break;}pos+=8+len;}
 CHECK(header&&value<n&&saved[value]==1);
 memcpy(mutated,saved,n);mutated[value]=2;uint32_t crc=crc32(mutated+20,n-20);memcpy(mutated+16,&crc,4);CHECK(!gc_run_load(&loaded,mutated,n,&why));
 memcpy(mutated,saved,n);uint32_t unknown=hashname("old-save-without-transfer-lock");memcpy(mutated+header,&unknown,4);crc=crc32(mutated+20,n-20);memcpy(mutated+16,&crc,4);CHECK(gc_run_load(&loaded,mutated,n,&why));CHECK(loaded.load_dropped>0&&!loaded.items[0].non_transferable&&loaded.items[0].stack==1&&loaded.items[0].pokemon_id==run.items[0].pokemon_id);
}
int main(void){command();families();order();save_guards();printf("Held-item transfer commands/inventory/save: %d checks passed\n",checks);return 0;}
