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
    /* With two stone offers per round the Mawile one may sit in either slot: find it. */
#define MAWILE_OFFER (c.stone_target==0?2u:3u)
    while(!((c.stone_target==0 && c.wallet>=c.prices[2]) || (c.stone2_target==0 && c.wallet>=c.prices[3])) && c.round<6){
        CHECK(football_career_continue(&c),"leave shop");CHECK(football_career_start(&c),"next match");win();
    }
    const unsigned offer=MAWILE_OFFER;
    CHECK(football_career_stone_target(&c,offer)==0 && football_career_stone_item(&c,offer)==FORM_CHANGE_ITEM_MAWILITE,"actual applicable stone in shop");
    unsigned wallet=c.wallet;
    int *item_slot=offer==2?&c.stone_item:&c.stone2_item;
    int item=*item_slot;*item_slot=FORM_CHANGE_ITEM_VENUSAURITE;
    CHECK(!football_career_buy(&c,offer,0) && c.wallet==wallet && !c.roster.mega_item[0],"wrong-species stone cannot charge or equip");*item_slot=item;
    CHECK(football_career_buy(&c,offer,0),"buy actual generated stone");
    CHECK(c.wallet+c.prices[offer]==wallet && c.roster.mega_item[0]==FORM_CHANGE_ITEM_MAWILITE,"pay once and retain ownership");
    CHECK(!football_career_buy(&c,offer,0),"no duplicate purchase");
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
    /* Two stone offers a round, each for a different holder; with more holders than offers the stones cycle from round to round. */
    {
        CHECK(football_career_new(&c,&scratch,&profile,11),"second career");
        gc_starter st={.species=SPECIES_ID_MAWILE,.tera_type=-1};
        CHECK(football_roster_add(&c.roster,&scratch,&st,&profile,NULL),"Mawile again");
        for(int i=0;i<gc_starter_cost_count && c.roster.count<22;++i){
            if(gc_starter_costs[i].cost>1)continue;
            st.species=gc_starter_costs[i].id;football_roster_add(&c.roster,&scratch,&st,&profile,NULL);
        }
        static const int holders[3]={3,9,65}; /* Venusaur, Blastoise, Alakazam stand in for members 1-3 */
        for(unsigned i=0;i<3;++i){c.roster.mon[1+i].b.species=holders[i];c.roster.mon[1+i].b.form=0;CHECK(football_roster_refresh(&c.roster,1+i),"holder %u",i);}
        c.phase=FB_CAREER_READY;
        unsigned able=0;
        for(unsigned i=0;i<c.roster.count;++i){gc_football_new(&other,NULL,"count-able",1);other.party[0]=c.roster.mon[i];other.n_party=1;if(gc_football_mega_item(&other,0)>0)++able;}
        CHECK(able>=3,"enough holders to cycle (%u)",able);
        unsigned seen[22]={0},two=0;int last_a=-1;
        for(unsigned round=1;round<=5;++round){
            CHECK(football_career_start(&c),"match %u",round);win();
            const int a=football_career_stone_target(&c,2),b=football_career_stone_target(&c,3);
            CHECK(a>=0 && b>=0 && a!=b,"two different holders in round %u (%d %d)",round,a,b);
            if(a>=0 && b>=0){++seen[a];++seen[b];++two;
                CHECK(football_career_stone_item(&c,2)>0 && football_career_stone_item(&c,3)>0,"both stones are real");
                /* a saved checkpoint keeps both offers */
                size_t k=football_career_save(&c,bytes,sizeof bytes);
                CHECK(k && football_career_load(&loaded,&other,&profile,bytes,k) && football_career_stone_target(&loaded,2)==a && football_career_stone_target(&loaded,3)==b &&
                    football_career_stone_item(&loaded,3)==football_career_stone_item(&c,3),"both offers survive a save (round %u, saved %zu, load %d)",round,k,(int)(k&&football_career_load(&loaded,&other,&profile,bytes,k)));
                /* buying the second leaves the first on the shelf, and the holder owns it */
                if(round==5){
                    if(c.wallet<c.prices[3])c.wallet=c.prices[3]; /* stay within what a career can earn */
                    const unsigned before=c.wallet;
                    CHECK(football_career_buy(&c,3,0) && c.wallet+1000==before && c.sold[3] && !c.sold[2] && c.roster.mega_item[b],"buy the second stone");
                    CHECK(!football_career_buy(&c,3,0),"no second purchase");
                }
            }
            if(round>1)CHECK(a!=last_a || able<=2,"the pair moves on from round to round (round %u)",round);
            last_a=a;
            CHECK(football_career_continue(&c),"leave shop");
            /* a member who already holds a stone is not offered another */
            for(unsigned i=0;i<c.roster.count;++i)if(c.roster.mega_item[i]){CHECK(football_career_stone_target(&c,2)!=(int)i,"owner not offered again");}
        }
        unsigned covered=0;for(unsigned i=0;i<22;++i)if(seen[i])++covered;
        CHECK(two==5 && covered>=3,"the stones reach several holders over the rounds (%u holders)",covered);
    }
    printf("football live Mega: %u checks, %u failures\n",checks,failed);return failed?1:0;
}
