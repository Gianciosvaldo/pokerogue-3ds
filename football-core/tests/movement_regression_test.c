#include "football_live_systems.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
static FootballRoster roster;
static FootballLive m;
static unsigned checks,failed;
#define CHECK(c,...) do{++checks;if(!(c)){++failed;if(failed<25){printf("FAIL: ");printf(__VA_ARGS__);puts("");}}}while(0)
static void setup(unsigned away,FootballPlayId play)
{
    CHECK(football_live_begin(&m,&roster,111,1),"begin");
    m.rules.offense=away?FB_AWAY:FB_HOME;
    CHECK(football_live_call(&m,play),"call");m.phase=FB_LIVE_PLAY;
    m.controlled=away?11:0;m.thrown=true; /* hold the ball while isolated route behavior is observed */
}
static void isolate(unsigned receiver,unsigned defender)
{
    unsigned off=m.rules.offense==FB_HOME?0:11,def=11-off;
    for(unsigned i=0;i<22;++i){
        m.actors[i].x=i>=off && i<off+11?(off?118:2):(off?2:118);
        m.actors[i].y=2+(i%11)*4;
        m.actors[i].attributes=(FootballAttributes){1,1,1,1,1,1,1,1};
    }
    m.actors[off].x=m.los+(off?8:-8);m.actors[off].y=26.67f;
    m.actors[receiver].x=m.los+(off?1:-1);m.actors[receiver].y=20;
    m.actors[receiver].attributes=(FootballAttributes){99,99,25,60,60,60,60,60};
    if(defender<22){m.actors[defender].x=m.los+(off?-16:16);m.actors[defender].y=20;
        m.actors[defender].attributes=(FootballAttributes){25,25,80,60,60,60,60,60};m.coverage[defender-def]=(uint8_t)receiver;}
}
int main(void)
{
    memset(&roster,0,sizeof roster);roster.count=22;
    CHECK(football_stats_build_maxima(&roster.maxima,pr_forms,PR_FORM_COUNT),"maxima");
    for(unsigned i=0;i<22;++i){roster.lineup[i]=(uint8_t)i;roster.player[i].attributes=(FootballAttributes){60,60,60,60,60,60,60,60};}
    const FootballPlayId passes[]={FB_PASS_SLANTS,FB_PASS_PLAY_ACTION,FB_PASS_VERTICALS,FB_PASS_POST};
    FootballAttributes profiles[3]={{25,25,25,60,60,60,60,60},{99,99,25,60,60,60,60,60},{0}};
    FootballPlayer electrode;bool found=false;
    for(unsigned f=0;f<PR_FORM_COUNT;++f)if(pr_forms[f].species==SPECIES_ID_ELECTRODE && !pr_forms[f].form_index){
        found=football_stats_generate(&electrode,101,&pr_forms[f],&roster.maxima);break;
    }
    CHECK(found,"actual Electrode attributes generated");profiles[2]=electrode.attributes;
    unsigned backward=0;
    for(unsigned profile=0;profile<3;++profile)for(unsigned away=0;away<2;++away)for(unsigned p=0;p<4;++p)for(unsigned rank=1;rank<=5;++rank){
        setup(away,passes[p]);unsigned off=away?11:0,i=off+rank;float dir=away?-1:1;
        isolate(i,22);m.actors[i].attributes=profiles[profile];
        for(unsigned f=0;f<300;++f){float before=m.actors[i].x;football_live_tick(&m,17,(FootballLiveInput){.receiver=-1});
            if(m.phase!=FB_LIVE_PLAY)break;
            if((m.actors[i].x-before)*dir<-.005f)backward++;
        }
    }
    CHECK(backward==0,"unobstructed receivers turned backward on %u frames",backward);
    for(unsigned profile=1;profile<3;++profile)for(unsigned away=0;away<2;++away){
        setup(away,FB_PASS_VERTICALS);unsigned off=away?11:0,def=11-off,i=off+2,d=def+7;float dir=away?-1:1;
        isolate(i,d);m.defense=FB_DEF_MAN;m.actors[i].y=m.actors[d].y=6;m.actors[i].attributes=profiles[profile];m.elapsed=3;m.actors[i].x=m.los+dir*15;m.actors[d].x=m.actors[i].x+dir*1.55f;
        float before=m.actors[i].x;
        for(unsigned f=0;f<240;++f)football_live_tick(&m,17,(FootballLiveInput){.receiver=-1});
        float gain=(m.actors[i].x-before)*dir;
        CHECK(gain>10 && (m.actors[i].x-m.actors[d].x)*dir>=FB_BODY_DIAMETER-.01f,"fast receiver pinned: side%u gain%.2f relative%.2f",away,gain,(m.actors[i].x-m.actors[d].x)*dir);
    }
    for(unsigned away=0;away<2;++away){
        setup(away,FB_PASS_VERTICALS);m.thrown=false;
        unsigned off=away?11:0,def=11-off;float dir=away?-1:1;
        const float lane[4]={14,20,28,34};
        for(unsigned d=0;d<4;++d){m.actors[def+d].x=m.los+dir*2;m.actors[def+d].y=lane[d];}
        unsigned seen=0;
        for(unsigned i=off+6;i<off+11;++i){unsigned target=football_live_block_target(&m,i);
            if(target>=22)continue;
            CHECK(!(seen&(1u<<(target-def))),"duplicate pass-protection assignment side%u defender%u",away,target-def);
            seen|=1u<<(target-def);
        }
        CHECK((seen&15)==15,"edge rusher left without protection side%u mask%u",away,seen);
        /* Contact must honor the assignment too, rather than grabbing a nearer DT on the way to the edge. */
        m.actors[def+1].x=m.actors[off+6].x+dir;m.actors[def+1].y=m.actors[off+6].y;
        CHECK(football_live_block_target(&m,off+6)==def,"edge assignment before contact");
        football_live_physics(&m);
        CHECK(m.actors[off+6].block_peer==255,"pass blocker abandoned edge assignment for an incidental contact");
    }
    printf("football movement regressions: %u checks, %u failures\n",checks,failed);return failed?1:0;
}
