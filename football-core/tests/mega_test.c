#include "football_mode.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static gc_run roster;
static FootballMode mode;
static const pr_form *base(unsigned species)
{
    for (unsigned i=0;i<PR_FORM_COUNT;++i)
        if (pr_forms[i].species==species && !pr_forms[i].form_index) return &pr_forms[i];
    return NULL;
}
static void setup(void)
{
    assert(football_mode_new(&mode,&roster,NULL,123));
    gc_starter team[2]={{.species=SPECIES_ID_MAWILE,.tera_type=-1},{.species=SPECIES_ID_BULBASAUR,.tera_type=-1}};
    assert(football_mode_draft(&mode,team,2,NULL));
    assert(mode.season.mega_slot==-1);
}
int main(void)
{
    setup();
    assert(!football_season_select_mega(&mode.season,0));
    assert(!gc_football_give_mega(&roster,0,FORM_CHANGE_ITEM_CHARIZARDITE_X));
    int stone=gc_football_mega_item(&roster,0);assert(stone>=0);
    /* Exercise the real purchase path, not a fabricated active-form flag. */
    mode.season.phase=FB_SEASON_SHOP;mode.season.wallet=1000;mode.season.shop_count=1;
    mode.season.shop[0]=(FootballShopOffer){FB_ITEM_MEGA_STONE,1000,(int16_t)stone,0,false};
    assert(football_season_buy(&mode.season,0,0));assert(mode.season.wallet==0);
    assert(!football_season_buy(&mode.season,0,0));
    assert(!football_season_select_mega(&mode.season,0));
    assert(football_season_leave_shop(&mode.season));
    assert(gc_football_mega_item(&roster,0)==-1); /* No duplicate shop offer. */
    const pr_form *mega=gc_football_owned_mega(&roster,0);assert(mega && !strcmp(mega->form_key,"mega"));
    football_mode_input(&mode,(FootballCallInput){0},1);assert(mode.season.mega_slot==0);
    football_mode_input(&mode,(FootballCallInput){0},1);assert(mode.season.mega_slot==-1);
    football_mode_input(&mode,(FootballCallInput){.touch_down=true,.touch_x=20,.touch_y=140},0);
    assert(mode.season.mega_slot==0);
    assert(!football_season_select_mega(&mode.season,1));
    mode.season.speed_boost[0]=4;mode.season.strength_boost[0]=6;
    FootballPlayer expected;
    assert(football_stats_generate(&expected,roster.party[0].b.id,mega,&mode.season.maxima));
    FootballPlayer regular=mode.season.players[0];
    uint32_t rng=mode.season.rng;unsigned items=roster.n_items;
    football_mode_input(&mode,(FootballCallInput){.pressed=FB_INPUT_ACCEPT},0);
    assert(mode.phase==FB_MODE_PLAY && mode.session.match.teams[0].players[0].form_index==mega->form_index);
    FootballPlayer actual=mode.session.match.teams[0].players[0];
    assert(actual.attributes.spd==(expected.attributes.spd+4>99 ? 99 : expected.attributes.spd+4));
    assert(actual.attributes.str==(expected.attributes.str+6>99 ? 99 : expected.attributes.str+6));
    assert(roster.party[0].b.form==0 && roster.n_items==(int)items && mode.season.rng!=rng);
    assert(!memcmp(&regular,&mode.season.players[0],sizeof regular));
    assert(!football_season_select_mega(&mode.season,-1)); /* No mid-snap changes. */
    assert(football_season_finish_match(&mode.season,false,NULL));assert(mode.season.mega_slot==-1);
    assert(roster.party[0].b.form==0 && gc_football_owned_mega(&roster,0)==mega);
    /* Alternate stone identity resolves the actual target, never guessed form 1. */
    for (unsigned y=0;y<2;++y) {
        setup();roster.party[0].b.species=SPECIES_ID_CHARIZARD;roster.n_items=0;
        int item=y ? FORM_CHANGE_ITEM_CHARIZARDITE_Y : FORM_CHANGE_ITEM_CHARIZARDITE_X;
        assert(gc_football_give_mega(&roster,0,item));
        mega=gc_football_owned_mega(&roster,0);assert(mega);
        assert(!strcmp(mega->form_key,y ? "mega-y" : "mega-x"));
        roster.party[0].b.form=mega->form_index;
        assert(!gc_football_owned_mega(&roster,0)); /* Already transformed cannot retrigger. */
    }
    /* Walk every generated species to catch missing reverse-key mappings. */
    unsigned supported=0;
    setup();
    for (unsigned i=0;i<PR_SPECIES_COUNT;++i) {
        unsigned species=pr_species_table[i].id;
        if (!base(species)) continue;
        roster.party[0].b.species=(uint16_t)species;roster.party[0].b.form=0;roster.n_items=0;
        stone=gc_football_mega_item(&roster,0);
        if (stone<0) continue;
        assert(gc_football_give_mega(&roster,0,stone));
        mega=gc_football_owned_mega(&roster,0);
        assert(mega && mega->species==species && !strncmp(mega->form_key,"mega",4));++supported;
    }
    assert(supported>=40);
    puts("football mega: purchase, explicit selection/none, match-only stats, vitamin boosts, X/Y keys and catalog coverage passed");
}
