#ifndef FOOTBALL_DEFENSE_H
#define FOOTBALL_DEFENSE_H
#include "playcall_ui.h"
typedef enum { FB_DEF_MAN, FB_DEF_ZONE, FB_DEF_BLITZ,
    FB_DEF_COVER1_ROBBER, FB_DEF_COVER3, FB_DEF_COVER4, FB_DEF_ZONE_BLITZ, FB_DEF_COVER0, FB_DEF_COUNT } FootballDefenseId;
/* Shared responsibilities for the live AI, contact planner and previews. */
static inline bool football_defense_is_zone(FootballDefenseId id)
{ return id==FB_DEF_ZONE||id==FB_DEF_COVER3||id==FB_DEF_COVER4||id==FB_DEF_ZONE_BLITZ; }
static inline uint16_t football_defense_rush_mask(FootballDefenseId id)
{ return id==FB_DEF_COVER0?0x3fu:id==FB_DEF_BLITZ?0x2fu:id==FB_DEF_ZONE_BLITZ?0x1fu:0x0fu; }
static inline bool football_defense_is_rusher(FootballDefenseId id,unsigned rank)
{ return rank<11 && (football_defense_rush_mask(id)&(1u<<rank))!=0; }
static inline bool football_defense_is_blitz(FootballDefenseId id)
{ return id==FB_DEF_BLITZ||id==FB_DEF_ZONE_BLITZ||id==FB_DEF_COVER0; }
static inline uint16_t football_defense_deep_mask(FootballDefenseId id)
{
    switch(id){
    case FB_DEF_COVER0:return 0;
    case FB_DEF_BLITZ:return 1u<<10; /* FS rotates onto the back; SS stays high. */
    case FB_DEF_COVER1_ROBBER:return 1u<<9;
    case FB_DEF_COVER3:case FB_DEF_ZONE_BLITZ:return (1u<<7)|(1u<<8)|(1u<<9);
    case FB_DEF_COVER4:return (1u<<7)|(1u<<8)|(1u<<9)|(1u<<10);
    default:return (1u<<9)|(1u<<10);
    }
}
static inline FootballDefenseId football_defense_pick(unsigned roll,unsigned down,unsigned to_go,unsigned ball)
{
    unsigned weight[FB_DEF_COUNT]={14,14,10,12,14,14,12,10},total=0;
    if(down>=3&&to_go>=5){weight[FB_DEF_MAN]=9;weight[FB_DEF_BLITZ]*=3;weight[FB_DEF_ZONE_BLITZ]*=2;weight[FB_DEF_COVER0]*=2;weight[FB_DEF_COVER3]=18;weight[FB_DEF_COVER4]=19;}
    if(to_go<=2){weight[FB_DEF_MAN]=17;weight[FB_DEF_BLITZ]=15;weight[FB_DEF_COVER0]=18;weight[FB_DEF_COVER1_ROBBER]=16;weight[FB_DEF_COVER3]/=2;weight[FB_DEF_COVER4]/=4;}
    if(ball>=90){weight[FB_DEF_COVER3]/=2;weight[FB_DEF_COVER4]/=4;}
    for(unsigned i=0;i<FB_DEF_COUNT;++i)total+=weight[i];
    unsigned pick=(roll%10000)*total/10000;
    for(unsigned i=0;i<FB_DEF_COUNT;++i){if(pick<weight[i])return (FootballDefenseId)i;pick-=weight[i];}
    return FB_DEF_MAN;
}
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
