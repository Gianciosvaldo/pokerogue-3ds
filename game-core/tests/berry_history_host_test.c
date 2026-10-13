/* Berry histories survive legal command saves; inventory restoration is idempotent. */
#include "../src/internal.h"
#include <stdio.h>
#include <stdlib.h>
static gc_run run, loaded;
static unsigned char saved[GC_SAVE_MAX], mutated[GC_SAVE_MAX];
static int checks;
#define CHECK(x) do {checks++;if(!(x)){fprintf(stderr,"berry history line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static uint32_t hashname(const char *s){uint32_t h=2166136261u;while(*s)h=(h^(unsigned char)*s++)*16777619u;return h;}
static uint32_t crc32(const unsigned char *p,size_t n){uint32_t c=~0u;for(size_t i=0;i<n;i++){c^=p[i];for(int k=0;k<8;k++)c=c>>1^(0xedb88320u&(0u-(c&1)));}return ~c;}
static int hide(size_t n,const char *field){uint32_t h=hashname(field),unknown=hashname("unknown-berry-field");int hits=0;for(size_t i=20;i+4<=n;i++)if(!memcmp(mutated+i,&h,4)){memcpy(mutated+i,&unknown,4);hits++;}uint32_t crc=crc32(mutated+20,n-20);memcpy(mutated+16,&crc,4);return hits;}
int main(void)
{
    const char *why=NULL;
    gc_run_new(&run,GAME_MODES_CLASSIC,"berry-history-save",777);
    gc_starter starter={.species=SPECIES_ID_BULBASAUR,.tera_type=-1,.moves={MOVE_ID_SPLASH}};
    memset(starter.ivs,15,sizeof starter.ivs);CHECK(gc_run_set_starters(&run,&starter,1,&why));
    gc_event ev;while(gc_run_next_event(&run,&ev)){}
    CHECK(run.screen.kind==GC_SCREEN_COMMAND&&gc_run_can_save(&run));
    pr_pokemon *p=run.bt.field[0];CHECK(p!=NULL);
    pr_record_eaten_berry(&run.bt,p,BERRY_TYPE_LUM,true);
    pr_record_eaten_berry(&run.bt,p,BERRY_TYPE_SITRUS,false);
    CHECK(p->has_eaten_berry&&p->n_battle_berries==1&&p->battle_berries[0]==BERRY_TYPE_LUM);
    CHECK(p->n_turn_berries==2&&p->turn_berries[1]==BERRY_TYPE_SITRUS);
    p->n_last_berries=2;p->last_berries[0]=BERRY_TYPE_LUM;p->last_berries[1]=BERRY_TYPE_STARF;
    size_t n=gc_run_save(&run,saved,sizeof saved);CHECK(n>0);
    CHECK(gc_run_load(&loaded,saved,n,&why));pr_pokemon *q=loaded.bt.field[0];CHECK(q!=NULL);
    CHECK(q->has_eaten_berry&&q->n_battle_berries==1&&q->battle_berries[0]==BERRY_TYPE_LUM);
    CHECK(q->n_last_berries==2&&q->last_berries[1]==BERRY_TYPE_STARF);
    const char *fields[]={"battle_berries","turn_berries","last_berries","n_battle_berries","n_turn_berries","n_last_berries"};
    for(int i=0;i<6;i++){memcpy(mutated,saved,n);CHECK(hide(n,fields[i])>0);CHECK(!gc_run_load(&loaded,mutated,n,&why));}
    memcpy(mutated,saved,n);for(int i=0;i<6;i++)CHECK(hide(n,fields[i])>0);
    CHECK(gc_run_load(&loaded,mutated,n,&why));CHECK(loaded.bt.field[0]->n_battle_berries==0&&loaded.bt.field[0]->n_last_berries==0);
    CHECK(gc_run_load(&loaded,saved,n,&why));q=loaded.bt.field[0];
    pr_reset_turn_data(q);CHECK(q->n_turn_berries==0&&q->n_last_berries==2&&q->n_battle_berries==1);
    pr_reset_summon_data(q);CHECK(q->n_last_berries==0&&q->n_battle_berries==1&&q->has_eaten_berry);
    pr_reset_battle_data(q);CHECK(q->n_battle_berries==0&&!q->has_eaten_berry);
    /* Reject oversized and unknown histories before reset can conceal invalid turn data. */
    const uint16_t turn_count=p->n_turn_berries;
    p->n_turn_berries=PR_MAX_BERRY_HISTORY+1;n=gc_run_save(&run,saved,sizeof saved);CHECK(n>0);CHECK(!gc_run_load(&loaded,saved,n,&why));p->n_turn_berries=turn_count;
    const uint8_t berry=p->turn_berries[0];p->turn_berries[0]=BERRY_TYPE_LEPPA+1;
    n=gc_run_save(&run,saved,sizeof saved);CHECK(n>0);CHECK(!gc_run_load(&loaded,saved,n,&why));p->turn_berries[0]=berry;
    run.n_items=1;run.items[0]=(gc_item){.key="BERRY_POUCH",.stack=1,.virtual_stack=2};gc_items_to_battle(&run);
    CHECK(run.bt.mods[0][0].cls==PR_MOD_PRESERVE_BERRY&&!run.bt.mods[0][0].unknown_subclass&&pr_mod_get_stack_count(&run.bt.mods[0][0])==3);
    pr_refresh_berry_modifiers(&run.bt,0);gc_items_from_battle(&run);CHECK(run.n_items==1&&run.items[0].stack==1&&run.items[0].virtual_stack==0&&!run.items[0].detached);
    run.items[0].stack=2;run.items[0].virtual_stack=2;gc_items_to_battle(&run);CHECK(run.bt.mods[0][0].unknown_subclass);
    /* Restore a berry that was absent at setup, and preserve it on repeated sync. */
    run.n_items=0;gc_items_to_battle(&run);
    run.bt.mods[0][0]=(pr_modifier){.cls=PR_MOD_BERRY,.pokemon_id=p->id,.berry_type=BERRY_TYPE_LEPPA,.stack_count=1,.is_transferable=true};
    run.bt.n_mods[0]=1;gc_items_from_battle(&run);
    CHECK(run.n_items==1&&!strcmp(run.items[0].key,"BERRY")&&run.items[0].arg==BERRY_TYPE_LEPPA&&run.items[0].stack==1);
    gc_items_from_battle(&run);CHECK(run.n_items==1&&run.items[0].stack==1);
    run.bt.mods[0][0].stack_count=2;gc_items_from_battle(&run);CHECK(run.n_items==1&&run.items[0].stack==2);
    run.bt.n_mods[0]=0;gc_items_from_battle(&run);CHECK(run.n_items==0);
    /* Both owners, both sides, and different berry kinds remain distinct. */
    uint32_t enemy=run.bt.field[2]->id;
    run.bt.mods[0][0]=(pr_modifier){.cls=PR_MOD_BERRY,.pokemon_id=p->id,.berry_type=BERRY_TYPE_LUM,.stack_count=1};
    run.bt.mods[0][1]=(pr_modifier){.cls=PR_MOD_BERRY,.pokemon_id=p->id,.berry_type=BERRY_TYPE_SITRUS,.stack_count=2};
    run.bt.mods[1][0]=(pr_modifier){.cls=PR_MOD_BERRY,.pokemon_id=enemy,.berry_type=BERRY_TYPE_LUM,.stack_count=2};
    run.bt.n_mods[0]=2;run.bt.n_mods[1]=1;gc_items_from_battle(&run);
    CHECK(run.n_items==2&&run.n_enemy_items==1&&run.enemy_items[0].pokemon_id==enemy);
    gc_items_from_battle(&run);CHECK(run.n_items==2&&run.n_enemy_items==1);
    /* Inventory pressure preserves opaque entries and explicitly reports refusal. */
    run.n_items=GC_MAX_ITEMS;for(int i=0;i<run.n_items;i++)run.items[i]=(gc_item){.key="HEALING_CHARM",.stack=1};
    gc_items_to_battle(&run);
    run.bt.mods[0][0]=(pr_modifier){.cls=PR_MOD_BERRY,.pokemon_id=p->id,.berry_type=BERRY_TYPE_LUM,.stack_count=1};
    run.bt.n_mods[0]=1;run.bt.n_unported=0;gc_items_from_battle(&run);
    CHECK(run.n_items==GC_MAX_ITEMS&&run.bt.n_unported>0);
    /* The fixed history bound refuses overflow rather than silently calling it parity. */
    p->n_battle_berries=p->n_turn_berries=PR_MAX_BERRY_HISTORY;run.bt.n_unported=0;
    pr_record_eaten_berry(&run.bt,p,BERRY_TYPE_LUM,true);
    CHECK(p->n_battle_berries==PR_MAX_BERRY_HISTORY&&p->n_turn_berries==PR_MAX_BERRY_HISTORY&&run.bt.n_unported>0);
    printf("Berry history command/save/reset/inventory: %d checks passed\n",checks);return 0;
}
