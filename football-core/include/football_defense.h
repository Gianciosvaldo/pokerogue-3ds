#ifndef FOOTBALL_DEFENSE_H
#define FOOTBALL_DEFENSE_H
#include "playcall_ui.h"
typedef enum { FB_DEF_MAN, FB_DEF_ZONE, FB_DEF_BLITZ, FB_DEF_COUNT } FootballDefenseId;
typedef struct {
    const char *name,*description;
    int8_t sack,completion,interception,run_gain;
} FootballDefenseDefinition;
typedef struct { unsigned selected; bool ready; FootballDefenseId pending; } FootballDefenseMenu;
const FootballDefenseDefinition *football_defense_get(FootballDefenseId id);
FootballTouchRect football_defense_rect(unsigned option);
void football_defense_update(FootballDefenseMenu *menu,FootballCallInput input);
bool football_defense_take(FootballDefenseMenu *menu,FootballDefenseId *out);
#endif
