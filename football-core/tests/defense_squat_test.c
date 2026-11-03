#include "football_live_systems.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
/* Cover 4 and Zone Blitz used to start their corners 16-18 yd deep and look only for receivers past 8 yd, so a sideline route that stayed short
 * was nobody's. The corners now start 12 yd off the ball, drop to their deep third or quarter, and keep a shorter receiver in their strip until
 * he goes deep (then the deep rule takes him). Every other call is as it was. */
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
    m.defense=defense;football_live_defense_setup(&m);
}
static float depth_of(unsigned actor){float dir=m.rules.offense==FB_HOME?1.f:-1.f;return (m.actors[actor].x-m.los)*dir;}
static float zone_depth(unsigned rank){float dir=m.rules.offense==FB_HOME?1.f:-1.f;return (m.zone_x[rank]-m.los)*dir;}
/* Puts receiver `rank` at `depth` yd past the line and y, standing still. */
static void put(unsigned away,unsigned rank,float depth,float y)
{
    unsigned off=away?11:0;float dir=away?-1.f:1.f;
    FootballLiveActor *w=&m.actors[off+rank];w->x=m.los+dir*depth;w->y=y;w->vx=w->vy=0;
}
static void starts(void)
{
    for(unsigned away=0;away<2;++away){
        unsigned def=away?0:11;
        setup(5,away,40,FB_PASS_SLANTS,FB_DEF_COVER4);
        CHECK(fabsf(depth_of(def+7)-12)<.001f && fabsf(depth_of(def+8)-12)<.001f);                /* the corners start 12 yd off the ball... */
        CHECK(fabsf(zone_depth(7)-16)<.001f && fabsf(zone_depth(8)-16)<.001f);                    /* ...and drop to 16 */
        CHECK(fabsf(depth_of(def+9)-22)<.001f && fabsf(depth_of(def+10)-22)<.001f);               /* the safeties are where they were */
        setup(5,away,40,FB_PASS_SLANTS,FB_DEF_ZONE_BLITZ);
        CHECK(fabsf(depth_of(def+7)-12)<.001f && fabsf(depth_of(def+8)-12)<.001f);
        CHECK(fabsf(zone_depth(7)-18)<.001f && fabsf(zone_depth(8)-18)<.001f && fabsf(zone_depth(9)-24)<.001f);
        /* nobody else starts differently: Cover 3 keeps its corners at 18 */
        setup(5,away,40,FB_PASS_SLANTS,FB_DEF_COVER3);
        CHECK(fabsf(depth_of(def+7)-18)<.001f && fabsf(depth_of(def+8)-18)<.001f);
        for(unsigned d=0;d<FB_DEF_COUNT;++d)CHECK(((football_defense_squat_mask((FootballDefenseId)d))!=0)==(d==FB_DEF_COVER4||d==FB_DEF_ZONE_BLITZ));
    }
}
/* The rule itself, with the receivers put where the test wants them. Corner rank 7 owns the strip around y 5 (rank 8 the one around y 48). */
static void strip(void)
{
    static const FootballDefenseId calls[]={FB_DEF_COVER4,FB_DEF_ZONE_BLITZ};
    for(unsigned c=0;c<2;++c)for(unsigned away=0;away<2;++away){
        unsigned off=away?11:0,def=away?0:11;float dir=away?-1.f:1.f,ix,iy;
        FootballDefenseId call=calls[c];
        /* a short receiver in the strip is the corner's: he closes on him instead of staying at his drop spot */
        setup(21+c,away,40,FB_PASS_SLANTS,call);
        for(unsigned r=1;r<=5;++r)put(away,r,-3.f,26.67f+2.f*r);          /* everyone else behind the line, in the middle: nobody to read */
        put(away,2,5.f,8.f);
        football_live_defense_intent(&m,def+7,&ix,&iy);
        CHECK(m.coverage[7]==off+2);
        CHECK(fabsf(depth_of(def+7)+ix*dir-(5.f+FB_BODY_DIAMETER+.4f))<.01f);   /* he aims just past the receiver (about 6.7 yd off the ball), not at his drop spot, 16 */
        /* the same receiver 9 yd deep: past the 8 yd mark the ordinary deep rule has him */
        put(away,2,9.f,8.f);football_live_defense_intent(&m,def+7,&ix,&iy);
        CHECK(m.coverage[7]==off+2);
        /* too far across: not his strip */
        put(away,2,5.f,20.f);football_live_defense_intent(&m,def+7,&ix,&iy);
        CHECK(m.coverage[7]==255);
        /* behind the line by more than 3 yd: not his either */
        put(away,2,-5.f,8.f);football_live_defense_intent(&m,def+7,&ix,&iy);
        CHECK(m.coverage[7]==255);
        /* the other corner has the other strip */
        put(away,2,5.f,45.f);football_live_defense_intent(&m,def+8,&ix,&iy);
        CHECK(m.coverage[8]==off+2);
        football_live_defense_intent(&m,def+7,&ix,&iy);
        CHECK(m.coverage[7]==255);
        /* a deep receiver in the strip comes first: the corner takes WR2 at 15 yd, not WR1 at 7 yd standing right on his spot (which scores
         * better on distance alone) */
        put(away,2,7.f,5.f);put(away,3,15.f,12.f);
        football_live_defense_intent(&m,def+7,&ix,&iy);
        CHECK(m.coverage[7]==off+3);
    }
    /* Cover 3 keeps its flat defender and its corners read only the deep receivers: the same short receiver is not the corner's */
    for(unsigned away=0;away<2;++away){
        unsigned def=away?0:11;float ix,iy;
        setup(31,away,40,FB_PASS_SLANTS,FB_DEF_COVER3);
        for(unsigned r=1;r<=5;++r)put(away,r,-3.f,26.67f+2.f*r);
        put(away,2,5.f,8.f);football_live_defense_intent(&m,def+7,&ix,&iy);
        CHECK(m.coverage[7]==255);
    }
}
/* The measured thing: the QB never throws; at 1.2 s WR1 and WR2 are not left alone in the strips. The old engine left them more than 4 yd from
 * every defender in 66-80% of snaps. */
