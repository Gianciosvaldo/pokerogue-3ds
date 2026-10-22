#include "football_stats.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>

int main(void)
{
    pr_form f={0};
    f.species=25; f.type1=POKEMON_TYPE_ELECTRIC; f.type2=-1;
    f.weight_x10=990; /* 99 kg => log(100)/log(1000) = 2/3 */
    for (unsigned i=0;i<6;++i) f.base_stats[i]=50;
    pr_form dex[2]={f,f};
    for (unsigned i=0;i<6;++i) dex[1].base_stats[i]=100;
    FootballDexMaxima m;
    FootballPlayer p;
    assert(football_stats_build_maxima(&m,dex,2));
    assert(football_stats_generate(&p,42,&f,&m));
    assert(p.roster_id==42 && p.species_id==25);
    /* Normalized stats=50, weight=66.333; independently calculated results. */
    assert(p.attributes.spd==50 && p.attributes.agi==45);
    assert(p.attributes.str==57 && p.attributes.tak==57);
    assert(p.attributes.sta==50 && p.attributes.thp==50);
    assert(p.attributes.tha==50 && p.attributes.cth==50);
    FootballPlayer saved=p;
    assert(!football_stats_generate(&p,0,NULL,&m));
    assert(memcmp(&saved,&p,sizeof p)==0);
    FootballDexMaxima old=m;
    assert(!football_stats_build_maxima(&m,dex,0));
    assert(memcmp(&old,&m,sizeof m)==0);
    dex[0].base_stats[0]=0;
    assert(!football_stats_build_maxima(&m,dex,2));
    assert(memcmp(&old,&m,sizeof m)==0);
    m.stats[0]=1;
    assert(!football_stats_generate(&p,0,&f,&m));
    m=old;
    f.weight_x10=65535;
    assert(football_stats_generate(&p,0,&f,&m));
    assert(p.attributes.str==70); /* Weight saturates at 99. */
    f.weight_x10=0;
    assert(football_stats_generate(&p,0,&f,&m));
    assert(p.attributes.str==30);
    f.type2=POKEMON_TYPE_STELLAR;
    assert(!football_stats_generate(&p,0,&f,&m));
    assert(football_stats_type_affinity(-1,FB_QB)==1);
    assert(football_stats_type_affinity(0,FB_POSITION_COUNT)==1);
    /* Affinity alone decides otherwise equal scores. */
    memset(&p,0,sizeof p);
    p.attributes=(FootballAttributes){50,50,50,50,50,50,50,50};
    p.primary_type=POKEMON_TYPE_PSYCHIC; p.secondary_type=-1;
    assert(football_stats_best_position(&p)==FB_QB);
    p.secondary_type=POKEMON_TYPE_PSYCHIC;
    assert(football_stats_best_position(&p)==FB_QB);
    p.primary_type=POKEMON_TYPE_ELECTRIC; p.secondary_type=-1;
    assert(football_stats_best_position(&p)==FB_WR);
    p.secondary_type=POKEMON_TYPE_PSYCHIC;
    assert(football_stats_best_position(&p)==FB_QB);
    assert(football_stats_build_maxima(&m,pr_forms,PR_FORM_COUNT));
    for (unsigned i=0;i<PR_FORM_COUNT;++i) {
        assert(football_stats_generate(&p,i,&pr_forms[i],&m));
        const uint8_t attrs[]={p.attributes.spd,p.attributes.agi,p.attributes.str,
            p.attributes.tak,p.attributes.sta,p.attributes.thp,p.attributes.tha,p.attributes.cth};
        for (unsigned j=0;j<8;++j) assert(attrs[j]>=1 && attrs[j]<=99);
        assert((unsigned)p.position<FB_POSITION_COUNT);
    }
    printf("football stats: formulas, guards, affinities and %u forms passed\n",PR_FORM_COUNT);
    return 0;
}
