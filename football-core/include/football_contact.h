#ifndef FOOTBALL_CONTACT_H
#define FOOTBALL_CONTACT_H
#include "football_stats.h"
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
#endif
