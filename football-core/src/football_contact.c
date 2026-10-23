#include "football_contact.h"
static int clamp(int v,int low,int high) { return v<low ? low : v>high ? high : v; }
static bool valid(const FootballAttributes *a)
{
    if (!a) return false;
    const unsigned v[]={a->spd,a->agi,a->str,a->tak,a->sta,a->thp,a->tha,a->cth};
    for (unsigned i=0;i<8;++i) if (!v[i] || v[i]>99) return false;
    return true;
}
bool football_contact_block(FootballBlockResult *out,const FootballAttributes *b,
                             const FootballAttributes *d,unsigned roll)
{
    if (!out || !valid(b) || !valid(d) || roll>=100) return false;
    /* Anchor/power against power/agility, following the reference engagement
     * model. STR and TAK already contain the Pokemon's normalized weight. */
    int anchor=(55*b->tak+45*b->str)/100;
    int shed=(65*d->str+35*d->agi)/100;
    int edge=anchor-shed;
    bool pancake=b->str>d->str+25 && roll<(unsigned)clamp((b->str-d->str)/2,0,35);
    FootballBlockResult r={0};
    r.hold_ms=(uint16_t)clamp(700+edge*8+(int)roll*4-198,200,1600);
    r.edge=(int8_t)clamp(edge/8+((int)r.hold_ms-700)/150+(pancake ? 4 : 0),-12,12);
    r.pancake=pancake;
    *out=r;return true;
}
bool football_contact_tactical(FootballContactResult *out,const FootballAttributes *c,
                              const FootballAttributes *d,bool just_caught,FootballContactTactic tactic,bool dive,
                              unsigned tackle_roll,unsigned fumble_roll)
{
    if (!out || !valid(c) || !valid(d) || tackle_roll>=100 || fumble_roll>=100 || (unsigned)tactic>=FB_CONTACT_COUNT) return false;
    FootballContactResult r={0};r.attempted=true;
    /* An estimated closing-speed impact for the snap abstraction. This does
     * not claim to integrate real positions/velocities from physics.js. */
    int closing=(d->spd+c->spd)/2;
    r.impact=(uint8_t)clamp((d->str*3+closing*2)/5,1,99);
    r.impact=(uint8_t)clamp(r.impact+(dive ? 8 : 0),1,99);
    r.big_hit=r.impact>=70;
    int resistance=(c->str+c->agi)/2;
    int evade=tactic==FB_CONTACT_JUKE ? clamp(3+((int)c->agi-d->agi)/8,0,14) :
        tactic==FB_CONTACT_POWER ? clamp(2+((int)c->str-d->str)/8,0,12) : 0;
    int dive_bonus=dive ? clamp(5+((int)d->agi-c->agi)/12,0,10) : 0;
    r.tackle_chance=(uint8_t)clamp(70+(d->tak-resistance)/3+(just_caught ? 10 : 0)-evade+dive_bonus,25,95);
    r.broken=tackle_roll>=r.tackle_chance;
    if (r.broken) {
        r.extra_yards=(uint8_t)clamp(2+(c->spd+c->agi-d->tak)/30+(dive ? 2 : 0),1,dive ? 9 : 7);
    } else {
        int toughness=(c->sta+c->str)/2;
        r.fumble_chance=(uint8_t)clamp(2+r.impact*2/(toughness<20 ? 20 : toughness)+(r.big_hit ? 2 : 0)+(tactic!=FB_CONTACT_BALANCED ? 1 : 0),1,12);
        r.fumble=fumble_roll<r.fumble_chance;
    }
    *out=r;return true;
}

bool football_contact_tackle(FootballContactResult *out,const FootballAttributes *c,
                              const FootballAttributes *d,bool just_caught,unsigned tackle_roll,unsigned fumble_roll)
{
    return football_contact_tactical(out,c,d,just_caught,FB_CONTACT_BALANCED,false,tackle_roll,fumble_roll);
}
bool football_contact_recover(FootballRecoveryResult *out,const FootballAttributes *offense,const FootballAttributes *defense,unsigned roll)
{
    if(!out || !valid(offense) || !valid(defense) || roll>=100)return false;
    int edge=((int)offense->spd+offense->agi+offense->cth-defense->spd-defense->agi-defense->cth)/6;
    FootballRecoveryResult r={true,false,(uint8_t)clamp(50+edge,15,85)};r.offense_keeps=roll<r.offense_chance;
    *out=r;return true;
}
const char *football_contact_tactic_name(FootballContactTactic tactic)
{
    const char *names[]={"Balanced","Juke","Power"};return (unsigned)tactic<FB_CONTACT_COUNT ? names[tactic] : "";
}
