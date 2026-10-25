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
    bool first_down, turnover, touchdown, safety, quarter_end, halftime, tiebreak;
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
#endif
