/* Canonical storage/party order, atomic guards, real encounter saves and effects. */
#include "../src/internal.h"
#include <stdio.h>
#include <stdlib.h>
static gc_run run,loaded;static gc_system sys;static unsigned char saved[GC_SAVE_MAX];static int checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"Multi acquisition host line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
static void setup(void){gc_run_new(&run,GAME_MODES_CLASSIC,"multi-acquire",777);gc_system_init_defaults(&sys);gc_starter st[2]={{.species=SPECIES_ID_BULBASAUR,.tera_type=-1},{.species=SPECIES_ID_RATTATA,.tera_type=-1}};memset(st[0].ivs,15,6);memset(st[1].ivs,15,6);const char *why;CHECK(gc_run_set_starters(&run,st,1,&why));run.sys=&sys;run.party[1]=run.party[0];run.party[1].b.id++;run.party[1].b.on_field=false;run.n_party=2;run.party_order[1]=1;run.bt.party[0][1]=&run.party[1].b;run.bt.n_party[0]=2;gc_event ev;while(gc_run_next_event(&run,&ev)){};}
static gc_offer offer(int tier,bool paid){return(gc_offer){.key="MULTI_LENS",.arg=-1,.tier=tier,.target=GC_TARGET_POKEMON,.cost=paid?100:-1,.is_purchase=paid};}
static void acquisition(void){const char *why;for(int target=0;target<2;target++)for(int stack=0;stack<=2;stack++)for(int wildcard=0;wildcard<2;wildcard++)for(int paid=0;paid<2;paid++){
 setup();run.money=1000.25;int before=0;
 if(wildcard)run.items[before++]=(gc_item){.key="MULTI_LENS",.arg=-1,.stack=2};
 if(stack)run.items[before++]=(gc_item){.key="MULTI_LENS",.arg=-1,.stack=stack,.pokemon_id=run.party[target].b.id};
 run.n_items=before;gc_items_to_battle(&run);gc_item originals[GC_MAX_ITEMS];memcpy(originals,run.items,sizeof originals);gc_system profile=sys;pr_rng rng=run.run.rng;
 gc_offer o=offer(1,paid);int status=gc_reward_apply(&run,&o,target,-1,&why);
 CHECK(status==(stack==2||paid?GC_APPLY_REFUSED:GC_APPLY_DONE));CHECK(run.money==1000.25&&!memcmp(&sys,&profile,sizeof sys)&&!memcmp(&run.run.rng,&rng,sizeof rng));
 if(stack==2){CHECK(run.n_items==before&&!memcmp(run.items,originals,sizeof originals));CHECK(!strcmp(why,"modifierType:ModifierType.PokemonHeldItemModifierType.extra.tooMany"));}
 else{CHECK(run.n_items==1&&run.items[0].pokemon_id==run.party[target].b.id&&run.items[0].stack==stack+1);CHECK(run.items[0].arg==(stack?-1:0));}
 int hits=1;pr_apply_multi_hit_count_modifiers(&run.bt,&run.party[target].b,MOVE_ID_TACKLE,&hits);CHECK(hits==1+(stack==2?2:stack+1)+(stack==2&&wildcard?2:0));run.party[target].b.hit_count=run.party[target].b.hits_left=hits;double damage=1;pr_apply_multi_hit_damage_modifiers(&run.bt,&run.party[target].b,MOVE_ID_TACKLE,&damage);CHECK(damage==(1-0.25*(stack==2?2:stack+1))*(stack==2&&wildcard?0.5:1));
 CHECK(gc_run_can_save(&run));size_t size=gc_run_save(&run,saved,sizeof saved);CHECK(size&&gc_run_load(&loaded,saved,size,&why));CHECK(loaded.n_items==run.n_items);
 for(int j=0;j<run.n_items;j++)CHECK(loaded.items[j].stack==run.items[j].stack&&loaded.items[j].pokemon_id==run.items[j].pokemon_id&&loaded.items[j].arg==run.items[j].arg);
 int restored=1;pr_apply_multi_hit_count_modifiers(&loaded.bt,&loaded.party[target].b,MOVE_ID_TACKLE,&restored);CHECK(restored==hits);loaded.party[target].b.hit_count=loaded.party[target].b.hits_left=restored;double restoredDamage=1;pr_apply_multi_hit_damage_modifiers(&loaded.bt,&loaded.party[target].b,MOVE_ID_TACKLE,&restoredDamage);CHECK(restoredDamage==damage);
 }
 for(int tier=0;tier<5;tier++){setup();run.items[0]=(gc_item){.key="MULTI_LENS",.arg=-1,.stack=2,.pokemon_id=run.party[0].b.id};run.items[1]=(gc_item){.key="MULTI_LENS",.arg=-1,.stack=2};run.n_items=2;gc_items_to_battle(&run);gc_offer o=offer(tier,true);gc_item old[2];memcpy(old,run.items,sizeof old);uint32_t balls[5];for(int j=0;j<5;j++)balls[j]=run.pokeballs[j];CHECK(gc_multi_scene_apply(&run,&o,0,&why,false)==GC_APPLY_DONE);CHECK(!memcmp(old,run.items,sizeof old)&&run.n_items==2);int changed=0;for(int j=0;j<5;j++)changed+=run.pokeballs[j]>balls[j];if(changed!=1)fprintf(stderr,"fallback tier%d balls%u/%u/%u/%u/%u before%u/%u/%u/%u/%u\n",tier,run.pokeballs[0],run.pokeballs[1],run.pokeballs[2],run.pokeballs[3],run.pokeballs[4],balls[0],balls[1],balls[2],balls[3],balls[4]);CHECK(changed==1);}
 setup();run.items[0]=(gc_item){.key="MULTI_LENS",.arg=-1,.stack=2,.pokemon_id=run.party[0].b.id};run.items[1]=(gc_item){.key="MULTI_LENS",.arg=0,.stack=1,.pokemon_id=run.party[0].b.id};run.n_items=2;gc_item old[2];memcpy(old,run.items,sizeof old);CHECK(gc_multi_add(&run,0)==PR_REWARD_HELD_FULL&&!memcmp(old,run.items,sizeof old));
 setup();run.n_items=1;run.items[0]=(gc_item){.key="WIDE_LENS",.arg=-1,.stack=2,.pokemon_id=run.party[1].b.id};CHECK(gc_multi_add(&run,0)==PR_REWARD_HELD_ADDED&&run.items[0].arg==-1&&run.items[0].stack==2);CHECK(run.n_items==2&&run.items[1].pokemon_id==run.party[0].b.id);
}
static void refusal(void){for(int guard=0;guard<16;guard++){
 setup();run.n_items=1;run.items[0]=(gc_item){.key="MULTI_LENS",.arg=-1,.stack=1,.pokemon_id=run.party[0].b.id};
 switch(guard){case 0:run.n_items=-1;break;case 1:run.n_items=GC_MAX_ITEMS+1;break;case 2:run.n_party=7;break;case 3:run.party_order[1]=run.party_order[0];break;case 4:run.party[1].b.id=run.party[0].b.id;break;case 5:run.items[0].key="UNKNOWN";break;case 6:run.items[0].virtual_stack=1;break;case 7:run.items[0].detached=true;break;case 8:run.items[0].pokemon_id=123;break;case 9:run.items[0].arg=5;break;case 10:run.items[0].stack=3;break;case 11:run.items[0].stack=0;break;case 12:run.items[0].battle_count=1;break;case 13:run.run.has_challenges=true;break;case 14:run.items[0].key="WIDE_LENS";run.items[0].stack=4;break;case 15:run.n_items=PR_MAX_MODIFIERS;for(int j=0;j<run.n_items;j++)run.items[j]=(gc_item){.key="HEALING_CHARM",.stack=1};break;}
 gc_item before[GC_MAX_ITEMS];memcpy(before,run.items,sizeof before);int count=run.n_items;gc_mon party[6];memcpy(party,run.party,sizeof party);pr_rng rng=run.run.rng;double money=run.money;
 CHECK(gc_multi_add(&run,0)==PR_REWARD_HELD_UNSUPPORTED);CHECK(run.n_items==count&&!memcmp(before,run.items,sizeof before)&&!memcmp(party,run.party,sizeof party)&&!memcmp(&rng,&run.run.rng,sizeof rng)&&run.money==money);
 }
 setup();const char *why;gc_offer o=offer(0,false);
 for(int stack=-1;stack<=0;stack++){setup();run.n_items=1;run.items[0]=(gc_item){.key="HEALING_CHARM",.arg=-1,.stack=stack};gc_run before=run;CHECK(gc_multi_scene_apply(&run,&o,0,&why,true)==GC_APPLY_UNHANDLED&&!memcmp(&run,&before,sizeof run));}
CHECK(gc_reward_apply(&run,&o,-1,-1,&why)==GC_APPLY_REFUSED&&!strcmp(why,"pick a Pokemon"));
 const char *keys[]={"EVOLUTION_TRACKER_GIMMIGHOUL","FORM_CHANGE_ITEM","RARE_FORM_CHANGE_ITEM","BASE_STAT_BOOSTER","SOUL_DEW","SPECIES_STAT_BOOSTER","RARE_SPECIES_STAT_BOOSTER","EVIOLITE","MYSTERY_ENCOUNTER_SHUCKLE_JUICE","MYSTERY_ENCOUNTER_OLD_GATEAU","MYSTERY_ENCOUNTER_MACHO_BRACE",NULL};
 for(unsigned k=0;k<sizeof keys/sizeof *keys;k++){setup();run.n_items=1;run.items[0]=(gc_item){.key=keys[k],.arg=0,.stack=1,.pokemon_id=run.party[0].b.id};gc_run before=run;CHECK(gc_multi_scene_apply(&run,&o,0,&why,true)==GC_APPLY_UNHANDLED&&!memcmp(&run,&before,sizeof run));}
 setup();run.party[0].b.fusion_species=SPECIES_ID_RATTATA;gc_run before=run;CHECK(gc_multi_scene_apply(&run,&o,0,&why,true)==GC_APPLY_UNHANDLED&&!memcmp(&run,&before,sizeof run));
 setup();run.n_items=1;run.items[0]=(gc_item){.key="LEFTOVERS",.stack=1};before=run;CHECK(gc_multi_scene_apply(&run,&o,0,&why,true)==GC_APPLY_UNHANDLED&&!memcmp(&run,&before,sizeof run));
 setup();run.n_items=1;run.items[0]=(gc_item){.key="MULTI_LENS",.arg=-1,.stack=1,.pokemon_id=run.party[0].b.id,.virtual_stack=1};CHECK(gc_reward_apply(&run,&o,0,-1,&why)==GC_APPLY_UNHANDLED);CHECK(gc_port_apply_reward(&run,&o,0,-1,&why)&&run.stubs[GC_STUB_REWARD_APPLY]==1&&run.items[0].virtual_stack==1);
}
#include "multi-fallback.native.inc"
static void fallback_inventory(void){
 CHECK(sizeof fallback_native/sizeof *fallback_native==192);
 for(unsigned i=0;i<sizeof fallback_native/sizeof *fallback_native;i++){
  setup();int target=fallback_native[i].storage;run.party_order[0]=target;run.party_order[1]=1-target;
  if(fallback_native[i].context==7)run.party[target].b.fusion_species=SPECIES_ID_RATTATA;
  else run.items[run.n_items++]=(gc_item){.key=fallback_native[i].context<6?"BASE_STAT_BOOSTER":"SOUL_DEW",.arg=fallback_native[i].context<6?fallback_native[i].context:0,.stack=1,.pokemon_id=run.party[target].b.id};
  int other=run.n_items;
  if(fallback_native[i].stack)run.items[run.n_items++]=(gc_item){.key="MULTI_LENS",.arg=fallback_native[i].old_arg,.stack=fallback_native[i].stack,.pokemon_id=run.party[target].b.id};
  gc_items_to_battle(&run);gc_item original=run.items[0];gc_system before=sys;pr_rng rng=run.run.rng;
  gc_offer o=offer(1,false);o.arg=fallback_native[i].incoming_arg;const char*why=NULL;
  CHECK(gc_reward_apply(&run,&o,target,-1,&why)==GC_APPLY_UNHANDLED); /* keep the whole-Scene coverage guard */
  bool result=gc_port_apply_reward(&run,&o,target,-1,&why);
  if(result!=fallback_native[i].ok||run.n_items!=other+fallback_native[i].n_lenses)fprintf(stderr,"fallback native case%u context%d stock%d oldArg%d incomingArg%d: result%d items%d\n",i,fallback_native[i].context,fallback_native[i].stack,fallback_native[i].old_arg,fallback_native[i].incoming_arg,result,run.n_items);
  CHECK(result==fallback_native[i].ok);CHECK(run.n_items==other+fallback_native[i].n_lenses);CHECK(run.items[other].stack==fallback_native[i].output_stack);
  if(other)CHECK(!memcmp(&original,&run.items[0],sizeof original));
  CHECK(run.stubs[GC_STUB_REWARD_APPLY]==1&&!memcmp(&sys,&before,sizeof sys)&&!memcmp(&rng,&run.run.rng,sizeof rng));
  gc_items_to_battle(&run);pr_pokemon*p=&run.party[target].b;int hits=1;pr_apply_multi_hit_count_modifiers(&run.bt,p,MOVE_ID_TACKLE,&hits);CHECK(hits==fallback_native[i].hits);
  for(int ordinal=0;ordinal<hits;ordinal++){p->hit_count=hits;p->hits_left=hits-ordinal;double d=1;pr_apply_multi_hit_damage_modifiers(&run.bt,p,MOVE_ID_TACKLE,&d);CHECK(d==fallback_native[i].factors[ordinal]);}
  CHECK(gc_run_can_save(&run));size_t size=gc_run_save(&run,saved,sizeof saved);CHECK(size&&gc_run_load(&loaded,saved,size,&why));
  int restored=1;pr_apply_multi_hit_count_modifiers(&loaded.bt,&loaded.party[target].b,MOVE_ID_TACKLE,&restored);CHECK(restored==hits);
  for(int ordinal=0;ordinal<hits;ordinal++){pr_pokemon*q=&loaded.party[target].b;q->hit_count=hits;q->hits_left=hits-ordinal;double d=1;pr_apply_multi_hit_damage_modifiers(&loaded.bt,q,MOVE_ID_TACKLE,&d);CHECK(d==fallback_native[i].factors[ordinal]);}
 }
 puts("Multi fallback: 192 native inventory/strike records and save round trips compared; Scene guards retained");
}
/* Remove only the optional extension to emulate an older tagged-field save.
 * Recompute its envelope checksum, leaving the inventory bytes untouched. */
