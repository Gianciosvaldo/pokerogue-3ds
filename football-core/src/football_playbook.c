#include "football_playbook.h"
#include <math.h>
#include <stddef.h>

/* Standard concepts adapted to the eleven-player live field. */
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
     8,-10,4,3,0,4,{{6,0},{12,0},{25,8},{40,12}}},
    {FB_RUN_POWER,FB_FORM_UNDER_CENTER,FB_PLAY_RUN,"HB Power","Follow the lead block into the strong-side gap.",
     1,0,0,0,0,4,{{-3,1},{0,5},{6,6},{18,6}}},
    {FB_RUN_COUNTER,FB_FORM_UNDER_CENTER,FB_PLAY_RUN,"HB Counter","Show one gap, then cut to the opposite edge.",
     1,0,0,0,1,4,{{-4,3},{-2,-2},{4,-6},{18,-6}}},
    {FB_RUN_OUTSIDE_ZONE,FB_FORM_SHOTGUN,FB_PLAY_RUN,"Outside Zone","Stretch the edge before turning upfield.",
     1,0,0,0,2,4,{{-4,4},{0,11},{7,12},{22,12}}},
    {FB_RUN_JET_SWEEP,FB_FORM_SHOTGUN,FB_PLAY_RUN,"Jet Sweep","Hand off to WR3 and race around the edge.",
     2,0,0,0,2,4,{{-1,10},{0,17},{6,18},{22,18}}},
    {FB_PASS_STICK,FB_FORM_SHOTGUN,FB_PLAY_PASS,"Stick","Quick stick route with flat and seam outlets.",
     -1,9,-3,-2,0,4,{{3,0},{6,1},{7,3},{7,3}}},
    {FB_PASS_MESH,FB_FORM_SHOTGUN,FB_PLAY_PASS,"Mesh","Shallow crossers create traffic underneath.",
     0,6,-1,0,1,4,{{2,0},{4,5},{5,17},{7,26}}},
    {FB_PASS_FLOOD,FB_FORM_UNDER_CENTER,FB_PLAY_PASS,"Flood","Stretch one sideline at three depths.",
     4,0,1,1,0,4,{{4,0},{10,1},{16,-1},{27,-1}}},
    {FB_PASS_CURLS,FB_FORM_SHOTGUN,FB_PLAY_PASS,"Curls","Settle in front of deep coverage.",
     0,5,-1,0,0,4,{{4,0},{10,0},{12,1},{10,2}}},
    {FB_PASS_SMASH,FB_FORM_UNDER_CENTER,FB_PLAY_PASS,"Smash","A short hitch under an inside corner route.",
     3,1,1,1,0,4,{{3,0},{6,0},{7,1},{6,1}}},
    {FB_PASS_HB_SCREEN,FB_FORM_SHOTGUN,FB_PLAY_PASS,"HB Screen","Invite pressure, then find the back in space.",
     -1,9,-4,-2,1,4,{{6,0},{15,0},{30,1},{45,1}}},
    {FB_PASS_BUBBLE,FB_FORM_SHOTGUN,FB_PLAY_PASS,"WR Bubble","Throw quickly to WR3 outside the formation.",
     -2,10,-4,-2,2,4,{{6,0},{15,0},{30,0},{45,0}}},
    {FB_PASS_SHALLOW_CROSS,FB_FORM_UNDER_CENTER,FB_PLAY_PASS,"Shallow Cross","Find the drag under a clearing deep route.",
     0,7,-1,0,1,4,{{2,0},{3,7},{4,19},{6,29}}}
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
FootballRoutePoint football_playbook_sample_path(FootballRouteView route,float progress)
{
    FootballRoutePoint origin={0,0};
    if(!route.point || !route.count || route.count>FB_ROUTE_POINTS || !isfinite(progress) || progress<=0)return origin;
    if(progress>=1)return route.point[route.count-1];
    float segment=progress*route.count;unsigned i=(unsigned)segment;
    FootballRoutePoint start=i?route.point[i-1]:origin,end=route.point[i];float t=segment-i;
    return (FootballRoutePoint){start.x+(end.x-start.x)*t,start.y+(end.y-start.y)*t};
}
FootballRoutePoint football_playbook_route(const FootballPlayDefinition *p,float progress)
{
    return football_playbook_sample_path((FootballRouteView){p?p->route:NULL,p?p->route_count:0},progress);
}

