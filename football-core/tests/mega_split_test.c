#include "football_progress_fixture.h"
#include "gc/data.h"
#include <stdio.h>
#include <stdlib.h>
/* Species with two Mega Stones (Mewtwonite X and Y, Charizardite X and Y, Raichunite X and Y, Absolite and Absolite Z, Garchompite and
 * Garchompite Z, Lucarionite and Lucarionite Z): the shop used to offer only the first of the two, every round (lorenzo 2026-11-08: "only
 * Mewtwonite X appears in the football shop, never the Y counterpart"). Both must come up, each for the form it makes. */
#define PLAYOFF_ROUNDS FB_CAREER_ROUNDS
static gc_run scratch,other,probe;
static gc_system profile;
static FootballCareer career,loaded;
static unsigned char bytes[FB_CAREER_SAVE_MAX];
typedef struct { const char *name;unsigned species;int first,second; } Split;
static const Split splits[]={
    {"Mewtwo",SPECIES_ID_MEWTWO,FORM_CHANGE_ITEM_MEWTWONITE_X,FORM_CHANGE_ITEM_MEWTWONITE_Y},
    {"Charizard",SPECIES_ID_CHARIZARD,FORM_CHANGE_ITEM_CHARIZARDITE_X,FORM_CHANGE_ITEM_CHARIZARDITE_Y},
    {"Raichu",26,FORM_CHANGE_ITEM_RAICHUNITE_X,FORM_CHANGE_ITEM_RAICHUNITE_Y},
    {"Absol",SPECIES_ID_ABSOL,FORM_CHANGE_ITEM_ABSOLITE,FORM_CHANGE_ITEM_ABSOLITE_Z},
    {"Garchomp",445,FORM_CHANGE_ITEM_GARCHOMPITE,FORM_CHANGE_ITEM_GARCHOMPITE_Z},
    {"Lucario",448,FORM_CHANGE_ITEM_LUCARIONITE,FORM_CHANGE_ITEM_LUCARIONITE_Z},
};
#define SPLITS (sizeof splits/sizeof splits[0])
/* A scratch run whose only member is this species, as the shop sees a roster member. */
static bool member(gc_run *run,unsigned species)
{
    if(!fb_member_of(run,&profile,4,"mega-split"))return false;
    run->party[0].b.species=(uint16_t)species;run->party[0].b.form=0;
    return true;
}
/* A career at round 1 whose twenty-two members are all this species, from a seed. */
static void career_of(unsigned species,unsigned seed)
{
    fb_career_of(&career,&scratch,&profile,4);
    career.seed=seed;
    for(unsigned i=0;i<22;++i){career.roster.mon[i].b.species=(uint16_t)species;career.roster.mon[i].b.form=0;assert(football_roster_refresh(&career.roster,i));}
}
int main(void)
{
    (void)fb_win_round; /* a fixture helper this test has no use for */
    football_test_unlock_all(&profile);

    /* ---- game-core: every stone a member can use, in catalog order ---- */
    for(unsigned s=0;s<SPLITS;++s){
        int items[GC_FOOTBALL_MAX_MEGA_ITEMS+1];
        assert(member(&probe,splits[s].species));
        assert(gc_football_mega_items(&probe,0,items,GC_FOOTBALL_MAX_MEGA_ITEMS)==2);
        assert(items[0]==splits[s].first && items[1]==splits[s].second);
        assert(gc_football_mega_item(&probe,0)==splits[s].first); /* the single-stone call keeps answering with the first */
        const pr_form *a=gc_football_mega_form(&probe,0,items[0]),*b=gc_football_mega_form(&probe,0,items[1]);
        assert(a && b && a!=b && a->form_index!=b->form_index && !strncmp(a->form_key,"mega",4) && !strncmp(b->form_key,"mega",4)); /* two different Megas */
        assert(gc_football_mega_items(&probe,0,items,1)==1 && items[0]==splits[s].first); /* the room given is respected */
        assert(gc_football_mega_items(&probe,0,items,0)==0 && gc_football_mega_items(&probe,0,NULL,2)==0 && gc_football_mega_items(NULL,0,items,2)==0 && gc_football_mega_items(&probe,3,items,2)==0);
        /* A member that holds one is offered none. */
        assert(gc_football_give_mega(&probe,0,splits[s].second) && gc_football_mega_items(&probe,0,items,GC_FOOTBALL_MAX_MEGA_ITEMS)==0 && gc_football_mega_item(&probe,0)==-1);
    }
    {
        /* One stone, or none: unchanged. */
        static const struct { unsigned species;int item; } single[]={{3,FORM_CHANGE_ITEM_VENUSAURITE},{9,FORM_CHANGE_ITEM_BLASTOISINITE},{94,FORM_CHANGE_ITEM_GENGARITE},{SPECIES_ID_MAWILE,FORM_CHANGE_ITEM_MAWILITE}};
        int items[GC_FOOTBALL_MAX_MEGA_ITEMS];
        for(unsigned i=0;i<sizeof single/sizeof single[0];++i){
            assert(member(&probe,single[i].species));
            assert(gc_football_mega_items(&probe,0,items,GC_FOOTBALL_MAX_MEGA_ITEMS)==1 && items[0]==single[i].item && gc_football_mega_item(&probe,0)==single[i].item);
        }
        assert(member(&probe,25));assert(gc_football_mega_items(&probe,0,items,GC_FOOTBALL_MAX_MEGA_ITEMS)==0 && gc_football_mega_item(&probe,0)==-1); /* Pikachu has none */
    }

    /* ---- the shop: both stones of a species come up, whoever the holder is ---- */
    unsigned total_a=0,total_b=0;
    for(unsigned s=0;s<SPLITS;++s){
        unsigned offered_a=0,offered_b=0;
        for(unsigned seed=1;seed<=24;++seed){
            career_of(splits[s].species,seed);
            fb_play_win(&career);
            for(unsigned offer=2;offer<2+FB_SHOP_STONES;++offer){
                const int target=football_career_stone_target(&career,offer),item=football_career_stone_item(&career,offer);
                assert(target>=0);
                assert(item==splits[s].first || item==splits[s].second);
                assert(member(&probe,splits[s].species) && gc_football_mega_form(&probe,0,item)); /* it is a stone that holder can use */
                if(item==splits[s].first)++offered_a;else ++offered_b;
            }
        }
        printf("mega split: %-9s %2u offers of the first stone, %2u of the second\n",splits[s].name,offered_a,offered_b);
        /* Roughly half and half: a pick that never moves, or that always favours one, is the bug. */
        assert(offered_a+offered_b==48 && offered_a>=14 && offered_b>=14);
        total_a+=offered_a;total_b+=offered_b;
    }
    /* The same career in each round: the round moves the pick too (one seed, seven rounds, both stones). */
    {
        unsigned seen_a=0,seen_b=0;
        for(unsigned seed=1;seed<=6;++seed){
            career_of(SPECIES_ID_MEWTWO,seed);
            unsigned a=0,b=0;
            for(unsigned round=1;round<=PLAYOFF_ROUNDS;++round){
                fb_play_win(&career);
                for(unsigned offer=2;offer<2+FB_SHOP_STONES;++offer)if(football_career_stone_item(&career,offer)==FORM_CHANGE_ITEM_MEWTWONITE_X)++a;else ++b;
                if(round<PLAYOFF_ROUNDS)assert(football_career_continue(&career));
            }
            assert(a+b==2*PLAYOFF_ROUNDS);
            if(a&&b)++seen_a; /* a playoff that saw both */
            seen_b+=b;
        }
        assert(seen_a>=5 && seen_b>0); /* nearly every playoff meets both stones */
    }

    /* ---- Mewtwonite Y bought, worn, played and resumed ---- */
    {
        unsigned seed=1;int offer=-1;
        for(;seed<=24 && offer<0;++seed){
            career_of(SPECIES_ID_MEWTWO,seed);fb_play_win(&career);
            for(unsigned o=2;o<2+FB_SHOP_STONES;++o)if(football_career_stone_item(&career,o)==FORM_CHANGE_ITEM_MEWTWONITE_Y)offer=(int)o;
        }
        assert(offer>=0);
        const int target=football_career_stone_target(&career,(unsigned)offer);
        career.wallet=5000;
        const unsigned wallet=career.wallet;
        assert(football_career_buy(&career,(unsigned)offer,0) && career.roster.mega_item[target]==FORM_CHANGE_ITEM_MEWTWONITE_Y && career.wallet+career.prices[offer]==wallet);
        assert(!football_career_buy(&career,(unsigned)offer,0)); /* once */
        assert(member(&probe,SPECIES_ID_MEWTWO));
        const pr_form *y=gc_football_mega_form(&probe,0,FORM_CHANGE_ITEM_MEWTWONITE_Y),*x=gc_football_mega_form(&probe,0,FORM_CHANGE_ITEM_MEWTWONITE_X);
        FootballPlayer expected,other_mega;
        assert(y && x && football_stats_generate(&expected,career.roster.mon[target].b.id,y,&career.roster.maxima) && football_stats_generate(&other_mega,career.roster.mon[target].b.id,x,&career.roster.maxima));
        assert(memcmp(&expected.attributes,&other_mega.attributes,sizeof expected.attributes)); /* Mega Y is not Mega X: the stats say which one was played */
        size_t n=football_career_save(&career,bytes,sizeof bytes);
        assert(n && football_career_load(&loaded,&other,&profile,bytes,n) && loaded.roster.mega_item[target]==FORM_CHANGE_ITEM_MEWTWONITE_Y);
        assert(football_career_continue(&career) && career.phase==FB_CAREER_READY);
        career.mega=target;
        assert(football_career_start(&career));
        unsigned active=0;
        for(unsigned i=0;i<22;++i)if(career.live.actors[i].home && career.live.actors[i].roster==(unsigned)target){
            ++active;
            assert(!memcmp(&career.live.actors[i].attributes,&expected.attributes,sizeof expected.attributes) || !memcmp(&career.live.units[FB_HOME][target],&expected.attributes,sizeof expected.attributes));
        }
        assert(!memcmp(&career.live.units[FB_HOME][target],&expected.attributes,sizeof expected.attributes) && active<=1);
    }
    printf("football mega split stones: %u species with two stones, %u offers of the first and %u of the second over the sweep, Mewtwonite Y bought, played and resumed passed\n",(unsigned)SPLITS,total_a,total_b);
    return 0;
}
