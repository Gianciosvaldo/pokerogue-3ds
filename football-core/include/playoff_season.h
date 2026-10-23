#ifndef PLAYOFF_SEASON_H
#define PLAYOFF_SEASON_H
#include "football_stats.h"
#include "gc/football.h"
#define FB_TEAM_CAP 6
#define FB_PLAYOFF_ROUNDS 7
#define FB_SHOP_CAP 3

typedef enum {
    FB_SEASON_DRAFT, FB_SEASON_READY, FB_SEASON_MATCH, FB_SEASON_EXP,
    FB_SEASON_SHOP, FB_SEASON_CLEAR, FB_SEASON_ELIMINATED, FB_SEASON_ERROR
} FootballSeasonPhase;
typedef enum { FB_ITEM_CARBOS, FB_ITEM_PROTEIN, FB_ITEM_MEGA_STONE } FootballShopItem;
typedef struct {
    FootballShopItem item;
    uint32_t price;
    int16_t classic_arg;
    int8_t target; /* Mega stone's compatible storage slot; -1 for vitamins. */
    bool sold;
} FootballShopOffer;
typedef struct {
    unsigned round; /* 1..7 */
    uint16_t difficulty_q8; /* 256 = 1x; final = 640 = 2.5x */
    uint32_t seed;
} FootballMatchSpec;
typedef struct {
    gc_run *roster; /* Dedicated existing roster; caller keeps it static/heap. */
    FootballDexMaxima maxima;
    FootballPlayer players[FB_TEAM_CAP];
    uint8_t speed_boost[FB_TEAM_CAP], strength_boost[FB_TEAM_CAP];
    FootballShopOffer shop[FB_SHOP_CAP];
    unsigned shop_count;
    uint32_t wallet, rng;
    unsigned round, wins;
    int8_t mega_slot; /* -1: none; selected afresh before each match. */
    FootballSeasonPhase phase;
} FootballSeason;

bool football_season_new(FootballSeason *s, gc_run *roster, gc_system *profile,
                         const char *seed, uint32_t entropy);
bool football_season_draft(FootballSeason *s, const gc_starter *team, int count,
                           const char **why);
bool football_season_select_mega(FootballSeason *s, int slot);
bool football_season_match_player(const FootballSeason *s, unsigned slot, FootballPlayer *out);
bool football_season_start_match(FootballSeason *s, FootballMatchSpec *out);
bool football_season_finish_match(FootballSeason *s, bool won, const char **why);
/* Drain roster events before calling each frame. WAIT uses the existing gc_screen. */
gc_football_progress football_season_tick(FootballSeason *s);
bool football_season_choose(FootballSeason *s, const gc_choice *choice,
                            unsigned evolution_choice, const char **why);
bool football_season_buy(FootballSeason *s, unsigned offer, unsigned party);
bool football_season_leave_shop(FootballSeason *s);
/* Classic item asset identity for the existing menu asset loader. */
const char *football_shop_asset_key(const FootballShopOffer *offer);
#endif
