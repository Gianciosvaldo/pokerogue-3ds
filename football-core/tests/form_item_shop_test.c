/* Kyurem's and Calyrex's form items in the live football shop (lorenzo 2026-11-15): Dark or Light Stone and Icy or Shadow Reins of Unity sell
 * at a Mega Stone's price, only for a member that is that species and whose dex condition holds (Zekrom, Reshiram, Glastrier or Spectrier
 * caught), always while they are unbought; the holder takes the form for one match like a Mega. A real career, real generated shops. */
#include "football_career.h"
#include "gc/data.h"
#include "football_profile_fixture.h"
#include <stdio.h>
#include <string.h>
static FootballCareer c,loaded;
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
/* a full legal roster: the named species first, then the cheap ones (none of them takes a Mega Stone or a form item) */
static void roster(unsigned seed,const int *first,int n_first)
{
    CHECK(football_career_new(&c,&scratch,&profile,seed),"new career");
    for(int i=0;i<n_first;++i){gc_starter s={.species=(uint16_t)first[i],.tera_type=-1};CHECK(football_roster_add(&c.roster,&scratch,&s,&profile,NULL),"draft %d",first[i]);}
    for(int i=0;i<gc_starter_cost_count && c.roster.count<22;++i){
        if(gc_starter_costs[i].cost>1)continue;
        gc_starter s={.species=gc_starter_costs[i].id,.tera_type=-1};
        football_roster_add(&c.roster,&scratch,&s,&profile,NULL);
    }
    CHECK(c.roster.count==22,"legal full roster (%u)",c.roster.count);c.phase=FB_CAREER_READY;
}
static void first_shop(void){CHECK(football_career_start(&c),"first match");win();}
static bool form_pair(int item,int a,int b){return item==a || item==b;}
int main(void)
{
    /* ---- the rules of the helpers ---- */
    football_test_unlock_all(&profile);
    {
        gc_football_new(&other,&profile,"form-items",1);
        gc_starter k={.species=SPECIES_ID_KYUREM,.tera_type=-1};
        CHECK(gc_football_create_member(&other,&k,NULL),"Kyurem member");
        int list[GC_FOOTBALL_MAX_MEGA_ITEMS];
        CHECK(gc_football_form_items(&other,0,list,GC_FOOTBALL_MAX_MEGA_ITEMS)==2 &&
              form_pair(list[0],FORM_CHANGE_ITEM_DARK_STONE,FORM_CHANGE_ITEM_LIGHT_STONE) && form_pair(list[1],FORM_CHANGE_ITEM_DARK_STONE,FORM_CHANGE_ITEM_LIGHT_STONE) && list[0]!=list[1],
              "Zekrom and Reshiram caught: both Kyurem stones");
        CHECK(gc_football_mega_items(&other,0,list,GC_FOOTBALL_MAX_MEGA_ITEMS)==0 && gc_football_mega_item(&other,0)==-1,"never among the Mega Stones");
        CHECK(gc_football_form_items(&other,0,list,1)==1 && !gc_football_form_items(&other,0,list,0) && !gc_football_form_items(&other,0,NULL,2) && !gc_football_form_items(NULL,0,list,2),"room and null guards");
        const pr_form *black=gc_football_mega_form(&other,0,FORM_CHANGE_ITEM_DARK_STONE),*white=gc_football_mega_form(&other,0,FORM_CHANGE_ITEM_LIGHT_STONE);
        CHECK(black && !strcmp(black->form_key,"black") && white && !strcmp(white->form_key,"white"),"Black and White Kyurem");
        CHECK(!gc_football_mega_form(&other,0,FORM_CHANGE_ITEM_ICY_REINS_OF_UNITY) && !gc_football_mega_form(&other,0,FORM_CHANGE_ITEM_MAWILITE),"a Calyrex item or a stone does not fit Kyurem");
        profile.dex[SPECIES_ID_RESHIRAM].caught_attr=0;
        CHECK(gc_football_form_items(&other,0,list,GC_FOOTBALL_MAX_MEGA_ITEMS)==1 && list[0]==FORM_CHANGE_ITEM_DARK_STONE,"only Zekrom caught: the Dark Stone only");
        profile.dex[SPECIES_ID_ZEKROM].caught_attr=0;
        CHECK(!gc_football_form_items(&other,0,list,GC_FOOTBALL_MAX_MEGA_ITEMS),"neither caught: nothing to sell");
        CHECK(gc_football_mega_form(&other,0,FORM_CHANGE_ITEM_DARK_STONE)!=NULL,"a form item already held keeps working whatever the dex says");
        football_test_unlock_all(&profile);
        gc_football_new(&other,&profile,"form-items",1);
        gc_starter cal={.species=SPECIES_ID_CALYREX,.tera_type=-1};
        CHECK(gc_football_create_member(&other,&cal,NULL),"Calyrex member");
        CHECK(gc_football_form_items(&other,0,list,GC_FOOTBALL_MAX_MEGA_ITEMS)==2,"Glastrier and Spectrier caught: both Calyrex items");
        const pr_form *ice=gc_football_mega_form(&other,0,FORM_CHANGE_ITEM_ICY_REINS_OF_UNITY),*shadow=gc_football_mega_form(&other,0,FORM_CHANGE_ITEM_SHADOW_REINS_OF_UNITY);
        CHECK(ice && !strcmp(ice->form_key,"ice") && shadow && !strcmp(shadow->form_key,"shadow"),"Ice and Shadow Rider");
        CHECK(gc_football_give_mega(&other,0,FORM_CHANGE_ITEM_ICY_REINS_OF_UNITY) && gc_football_owned_mega(&other,0)==ice && !gc_football_form_items(&other,0,list,GC_FOOTBALL_MAX_MEGA_ITEMS),"holding one, no second is offered");
        profile.dex[SPECIES_ID_GLASTRIER].caught_attr=0;
        gc_football_new(&other,&profile,"form-items",1);
        CHECK(gc_football_create_member(&other,&cal,NULL) && gc_football_form_items(&other,0,list,GC_FOOTBALL_MAX_MEGA_ITEMS)==1 && list[0]==FORM_CHANGE_ITEM_SHADOW_REINS_OF_UNITY,"only Spectrier caught: the Shadow Reins only");
        football_test_unlock_all(&profile);
        gc_football_new(&other,NULL,"no-profile",1);
        CHECK(gc_football_create_member(&other,&cal,NULL)==false || !gc_football_form_items(&other,0,list,GC_FOOTBALL_MAX_MEGA_ITEMS),"no profile, no dex, nothing sold");
    }
    /* ---- the shop: Kyurem and Calyrex on one roster ---- */
    {
        static const int both[2]={SPECIES_ID_KYUREM,SPECIES_ID_CALYREX};
        roster(21,both,2);first_shop();
        for(unsigned round=1;round<=4;++round){
            const int a=football_career_stone_target(&c,2),b=football_career_stone_target(&c,3);
            CHECK(a>=0 && b>=0 && a!=b && ((a==0 && b==1) || (a==1 && b==0)),"round %u: both are on the shelf (%d %d)",round,a,b);
            const unsigned kyurem=a==0?2u:3u,calyrex=a==0?3u:2u;
            CHECK(form_pair(football_career_stone_item(&c,kyurem),FORM_CHANGE_ITEM_DARK_STONE,FORM_CHANGE_ITEM_LIGHT_STONE),"Kyurem is offered one of its stones (%d)",football_career_stone_item(&c,kyurem));
            CHECK(form_pair(football_career_stone_item(&c,calyrex),FORM_CHANGE_ITEM_ICY_REINS_OF_UNITY,FORM_CHANGE_ITEM_SHADOW_REINS_OF_UNITY),"Calyrex is offered its reins (%d)",football_career_stone_item(&c,calyrex));
            CHECK(c.prices[2]==1000 && c.prices[3]==1000,"the Mega Stone price");
            if(round==2){
                /* too little cash buys nothing; a wrong holder cannot take the item either */
                unsigned wallet=c.wallet;c.wallet=999;
                CHECK(!football_career_buy(&c,kyurem,0) && c.wallet==999 && !c.roster.mega_item[0],"999 buys nothing");
                c.wallet=wallet;
                int *slot=kyurem==2?&c.stone_item:&c.stone2_item;int keep=*slot;*slot=FORM_CHANGE_ITEM_ICY_REINS_OF_UNITY;
                CHECK(!football_career_buy(&c,kyurem,0) && !c.roster.mega_item[0],"Calyrex's item does not fit Kyurem");*slot=keep;
                /* buy Kyurem's */
                c.wallet=1500;
                CHECK(football_career_buy(&c,kyurem,0) && c.wallet==500 && c.sold[kyurem] && !c.sold[calyrex] && form_pair(c.roster.mega_item[0],FORM_CHANGE_ITEM_DARK_STONE,FORM_CHANGE_ITEM_LIGHT_STONE),"Kyurem buys its stone for 1000");
                CHECK(!football_career_buy(&c,kyurem,0),"no second purchase");
                CHECK(football_career_buy(&c,calyrex,1)==false,"500 cash does not reach the reins");
                c.wallet=1000;
                CHECK(football_career_buy(&c,calyrex,1) && !c.wallet && form_pair(c.roster.mega_item[1],FORM_CHANGE_ITEM_ICY_REINS_OF_UNITY,FORM_CHANGE_ITEM_SHADOW_REINS_OF_UNITY),"Calyrex buys its reins for 1000");
                /* a saved shop keeps both offers and the purchases */
                size_t n=football_career_save(&c,bytes,sizeof bytes);
                CHECK(n && football_career_load(&loaded,&other,&profile,bytes,n) && loaded.roster.mega_item[0]==c.roster.mega_item[0] && loaded.roster.mega_item[1]==c.roster.mega_item[1],"owned items survive a save");
                break;
            }
            CHECK(football_career_continue(&c),"leave shop");CHECK(football_career_start(&c),"next match");win();
        }
        /* both owned: no form offer for either, and a later shop does not offer one again */
        CHECK(football_career_continue(&c),"leave shop");CHECK(football_career_start(&c),"next match");win();
        CHECK(football_career_stone_target(&c,2)>1 && !gc_football_is_form_item(football_career_stone_item(&c,2)) &&
              football_career_stone_target(&c,3)!=0 && football_career_stone_target(&c,3)!=1 && !gc_football_is_form_item(football_career_stone_item(&c,3)),
              "nothing left to offer them (%d %d)",football_career_stone_target(&c,2),football_career_stone_target(&c,3));
        /* select Kyurem for the match: the live unit takes the form's stats, the roster stays in its own form */
        CHECK(football_career_continue(&c),"leave shop");c.mega=0;
        gc_football_new(&other,&profile,"expected",1);other.party[0]=c.roster.mon[0];other.n_party=1;
        const pr_form *form=gc_football_mega_form(&other,0,c.roster.mega_item[0]);
        FootballPlayer expected;
        CHECK(form && football_stats_generate(&expected,c.roster.mon[0].b.id,form,&c.roster.maxima),"the form's stats");
        CHECK(form && c.roster.mon[0].b.form==0 && form->form_index>0,"the roster member is in its regular form");
        CHECK(football_career_start(&c),"start with Kyurem selected");
        const FootballAttributes base=c.roster.player[0].attributes;
        FootballAttributes want=expected.attributes;
        unsigned spd=want.spd+c.roster.speed[0],str=want.str+c.roster.strength[0];want.spd=(uint8_t)(spd>99?99:spd);want.str=(uint8_t)(str>99?99:str);
        CHECK(!memcmp(&c.live.units[FB_HOME][0],&want,sizeof want),"the live unit has the form's attributes");
        CHECK(memcmp(&want,&base,sizeof want)!=0,"and they differ from the regular form's");
        size_t n=football_career_save(&c,bytes,sizeof bytes);
        profile.dex[SPECIES_ID_ZEKROM].caught_attr=0;profile.dex[SPECIES_ID_RESHIRAM].caught_attr=0;   /* the dex only gates the shop */
        CHECK(n && football_career_load(&loaded,&other,&profile,bytes,n) && loaded.mega==0 && loaded.roster.mega_item[0]==c.roster.mega_item[0] &&
              !memcmp(&loaded.live.units[FB_HOME][0],&want,sizeof want),"a resumed match keeps the form even when the dex has changed");
        football_test_unlock_all(&profile);
    }
    /* ---- the dex gates the shop ---- */
    {
        static const int kyurem[1]={SPECIES_ID_KYUREM};
        profile.dex[SPECIES_ID_ZEKROM].caught_attr=0;profile.dex[SPECIES_ID_RESHIRAM].caught_attr=0;
        roster(22,kyurem,1);first_shop();
        CHECK(football_career_stone_target(&c,2)!=0 && football_career_stone_target(&c,3)!=0,"Kyurem without Zekrom or Reshiram caught is offered nothing");
        football_test_unlock_all(&profile);
        profile.dex[SPECIES_ID_RESHIRAM].caught_attr=0;
        roster(23,kyurem,1);first_shop();
        for(unsigned round=1;round<=4;++round){
            CHECK(football_career_stone_target(&c,2)==0 && football_career_stone_item(&c,2)==FORM_CHANGE_ITEM_DARK_STONE,"round %u: Zekrom only, the Dark Stone every round (%d)",round,football_career_stone_item(&c,2));
            CHECK(football_career_continue(&c),"leave shop");CHECK(football_career_start(&c),"next match");win();
        }
        football_test_unlock_all(&profile);
    }
    /* ---- both caught: one stone a round for the same Kyurem, and over the rounds both ---- */
    {
        static const int kyurem[1]={SPECIES_ID_KYUREM};
        roster(24,kyurem,1);first_shop();
        unsigned dark=0,light=0;
        for(unsigned round=1;round<=8;++round){
            CHECK(football_career_stone_target(&c,2)==0 && football_career_stone_target(&c,3)!=0,"round %u: one offer for Kyurem",round);
            const int item=football_career_stone_item(&c,2);dark+=item==FORM_CHANGE_ITEM_DARK_STONE;light+=item==FORM_CHANGE_ITEM_LIGHT_STONE;
            CHECK(football_career_continue(&c),"leave shop");CHECK(football_career_start(&c),"next match");win();
        }
        CHECK(dark>0 && light>0,"both stones come up over the rounds (%u, %u)",dark,light);
    }
    /* ---- Kyurem takes the first slot and the Mega Stones keep going through the second ---- */
    {
        static const int mixed[2]={SPECIES_ID_MAWILE,SPECIES_ID_KYUREM};
        roster(25,mixed,2);
        static const int holders[3]={3,9,65};
        for(unsigned i=0;i<3;++i){c.roster.mon[2+i].b.species=holders[i];c.roster.mon[2+i].b.form=0;CHECK(football_roster_refresh(&c.roster,2+i),"holder %u",i);}
        first_shop();
        unsigned seen[22]={0};
        for(unsigned round=1;round<=5;++round){
            const int a=football_career_stone_target(&c,2),b=football_career_stone_target(&c,3);
            CHECK(a==1 && gc_football_is_form_item(football_career_stone_item(&c,2)),"round %u: Kyurem's item is in the first slot (%d)",round,a);
            CHECK(b>=0 && b!=1 && !gc_football_is_form_item(football_career_stone_item(&c,3)),"round %u: a Mega Stone is in the second (%d)",round,b);
            if(b>=0)++seen[b];
            CHECK(football_career_stone_item(&c,2)>0 && c.prices[2]==1000 && c.prices[3]==1000,"the same price");
            if(round==5){
                c.wallet=1000;
                CHECK(football_career_buy(&c,2,0) && c.roster.mega_item[1],"buy Kyurem's");
            }
            CHECK(football_career_continue(&c),"leave shop");CHECK(football_career_start(&c),"next match");win();
        }
        unsigned holders_seen=0;for(unsigned i=0;i<22;++i)if(seen[i])++holders_seen;
        CHECK(holders_seen>=3,"the Mega Stones still reach several holders (%u)",holders_seen);
        /* Kyurem bought its item: both slots are Mega Stones again */
        CHECK(football_career_stone_target(&c,2)>=0 && football_career_stone_target(&c,3)>=0 && football_career_stone_target(&c,2)!=1 && football_career_stone_target(&c,3)!=1 &&
              !gc_football_is_form_item(football_career_stone_item(&c,2)) && !gc_football_is_form_item(football_career_stone_item(&c,3)),"after the purchase the Mega Stones fill both slots");
    }
    printf("football form items: %u checks, %u failures\n",checks,failed);return failed?1:0;
}
