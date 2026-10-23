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
bool football_contact_tackle(FootballContactResult *out,const FootballAttributes *c,
                              const FootballAttributes *d,bool just_caught,
                              unsigned tackle_roll,unsigned fumble_roll)
{
    if (!out || !valid(c) || !valid(d) || tackle_roll>=100 || fumble_roll>=100) return false;
    FootballContactResult r={0};r.attempted=true;
    /* An estimated closing-speed impact for the snap abstraction. This does
     * not claim to integrate real positions/velocities from physics.js. */
    int closing=(d->spd+c->spd)/2;
    r.impact=(uint8_t)clamp((d->str*3+closing*2)/5,1,99);
    r.big_hit=r.impact>=70;
    int resistance=(c->str+c->agi)/2;
    r.tackle_chance=(uint8_t)clamp(70+(d->tak-resistance)/3+(just_caught ? 10 : 0),25,95);
    r.broken=tackle_roll>=r.tackle_chance;
    if (r.broken) {
        r.extra_yards=(uint8_t)clamp(2+(c->spd+c->agi-d->tak)/30,1,7);
    } else {
        int toughness=(c->sta+c->str)/2;
        r.fumble_chance=(uint8_t)clamp(2+r.impact*2/(toughness<20 ? 20 : toughness)+(r.big_hit ? 2 : 0),1,12);
        r.fumble=fumble_roll<r.fumble_chance;
    }
    *out=r;return true;
}
