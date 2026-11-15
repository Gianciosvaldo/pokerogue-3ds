#include "football_progress_fixture.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* The football shop sells all six vitamins, and each can be bought FB_VITAMIN_LIMIT (3) times a round (lorenzo 2026-11-15: "a limit of 3x boosters
 * per round... the other stat boosting vitamins should be added as well"). Before it only Carbos and Protein were on sale, once each. */
static gc_run scratch,other;
static gc_system profile;
static FootballCareer career,loaded;
static unsigned char bytes[FB_CAREER_SAVE_MAX];
static const unsigned VITAMINS[6]={0,1,4,5,6,7};
static const char *const NAMES[6]={"Carbos","Protein","HP Up","Iron","Calcium","Zinc"};
static const FootballBoost KINDS[6]={FB_BOOST_SPD,FB_BOOST_STR,FB_BOOST_STA,FB_BOOST_TAK,FB_BOOST_THA,FB_BOOST_CTH};
static unsigned attribute(unsigned member,FootballBoost kind){return *football_attribute_of(&career.roster.player[member].attributes,kind);}
static uint32_t crc(const unsigned char *p,size_t n){uint32_t v=~0u;while(n--){v^=*p++;for(unsigned i=0;i<8;++i)v=(v>>1)^(0xedb88320u & (0u-(v&1)));}return ~v;}
static void put32(size_t at,uint32_t v){for(unsigned i=0;i<4;++i)bytes[at+i]=(unsigned char)(v>>(8*i));}
static void reseal(size_t length,unsigned version){put32(8,version);put32(12,(uint32_t)length);put32(16,crc(bytes+20,length-20));}
/* A career in the shop after round 1, with plenty of cash. */
static void shop_of(unsigned seed)
{
    fb_career_of(&career,&scratch,&profile,4);career.seed=seed;
    fb_play_win(&career);career.wallet=20000;
}
int main(void)
{
    (void)fb_win_round; /* a fixture helper this test has no use for */
    football_test_unlock_all(&profile);
    shop_of(7);

    /* ---- the offers: six vitamins at the same price, the two stones and the rest are not vitamins ---- */
    for(unsigned k=0;k<6;++k){
        assert(football_career_offer_boost(VITAMINS[k])==(int)KINDS[k] && !strcmp(football_career_offer_name(VITAMINS[k]),NAMES[k]));
        assert(career.prices[VITAMINS[k]]==FB_CAREER_VITAMIN_PRICE(career.round) && career.bought[VITAMINS[k]]==0 && !career.sold[VITAMINS[k]]);
        assert(football_career_vitamins_left(&career,VITAMINS[k])==FB_VITAMIN_LIMIT);
    }
    assert(football_career_offer_boost(2)<0 && football_career_offer_boost(3)<0 && football_career_offer_boost(FB_SHOP_FIXED)<0);
    assert(football_career_vitamins_left(&career,2)==0 && football_career_vitamins_left(NULL,0)==0);
    assert(FB_VITAMIN_LIMIT==3);

    /* ---- each vitamin raises its own attribute by 2 (to 99 at most) and nothing else ---- */
    for(unsigned k=0;k<6;++k){
        const unsigned member=3+k;FootballAttributes before=career.roster.player[member].attributes;const unsigned wallet=career.wallet;
        assert(before.spd<90 && before.str<90 && before.sta<90 && before.tak<90 && before.tha<90 && before.cth<90);
        assert(football_career_buy(&career,VITAMINS[k],member));
        assert(*football_roster_boost(&career.roster,KINDS[k],member)==2 && career.bought[VITAMINS[k]]==1 && career.wallet==wallet-career.prices[VITAMINS[k]]);
        assert(attribute(member,KINDS[k])==(unsigned)*football_attribute_of(&before,KINDS[k])+2);
        for(unsigned other_kind=0;other_kind<6;++other_kind)if(other_kind!=k)assert(attribute(member,KINDS[other_kind])==(unsigned)*football_attribute_of(&before,KINDS[other_kind]));
        assert(career.roster.player[member].attributes.agi==before.agi && career.roster.player[member].attributes.thp==before.thp);
        char want[96];snprintf(want,sizeof want,"%s %u to %u (boost 2/%u)",football_boost_name(KINDS[k]),*football_attribute_of(&before,KINDS[k]),attribute(member,KINDS[k]),(unsigned)FB_BOOST_MAX);
        assert(strstr(career.message,want)); /* the purchase says what it did */
        assert(football_career_vitamins_left(&career,VITAMINS[k])==FB_VITAMIN_LIMIT-1 && !career.sold[VITAMINS[k]]);
    }

    /* ---- three of each a round, for one member or several, then no more ---- */
    shop_of(8);
    for(unsigned k=0;k<6;++k){
        const unsigned offer=VITAMINS[k],wallet=career.wallet;
        assert(football_career_buy(&career,offer,10) && football_career_buy(&career,offer,10) && football_career_buy(&career,offer,11)); /* two for one, one for another */
        assert(!football_career_buy(&career,offer,12) && !football_career_buy(&career,offer,10)); /* a fourth is refused for anyone */
        assert(career.bought[offer]==FB_VITAMIN_LIMIT && career.sold[offer] && football_career_vitamins_left(&career,offer)==0);
        assert(career.wallet==wallet-3*career.prices[offer]); /* the refused ones cost nothing */
        assert(*football_roster_boost(&career.roster,KINDS[k],10)==4 && *football_roster_boost(&career.roster,KINDS[k],11)==2 && *football_roster_boost(&career.roster,KINDS[k],12)==0);
    }
    /* A kind that is used up does not stop the others: each has its own count. */
    shop_of(9);
    for(unsigned i=0;i<3;++i)assert(football_career_buy(&career,0,0));
    assert(!football_career_buy(&career,0,0) && football_career_buy(&career,1,0) && football_career_buy(&career,5,0) && career.bought[1]==1 && career.bought[5]==1);

    /* ---- the other refusals ---- */
    shop_of(10);
    {
        const unsigned wallet=career.wallet;
        assert(!football_career_buy(&career,4,22) && !football_career_buy(&career,4,200) && career.wallet==wallet && career.bought[4]==0);
        career.wallet=career.prices[4]-1;assert(!football_career_buy(&career,4,0) && career.bought[4]==0 && career.wallet==career.prices[4]-1);
        career.wallet=career.prices[4];assert(football_career_buy(&career,4,0) && career.wallet==0);
        career.wallet=20000;
        /* A member holds 20 of a boost at most; the refused purchase is not counted or paid for. */
        career.roster.stamina[5]=18;assert(football_career_buy(&career,4,5) && career.roster.stamina[5]==20 && career.bought[4]==2);
        const unsigned after=career.wallet;
        assert(!football_career_buy(&career,4,5) && career.roster.stamina[5]==20 && career.bought[4]==2 && career.wallet==after);
        football_career_continue(&career);assert(!football_career_buy(&career,4,5)); /* not in the shop any more */
    }

    /* ---- a new round, a new shop ---- */
    shop_of(11);
    for(unsigned k=0;k<6;++k)for(unsigned i=0;i<3;++i)assert(football_career_buy(&career,VITAMINS[k],(unsigned)i));
    assert(football_career_continue(&career) && career.phase==FB_CAREER_READY && career.round==2);
    assert(football_career_start(&career));
    fb_play_win(&career);career.wallet=20000;
    for(unsigned k=0;k<6;++k){
        assert(career.bought[VITAMINS[k]]==0 && !career.sold[VITAMINS[k]] && career.prices[VITAMINS[k]]==FB_CAREER_VITAMIN_PRICE(career.round));
        assert(football_career_buy(&career,VITAMINS[k],0));
    }
    assert(career.roster.speed[0]==4 && career.roster.catching[0]==4 && career.roster.stamina[0]==4); /* the boosts of round one are still there */

    /* ---- a checkpoint in the shop keeps what was bought and the boosts of every kind ---- */
    shop_of(12);
    for(unsigned k=0;k<6;++k)for(unsigned i=0;i<=k%3;++i)assert(football_career_buy(&career,VITAMINS[k],2+k));
    {
        const size_t n=football_career_save(&career,bytes,sizeof bytes);assert(n);
        assert(football_career_load(&loaded,&other,&profile,bytes,n));
        for(unsigned k=0;k<6;++k){
            assert(loaded.bought[VITAMINS[k]]==career.bought[VITAMINS[k]] && loaded.sold[VITAMINS[k]]==career.sold[VITAMINS[k]] && loaded.prices[VITAMINS[k]]==career.prices[VITAMINS[k]]);
            for(unsigned m=0;m<22;++m)assert(*football_roster_boost(&loaded.roster,KINDS[k],m)==*football_roster_boost(&career.roster,KINDS[k],m));
        }
        assert(loaded.sold[0]==(career.bought[0]>=FB_VITAMIN_LIMIT) && loaded.wallet==career.wallet);
        for(unsigned m=0;m<22;++m)assert(!memcmp(&loaded.roster.player[m].attributes,&career.roster.player[m].attributes,sizeof(FootballAttributes)));
        assert(football_career_buy(&loaded,4,2) || loaded.bought[4]==FB_VITAMIN_LIMIT); /* and the shop goes on from there */

        /* A checkpoint of version 3 (before the other vitamins) loads: the Carbos and Protein it had sold count as one bought, and the new ones
         * are fresh and priced like them. Its vitamin block is 22 members of four boosts, six counts and four prices, at the end of a shop's. */
        const size_t tail=(22u*4u+6u+4u)*4u;
        FootballCareer *v3=calloc(1,sizeof *v3);assert(v3);*v3=career;
        v3->bought[0]=1;v3->sold[0]=true;v3->bought[1]=0;v3->sold[1]=false;
        const size_t m=football_career_save(v3,bytes,sizeof bytes);assert(m>tail+20);
        reseal(m-tail,3);
        assert(football_career_load(&loaded,&other,&profile,bytes,m-tail));
        assert(loaded.bought[0]==1 && !loaded.sold[0] && loaded.bought[1]==0 && football_career_vitamins_left(&loaded,0)==2);
        for(unsigned k=2;k<6;++k)assert(loaded.bought[VITAMINS[k]]==0 && !loaded.sold[VITAMINS[k]] && loaded.prices[VITAMINS[k]]==loaded.prices[0]);
        for(unsigned m2=0;m2<22;++m2)assert(loaded.roster.stamina[m2]==0 && loaded.roster.catching[m2]==0); /* the old save knew only two boosts */
        assert(football_career_buy(&loaded,0,0) && football_career_buy(&loaded,0,0) && !football_career_buy(&loaded,0,0));
        free(v3);

        /* A count above the limit, or a boost above 20, is a corrupt save, not a shop that sells a fourth. */
        const size_t full=football_career_save(&career,bytes,sizeof bytes);assert(full>tail+20);
        const size_t counts=full-(6u+4u)*4u; /* the six counts follow the boosts of 22 members */
        const unsigned saved=bytes[counts];
        bytes[counts]=FB_VITAMIN_LIMIT+1;reseal(full,4);assert(!football_career_load(&loaded,&other,&profile,bytes,full));
        bytes[counts]=(unsigned char)saved;reseal(full,4);assert(football_career_load(&loaded,&other,&profile,bytes,full));
        bytes[full-tail]=FB_BOOST_MAX+1;reseal(full,4);assert(!football_career_load(&loaded,&other,&profile,bytes,full));
    }
    printf("football vitamins: six vitamins, %u a round each, boosts of every kind, checkpoints before and after passed\n",(unsigned)FB_VITAMIN_LIMIT);
    puts("football vitamin tests passed");
    return 0;
}
