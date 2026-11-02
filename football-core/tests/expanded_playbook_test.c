#include "football_live_systems.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
static FootballRoster roster;
static FootballLive m;
static unsigned trajectories;
static void setup(unsigned away,FootballPlayId p)
{
    assert(football_live_begin(&m,&roster,123,1));
    m.rules.offense=away?FB_AWAY:FB_HOME;
    assert(football_live_call(&m,p));
}
static float distance_to_path(FootballRouteView path,bool settles,float x,float y)
{
    float best=1e9f;FootballRoutePoint start={0,0};
    for(unsigned k=0;k<path.count+(settles?0u:1u);++k){
        FootballRoutePoint end=k<path.count?path.point[k]:(FootballRoutePoint){start.x+100,start.y};
        float dx=end.x-start.x,dy=end.y-start.y,len=dx*dx+dy*dy;
        float t=len?fmaxf(0,fminf(1,((x-start.x)*dx+(y-start.y)*dy)/len)):0;
        best=fminf(best,hypotf(x-start.x-t*dx,y-start.y-t*dy));start=end;
    }
    return best;
}
static void routes(void)
{
    static const FootballAttributes profiles[]={{35,35,60,60,60,60,60,60},{99,25,60,60,60,60,60,60},{99,99,60,60,60,60,60,60}};
    for(unsigned a=0;a<3;++a)for(unsigned away=0;away<2;++away)for(unsigned p=FB_PASS_STICK;p<FB_PLAY_COUNT;++p)for(unsigned rank=1;rank<=5;++rank){
        setup(away,(FootballPlayId)p);unsigned off=away?11:0,i=off+rank,def=11-off;
        FootballLiveActor *actor=&m.actors[i];float sx=actor->x,sy=actor->y,dir=away?-1:1,inside=sy<26.67f?1:-1;
        const FootballPlayDefinition *play=football_playbook_get((FootballPlayId)p);
        FootballRouteView path=football_playbook_receiver_path(play,rank);assert(path.count==4);
        for(unsigned d=def;d<def+11;++d){m.actors[d].x=away?1:119;m.actors[d].y=1;}
        actor->attributes=profiles[a];float worst=0;
        /* Real route steering and acceleration, isolated from throws/contact. */
        for(unsigned frame=0;frame<600;++frame){float x,y;football_live_ai_receiver(&m,i,&x,&y);football_live_move(actor,x,y,false);
            worst=fmaxf(worst,distance_to_path(path,football_playbook_settles(play),(actor->x-sx)*dir,(actor->y-sy)*inside));}
        if(worst>=1)fprintf(stderr,"route error %.3f profile%u side%u play%u rank%u\n",worst,a,away,p,rank);
        assert(worst<1);
        if(football_playbook_settles(play)){
            FootballRoutePoint end=path.point[path.count-1];
            assert(hypotf((actor->x-sx)*dir-end.x,(actor->y-sy)*inside-end.y)<.3f);
            assert(hypotf(actor->vx,actor->vy)<.3f);
        }
        ++trajectories;
    }
}
static void assignments(void)
{
    /* Independent responsibilities: four/five/six rushers and 0..4 deep zones. */
    static const unsigned rush[]={4,4,5,4,4,4,5,6},deep[]={2,2,1,1,3,4,3,0};
    for(unsigned away=0;away<2;++away)for(unsigned near_goal=0;near_goal<2;++near_goal)for(unsigned d=0;d<FB_DEF_COUNT;++d){
        setup(away,FB_PASS_MESH);unsigned off=away?11:0,def=11-off;
        if(near_goal)m.los=away?11:109;
        m.defense=(FootballDefenseId)d;football_live_defense_setup(&m);
        unsigned nr=0,nd=0,seen=0;
        for(unsigned rank=0;rank<11;++rank){
            if(football_defense_is_rusher(m.defense,rank)){++nr;assert(m.coverage[rank]==255);}
            if(football_defense_deep_mask(m.defense)&(1u<<rank)){++nd;if(!football_defense_is_zone(m.defense))assert(m.coverage[rank]==255);}
            if(!football_defense_is_zone(m.defense) && m.coverage[rank]!=255){unsigned mark=m.coverage[rank]-off;assert(mark>=1 && mark<=5);assert(!(seen&(1u<<mark)));seen|=1u<<mark;}
            assert(m.zone_x[rank]>=1 && m.zone_x[rank]<=119 && m.zone_y[rank]>=2 && m.zone_y[rank]<=51);
            assert(isfinite(m.actors[def+rank].x) && m.actors[def+rank].y>=2 && m.actors[def+rank].y<=51);
        }
        assert(nr==rush[d] && nd==deep[d]);
        if(!football_defense_is_zone(m.defense))assert(seen==62);
    }
    /* Three/four-deep outside corners must take the outside receivers once
     * their routes enter the shell, in both field directions. */
    for(unsigned away=0;away<2;++away)for(unsigned d=FB_DEF_COVER3;d<=FB_DEF_ZONE_BLITZ;++d){
        setup(away,FB_PASS_VERTICALS);unsigned off=away?11:0,def=11-off;float dir=away?-1:1;
        m.defense=(FootballDefenseId)d;football_live_defense_setup(&m);m.phase=FB_LIVE_PLAY;
        for(unsigned rank=1;rank<=5;++rank){m.actors[off+rank].x=m.los+dir*12;m.actors[off+rank].y=26;}
        m.actors[off+2].y=4;m.actors[off+3].y=49;
        float x,y;football_live_defense_intent(&m,def+7,&x,&y);assert(m.coverage[7]==off+2);
        football_live_defense_intent(&m,def+8,&x,&y);assert(m.coverage[8]==off+3);
    }
    unsigned called[FB_DEF_COUNT]={0};
    for(unsigned seed=0;seed<10000;++seed)++called[football_defense_pick(seed,1,10,25)];
    for(unsigned d=0;d<FB_DEF_COUNT;++d)assert(called[d]>500);
}
static void carriers(void)
{
    for(unsigned away=0;away<2;++away){
        setup(away,FB_RUN_JET_SWEEP);unsigned off=away?11:0,def=11-off;float dir=away?-1:1;
        m.autoplay=away?0:1;m.elapsed=1.3f;football_live_tick(&m,17,(FootballLiveInput){.receiver=-1});
        assert(m.phase==FB_LIVE_PLAY && m.carrier==off+4);
        assert(football_live_is_blocker(&m,off+1) && !football_live_is_blocker(&m,off+4));
        for(unsigned d=def;d<def+11;++d){m.actors[d].x=away?1:119;m.actors[d].y=1;}
        m.actors[off+4].x=m.los-dir;m.actors[off+4].y=26.67f;m.run_side=1;
        float x,y;football_live_ai_carrier(&m,off+4,&x,&y);assert(y<-.5f); /* called jet edge */
        /* The RB is an actual contact blocker, not just an eligible flag. */
        m.actors[off+1].x=50;m.actors[off+1].y=25;m.actors[def].x=51.4f;m.actors[def].y=25;
        football_live_physics(&m);assert(m.actors[off+1].block_peer==def);
        setup(away,FB_PASS_HB_SCREEN);m.phase=FB_LIVE_PLAY;m.carrier=off+1;m.caught=true;
        assert(!football_live_is_blocker(&m,off+1));
        for(unsigned rank=2;rank<=5;++rank)assert(football_live_is_blocker(&m,off+rank));
        m.caught=false;m.carrier=off;for(unsigned rank=1;rank<=5;++rank)assert(!football_live_is_blocker(&m,off+rank));
    }
}
int main(void)
{
    /* Serialized and historical IDs retain their values. */
    assert(FB_RUN_INSIDE_ZONE==0 && FB_PASS_POST==7 && FB_DEF_MAN==0 && FB_DEF_ZONE==1 && FB_DEF_BLITZ==2);
    assert(FB_PLAY_COUNT==20 && FB_DEF_COUNT==8);
    assert(football_roster_init(&roster));roster.count=22;
    for(unsigned i=0;i<22;++i)roster.player[i].attributes=(FootballAttributes){70,70,70,70,70,70,70,70};
    routes();assignments();carriers();
    printf("expanded playbook: %u mirrored route trajectories, eight coverage/rush assignments, Jet Sweep carrier and RB contact blocking passed\n",trajectories);
}
