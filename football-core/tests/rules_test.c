#include "football_rules.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static void incomplete(FootballRules *r) { assert(football_rules_advance(r,(FootballResolvedPlay){FB_RESULT_INCOMPLETE,0,5})); }
int main(void)
{
    FootballRules r;
    assert(football_rules_begin(&r,FB_HOME));
    assert(r.offense==FB_HOME && r.ball==25 && r.down==1 && football_rules_distance(&r)==10);
    assert(football_rules_advance(&r,(FootballResolvedPlay){FB_RESULT_RUN,6,12}));
    assert(r.ball==31 && r.down==2 && football_rules_distance(&r)==4 && r.seconds_left==108);
    assert(football_rules_advance(&r,(FootballResolvedPlay){FB_RESULT_COMPLETE,4,12}));
    assert(r.ball==35 && r.down==1 && r.last.first_down && football_rules_distance(&r)==10);
    for (unsigned i=0;i<4;++i) incomplete(&r);
    assert(r.offense==FB_AWAY && r.ball==65 && r.down==1 && r.last.turnover);
    assert(football_rules_advance(&r,(FootballResolvedPlay){FB_RESULT_INTERCEPTION,8,5}));
    assert(r.offense==FB_HOME && r.ball==27 && r.last.turnover);
    assert(football_rules_advance(&r,(FootballResolvedPlay){FB_RESULT_COMPLETE,73,12}));
    assert(r.score[0]==7 && r.offense==FB_AWAY && r.ball==25 && r.last.touchdown);
    r.ball=2; r.first_down_at=12;
    assert(football_rules_advance(&r,(FootballResolvedPlay){FB_RESULT_SACK,-5,14}));
    assert(r.score[0]==9 && r.offense==FB_HOME && r.ball==25 && r.last.safety);
    r.ball=99; r.first_down_at=100;
    assert(football_rules_advance(&r,(FootballResolvedPlay){FB_RESULT_FUMBLE,2,5}));
    assert(r.offense==FB_AWAY && r.ball==25 && r.score[0]==9); /* Touchback, not a touchdown. */
    r.ball=1;r.first_down_at=11;
    assert(football_rules_advance(&r,(FootballResolvedPlay){FB_RESULT_FUMBLE,-1,5}));
    assert(r.score[0]==16 && r.offense==FB_AWAY && r.last.touchdown);
    FootballRules saved=r;
    assert(!football_rules_advance(&r,(FootballResolvedPlay){FB_RESULT_INCOMPLETE,5,5}));
    assert(!football_rules_advance(&r,(FootballResolvedPlay){FB_RESULT_SACK,1,5}));
    assert(!football_rules_advance(&r,(FootballResolvedPlay){FB_RESULT_RUN,1,0}));
    assert(memcmp(&saved,&r,sizeof r)==0);

    assert(football_rules_begin(&r,FB_HOME));
    r.seconds_left=1;
    incomplete(&r); assert(r.quarter==2 && r.seconds_left==120 && r.last.quarter_end);
    r.seconds_left=1;
    incomplete(&r); assert(r.quarter==3 && r.offense==FB_AWAY && r.ball==25 && r.last.halftime);
    r.quarter=4;r.seconds_left=1;r.score[1]=7;
    incomplete(&r); assert(r.finished && r.winner==FB_AWAY);
    saved=r; assert(!football_rules_advance(&r,(FootballResolvedPlay){FB_RESULT_RUN,1,5}));
    assert(memcmp(&saved,&r,sizeof r)==0);

    /* Scoring on the final play counts before checking regulation's winner. */
    assert(football_rules_begin(&r,FB_HOME));
    r.quarter=4;r.seconds_left=1;r.ball=99;r.first_down_at=100;
    assert(football_rules_advance(&r,(FootballResolvedPlay){FB_RESULT_RUN,1,10}));
    assert(r.finished && r.winner==FB_HOME && r.score[0]==7);

    assert(football_rules_begin(&r,FB_HOME));
    r.quarter=4;r.seconds_left=1;incomplete(&r);
    assert(!r.finished && r.quarter==5 && r.offense==FB_HOME && r.ball==75);
    assert(football_rules_advance(&r,(FootballResolvedPlay){FB_RESULT_RUN,25,10}));
    assert(!r.finished && r.offense==FB_AWAY); /* Both get a possession. */
    for (unsigned i=0;i<4;++i) incomplete(&r);
    assert(r.finished && r.winner==FB_HOME && r.score[0]==7);

    assert(football_rules_begin(&r,FB_AWAY));
    r.quarter=4;r.seconds_left=1;incomplete(&r);
    for (unsigned drive=0;drive<2*FB_OVERTIME_PAIRS;++drive) {
        for (unsigned down=0;down<4;++down) incomplete(&r);
        assert(r.finished==(drive+1==2*FB_OVERTIME_PAIRS));
    }
    assert(r.last.tiebreak && r.winner==FB_AWAY && r.score[1]==2 && r.down<=4);
    puts("football rules: downs, spots, turnovers, scoring, halftime, final-play scoring and bounded overtime passed");
}
