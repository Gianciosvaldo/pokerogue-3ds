#ifndef FOOTBALL_OPPONENT_H
#define FOOTBALL_OPPONENT_H
#include <stdbool.h>
#include <stdint.h>
#include "pr_data.h"
/* The playoff is thirteen matches, one against each trainer of the Kanto line-up, in the order the classic game's run meets them: the eight Gym
 * Leaders (rounds 1-8), the four of the Elite Four (9-12) and the Champion (13). Every Gym Leader and Elite Four member fields a team of the
 * type the classic game gives that trainer (Brock's team is all Rock-types), and the Champion, who has no type, fields a stacked team: only the
 * strongest forms of the Pokedex. The teams also get stronger every round (football_live_begin). */
#define FB_CAREER_ROUNDS 13
#define FB_GYM_LEADERS 8
#define FB_ELITE_FOUR 4
#define FB_CHAMPION_MIN_BST 580 /* the champion's team is made of forms with at least this base stat total (about one form in six) */
/* The average attribute of a Gym Leader's or Elite Four member's team in the first round; each round after it is 6.25% stronger (the Champion's
 * stacked team is not set by this, it is whatever its pool of FB_CHAMPION_MIN_BST forms gives, scaled the same way). Before 2026-11-10 the team
 * took whatever its type's pool gave: 36.6 in round 1, but 44.1 for Lt. Surge, and lorenzo found him "absurdly strong"; 34 is a blind tune, the
 * knob if it is still too hard or too easy. */
#define FB_LEADER_ATTRIBUTE_BASE 34.0
typedef enum { FB_OPPONENT_GYM_LEADER,FB_OPPONENT_ELITE_FOUR,FB_OPPONENT_CHAMPION } FootballOpponentRole;
typedef struct {
    const char *name;
    int trainer_type;   /* the classic game's TrainerType, for the portrait and the type check */
    uint8_t role;
    int8_t type;        /* the type of every member of the team, -1 for the Champion's stacked team */
} FootballOpponent;
/* The opponent of a round (1..FB_CAREER_ROUNDS); NULL outside it. */
const FootballOpponent *football_opponent(unsigned round);
const char *football_opponent_role_name(FootballOpponentRole role);
/* "Rock", "Water"...; "Any" for -1. */
const char *football_type_name(int type);
/* Whether a form may play for the opponent: a form of its type, or for the stacked team one strong enough. */
bool football_opponent_form_fits(const FootballOpponent *opponent,const pr_form *form);
#endif
