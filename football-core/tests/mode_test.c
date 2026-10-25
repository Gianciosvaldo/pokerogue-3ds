#include "football_mode.h"
#include <assert.h>
#include <stdio.h>
static gc_run roster;
static FootballMode mode;
static void press(unsigned key) { football_mode_input(&mode,(FootballCallInput){.pressed=key},0); }
static void strengthen(bool win)
{
    for(unsigned side=0;side<2;++side) for(unsigned i=0;i<mode.session.match.teams[side].count;++i){
        uint8_t r=(side==FB_HOME)==win ? 99 : 1;
        mode.session.match.teams[side].players[i].attributes=(FootballAttributes){r,r,r,r,r,r,r,r};
    }
}
int main(void)
{
    const unsigned outcomes[]={SPECIES_ID_NINJASK,SPECIES_ID_SHEDINJA,SPECIES_ID_NINCADA};
    for(unsigned scenario=0;scenario<3;++scenario){
        assert(football_mode_new(&mode,&roster,NULL,123));
        gc_starter team[2]={{.species=SPECIES_ID_NINCADA,.tera_type=-1},{.species=SPECIES_ID_BULBASAUR,.tera_type=-1}};
        gc_starter bad={.species=SPECIES_ID_IVYSAUR,.tera_type=-1};
        assert(!gc_football_validate_draft(NULL,&bad,1,NULL));
        assert(!football_mode_draft(&mode,&bad,1,NULL));assert(mode.phase==FB_MODE_DRAFT);
        assert(football_mode_draft(&mode,team,2,NULL));
        unsigned guard=0,rounds=0,seen_evolutions=0,seen_moves=0,messages=0;
        while(mode.phase!=FB_MODE_END && ++guard<30000){
            switch(mode.phase){
            case FB_MODE_ROUND:
                press(FB_INPUT_ACCEPT);assert(mode.phase==FB_MODE_PLAY);strengthen(true);++rounds;break;
            case FB_MODE_PLAY:
                if((mode.session.match.phase==FB_MATCH_CALL)) press(FB_INPUT_ACCEPT);
                break;
            case FB_MODE_RESULT:
                assert(mode.session.match.rules.winner==FB_HOME);
                press(FB_INPUT_ACCEPT);break;
            case FB_MODE_MESSAGE:
                ++messages;assert(mode.message[0]);press(FB_INPUT_ACCEPT);break;
            case FB_MODE_EVOLVE: {
                ++seen_evolutions;
                if(scenario==2){press(FB_INPUT_BACK);break;}
                unsigned picked=0;
                for(unsigned i=0;i<mode.evolution.count;++i)
                    if(mode.evolution.species[i]==outcomes[scenario])picked=i;
                while(mode.evolution.selected!=picked)press(FB_INPUT_DOWN);
                press(FB_INPUT_ACCEPT);break;
            }
            case FB_MODE_LEARN:
                ++seen_moves;
                assert(football_mode_move_count(&mode)>=2 && football_mode_new_move_name(&mode)[0]);
                assert(football_mode_move_name(&mode,0)[0]);press(FB_INPUT_BACK);break;
            case FB_MODE_SHOP: {
                uint32_t wallet=mode.season.wallet;
                press(FB_INPUT_ACCEPT);assert(mode.season.shop[0].sold);
                press(FB_INPUT_ACCEPT);assert(mode.shop.last_action==FB_SHOP_REFUSED);
                assert(mode.season.wallet==wallet-mode.season.shop[0].price);
                press(FB_INPUT_BACK);break;
            }
            default:break;
            }
            football_mode_tick(&mode,250);
            assert(mode.phase!=FB_MODE_ERROR);
        }
        assert(guard<30000 && rounds==7 && mode.season.phase==FB_SEASON_CLEAR);
        assert(mode.profile_dirty && messages>0 && seen_evolutions>0 && seen_moves>0);
        assert(roster.party[0].b.species==outcomes[scenario]);
        assert(!mode.done);press(FB_INPUT_ACCEPT);assert(mode.done);
    }
    assert(football_mode_new(&mode,&roster,NULL,123));
    gc_starter team={.species=SPECIES_ID_BULBASAUR,.tera_type=-1};
    assert(football_mode_draft(&mode,&team,1,NULL));press(FB_INPUT_ACCEPT);strengthen(false);
    unsigned guard=0;
    while(mode.phase==FB_MODE_PLAY && ++guard<10000){
        if((mode.session.match.phase==FB_MATCH_CALL))press(FB_INPUT_ACCEPT);
        football_mode_tick(&mode,250);
    }
    assert(mode.phase==FB_MODE_RESULT && mode.season.phase==FB_SEASON_ELIMINATED);
    press(FB_INPUT_ACCEPT);assert(mode.phase==FB_MODE_END);press(FB_INPUT_BACK);assert(mode.done);
    puts("football mode: complete seven-round flows, both Nincada evolutions, cancellation, move prompts, shops and elimination passed");
}
