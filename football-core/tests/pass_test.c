#include "football_pass.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static FootballAttributes rating(unsigned v){return (FootballAttributes){v,v,v,v,v,v,v,v};}
static FootballPassReport resolve(FootballPassInput input)
{
    FootballPassReport r;assert(football_pass_resolve(&r,&input,99,0,50));return r;
}
int main(void)
{
    FootballPassInput in={rating(60),rating(60),rating(60),rating(60),FB_PASS_SLANTS,FB_DEF_MAN,0};
    FootballPassReport base=resolve(in),r=base;
    assert(!football_pass_resolve(&r,&in,100,0,0) && !memcmp(&r,&base,sizeof r));
    FootballPassInput bad=in;bad.play=FB_RUN_DIVE;
    assert(!football_pass_resolve(&r,&bad,0,0,0));bad=in;bad.defense=FB_DEF_COUNT;
    assert(!football_pass_resolve(&r,&bad,0,0,0));bad=in;bad.protection=13;
    assert(!football_pass_resolve(&r,&bad,0,0,0));bad=in;bad.receiver.cth=0;
    assert(!football_pass_resolve(&r,&bad,0,0,0) && !memcmp(&r,&base,sizeof r));
    in.quarterback.tha=99;r=resolve(in);assert(r.completion_chance>base.completion_chance);
    in.quarterback.tha=60;in.receiver.spd=in.receiver.agi=99;r=resolve(in);
    assert(r.separation>base.separation && r.completion_chance>base.completion_chance);
    in.receiver=rating(60);in.coverer.spd=in.coverer.agi=99;r=resolve(in);
    assert(r.separation<base.separation && r.completion_chance<base.completion_chance);
    in.coverer=rating(60);in.protection=12;FootballPassReport protected=resolve(in);
    in.protection=-12;FootballPassReport pressured=resolve(in);
    assert(protected.pressure<pressured.pressure && protected.sack_chance<pressured.sack_chance);
    in.protection=0;in.play=FB_PASS_VERTICALS;in.quarterback.thp=1;FootballPassReport weak=resolve(in);
    in.quarterback.thp=99;FootballPassReport strong=resolve(in);
    assert(strong.completion_chance>weak.completion_chance && strong.depth_yards>base.depth_yards);
    assert(strong.release_ms>base.release_ms);
    const FootballPlayId plays[]={FB_PASS_SLANTS,FB_PASS_PLAY_ACTION,FB_PASS_VERTICALS,FB_PASS_POST};
    for(unsigned p=0;p<4;++p)for(unsigned d=0;d<FB_DEF_COUNT;++d) {
        in=(FootballPassInput){rating(60),rating(60),rating(60),rating(60),plays[p],(FootballDefenseId)d,0};
        unsigned counts[6]={0};
        for(unsigned pressure=0;pressure<100;++pressure)for(unsigned catch_roll=0;catch_roll<100;++catch_roll){
            assert(football_pass_resolve(&r,&in,pressure,catch_roll,50));
            assert(r.pressure<=99 && r.separation<=99 && r.completion_chance>=15 && r.completion_chance<=92);
            assert(r.resolved.kind<=FB_RESULT_SACK && r.resolved.yards>=-5 && r.resolved.yards<=40);
            counts[r.resolved.kind]++;
        }
        assert(counts[FB_RESULT_SACK]==100u*r.sack_chance);
        assert(counts[FB_RESULT_INTERCEPTION]==100u*r.interception_chance);
        assert(counts[FB_RESULT_COMPLETE]+counts[FB_RESULT_INCOMPLETE]+counts[FB_RESULT_SACK]+counts[FB_RESULT_INTERCEPTION]==10000);
    }
    puts("football passing: atomic guards, arm/accuracy, separation, coverage, protection, release depth and 120,000 probability cases passed");
}
