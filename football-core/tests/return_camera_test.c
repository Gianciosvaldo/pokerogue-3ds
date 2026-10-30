#include "football_live_systems.h"
#include "football_camera.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
static FootballRoster roster;
static FootballLive m;
static unsigned flight_frames,return_frames,plays;
static FootballLiveInput idle(void){return(FootballLiveInput){.receiver=-1};}
static void begin(unsigned seed,FootballSpecial kind,FootballSide kicker)
{
    assert(football_live_begin(&m,&roster,seed,1));
    if(kind==FB_SPECIAL_KICKOFF){
        /* Enter the real next-call transition used after a score or the half. */
        m.phase=FB_LIVE_DEAD;m.after=FB_AFTER_KICKOFF;m.after_side=kicker;
        for(unsigned i=0;m.phase==FB_LIVE_DEAD;i++){assert(i<400);football_live_tick(&m,17,idle());}
        assert(m.phase==FB_LIVE_SET&&m.special==kind);
    }else{
        m.phase=FB_LIVE_CALL;m.special=FB_SPECIAL_NONE;m.rules.offense=kicker;
        m.rules.ball=45;m.rules.down=4;m.rules.first_down_at=55;
        assert(football_live_special(&m,kind));
    }
}
static void receiving(unsigned seed,FootballSpecial kind)
{
    begin(seed,kind,FB_AWAY);assert(m.controlled==9);
    FootballCamera camera={0};unsigned frames=0,flights=0;
    while(m.phase!=FB_LIVE_DEAD&&m.phase!=FB_LIVE_END){
        assert(++frames<3000);FootballLiveInput input=idle();
        if(m.phase==FB_LIVE_SET)input.pressed=FB_LIVE_ACTION;
        if(m.stage==FB_STAGE_FLIGHT){
            const FootballLiveActor*p=&m.actors[m.controlled];float dx=m.to_x-p->x,dy=m.to_y-p->y,d=hypotf(dx,dy);
            if(d>.3f){input.x=dx/d;input.y=dy/d;}
        }
        float x,y,vx;football_live_focus(&m,&x,&y,&vx);
        const FootballLiveActor*p=&m.actors[m.controlled];
        assert(x==p->x&&y==p->y&&vx==p->vx);
        football_camera_follow(&camera,x,y,vx,16);
        /* Check actual projected visibility throughout the flight, not merely
         * which coordinate was returned by the focus helper. */
        float px=football_camera_x(&camera,p->x),py=football_camera_y(&camera,p->y);
        assert(px>=12&&px<=388&&py>=40&&py<=220);
        if(m.stage==FB_STAGE_FLIGHT){flights++;flight_frames++;}
        if(m.stage==FB_STAGE_RETURN)return_frames++;
        football_live_tick(&m,16,input);
    }
    assert(flights>0);plays++;
}
static void other_views(void)
{
    float x,y,vx;
    begin(20,FB_SPECIAL_PUNT,FB_HOME);
    football_live_focus(&m,&x,&y,&vx);assert(x==m.actors[0].x&&y==m.actors[0].y);
    m.flying=true;m.stage=FB_STAGE_FLIGHT;m.phase=FB_LIVE_PLAY;m.ball_x=72;m.ball_y=8;
    football_live_focus(&m,&x,&y,&vx);assert(x==72&&y==8&&vx==0); /* human kicking */
    begin(20,FB_SPECIAL_KICKOFF,FB_AWAY);m.autoplay=1;m.flying=true;m.stage=FB_STAGE_FLIGHT;m.phase=FB_LIVE_PLAY;m.ball_x=50;m.ball_y=7;
    football_live_focus(&m,&x,&y,&vx);assert(x==50&&y==7&&vx==0); /* CPU-only demo */
    m.autoplay=0;m.special=FB_SPECIAL_NONE;
    football_live_focus(&m,&x,&y,&vx);assert(x==50&&y==7&&vx==0); /* ordinary pass */
    m.special=FB_SPECIAL_FIELD_GOAL;
    football_live_focus(&m,&x,&y,&vx);assert(x==50&&y==7&&vx==0);
    m.special=FB_SPECIAL_KICKOFF;m.phase=FB_LIVE_DEAD;m.flying=false;
    football_live_focus(&m,&x,&y,&vx);assert(x==50&&y==7&&vx==0); /* dead-ball result */
}
int main(void)
{
    assert(football_roster_init(&roster));roster.count=22;
    for(unsigned i=0;i<22;i++)roster.player[i].attributes=(FootballAttributes){60,60,60,60,60,60,60,60};
    for(unsigned seed=1;seed<=80;seed++){receiving(seed,FB_SPECIAL_KICKOFF);receiving(seed,FB_SPECIAL_PUNT);}
    other_views();assert(plays==160&&flight_frames>10000&&return_frames>0);
    printf("Return camera: %u real kicks, %u flight frames and %u return frames keep the human visible; other views retained\n",plays,flight_frames,return_frames);
    return 0;
}
