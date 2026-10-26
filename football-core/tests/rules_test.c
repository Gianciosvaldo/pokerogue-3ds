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
    assert(football_rules_begin(&r,FB_HOME));r.ball=40;r.first_down_at=50;
    assert(football_rules_advance(&r,(FootballResolvedPlay){FB_RESULT_FUMBLE_RECOVERED,3,5}));
    assert(r.offense==FB_HOME && r.down==2 && r.ball==43 && !r.last.turnover);
    assert(football_rules_advance(&r,(FootballResolvedPlay){FB_RESULT_FUMBLE_RECOVERED,10,5}));
    assert(r.down==1 && r.ball==53 && r.last.first_down);
    r.ball=1;r.first_down_at=11;
    assert(football_rules_advance(&r,(FootballResolvedPlay){FB_RESULT_FUMBLE_RECOVERED,-2,5}));
    assert(r.last.safety && r.score[FB_AWAY]==2);
    assert(football_rules_begin(&r,FB_HOME));r.ball=95;r.first_down_at=100;
    assert(football_rules_advance(&r,(FootballResolvedPlay){FB_RESULT_FUMBLE_RECOVERED,5,5}));assert(r.last.touchdown && r.score[FB_HOME]==7);
    assert(football_rules_begin(&r,FB_HOME));r.down=4;
    assert(football_rules_advance(&r,(FootballResolvedPlay){FB_RESULT_FUMBLE_RECOVERED,1,5}));assert(r.last.turnover && r.offense==FB_AWAY);

    /* Special teams: punts, kicks and returns change possession without a scrimmage result. */
    assert(football_rules_begin(&r,FB_HOME));r.ball=30;r.down=4;r.first_down_at=40;
    FootballRules before=r;
    assert(!football_rules_possession(&r,FB_AWAY,0,5) && !football_rules_possession(&r,FB_AWAY,100,5) &&
        !football_rules_possession(&r,FB_AWAY,30,0) && !football_rules_possession(&r,FB_AWAY,30,31) &&
        !football_rules_possession(&r,(FootballSide)2,30,5) && !memcmp(&r,&before,sizeof r));
    assert(football_rules_possession(&r,FB_AWAY,38,6));
    assert(r.offense==FB_AWAY && r.ball==38 && r.down==1 && r.first_down_at==48 && r.last.turnover && r.seconds_left==FB_QUARTER_SECONDS-6);
    /* The same side can keep the ball (a muffed punt it recovers). */
    assert(football_rules_possession(&r,FB_AWAY,60,3) && r.offense==FB_AWAY && r.ball==60 && r.first_down_at==70);
    /* A kick that runs out the quarter still ends it, and the half hands the ball to AWAY at its 25. */
    r.quarter=2;r.seconds_left=3;
    assert(football_rules_possession(&r,FB_HOME,20,4));
    assert(r.quarter==3 && r.last.halftime && r.offense==FB_AWAY && r.ball==25 && r.seconds_left==FB_QUARTER_SECONDS);
    /* The end of regulation decides the game unless it is tied. */
    r.quarter=4;r.seconds_left=2;r.score[0]=3;r.score[1]=0;
    assert(football_rules_possession(&r,FB_AWAY,30,2) && r.finished && r.winner==FB_HOME);
    assert(!football_rules_possession(&r,FB_AWAY,30,2) && !football_rules_field_goal(&r,3) && !football_rules_touchdown(&r,FB_HOME,3));
    /* Field goals are three points and the other team then starts at its 25. */
    assert(football_rules_begin(&r,FB_HOME));r.ball=70;r.down=4;r.first_down_at=80;
    assert(!football_rules_field_goal(&r,0) && !football_rules_field_goal(&r,31));
    assert(football_rules_field_goal(&r,4));
    assert(r.score[FB_HOME]==3 && r.last.field_goal && !r.last.touchdown && r.offense==FB_AWAY && r.ball==25 && r.down==1);
    /* Overtime: a field goal ends that team's possession, and a possession change ends it too. */
    assert(football_rules_begin(&r,FB_HOME));r.quarter=5;r.seconds_left=0;r.offense=FB_HOME;r.ball=75;r.first_down_at=85;
    assert(football_rules_field_goal(&r,4) && r.offense==FB_AWAY && r.ball==75 && r.overtime_drives==1 && r.score[FB_HOME]==3);
    assert(football_rules_possession(&r,FB_HOME,30,4) && r.overtime_drives==2 && r.finished && r.winner==FB_HOME);
    /* Return touchdowns score for the returning side. */
    assert(football_rules_begin(&r,FB_HOME));r.ball=40;r.down=4;r.first_down_at=50;
    assert(football_rules_touchdown(&r,FB_AWAY,9));
    assert(r.score[FB_AWAY]==7 && r.last.touchdown && r.offense==FB_HOME && r.ball==25 && r.seconds_left==FB_QUARTER_SECONDS-9);

    /* Six-point touchdowns (live mode): the try follows, except on the last play of regulation and in overtime. */
    assert(football_rules_begin(&r,FB_HOME));r.td_points=6;r.ball=95;r.first_down_at=100;
    assert(football_rules_advance(&r,(FootballResolvedPlay){FB_RESULT_RUN,5,6}));
    assert(r.score[FB_HOME]==6 && r.last.touchdown && r.offense==FB_AWAY && r.ball==25);
    assert(football_rules_touchdown(&r,FB_AWAY,6) && r.score[FB_AWAY]==6);
    assert(football_rules_begin(&r,FB_HOME));r.td_points=6;r.quarter=4;r.seconds_left=5;r.ball=95;r.first_down_at=100;r.score[1]=3;
    assert(football_rules_advance(&r,(FootballResolvedPlay){FB_RESULT_RUN,5,6}));
    assert(r.score[FB_HOME]==7 && r.finished && r.winner==FB_HOME); /* the clock ran out on the touchdown: full seven, no try */
    assert(football_rules_begin(&r,FB_HOME));r.td_points=6;r.quarter=4;r.seconds_left=20;r.ball=95;r.first_down_at=100;
    assert(football_rules_advance(&r,(FootballResolvedPlay){FB_RESULT_RUN,5,6}) && r.score[FB_HOME]==6 && !r.finished);
    assert(football_rules_begin(&r,FB_HOME));r.td_points=6;r.quarter=5;r.ball=75;r.first_down_at=85;r.overtime_drives=0;
    assert(football_rules_advance(&r,(FootballResolvedPlay){FB_RESULT_RUN,30,6}) && r.score[FB_HOME]==7);
    /* A touchdown at the end of the first half still gets its try, and the second half then belongs to AWAY. */
    assert(football_rules_begin(&r,FB_HOME));r.td_points=6;r.quarter=2;r.seconds_left=4;r.ball=95;r.first_down_at=100;
    assert(football_rules_advance(&r,(FootballResolvedPlay){FB_RESULT_RUN,5,6}) && r.score[FB_HOME]==6 && r.quarter==3 && r.last.halftime && r.offense==FB_AWAY);
    /* Conversions: one or two points, none for a miss; no clock, and the receiver starts at its 25. */
    assert(football_rules_begin(&r,FB_HOME));r.td_points=6;r.score[0]=6;r.offense=FB_HOME;r.ball=85;r.first_down_at=100;r.seconds_left=77;
    assert(!football_rules_conversion(&r,FB_HOME,3,FB_AWAY) && !football_rules_conversion(&r,(FootballSide)2,1,FB_AWAY) && !football_rules_conversion(&r,FB_HOME,1,(FootballSide)2));
    assert(football_rules_conversion(&r,FB_HOME,1,FB_AWAY));
    assert(r.score[0]==7 && r.last.conversion && r.offense==FB_AWAY && r.ball==25 && r.down==1 && r.seconds_left==77);
    assert(football_rules_conversion(&r,FB_AWAY,2,FB_HOME) && r.score[1]==2 && r.offense==FB_HOME);
    assert(football_rules_conversion(&r,FB_AWAY,0,FB_HOME) && r.score[1]==2 && !r.last.conversion);
    r.quarter=5;assert(!football_rules_conversion(&r,FB_HOME,1,FB_AWAY));
    r.quarter=4;r.finished=true;assert(!football_rules_conversion(&r,FB_HOME,1,FB_AWAY));

    puts("football rules: downs, spots, turnovers, scoring, halftime, final-play scoring and bounded overtime passed");
}
