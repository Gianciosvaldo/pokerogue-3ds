#ifndef FOOTBALL_LIVE_H
#define FOOTBALL_LIVE_H
#include "football_roster.h"
#include "football_rules.h"
#include "football_playbook.h"
#include "football_defense.h"
#define FB_LIVE_ACTORS 22
#define FB_LIVE_STEP (1.0f/60.0f)
typedef enum { FB_LIVE_CALL,FB_LIVE_SET,FB_LIVE_PLAY,FB_LIVE_DEAD,FB_LIVE_END } FootballLivePhase;
/* A kick played in place of a scrimmage play, and where in its flight it is. The return stage hands the ball
 * to the receiving team: rules.offense flips to it, so carrier, blockers and pursuit work as on any run.
 * The kickoff is never chosen from a playcall: it starts the drive that follows a score or the half. */
typedef enum { FB_SPECIAL_NONE,FB_SPECIAL_PUNT,FB_SPECIAL_FIELD_GOAL,FB_SPECIAL_EXTRA_POINT,FB_SPECIAL_KICKOFF } FootballSpecial;
typedef enum { FB_STAGE_SNAP,FB_STAGE_FLIGHT,FB_STAGE_ROLL,FB_STAGE_RETURN } FootballKickStage;
/* What just happened, for the banner the screen shows during the dead ball. */
typedef enum {
    FB_EVENT_NONE,FB_EVENT_TOUCHDOWN,FB_EVENT_INTERCEPTION,FB_EVENT_SACK,FB_EVENT_FUMBLE,FB_EVENT_SAFETY,
    FB_EVENT_PUNT,FB_EVENT_FAIR_CATCH,FB_EVENT_TOUCHBACK,FB_EVENT_OUT_OF_BOUNDS,FB_EVENT_MUFF,FB_EVENT_BLOCKED,
    FB_EVENT_FIELD_GOAL,FB_EVENT_FIELD_GOAL_MISSED,FB_EVENT_EXTRA_POINT,FB_EVENT_EXTRA_POINT_MISSED,
    FB_EVENT_TWO_POINT,FB_EVENT_TWO_POINT_FAILED,FB_EVENT_KICKOFF
} FootballLiveEvent;
/* What the dead ball leads to: the try after a touchdown, or a kickoff (after a score, a safety or at the half). */
typedef enum { FB_AFTER_NONE,FB_AFTER_CONVERSION,FB_AFTER_KICKOFF } FootballLiveAfter;
/* Plays that name one player on the field: the engine tallies them per team member (stats) and lists the ones of the
 * dead ball in credits, for the career to turn into species candy. */
typedef enum { FB_CREDIT_NONE,FB_CREDIT_INTERCEPTION,FB_CREDIT_SACK,FB_CREDIT_TOUCHDOWN } FootballCreditKind;
typedef struct { uint8_t side,member,kind; } FootballCredit;
/* Species candy for the human team's Pokemon: every one of the 22 gets FB_CANDY_PLAYED at the end of a match (and
 * FB_CANDY_WIN more for a win); the player who makes the play gets the rest, when the ball is dead. */
enum { FB_CANDY_PLAYED=1,FB_CANDY_WIN=1,FB_CANDY_SACK=2,FB_CANDY_INTERCEPTION=4,FB_CANDY_TOUCHDOWN=5 };
static inline unsigned football_credit_candy(unsigned kind)
{
    return kind==FB_CREDIT_INTERCEPTION?FB_CANDY_INTERCEPTION:kind==FB_CREDIT_SACK?FB_CANDY_SACK:kind==FB_CREDIT_TOUCHDOWN?FB_CANDY_TOUCHDOWN:0;
}
enum { FB_LIVE_ACTION=1,FB_LIVE_JUKE=2,FB_LIVE_SWITCH=4 };
typedef struct { float x,y;unsigned pressed;int receiver; } FootballLiveInput;
typedef struct { FootballAttributes attributes;float x,y,vx,vy,stun,contact,block_time,block_cooldown,block_edge;
    uint8_t roster,block_peer;int8_t facing;bool home; } FootballLiveActor;
