#include "evolution_ui.h"
#include <string.h>
bool football_evolution_open(FootballEvolutionMenu *menu, const uint16_t *species,
                             unsigned count)
{
    if (!menu || !species || !count || count>FB_EVOLUTION_CAP) return false;
    memset(menu,0,sizeof *menu);
    memcpy(menu->species,species,count*sizeof *species);
    menu->count=count; menu->active=true;
    return true;
}
unsigned football_evolution_page_start(const FootballEvolutionMenu *menu)
{
    return menu ? (menu->selected/FB_EVOLUTION_VISIBLE)*FB_EVOLUTION_VISIBLE : 0;
}
FootballTouchRect football_evolution_rect(unsigned row)
{
    return row<FB_EVOLUTION_VISIBLE ? (FootballTouchRect){20,60+(int)row*48,280,40} :
                                     (FootballTouchRect){0,0,0,0};
}
void football_evolution_update(FootballEvolutionMenu *menu, FootballCallInput input)
{
    if (!menu || !menu->active || menu->ready || !menu->count || menu->count>FB_EVOLUTION_CAP) return;
    if (input.pressed&FB_INPUT_BACK) {
        menu->cancelled=true; menu->ready=true; return;
    }
    unsigned count=menu->count+1; /* Last option cancels evolution. */
    if (input.pressed&FB_INPUT_UP) menu->selected=(menu->selected+count-1)%count;
    else if (input.pressed&FB_INPUT_DOWN) menu->selected=(menu->selected+1)%count;
    bool activate=(input.pressed&FB_INPUT_ACCEPT)!=0;
    if (input.touch_down) {
        activate=false;
        unsigned page=football_evolution_page_start(menu);
        for (unsigned row=0;row<FB_EVOLUTION_VISIBLE && page+row<count;++row) {
            FootballTouchRect r=football_evolution_rect(row);
            if (input.touch_x>=r.x && input.touch_y>=r.y && input.touch_x<r.x+r.w && input.touch_y<r.y+r.h) {
                menu->selected=page+row; activate=true; break;
            }
        }
    }
    if (activate) { menu->ready=true; menu->cancelled=menu->selected==menu->count; }
}
bool football_evolution_take(FootballEvolutionMenu *menu, unsigned *choice, bool *cancelled)
{
    if (!menu || !choice || !cancelled || !menu->active || !menu->ready) return false;
    *choice=menu->selected; *cancelled=menu->cancelled;
    menu->active=false;
    return true;
}
#ifdef __3DS__
void football_evolution_poll(FootballEvolutionMenu *menu)
{
    football_evolution_update(menu,football_playcall_read_input());
}
#endif