/* WR2, WR3, TE and RB routes complement the primary WR1 path. The four
 * original passing concepts keep their existing geometry. */
static const FootballRoutePoint support[6][4][FB_ROUTE_POINTS]={
    {{{5,0},{12,0},{22,2},{35,3}},{{5,0},{12,0},{22,1},{35,1}},{{2,1},{4,-3},{6,-7},{10,-8}},{{0,-3},{2,-7},{5,-10},{10,-10}}},
    {{{2,0},{3,8},{4,20},{6,28}},{{6,0},{15,0},{30,1},{42,1}},{{4,0},{9,5},{12,14},{16,20}},{{0,-3},{2,-8},{5,-12},{9,-12}}},
    {{{4,0},{9,6},{12,16},{16,25}},{{2,-2},{4,-6},{7,-8},{11,-8}},{{3,2},{7,10},{12,23},{18,26}},{{0,3},{2,9},{4,17},{8,18}}},
    {{{4,0},{10,0},{12,1},{10,2}},{{3,0},{7,2},{9,3},{8,4}},{{3,0},{7,2},{9,3},{8,4}},{{0,-2},{3,-5},{6,-6},{8,-6}}},
    {{{5,0},{12,0},{26,0},{42,0}},{{5,0},{10,-3},{17,-8},{24,-10}},{{4,0},{10,2},{20,7},{30,10}},{{0,-3},{2,-7},{5,-10},{10,-10}}},
    {{{6,0},{15,0},{30,0},{45,0}},{{0,-2},{1,-5},{3,-8},{8,-8}},{{4,0},{10,1},{17,3},{24,3}},{{0,-4},{1,-7},{3,-10},{7,-10}}}
};
FootballRouteView football_playbook_receiver_path(const FootballPlayDefinition *p,unsigned rank)
{
    if(!p || rank<1 || rank>5)return (FootballRouteView){NULL,0};
    unsigned style=0;
    switch(p->id){
    case FB_PASS_STICK:style=0;break;
    case FB_PASS_MESH:case FB_PASS_SHALLOW_CROSS:style=1;break;
    case FB_PASS_FLOOD:style=2;break;
    case FB_PASS_CURLS:style=3;break;
    case FB_PASS_SMASH:style=4;break;
    case FB_PASS_HB_SCREEN:case FB_PASS_BUBBLE:style=5;break;
    default:return (FootballRouteView){p->route,p->route_count};
    }
    if(rank==2)return (FootballRouteView){p->route,p->route_count};
    unsigned role=rank==1?3:rank-3;
    return (FootballRouteView){support[style][role],FB_ROUTE_POINTS};
}
unsigned football_playbook_primary_receiver(const FootballPlayDefinition *p)
{
    return p&&p->id==FB_PASS_HB_SCREEN?4:p&&p->id==FB_PASS_BUBBLE?2:0;
}
unsigned football_playbook_runner(const FootballPlayDefinition *p){return p&&p->id==FB_RUN_JET_SWEEP?4:1;}
bool football_playbook_screen(const FootballPlayDefinition *p){return p&&(p->id==FB_PASS_HB_SCREEN||p->id==FB_PASS_BUBBLE);}
bool football_playbook_settles(const FootballPlayDefinition *p){return p&&(p->id==FB_PASS_STICK||p->id==FB_PASS_CURLS||p->id==FB_PASS_SMASH);}
