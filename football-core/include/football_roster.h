#ifndef FOOTBALL_ROSTER_H
#define FOOTBALL_ROSTER_H
#include "gc/football.h"
#include "football_stats.h"
#define FB_ROSTER_SIZE 22
#define FB_ROSTER_BUDGET 55
#define FB_UNIT_SIZE 11
/* Each slot owns a different roster member: offense 0..10, defense 11..21. */
/* The attribute each vitamin raises (lorenzo 2026-11-15 asked for them all): Carbos Speed, Protein Strength, HP Up Stamina, Iron Tackling,
 * Calcium Throw accuracy (the special attack side of the throw), Zinc Catching (the other special-side attribute: Special Defense feeds
 * only Stamina, so Zinc has no attribute of its own and this was my pick). */
typedef enum { FB_BOOST_SPD, FB_BOOST_STR, FB_BOOST_STA, FB_BOOST_TAK, FB_BOOST_THA, FB_BOOST_CTH, FB_BOOST_COUNT } FootballBoost;
#define FB_BOOST_MAX 20 /* the most a member can have of one boost: ten vitamins of +2 */
typedef struct {
    gc_mon mon[FB_ROSTER_SIZE];
    FootballPlayer player[FB_ROSTER_SIZE];
    uint8_t lineup[FB_ROSTER_SIZE], speed[FB_ROSTER_SIZE], strength[FB_ROSTER_SIZE];
    /* The other three vitamins' boosts and the two more: stamina (HP Up), tackling (Iron), throwing accuracy (Calcium), catching (Zinc). */
    uint8_t stamina[FB_ROSTER_SIZE], tackling[FB_ROSTER_SIZE], accuracy[FB_ROSTER_SIZE], catching[FB_ROSTER_SIZE];
    int16_t mega_item[FB_ROSTER_SIZE];
    uint8_t count;
    double cost;
    FootballDexMaxima maxima;
} FootballRoster;
extern const char *const football_slot_names[FB_ROSTER_SIZE];
/* The slot spelled out ("Quarterback", "Wide receiver 1") for the draft, which fills the slots in this order. */
extern const char *const football_slot_titles[FB_ROSTER_SIZE];
FootballPosition football_slot_position(unsigned slot);
/* The short name of the position class a player is best at ("QB", "WR", "RB", "OL/DL", "LB", "TE/S"). */
const char *football_position_label(FootballPosition position);
bool football_roster_init(FootballRoster *r);
/* A valid base-form starter from the same unlock data Classic uses. */
bool football_roster_starter(const gc_system *profile,unsigned species,gc_starter *out);
bool football_roster_add(FootballRoster *r,gc_run *scratch,const gc_starter *starter,const gc_system *profile,const char **why);
bool football_roster_refresh(FootballRoster *r,unsigned member);
/* The boost a member has of one kind (NULL for a bad kind or member), and the attribute of a player it raises. */
uint8_t *football_roster_boost(FootballRoster *r,FootballBoost kind,unsigned member);
uint8_t *football_attribute_of(FootballAttributes *a,FootballBoost kind);
/* Adds all of a member's boosts to attributes made from its form (a mega form's, for the match), each capped at 99. */
void football_roster_apply_boosts(const FootballRoster *r,unsigned member,FootballAttributes *a);
const char *football_boost_name(FootballBoost kind);
bool football_roster_assign(FootballRoster *r,unsigned slot,unsigned member);
void football_roster_autofill(FootballRoster *r);
#endif
