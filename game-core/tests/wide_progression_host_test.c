/* Representation boundaries and actual queued level metadata; native numeric
 * behavior is independently replayed by wide_progression_diff_test. */
#include "../src/internal.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
static gc_run run;
static pr_reward_ctx ctx;
static int checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"wide progression line%d: %s\n",__LINE__,#x); exit(1); } } while(0)
int main(void)
{
    gc_starter s = {.species=SPECIES_ID_BULBASAUR,.tera_type=-1};
    memset(s.ivs,15,sizeof s.ivs);
    const int levels[] = {199,200,201,254,255,256,32767,32768,65535,65536,1000000};
    for (int i=0;i<11;i++) {
        const char *why;
        gc_run_new(&run,GAME_MODES_ENDLESS,"wide-progression-host",777);
        CHECK(gc_run_set_starters(&run,&s,1,&why));
        gc_mon *m=&run.party[0];
        m->b.level=levels[i];m->exp=gc_level_total_exp(m->b.level,3);
        const double next=gc_level_total_exp(m->b.level+1,3);
        CHECK(gc_exp_to_next_level(m)==next-m->exp);
        run.wave=100000;
        gc_mon_add_exp(&run,m,next-m->exp);
        CHECK(m->b.level==levels[i]+1&&m->exp==next);
        gc_rq_clear(&run);
        gc_offer offer={.key="RARE_CANDY",.arg=-1};
        offer.target=gc_reward_target(offer.key);
        CHECK(gc_port_apply_reward(&run,&offer,0,-1,&why));
        CHECK(m->b.level==levels[i]+2);
        gc_rphase phase;
        CHECK(gc_rq_next(&gc_int(&run)->rq,&phase));
        CHECK(phase.type==RP_LEVEL_UP&&phase.a==0&&phase.b==levels[i]+1&&phase.c==levels[i]+2);
        CHECK(!gc_rq_next(&gc_int(&run)->rq,&phase));
    }
    /* Outside int32 is explicitly refused before any party member changes. */
    memset(&ctx,0,sizeof ctx);ctx.run=&run.run;ctx.n_party=2;
    ctx.party[0].species=ctx.party[1].species=SPECIES_ID_BULBASAUR;
    ctx.party[0].level=1000000;ctx.party[1].level=INT32_MAX;
    run.run.unported=0;
    CHECK(!pr_reward_apply_candy(&ctx,PR_ITEM_RARER_CANDY,-1));
    CHECK(ctx.party[0].level==1000000&&ctx.party[1].level==INT32_MAX&&ctx.n_level_events==0);
    CHECK(run.run.unported&PR_REWARD_UNPORTED_LEVEL);
    run.run.unported=0;
    CHECK(!pr_reward_apply_candy(&ctx,PR_ITEM_RARE_CANDY,1));
    CHECK(ctx.party[1].level==INT32_MAX&&ctx.n_level_events==0);
    CHECK(run.run.unported&PR_REWARD_UNPORTED_LEVEL);
    run.party[0].b.level=INT32_MAX;
    CHECK(gc_exp_to_next_level(&run.party[0])==0);
    CHECK(gc_exp_level_fraction(&run.party[0])==0);
    ctx.party[1].level=INT32_MAX-1;run.run.unported=0;
    CHECK(pr_reward_apply_candy(&ctx,PR_ITEM_RARE_CANDY,1));
    CHECK(ctx.party[1].level==INT32_MAX&&ctx.level_events[0].old_level==INT32_MAX-1&&ctx.level_events[0].new_level==INT32_MAX);
    CHECK(!run.run.unported);
    printf("Wide progression host: %d checks passed\n",checks);
    return 0;
}
