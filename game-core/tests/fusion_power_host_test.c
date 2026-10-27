/* Native Move.calculateEffectivePower observations in moveset-generation mode. */
#include "../src/internal.h"
#include "fixtures/fusion-power.native.inc"
#include <stdio.h>
#include <stdlib.h>
static gc_run run;
static unsigned checks, cases;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"fusion power case %u line %d: %s\n",cases,__LINE__,#x); exit(1); } } while (0)
static bool same_rng(const pr_rng *a,const pr_rng *b)
{ return a->c==b->c && a->s0==b->s0 && a->s1==b->s1 && a->s2==b->s2; }
static void one(const native_fusion_power_case *n)
{
    gc_run_new(&run,n->mode,"native-fusion-power",1);
    run.wave=1;run.battle_type=BATTLE_TYPE_WILD;run.double_battle=n->dbl;
    run.bt.weather=n->weather;run.bt.terrain=n->terrain;
    gc_mon *m=&run.enemy[0];gc_mon_new_enemy(&run,m,n->species,n->level,&n->g,0);
    CHECK(m->b.ability==n->ability);
    pr_battle constructor={0};constructor.game_mode=n->mode;constructor.wave_index=1;
    constructor.weather=n->weather;constructor.terrain=n->terrain;
    pr_pokemon view=m->b;pr_update_passive(&constructor,&view);pr_calculate_stats(&constructor,&view);
    CHECK(constructor.n_unported==0);CHECK(pr_has_passive(&constructor,&view));CHECK(view.passive==n->passive);
    for(int i=0;i<6;i++)CHECK(view.stats[i]==n->stats[i]);
    gc_learn_row pool[512];int np=gc_level_moves(m,1,true,true,false,pool,512);
    CHECK(np==n->np);
    for(int i=0;i<np;i++) {
        CHECK(pool[i].level==n->pool[i][0]);CHECK(pool[i].move==n->pool[i][1]);CHECK(pool[i].source==n->pool[i][2]);
    }
    run.run.rng=n->rng;run.bt.rng=n->rng;
    const gc_mon before=*m;const pr_rng rng=run.run.rng,bt_rng=run.bt.rng;
    double powers[64];CHECK(gc_mg_effective_powers(&run,m,n->moves,n->n,powers));
    for(int i=0;i<n->n;i++) {
        if(powers[i]!=n->powers[i])fprintf(stderr,"move %u actual %.17g native %.17g\n",n->moves[i],powers[i],n->powers[i]);
        CHECK(powers[i]==n->powers[i]);
    }
    CHECK(!memcmp(m,&before,sizeof before));CHECK(same_rng(&run.run.rng,&rng));CHECK(same_rng(&run.bt.rng,&bt_rng));
    /* This proof does not enable the guarded fused-boss generator. */
    CHECK(!gc_generate_moveset(&run,&run.run.rng,m));CHECK(!memcmp(m,&before,sizeof before));CHECK(same_rng(&run.run.rng,&rng));
    cases++;
}
int main(void)
{
    for(unsigned i=0;i<sizeof native_fusion_power_cases/sizeof *native_fusion_power_cases;i++)one(&native_fusion_power_cases[i]);
    CHECK(cases==812);printf("native fusion powers: %u cases, %u checks\n",cases,checks);return 0;
}
