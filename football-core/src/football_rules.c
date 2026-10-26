#include "football_rules.h"
#include <string.h>

static void drive(FootballRules *r, unsigned offense, unsigned ball)
{
    r->offense=(uint8_t)offense; r->ball=(uint8_t)ball; r->down=1;
    r->first_down_at=(uint8_t)(ball+10<100 ? ball+10 : 100);
}
static void finish(FootballRules *r)
{
    r->finished=true; r->winner=r->score[FB_HOME]>r->score[FB_AWAY] ? FB_HOME : FB_AWAY;
}
static void end_drive(FootballRules *r, unsigned next, unsigned spot)
{
    if (r->quarter<5) { drive(r,next,spot); return; }
    unsigned previous=r->offense;
    ++r->overtime_drives;
    if (!(r->overtime_drives%2)) {
        if (r->score[0]!=r->score[1]) { finish(r); return; }
        if (r->overtime_drives==2*FB_OVERTIME_PAIRS) {
            r->score[r->tiebreak_side]+=2; r->last.tiebreak=true; finish(r); return;
        }
    }
    /* Alternate the first team of each paired overtime round. */
    drive(r,r->overtime_drives%2 ? 1-previous : previous,75);
}
/* True when the rules are in a state every transition may start from. */
static bool sound(const FootballRules *out)
{
    return out && !out->finished && out->offense<=FB_AWAY && out->down>=1 && out->down<=4 &&
        out->ball && out->ball<100 && out->first_down_at>out->ball && out->first_down_at<=100 &&
        out->quarter>=1 && out->quarter<=5 && out->tiebreak_side<=FB_AWAY &&
        (out->quarter>=5 || (out->seconds_left && out->seconds_left<=FB_QUARTER_SECONDS)) &&
        out->overtime_drives<2*FB_OVERTIME_PAIRS && out->score[0]<=10000 && out->score[1]<=10000;
}
/* Runs the game clock; the end of a quarter, the half and regulation are handled here for every transition. */
static void run_clock(FootballRules *r, unsigned seconds)
{
    if (r->quarter>=5) return;
    r->seconds_left=seconds>=r->seconds_left ? 0 : (uint16_t)(r->seconds_left-seconds);
    if (r->seconds_left) return;
    r->last.quarter_end=true;
    if (r->quarter==4) {
        if (r->score[0]!=r->score[1]) finish(r);
        else { r->quarter=5; drive(r,FB_HOME,75); }
    } else {
        ++r->quarter; r->seconds_left=FB_QUARTER_SECONDS;
        if (r->quarter==3) { r->last.halftime=true; drive(r,FB_AWAY,25); }
    }
}
static void touchdown(FootballRules *r, unsigned scorer)
{
    r->score[scorer]+=7; r->last.touchdown=true; end_drive(r,1-scorer,25);
}
bool football_rules_begin(FootballRules *out, FootballSide tiebreak_side)
{
    if (!out || (unsigned)tiebreak_side>FB_AWAY) return false;
    FootballRules r={0};
    r.seconds_left=FB_QUARTER_SECONDS; r.quarter=1; r.tiebreak_side=(uint8_t)tiebreak_side;
    drive(&r,FB_HOME,25); *out=r; return true;
}
unsigned football_rules_distance(const FootballRules *r)
{
    return r && r->first_down_at>r->ball ? (unsigned)(r->first_down_at-r->ball) : 0;
}
bool football_rules_advance(FootballRules *out, FootballResolvedPlay p)
{
    if (!sound(out) || (unsigned)p.kind>FB_RESULT_FUMBLE_RECOVERED || p.yards< -100 || p.yards>100 ||
        !p.seconds || p.seconds>30 ||
        (p.kind==FB_RESULT_INCOMPLETE && p.yards!=0) ||
        (p.kind==FB_RESULT_INTERCEPTION && p.yards<0) ||
        (p.kind==FB_RESULT_SACK && p.yards>0)) return false;
    FootballRules r=*out;
    memset(&r.last,0,sizeof r.last);
    unsigned offense=r.offense, defense=1-offense;
    int spot=r.ball+p.yards;
    bool lost=p.kind==FB_RESULT_INTERCEPTION || p.kind==FB_RESULT_FUMBLE;
    if (lost) {
        r.last.turnover=true;
        if (spot<=0) { /* Lost ball recovered in the offense's own end zone. */
            touchdown(&r,defense);
        } else end_drive(&r,defense,spot>=100 ? 25 : (unsigned)(100-spot));
    } else if (spot>=100) {
        touchdown(&r,offense);
    } else if (spot<=0) {
        r.score[defense]+=2; r.last.safety=true; end_drive(&r,defense,25);
    } else {
        r.ball=(uint8_t)spot;
        if (spot>=r.first_down_at) {
            r.last.first_down=true; drive(&r,offense,(unsigned)spot);
        } else if (++r.down>4) {
            r.down=4; r.last.turnover=true; end_drive(&r,defense,(unsigned)(100-spot));
        }
    }
    run_clock(&r,p.seconds);
    *out=r; return true;
}
bool football_rules_possession(FootballRules *out, FootballSide receiver, unsigned spot, unsigned seconds)
{
    if (!sound(out) || (unsigned)receiver>FB_AWAY || spot<1 || spot>99 || !seconds || seconds>30) return false;
    FootballRules r=*out;
    memset(&r.last,0,sizeof r.last);
    r.last.turnover=true;
    end_drive(&r,receiver,spot);
    run_clock(&r,seconds);
    *out=r; return true;
}
bool football_rules_field_goal(FootballRules *out, unsigned seconds)
{
    if (!sound(out) || !seconds || seconds>30) return false;
    FootballRules r=*out;
    memset(&r.last,0,sizeof r.last);
    r.score[r.offense]+=3; r.last.field_goal=true;
    end_drive(&r,1-r.offense,25);
    run_clock(&r,seconds);
    *out=r; return true;
}
bool football_rules_touchdown(FootballRules *out, FootballSide scorer, unsigned seconds)
{
    if (!sound(out) || (unsigned)scorer>FB_AWAY || !seconds || seconds>30) return false;
    FootballRules r=*out;
    memset(&r.last,0,sizeof r.last);
    touchdown(&r,scorer);
    run_clock(&r,seconds);
    *out=r; return true;
}
