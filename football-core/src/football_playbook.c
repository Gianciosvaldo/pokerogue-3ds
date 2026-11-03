#include "football_playbook.h"
#include <math.h>
#include <stddef.h>

/* Standard concepts adapted to the eleven-player live field. */
static const FootballPlayDefinition plays[FB_PLAY_COUNT]={
    {FB_RUN_INSIDE_ZONE,FB_FORM_SHOTGUN,FB_PLAY_RUN,"Inside Zone","Balanced blocking and inside run.",
     0,0,0,0,0,4,{{-4,2},{0,3},{7,3},{18,3}}},
    {FB_PASS_SLANTS,FB_FORM_SHOTGUN,FB_PLAY_PASS,"Quick Slants","Quick slants; the back is the flat outlet.",
     -2,8,-2,-2,0,4,{{2,0},{6,3},{14,8},{22,12}}},
    {FB_RUN_DIVE,FB_FORM_UNDER_CENTER,FB_PLAY_RUN,"HB Dive","Downhill push behind the line.",
     0,0,0,0,0,4,{{-3,0},{0,1},{6,1},{16,1}}},
    {FB_PASS_PLAY_ACTION,FB_FORM_UNDER_CENTER,FB_PLAY_PASS,"PA Cross","Run fake, then a shallow and a deep crosser.",
     0,0,1,0,0,4,{{3,1},{5,6},{6,14},{8,23}}},
    {FB_RUN_DRAW,FB_FORM_SHOTGUN,FB_PLAY_RUN,"HB Draw","Pass-set look, then a late handoff.",
     1,0,0,0,1,4,{{-5,2},{-3,2},{3,2},{16,2}}},
    {FB_PASS_VERTICALS,FB_FORM_SHOTGUN,FB_PLAY_PASS,"Four Verticals","Four seams deep; the QB needs time to throw.",
     10,-16,4,5,0,4,{{6,0},{15,0},{30,1},{50,1}}},
    {FB_RUN_TOSS,FB_FORM_UNDER_CENTER,FB_PLAY_RUN,"HB Toss","Pitch to the edge behind the lead wing.",
     0,0,0,0,2,4,{{-5,-3},{-2,-9},{5,-12},{20,-12}}},
    {FB_PASS_POST,FB_FORM_UNDER_CENTER,FB_PLAY_PASS,"PA Post","Long fake and deep break toward midfield.",
     8,-10,4,3,0,4,{{6,0},{12,0},{25,8},{40,12}}},
    {FB_RUN_POWER,FB_FORM_UNDER_CENTER,FB_PLAY_RUN,"HB Power","Follow the lead block into the strong-side gap.",
     1,0,0,0,0,4,{{-3,1},{0,5},{6,6},{18,6}}},
    {FB_RUN_COUNTER,FB_FORM_UNDER_CENTER,FB_PLAY_RUN,"HB Counter","Show one gap, then cut to the opposite edge.",
     1,0,0,0,1,4,{{-4,3},{-2,-2},{4,-6},{18,-6}}},
    {FB_RUN_OUTSIDE_ZONE,FB_FORM_SHOTGUN,FB_PLAY_RUN,"Outside Zone","Stretch the edge before turning upfield.",
     1,0,0,0,2,4,{{-4,3},{0,8},{7,9},{22,9}}},
    {FB_RUN_JET_SWEEP,FB_FORM_SHOTGUN,FB_PLAY_RUN,"Jet Sweep","Hand off to WR3 and race around the edge.",
     2,0,0,0,2,4,{{-2.4f,3.5f},{-1,6.5f},{3,8.5f},{22,10}}},
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
    {FB_PASS_HB_SCREEN,FB_FORM_SHOTGUN,FB_PLAY_PASS,"HB Screen","Let the rush in, then screen to the back.",
     -1,9,-4,-2,1,4,{{6,0},{15,0},{30,1},{45,1}}},
    {FB_PASS_BUBBLE,FB_FORM_SHOTGUN,FB_PLAY_PASS,"WR Bubble","Quick throw to WR3 behind the outside blocks.",
     -2,10,-4,-2,2,4,{{4,0},{6,0},{7,1},{7,1}}},
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

/* WR2, WR3, TE and RB routes complement the primary WR1 path (route[]), one set per concept: rows are WR2, WR3, TE, RB. Routes are relative
 * to each player's own spot and inside-positive, so they hold in every alignment and mirror. The RB is the checkdown on every concept
 * but the screens, so no back runs a deep route; two receivers never share a coordinate (tests/pass_game_test.c measures it). */
enum { S_STICK,S_MESH,S_FLOOD,S_CURLS,S_SMASH,S_SCREEN,S_BUBBLE,S_SHALLOW,S_SLANTS,S_PA_CROSS,S_VERTICALS,S_PA_POST,S_COUNT };
static const FootballRoutePoint support[S_COUNT][4][FB_ROUTE_POINTS]={
    /* Stick: WR1 sticks, the others run seams, the TE and back are the flat outlets. */
    [S_STICK]={{{5,0},{12,0},{22,2},{35,3}},{{5,0},{12,0},{22,1},{35,1}},{{2,1},{4,-3},{6,-7},{10,-8}},{{0,-3},{2,-7},{5,-10},{10,-10}}},
    /* Mesh: two shallow crossers, the second a yard or two deeper so they rub; the slot goes deep, the TE drags behind them. */
    [S_MESH]={{{3,0},{5,9},{6,22},{8,29}},{{6,0},{15,0},{30,1},{42,1}},{{4,0},{9,5},{12,14},{16,20}},{{0,-3},{2,-8},{5,-12},{9,-12}}},
    [S_FLOOD]={{{4,0},{9,6},{12,16},{16,25}},{{2,-2},{4,-6},{7,-8},{11,-8}},{{3,2},{7,10},{12,23},{18,26}},{{0,3},{2,9},{4,17},{8,18}}},
    [S_CURLS]={{{4,0},{10,0},{12,1},{10,2}},{{3,0},{7,2},{9,3},{8,4}},{{4,0},{8,1},{10,2},{9,3}},{{0,-2},{3,-5},{6,-6},{8,-6}}},
    [S_SMASH]={{{5,0},{12,0},{26,0},{42,0}},{{5,0},{10,-3},{17,-8},{24,-10}},{{4,0},{10,2},{20,7},{30,10}},{{0,-3},{2,-7},{5,-10},{10,-10}}},
    /* HB Screen: the receivers run off the defense, the back slips out to the TE's side behind the line. */
    [S_SCREEN]={{{6,0},{15,0},{30,0},{45,0}},{{6,0},{15,1},{30,2},{45,3}},{{2,0},{4,-2},{6,-4},{8,-5}},{{0,-4},{1,-7},{3,-10},{7,-10}}},
    /* WR Bubble: the outside receivers stem to the corners and stay on them (they block once it is caught), WR3 swings out behind. */
    [S_BUBBLE]={{{4,0},{6,0},{7,-1},{7,-1}},{{1,-2},{2,-5},{3,-8},{5,-9}},{{2,0},{3,0},{4,0},{4,0}},{{0,-2},{2,-6},{4,-9},{6,-11}}},
    /* Shallow Cross: a shallow drag from one side, a deep over from the other, the TE dragging between them; the slot clears to the corner. */
    [S_SHALLOW]={{{5,0},{10,2},{14,9},{19,24}},{{6,0},{12,-2},{20,-6},{28,-9}},{{4,0},{9,5},{12,14},{16,20}},{{0,-3},{2,-8},{5,-12},{9,-12}}},
    /* Quick Slants: three slants (the slot's shorter), the TE settles over the middle, the back is the flat outlet. */
    [S_SLANTS]={{{2,0},{6,3},{14,8},{22,12}},{{2,0},{5,3},{9,6},{13,8}},{{3,0},{5,1},{8,3},{11,5}},{{0,-2},{2,-6},{4,-10},{7,-12}}},
    /* PA Cross: the back's route is the fake: it runs through the mesh point beside the QB (about 3.5 yards past his spot) before it bends out. A shallow drag (WR1), a deep over (WR2), the slot up the seam, the TE across behind the line. */
    [S_PA_CROSS]={{{6,0},{10,2},{15,10},{21,30}},{{5,0},{12,0},{20,1},{28,1}},{{4,0},{8,2},{11,6},{14,10}},{{4,-1.5f},{6,-5},{8,-9},{11,-12}}},
    /* Four Verticals: four seams (the two inside ones bend a little toward the middle, together, so they stay apart), the back is the checkdown instead of a fifth vertical. */
    [S_VERTICALS]={{{6,0},{15,0},{30,1},{50,1}},{{6,0},{15,1},{30,2},{50,3}},{{6,0},{15,1},{30,3},{50,4}},{{0,-3},{2,-7},{5,-10},{9,-11}}},
    /* PA Post: two posts, a dig under them from the slot, the TE leaking to the flat, the back checking down after the fake. */
    [S_PA_POST]={{{6,0},{12,0},{25,8},{40,12}},{{5,0},{10,1},{14,8},{17,16}},{{3,0},{6,-3},{9,-6},{12,-8}},{{4,-1.5f},{6,-5},{8,-9},{11,-12}}}
};
static int concept_style(FootballPlayId id)
{
    switch(id){
    case FB_PASS_STICK:return S_STICK;
    case FB_PASS_MESH:return S_MESH;
    case FB_PASS_FLOOD:return S_FLOOD;
    case FB_PASS_CURLS:return S_CURLS;
    case FB_PASS_SMASH:return S_SMASH;
    case FB_PASS_HB_SCREEN:return S_SCREEN;
    case FB_PASS_BUBBLE:return S_BUBBLE;
    case FB_PASS_SHALLOW_CROSS:return S_SHALLOW;
    case FB_PASS_SLANTS:return S_SLANTS;
    case FB_PASS_PLAY_ACTION:return S_PA_CROSS;
    case FB_PASS_VERTICALS:return S_VERTICALS;
    case FB_PASS_POST:return S_PA_POST;
    default:return -1;
    }
}
FootballRouteView football_playbook_receiver_path(const FootballPlayDefinition *p,unsigned rank)
{
    if(!p || rank<1 || rank>5)return (FootballRouteView){NULL,0};
    int style=concept_style(p->id);
    if(style<0 || rank==2)return (FootballRouteView){p->route,p->route_count};
    unsigned role=rank==1?3:rank-3;
    return (FootballRouteView){support[style][role],FB_ROUTE_POINTS};
}
unsigned football_playbook_primary_receiver(const FootballPlayDefinition *p)
{
    return p&&p->id==FB_PASS_HB_SCREEN?4:p&&p->id==FB_PASS_BUBBLE?2:0;
}
unsigned football_playbook_runner(const FootballPlayDefinition *p){return p&&p->id==FB_RUN_JET_SWEEP?4:1;}
/* Alignments (unmirrored, low y is the "left"). SPREAD is the 2x2 every play used to start from. TRIPS puts three receivers on one side
 * and one on the other. TIGHT keeps the TE on the line with a wing behind him, the back deeper and straight behind the QB. JET lines the
 * slot receiver up in the backfield beside the QB, the back on the sweep side to lead it, and the TE close enough to seal. */
static const FootballAlignment alignments[FB_ALIGN_COUNT]={
    {"Spread",1.33f,0,{{-1,4},{-1,49},{-1,13},{-1,39}}},
    {"Trips",1.33f,0,{{-1,3.5f},{-1,49.5f},{-1,9.5f},{-1.5f,16}}},
    {"Tight",0,-.5f,{{-1,6},{-1,47.5f},{-1.5f,38.2f},{0,34.8f}}},
    {"Jet",6,2.5f,{{-1,6},{-1,48},{-3.5f,24.2f},{-1,36}}}
};
const FootballAlignment *football_playbook_alignment(FootballAlignmentId id){return (unsigned)id<FB_ALIGN_COUNT?&alignments[id]:&alignments[FB_ALIGN_SPREAD];}
FootballAlignmentId football_playbook_alignment_for(const FootballPlayDefinition *p,unsigned option)
{
    if(!p)return FB_ALIGN_SPREAD;
    switch(p->id){
    case FB_RUN_JET_SWEEP:return FB_ALIGN_JET;
    case FB_RUN_INSIDE_ZONE:return option?FB_ALIGN_TIGHT:FB_ALIGN_SPREAD;
    case FB_RUN_DRAW:return option?FB_ALIGN_TRIPS:FB_ALIGN_SPREAD;
    case FB_RUN_OUTSIDE_ZONE:return option?FB_ALIGN_TRIPS:FB_ALIGN_TIGHT; /* the wing is the lead blocker, on the strong side */
    case FB_RUN_DIVE:return option?FB_ALIGN_SPREAD:FB_ALIGN_TIGHT;
    case FB_RUN_TOSS:case FB_RUN_POWER:case FB_RUN_COUNTER:return FB_ALIGN_TIGHT; /* the wing is the lead blocker */
    /* Passes: the picture each concept is usually run from, and a second one a third of the time. */
    case FB_PASS_SLANTS:case FB_PASS_VERTICALS:case FB_PASS_CURLS:return option?FB_ALIGN_TRIPS:FB_ALIGN_SPREAD;
    case FB_PASS_STICK:return option?FB_ALIGN_SPREAD:FB_ALIGN_TRIPS;
    case FB_PASS_SMASH:case FB_PASS_BUBBLE:return option?FB_ALIGN_TIGHT:FB_ALIGN_SPREAD; /* both send the slot to the sideline, which a trips side has no room for */
    case FB_PASS_PLAY_ACTION:case FB_PASS_POST:return option?FB_ALIGN_SPREAD:FB_ALIGN_TIGHT; /* the fake wants the back close to the QB */
    case FB_PASS_FLOOD:return option?FB_ALIGN_SPREAD:FB_ALIGN_TIGHT; /* the TE crosses the whole field: not from a trips side */
    case FB_PASS_MESH:case FB_PASS_SHALLOW_CROSS:case FB_PASS_HB_SCREEN:return option?FB_ALIGN_TIGHT:FB_ALIGN_SPREAD;
    default:return FB_ALIGN_SPREAD;
    }
}
FootballLead football_playbook_lead(const FootballPlayDefinition *p)
{
    if(!p)return FB_LEAD_NONE;
    switch(p->id){
    case FB_RUN_JET_SWEEP:return FB_LEAD_RB;
    case FB_RUN_POWER:case FB_RUN_COUNTER:case FB_RUN_TOSS:case FB_RUN_OUTSIDE_ZONE:return FB_LEAD_WING;
    default:return FB_LEAD_NONE;
    }
}
FootballHandoff football_playbook_handoff(const FootballPlayDefinition *p)
{
    if(!p)return (FootballHandoff){FB_MESH_LINED,1.4f,0};
    switch(p->id){
    case FB_RUN_DRAW:return (FootballHandoff){FB_MESH_LINED,1.4f,.25f};
    case FB_RUN_TOSS:return (FootballHandoff){FB_MESH_STRONG,3.2f,0};
    case FB_RUN_OUTSIDE_ZONE:return (FootballHandoff){FB_MESH_STRONG,2.2f,0};
    case FB_RUN_COUNTER:return (FootballHandoff){FB_MESH_LINED,1.4f,.2f};
    case FB_RUN_JET_SWEEP:return (FootballHandoff){FB_MESH_STRONG,1.0f,0};
    default:return (FootballHandoff){FB_MESH_LINED,1.4f,0};
    }
}
bool football_playbook_strong_side(const FootballPlayDefinition *p){return football_playbook_lead(p)!=FB_LEAD_NONE;}
bool football_playbook_screen(const FootballPlayDefinition *p){return p&&(p->id==FB_PASS_HB_SCREEN||p->id==FB_PASS_BUBBLE);}
float football_playbook_window(const FootballPlayDefinition *p)
{
    if(!p)return .85f;
    switch(p->id){
    case FB_PASS_HB_SCREEN:return 1.45f; /* the back has to get out and the wall to form */
    case FB_PASS_BUBBLE:return .45f;
    default:return .85f+(p->gain>0?p->gain:0)*.11f+(p->formation==FB_FORM_UNDER_CENTER?.4f:0);
    }
}
bool football_playbook_fake(const FootballPlayDefinition *p){return p&&(p->id==FB_PASS_PLAY_ACTION||p->id==FB_PASS_POST);}
bool football_playbook_settles(const FootballPlayDefinition *p)
{
    if(!p)return false;
    switch(p->id){
    case FB_PASS_STICK:case FB_PASS_CURLS:case FB_PASS_SMASH:case FB_PASS_PLAY_ACTION:case FB_PASS_SHALLOW_CROSS:case FB_PASS_BUBBLE:return true;
    default:return false;
    }
}
