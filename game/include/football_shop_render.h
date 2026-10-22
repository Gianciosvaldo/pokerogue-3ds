#ifndef FOOTBALL_SHOP_RENDER_H
#define FOOTBALL_SHOP_RENDER_H
#include "football_render.h"
#include "football_shop_ui.h"
typedef struct {
    const rr_frame *icons[FB_SHOP_CAP]; /* Borrowed from the existing mui cache. */
    const char *names[FB_SHOP_CAP];
} FootballShopView;
/* Call once when entering SHOP, outside the render loop and after mui_init.
 * Borrowed icons expire at mui_unload_all. No new item assets are introduced. */
bool football_shop_prepare(FootballShopView *view, const FootballSeason *season);
/* Bottom target must already be selected by the caller's rr frame. */
void football_shop_draw(FootballRenderer *renderer, const FootballShopView *view,
                        const FootballSeason *season, const FootballShopMenu *menu);
#endif
