#include "football_career.h"
#include "gc/data.h"
#include "football_profile_fixture.h"
#include <stdio.h>
#include <string.h>
static FootballCareer c,loaded;
static FootballLive preview;
static gc_run scratch,other;
static gc_system profile;
static unsigned char bytes[FB_CAREER_SAVE_MAX];
static unsigned checks,failed;
#define CHECK(c,...) do{++checks;if(!(c)){++failed;printf("FAIL: ");printf(__VA_ARGS__);puts("");}}while(0)
static void win(void)
{
    c.phase=FB_CAREER_RESULT;c.live.rules.winner=FB_HOME;
    CHECK(football_career_continue(&c),"continue win");
    unsigned guard=0;
    while(c.phase!=FB_CAREER_SHOP && ++guard<10000){
        if(c.phase==FB_CAREER_EVOLVE)CHECK(football_career_evolve(&c,0),"evolve");
        else if(c.phase==FB_CAREER_EXP)football_career_tick(&c,16,(FootballLiveInput){.receiver=-1});
        else break;
    }
    CHECK(c.phase==FB_CAREER_SHOP,"actual EXP/evolution queue reaches shop");
}
int main(void)
{
    football_test_unlock_all(&profile);
    CHECK(football_career_new(&c,&scratch,&profile,7),"new career");
    gc_starter starter={.species=SPECIES_ID_MAWILE,.tera_type=-1};
    CHECK(football_roster_add(&c.roster,&scratch,&starter,&profile,NULL),"draft Mawile");
    for(int i=0;i<gc_starter_cost_count && c.roster.count<22;++i){
        if(gc_starter_costs[i].cost>1)continue;
        starter.species=gc_starter_costs[i].id;
        football_roster_add(&c.roster,&scratch,&starter,&profile,NULL);
    }
    CHECK(c.roster.count==22,"legal full 55-point roster");c.phase=FB_CAREER_READY;
    CHECK(football_career_start(&c),"first match");win();
    CHECK(c.stone_target==0 && c.stone_item==FORM_CHANGE_ITEM_MAWILITE,"first eligible stone offered (target%d item%d)",c.stone_target,c.stone_item);
    CHECK(!football_career_buy(&c,2,0) && !c.roster.mega_item[0],"cannot spend the first win's insufficient cash");
    /* Keep real generated shops: do not fabricate an offer to make the activation test pass. */
    while((c.stone_target<0 || c.wallet<c.prices[2]) && c.round<6){
        CHECK(football_career_continue(&c),"leave shop");CHECK(football_career_start(&c),"next match");win();
    }
    CHECK(c.stone_target==0 && c.stone_item==FORM_CHANGE_ITEM_MAWILITE,"actual applicable stone in shop");
    unsigned wallet=c.wallet;
    int item=c.stone_item;c.stone_item=FORM_CHANGE_ITEM_VENUSAURITE;
    CHECK(!football_career_buy(&c,2,0) && c.wallet==wallet && !c.roster.mega_item[0],"wrong-species stone cannot charge or equip");c.stone_item=item;
    CHECK(football_career_buy(&c,2,0),"buy actual generated stone");
    CHECK(c.wallet+1000==wallet && c.roster.mega_item[0]==FORM_CHANGE_ITEM_MAWILITE,"pay once and retain ownership");
    CHECK(!football_career_buy(&c,2,0),"no duplicate purchase");
    CHECK(football_career_continue(&c),"next ready screen");c.mega=1;
    CHECK(!football_career_start(&c) && c.phase==FB_CAREER_READY,"unowned Mega selection cannot start a regular-form match silently");c.mega=0;
    gc_football_new(&other,NULL,"mega-expected",1);other.party[0]=c.roster.mon[0];other.n_party=1;
    const pr_form *form=gc_football_mega_form(&other,0,c.roster.mega_item[0]);
    FootballPlayer expected;
    CHECK(form && football_stats_generate(&expected,c.roster.mon[0].b.id,form,&c.roster.maxima),"actual Mega form stats");
    /* Put the selected member on the active unit, whichever side receives the first possession. */
    CHECK(football_live_begin(&preview,&c.roster,c.seed+c.round*104729u,c.round),"deterministic possession preview");
    if(preview.rules.offense==FB_AWAY){uint8_t swap=c.roster.lineup[0];c.roster.lineup[0]=c.roster.lineup[18];c.roster.lineup[18]=swap;}
    CHECK(football_career_start(&c),"start with selected stone");
    CHECK(!memcmp(&c.live.units[FB_HOME][0],&expected.attributes,sizeof expected.attributes),"Mega stats in live roster");
    unsigned active=0;
    for(unsigned i=0;i<22;++i)if(c.live.actors[i].home && c.live.actors[i].roster==0){
        active++;CHECK(!memcmp(&c.live.actors[i].attributes,&expected.attributes,sizeof expected.attributes),"first formation reflects Mega stats");
    }
    CHECK(active==1,"selected member is active");
    CHECK(c.roster.mon[0].b.form==0,"underlying roster stays in regular form");
    size_t n=football_career_save(&c,bytes,sizeof bytes);
    CHECK(n && football_career_load(&loaded,&other,&profile,bytes,n),"owned stone and match selection resume");
    CHECK(loaded.mega==0 && loaded.roster.mega_item[0]==FORM_CHANGE_ITEM_MAWILITE &&
        !memcmp(&loaded.live.units[0][0],&expected.attributes,sizeof expected.attributes),"resumed Mega attributes and ownership");
    printf("football live Mega: %u checks, %u failures\n",checks,failed);return failed?1:0;
}
