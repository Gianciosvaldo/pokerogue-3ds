#include "football_live_systems.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
/* The goal line squeezes the defense: whatever the call, the play and the side, nobody starts (or is placed to drop) deeper than a couple of
 * yards past the goal line, nobody starts off the field, and nothing moves in the open field. A deep player takes a receiver in the end zone
 * over from the underneath players as soon as he is past the goal line, instead of only at 8 yd. */
static FootballRoster roster;
static FootballLive m;
static unsigned checks;
#define CHECK(c) do{++checks;assert(c);}while(0)
static void setup(uint32_t seed,unsigned away,unsigned ball,FootballPlayId play,FootballDefenseId defense)
{
    assert(football_live_begin(&m,&roster,seed,1));
    m.rules.offense=away?FB_AWAY:FB_HOME;m.rules.down=1;m.rules.ball=(uint8_t)ball;m.rules.first_down_at=ball>=90?100:ball+10;m.rules.quarter=2;m.rules.seconds_left=900;
    m.autoplay=1;
    assert(football_live_call(&m,play));
    /* The CPU picked a call of its own for the formation; put the defenders back on the default look before the forced call is set up, so a
     * player the forced call does not place does not keep the spot the other call gave him (the game always arranges after the call is set). */
    static const float dx[11]={2,2,2,2,7,7,7,2,2,15,15},dy[11]={20,24,29,33,15,26.67f,38,5,48,18,36};
    unsigned def=away?0:11;float dir=away?-1.f:1.f;
    for(unsigned r=0;r<11;++r){m.actors[def+r].x=m.los+dir*dx[r];m.actors[def+r].y=dy[r];m.actors[def+r].vx=m.actors[def+r].vy=0;}
    m.defense=defense;football_live_defense_setup(&m);
}
static float depth_of(unsigned actor){float dir=m.rules.offense==FB_HOME?1.f:-1.f;return (m.actors[actor].x-m.los)*dir;}
static void squeeze(void)
{
    static const unsigned balls[]={1,30,60,79,80,85,90,93,95,97,98,99};
    for(unsigned b=0;b<sizeof balls/sizeof *balls;++b)for(unsigned away=0;away<2;++away)for(unsigned d=0;d<FB_DEF_COUNT;++d)for(unsigned p=0;p<FB_PLAY_COUNT;++p){
        uint32_t seed=1u+b*7717u+p*131u+d;float open_depth[11];
        setup(seed,away,40,(FootballPlayId)p,(FootballDefenseId)d);for(unsigned r=0;r<11;++r)open_depth[r]=depth_of((away?0:11)+r);   /* the same snap in the open field */
        setup(seed,away,balls[b],(FootballPlayId)p,(FootballDefenseId)d);
        float room=100.f-balls[b],dir=away?-1.f:1.f;unsigned def=away?0:11;
        CHECK(fabsf(football_live_goal_room(&m)-room)<.001f);
        for(unsigned r=0;r<11;++r){
            bool deep=(football_defense_deep_mask((FootballDefenseId)d)&(1u<<r))!=0;
            float cap=room+(deep?FB_LIVE_DEEP_BEHIND:FB_LIVE_SHALLOW_BEHIND);
            CHECK(depth_of(def+r)<=cap+.001f && (m.zone_x[r]-m.los)*dir<=cap+.001f);
            CHECK(m.actors[def+r].x>=0 && m.actors[def+r].x<=120 && m.zone_x[r]>=1 && m.zone_x[r]<=119);
            if(balls[b]==97)CHECK(depth_of(def+r)<=(deep?5.5f:4.f)+.001f); /* from the 3: the deep players 3 yd into the end zone at most, the rest 1 */
            /* squeezed to the cap exactly when the open-field spot is deeper than it, and not moved at all otherwise */
            CHECK(fabsf(depth_of(def+r)-(open_depth[r]>cap?cap:open_depth[r]))<.001f);
        }
    }
}
/* Away from the goal line the look is the same wherever the ball is (relative to the line of scrimmage), as it was before the squeeze. */
static void open_field(void)
{
    for(unsigned away=0;away<2;++away)for(unsigned d=0;d<FB_DEF_COUNT;++d)for(unsigned p=0;p<FB_PLAY_COUNT;++p){
        float depth[2][11],y[2][11],zone[2][11];
        for(unsigned k=0;k<2;++k){
            setup(99u+p*13u+d,away,k?70:30,(FootballPlayId)p,(FootballDefenseId)d);unsigned def=away?0:11;float dir=away?-1.f:1.f;
            for(unsigned r=0;r<11;++r){depth[k][r]=depth_of(def+r);y[k][r]=m.actors[def+r].y;zone[k][r]=(m.zone_x[r]-m.los)*dir;}
        }
        for(unsigned r=0;r<11;++r)CHECK(fabsf(depth[0][r]-depth[1][r])<.001f && fabsf(y[0][r]-y[1][r])<.001f && fabsf(zone[0][r]-zone[1][r])<.001f);
    }
    /* and the open-field depths themselves are the old ones: Cover 3 has its deep players at 18, 18 and 24 yd, Cover 4 its safeties at 22, 22 (its
 * corners start at 12 now and drop to 16, see defense_squat_test). */
    setup(5,0,40,FB_PASS_SLANTS,FB_DEF_COVER3);CHECK(fabsf(depth_of(18)-18)<.001f && fabsf(depth_of(19)-18)<.001f && fabsf(depth_of(20)-24)<.001f);
    setup(5,0,40,FB_PASS_SLANTS,FB_DEF_COVER4);CHECK(fabsf(depth_of(18)-12)<.001f && fabsf(depth_of(19)-12)<.001f && fabsf(depth_of(20)-22)<.001f && fabsf(depth_of(21)-22)<.001f);
}
/* From the 3 the deep middle safety of Cover 3 has the receiver standing 5 yd deep (2 yd into the end zone); from the 40 he does not. */
static void deep_handoff(void)
{
    for(unsigned away=0;away<2;++away){
        for(unsigned k=0;k<2;++k){
            setup(21,away,k?97:40,FB_PASS_SLANTS,FB_DEF_COVER3);
            unsigned off=away?11:0,def=away?0:11;float dir=away?-1.f:1.f;
            FootballLiveActor *w=&m.actors[off+2];w->x=m.los+dir*5;w->y=26.67f;w->vx=w->vy=0;
            float ix,iy;football_live_defense_intent(&m,def+9,&ix,&iy);
            if(k)CHECK(m.coverage[9]==off+2 && ix*dir>-.001f); /* he takes the receiver in the end zone */
            else CHECK(m.coverage[9]==255);                    /* 5 yd deep is still the linebackers' in the open field */
        }
    }
}
int main(void)
{
    assert(football_roster_init(&roster));roster.count=22;
    for(unsigned i=0;i<22;++i)roster.player[i].attributes=(FootballAttributes){70,70,70,70,70,70,70,70};
    squeeze();open_field();deep_handoff();
    printf("defense goal line: %u checks (no defender deeper than 2.5 yd (deep) or 1 yd past the goal line, none off the field, every call, play, side and ball position; open-field looks unchanged; end-zone receivers taken over by the deep players) passed\n",checks);
}
