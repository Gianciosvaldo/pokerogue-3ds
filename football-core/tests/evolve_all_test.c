#include "football_progress_fixture.h"
#include "gc/data.h"
#include <stdio.h>
#include <stdlib.h>
/* The level evolutions football waives a condition for: the ones a member has no way to meet (weather, a party type, a treasure count, a
 * gender it was not drafted with, a move it missed) happen by their level, and nothing else changes: not who evolves, not when, not what
 * into, and not a classic run. */
double gc_level_total_exp(int level,int growth_rate); /* game-core's EXP table (internal.h) */
static gc_run scratch,classic;
static gc_system profile;
static FootballCareer career,loaded;
static gc_run other;
static unsigned char bytes[FB_CAREER_SAVE_MAX];
static bool member_of(gc_run *run,unsigned species){return fb_member_of(run,&profile,species,"evolve-all");}
static int rate_of(unsigned species){for(unsigned i=0;i<PR_SPECIES_COUNT;++i)if(pr_species_table[i].id==species)return pr_species_table[i].growth_rate;return 0;}
/* The member, already at this level with the EXP for it. */
static void at_level(gc_run *run,unsigned level){run->party[0].b.level=(int)level;run->party[0].exp=gc_level_total_exp((int)level,rate_of((unsigned)run->party[0].b.species));}
static bool knows(const gc_run *run,unsigned move){const pr_pokemon *p=&run->party[0].b;for(int i=0;i<PR_N_MOVES(p) && i<4;++i)if(PR_MOVES(p)[i]==move)return true;return false;}
static void forget(gc_run *run,unsigned move){pr_pokemon *p=&run->party[0].b;for(int i=0;i<PR_N_MOVES(p) && i<4;++i)if(PR_MOVES(p)[i]==move)PR_MOVES(p)[i]=MOVE_ID_TACKLE;}
/* What the member has become after each of the seven won rounds. */
static void seven(gc_run *run,unsigned out[7]){for(unsigned round=1;round<=7;++round){assert(fb_win_round(run,round,NULL));out[round-1]=(unsigned)run->party[0].b.species;}}
/* The seven rounds of a species, which must be one of the two rows (a form that depends on the time of day the seed gives is either). */
static void expect_either(unsigned species,int gender,const unsigned a[7],const unsigned b[7])
{
    unsigned got[7];
    assert(member_of(&scratch,species));
    if(gender>=0)scratch.party[0].b.gender=gender;
    seven(&scratch,got);
    if(memcmp(got,a,sizeof got) && memcmp(got,b,sizeof got)){
        fprintf(stderr,"species %u gender %d: %u %u %u %u %u %u %u\n",species,gender,got[0],got[1],got[2],got[3],got[4],got[5],got[6]);
        assert(0);
    }
}
static void expect(unsigned species,int gender,const unsigned want[7]){expect_either(species,gender,want,want);}
/* A run that is not football's: the same member and the same award, but the classic rules. */
static void classic_new(gc_run *run)
{
    gc_run_new(run,GAME_MODES_CLASSIC,"classic",5);
    run->sys=&profile;run->money=0;run->bt.engine=&run->eng;run->bt.game_mode=GAME_MODES_CLASSIC;run->bt.challenge_flags_known=true;
}
int main(void)
{
    football_test_unlock_all(&profile);

    /* ---- the species that could never evolve here: every one, whichever gender it was drafted as ---- */
    static const struct { unsigned from; const char *what; unsigned want[7]; } stuck[]={
        {56,"Mankey -> Primeape -> Annihilape: Rage Fist at 35 is passed while it is still a Mankey",{56,56,57,979,979,979,979}},
        {220,"Swinub -> Piloswine -> Mamoswine: Ancient Power is a relearn move",{220,220,221,473,473,473,473}},
        {415,"Combee -> Vespiquen: female only",{415,416,416,416,416,416,416}},
        {757,"Salandit -> Salazzle: female only",{757,757,758,758,758,758,758}},
        {674,"Pancham -> Pangoro: a Dark type in the party",{674,674,675,675,675,675,675}},
        {999,"Gimmighoul -> Gholdengo: 999 treasures",{1000,1000,1000,1000,1000,1000,1000}},
    };
    for(unsigned k=0;k<sizeof stuck/sizeof stuck[0];++k)for(int gender=0;gender<2;++gender){
        expect(stuck[k].from,gender,stuck[k].want);
    }

    /* Goomy -> Sliggoo -> Goodra: Goodra asks for rain; which Sliggoo and Goodra (Hisuian or not) is the time of day's doing */
    for(int gender=0;gender<2;++gender){
        static const unsigned plain[7]={704,704,704,705,706,706,706},hisuian[7]={704,704,704,6705,6706,6706,6706};
        expect_either(704,gender,plain,hisuian);
    }

    /* ---- everything that evolved before does so in the same round, into the same species ---- */
    static const struct { unsigned from; int gender; unsigned want[7]; } same[]={
        {114,-1,{114,114,114,465,465,465,465}},   /* Tangela: Ancient Power is learned at 33, which the third round passes: Tangrowth after it */
        {108,-1,{108,108,108,463,463,463,463}},   /* Lickitung: Rollout at 33 */
        {203,-1,{203,203,203,981,981,981,981}},
        {6211,-1,{6211,6211,6211,904,904,904,904}},
        {761,-1,{761,762,762,763,763,763,763}},   /* Bounsweet: Stomp at 23 for Tsareena */
        {412,0,{412,414,414,414,414,414,414}},    /* Burmy: Mothim if male ... */
        {412,1,{412,413,413,413,413,413,413}},    /* ... Wormadam if female: a gender it has is never waived while another evolution is open */
        {290,-1,{290,291,291,291,291,291,291}},
        {236,-1,{236,106,106,106,106,106,106}},
        {133,-1,{133,133,133,133,700,700,700}},
        {633,-1,{633,633,633,633,634,635,635}},
    };
    for(unsigned k=0;k<sizeof same/sizeof same[0];++k)expect(same[k].from,same[k].gender,same[k].want);
    {   /* Wurmple: Silcoon then Beautifly, or Cascoon then Dustox, by a seeded roll of its own */
        static const unsigned silcoon[7]={266,267,267,267,267,267,267},cascoon[7]={268,269,269,269,269,269,269};
        expect_either(265,-1,silcoon,cascoon);
    }
    {   /* Mime Jr.: Mimic at 18, then the Mr. Mime the time of day gives: Galarian (and on to Mr. Rime) or Kanto */
        static const unsigned galarian[7]={439,439,4122,866,866,866,866},kanto[7]={439,439,122,122,122,122,122};
        expect_either(439,-1,galarian,kanto);
    }

    /* ---- a move: waived when it was missed, never for one still to come ---- */
    {
        const unsigned ancient_power=MOVE_ID_ANCIENT_POWER;
        /* Tangela at 25 learns Ancient Power on the way to 35 (at 33): that level-up does not evolve it, the next one does. */
        assert(member_of(&scratch,114));at_level(&scratch,25);forget(&scratch,ancient_power);
        assert(gc_football_award_levels(&scratch,3,10,NULL) && fb_drive(&scratch,NULL));
        assert(scratch.party[0].b.level==35 && scratch.party[0].b.species==114 && knows(&scratch,ancient_power));
        assert(gc_football_award_levels(&scratch,4,10,NULL) && fb_drive(&scratch,NULL) && scratch.party[0].b.species==465);
        /* At 35 without it (forgotten, or never taught) the level has gone by: the next level-up evolves it all the same. */
        assert(member_of(&scratch,114));at_level(&scratch,35);forget(&scratch,ancient_power);
        assert(!knows(&scratch,ancient_power));
        assert(gc_football_award_levels(&scratch,4,10,NULL) && fb_drive(&scratch,NULL) && scratch.party[0].b.species==465);
        /* At 25 it is still to come (33): nothing is waived. */
        assert(member_of(&scratch,114));at_level(&scratch,25);forget(&scratch,ancient_power);
        assert(gc_football_award_levels(&scratch,2,5,NULL) && fb_drive(&scratch,NULL) && scratch.party[0].b.level==30 && scratch.party[0].b.species==114);
    }

    /* ---- a run that is not football's evolves as it always did ---- */
    {
        static const struct { unsigned drafted,as,level,round,football,classic; } cases[]={
            {56,57,35,4,979,57},   /* Primeape without Rage Fist */
            {415,415,25,3,416,415},/* Combee, a Vespiquen only if it is female */
            {674,674,25,3,675,674},/* Pancham without a Dark type beside it */
            {999,999,5,1,1000,999},/* Gimmighoul without its treasure */
            {4,4,25,3,5,5},        /* Charmander: an evolution that works is the same in both */
        };
        for(unsigned k=0;k<sizeof cases/sizeof cases[0];++k){
            gc_run *runs[2]={&scratch,&classic};
            for(unsigned which=0;which<2;++which){
                gc_run *run=runs[which];
                if(which==0)assert(member_of(run,cases[k].drafted));
                else {gc_starter s;assert(football_roster_starter(&profile,cases[k].drafted,&s));classic_new(run);assert(gc_football_create_member(run,&s,NULL));}
                run->party[0].b.species=(int)cases[k].as;run->party[0].b.gender=0;
                at_level(run,cases[k].level);forget(run,MOVE_ID_RAGE_FIST);
                assert(gc_football_award_levels(run,cases[k].round,10,NULL) && fb_drive(run,NULL));
                const unsigned want=which==0?cases[k].football:cases[k].classic;
                if((unsigned)run->party[0].b.species!=want)fprintf(stderr,"%s %u: became %d, wanted %u\n",which==0?"football":"classic",cases[k].drafted,(int)run->party[0].b.species,want);
                assert((unsigned)run->party[0].b.species==want);
            }
        }
    }

    /* ---- the career: all twenty-two, through the evolution prompt and a checkpoint ---- */
    fb_career_of(&career,&scratch,&profile,56); /* Mankey */
    for(unsigned round=1;round<=4;++round){
        fb_play_win(&career);
        for(unsigned i=0;i<22;++i)assert(career.roster.mon[i].b.species==(round<3?56:round==3?57:979));
        if(round<4)assert(football_career_continue(&career));
    }
    {
        size_t n=football_career_save(&career,bytes,sizeof bytes);
        assert(n && football_career_load(&loaded,&other,&profile,bytes,n));
        for(unsigned i=0;i<22;++i)assert(loaded.roster.mon[i].b.species==979);
    }

    printf("football evolve-all: %zu species that could not evolve now do by their level (both genders), %zu unchanged ones still on their own rounds, missed moves waived and moves to come not, classic runs untouched, career and checkpoint passed\n",sizeof stuck/sizeof stuck[0]+1,sizeof same/sizeof same[0]+2);
    return 0;
}
