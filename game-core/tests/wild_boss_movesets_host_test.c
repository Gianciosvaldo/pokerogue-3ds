/* Full native generator at the actual ordinary-wild owner, not a factory oracle. */
#include "../src/internal.h"
#include "fixtures/wild-movesets.native.inc"
#include "fixtures/wild-boss-movesets.native.inc"
#include <stdio.h>
#include <stdlib.h>
static gc_run run;
static unsigned checks,cases;
static bool inject_unported;
#define CHECK(x) do {checks++;if(!(x)){fprintf(stderr,"wild movesets case %u line %d: %s\n",cases,__LINE__,#x);exit(1);}}while(0)
void __real_pr_calculate_stats(pr_battle*,pr_pokemon*);
void __wrap_pr_calculate_stats(pr_battle *bt,pr_pokemon *p)
{
    __real_pr_calculate_stats(bt,p);
    if(inject_unported)bt->n_unported++;
}
static bool same_rng(const pr_rng*a,const pr_rng*b)
{return a->c==b->c&&a->s0==b->s0&&a->s1==b->s1&&a->s2==b->s2;}
static void moves(const gc_mon*m,const native_move_case*n)
{
    CHECK(m->b.n_moves==n->n);
    for(int i=0;i<n->n;i++) {
        CHECK(m->b.moves[i]==n->moves[i][0]);CHECK(m->b.pp_used[i]==n->moves[i][1]);
        CHECK(m->b.pp_up[i]==n->moves[i][2]);CHECK(m->b.max_pp_override[i]==n->moves[i][3]);
    }
}
static void one(const native_boss_move_case*b)
{
    const native_move_case*n=&b->mon;
    gc_run_new(&run,n->mode,"native-wild-movesets",1);
    run.wave=1;run.battle_type=BATTLE_TYPE_WILD;run.bt.weather=b->weather;run.bt.terrain=b->terrain;run.double_battle=b->dbl;
    run.run.rng=n->before;
    gc_mon*m=&run.enemy[0];gc_mon_new_enemy(&run,m,n->species,n->level,&n->g,0);
    CHECK(same_rng(&run.run.rng,&n->before));CHECK(m->b.ability==n->ability);
    /* gc_mon_new_enemy defers stats; the generator computes its own clean
     * constructor view before items/summon effects. Check that view here. */
    pr_battle constructor={0};constructor.game_mode=run.run.mode;constructor.wave_index=run.wave;constructor.weather=b->weather;constructor.terrain=b->terrain;
    pr_pokemon view=m->b;pr_update_passive(&constructor,&view);pr_calculate_stats(&constructor,&view);
    CHECK(constructor.n_unported==0);CHECK(pr_has_passive(&constructor,&view));
    if(b->passive>=0)CHECK(view.passive==b->passive);
    for(int i=0;i<6;i++)CHECK(view.stats[i]==n->stats[i]);
    gc_learn_row pool[512];int np=gc_level_moves(m,1,true,true,false,pool,512);
    CHECK(np==n->np);for(int i=0;i<np;i++) {
        CHECK(pool[i].level==n->pool[i][0]);CHECK(pool[i].move==n->pool[i][1]);CHECK(pool[i].source==n->pool[i][2]);
    }
    run.bt.n_mods[1]=1;
    run.bt.mods[1][0]=(pr_modifier){.cls=PR_MOD_BASE_STAT,.pokemon_id=m->b.id,.stat=STAT_ATK,.stack_count=10};
    gc_mon original=*m;
    CHECK(gc_enemy_moveset(&run,m,&n->g));moves(m,n);CHECK(same_rng(&run.run.rng,&n->after));
    CHECK(run.stubs[GC_STUB_ENEMY_MOVESET]==0);
    gc_refresh_enemy(&run,m,n->species,n->level,&n->g,0);
    moves(m,n);CHECK(same_rng(&run.run.rng,&n->after));
    *m=original;run.run.rng=n->before;
    CHECK(gc_generate_moveset(&run,&run.run.rng,m));moves(m,n);CHECK(same_rng(&run.run.rng,&n->after));
    cases++;
}
static void atomic_rejection(void)
{
    const native_move_case*n=&native_boss_constructor_moves[0].mon;
    gc_run_new(&run,n->mode,"atomic-boss-movegen",1);run.wave=1;run.battle_type=BATTLE_TYPE_WILD;
    gc_mon m;gc_mon_new_enemy(&run,&m,n->species,n->level,&n->g,0);
    gc_mon before=m;pr_rng rng=n->before;run.run.rng=rng;
    inject_unported=true;
    CHECK(!gc_enemy_moveset(&run,&m,&n->g));CHECK(!memcmp(&m,&before,sizeof m));CHECK(same_rng(&run.run.rng,&rng));
    inject_unported=false;
    CHECK(gc_enemy_moveset(&run,&m,&n->g));moves(&m,n);CHECK(same_rng(&run.run.rng,&n->after));
}
int main(void)
{
    for(unsigned i=0;i<sizeof native_boss_constructor_moves/sizeof *native_boss_constructor_moves;i++)one(&native_boss_constructor_moves[i]);
    for(unsigned i=0;i<sizeof native_boss_retained_moves/sizeof *native_boss_retained_moves;i++)one(&native_boss_retained_moves[i]);
    atomic_rejection();CHECK(cases==1766);
    printf("native unfused boss movesets: %u cases, %u checks\n",cases,checks);return 0;
}
