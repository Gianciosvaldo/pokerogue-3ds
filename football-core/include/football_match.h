#ifndef FOOTBALL_MATCH_H
#define FOOTBALL_MATCH_H
#include "football_rules.h"
#include "football_stats.h"
#include "football_contact.h"
#include "playcall_ui.h"
#define FB_MATCH_TEAM_CAP 6
#define FB_PLAY_ANIMATION_MS 700

typedef struct { FootballPlayer players[FB_MATCH_TEAM_CAP]; uint8_t count; } FootballTeam;
typedef enum { FB_MATCH_CALL, FB_MATCH_ANIMATION, FB_MATCH_FINISHED, FB_MATCH_ERROR } FootballMatchPhase;
typedef struct {
    FootballTeam teams[2];
    uint8_t energy[2][FB_MATCH_TEAM_CAP];
    FootballRules rules;
    FootballMatchPhase phase;
    uint32_t rng;
    uint16_t difficulty_q8, animation_ms, plays;
    PlayInfo play;
    FootballResolvedPlay resolved;
    uint8_t carrier, blocker, rusher, tackler; /* 255: no separate blocker. */
    FootballBlockResult blocking;
    FootballContactResult contact;
} FootballMatch;
/* Fixed-size, allocation-free state. Only match initialization rolls a tiebreak
 * side; play RNG is consumed once per accepted snap, never by rendering. */
bool football_match_begin(FootballMatch *out, const FootballTeam *home,
                           const FootballTeam *away, uint32_t seed, uint16_t difficulty_q8);
unsigned football_match_quarterback(const FootballMatch *match, FootballSide side);
bool football_match_waiting_for_home(const FootballMatch *match);
/* Accepts validated home calls only; missing target slots fall back to the best
 * receiver/runner, supporting every currently permitted draft size (1..6). */
bool football_match_call(FootballMatch *match, PlayInfo play);
/* At most one AI snap or one animation update/commit per call. Delta clamped to
 * 250 ms; the game clock advances by resolved play time, not menu/frame time. */
bool football_match_tick(FootballMatch *match, uint32_t elapsed_ms);
/* Deterministic opponent species pool; one-time generation, not frame work. */
bool football_match_opponent(FootballTeam *out, const FootballDexMaxima *maxima,
                              uint32_t seed, unsigned round);
const char *football_match_result_name(FootballResultKind result);
#endif
