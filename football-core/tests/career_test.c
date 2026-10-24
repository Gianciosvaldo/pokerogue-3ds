#include "football_career.h"
#include "gc/data.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static FootballCareer career,loaded;
static gc_run scratch,other;
static unsigned char bytes[FB_CAREER_SAVE_MAX];
static uint32_t crc(const unsigned char *p,size_t n){uint32_t v=~0u;while(n--){v^=*p++;for(unsigned i=0;i<8;++i)v=(v>>1)^(0xedb88320u & (0u-(v&1)));}return ~v;}
static void put32(unsigned offset,uint32_t value){for(unsigned i=0;i<4;++i)bytes[offset+i]=(unsigned char)(value>>(8*i));}
int main(void)
{
    assert(football_career_new(&career,&scratch,NULL,7));
    for(int i=0;i<gc_starter_cost_count && career.roster.count<22;++i){if(gc_starter_costs[i].cost>1)continue;gc_starter s={.species=gc_starter_costs[i].id,.tera_type=-1};football_roster_add(&career.roster,&scratch,&s,NULL,NULL);}
    assert(career.roster.count==22);football_roster_autofill(&career.roster);career.phase=FB_CAREER_LINEUP;
    size_t n=football_career_save(&career,bytes,sizeof bytes);assert(n);
    assert(football_career_load(&loaded,&other,NULL,bytes,n));assert(loaded.roster.count==22 && loaded.seed==career.seed);
    assert(!memcmp(career.roster.lineup,loaded.roster.lineup,22));
    /* Valid-CRC out-of-range count/index values must fail before indexing. */
    put32(44,255);put32(16,crc(bytes+20,n-20));assert(!football_career_load(&loaded,&other,NULL,bytes,n));
    n=football_career_save(&career,bytes,sizeof bytes);assert(n);
    put32(52,255);put32(16,crc(bytes+20,n-20));assert(!football_career_load(&loaded,&other,NULL,bytes,n));
    n=football_career_save(&career,bytes,sizeof bytes);assert(n);
    bytes[n-1]^=1;assert(!football_career_load(&loaded,&other,NULL,bytes,n));bytes[n-1]^=1;
    assert(!football_career_load(&loaded,&other,NULL,bytes,n-1));
    assert(football_career_start(&career));n=football_career_save(&career,bytes,sizeof bytes);assert(n && football_career_load(&loaded,&other,NULL,bytes,n));
    assert(loaded.live.rules.ball==career.live.rules.ball && loaded.live.rng==career.live.rng && loaded.live.actors[0].x==career.live.actors[0].x);
    assert(football_live_call(&career.live,FB_RUN_DIVE));assert(!football_career_save_ready(&career));
    /* Reach real shared EXP/evolution phases for all 22 members. */
    career.phase=FB_CAREER_RESULT;career.live.rules.winner=FB_HOME;assert(football_career_continue(&career));
    unsigned guard=0,choices=0;
    while(career.phase!=FB_CAREER_SHOP && ++guard<10000){
        if(career.phase==FB_CAREER_EVOLVE){assert(football_career_evolve(&career,0));choices++;}
        else if(career.phase==FB_CAREER_LEARN)assert(football_career_learn(&career,-1));
        else {assert(career.phase==FB_CAREER_EXP);football_career_tick(&career,16,(FootballLiveInput){.receiver=-1});}
    }
    assert(career.phase==FB_CAREER_SHOP && guard<10000 && career.wins==1 && career.wallet==500);
    for(unsigned i=0;i<22;++i)assert(career.roster.mon[i].b.level>5);
    assert(football_career_buy(&career,0,21));assert(!football_career_buy(&career,0,21));
    n=football_career_save(&career,bytes,sizeof bytes);assert(n && football_career_load(&loaded,&other,NULL,bytes,n));assert(loaded.roster.speed[21]==2 && loaded.sold[0]);
    assert(football_career_continue(&loaded) && loaded.round==2 && loaded.phase==FB_CAREER_READY);
    printf("football career: 22-member persistence, CRC/truncation rejection, safe checkpoints, shared EXP/evolution (%u choices) and shop conservation passed\n",choices);
}
