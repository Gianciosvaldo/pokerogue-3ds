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

typedef struct { const FootballRoutePoint *point; unsigned count; } FootballRouteView;
/* Actor rank 1 RB, 2/3/4 WR1/2/3, 5 TE. Primary geometry remains in route[]. */
FootballRoutePoint football_playbook_sample_path(FootballRouteView route,float progress);
FootballRouteView football_playbook_receiver_path(const FootballPlayDefinition *play,unsigned rank);
unsigned football_playbook_primary_receiver(const FootballPlayDefinition *play);
unsigned football_playbook_runner(const FootballPlayDefinition *play);
bool football_playbook_screen(const FootballPlayDefinition *play);
bool football_playbook_settles(const FootballPlayDefinition *play);
/* Immutable ROM data, stable legacy IDs 0..7, no allocation or RNG. */
const FootballPlayDefinition *football_playbook_get(FootballPlayId id);
unsigned football_playbook_count(FootballFormation formation, FootballPlayType type);
const FootballPlayDefinition *football_playbook_option(FootballFormation formation,
                                                       FootballPlayType type, unsigned option);
bool football_playbook_valid(PlayInfo play);
/* Sample receiver-relative path by segment progress. y is inside-positive.
 * Callers mirror y for the lower receiver and x for the away offense. */
FootballRoutePoint football_playbook_route(const FootballPlayDefinition *play, float progress);
#endif
