#ifndef FOOTBALL_CONTACT_H
#define FOOTBALL_CONTACT_H
#include "football_stats.h"
typedef enum { FB_CONTACT_BALANCED, FB_CONTACT_JUKE, FB_CONTACT_POWER, FB_CONTACT_COUNT } FootballContactTactic;
typedef struct { bool attempted,offense_keeps; uint8_t offense_chance; } FootballRecoveryResult;
typedef struct {
    uint16_t hold_ms;
    int8_t edge; /* -12..12: run lane / pass protection advantage. */
    bool pancake;
} FootballBlockResult;
typedef struct {
    bool attempted, broken, fumble, big_hit;
    uint8_t impact, tackle_chance, fumble_chance, extra_yards;
} FootballContactResult;
/* Pure, bounded contests. Caller supplies independent uniform 0..99 rolls;
 * these functions never own RNG, mutate ratings, or allocate memory. */
bool football_contact_block(FootballBlockResult *out, const FootballAttributes *blocker,
                             const FootballAttributes *rusher, unsigned roll);
bool football_contact_tackle(FootballContactResult *out, const FootballAttributes *carrier,
                              const FootballAttributes *defender, bool just_caught,
                              unsigned tackle_roll, unsigned fumble_roll);
bool football_contact_tactical(FootballContactResult *out,const FootballAttributes *carrier,
                                const FootballAttributes *defender,bool just_caught,FootballContactTactic tactic,
                                bool dive,unsigned tackle_roll,unsigned fumble_roll);
bool football_contact_recover(FootballRecoveryResult *out,const FootballAttributes *offense,
                               const FootballAttributes *defense,unsigned roll);
const char *football_contact_tactic_name(FootballContactTactic tactic);
#endif
