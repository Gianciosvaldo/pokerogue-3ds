#include "football_career.h"
#include "gc/data.h"
#include <assert.h>
#include "football_profile_fixture.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* Friendship from won rounds: the gain for each member, what it leaves alone, the friendship evolutions it brings about and the
 * checkpoint that keeps it. */
static gc_run scratch,other;
static gc_system profile;
static FootballCareer career,loaded;
static unsigned char bytes[FB_CAREER_SAVE_MAX];
static bool member_of(gc_run *run,unsigned species)
{
    gc_starter s;
    if(!football_roster_starter(&profile,species,&s))return false;
    gc_football_new(run,&profile,"friendship",species);
    return gc_football_create_member(run,&s,NULL);
}
/* Runs what a won round queued to its end, answering as the career does: the first evolution, the weakest move forgotten. */
static bool drive(gc_run *run)
{
    gc_event event;unsigned guard=0;gc_football_progress progress;
    do{
        while(gc_run_next_event(run,&event)){}
        progress=gc_football_tick(run);
        if(progress==GC_FB_WAIT){
            gc_choice answer={.kind=run->screen.kind==GC_SCREEN_EVOLVE?GC_CH_EVOLVE:run->screen.kind==GC_SCREEN_LEARN_MOVE?GC_CH_FORGET:GC_CH_CONFIRM,.a=-1};
            if(run->screen.kind==GC_SCREEN_LEARN_MOVE)answer.a=football_career_forget_slot(&run->party[run->screen.party].b);
            if(!gc_football_choose(run,&answer,0,NULL))return false;
        }
    }while(progress!=GC_FB_DONE && progress!=GC_FB_ERROR && ++guard<2000);
    return progress==GC_FB_DONE;
}
static bool win_round(gc_run *run,unsigned round){return gc_football_award_exp(run,round,NULL) && drive(run);}
static unsigned capped(unsigned v){return v>255?255:v;}
static unsigned need(unsigned base,unsigned threshold){return threshold<=base?0:(threshold-base+GC_FOOTBALL_FRIENDSHIP_PER_WIN-1)/GC_FOOTBALL_FRIENDSHIP_PER_WIN;}
static bool is_target(unsigned from,unsigned to)
{
    for(int i=0;i<gc_species_evo_count;++i)if(gc_species_evos[i].id==from)
        for(unsigned k=0;k<gc_species_evos[i].n;++k)if(gc_evo_data[gc_species_evos[i].start+k].species==to)return true;
    return false;
}
/* A career at a won match (the whole roster of this species), played through EXP, evolutions and into the shop. */
static void play_win(FootballCareer *c)
{
    c->live.rules.winner=FB_HOME;c->phase=FB_CAREER_RESULT;
    assert(football_career_continue(c));
    unsigned guard=0;
    while(c->phase!=FB_CAREER_SHOP && ++guard<10000){
        if(c->phase==FB_CAREER_EVOLVE)assert(football_career_evolve(c,0));
        else {assert(c->phase==FB_CAREER_EXP);football_career_tick(c,16,(FootballLiveInput){.receiver=-1});}
    }
    assert(c->phase==FB_CAREER_SHOP);
}
static void career_of(FootballCareer *c,unsigned species)
{
    memset(c,0,sizeof *c);
    assert(football_career_new(c,&scratch,&profile,11));
    for(unsigned i=0;i<22;++i){
        assert(member_of(&scratch,species));
        c->roster.mon[i]=scratch.party[0];c->roster.lineup[i]=(uint8_t)i;
    }
    c->roster.count=22;
    for(unsigned i=0;i<22;++i)assert(football_roster_refresh(&c->roster,i));
    football_roster_autofill(&c->roster);
    c->round=1;c->wins=0;
}
int main(void)
{
    football_test_unlock_all(&profile);

    /* ---- game-core: what a won round gives a member ---- */
    assert(GC_FOOTBALL_FRIENDSHIP_PER_WIN>0);
    assert(member_of(&scratch,4)); /* Charmander */
    const unsigned base=scratch.party[0].b.friendship,gain=GC_FOOTBALL_FRIENDSHIP_PER_WIN;
    const gc_starter_entry starter_before=profile.starters[4];
    assert(base>0 && base<200);
    assert(!gc_football_award_exp(&scratch,0,NULL) && !gc_football_award_exp(&scratch,8,NULL) && !gc_football_award_exp(NULL,1,NULL));
    assert(scratch.party[0].b.friendship==base); /* a refused round gives nothing */
    assert(gc_football_award_exp(&scratch,1,NULL) && scratch.party[0].b.friendship==base+gain);
    assert(!gc_football_award_exp(&scratch,1,NULL) && scratch.party[0].b.friendship==base+gain); /* ... nor does one asked for twice while it is pending */
    assert(drive(&scratch) && scratch.party[0].b.friendship==base+gain); /* the EXP and level-ups change nothing of it */
    for(unsigned round=2;round<=7;++round){
        assert(win_round(&scratch,round));
        assert(scratch.party[0].b.friendship==capped(base+round*gain));
    }
    /* The starter's candy is the football rules' business, not friendship's as in the classic game. */
    assert(profile.starters[4].friendship==starter_before.friendship && profile.starters[4].candy_count==starter_before.candy_count);

    /* It stops at 255 and never wraps. */
    assert(member_of(&scratch,4));scratch.party[0].b.friendship=(uint8_t)(256-gain); /* one more than fits */
    assert(win_round(&scratch,1) && scratch.party[0].b.friendship==255);
    assert(win_round(&scratch,2) && scratch.party[0].b.friendship==255);

    /* The six-player team gets it for every member, each from where it stood. */
    {
        gc_starter team[2];
        assert(football_roster_starter(&profile,4,&team[0]) && football_roster_starter(&profile,7,&team[1]));
        gc_football_new(&scratch,&profile,"friendship-team",5);
        const char *why=NULL;
        assert(gc_football_draft(&scratch,team,2,&why) && scratch.n_party==2);
        scratch.party[1].b.friendship=100;
        const unsigned first=scratch.party[0].b.friendship;
        assert(win_round(&scratch,1));
        assert(scratch.party[0].b.friendship==first+gain && scratch.party[1].b.friendship==100+gain);
    }

    /* ---- the sweep: every species whose evolution asks for friendship, from its draftable form, through seven won rounds ---- */
    bool seen[GC_MAX_SPECIES_ID+1];memset(seen,0,sizeof seen);
    unsigned swept=0,exact=0,earliest=99,latest=0;
    for(int i=0;i<gc_species_evo_count;++i){
        const unsigned id=gc_species_evos[i].id;
        int threshold=-1;bool pure=true;
        for(unsigned k=0;k<gc_species_evos[i].n;++k){
            const gc_evo *e=&gc_evo_data[gc_species_evos[i].start+k];
            for(unsigned c=0;c<e->n_cond;++c)if(gc_evo_conds[e->cond_start+c].key==GC_EVO_FRIENDSHIP){
                const int t=gc_evo_conds[e->cond_start+c].v[0];
                if(threshold<0 || t<threshold)threshold=t;
                pure&=e->n_cond==1;
            }
        }
        if(threshold<0 || id>GC_MAX_SPECIES_ID || seen[id] || !member_of(&scratch,id))continue; /* later forms are not draftable: members reach them by levels */
        seen[id]=true;
        const unsigned start=scratch.party[0].b.friendship,first=need(start,(unsigned)threshold);
        assert(start<(unsigned)threshold && first>=1);
        unsigned round=0;
        while(round<7 && scratch.party[0].b.species==id){
            ++round;
            assert(win_round(&scratch,round));
            assert(scratch.party[0].b.friendship==capped(start+round*gain));
            if(scratch.party[0].b.species==id)assert(round<first || !pure); /* a plain friendship evolution does not wait once the friendship is there */
        }
        assert(scratch.party[0].b.species!=id); /* the champion's run sees it */
        assert(is_target(id,scratch.party[0].b.species));
        assert(round>=first); /* ... and never before the friendship is */
        if(pure){assert(round==first);++exact;}
        if(round<earliest)earliest=round;
        if(round>latest)latest=round;
        ++swept;
    }
    assert(swept>=15 && exact>=10);
    assert(earliest>=2 && latest<=7); /* none in the first round, all by the last */

    /* ---- the career: all twenty-two members, from the result of a won match ---- */
    career_of(&career,175); /* Togepi: friendship 70 for Togetic */
    const unsigned togepi=career.roster.mon[0].b.friendship;
    assert(togepi+gain<70 && togepi+2*gain>=70);
    play_win(&career);
    assert(career.wins==1);
    for(unsigned i=0;i<22;++i)assert(career.roster.mon[i].b.friendship==togepi+gain && career.roster.mon[i].b.species==175);
    size_t n=football_career_save(&career,bytes,sizeof bytes);assert(n && football_career_load(&loaded,&other,&profile,bytes,n));
    for(unsigned i=0;i<22;++i)assert(loaded.roster.mon[i].b.friendship==togepi+gain); /* the checkpoint keeps it */
    assert(football_career_continue(&career) && career.round==2);
    play_win(&career);
    assert(career.wins==2);
    for(unsigned i=0;i<22;++i)assert(career.roster.mon[i].b.friendship==togepi+2*gain && career.roster.mon[i].b.species==176); /* all Togetic now */
    n=football_career_save(&career,bytes,sizeof bytes);assert(n && football_career_load(&loaded,&other,&profile,bytes,n));
    for(unsigned i=0;i<22;++i)assert(loaded.roster.mon[i].b.friendship==togepi+2*gain && loaded.roster.mon[i].b.species==176);

    /* A lost match gives nothing. */
    career_of(&career,175);
    career.live.rules.winner=FB_AWAY;career.phase=FB_CAREER_RESULT;
    assert(football_career_continue(&career) && career.phase==FB_CAREER_END);
    for(unsigned i=0;i<22;++i)assert(career.roster.mon[i].b.friendship==togepi);

    printf("football friendship: gain per won round, no starter candy, cap, six-player team, career and checkpoint passed; %u friendship evolutions swept (%u exactly on the round the friendship allows), first in round %u, last in round %u\n",swept,exact,earliest,latest);
    return 0;
}
