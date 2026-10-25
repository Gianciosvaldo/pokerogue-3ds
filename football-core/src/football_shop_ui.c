#include "football_shop_ui.h"
#ifdef __3DS__
#include <3ds.h>
#endif
FootballTouchRect football_shop_target_rect(void) { return (FootballTouchRect){12,32,296,28}; }
FootballTouchRect football_shop_offer_rect(unsigned offer)
{
    return offer<FB_SHOP_CAP ? (FootballTouchRect){12,64+(int)offer*40,296,36} :
                              (FootballTouchRect){0,0,0,0};
}
FootballTouchRect football_shop_leave_rect(void) { return (FootballTouchRect){12,188,296,26}; }
static bool hit(FootballTouchRect r, unsigned x, unsigned y)
{
    return (int)x>=r.x && (int)y>=r.y && (int)x<r.x+r.w && (int)y<r.y+r.h;
}
void football_shop_update(FootballShopMenu *m, FootballSeason *s,
                          FootballCallInput input, int step)
{
    if (!m || !s || !s->roster || s->phase!=FB_SEASON_SHOP ||
        s->roster->n_party<1 || s->roster->n_party>FB_TEAM_CAP || s->shop_count>FB_SHOP_CAP) return;
    unsigned party_count=(unsigned)s->roster->n_party;
    if (m->party>=party_count) m->party=0;
    if (m->selected>s->shop_count) m->selected=0;
    if (input.pressed&FB_INPUT_BACK) {
        if (football_season_leave_shop(s)) m->last_action=FB_SHOP_LEFT;
        return;
    }
    if (step) {
        m->party=(m->party+(step<0 ? party_count-1 : 1))%party_count;
        m->last_action=FB_SHOP_IDLE;
        return;
    }
    unsigned count=s->shop_count+1;
    if (input.pressed&FB_INPUT_UP) m->selected=(m->selected+count-1)%count;
    else if (input.pressed&FB_INPUT_DOWN) m->selected=(m->selected+1)%count;
    bool activate=(input.pressed&FB_INPUT_ACCEPT)!=0;
    if (input.touch_down) {
        activate=false;
        if (hit(football_shop_target_rect(),input.touch_x,input.touch_y)) {
            m->party=(m->party+1)%party_count; m->last_action=FB_SHOP_IDLE; return;
        }
        for (unsigned i=0;i<s->shop_count;++i)
            if (hit(football_shop_offer_rect(i),input.touch_x,input.touch_y)) {
                m->selected=i; activate=true; break;
            }
        if (hit(football_shop_leave_rect(),input.touch_x,input.touch_y)) {
            m->selected=s->shop_count; activate=true;
        }
    }
    if (!activate) return;
    if (m->selected==s->shop_count) {
        if (football_season_leave_shop(s)) m->last_action=FB_SHOP_LEFT;
    } else m->last_action=football_season_buy(s,m->selected,m->party) ? FB_SHOP_BOUGHT : FB_SHOP_REFUSED;
}
#ifdef __3DS__
void football_shop_poll(FootballShopMenu *menu, FootballSeason *season)
{
    FootballCallInput input=football_playcall_read_input();
    u32 keys=hidKeysDown();
    int step=keys&KEY_LEFT ? -1 : keys&KEY_RIGHT ? 1 : 0;
    football_shop_update(menu,season,input,step);
}
#endif
