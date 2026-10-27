#ifndef FOOTBALL_PROGRESS_FIXTURE_H
#define FOOTBALL_PROGRESS_FIXTURE_H
#include "football_career.h"
#include "football_profile_fixture.h"
#include <assert.h>
#include <string.h>
/* Host fixture: a member in a scratch run, and the progression of a won round as the live career drives it. */
static bool fb_member_of(gc_run *run,gc_system *profile,unsigned species,const char *seed)
{
    gc_starter s;
    if(!football_roster_starter(profile,species,&s))return false;
    gc_football_new(run,profile,seed,species);
    return gc_football_create_member(run,&s,NULL);
}
/* Runs what a won round queued to its end, answering as the career does: the first evolution on offer, the move
 * football_career_forget_slot names forgotten. An evolution screen with nothing to pick is cancelled and counted. */
static bool fb_drive(gc_run *run,unsigned *empty_evolutions)
{
    gc_event event;unsigned guard=0;gc_football_progress progress;
    do{
        while(gc_run_next_event(run,&event)){}
        progress=gc_football_tick(run);
        if(progress==GC_FB_WAIT){
            gc_choice answer={.kind=GC_CH_CONFIRM,.a=-1};
            if(run->screen.kind==GC_SCREEN_EVOLVE){
                uint16_t choices[GC_FOOTBALL_MAX_EVOLUTIONS];
                if(gc_football_evolution_choices(run,choices)==0){
                    if(empty_evolutions)++*empty_evolutions;
                    answer.kind=GC_CH_CANCEL;
                }else answer.kind=GC_CH_EVOLVE;
            }else if(run->screen.kind==GC_SCREEN_LEARN_MOVE){
                answer.kind=GC_CH_FORGET;answer.a=football_career_forget_slot(&run->party[run->screen.party].b);
            }
            if(!gc_football_choose(run,&answer,0,NULL))return false;
        }
    }while(progress!=GC_FB_DONE && progress!=GC_FB_ERROR && ++guard<2000);
    return progress==GC_FB_DONE;
}
/* A won round for the member in the run, with the live career's award. */
static bool fb_win_round(gc_run *run,unsigned round,unsigned *empty_evolutions)
{
    return gc_football_award_levels(run,round,GC_FOOTBALL_LEVELS_PER_WIN,NULL) && fb_drive(run,empty_evolutions);
}
/* A career at round 1 whose twenty-two members are all of this species. */
static void fb_career_of(FootballCareer *c,gc_run *scratch,gc_system *profile,unsigned species)
{
    memset(c,0,sizeof *c);
    assert(football_career_new(c,scratch,profile,11));
    for(unsigned i=0;i<22;++i){
        assert(fb_member_of(scratch,profile,species,"career"));
        c->roster.mon[i]=scratch->party[0];c->roster.lineup[i]=(uint8_t)i;
    }
    c->roster.count=22;
    for(unsigned i=0;i<22;++i)assert(football_roster_refresh(&c->roster,i));
    football_roster_autofill(&c->roster);
    c->round=1;c->wins=0;
}
/* The career's current round, won: EXP, evolutions (the first on offer) and on into the shop. */
static void fb_play_win(FootballCareer *c)
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
#endif
