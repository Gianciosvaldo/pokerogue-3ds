#include "football_playbook.h"
#include <math.h>
#include <stddef.h>

/* Six-player arcade adaptations of standard concepts. These are not the
 * browser's eleven-player formations or its real-time collision simulation. */
static const FootballPlayDefinition plays[FB_PLAY_COUNT]={
    {FB_RUN_INSIDE_ZONE,FB_FORM_SHOTGUN,FB_PLAY_RUN,"Inside Zone","Balanced blocking and inside run.",
     0,0,0,0,0,4,{{-4,2},{0,3},{7,3},{18,3}}},
    {FB_PASS_SLANTS,FB_FORM_SHOTGUN,FB_PLAY_PASS,"Quick Slants","Quick inside pass; safer, shorter gains.",
     -2,8,-2,-2,0,4,{{2,0},{6,3},{14,8},{22,12}}},
    {FB_RUN_DIVE,FB_FORM_UNDER_CENTER,FB_PLAY_RUN,"HB Dive","Downhill push behind the line.",
     0,0,0,0,0,4,{{-3,0},{0,1},{6,1},{16,1}}},
    {FB_PASS_PLAY_ACTION,FB_FORM_UNDER_CENTER,FB_PLAY_PASS,"PA Cross","Fake the run, cross the middle.",
     0,0,1,0,0,4,{{3,1},{5,6},{6,14},{8,23}}},
    {FB_RUN_DRAW,FB_FORM_SHOTGUN,FB_PLAY_RUN,"HB Draw","Delayed handoff; agility beats pursuit.",
     1,0,0,0,1,4,{{-5,2},{-3,2},{3,2},{16,2}}},
    {FB_PASS_VERTICALS,FB_FORM_SHOTGUN,FB_PLAY_PASS,"Four Verticals","Deep routes; more sacks and picks.",
     10,-16,4,5,0,4,{{6,0},{15,0},{30,1},{50,1}}},
    {FB_RUN_TOSS,FB_FORM_UNDER_CENTER,FB_PLAY_RUN,"HB Toss","Attack the edge with speed and agility.",
     0,0,0,0,2,4,{{-5,-3},{-2,-9},{5,-12},{20,-12}}},
    {FB_PASS_POST,FB_FORM_UNDER_CENTER,FB_PLAY_PASS,"PA Post","Long fake and deep break toward midfield.",
     8,-10,4,3,0,4,{{6,0},{12,0},{25,8},{40,12}}}
};
const FootballPlayDefinition *football_playbook_get(FootballPlayId id)
{
    return (unsigned)id<FB_PLAY_COUNT ? &plays[id] : NULL;
}
unsigned football_playbook_count(FootballFormation formation,FootballPlayType type)
{
    unsigned count=0;
    for (unsigned i=0;i<FB_PLAY_COUNT;++i)
        count+=plays[i].formation==formation && plays[i].type==type;
    return count;
}
const FootballPlayDefinition *football_playbook_option(FootballFormation formation,FootballPlayType type,unsigned option)
{
    for (unsigned i=0;i<FB_PLAY_COUNT;++i)
        if (plays[i].formation==formation && plays[i].type==type && option--==0) return &plays[i];
    return NULL;
}
bool football_playbook_valid(PlayInfo play)
{
    const FootballPlayDefinition *p=football_playbook_get(play.id);
    return p && p->formation==play.formation && p->type==play.type;
}
FootballRoutePoint football_playbook_route(const FootballPlayDefinition *p,float progress)
{
    FootballRoutePoint origin={0,0};
    if (!p || !p->route_count || p->route_count>FB_ROUTE_POINTS || !isfinite(progress) || progress<=0) return origin;
    if (progress>=1) return p->route[p->route_count-1];
    float segment=progress*p->route_count;
    unsigned i=(unsigned)segment;
    FootballRoutePoint a=i ? p->route[i-1] : origin,b=p->route[i];
    float t=segment-i;
    return (FootballRoutePoint){a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t};
}
