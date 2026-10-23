#include "football_contact.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static FootballAttributes rating(unsigned v)
{
    return (FootballAttributes){v,v,v,v,v,v,v,v};
}
int main(void)
{
    FootballAttributes weak=rating(20),strong=rating(90),equal=rating(60);
    FootballBlockResult block={0},saved_block=block;
    FootballContactResult hit={0},saved_hit=hit;
    assert(!football_contact_block(&block,NULL,&equal,0));
    assert(!football_contact_block(&block,&equal,&equal,100));
    assert(!memcmp(&block,&saved_block,sizeof block));
    assert(!football_contact_tackle(&hit,&equal,&equal,false,100,0));
    FootballAttributes invalid=equal;invalid.sta=0;
    assert(!football_contact_tackle(&hit,&invalid,&equal,false,0,0));
    assert(!memcmp(&hit,&saved_hit,sizeof hit));
    assert(football_contact_block(&block,&strong,&weak,0));
    assert(block.pancake && block.edge>0 && block.hold_ms>700);
    assert(football_contact_block(&block,&weak,&strong,99));
    assert(!block.pancake && block.edge<0 && block.hold_ms<700);
    assert(football_contact_tackle(&hit,&equal,&equal,false,99,0));
    assert(hit.broken && hit.extra_yards>0 && !hit.fumble); /* A missed tackle cannot jar the ball loose. */
    assert(football_contact_tackle(&hit,&equal,&equal,false,0,0));
    assert(!hit.broken && hit.fumble && !hit.extra_yards);
    FootballContactResult reception;
    assert(football_contact_tackle(&reception,&equal,&equal,true,0,99));
    assert(reception.tackle_chance>hit.tackle_chance && !reception.fumble);
    unsigned weak_breaks=0,strong_breaks=0,weak_fumbles=0,strong_fumbles=0;
    for (unsigned tackle=0;tackle<100;++tackle) for (unsigned fumble=0;fumble<100;++fumble) {
        assert(football_contact_tackle(&hit,&weak,&strong,false,tackle,fumble));
        weak_breaks+=hit.broken;weak_fumbles+=hit.fumble;
        assert(football_contact_tackle(&hit,&strong,&weak,false,tackle,fumble));
        strong_breaks+=hit.broken;strong_fumbles+=hit.fumble;
    }
    assert(strong_breaks>weak_breaks && weak_fumbles>strong_fumbles);
    for (unsigned c=1;c<=99;++c) for (unsigned d=1;d<=99;++d) {
        FootballAttributes carrier=rating(c),defender=rating(d);
        assert(football_contact_block(&block,&carrier,&defender,50));
        assert(block.hold_ms>=200 && block.hold_ms<=1600 && block.edge>=-12 && block.edge<=12);
        assert(football_contact_tackle(&hit,&carrier,&defender,c%2,50,50));
        assert(hit.tackle_chance>=25 && hit.tackle_chance<=95);
        assert(hit.impact>=1 && hit.impact<=99 && hit.extra_yards<=7);
        assert(!hit.broken || (!hit.fumble && hit.extra_yards>0));
    }
    puts("football contact: atomic guards, engagement/pancake, catch vulnerability, broken tackles, fumbles and all rating pairs passed");
}
