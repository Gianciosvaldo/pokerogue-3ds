#include "playoff_season.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* These structures are too large for the 3DS main-thread stack. */
static gc_run roster;
static FootballSeason season;
static void drain(void) { gc_event ev; while (gc_run_next_event(&roster,&ev)) {} }
static bool finish_exp(bool shedinja)
{
    bool selected=false;
    for (unsigned guard=0;guard<10000;++guard) {
        drain();
        gc_football_progress p=football_season_tick(&season);
        assert(p!=GC_FB_ERROR);
        if (p==GC_FB_DONE) return selected;
        if (p!=GC_FB_WAIT) continue;
        gc_choice c={0}; unsigned choice=0;
        if (roster.screen.kind==GC_SCREEN_LEARN_MOVE) { c.kind=GC_CH_FORGET; c.a=-1; }
        else {
            assert(roster.screen.kind==GC_SCREEN_EVOLVE);
            uint16_t options[GC_FOOTBALL_MAX_EVOLUTIONS];
            int n=gc_football_evolution_choices(&roster,options);
            assert(n>0);
            for (int i=0;i<n;++i) if (shedinja && options[i]==SPECIES_ID_SHEDINJA) {
                choice=(unsigned)i; selected=true;
            }
            c.kind=GC_CH_EVOLVE;
            assert(!football_season_choose(&season,&c,GC_FOOTBALL_MAX_EVOLUTIONS,NULL));
        }
        assert(football_season_choose(&season,&c,choice,NULL));
    }
    assert(!"EXP did not complete");
    return false;
}
static gc_starter starter(unsigned species)
{
    gc_starter s={0}; s.species=(uint16_t)species; s.tera_type=-1;
    return s;
}
int main(void)
{
    const char *why=NULL;
    assert(football_season_new(&season,&roster,NULL,"football",123));
    FootballMatchSpec match;
    assert(!football_season_start_match(&season,&match));
    gc_starter team[2]={starter(SPECIES_ID_NINCADA),starter(SPECIES_ID_BULBASAUR)};
    gc_starter bad=starter(SPECIES_ID_IVYSAUR);
    assert(!football_season_draft(&season,&bad,1,&why));
    bad=team[0]; bad.form=1;
    assert(!football_season_draft(&season,&bad,1,&why));
    gc_starter duplicates[2]={team[0],team[0]};
    assert(!football_season_draft(&season,duplicates,2,&why));
    gc_starter pricey[2]={starter(SPECIES_ID_MEWTWO),starter(SPECIES_ID_ARCEUS)};
    assert(!football_season_draft(&season,pricey,2,&why));
    assert(roster.n_party==0);
    assert(football_season_draft(&season,team,2,&why));
    assert(!football_season_draft(&season,team,2,&why));
    bool got_shedinja=false;
    for (unsigned round=1;round<=7;++round) {
        assert(football_season_start_match(&season,&match));
        assert(match.round==round && match.difficulty_q8==256+64*(round-1));
        assert(!football_season_start_match(&season,&match));
        int old_level=roster.party[0].b.level;
        assert(football_season_finish_match(&season,true,&why));
        assert(!football_season_finish_match(&season,true,&why));
        assert(!football_season_leave_shop(&season));
        got_shedinja|=finish_exp(true);
        assert(roster.party[0].b.level>=old_level);
        assert(season.phase==FB_SEASON_SHOP && season.shop_count>=2);
        assert(!football_season_start_match(&season,&match));
        uint32_t wallet=season.wallet;
        uint8_t speed=season.players[1].attributes.spd;
        assert(!football_season_buy(&season,FB_SHOP_CAP,0));
        assert(!football_season_buy(&season,0,FB_TEAM_CAP));
        assert(football_season_buy(&season,0,1));
        assert(season.players[1].attributes.spd>=speed);
        assert(season.wallet==wallet-season.shop[0].price);
        wallet=season.wallet;
        assert(!football_season_buy(&season,0,1));
        assert(season.wallet==wallet);
        assert(football_season_leave_shop(&season));
    }
    assert(got_shedinja && roster.party[0].b.species==SPECIES_ID_SHEDINJA);
    assert(roster.n_party==2); /* Choice replaces, never silently duplicates. */
    assert(season.phase==FB_SEASON_CLEAR && season.wins==7);
    assert(!football_season_start_match(&season,&match));
    assert(!football_season_leave_shop(&season));
    assert(football_season_new(&season,&roster,NULL,"football",123));
    assert(football_season_draft(&season,team,2,&why));
    assert(football_season_start_match(&season,&match));
    assert(football_season_finish_match(&season,false,&why));
    assert(season.phase==FB_SEASON_ELIMINATED && season.wallet==0);
    puts("football season: draft, seven wins, EXP/evolution choices, shop and elimination passed");
}
