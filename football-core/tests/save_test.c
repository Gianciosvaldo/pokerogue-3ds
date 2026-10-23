#include "football_save.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static gc_run run,loaded;
static FootballMode mode,restored;
static unsigned char bytes[FB_SAVE_MAX],again[FB_SAVE_MAX];
static uint32_t crc32(const unsigned char *p,size_t n)
{
    uint32_t crc=UINT32_MAX;for(size_t i=0;i<n;++i){crc^=p[i];for(unsigned bit=0;bit<8;++bit)crc=(crc>>1)^(0xedb88320u & (0u-(crc&1)));}return ~crc;
}
int main(void)
{
    assert(football_mode_new(&mode,&run,NULL,123));
    gc_starter team={.species=SPECIES_ID_MAWILE,.tera_type=-1};
    assert(football_mode_draft(&mode,&team,1,NULL));
    assert(football_save_ready(&mode));
    size_t n=football_save_encode(&mode,bytes,sizeof bytes);assert(n);
    assert(football_save_decode(&restored,&loaded,NULL,bytes,n));
    assert(restored.phase==FB_MODE_ROUND && loaded.party[0].b.id==run.party[0].b.id);
    assert(loaded.party[0].b.level==run.party[0].b.level);
    assert(!gc_run_can_save(&loaded)); /* No relaxed Classic save gate. */
    for(size_t i=0;i<n;++i) {
        unsigned char old=bytes[i];bytes[i]^=128;
        assert(!football_save_decode(&restored,&loaded,NULL,bytes,n));bytes[i]=old;
    }
    for(size_t i=0;i<n;i+=17)assert(!football_save_decode(&restored,&loaded,NULL,bytes,i));
    football_mode_input(&mode,(FootballCallInput){.pressed=FB_INPUT_ACCEPT},0);
    assert(football_match_contact_choice(&mode.session.match,FB_CONTACT_JUKE,true));
    n=football_save_encode(&mode,bytes,sizeof bytes);assert(n);
    assert(football_save_decode(&restored,&loaded,NULL,bytes,n));
    assert(restored.session.match.home_tactic==FB_CONTACT_JUKE && restored.session.match.home_dive);
    assert(restored.session.owner==&restored.season && restored.session.active);
    /* Version 1 active-match checkpoint migrates to balanced/standard tactics. */
    memcpy(again,bytes,n);uint32_t offset=0;memcpy(&offset,again+12,4);
    memmove(again+offset-8,again+offset,n-offset);offset-=8;
    uint32_t version=1,crc=crc32(again+20,n-8-20);
    memcpy(again+8,&version,4);memcpy(again+12,&offset,4);memcpy(again+16,&crc,4);
    assert(football_save_decode(&restored,&loaded,NULL,again,n-8));
    assert(restored.session.match.home_tactic==FB_CONTACT_BALANCED && !restored.session.match.home_dive);
    assert(football_save_decode(&restored,&loaded,NULL,bytes,n));
    for(unsigned plays=0;plays<12;++plays) {
        if(football_match_waiting_for_home(&mode.session.match)) {
            PlayInfo p={FB_FORM_UNDER_CENTER,FB_PLAY_RUN,FB_RUN_DIVE,255};
            assert(football_match_call(&mode.session.match,p));assert(football_match_call(&restored.session.match,p));
        } else { assert(football_match_defend(&mode.session.match,FB_DEF_MAN));assert(football_match_defend(&restored.session.match,FB_DEF_MAN)); }
        assert(!football_save_encode(&mode,again,sizeof again));
        for(unsigned tick=0;tick<7;++tick) { football_match_tick(&mode.session.match,100);football_match_tick(&restored.session.match,100); }
        assert(mode.session.match.rng==restored.session.match.rng);
        assert(!memcmp(&mode.session.match.rules,&restored.session.match.rules,sizeof(FootballRules)));
        assert(!memcmp(mode.session.match.energy,restored.session.match.energy,sizeof mode.session.match.energy));
        n=football_save_encode(&mode,bytes,sizeof bytes);assert(n);
        assert(football_save_decode(&restored,&loaded,NULL,bytes,n));
    }
    /* Preserve actual post-match EXP, shop stock, held stone and mega selection. */
    assert(football_mode_new(&mode,&run,NULL,123));assert(football_mode_draft(&mode,&team,1,NULL));
    FootballMatchSpec spec;assert(football_season_start_match(&mode.season,&spec));
    assert(football_season_finish_match(&mode.season,true,NULL));mode.phase=FB_MODE_EVENTS;
    for(unsigned guard=0;mode.phase!=FB_MODE_SHOP && guard<2000;++guard) {
        if(mode.phase==FB_MODE_MESSAGE)football_mode_input(&mode,(FootballCallInput){.pressed=FB_INPUT_ACCEPT},0);
        else if(mode.phase==FB_MODE_EVOLVE || mode.phase==FB_MODE_LEARN)football_mode_input(&mode,(FootballCallInput){.pressed=FB_INPUT_BACK},0);
        football_mode_tick(&mode,50);
    }
    assert(mode.phase==FB_MODE_SHOP);int level=run.party[0].b.level;
    assert(football_season_buy(&mode.season,0,0));
    int stone=gc_football_mega_item(&run,0);assert(stone>=0);mode.season.wallet=1000;mode.season.shop_count=3;
    mode.season.shop[2]=(FootballShopOffer){FB_ITEM_MEGA_STONE,1000,(int16_t)stone,0,false};
    assert(football_season_buy(&mode.season,2,0));
    n=football_save_encode(&mode,bytes,sizeof bytes);assert(n);
    assert(football_save_decode(&restored,&loaded,NULL,bytes,n));
    assert(loaded.party[0].b.level==level && restored.season.shop[0].sold && restored.season.shop[2].sold);
    assert(restored.season.speed_boost[0]==2 && gc_football_owned_mega(&loaded,0));
    assert(!football_season_buy(&restored.season,0,0));
    football_mode_input(&restored,(FootballCallInput){.pressed=FB_INPUT_BACK},0);
    assert(football_season_select_mega(&restored.season,0));
    n=football_save_encode(&restored,bytes,sizeof bytes);assert(n);
    assert(football_save_decode(&mode,&run,NULL,bytes,n));assert(mode.season.mega_slot==0);
    football_mode_input(&mode,(FootballCallInput){.pressed=FB_INPUT_ACCEPT},0);
    n=football_save_encode(&mode,bytes,sizeof bytes);assert(n);
    assert(football_save_decode(&restored,&loaded,NULL,bytes,n));
    assert(restored.session.match.teams[0].players[0].form_index>0 && loaded.party[0].b.form==0);
    puts("football save: roster roundtrip, corrupt/truncated rejection and resumed match replay passed");
}
