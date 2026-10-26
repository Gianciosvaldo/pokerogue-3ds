#include "football_career.h"
#include <stdio.h>
#include <string.h>
static void prepare(FootballCareer *c,unsigned member)
{
    char seed[20];snprintf(seed,sizeof seed,"%u-%u",c->seed,member);
    gc_football_new(c->scratch,c->profile,seed,c->seed+member);
    c->scratch->party[0]=c->roster.mon[member];c->scratch->n_party=1;
    c->scratch->party_order[0]=0;c->scratch->bt.party[0][0]=&c->scratch->party[0].b;c->scratch->bt.n_party[0]=1;c->scratch->screen.kind=GC_SCREEN_NONE;
}
bool football_career_new(FootballCareer *c,gc_run *scratch,gc_system *profile,unsigned seed)
{
    if(!c || !scratch)return false;
    memset(c,0,sizeof *c);c->scratch=scratch;c->profile=profile;c->seed=seed?seed:1;c->round=1;c->mega=-1;c->stone_target=-1;
    gc_football_new(scratch,profile,"football-live",c->seed);
    return football_roster_init(&c->roster);
}
bool football_career_start(FootballCareer *c)
{
    if(!c || (c->phase!=FB_CAREER_READY && c->phase!=FB_CAREER_LINEUP) || c->roster.count!=22)return false;
    if(!football_live_begin(&c->live,&c->roster,c->seed+c->round*104729u,c->round))return false;
    if(c->mega>=0 && c->mega<22 && c->roster.mega_item[c->mega]){
        prepare(c,(unsigned)c->mega);const pr_form *form=gc_football_mega_form(c->scratch,0,c->roster.mega_item[c->mega]);FootballPlayer p;
        if(form && football_stats_generate(&p,c->roster.mon[c->mega].b.id,form,&c->roster.maxima)){
            unsigned spd=p.attributes.spd+c->roster.speed[c->mega],str=p.attributes.str+c->roster.strength[c->mega];
            p.attributes.spd=(uint8_t)(spd>99?99:spd);p.attributes.str=(uint8_t)(str>99?99:str);c->live.units[0][c->mega]=p.attributes;
        }
    }
    c->phase=FB_CAREER_MATCH;return true;
}
static void shop(FootballCareer *c)
{
    c->phase=FB_CAREER_SHOP;memset(c->sold,0,sizeof c->sold);c->prices[0]=125+25*c->round;c->prices[1]=c->prices[0];c->prices[2]=1000;c->stone_target=-1;c->stone_item=0;
    if((c->seed+c->round*7)%4==0)for(unsigned i=0;i<22;++i){prepare(c,i);int item=gc_football_mega_item(c->scratch,0);if(item>0 && !c->roster.mega_item[i]){c->stone_target=(int)i;c->stone_item=item;break;}}
}
int football_career_forget_slot(const pr_pokemon *p)
{
    int slot=-1,weakest=0;
    if(!p)return -1;
    for(int i=0;i<PR_N_MOVES(p) && i<4;++i){
        unsigned id=PR_MOVES(p)[i];int power=id<PR_MOVE_COUNT && pr_moves[id].power>0?pr_moves[id].power:0;
        if(slot<0 || power<weakest){slot=i;weakest=power;}
    }
    return slot;
}
static bool award(FootballCareer *c)
{
    prepare(c,c->exp_member);return gc_football_award_exp(c->scratch,c->round,NULL);
}
void football_career_tick(FootballCareer *c,unsigned ms,FootballLiveInput input)
{
    if(!c)return;
    if(c->phase==FB_CAREER_MATCH){football_live_tick(&c->live,ms,input);if(c->live.phase==FB_LIVE_END)c->phase=FB_CAREER_RESULT;return;}
    if(c->phase!=FB_CAREER_EXP)return;
    gc_event event;while(gc_run_next_event(c->scratch,&event)){}
    gc_football_progress progress=gc_football_tick(c->scratch);
    while(gc_run_next_event(c->scratch,&event)){}
    if(progress==GC_FB_ERROR){c->phase=FB_CAREER_ERROR;snprintf(c->message,sizeof c->message,"Team progression failed; checkpoint preserved.");}
    else if(progress==GC_FB_WAIT){
        if(c->scratch->screen.kind==GC_SCREEN_EVOLVE){c->evolution_count=(unsigned)gc_football_evolution_choices(c->scratch,c->evolutions);c->phase=FB_CAREER_EVOLVE;}
        else if(c->scratch->screen.kind==GC_SCREEN_LEARN_MOVE){
            /* Moves play no part in a match, so a level-up move is learned without asking. */
            gc_choice choice={.kind=GC_CH_FORGET,.a=football_career_forget_slot(&c->scratch->party[c->scratch->screen.party].b)};
            if(!gc_football_choose(c->scratch,&choice,0,NULL))c->phase=FB_CAREER_ERROR;
        }
        else {gc_choice choice={.kind=GC_CH_CONFIRM};if(!gc_football_choose(c->scratch,&choice,0,NULL))c->phase=FB_CAREER_ERROR;}
    }else if(progress==GC_FB_DONE){
        c->roster.mon[c->exp_member]=c->scratch->party[0];football_roster_refresh(&c->roster,c->exp_member);
        if(++c->exp_member<22){if(!award(c))c->phase=FB_CAREER_ERROR;}else shop(c);
    }
}
bool football_career_continue(FootballCareer *c)
{
    if(!c)return false;
    if(c->phase==FB_CAREER_RESULT){
        if(c->live.rules.winner!=FB_HOME){c->phase=FB_CAREER_END;return true;}
        ++c->wins;c->wallet+=400+100*c->round;c->exp_member=0;c->phase=FB_CAREER_EXP;
        if(!award(c)){c->phase=FB_CAREER_ERROR;return false;}return true;
    }
    if(c->phase==FB_CAREER_SHOP){if(c->round==7)c->phase=FB_CAREER_END;else {++c->round;c->mega=-1;c->phase=FB_CAREER_READY;}return true;}
    return false;
}
bool football_career_evolve(FootballCareer *c,int pick)
{
    if(!c || c->phase!=FB_CAREER_EVOLVE || pick>=(int)c->evolution_count)return false;
    gc_choice choice={.kind=pick<0?GC_CH_CANCEL:GC_CH_EVOLVE};
    if(!gc_football_choose(c->scratch,&choice,pick<0?0:(unsigned)pick,NULL))return false;
    c->phase=FB_CAREER_EXP;return true;
}
bool football_career_buy(FootballCareer *c,unsigned offer,unsigned member)
{
    if(!c || c->phase!=FB_CAREER_SHOP || offer>2 || member>=22 || c->sold[offer] || c->wallet<c->prices[offer])return false;
    if(offer==2){if(c->stone_target<0)return false;c->roster.mega_item[c->stone_target]=(int16_t)c->stone_item;}
    else {uint8_t *boost=offer==0?&c->roster.speed[member]:&c->roster.strength[member];if(*boost>=20)return false;*boost=(uint8_t)(*boost+2>20?20:*boost+2);football_roster_refresh(&c->roster,member);}
    c->wallet-=c->prices[offer];c->sold[offer]=true;return true;
}
