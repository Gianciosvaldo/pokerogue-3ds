#ifndef FOOTBALL_LIVE_SYSTEMS_H
#define FOOTBALL_LIVE_SYSTEMS_H
#include "football_live.h"
#define FB_BODY_RADIUS .65f
#define FB_BODY_DIAMETER (2*FB_BODY_RADIUS)
#define FB_NO_ACTOR 255
/* Snap-local state only: rebuilt at formation setup, not serialized. */
void football_live_defense_setup(FootballLive *match);
void football_live_defense_intent(FootballLive *match,unsigned actor,float *x,float *y);
unsigned football_live_block_target(const FootballLive *match,unsigned blocker);
void football_live_physics(FootballLive *match);
void football_live_update_facing(FootballLive *match);
bool football_live_faces_left(const FootballLive *match,unsigned actor);
#endif
