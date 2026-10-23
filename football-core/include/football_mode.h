#ifndef FOOTBALL_MODE_H
#define FOOTBALL_MODE_H
#include "football_playoff_match.h"
#include "football_shop_ui.h"
#include "evolution_ui.h"
typedef enum {
    FB_MODE_DRAFT, FB_MODE_ROUND, FB_MODE_PLAY, FB_MODE_RESULT, FB_MODE_EVENTS,
    FB_MODE_MESSAGE, FB_MODE_EVOLVE, FB_MODE_LEARN, FB_MODE_SHOP, FB_MODE_END, FB_MODE_ERROR
} FootballModePhase;
typedef struct {
    FootballSeason season;
    FootballPlayoffMatch session;
    FootballPlaycall playcall;
    FootballEvolutionMenu evolution;
    FootballShopMenu shop;
    FootballModePhase phase;
    unsigned move_choice;
    bool done, profile_dirty;
    char message[256];
} FootballMode;
bool football_mode_new(FootballMode *mode, gc_run *roster, gc_system *profile, uint32_t entropy);
bool football_mode_draft(FootballMode *mode, const gc_starter *team, int count, const char **why);
void football_mode_tick(FootballMode *mode, uint32_t elapsed_ms);
/* One input event at a time. The app owns quit confirmation and key repeat policy. */
void football_mode_input(FootballMode *mode, FootballCallInput input, int target_step);
unsigned football_mode_move_count(const FootballMode *mode);
const char *football_mode_move_name(const FootballMode *mode, unsigned option);
const char *football_mode_new_move_name(const FootballMode *mode);
FootballTouchRect football_mode_move_rect(unsigned option);
#endif
