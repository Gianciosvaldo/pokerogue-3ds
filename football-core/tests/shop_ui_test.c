#include "football_shop_ui.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static gc_run roster;
static FootballSeason season;
static void enter_shop(unsigned species)
{
    assert(football_season_new(&season,&roster,NULL,"shop",1));
    gc_starter team={.species=(uint16_t)species,.tera_type=-1};
    assert(football_season_draft(&season,&team,1,NULL));
    FootballMatchSpec match;
    assert(football_season_start_match(&season,&match));
    assert(football_season_finish_match(&season,true,NULL));
    for (unsigned guard=0;guard<1000 && season.phase==FB_SEASON_EXP;++guard) {
        gc_event e; while (gc_run_next_event(&roster,&e)) {}
        gc_football_progress progress=football_season_tick(&season);
        assert(progress!=GC_FB_ERROR);
        if (progress==GC_FB_WAIT) {
            gc_choice choice={.kind=roster.screen.kind==GC_SCREEN_EVOLVE ? GC_CH_CANCEL : GC_CH_FORGET,.a=-1};
            assert(football_season_choose(&season,&choice,0,NULL));
        }
    }
    assert(season.phase==FB_SEASON_SHOP);
}
static void tap(FootballShopMenu *menu,unsigned x,unsigned y)
{
    football_shop_update(menu,&season,(FootballCallInput){.touch_down=true,
        .touch_x=(uint16_t)x,.touch_y=(uint16_t)y},0);
}
int main(void)
{
    enter_shop(SPECIES_ID_BULBASAUR);
    FootballShopMenu menu={0};
    uint32_t wallet=season.wallet;
    tap(&menu,20,40); assert(menu.party==0 && season.wallet==wallet);
    uint8_t speed=season.players[0].attributes.spd;
    tap(&menu,20,70);
    assert(menu.last_action==FB_SHOP_BOUGHT && season.shop[0].sold);
    assert(season.wallet==wallet-season.shop[0].price);
    assert(season.players[0].attributes.spd==speed+2);
    wallet=season.wallet;
    tap(&menu,20,70);
    assert(menu.last_action==FB_SHOP_REFUSED && season.wallet==wallet);
    season.wallet=0;
    tap(&menu,20,110);
    assert(menu.last_action==FB_SHOP_REFUSED && !season.shop[1].sold && season.wallet==0);
    tap(&menu,320,70); assert(!season.shop[1].sold);
    football_shop_update(&menu,&season,(FootballCallInput){0},-1);
    assert(menu.party==0);
    tap(&menu,20,195);
    assert(menu.last_action==FB_SHOP_LEFT && season.phase==FB_SEASON_READY);
    tap(&menu,20,195); assert(season.round==2); /* Hidden shop cannot act again. */

    enter_shop(SPECIES_ID_MEWTWO);
    int stone=gc_football_mega_item(&roster,0);
    assert(stone>=0);
    season.shop_count=3;
    season.shop[2]=(FootballShopOffer){FB_ITEM_MEGA_STONE,1000,(int16_t)stone,0,false};
    memset(&menu,0,sizeof menu);
    season.wallet=999;
    tap(&menu,20,150);
    assert(menu.last_action==FB_SHOP_REFUSED && roster.n_items==0 && season.wallet==999);
    season.wallet=2000;
    tap(&menu,20,150);
    assert(menu.last_action==FB_SHOP_BOUGHT && roster.n_items==1 && season.wallet==1000);
    assert(!strcmp(roster.items[0].key,"FORM_CHANGE_ITEM") && roster.items[0].arg==stone);
    assert(roster.items[0].pokemon_id==roster.party[0].b.id);
    tap(&menu,20,150);
    assert(menu.last_action==FB_SHOP_REFUSED && roster.n_items==1 && season.wallet==1000);
    football_shop_update(&menu,&season,(FootballCallInput){.pressed=FB_INPUT_BACK},0);
    assert(season.phase==FB_SEASON_READY);
    puts("football shop: touch purchases, affordability, sold stock, rare inventory and leaving passed");
}
