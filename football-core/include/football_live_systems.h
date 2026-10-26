#ifndef FOOTBALL_LIVE_SYSTEMS_H
#define FOOTBALL_LIVE_SYSTEMS_H
#include "football_live.h"
#define FB_BODY_RADIUS .65f
#define FB_BODY_DIAMETER (2*FB_BODY_RADIUS)
#define FB_NO_ACTOR 255
/* CPU decisions inside a play (football_live_ai.c). */
#define FB_AI_HOLD -1
#define FB_AI_THROW_AWAY 5
#define FB_AI_SCRAMBLE 6
uint32_t football_live_random(FootballLive *match);
/* Shared movement step for a player's intended direction (-1..1 per axis). */
void football_live_move(FootballLiveActor *actor,float x,float y,bool sprint);
/* Kick plays (football_live_special.c): formation, one fixed step, and the end of the dead ball. */
void football_live_special_arrange(FootballLive *match);
void football_live_special_step(FootballLive *match,FootballLiveInput input);
/* The ball is dead and the rules ran: decide whether a try or a kickoff follows (football_live.c). */
void football_live_after_play(FootballLive *match);
/* The try after a touchdown ended (a kick or a play): rules, banner, then the kickoff. */
void football_live_conversion_done(FootballLive *match,unsigned points,FootballLiveEvent event);
/* A return ended (tackle, sideline, goal line, fumble): rules, banner and dead ball. */
void football_live_special_finish(FootballLive *match,FootballResultKind kind,float x);
/* A carrier's steering: the scripted lane of a run play, then reading the defenders. */
void football_live_ai_carrier(const FootballLive *match,unsigned actor,float *x,float *y);
/* The QB's read: FB_AI_HOLD, a receiver 0..4, FB_AI_THROW_AWAY or FB_AI_SCRAMBLE. */
int football_live_ai_qb(FootballLive *match);
/* A receiver's nudge away from the closest defender, in field axes. */
float football_live_ai_separation(const FootballLive *match,unsigned actor);
FootballDefenseId football_live_ai_defense(FootballLive *match);
/* Snap-local state only: rebuilt at formation setup, not serialized. */
void football_live_defense_setup(FootballLive *match);
void football_live_defense_intent(FootballLive *match,unsigned actor,float *x,float *y);
bool football_live_is_blocker(const FootballLive *match,unsigned actor);
unsigned football_live_block_target(const FootballLive *match,unsigned blocker);
void football_live_physics(FootballLive *match);
void football_live_update_facing(FootballLive *match);
bool football_live_faces_left(const FootballLive *match,unsigned actor);
#endif
