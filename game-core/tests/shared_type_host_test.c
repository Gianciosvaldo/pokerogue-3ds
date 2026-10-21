/* Native two-theft sequence, with inventory projection between each operation. */
#define main retained_transfer_host_main
#include "item_transfer_host_test.c"
#undef main
static pr_modifier *held(int actor) {
 int side=actor/2;int64_t id=run.bt.party[side][actor%2]->id;
 for(int k=0;k<run.bt.n_mods[side];k++)if(run.bt.mods[side][k].pokemon_id==id)return &run.bt.mods[side][k];
 return NULL;
}
static void sync_stock(void){gc_items_from_battle(&run);gc_items_to_battle(&run);}
int main(void){
 const char *keys[]={"SCOPE_LENS","LEFTOVERS","SHELL_BELL","KINGS_ROCK"};
 for(int family=0;family<4;family++)for(int enemy=0;enemy<2;enemy++){
  setup();
  run.party[1]=run.party[0];run.party[1].b.id+=1000;run.n_party=2;run.party_order[1]=1;
  run.enemy[1]=run.enemy[0];run.enemy[1].b.id+=2000;run.n_enemy=2;
  for(int side=0;side<2;side++)for(int i=0;i<2;i++){
   pr_pokemon *p=side?&run.enemy[i].b:&run.party[i].b;
   run.bt.party[side][i]=run.bt.field[side*2+i]=p;p->on_field=true;p->has_trainer=side;
   run.bt.n_party[side]=2;
  }
  run.bt.double_battle=true;
  int owner=enemy?2:0,recipient=enemy?0:2,thief=enemy?3:1,independent=enemy?1:3;
  gc_item *stock=enemy?run.enemy_items:run.items,*other=enemy?run.items:run.enemy_items;
  stock[0]=(gc_item){.key=keys[family],.arg=-1,.stack=2,.pokemon_id=run.bt.field[owner]->id};
  other[0]=(gc_item){.key=keys[family],.arg=-1,.stack=1,.pokemon_id=run.bt.field[independent]->id};
  run.n_items=run.n_enemy_items=1;gc_items_to_battle(&run);
  CHECK(pr_try_transfer_held_item(&run.bt,&run.eng,enemy,0,run.bt.field[recipient],1,true,true));
  sync_stock();
  CHECK(held(owner)&&held(recipient)&&held(independent));
  CHECK(held(owner)->type_ref && held(owner)->type_ref==held(recipient)->type_ref);
  CHECK(held(independent)->type_ref==0);
  CHECK(pr_steal_move_held_item(&run.bt,&run.eng,run.bt.field[thief],run.bt.field[recipient],1));
  sync_stock();
  int tier=enemy?3:4;
  CHECK(held(owner)->type_tier==tier+1);
  CHECK(held(thief)->type_tier==tier+1 && held(owner)->type_ref==held(thief)->type_ref);
  CHECK(held(independent)->type_tier==0);
  CHECK(pr_steal_move_held_item(&run.bt,&run.eng,run.bt.field[recipient],run.bt.field[owner],1));
  sync_stock();
  CHECK(held(recipient)->type_tier==tier+1 && held(thief)->type_tier==tier+1);
  CHECK(held(recipient)->type_ref==held(thief)->type_ref);
  CHECK(!held(owner) && held(independent)->type_tier==0);
  CHECK(!run.bt.n_unported && !run.eng.overflow);
 }
 /* An actual legal encounter save reconstructs fresh native ModifierType objects. */
 setup();run.items[0]=(gc_item){.key="SCOPE_LENS",.arg=-1,.stack=2,.pokemon_id=run.party[0].b.id};run.n_items=1;
 gc_items_to_battle(&run);
 CHECK(pr_try_transfer_held_item(&run.bt,&run.eng,0,0,run.bt.field[2],1,true,true));sync_stock();
 CHECK(run.items[0].type_ref && run.items[0].type_ref==run.enemy_items[0].type_ref);
 roundtrip();CHECK(loaded.items[0].type_ref==0 && loaded.enemy_items[0].type_ref==0);
 printf("Shared held-type native sequences/bridge/save: %d checks passed\n",checks);return 0;
}
