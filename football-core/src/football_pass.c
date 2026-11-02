#include "football_pass.h"
static int clamp(int v,int low,int high){return v<low?low:v>high?high:v;}
static bool valid(const FootballAttributes *a)
{
    const unsigned v[]={a->spd,a->agi,a->str,a->tak,a->sta,a->thp,a->tha,a->cth};
    for(unsigned i=0;i<8;++i)if(!v[i] || v[i]>99)return false;
    return true;
}
bool football_pass_resolve(FootballPassReport *out,const FootballPassInput *in,
                            unsigned pressure_roll,unsigned catch_roll,unsigned gain_roll)
{
    if(!out || !in || pressure_roll>=100 || catch_roll>=100 || gain_roll>=100 ||
       !valid(&in->quarterback) || !valid(&in->receiver) || !valid(&in->coverer) || !valid(&in->rusher) ||
       in->protection< -12 || in->protection>12)return false;
    const FootballPlayDefinition *play=football_playbook_get(in->play);
    const FootballDefenseDefinition *defense=football_defense_get(in->defense);
    if(!play || play->type!=FB_PLAY_PASS || !defense)return false;
    FootballPassReport r={0};r.attempted=true;
    int depth,release,break_bonus;
    switch(in->play){
    case FB_PASS_SLANTS: depth=8;release=450;break_bonus=5;break;
    case FB_PASS_PLAY_ACTION: depth=12;release=1000;break_bonus=8;break;
    case FB_PASS_VERTICALS: depth=26;release=1300;break_bonus=0;break;
    case FB_PASS_POST: depth=24;release=1500;break_bonus=3;break;
    case FB_PASS_STICK: depth=6;release=450;break_bonus=5;break;
    case FB_PASS_MESH: depth=7;release=850;break_bonus=8;break;
    case FB_PASS_FLOOD: depth=16;release=1100;break_bonus=5;break;
    case FB_PASS_CURLS: depth=10;release=950;break_bonus=5;break;
    case FB_PASS_SMASH: depth=6;release=800;break_bonus=4;break;
    case FB_PASS_HB_SCREEN: depth=2;release=550;break_bonus=8;break;
    case FB_PASS_BUBBLE: depth=1;release=400;break_bonus=6;break;
    case FB_PASS_SHALLOW_CROSS: depth=5;release=700;break_bonus=7;break;
    default:return false;
    }
    const FootballAttributes *q=&in->quarterback,*rec=&in->receiver,*cover=&in->coverer,*rush=&in->rusher;
    int separation=50+((int)rec->spd+rec->agi-cover->spd-cover->agi)/4+break_bonus;
    if(football_defense_is_zone(in->defense))separation+=depth>=20 ? -8 : -2;
    if(football_defense_is_blitz(in->defense))separation+=12;
    int pressure=40+((int)rush->str+rush->agi-q->agi-50)/3-in->protection*2+(release-450)/100;
    if(football_defense_is_blitz(in->defense))pressure+=20;
    if(football_defense_is_zone(in->defense))pressure-=8;
    separation=clamp(separation,0,99);pressure=clamp(pressure,0,99);
    int short_arm=depth-(6+q->thp/2);if(short_arm<0)short_arm=0;
    r.depth_yards=(uint8_t)depth;r.release_ms=(uint16_t)release;
    r.separation=(uint8_t)separation;r.pressure=(uint8_t)pressure;
    r.completion_chance=(uint8_t)clamp(55+((int)q->tha+rec->cth-100)/4+(separation-50)/3-pressure/10-
        short_arm*2+play->completion/2+defense->completion,15,92);
    r.sack_chance=(uint8_t)clamp(2+pressure/10+(release-450)/300+defense->sack/2+play->sack/2,2,24);
    r.interception_chance=(uint8_t)clamp(4+((int)cover->cth-q->tha)/20+(50-separation)/15+
        depth/15+pressure/30+defense->interception+play->interception/2,1,20);
    r.resolved=(FootballResolvedPlay){FB_RESULT_INCOMPLETE,0,5};
    if(pressure_roll<r.sack_chance)r.resolved=(FootballResolvedPlay){FB_RESULT_SACK,-5,14};
    else if(pressure_roll<(unsigned)(r.sack_chance+r.interception_chance))
        r.resolved=(FootballResolvedPlay){FB_RESULT_INTERCEPTION,(int16_t)depth,5};
    else if(catch_roll<r.completion_chance)
        r.resolved=(FootballResolvedPlay){FB_RESULT_COMPLETE,(int16_t)clamp(depth+play->gain/4+(int)(gain_roll%7)+separation/25,1,40),12};
    *out=r;return true;
}
