#include "football_progress_fixture.h"
#include "gc/data.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
/* How fast the live career levels its members: exactly GC_FOOTBALL_LEVELS_PER_WIN levels for each won round whatever the growth rate,
 * so that every level evolution has happened by the last of the seven games, and the move an evolution asks for is kept. */
double gc_level_total_exp(int level,int growth_rate); /* game-core's EXP table (internal.h) */
static gc_run scratch,other;
static gc_system profile;
static FootballCareer career;
static bool member_of(gc_run *run,unsigned species){return fb_member_of(run,&profile,species,"pace");}
static const pr_species *species_of(unsigned id){for(unsigned i=0;i<PR_SPECIES_COUNT;++i)if(pr_species_table[i].id==id)return &pr_species_table[i];return NULL;}
static int rate_of(const gc_run *run){const pr_species *sp=species_of((unsigned)run->party[0].b.species);return sp?sp->growth_rate:0;}
/* The evolution a species has left to make by level, move or other conditions alone (no item), as far as level 75: its target, or 0. */
static unsigned left_to_make(unsigned species)
{
    for(int i=0;i<gc_species_evo_count;++i)if(gc_species_evos[i].id==species)
        for(unsigned k=0;k<gc_species_evos[i].n;++k){
            const gc_evo *e=&gc_evo_data[gc_species_evos[i].start+k];
            if(!e->item && e->level<=75)return e->species;
        }
    return 0;
}
int main(void)
{
    football_test_unlock_all(&profile);
    const unsigned step=GC_FOOTBALL_LEVELS_PER_WIN;
    assert(step==10);

    /* ---- game-core: the award ---- */
    assert(member_of(&scratch,4)); /* Charmander */
    const char *why=NULL;
    assert(!gc_football_award_levels(&scratch,1,0,&why) && why && !gc_football_award_levels(&scratch,1,100,&why));
    assert(!gc_football_award_levels(&scratch,0,step,&why) && !gc_football_award_levels(&scratch,8,step,&why) && !gc_football_award_levels(NULL,1,step,&why));
    assert(scratch.party[0].b.level==5 && scratch.party[0].exp==gc_level_total_exp(5,rate_of(&scratch))); /* a refused award gives nothing */
    const unsigned friendship=scratch.party[0].b.friendship;
    assert(gc_football_award_levels(&scratch,1,step,&why) && scratch.party[0].b.friendship==friendship+GC_FOOTBALL_FRIENDSHIP_PER_WIN);
    assert(!gc_football_award_levels(&scratch,1,step,&why) && scratch.party[0].b.friendship==friendship+GC_FOOTBALL_FRIENDSHIP_PER_WIN); /* pending: not twice */
    assert(fb_drive(&scratch,NULL) && scratch.party[0].b.level==15);

    /* The season's schedule is left as it was: 2000 EXP in round 1, whatever it makes of the level. */
    assert(member_of(&scratch,4));
    const double before=scratch.party[0].exp;
    assert(gc_football_award_exp(&scratch,1,NULL) && fb_drive(&scratch,NULL));
    assert(scratch.party[0].exp-before==(double)GC_FOOTBALL_EXP_FACTOR && scratch.party[0].b.level!=15);

    /* Seven rounds, whatever the growth rate: 15, 25, ... 75, with the EXP exactly at the start of the level (nothing left over). */
    unsigned rates_seen=0;bool rate_done[6]={false};
    for(unsigned i=0;i<PR_SPECIES_COUNT && rates_seen<6;++i){
        const unsigned id=pr_species_table[i].id;
        if(pr_species_table[i].growth_rate>5 || rate_done[pr_species_table[i].growth_rate] || !member_of(&scratch,id))continue;
        rate_done[pr_species_table[i].growth_rate]=true;++rates_seen;
        for(unsigned round=1;round<=7;++round){
            assert(fb_win_round(&scratch,round,NULL));
            assert((unsigned)scratch.party[0].b.level==5+step*round);
            assert(scratch.party[0].exp==gc_level_total_exp(scratch.party[0].b.level,rate_of(&scratch)));
        }
    }
    assert(rates_seen==6);

    /* From anywhere in a level, +10 exactly; the wave's level cap and level 100 still hold. */
    assert(member_of(&scratch,4));
    scratch.party[0].b.level=30;scratch.party[0].exp=floor((gc_level_total_exp(30,rate_of(&scratch))+gc_level_total_exp(31,rate_of(&scratch)))/2);
    assert(gc_football_award_levels(&scratch,3,step,NULL) && fb_drive(&scratch,NULL) && scratch.party[0].b.level==40 && scratch.party[0].exp==gc_level_total_exp(40,rate_of(&scratch)));
    assert(member_of(&scratch,4)); /* an EXP that is not a whole number still reaches the level (the phase floors what it adds) */
    scratch.party[0].b.level=30;scratch.party[0].exp=gc_level_total_exp(30,rate_of(&scratch))+0.5;
    assert(gc_football_award_levels(&scratch,3,step,NULL) && fb_drive(&scratch,NULL) && scratch.party[0].b.level==40);
    assert(member_of(&scratch,4));
    scratch.party[0].b.level=10;scratch.party[0].exp=gc_level_total_exp(10,rate_of(&scratch));
    assert(gc_football_award_levels(&scratch,1,20,NULL) && fb_drive(&scratch,NULL) && scratch.party[0].b.level==16); /* round 1's cap */
    assert(member_of(&scratch,4));
    scratch.party[0].b.level=95;scratch.party[0].exp=gc_level_total_exp(95,rate_of(&scratch));
    assert(gc_football_award_levels(&scratch,7,step,NULL) && fb_drive(&scratch,NULL) && scratch.party[0].b.level==100);

    /* ---- every draftable species through the seven games ---- */
    unsigned total=0,empty=0;
    for(unsigned i=0;i<PR_SPECIES_COUNT;++i){
        const unsigned id=pr_species_table[i].id;
        if(!member_of(&scratch,id))continue;
        ++total;
        for(unsigned round=1;round<=7;++round){
            assert(fb_win_round(&scratch,round,&empty));
            assert((unsigned)scratch.party[0].b.level==5+step*round);
            /* Deino: Zweilous at 50, Hydreigon at 64 (the highest level evolution there is) */
            if(id==633){assert(scratch.party[0].b.species==(round<5?633:round<6?634:635));}
        }
        assert(!left_to_make(scratch.party[0].b.species)); /* nobody is left with a level evolution to make: see evolve_all_test.c for the ones football waives a condition for */
    }
    assert(total>=500);
    assert(empty==0); /* an evolution on offer is always one that can be taken */
    /* Their moves are why these evolve at all: Tangela's Ancient Power, Lickitung's Rollout, Girafarig's Twin Beam. */
    static const unsigned from[]={114,108,203,852,6211},to[]={465,463,981,853,904};
    for(unsigned k=0;k<5;++k){
        assert(member_of(&scratch,from[k]));
        for(unsigned round=1;round<=7;++round)assert(fb_win_round(&scratch,round,NULL));
        assert((unsigned)scratch.party[0].b.species==to[k]);
    }

    /* ---- which move is forgotten ---- */
    {
        pr_pokemon p;memset(&p,0,sizeof p);
        p.species=114;p.n_moves=4;  /* Tangela: Ancient Power (60) is the weakest of these, and Tangrowth needs it */
        p.moves[0]=MOVE_ID_ANCIENT_POWER;p.moves[1]=MOVE_ID_SLAM;p.moves[2]=MOVE_ID_GIGA_DRAIN;p.moves[3]=MOVE_ID_POWER_WHIP;
        assert(football_career_forget_slot(&p)==2); /* Giga Drain (75), not slot 0 */
        p.species=4;assert(football_career_forget_slot(&p)==0); /* a species that needs nothing loses the weakest, as ever */
        p.species=114;p.moves[1]=MOVE_ID_TACKLE;
        assert(football_career_forget_slot(&p)==1); /* a weaker move still goes first */
        assert(football_career_forget_slot(NULL)==-1);
        /* Eevee needs one Fairy move for Sylveon: the only one is kept, a second may go */
        int fairy[2]={-1,-1};unsigned found=0;
        for(unsigned m=1;m<PR_MOVE_COUNT && found<2;++m)if(pr_moves[m].type==POKEMON_TYPE_FAIRY && pr_moves[m].power>0 && pr_moves[m].power<70)fairy[found++]=(int)m;
        assert(found==2);
        memset(&p,0,sizeof p);p.species=133;p.n_moves=4;
        p.moves[0]=(uint16_t)fairy[0];p.moves[1]=MOVE_ID_SLAM;p.moves[2]=MOVE_ID_GIGA_DRAIN;p.moves[3]=MOVE_ID_POWER_WHIP;
        assert(football_career_forget_slot(&p)==2); /* not the fairy move, though it is the weakest */
        p.moves[1]=(uint16_t)fairy[1];
        assert(football_career_forget_slot(&p)==0 || football_career_forget_slot(&p)==1); /* two to choose from: the weaker of them */
    }

    /* ---- the career: the same ten levels for all twenty-two, through a checkpoint ---- */
    fb_career_of(&career,&scratch,&profile,633); /* Deino, a slow grower */
    for(unsigned round=1;round<=7;++round){
        fb_play_win(&career);
        for(unsigned i=0;i<22;++i)assert((unsigned)career.roster.mon[i].b.level==5+step*round);
        if(round==6)for(unsigned i=0;i<22;++i)assert(career.roster.mon[i].b.species==635); /* Hydreigon after the sixth game */
        if(round<7)assert(football_career_continue(&career));
    }
    static unsigned char bytes[FB_CAREER_SAVE_MAX];static FootballCareer loaded;
    size_t n=football_career_save(&career,bytes,sizeof bytes);assert(n && football_career_load(&loaded,&other,&profile,bytes,n));
    for(unsigned i=0;i<22;++i)assert(loaded.roster.mon[i].b.level==75 && loaded.roster.mon[i].b.species==635);

    printf("football pace: %u levels per won round for every growth rate (15..75), %u draftable species through seven games, every one fully evolved, no evolution on offer that cannot be taken; passed\n",step,total);
    return 0;
}
