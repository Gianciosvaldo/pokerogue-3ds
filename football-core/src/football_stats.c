#include "football_stats.h"
#include <math.h>

/* Columns: QB, WR, RB, OL/DL, LB, TE/S. Balance defaults. */
static const float affinity[PR_TYPE_COUNT][FB_POSITION_COUNT] = {
 [POKEMON_TYPE_NORMAL]   = {1,1,1.1f,1,1,1.1f},
 [POKEMON_TYPE_FIGHTING] = {.9f,1,1.2f,1.1f,1.2f,1},
 [POKEMON_TYPE_FLYING]   = {1,1.2f,1,.9f,1,1.2f},
 [POKEMON_TYPE_POISON]   = {1,1,1,1,1.2f,1.1f},
 [POKEMON_TYPE_GROUND]   = {.9f,.9f,1.1f,1.2f,1.1f,1},
 [POKEMON_TYPE_ROCK]     = {.9f,.9f,1,1.2f,1.1f,1},
 [POKEMON_TYPE_BUG]      = {.9f,1.1f,1.1f,1,1.1f,1},
 [POKEMON_TYPE_GHOST]    = {1.1f,1.2f,1,.9f,1,1.1f},
 [POKEMON_TYPE_STEEL]    = {.9f,.9f,1,1.2f,1.2f,1.1f},
 [POKEMON_TYPE_FIRE]     = {1.1f,1.1f,1.2f,1,1,1},
 [POKEMON_TYPE_WATER]    = {1.1f,1,1.1f,1,1,1.1f},
 [POKEMON_TYPE_GRASS]    = {1,1,1,1.1f,1.1f,1.1f},
 [POKEMON_TYPE_ELECTRIC] = {1.1f,1.2f,1.1f,.9f,1,1},
 [POKEMON_TYPE_PSYCHIC]  = {1.2f,1.1f,.9f,.9f,1,1.1f},
 [POKEMON_TYPE_ICE]      = {1.1f,1.1f,.9f,1,1,1.1f},
 [POKEMON_TYPE_DRAGON]   = {1.1f,1,1.1f,1.1f,1.1f,1.1f},
 [POKEMON_TYPE_DARK]     = {1,1.1f,1.1f,1,1.2f,1},
 [POKEMON_TYPE_FAIRY]    = {1.2f,1.1f,1,.9f,1,1.1f}
};
/* SPD, AGI, STR, TAK, STA, THP, THA, CTH; each row sums to 1. */
static const float weights[FB_POSITION_COUNT][8] = {
 { .05f,.05f,0,0,.10f,.35f,.45f,0 },
 { .30f,.25f,0,0,.10f,0,0,.35f },
 { .25f,.25f,.25f,0,.15f,0,0,.10f },
 { 0,.05f,.45f,.30f,.20f,0,0,0 },
 { .15f,.10f,.20f,.40f,.15f,0,0,0 },
 { .15f,.15f,.15f,.20f,.10f,0,0,.25f }
};
static bool valid_form(const pr_form *f)
{
    if (!f) return false;
    for (unsigned i=0; i<6; ++i) if (!f->base_stats[i]) return false;
    return true;
}
static float clamp(float v) { return v<1 ? 1 : v>99 ? 99 : v; }
static uint8_t rounded(float v) { return (uint8_t)(clamp(v)+.5f); }

bool football_stats_build_maxima(FootballDexMaxima *out,
                                const pr_form *forms, size_t count)
{
    if (!out || !forms || !count) return false;
    FootballDexMaxima m = {{0}};
    for (size_t n=0; n<count; ++n) {
        if (!valid_form(&forms[n])) return false;
        for (unsigned i=0; i<6; ++i)
            if (forms[n].base_stats[i]>m.stats[i]) m.stats[i]=forms[n].base_stats[i];
    }
    *out=m;
    return true;
}
float football_stats_type_affinity(int type, FootballPosition position)
{
    if (type<0 || type>=PR_TYPE_COUNT || (unsigned)position>=FB_POSITION_COUNT)
        return 1;
    return affinity[type][position];
}
FootballPosition football_stats_best_position(const FootballPlayer *p)
{
    if (!p) return FB_QB;
    const FootballAttributes *a=&p->attributes;
    const float v[]={a->spd,a->agi,a->str,a->tak,a->sta,a->thp,a->tha,a->cth};
    float best=-1;
    FootballPosition result=FB_QB;
    for (unsigned pos=0; pos<FB_POSITION_COUNT; ++pos) {
        float score=0;
        for (unsigned i=0; i<8; ++i) score+=v[i]*weights[pos][i];
        float scalar=football_stats_type_affinity(p->primary_type, pos);
        if (p->secondary_type>=0 && p->secondary_type!=p->primary_type)
            scalar=(scalar+football_stats_type_affinity(p->secondary_type,pos))*.5f;
        score*=scalar;
        if (score>best) { best=score; result=(FootballPosition)pos; }
    }
    return result;
}
bool football_stats_generate(FootballPlayer *out, uint32_t roster_id,
                            const pr_form *f, const FootballDexMaxima *m)
{
    if (!out || !m || !valid_form(f)) return false;
    if (f->type1<0 || f->type1>=PR_TYPE_COUNT ||
        f->type2 < -1 || f->type2>=PR_TYPE_COUNT) return false;
    float n[6];
    for (unsigned i=0; i<6; ++i) {
        if (m->stats[i]<f->base_stats[i]) return false;
        n[i]=((float)f->base_stats[i]/m->stats[i])*98+1;
    }
    /* Source weight is deci-kilograms; normalize kilograms. */
    float w=clamp(log1pf(f->weight_x10*.1f)*.1447648273f*98+1);
    FootballPlayer p={0};
    p.roster_id=roster_id; p.species_id=f->species; p.form_index=f->form_index;
    p.primary_type=f->type1; p.secondary_type=f->type2;
    p.attributes.spd=rounded(n[STAT_SPD]);
    p.attributes.agi=rounded(n[STAT_SPD]*.7f+(99-w)*.3f);
    p.attributes.str=rounded(n[STAT_ATK]*.6f+w*.4f);
    p.attributes.tak=rounded(n[STAT_DEF]*.6f+w*.4f);
    p.attributes.sta=rounded(n[STAT_HP]*.6f+n[STAT_DEF]*.2f+n[STAT_SPDEF]*.2f);
    p.attributes.thp=rounded(n[STAT_ATK]*.5f+n[STAT_SPATK]*.5f);
    p.attributes.tha=rounded(n[STAT_SPATK]);
    p.attributes.cth=rounded(n[STAT_SPD]*.4f+n[STAT_SPATK]*.6f);
    p.position=football_stats_best_position(&p);
    *out=p;
    return true;
}
