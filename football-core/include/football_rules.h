#ifndef FOOTBALL_RULES_H
#define FOOTBALL_RULES_H
#include <stdbool.h>
#include <stdint.h>
#define FB_QUARTER_SECONDS 120
#define FB_OVERTIME_PAIRS 4

typedef enum { FB_HOME, FB_AWAY } FootballSide;
typedef enum {
    FB_RESULT_RUN, FB_RESULT_COMPLETE, FB_RESULT_INCOMPLETE,
    FB_RESULT_INTERCEPTION, FB_RESULT_FUMBLE, FB_RESULT_SACK, FB_RESULT_FUMBLE_RECOVERED
} FootballResultKind;
typedef struct {
    FootballResultKind kind;
    int16_t yards;
    uint8_t seconds;
} FootballResolvedPlay;
typedef struct {
    bool first_down, turnover, touchdown, safety, quarter_end, halftime, tiebreak, field_goal;
} FootballRuleEvents;
typedef struct {
    uint16_t score[2], seconds_left;
    uint8_t quarter, offense, down, ball, first_down_at;
    uint8_t overtime_drives, tiebreak_side;
    bool finished;
    FootballSide winner;
    FootballRuleEvents last;
} FootballRules;
/* Ball/line-to-gain are yards from the CURRENT offense's own goal. Both teams
 * receive paired overtime possessions. Ties after four pairs use a seeded
 * two-point tiebreak selected by the engine at initialization. */
bool football_rules_begin(FootballRules *out, FootballSide tiebreak_side);
/* Invalid inputs and finished matches leave *rules unchanged. */
bool football_rules_advance(FootballRules *rules, FootballResolvedPlay play);
unsigned football_rules_distance(const FootballRules *rules);
/* Special teams and changes of possession (live mode). All validate like football_rules_advance (an
 * invalid argument, state or finished match leaves *rules unchanged) and run the clock through the same
 * quarter, half and overtime logic.
 * Possession: `receiver` gets a fresh first and ten `spot` yards from ITS OWN goal (1..99): punts after
 * the return, missed field goals, blocked kicks, muffs. Seconds 1..30 come off the clock.
 * Field goal: three points for the team on offense, then the other team starts a drive at its 25.
 * Touchdown: a score outside a scrimmage play (a return); the other team starts a drive at its 25. */
bool football_rules_possession(FootballRules *rules, FootballSide receiver, unsigned spot, unsigned seconds);
bool football_rules_field_goal(FootballRules *rules, unsigned seconds);
bool football_rules_touchdown(FootballRules *rules, FootballSide scorer, unsigned seconds);
#endif
