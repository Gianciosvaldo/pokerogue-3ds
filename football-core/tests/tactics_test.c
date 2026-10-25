#include "football_match.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static FootballTeam team(void)
{
    FootballTeam t={0};t.count=6;
    for(unsigned i=0;i<6;++i)t.players[i].attributes=(FootballAttributes){60,60,60,60,60,60,60,60};
    return t;
}
static void commit(FootballMatch *m){for(unsigned i=0;i<3;++i)assert(football_match_tick(m,250));}
int main(void)
{
    FootballTeam t=team();FootballMatch a,b,saved;
    unsigned kept=0,lost=0,runner_cost=0,dive_cost=0;
    for(unsigned seed=1;seed<=4000;++seed){
        assert(football_match_begin(&a,&t,&t,seed,256));a.manual_defense=true;b=a;saved=a;
        assert(!football_match_contact_choice(&a,FB_CONTACT_COUNT,false) && !memcmp(&a,&saved,sizeof a));
        assert(football_match_contact_choice(&b,FB_CONTACT_JUKE,false));
        PlayInfo play={FB_FORM_UNDER_CENTER,FB_PLAY_RUN,FB_RUN_DIVE,255};
        assert(football_match_call(&a,play) && football_match_call(&b,play));
        saved=a;assert(!football_match_contact_choice(&a,FB_CONTACT_JUKE,true) && !memcmp(&a,&saved,sizeof a));
        bool no_fumble=!a.contact.fumble && !b.contact.fumble;
        bool fumbled=a.contact.fumble,keeps=a.recovery.offense_keeps;
        if(fumbled){assert(a.recovery.attempted && a.recoverer<6);if(keeps)kept++;else lost++;}
        commit(&a);commit(&b);
        if(fumbled)assert(a.rules.last.turnover!=keeps);
        if(no_fumble){assert(a.energy[FB_HOME][a.carrier]==b.energy[FB_HOME][b.carrier]+1);runner_cost++;}
        assert(football_match_begin(&a,&t,&t,seed,256));a.manual_defense=true;a.rules.offense=FB_AWAY;b=a;
        assert(football_match_contact_choice(&b,FB_CONTACT_BALANCED,true));
        assert(football_match_defend(&a,FB_DEF_MAN) && football_match_defend(&b,FB_DEF_MAN));
        bool hit=a.contact.attempted && b.contact.attempted && !a.contact.fumble && !b.contact.fumble;
        commit(&a);commit(&b);
        if(hit){assert(a.energy[FB_HOME][a.tackler]==b.energy[FB_HOME][b.tackler]+2);dive_cost++;}
    }
    assert(kept && lost && runner_cost && dive_cost);
    puts("football tactics: atomic choices, offensive/defensive recoveries, rule commits and explicit juke/dive fatigue costs passed");
}
