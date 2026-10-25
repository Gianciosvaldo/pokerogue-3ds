#include "football_live.h"
#include "football_camera.h"
#include "gc/data.h"
#include <assert.h>
#include "football_profile_fixture.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
static FootballRoster roster;
static gc_run scratch;
static FootballLive game,copy;
static gc_system profile,locked_profile;
int main(void)
{
    football_test_unlock_all(&profile);
    assert(football_roster_init(&roster));gc_football_new(&scratch,NULL,"live",42);
    gc_system_init_defaults(&locked_profile);
    gc_starter candidate={.species=SPECIES_ID_RATTATA,.tera_type=-1};
    assert(!football_roster_starter(&locked_profile,candidate.species,&candidate));
    assert(!football_roster_add(&roster,&scratch,&candidate,&locked_profile,NULL) && roster.count==0);
    locked_profile.dex[SPECIES_ID_RATTATA].caught_attr=(UINT64_C(1)<<1)|(UINT64_C(1)<<6)|(UINT64_C(1)<<7)|4;
    locked_profile.dex[SPECIES_ID_RATTATA].nature_attr=1u<<18;
    locked_profile.starters[SPECIES_ID_RATTATA].ability_attr=4;
    assert(football_roster_starter(&locked_profile,SPECIES_ID_RATTATA,&candidate));
    assert(candidate.nature==17 && candidate.ability_index==2 && candidate.shiny && candidate.variant==2);
    /* Draft real cheap base species; neither 10-point nor six-member limits apply. */
    for(int i=0;i<gc_starter_cost_count && roster.count<22;++i){
        if(gc_starter_costs[i].cost>1)continue;
        gc_starter s={.species=gc_starter_costs[i].id,.tera_type=-1};
        football_roster_add(&roster,&scratch,&s,&profile,NULL);
    }
    assert(roster.count==22 && roster.cost<=55);
    football_roster_autofill(&roster);assert(football_roster_assign(&roster,0,21));assert(roster.lineup[0]==21);
    unsigned mask=0;for(unsigned i=0;i<22;++i)mask|=1u<<roster.lineup[i];assert(mask==(1u<<22)-1);
    FootballRoster partial=roster;partial.count=21;partial.cost=53;
    gc_starter boundary={.species=SPECIES_ID_BULBASAUR,.tera_type=-1};
    assert(!football_roster_add(&partial,&scratch,&boundary,&profile,NULL));
    partial.cost=52.1;
    assert(!football_roster_add(&partial,&scratch,&boundary,&profile,NULL));
    profile.starters[SPECIES_ID_BULBASAUR].value_reduction=1;
    assert(football_roster_add(&partial,&scratch,&boundary,&profile,NULL)); /* Classic cost reduction applies. */
    profile.starters[SPECIES_ID_BULBASAUR].value_reduction=0;partial=roster;partial.count=21;
    partial.cost=52;assert(football_roster_add(&partial,&scratch,&boundary,&profile,NULL));assert(partial.cost==55);
    gc_starter extra={.species=SPECIES_ID_BULBASAUR,.tera_type=-1};assert(!football_roster_add(&roster,&scratch,&extra,&profile,NULL));
    assert(football_live_begin(&game,&roster,42,1));assert(football_live_call(&game,FB_RUN_DIVE));
    assert(fabsf(game.actors[0].x-(game.los-1.5f))<.001f); /* Under Center */
    game.phase=FB_LIVE_CALL;assert(football_live_call(&game,FB_PASS_SLANTS));
    assert(fabsf(game.actors[0].x-(game.los-5))<.001f); /* Shotgun */
    for(unsigned i=0;i<300;++i)football_live_tick(&game,16,(FootballLiveInput){.receiver=-1});
    assert(game.phase==FB_LIVE_SET && game.snaps==0); /* Never auto-snap human offense. */
    football_live_tick(&game,1,(FootballLiveInput){.pressed=FB_LIVE_ACTION,.receiver=-1});
    football_live_tick(&game,16,(FootballLiveInput){.receiver=-1});assert(game.phase==FB_LIVE_PLAY);
    float before=game.actors[0].y;
    for(unsigned i=0;i<20 && game.phase==FB_LIVE_PLAY;++i)football_live_tick(&game,17,(FootballLiveInput){.y=-1,.receiver=-1});
    assert(game.actors[0].y<before);
    football_live_tick(&game,17,(FootballLiveInput){.receiver=2});assert(game.thrown);
    copy=game;for(unsigned i=0;i<50;++i){FootballLiveInput in={.x=.3f,.y=.4f,.receiver=-1};football_live_tick(&game,17,in);football_live_tick(&copy,17,in);}assert(!memcmp(&game,&copy,sizeof game));
    static const unsigned target_slots[5]={2,3,4,5,1};
    for(unsigned target=0;target<5;++target){
        assert(football_live_begin(&game,&roster,77,1));assert(football_live_call(&game,FB_PASS_SLANTS));
        football_live_tick(&game,17,(FootballLiveInput){.pressed=FB_LIVE_ACTION,.receiver=-1});
        football_live_tick(&game,17,(FootballLiveInput){.receiver=(int)target});
        assert(game.flying && game.target==target_slots[target]);
    }
    assert(football_live_begin(&game,&roster,77,1));assert(football_live_call(&game,FB_RUN_DIVE));
    football_live_tick(&game,17,(FootballLiveInput){.pressed=FB_LIVE_ACTION,.receiver=-1});copy=game;
    for(unsigned i=0;i<20;++i)football_live_tick(&game,20,(FootballLiveInput){.x=1,.receiver=-1});
    for(unsigned i=0;i<40;++i)football_live_tick(&copy,10,(FootballLiveInput){.x=1,.receiver=-1});
    assert(game.steps==copy.steps && game.rng==copy.rng && !memcmp(game.actors,copy.actors,sizeof game.actors));
    /* A real away formation must initially control a HOME linebacker. */
    game.phase=FB_LIVE_CALL;game.rules.offense=FB_AWAY;
    assert(football_live_defend(&game,FB_DEF_ZONE));assert(game.controlled<11 && game.actors[game.controlled].home);
    /* Defense cycles all eleven controlled actors. */
    game.phase=FB_LIVE_PLAY;game.rules.offense=FB_AWAY;game.controlled=0;
    for(unsigned i=1;i<=11;++i){football_live_switch(&game);assert(game.controlled==i%11);}
    /* A carrier can retreat farther than the old snap model's 20-yard cap. */
    assert(football_live_begin(&game,&roster,99,1));assert(football_live_call(&game,FB_RUN_DIVE));
    football_live_tick(&game,17,(FootballLiveInput){.pressed=FB_LIVE_ACTION,.receiver=-1});
    game.actors[game.carrier].x=0;
    football_live_tick(&game,17,(FootballLiveInput){.x=-1,.receiver=-1});
    assert(game.phase==FB_LIVE_DEAD && game.rules.last.safety && game.rules.score[FB_AWAY]==2);
    /* Full real-time matches must terminate without fabricated results. */
    for(unsigned seed=1;seed<=20;++seed){
        assert(football_live_begin(&game,&roster,seed,1));
        unsigned steps=0;
        while(game.phase!=FB_LIVE_END && ++steps<30000){
            if(game.phase==FB_LIVE_CALL)assert(football_live_call(&game,FB_RUN_DIVE));
            football_live_tick(&game,100,(FootballLiveInput){.x=game.rules.offense==FB_HOME?1:-1,.pressed=FB_LIVE_ACTION,.receiver=-1});
        }
        assert(game.phase==FB_LIVE_END && game.snaps>0 && steps<30000);
    }
    FootballCamera camera={0};football_camera_follow(&camera,35,26,0,16);float old=camera.x;
    football_camera_follow(&camera,95,26,0,100);assert(camera.x>old && camera.x-old<=1.801f);
    assert(fabsf(football_camera_x(&camera,camera.x)-200)<.001f);
    puts("football live: 22-member draft, lineup permutation, manual snap, movement, target throw, replay, defender switching, 20 full matches and bounded camera passed");
}