static uint32_t legacy_hash(const char*s){uint32_t h=2166136261u;for(;*s;s++)h=(h^(unsigned char)*s)*16777619u;return h;}
static size_t without_multi_marker(size_t n){
 size_t at=20;bool found=false;
 while(at+8<=n){uint32_t name,len;memcpy(&name,saved+at,4);memcpy(&len,saved+at+4,4);CHECK(len<=n-at-8);
  if(name==legacy_hash("multi_lens_storage")){memmove(saved+at,saved+at+8+len,n-at-8-len);n-=8+len;found=true;break;}at+=8+len;
 }
 CHECK(found);uint32_t length=n-20;memcpy(saved+12,&length,4);uint32_t crc=~0u;
 for(size_t i=20;i<n;i++){crc^=saved[i];for(int j=0;j<8;j++)crc=(crc>>1)^((0u-(crc&1u))&0xedb88320u);}crc=~crc;memcpy(saved+16,&crc,4);return n;
}
static void legacy_guards(void){
 for(int control=0;control<14;control++){
  setup();run.items[0]=(gc_item){.key="MULTI_LENS",.arg=0,.stack=1,.pokemon_id=run.party[0].b.id};run.items[1]=run.items[0];run.items[1].arg=-1;run.n_items=2;
  switch(control){case 1:run.items[1].arg=0;break;case 2:run.items[1].stack=2;break;case 3:run.items[1].detached=true;break;case 4:run.items[1].pokemon_id=run.party[1].b.id;break;case 5:run.items[1].battle_count=1;break;case 6:run.items[2]=run.items[1];run.n_items=3;break;case 7:run.items[1].key="WIDE_LENS";break;case 8:run.items[1].arg=5;break;case 9:run.items[0].pokemon_id=run.items[1].pokemon_id=0;break;case 10:run.items[1].non_transferable=true;break;case 11:run.n_items=1;run.items[0].stack=3;break;case 12:run.n_items=1;run.items[0].stack=4;break;case 13:run.items[2]=run.items[0];run.items[3]=run.items[1];run.n_items=4;break;}
  gc_item original[4];memcpy(original,run.items,sizeof original);int count=run.n_items;const char*why=NULL;
  size_t n=gc_run_save(&run,saved,sizeof saved);CHECK(n>0);n=without_multi_marker(n);CHECK(gc_run_load(&loaded,saved,n,&why));
  bool repair=control==0||control==2||control==6||control>=11;
  CHECK(loaded.n_items==(repair?1:count));
  if(repair)CHECK(loaded.items[0].stack==2&&loaded.items[0].arg==0);
  else for(int j=0;j<count;j++)CHECK(!strcmp(loaded.items[j].key,original[j].key)&&loaded.items[j].stack==original[j].stack&&loaded.items[j].arg==original[j].arg&&loaded.items[j].pokemon_id==original[j].pokemon_id&&loaded.items[j].detached==original[j].detached&&loaded.items[j].battle_count==original[j].battle_count&&loaded.items[j].non_transferable==original[j].non_transferable);
 }
 puts("Multi legacy: six alias/three/four-lens repairs plus eight nonmatching, detached and lock controls");
}
static void legacy_fallback(void){
 const char*why=NULL;FILE*f=fopen(MULTI_FALLBACK_LEGACY,"rb");CHECK(f);size_t n=fread(saved,1,sizeof saved,f);CHECK(!ferror(f)&&fclose(f)==0&&n>0);
 CHECK(gc_run_load(&run,saved,n,&why));CHECK(run.n_items==2&&!strcmp(run.items[0].key,"BASE_STAT_BOOSTER")&&run.items[0].stack==1);
 CHECK(!strcmp(run.items[1].key,"MULTI_LENS")&&run.items[1].stack==2&&run.items[1].arg==0);
 int hits=1;pr_apply_multi_hit_count_modifiers(&run.bt,&run.party[0].b,MOVE_ID_TACKLE,&hits);CHECK(hits==3);
 for(int i=0;i<3;i++){pr_pokemon*p=&run.party[0].b;p->hit_count=3;p->hits_left=3-i;double d=1;pr_apply_multi_hit_damage_modifiers(&run.bt,p,MOVE_ID_TACKLE,&d);CHECK(d==(i?.25:.5));}
 n=gc_run_save(&run,saved,sizeof saved);CHECK(n&&gc_run_load(&loaded,saved,n,&why)&&loaded.n_items==2&&loaded.items[1].stack==2);
 /* A new-format deliberate duplicate remains distinct, as in the native raw
  * inventory controls; migration must not rewrite every imported list. */
 setup();run.items[0]=(gc_item){.key="MULTI_LENS",.arg=0,.stack=1,.pokemon_id=run.party[0].b.id};run.items[1]=run.items[0];run.items[1].arg=-1;run.n_items=2;
 n=gc_run_save(&run,saved,sizeof saved);CHECK(n&&gc_run_load(&loaded,saved,n,&why)&&loaded.n_items==2&&loaded.items[0].stack==1&&loaded.items[1].stack==1);
 puts("Multi legacy: pre-fix split stock repaired to 50/25/25; current native duplicate controls preserved");
}
int main(void){legacy_guards();legacy_fallback();fallback_inventory();acquisition();refusal();printf("Multi acquisition host: %d checks passed\n",checks);return 0;}
