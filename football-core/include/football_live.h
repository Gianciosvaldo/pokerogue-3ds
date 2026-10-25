#ifndef FOOTBALL_LIVE_H
#define FOOTBALL_LIVE_H
#include "football_roster.h"
#include "football_rules.h"
#include "football_playbook.h"
#include "football_defense.h"
#define FB_LIVE_ACTORS 22
#define FB_LIVE_STEP (1.0f/60.0f)
typedef enum { FB_LIVE_CALL,FB_LIVE_SET,FB_LIVE_PLAY,FB_LIVE_DEAD,FB_LIVE_END } FootballLivePhase;
enum { FB_LIVE_ACTION=1,FB_LIVE_JUKE=2,FB_LIVE_SWITCH=4 };
typedef struct { float x,y;unsigned pressed;int receiver; } FootballLiveInput;
typedef struct { FootballAttributes attributes;float x,y,vx,vy,stun,contact,block_time,block_cooldown;
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
    bool flying,thrown,caught,loose;
    FootballResolvedPlay result;
    unsigned snaps;
} FootballLive;
bool football_live_begin(FootballLive *m,const FootballRoster *roster,uint32_t seed,unsigned round);
bool football_live_call(FootballLive *m,FootballPlayId play);
/* Input axes are screen/world axes, normalized; receiver -1 or 0..4.
 * Edges consumed once even when a frame advances several fixed steps. */
void football_live_tick(FootballLive *m,unsigned milliseconds,FootballLiveInput input);
bool football_live_defend(FootballLive *m,FootballDefenseId defense);
void football_live_switch(FootballLive *m);
#endif
