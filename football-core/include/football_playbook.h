#ifndef FOOTBALL_PLAYBOOK_H
#define FOOTBALL_PLAYBOOK_H
#include "playcall_ui.h"

#define FB_ROUTE_POINTS 4
typedef struct { float x, y; } FootballRoutePoint;
typedef struct {
    FootballPlayId id;
    FootballFormation formation;
    FootballPlayType type;
    const char *name, *description;
    /* Small additive balance modifiers, applied before probability/yard clamps. */
    int8_t gain, completion, sack, interception, speed_weight;
    uint8_t route_count;
    FootballRoutePoint route[FB_ROUTE_POINTS];
} FootballPlayDefinition;

/* Immutable ROM data, stable legacy IDs 0..3, no allocation or RNG. */
const FootballPlayDefinition *football_playbook_get(FootballPlayId id);
unsigned football_playbook_count(FootballFormation formation, FootballPlayType type);
const FootballPlayDefinition *football_playbook_option(FootballFormation formation,
                                                       FootballPlayType type, unsigned option);
bool football_playbook_valid(PlayInfo play);
/* Sample receiver-relative path by segment progress. y is inside-positive.
 * Callers mirror y for the lower receiver and x for the away offense. */
FootballRoutePoint football_playbook_route(const FootballPlayDefinition *play, float progress);
#endif