static unsigned open_wr(FootballDefenseId call,unsigned *receivers)
{
    unsigned open=0;*receivers=0;
    for(unsigned p=0;p<FB_PLAY_COUNT;++p){
        const FootballPlayDefinition *pd=football_playbook_get((FootballPlayId)p);if(pd->type!=FB_PLAY_PASS)continue;
        for(unsigned k=0;k<20;++k){
            assert(football_live_begin(&m,&roster,1u+k*7717u+p*31u,1));
            for(unsigned i=0;i<22;++i)m.units[0][i]=m.units[1][i];
            m.autoplay=0;m.rules.offense=FB_HOME;m.rules.down=1;m.rules.ball=40;m.rules.first_down_at=50;m.rules.quarter=2;m.rules.seconds_left=900;
            assert(football_live_call(&m,(FootballPlayId)p));m.defense=call;football_live_defense_setup(&m);
            unsigned before=m.snaps,guard=0;bool snapped=false,seen=false;
            while(m.snaps==before && !seen){
                FootballLiveInput in={.receiver=-1};if(m.phase==FB_LIVE_SET && !snapped){in.pressed=FB_LIVE_ACTION;snapped=true;}
                football_live_tick(&m,16,in);assert(++guard<20000);
                if(m.phase==FB_LIVE_PLAY && m.elapsed>=1.2f && !m.thrown && m.carrier==0 && !m.scramble){
                    seen=true;
                    for(unsigned r=2;r<=3;++r){float near=999;for(unsigned q=11;q<22;++q){float g=hypotf(m.actors[r].x-m.actors[q].x,m.actors[r].y-m.actors[q].y);if(g<near)near=g;}
                        ++*receivers;if(near>4.f)++open;}
                }
            }
        }
    }
    return open;
}
static void held(void)
{
    static const FootballDefenseId calls[]={FB_DEF_COVER4,FB_DEF_ZONE_BLITZ};
    for(unsigned c=0;c<2;++c){
        unsigned receivers,open=open_wr(calls[c],&receivers);
        printf("  %s: WR1/WR2 more than 4 yd from every defender at 1.2 s: %u of %u (%.1f%%)\n",c?"Zone Blitz":"Cover 4",open,receivers,100.*open/receivers);
        CHECK(receivers>=400);
        CHECK(open*100<receivers*25);                 /* it was 66-80% */
    }
}
int main(void)
{
    assert(football_roster_init(&roster));roster.count=22;
    for(unsigned i=0;i<22;++i)roster.player[i].attributes=(FootballAttributes){70,70,70,70,70,70,70,70};
    starts();strip();held();
    printf("defense squat: %u checks (Cover 4 and Zone Blitz corners start 12 yd off, hold a short receiver in their strip until he goes deep, deep receivers first; other calls unchanged; WR1/WR2 no longer left alone at 1.2 s) passed\n",checks);
}
