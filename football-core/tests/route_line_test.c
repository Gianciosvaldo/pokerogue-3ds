#include "football_live_systems.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
/* The lines the screen draws before the snap come from football_live_route_line: for every play, seed, side and mirror the line of each
 * eligible player starts at his own spot, follows the route he was called (a pass: the receiver's points from his spot, inside toward the
 * middle; a run: his way to the handoff and on along the called path), keeps to the field, and a run has exactly one line, the runner's. */
static FootballRoster roster;
static FootballLive m;
static unsigned checks;
#define CHECK(c) do{++checks;assert(c);}while(0)
static void setup(uint32_t seed,unsigned away,FootballPlayId play)
{
    assert(football_live_begin(&m,&roster,seed,1));
    for(unsigned i=0;i<22;++i)m.units[0][i]=m.units[1][i];
    m.rules.offense=away?FB_AWAY:FB_HOME;m.rules.down=1;m.rules.ball=40;m.rules.first_down_at=50;m.rules.quarter=2;m.rules.seconds_left=900;
    m.autoplay=1;
    assert(football_live_call(&m,play));
}
/* Run the CPU's snap (1.2 s in the set) and stop on the first frame of the play. */
static void snap(void){for(unsigned g=0;m.phase!=FB_LIVE_PLAY;++g){assert(g<400);football_live_tick(&m,16,(FootballLiveInput){.receiver=-1});}}
static void pass_line(const FootballPlayDefinition *p,unsigned rank)
{
    float x[8],y[8];bool settles=false;unsigned n=football_live_route_line(&m,rank,x,y,8,&settles);
    float dir=m.rules.offense==FB_HOME?1.f:-1.f,lane=m.snap_y[rank],inside=lane<26.67f?1.f:-1.f;
    FootballRouteView path=football_playbook_receiver_path(p,rank);
    CHECK(n==path.count+1 && x[0]==m.snap_x[rank] && y[0]==m.snap_y[rank]);
    for(unsigned k=0;k<path.count;++k){
        float wy=lane+path.point[k].y*inside;wy=wy<2?2:wy>51?51:wy;
        CHECK(fabsf(x[k+1]-(m.snap_x[rank]+dir*path.point[k].x))<.001f && fabsf(y[k+1]-wy)<.001f);
        CHECK(y[k+1]>=2 && y[k+1]<=51 && x[k+1]>=0 && x[k+1]<=120);
    }
    CHECK(settles==football_playbook_settles(p));
    /* Inside is toward the middle: the first lateral move of a crosser goes the way of the middle of the field. */
    float lateral=y[n-1]-y[0];
    if(fabsf(path.point[path.count-1].y)>1)CHECK((lateral>0)==((path.point[path.count-1].y>0)==(inside>0)));
}
static void run_line(const FootballPlayDefinition *p,unsigned rank)
{
    float x[8],y[8];bool settles=true;unsigned n=football_live_route_line(&m,rank,x,y,8,&settles);
    float dir=m.rules.offense==FB_HOME?1.f:-1.f;unsigned runner=football_playbook_runner(p);
    if(rank!=runner){CHECK(n==0 && !settles);return;}
    CHECK(n>=3 && n<=8 && !settles);
    CHECK(x[0]==m.snap_x[rank] && y[0]==m.snap_y[rank]);
    /* The second point is where the ball changes hands, the same place the live handoff uses, and it is on the side the play calls for. */
    float mx,my;football_live_mesh_point_for(&m,runner+(m.rules.offense==FB_HOME?0:11),&mx,&my);CHECK(x[1]==mx && y[1]==my);
    unsigned off=m.rules.offense==FB_HOME?0:11;float lined=m.snap_y[runner]-m.snap_y[0];FootballHandoff h=football_playbook_handoff(p);
    if(h.side==FB_MESH_LINED && fabsf(lined)>.4f)CHECK((my-m.actors[off].y>0)==(lined>0));
    /* Then it goes on along the called path to its end, keeping to the field. */
    float last=p->route[p->route_count-1].x;
    CHECK((x[n-1]-m.los)*dir>=last-.01f);
    for(unsigned k=0;k<n;++k)CHECK(isfinite(x[k]) && isfinite(y[k]) && y[k]>=0 && y[k]<=53.34f);
    for(unsigned k=2;k<n;++k)CHECK((x[k]-m.los)*dir>(x[k-1]-m.los)*dir-1.01f); /* never back across the line toward the QB */
    /* The line does not move at the snap: the handoff the runner then plays is the one it showed. */
    float before[8][2];for(unsigned k=0;k<n;++k){before[k][0]=x[k];before[k][1]=y[k];}
    snap();
    float px[8],py[8];unsigned again=football_live_route_line(&m,rank,px,py,8,&settles);
    CHECK(again==n);for(unsigned k=0;k<n;++k)CHECK(px[k]==before[k][0] && py[k]==before[k][1]);
    football_live_mesh_point(&m,&mx,&my);CHECK(mx==x[1] && my==y[1]);
}
int main(void)
{
    assert(football_roster_init(&roster));roster.count=22;
    for(unsigned i=0;i<22;++i)roster.player[i].attributes=(FootballAttributes){70,70,70,70,70,70,70,70};
    unsigned runs=0,passes=0,sides[2]={0,0};
    for(unsigned id=0;id<FB_PLAY_COUNT;++id){
        const FootballPlayDefinition *p=football_playbook_get((FootballPlayId)id);
        for(unsigned k=0;k<40;++k)for(unsigned away=0;away<2;++away){
            uint32_t seed=1u+k*7717u; /* spread out: neighbouring small seeds draw alike */
            setup(seed,away,(FootballPlayId)id);
            float x[8],y[8];bool settles;
            CHECK(football_live_route_line(&m,0,x,y,8,&settles)==0 && football_live_route_line(&m,6,x,y,8,&settles)==0 && football_live_route_line(&m,2,x,y,1,&settles)==0);
            if(p->type==FB_PLAY_PASS){for(unsigned rank=1;rank<=5;++rank)pass_line(p,rank);++passes;}
            else{for(unsigned rank=1;rank<=5;++rank){setup(seed,away,(FootballPlayId)id);run_line(p,rank);}++runs;sides[m.run_side>0]++;}
        }
    }
    CHECK(passes==12u*80 && runs==8u*80 && sides[0]>0 && sides[1]>0);
    /* A kick has no called path. */
    assert(football_live_begin(&m,&roster,1,1));m.rules.down=4;m.autoplay=1;
    float x[8],y[8];bool settles;
    assert(football_live_special(&m,FB_SPECIAL_PUNT));
    for(unsigned rank=0;rank<8;++rank)CHECK(football_live_route_line(&m,rank,x,y,8,&settles)==0);
    printf("route lines: %u checks (every play, 40 seeds, both sides: a line per receiver along his called route, the runner's way to the handoff and on, none for the QB, the line, kicks) passed\n",checks);
}