typedef struct {
    FootballRules rules;
    FootballLiveActor actors[22];
    FootballAttributes units[2][22];
    uint8_t lineup[2][22];
    uint16_t away_species[22];uint8_t away_form[22];
    FootballLivePhase phase;
    FootballPlayId play;
    FootballDefenseId defense;
    uint8_t coverage[11];
    float zone_x[11],zone_y[11];
    uint32_t rng,steps,pending;
    int pending_receiver;
    float accumulator,elapsed,dead_time,juke,cooldown;
    float ball_x,ball_y,ball_z,from_x,from_y,to_x,to_y,flight,flight_time,los;
    uint8_t controlled,carrier,target,snap_offense;
    bool flying,thrown,caught,loose,scramble;
    /* Not serialized: the CPU's last three calls (play id + 1), the side a scripted run bends toward
     * (-1/+1), and bit 0 set to let the CPU play the HOME side too (host tests, demos). */
    uint8_t recent[3],autoplay;int8_t run_side;
    /* Kicks (not serialized: a checkpoint is always taken at a call). kick_yards is the gross distance of the
     * last punt from the line of scrimmage; the landing spot is to_x/to_y while the ball flies. */
    uint8_t special,stage,kick_side,event;bool fair_catch,kick_good;int16_t kick_yards,return_yards;
    /* The try after a touchdown: conversion is 1 while the scoring team chooses a kick or a two-point play, and 2 while
     * that play is run. after/after_side say what the next call is (the scorer, or the kicker); next_offense is
     * who gets the ball once the try is over. catch_x/catch_time mark where and when a kick was caught. */
    uint8_t after,after_side,next_offense,conversion;uint16_t snap_score[2];float catch_x,catch_time;
    /* Credits (not serialized: the career pays them out the moment the ball is dead). stats[side][member][kind-1] counts
     * the match; `credited` names the defender who recovered a loose ball or intercepted, for a touchdown that follows. */
    FootballCredit credits[4];uint8_t credit_count,credited;uint8_t stats[2][22][3];
    FootballResolvedPlay result;
    unsigned snaps;
} FootballLive;
bool football_live_begin(FootballLive *m,const FootballRoster *roster,uint32_t seed,unsigned round);
bool football_live_call(FootballLive *m,FootballPlayId play);
/* Input axes are screen/world axes, normalized; receiver -1 or 0..4.
 * Edges consumed once even when a frame advances several fixed steps. */
void football_live_tick(FootballLive *m,unsigned milliseconds,FootballLiveInput input);
bool football_live_defend(FootballLive *m,FootballDefenseId defense);
/* The CPU's play for the current situation: down, distance, field position, score and clock decide the
 * run/pass mix and the call, and a play it just ran is rarely repeated. Records the call in recent[]. */
FootballPlayId football_live_ai_call(FootballLive *m);
/* True when the CPU runs that side (always AWAY; HOME too when autoplay is set). */
bool football_live_cpu(const FootballLive *m,FootballSide side);
void football_live_switch(FootballLive *m);
/* Special teams, CALL phase only. A punt is always allowed; a field goal only within kick range of the goal. The
 * kicker is the QB slot of the offense unit, so its throw power and accuracy set the kick. */
bool football_live_special(FootballLive *m,FootballSpecial kind);
bool football_live_special_allowed(const FootballLive *m,FootballSpecial kind);
/* Field-goal kick length in yards (line of scrimmage to goal line plus the end zone and the holder) and the
 * percent chance this kicker makes it. */
unsigned football_live_fg_distance(const FootballLive *m);
unsigned football_live_fg_chance(const FootballLive *m);
/* What the CPU does on this down: FB_SPECIAL_NONE (run a play), a punt or a field goal. */
FootballSpecial football_live_ai_special(const FootballLive *m);
/* The try after a touchdown (conversion == 1): the scoring team kicks FB_SPECIAL_EXTRA_POINT (football_live_special),
 * or plays it out from the 4-yard line for two points (football_live_go_for_two, then an ordinary playcall). */
bool football_live_go_for_two(FootballLive *m);
unsigned football_live_extra_point_chance(const FootballLive *m);
/* The CPU's choice at the try: true to go for two. Reads score and clock only (no randomness consumed). */
bool football_live_ai_goes_for_two(const FootballLive *m);
/* Where the camera should look: the controlled player, the ball in flight, or the kicker before a kick. */
void football_live_focus(const FootballLive *m,float *x,float *y,float *vx);
#endif
