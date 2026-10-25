#include "football_live_systems.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static FootballRoster roster;
static FootballLive m;
static float gap(unsigned a,unsigned b){return hypotf(m.actors[a].x-m.actors[b].x,m.actors[a].y-m.actors[b].y);}
static void setup(FootballDefenseId scheme,bool away)
{
    assert(football_live_begin(&m,&roster,100,1));m.rules.offense=away?FB_AWAY:FB_HOME;
    assert(football_live_call(&m,FB_PASS_SLANTS));m.defense=scheme;football_live_defense_setup(&m);m.phase=FB_LIVE_PLAY;
}
static void separate(void)
{
    for(unsigned i=0;i<22;++i){m.actors[i].x=5+(i%6)*20;m.actors[i].y=3+(i/6)*14;m.actors[i].vx=m.actors[i].vy=0;m.actors[i].block_peer=255;m.actors[i].block_time=m.actors[i].block_cooldown=0;}
}
int main(void)
{
    assert(football_roster_init(&roster));roster.count=22;
    for(unsigned i=0;i<22;++i)roster.player[i].attributes=(FootballAttributes){70,70,70,70,70,70,70,70};
    for(unsigned away=0;away<2;++away){setup(FB_DEF_MAN,away!=0);unsigned off=away?11:0,def=11-off;
        m.elapsed=7;
        for(unsigned rank=4;rank<=8;++rank){unsigned mark=m.coverage[rank];assert(mark>=off+1 && mark<=off+5);
            m.actors[mark].x=m.los+(away?-18:18);m.actors[mark].y=rank==7?5:rank==8?48:10+(rank-4)*13;
            m.actors[def+rank].x=m.los;m.actors[def+rank].y=m.actors[mark].y;
            float x,y;football_live_defense_intent(&m,def+rank,&x,&y);assert(away?x<0:x>0);
        }
    }
    /* Regression: a held pocket after three seconds still has five close matchups. */
    setup(FB_DEF_MAN,false);
    for(unsigned side=0;side<2;++side)for(unsigned i=0;i<22;++i)m.units[side][i]=roster.player[0].attributes;
    for(unsigned i=0;i<22;++i)m.actors[i].attributes=roster.player[0].attributes;
    for(unsigned i=11;i<15;++i)m.actors[i].x=105;
    for(unsigned frame=0;frame<250;++frame)football_live_tick(&m,17,(FootballLiveInput){.receiver=-1});
    assert(m.phase==FB_LIVE_PLAY && m.elapsed>4);
    for(unsigned rank=4;rank<=8;++rank)assert(gap(11+rank,m.coverage[rank])<3.5f);
    /* Zone holds an opposite-side responsibility; only matching zones break on a pass. */
    setup(FB_DEF_ZONE,false);m.elapsed=8;m.flying=true;m.target=2;m.to_x=m.los+12;m.to_y=5;
    float x,y;football_live_defense_intent(&m,11+6,&x,&y);
    assert(m.actors[17].y+y>30); /* right flat never chases a left sideline pass */
    /* Blitz preserves all five receiver matchups, with one rather than two high. */
    setup(FB_DEF_BLITZ,false);unsigned seen=0;
    for(unsigned i=4;i<11;++i)if(m.coverage[i]!=255)seen|=1u<<(m.coverage[i]-1);
    assert((seen&31)==31 && m.coverage[5]==255 && m.coverage[10]==255);
    /* Confirmed runs and catches release the shell for pursuit. */
    m.caught=true;m.flying=false;m.carrier=2;m.actors[2].y=50;m.actors[18].y=5;
    football_live_defense_intent(&m,18,&x,&y);assert(y>0);
    /* All actors have solid bodies. A controlled LB gets exactly the same contact. */
    setup(FB_DEF_MAN,true);separate();m.controlled=5;
    m.actors[17].x=50;m.actors[17].y=26;m.actors[17].attributes.str=99; /* away OL */
    m.actors[5].x=51.3f;m.actors[5].y=26;m.actors[5].attributes.str=30;
    for(unsigned frame=0;frame<180;++frame){m.actors[5].vx=-8;m.actors[5].x+=m.actors[5].vx*FB_LIVE_STEP;football_live_physics(&m);
        assert(m.actors[5].x>m.actors[17].x && gap(5,17)>=FB_BODY_DIAMETER-.002f);
    }
    /* Integration regression: held user input cannot bypass the offensive center. */
    setup(FB_DEF_MAN,true);m.controlled=5;
    for(unsigned i=0;i<22;++i){m.actors[i].x=2+i*2;m.actors[i].y=3;m.actors[i].vx=m.actors[i].vy=0;}
    m.actors[11].x=m.los+8;m.actors[11].y=26.67f;
    m.actors[19].x=m.los;m.actors[19].y=26.67f;m.actors[19].attributes.str=99;
    m.actors[5].x=m.los-2;m.actors[5].y=26.67f;m.actors[5].attributes.str=10;
    for(unsigned i=0;i<50;++i){football_live_tick(&m,17,(FootballLiveInput){.x=1,.receiver=-1});
        assert(m.phase==FB_LIVE_PLAY && m.actors[5].x<m.actors[19].x && gap(5,19)>=FB_BODY_DIAMETER-.002f);
    }
    /* Bodies work in either field direction and even without an active block timer. */
    setup(FB_DEF_MAN,false);separate();m.actors[6].x=50;m.actors[6].y=26;m.actors[11].x=51;m.actors[11].y=26;
    m.actors[6].block_cooldown=m.actors[11].block_cooldown=10;
    football_live_physics(&m);assert(gap(6,11)>=FB_BODY_DIAMETER-.002f && m.actors[6].block_peer==255);
    /* Running around a body remains possible: contact does not become a field-wide wall. */
    m.actors[11].x=51.4f;m.actors[11].y=29;m.actors[11].vx=-6;m.actors[11].x-=.1f;football_live_physics(&m);
    assert(m.actors[11].x<51.4f);
    /* Heading keeps the last horizontal run direction; QB watches receivers while retreating. */
    setup(FB_DEF_MAN,false);m.actors[2].vx=-3;football_live_update_facing(&m);assert(football_live_faces_left(&m,2));
    m.actors[2].vx=0;m.actors[2].vy=4;football_live_update_facing(&m);assert(football_live_faces_left(&m,2));
    m.actors[2].vx=3;football_live_update_facing(&m);assert(!football_live_faces_left(&m,2));
    m.actors[0].vx=-4;m.actors[2].x=m.actors[0].x+10;football_live_update_facing(&m);assert(!football_live_faces_left(&m,0));
    m.rules.offense=FB_AWAY;m.phase=FB_LIVE_DEAD;assert(!football_live_faces_left(&m,0)); /* old snap QB during turnover display */
    setup(FB_DEF_MAN,true);m.actors[11].vx=4;m.actors[13].x=m.actors[11].x-10;football_live_update_facing(&m);assert(football_live_faces_left(&m,11));
    puts("football live systems: persistent/mirrored coverage, zone discipline, blitz matchups, controlled-player body contact, movement facing and QB look direction passed");
}
