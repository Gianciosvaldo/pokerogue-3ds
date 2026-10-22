#ifndef FOOTBALL_SHOP_UI_H
#define FOOTBALL_SHOP_UI_H
#include "playoff_season.h"
#include "playcall_ui.h"
typedef enum { FB_SHOP_IDLE, FB_SHOP_BOUGHT, FB_SHOP_REFUSED, FB_SHOP_LEFT } FootballShopAction;
typedef struct {
    unsigned selected, party;
    FootballShopAction last_action;
} FootballShopMenu;
/* target_step is -1/0/+1 for previous/no-change/next roster member. */
void football_shop_update(FootballShopMenu *menu, FootballSeason *season,
                          FootballCallInput input, int target_step);
FootballTouchRect football_shop_target_rect(void);
FootballTouchRect football_shop_offer_rect(unsigned offer);
FootballTouchRect football_shop_leave_rect(void);
#ifdef __3DS__
void football_shop_poll(FootballShopMenu *menu, FootballSeason *season);
#endif
#endif
