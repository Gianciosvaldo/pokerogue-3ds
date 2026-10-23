/* Real native lists larger than the original battle and game storage limits. */
#define main retained_doubles_main
#include "doubles_host_test.c"
#undef main
static int classes[2][GC_MAX_ITEMS];
static gc_item expected[2][GC_MAX_ITEMS];
static int counts[2];
static uint32_t owner(int n){CHECK(n>=0&&n<8);return n<6?r.party[n].b.id:r.enemy[n-6].b.id;}
static void read_lists(FILE *f,const char *stage,bool fill){
 for(int side=0;side<2;side++){
  char tag[40],part[40];int got,n;
  CHECK(fscanf(f,"%39s %39s %d %d",tag,part,&got,&n)==4&&!strcmp(tag,"LIST")&&!strcmp(part,stage)&&got==side);
  CHECK(n==(side?(fill?114:113):342)&&n<=PR_MAX_MODIFIERS&&n<=GC_MAX_ITEMS);
  counts[side]=n;
  for(int i=0;i<n;i++){
   char key[80],cls[80];int arg,who,stack;
   CHECK(fscanf(f,"%39s %79s %d %d %d %79s",tag,key,&arg,&who,&stack,cls)==6&&!strcmp(tag,"ITEM"));
   int item=gc_reward_item_index(key),c=pr_mod_class_by_name(cls);
   CHECK(item>=0&&c>=0&&arg>=-1&&arg<18&&stack>=1&&stack<=99);
   expected[side][i]=(gc_item){.key=pr_item_keys[item],.arg=arg,.pokemon_id=owner(who),.stack=stack};classes[side][i]=c;
  }
  if(fill){gc_item *items=side?r.enemy_items:r.items;memcpy(items,expected[side],(size_t)n*sizeof *items);if(side)r.n_enemy_items=n;else r.n_items=n;}
 }
}
static void compare(gc_run *run){
 CHECK(run->bt.n_mods[0]==counts[0]&&run->bt.n_mods[1]==counts[1]);
 CHECK(run->n_items==counts[0]&&run->n_enemy_items==counts[1]);
 for(int side=0;side<2;side++)for(int i=0;i<counts[side];i++){
  const gc_item *it=&(side?run->enemy_items:run->items)[i],*want=&expected[side][i];
  const pr_modifier *m=&run->bt.mods[side][i];
  CHECK(!strcmp(it->key,want->key)&&it->arg==want->arg&&it->stack==want->stack&&it->pokemon_id==want->pokemon_id);
  CHECK(m->cls==classes[side][i]&&!m->unknown_subclass&&m->stack_count==want->stack&&m->pokemon_id==want->pokemon_id);
 }
 CHECK(!run->bt.n_unported);
}
static void restore_and_compare(void){
 const char *why=NULL;size_t n=gc_run_save(&r,save,sizeof save);
 CHECK(n&&gc_run_load(&back,save,n,&why)&&!back.load_dropped);compare(&back);
}
int main(int argc,char **argv){
 CHECK(argc==2);FILE *f=fopen(argv[1],"r");CHECK(f);
 char header[40],tag[40];int version,index;
 CHECK(fscanf(f,"# %39s %d %d",header,&version,&index)==3&&!strcmp(header,"INVENTORY_CAPACITY")&&version==1&&index==107);
 setup(1);for(int k=3;k<6;k++){r.party[k]=r.party[2];r.party[k].b.id+=k;r.party_order[k]=k;}
 r.n_party=r.bt.n_party[0]=6;for(int k=0;k<6;k++){r.bt.party[0][k]=&r.party[k].b;r.party[k].b.custom_ability=ABILITY_ID_NONE;pr_refresh_ability(&r.party[k].b);}
 for(int k=0;k<2;k++){r.enemy[k].b.custom_ability=ABILITY_ID_NONE;pr_refresh_ability(&r.enemy[k].b);}
 read_lists(f,"before",true);gc_items_to_battle(&r);compare(&r);restore_and_compare();
 pr_reward_ctx *ctx=gc_reward_ctx(&r);CHECK(ctx->n_items==342&&ctx->items[341].item==PR_ITEM_SPECIES_STAT_BOOSTER);
 CHECK(pr_try_transfer_held_item(&r.bt,&r.eng,1,index,&r.party[5].b,1,true,true));
 gc_items_from_battle(&r);read_lists(f,"after",false);compare(&r);restore_and_compare();
 int n;CHECK(fscanf(f,"%39s %d",tag,&n)==2&&!strcmp(tag,"LOOT")&&n==113);
 for(int k=0;k<2;k++)for(int i=0;i<r.bt.n_mods[1];i++)if(r.bt.mods[1][i].pokemon_id==r.enemy[k].b.id)CHECK(pr_add_post_battle_loot(&r.bt,&r.bt.mods[1][i]));
 CHECK(r.bt.post_battle_loot==n&&!r.bt.n_unported);
 for(int i=0;i<n;i++){int pos;CHECK(fscanf(f,"%d",&pos)==1&&pos>=0&&pos<r.bt.n_mods[1]);CHECK(r.bt.mods[1][pos].loot_ref==r.bt.post_battle_loot_refs[i]&&r.bt.mods[1][pos].pokemon_id==0);}
 CHECK(fscanf(f,"%39s",tag)==1&&!strcmp(tag,"COMPLETE")&&fscanf(f,"%39s",tag)==EOF);fclose(f);
 // Storage stress is separate from native acquisition parity: deliberately repeated
 // entries exercise the full serializable capacity on both sides.
 for(int side=0;side<2;side++){
  gc_item *items=side?r.enemy_items:r.items;
  for(int i=0;i<PR_MAX_MODIFIERS;i++)items[i]=(gc_item){.key="LEFTOVERS",.arg=-1,.stack=1,.pokemon_id=side?r.enemy[0].b.id:r.party[0].b.id};
 }
 r.n_items=r.n_enemy_items=PR_MAX_MODIFIERS;
 const char *why=NULL;size_t length=gc_run_save(&r,save,sizeof save);
 CHECK(length>128*1024&&length<=GC_SAVE_MAX&&gc_run_load(&back,save,length,&why)&&!back.load_dropped);
 CHECK(back.n_items==PR_MAX_MODIFIERS&&back.n_enemy_items==PR_MAX_MODIFIERS);
 for(int side=0;side<2;side++)for(int i=0;i<PR_MAX_MODIFIERS;i++){
  const gc_item *a=&(side?r.enemy_items:r.items)[i],*b=&(side?back.enemy_items:back.items)[i];
  CHECK(!strcmp(a->key,b->key)&&a->arg==b->arg&&a->stack==b->stack&&a->pokemon_id==b->pokemon_id);
 }
 // Full add and malformed counts must report limits, never silently discard stock.
 r.bt.n_unported=0;gc_item_add(&r,"SHELL_BELL",-1,r.party[0].b.id,1);
 CHECK(r.n_items==PR_MAX_MODIFIERS&&r.bt.n_unported);
 r.bt.n_unported=0;
 // Malformed counts must report an explicit limit rather than running truncated lists.
 r.n_items=PR_MAX_MODIFIERS+1;gc_items_to_battle(&r);CHECK(r.bt.n_mods[0]==0&&r.bt.n_mods[1]==0&&r.bt.n_unported);
 memset(r.items,0,sizeof r.items);memset(r.enemy_items,0,sizeof r.enemy_items);
 const int invalid[]={-1,PR_MAX_MODIFIERS+1,GC_MAX_ITEMS+1};
 for(int side=0;side<2;side++)for(unsigned k=0;k<sizeof invalid/sizeof *invalid;k++){
  r.n_items=side?0:invalid[k];r.n_enemy_items=side?invalid[k]:0;r.bt.n_unported=0;
  gc_items_to_battle(&r);CHECK(r.bt.n_mods[0]==0&&r.bt.n_mods[1]==0&&r.bt.n_unported);
 }
 printf("Native large inventory, transfer, loot and save: %d checks passed\n",checks);return 0;
}
