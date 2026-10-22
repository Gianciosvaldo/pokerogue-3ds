#ifndef FOOTBALL_EVOLUTION_UI_H
#define FOOTBALL_EVOLUTION_UI_H
#include "playcall_ui.h"
#define FB_EVOLUTION_CAP 16
#define FB_EVOLUTION_VISIBLE 3
/* Opens only after the season reports an evolution prompt. No default outcome
 * is committed: player must tap/A on a species or press B/Cancel. */
typedef struct {
    uint16_t species[FB_EVOLUTION_CAP];
    unsigned count, selected;
    bool active, ready, cancelled;
} FootballEvolutionMenu;
bool football_evolution_open(FootballEvolutionMenu *menu, const uint16_t *species,
                             unsigned count);
void football_evolution_update(FootballEvolutionMenu *menu, FootballCallInput input);
bool football_evolution_take(FootballEvolutionMenu *menu, unsigned *choice, bool *cancelled);
unsigned football_evolution_page_start(const FootballEvolutionMenu *menu);
FootballTouchRect football_evolution_rect(unsigned visible_row);
#ifdef __3DS__
void football_evolution_poll(FootballEvolutionMenu *menu);
#endif
#endif
