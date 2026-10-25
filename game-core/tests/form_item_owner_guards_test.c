#include "../src/internal.h"
#include "pr_item_forms.h"
#include <stdio.h>
#include <stdlib.h>
static gc_run r;
static gc_system sys;
static unsigned checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"form owner guard C%d: %s\n",__LINE__,#x);exit(1);}}while(0)
static void setup(int species,int item)
{
    gc_run_new(&r,GAME_MODES_CLASSIC,"form-guards",777);r.sys=&sys;r.n_party=1;r.party_order[0]=0;
    gc_starter starter={.species=species,.tera_type=-1};memset(starter.ivs,31,6);
    gc_mon_new_player(&r,&r.party[0],&starter,5);r.bt.n_party[0]=1;r.bt.party[0][0]=&r.party[0].b;
    r.n_items=1;r.items[0]=(gc_item){.key="FORM_CHANGE_ITEM",.arg=item,.stack=1,.pokemon_id=r.party[0].b.id,.form_inactive=true,.non_transferable=true};
    r.screen.kind=GC_SCREEN_REWARD;gc_int(&r)->rq.current=gc_rp(RP_SELECT_MODIFIER,0,0,0);gc_int(&r)->rq.waiting=true;
}
int main(void)
{
    const char *why=NULL;CHECK(!gc_form_item_toggle(NULL,0,0,&why)&&why);
    for(int bad=0;bad<17;bad++){
        setup(SPECIES_ID_VENUSAUR,FORM_CHANGE_ITEM_VENUSAURITE);
        int party=0,item=0;
        switch(bad){
        case 0:r.screen.kind=GC_SCREEN_COMMAND;break;
        case 1:r.finished=true;break;
        case 2:gc_int(&r)->rq.waiting=false;break;
        case 3:gc_int(&r)->rq.current.type=RP_COMMAND;break;
        case 4:party=-1;break;case 5:party=1;break;case 6:item=-1;break;case 7:item=1;break;
        case 8:r.items[0].pokemon_id++;break;case 9:r.items[0].detached=true;break;
        case 10:r.items[0].stack=2;break;case 11:r.items[0].virtual_stack=1;break;
        case 12:r.items[0].eviolite_base=true;break;case 13:r.items[0].arg=200;break;
        case 14:r.n_items=PR_MAX_MODIFIERS+1;break;case 15:r.n_enemy_items=-1;break;
        case 16:r.items[0].key=NULL;break;
        }
        gc_item before=r.items[0];gc_rqueue queue=gc_int(&r)->rq;pr_pokemon pokemon=r.party[0].b;
        CHECK(!gc_form_item_toggle(&r,party,item,&why)&&why);
        CHECK(!memcmp(&before,&r.items[0],sizeof before)&&!memcmp(&queue,&gc_int(&r)->rq,sizeof queue)&&!memcmp(&pokemon,&r.party[0].b,sizeof pokemon));
    }
    setup(SPECIES_ID_KYUREM,FORM_CHANGE_ITEM_DARK_STONE);
    r.sys=NULL;CHECK(!gc_form_item_toggle(&r,0,0,&why)&&why&&r.items[0].form_inactive&&r.party[0].b.form==0);
    CHECK(r.bt.n_unported==1&&r.bt.unported[0]==PR_FEAT_FORM_CHANGE);
    /* A failed predicate still flips the item, even when the owner has fainted. */
    setup(SPECIES_ID_MEW,FORM_CHANGE_ITEM_VENUSAURITE);r.party[0].b.hp=0;r.sys=NULL;
    CHECK(gc_form_item_toggle(&r,0,0,NULL)&&!r.items[0].form_inactive&&r.party[0].b.hp==0&&r.party[0].b.form==0);
    CHECK(!r.bt.n_unported&&!r.ev_count);
    printf("Form owner guard boundaries: %u checks\n",checks);return 0;
}
