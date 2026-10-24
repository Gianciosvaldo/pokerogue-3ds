#ifndef FOOTBALL_ROSTER_H
#define FOOTBALL_ROSTER_H
#include "gc/football.h"
#include "football_stats.h"
#define FB_ROSTER_SIZE 22
#define FB_ROSTER_BUDGET 40
#define FB_UNIT_SIZE 11
/* Each slot owns a different roster member: offense 0..10, defense 11..21. */
typedef struct {
    gc_mon mon[FB_ROSTER_SIZE];
    FootballPlayer player[FB_ROSTER_SIZE];
    uint8_t lineup[FB_ROSTER_SIZE], speed[FB_ROSTER_SIZE], strength[FB_ROSTER_SIZE];
    int16_t mega_item[FB_ROSTER_SIZE];
    uint8_t count;
    double cost;
    FootballDexMaxima maxima;
} FootballRoster;
extern const char *const football_slot_names[FB_ROSTER_SIZE];
FootballPosition football_slot_position(unsigned slot);
bool football_roster_init(FootballRoster *r);
bool football_roster_add(FootballRoster *r,gc_run *scratch,const gc_starter *starter,const gc_system *profile,const char **why);
bool football_roster_refresh(FootballRoster *r,unsigned member);
bool football_roster_assign(FootballRoster *r,unsigned slot,unsigned member);
void football_roster_autofill(FootballRoster *r);
#endif
