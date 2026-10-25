#include "football_match_view.h"
#include <assert.h>
#include <stdio.h>
#include <math.h>
#include "football_playbook.h"
#include <string.h>
static rr_frame frames[FB_RENDER_ACTOR_CAP];
static unsigned slots[FB_RENDER_ACTOR_CAP],lookups;
const rr_frame *football_render_front_frame(const FootballRenderer *r,unsigned slot,uint32_t ms)
{
    (void)r;(void)ms;assert(slot<FB_RENDER_ACTOR_CAP && lookups<FB_RENDER_ACTOR_CAP);
    slots[lookups++]=slot;return &frames[slot];
}
int main(void)
{
    FootballTeam home={0},away={0};home.count=2;away.count=3;
    for (unsigned i=0;i<2;++i) home.players[i].attributes=(FootballAttributes){50,50,50,50,50,50,50,50};
    for (unsigned i=0;i<3;++i) away.players[i].attributes=home.players[0].attributes;
    home.players[1].attributes.thp=90;home.players[1].attributes.tha=90;
    FootballMatch match;
    assert(football_match_begin(&match,&home,&away,42,256));
    FootballRenderer renderer={0};FootballRenderScene scene;
    uint32_t rng=match.rng;
    assert(football_match_scene(&scene,&match,&renderer,0));
    assert(scene.actor_count==5 && scene.ball_x_yards==35 && scene.down==1 && scene.distance==10);
    assert(scene.actors[1].x_yards==30); /* Actual best QB aligns in shotgun. */
    assert(!scene.actors[0].away && scene.actors[2].away);
    const unsigned expected[]={0,1,6,7,8};
    for (unsigned i=0;i<5;++i) assert(slots[i]==expected[i] && scene.actors[i].front_frame==&frames[expected[i]]);
    assert(match.rng==rng); /* A render snapshot never consumes match RNG. */
    match.rules.offense=FB_AWAY;
    match.phase=FB_MATCH_ANIMATION;match.animation_ms=600;match.resolved.yards=12;
    lookups=0;
    assert(football_match_scene(&scene,&match,&renderer,500));
    assert(scene.ball_x_yards==85 && scene.actors[2].movement_x<0);
    for (unsigned i=0;i<scene.actor_count;++i) {
        assert(scene.actors[i].x_yards>=0 && scene.actors[i].x_yards<=120);
        assert(scene.actors[i].y_yards>=0 && scene.actors[i].y_yards<=53.333334f);
    }
    match.rules.quarter=5;match.rules.overtime_drives=8;match.rules.finished=true;
    lookups=0;
    assert(football_match_scene(&scene,&match,&renderer,1000));assert(scene.quarter==8); /* OT4, not OT5. */
    /* Every named route mirrors across possession, reaches the resolved spot,
     * preserves the LOS, and is a pure snapshot regardless of render cadence. */
    match.rules.finished=false;match.rules.quarter=1;
    for (unsigned id=0;id<FB_PLAY_COUNT;++id) {
        const FootballPlayDefinition *p=football_playbook_get((FootballPlayId)id);
        match.play=(PlayInfo){p->formation,p->type,p->id,0};match.carrier=0;
        match.resolved.kind=p->type==FB_PLAY_PASS ? FB_RESULT_COMPLETE : FB_RESULT_RUN;
        match.resolved.yards=15;match.animation_ms=FB_PLAY_ANIMATION_MS;
        match.rules.offense=FB_HOME;lookups=0;
        FootballMatch before=match;
        assert(football_match_scene(&scene,&match,&renderer,100));
        assert(!memcmp(&before,&match,sizeof match));
        assert(scene.ball_x_yards==35 && fabsf(scene.actors[0].x_yards-50)<.001f);
        assert(scene.football_visible && fabsf(scene.football_x_yards-50)<.001f);
        match.rules.offense=FB_AWAY;lookups=0;
        assert(football_match_scene(&scene,&match,&renderer,100));
        assert(fabsf(scene.actors[2].x_yards-70)<.001f && scene.ball_x_yards==85);
        match.animation_ms=350;lookups=0;
        assert(football_match_scene(&scene,&match,&renderer,100));
        assert(isfinite(scene.football_x_yards) && scene.football_height>=0 && scene.football_height<=1);
        if (p->type==FB_PLAY_PASS) assert(scene.football_height>0);
    }
    FootballRenderScene saved=scene;
    match.teams[0].count=7;
    assert(!football_match_scene(&scene,&match,&renderer,0));
    assert(!memcmp(&saved,&scene,sizeof scene));
    puts("football match view: scoreboard, actual QB, field direction, borrowed sprite slots and overtime passed");
}
