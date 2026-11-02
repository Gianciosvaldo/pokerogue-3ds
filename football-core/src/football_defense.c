#include "football_defense.h"
#include <stddef.h>
#include <string.h>
static const FootballDefenseDefinition defenses[FB_DEF_COUNT]={
    {"2-Man","Five man matchups with two deep safeties.",0,-4,1,0},
    {"Cover 2 Zone","Two deep safeties and five underneath zones.",-2,-8,3,2},
    {"Man Blitz","Five rushers, man coverage, one deep safety.",8,6,-2,-1},
    {"Cover 1 Robber","One deep safety; a robber reads the middle.",1,-5,3,0},
    {"Cover 3","Three deep thirds and four underneath zones.",-1,-9,3,2},
    {"Cover 4 Quarters","Four deep quarters; protect the big play.",-4,-12,4,4},
    {"Zone Blitz","Five rushers, three underneath, three deep.",6,1,1,0},
    {"Cover 0 Blitz","Six rushers; five isolated man assignments.",11,10,-3,-2}
};
const FootballDefenseDefinition *football_defense_get(FootballDefenseId id)
{
    return (unsigned)id<FB_DEF_COUNT ? &defenses[id] : NULL;
}
FootballTouchRect football_defense_rect(unsigned option)
{
    return option<FB_DEF_COUNT ? (FootballTouchRect){16+(int)(option/4)*148,44+(int)(option%4)*38,140,32} : (FootballTouchRect){0,0,0,0};
}
void football_defense_update(FootballDefenseMenu *m,FootballCallInput in)
{
    if(!m || m->ready)return;
    if(m->selected>=FB_DEF_COUNT)m->selected=0;
    if(in.pressed&FB_INPUT_BACK){m->selected=0;return;}
    if(in.pressed&FB_INPUT_UP)m->selected=(m->selected+FB_DEF_COUNT-1)%FB_DEF_COUNT;
    else if(in.pressed&FB_INPUT_DOWN)m->selected=(m->selected+1)%FB_DEF_COUNT;
    bool activate=(in.pressed&FB_INPUT_ACCEPT)!=0;
    if(in.touch_down) {
        activate=false;
        for(unsigned i=0;i<FB_DEF_COUNT;++i){
            FootballTouchRect r=football_defense_rect(i);
            if(in.touch_x>=r.x && in.touch_x<r.x+r.w && in.touch_y>=r.y && in.touch_y<r.y+r.h){m->selected=i;activate=true;break;}
        }
    }
    if(activate){m->pending=(FootballDefenseId)m->selected;m->ready=true;}
}
bool football_defense_take(FootballDefenseMenu *m,FootballDefenseId *out)
{
    if(!m || !out || !m->ready)return false;
    *out=m->pending;memset(m,0,sizeof *m);return true;
}
