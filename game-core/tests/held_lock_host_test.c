/* Actual source constructor locks -> bridge, UI callbacks, save reconstruction. */
#define main retained_doubles_main
#include "doubles_host_test.c"
#undef main
static int key_index(const char *key){const char *keys[]={"FORM_CHANGE_ITEM","RARE_FORM_CHANGE_ITEM","BASE_STAT_BOOSTER","EVOLUTION_TRACKER_GIMMIGHOUL","MYSTERY_ENCOUNTER_SHUCKLE_JUICE","MYSTERY_ENCOUNTER_OLD_GATEAU","MYSTERY_ENCOUNTER_MACHO_BRACE"};for(int i=0;i<7;i++)if(!strcmp(key,keys[i]))return i;return -1;}
int main(int argc,char **argv){CHECK(argc==2);FILE *f=fopen(argv[1],"r");CHECK(f);char line[256];CHECK(fgets(line,sizeof line,f)&&!strcmp(line,"HELDLOCK 1 2632\n"));
 static bool seen[7][215][4][2];int totals[7]={0};setup(1);
 for(int k=0;k<4;k++){pr_pokemon *p=r.bt.field[k];p->custom_ability=ABILITY_ID_NONE;p->passive_flag=false;pr_refresh_ability(p);}
 for(int i=0;i<2632;i++){
  int id,arg,owner,active,locked,restored,removed,visible,end;char key[64];CHECK(fgets(line,sizeof line,f)&&strchr(line,'\n'));
  CHECK(sscanf(line,"C %d %63s %d %d %d %d %d %d %d %n",&id,key,&arg,&owner,&active,&locked,&restored,&removed,&visible,&end)==9&&line[end]==0);
  int ki=key_index(key);CHECK(id==i&&ki>=0&&arg>=-15&&arg<=199&&owner>=0&&owner<4&&(active==0||active==1));
  CHECK((locked==0||locked==1)&&(restored==0||restored==1)&&(removed==0||removed==1)&&visible>=0);
  CHECK(!seen[ki][arg+15][owner][active]);seen[ki][arg+15][owner][active]=true;totals[ki]++;
  if(ki==2)CHECK(arg>=0&&arg<=5);
  if(ki==3||ki>=5)CHECK(arg==0);
  if(ki==4)CHECK(arg==10||arg==-15);
  if(ki<2)CHECK(arg>=0);
  const int item=gc_reward_item_index(key);CHECK(item>=0);const char *stable=pr_item_keys[item];
  r.n_items=r.n_enemy_items=0;pr_pokemon *target=r.bt.field[owner],*user=r.bt.field[owner<2?2:0];
  gc_item stock={.key=stable,.arg=arg,.stack=1,.pokemon_id=target->id};
  if(owner<2){gc_item_add(&r,stable,arg,target->id,1);CHECK(r.n_items==1&&r.items[0].non_transferable==locked);
   /* An old run could have persisted false for an intrinsically locked item. */
   r.items[0].non_transferable=false;gc_item old=r.items[0];
   CHECK(!gc_item_transfer(&r,0,1,1)&&!gc_item_discard(&r,0,1)&&!memcmp(&old,&r.items[0],sizeof old));
  }else{r.enemy_items[0]=stock;r.n_enemy_items=1;}
  gc_items_to_battle(&r);pr_modifier *m=&r.bt.mods[owner<2?0:1][0];CHECK(!m->is_transferable==locked);
  pr_rng rng=r.bt.rng;r.bt.n_unported=0;r.eng.overflow=false;
  CHECK(pr_remove_move_held_item(&r.bt,&r.eng,user,target,false,NULL)==removed);
  CHECK(!memcmp(&rng,&r.bt.rng,sizeof rng)&&!r.bt.n_unported&&!r.eng.overflow);
  const char *why;size_t n=gc_run_save(&r,save,sizeof save);CHECK(n&&gc_run_load(&back,save,n,&why)&&!back.load_dropped);
  gc_item *loaded=owner<2?back.items:back.enemy_items;CHECK(loaded[0].non_transferable==restored&&loaded[0].arg==arg&&loaded[0].stack==1&&loaded[0].pokemon_id==target->id&&!strcmp(loaded[0].key,key));
  CHECK((int)!loaded[0].non_transferable==visible);gc_items_to_battle(&back);CHECK(!back.bt.mods[owner<2?0:1][0].is_transferable==restored);
 }
 CHECK(totals[0]==1272&&totals[1]==1272&&totals[2]==48&&totals[3]==8&&totals[4]==16&&totals[5]==8&&totals[6]==8);
 CHECK(fgets(line,sizeof line,f)&&!strcmp(line,"END 2632\n")&&fgetc(f)==EOF);fclose(f);
 /* Transferable constructor classes keep their behavior, including saved
  * explicit locks such as the encounter-granted Mini Black Hole. */
 const char *controls[]={"LEFTOVERS","EVIOLITE","SPECIES_STAT_BOOSTER","MINI_BLACK_HOLE"};
 for(int i=0;i<4;i++){r.n_items=0;gc_item_add(&r,controls[i],0,r.party[0].b.id,1);CHECK(!r.items[0].non_transferable&&!gc_item_key_locked(controls[i]));
  r.items[0].non_transferable=true;const char *why;size_t n=gc_run_save(&r,save,sizeof save);CHECK(n&&gc_run_load(&back,save,n,&why)&&back.items[0].non_transferable);}
 printf("Held constructor locks: 2632 native observations, %d bridge/UI/save checks passed\n",checks);return 0;
}
