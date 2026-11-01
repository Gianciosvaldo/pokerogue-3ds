/* Real native constructor observations, through the production party caller. */
#include "../src/internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    int species, level, slot, ability, tera;
    pr_enemy_gen initial, finished;
    int stats[6], hp, refreshed_stats[6], refreshed_hp;
    int count, moves[4], up[4];
    pr_rng ctor_before, moves_before, moves_after, ctor_after;
} trainer_member_case;
typedef struct {
    int index, type, wave, level;
    bool doubles, full;
    int cycle;
    pr_party_template tpl;
    pr_rng ambient;
    int start, n;
} trainer_party_case;
#include "trainer-constructor.native.inc"
static gc_run run;
static const trainer_party_case *group;
static const trainer_member_case *member;
static unsigned checks, constructors, generated, finished, parties, doubles, high_guards;
static int base_count, move_count, finish_count;
static bool observing, inject_stats_failure;
static int injected_finish_calls;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"Trainer constructor group%d type%d wave%d level%d member%d line%d: %s\n",group?group->index:-1,group?group->type:-1,group?group->wave:-1,group?group->level:-1,base_count-1,__LINE__,#x);exit(1); } } while (0)
static bool state(const pr_rng *a,const pr_rng *b)
{ return a->c==b->c && a->s0==b->s0 && a->s1==b->s1 && a->s2==b->s2; }
static void metadata(const pr_enemy_gen *g,const pr_enemy_gen *n,bool final)
{
    CHECK(g->id==n->id && g->form==n->form && g->gender==n->gender && g->nature==n->nature);
    CHECK(g->ability_index==n->ability_index && g->tera_type==n->tera_type && g->fusion_species==n->fusion_species);
    CHECK(!memcmp(g->ivs,n->ivs,sizeof g->ivs));
    if(final) CHECK(g->friendship==n->friendship && g->shiny==n->shiny && g->variant==n->variant && g->luck==n->luck);
}
static void moves(const gc_mon *m,const trainer_member_case *n)
{
    CHECK(m->b.n_moves==n->count);
    for (int k=0;k<n->count;k++) {
        CHECK(m->b.moves[k]==n->moves[k] && m->b.pp_up[k]==n->up[k] && !m->b.pp_used[k]);
        CHECK(m->b.max_pp_override[k]==-1);
    }
}
void __real_pr_calculate_stats(pr_battle*,pr_pokemon*);
void __wrap_pr_calculate_stats(pr_battle *bt,pr_pokemon *p)
{
    __real_pr_calculate_stats(bt,p);
    if(inject_stats_failure)bt->n_unported++;
}
int __real_pr_run_enemy_pokemon_base(pr_run*,const pr_enemy_gen_ctx*,pr_enemy_gen*);
int __wrap_pr_run_enemy_pokemon_base(pr_run *r,const pr_enemy_gen_ctx *e,pr_enemy_gen *g)
{
    if (!observing) return __real_pr_run_enemy_pokemon_base(r,e,g);
    CHECK(base_count<group->n);member=&trainer_members[group->start+base_count++];
    CHECK(e->species==member->species && e->level==member->level && e->trainer_slot==member->slot);
    CHECK(state(&r->rng,&member->ctor_before));
    int result=__real_pr_run_enemy_pokemon_base(r,e,g);
    CHECK(!result);CHECK(state(&r->rng,&member->moves_before));metadata(g,&member->initial,false);
    constructors++;return result;
}
bool __real_gc_generate_moveset(gc_run*,pr_rng*,gc_mon*);
bool __wrap_gc_generate_moveset(gc_run *r,pr_rng *rng,gc_mon *m)
{
    if (!observing) return __real_gc_generate_moveset(r,rng,m);
    CHECK(member && group->level<25 && state(rng,&member->moves_before));
    CHECK(m->b.friendship==member->initial.friendship);
    CHECK(m->b.id==member->initial.id && m->b.ability==member->ability && m->b.trainer_slot==member->slot);
    for(int k=0;k<6;k++) CHECK(m->b.ivs[k]==member->initial.ivs[k]);
    static pr_battle bt;memset(&bt,0,sizeof bt);bt.game_mode=GAME_MODES_CLASSIC;bt.wave_index=group->wave;
    pr_pokemon p=m->b;pr_update_passive(&bt,&p);pr_calculate_stats(&bt,&p);
    for (int k=0;k<6;k++) CHECK(p.stats[k]==member->stats[k]);
    CHECK(p.hp==member->hp);
    CHECK(__real_gc_generate_moveset(r,rng,m));moves(m,member);
    CHECK(state(rng,&member->moves_after));move_count++;generated++;return true;
}
int __real_pr_run_enemy_pokemon_finish(pr_run*,const pr_enemy_gen_ctx*,pr_enemy_gen*);
int __wrap_pr_run_enemy_pokemon_finish(pr_run *r,const pr_enemy_gen_ctx *e,pr_enemy_gen *g)
{
    if(injected_finish_calls && ++injected_finish_calls==3)return -1;
    if (!observing) return __real_pr_run_enemy_pokemon_finish(r,e,g);
    CHECK(member && state(&r->rng,&member->moves_after));
    for (int k=0;k<member->count;k++) CHECK(g->moveset[k]==member->moves[k]);
    int result=__real_pr_run_enemy_pokemon_finish(r,e,g);
    CHECK(!result);CHECK(state(&r->rng,&member->ctor_after));metadata(g,&member->finished,true);
    finish_count++;finished++;return result;
}
static void setup(const trainer_party_case *g)
{
    observing=false;gc_run_new(&run,GAME_MODES_CLASSIC,"trainer-native",777);
    run.wave=g->wave;run.wave_cycle_offset=g->cycle;run.biome=BIOME_ID_TOWN;
    run.trainer_type=g->type;run.trainer_variant=g->doubles?TRAINER_VARIANT_DOUBLE:TRAINER_VARIANT_DEFAULT;
    run.double_battle=g->doubles;run.battle_type=BATTLE_TYPE_TRAINER;
    run.trainer_id=12345;run.secret_id=54321;run.run.unported=0;
    char seed[64];snprintf(seed,sizeof seed,g->full?"trainer-party-%d":"trainer-full-%d",g->index);
    pr_seed_from_ascii(&run.run.seed,seed);
    snprintf(seed,sizeof seed,g->full?"trainer-party-wave-%d":"trainer-full-wave-%d",g->index);
    pr_seed_from_ascii(&run.run.wave_seed,seed);run.run.rng=g->ambient;
    base_count=move_count=finish_count=0;member=NULL;
}
static pr_trainer_party_ctx context(void)
{
    pr_trainer_party_ctx c={0};gc_enemy_ctx(&run,&c.enemy);c.is_double=group->doubles;
    c.enable_gym_e4=true;c.template_=&group->tpl;c.n_levels=group->n;
    for(int k=0;k<c.n_levels;k++)c.levels[k]=group->level;
    return c;
}
static void native_cases(void)
{
    for (unsigned i=0;i<sizeof trainer_parties/sizeof *trainer_parties;i++) {
        group=&trainer_parties[i];setup(group);
        int levels[6];for(int k=0;k<group->n;k++)levels[k]=group->level;
        if (group->level>=25) {
            /* Higher pools remain explicitly unowned: retain actual prefix proof
             * and require atomic rejection of the weighted factory. */
            pr_trainer_party_ctx c=context();pr_trainer_party p;observing=true;
            CHECK(!pr_run_gen_trainer_party(&run.run,&c,&p));observing=false;
            CHECK(p.n==1 && base_count==1 && !move_count && !finish_count);
            CHECK(state(&run.run.rng,&group->ambient));
            gc_mon m;gc_mon_new_enemy(&run,&m,member->species,member->level,&member->initial,member->slot);
            gc_mon before=m;pr_rng rng=member->moves_before;
            CHECK(!gc_generate_moveset(&run,&rng,&m));CHECK(!memcmp(&m,&before,sizeof m));
            CHECK(state(&rng,&member->moves_before));high_guards++;continue;
        }
        uint32_t stub_before[16];memcpy(stub_before,run.stubs,sizeof stub_before);observing=true;
        CHECK(gc_gen_trainer_party(&run,&group->tpl,group->type,run.trainer_variant,levels,group->n));
        observing=false;
        CHECK(run.n_enemy==group->n && base_count==group->n && move_count==group->n && finish_count==group->n);
        CHECK(state(&run.run.rng,&group->ambient));CHECK(!run.run.unported && !memcmp(run.stubs,stub_before,sizeof stub_before));
        for(int k=0;k<group->n;k++) {
            const trainer_member_case *n=&trainer_members[group->start+k];gc_mon *m=&run.enemy[k];
            moves(m,n);CHECK(m->b.id==n->finished.id && m->trainer_slot==n->slot && m->b.trainer_slot==n->slot);
            for(int j=0;j<6;j++) CHECK(m->b.ivs[j]==n->finished.ivs[j]);
            CHECK(m->b.friendship==n->finished.friendship && m->b.shiny==n->finished.shiny && m->variant==n->finished.variant && m->luck==n->finished.luck);
            if(group->full) {
                /* Isolated no-item updateModifiers stat refresh. Full encounter
                 * item rolls and the subsequent battle remain separate proofs. */
                static pr_battle bt;memset(&bt,0,sizeof bt);bt.game_mode=GAME_MODES_CLASSIC;bt.wave_index=group->wave;
                pr_update_passive(&bt,&m->b);pr_calculate_stats(&bt,&m->b);
                for(int j=0;j<6;j++) CHECK(m->b.stats[j]==n->refreshed_stats[j]);
                CHECK(m->b.hp==n->refreshed_hp);
            }
        }
        if(group->full){parties++;doubles+=group->doubles;}
    }
    CHECK(parties==504 && constructors==1880 && generated==1752 && finished==1752 && high_guards==128);
}
/* The run-core continuation contract must keep old prefix callers intact even
 * if a declining callback has already touched its temporary RNG/output. */
static int decline(pr_run *r,const pr_enemy_gen_ctx *e,int index,pr_enemy_gen *g,void *user)
{ (void)e;(void)index;(void)user;pr_rng_seed_int(&r->rng,100,0);g->id^=123;g->ivs[0]=99;return 0; }
static int fail(pr_run *r,const pr_enemy_gen_ctx *e,int index,pr_enemy_gen *g,void *user)
{ decline(r,e,index,g,user);return -1; }
static void continuation_contract(void)
{
    group=&trainer_parties[0];setup(group);pr_trainer_party_ctx c=context();pr_trainer_party plain,actual;
    CHECK(!pr_run_gen_trainer_party(&run.run,&c,&plain));
    CHECK(!pr_run_gen_trainer_party_with_constructor(&run.run,&c,&actual,decline,NULL));
    CHECK(!memcmp(&plain,&actual,sizeof plain));CHECK(state(&run.run.rng,&group->ambient));
    CHECK(pr_run_gen_trainer_party_with_constructor(&run.run,&c,&actual,fail,NULL)<0);
    CHECK(state(&run.run.rng,&group->ambient));
}
static void unsupported(void)
{
    group=&trainer_parties[0];
    for(int k=0;k<20;k++) {
        setup(group);const trainer_member_case *n=&trainer_members[group->start];
        gc_mon m;gc_mon_new_enemy(&run,&m,n->species,n->level,&n->initial,n->slot);
        switch(k) {
        case 0:m.b.has_trainer=false;break;
        case 1:m.trainer_slot=0;break;
        case 2:m.b.trainer_slot=0;break;
        case 3:m.trainer_slot=TRAINER_SLOT_TRAINER_PARTNER;break;
        case 4:run.trainer_type=TRAINER_TYPE_RIVAL;break;
        case 5:m.b.level=25;break;
        case 6:m.b.fusion_species=SPECIES_ID_DITTO;break;
        case 7:m.b.is_boss=true;break;
        case 8:m.b.passive_flag=true;break;
        case 9:m.b.terastallized=true;break;
        case 10:m.b.n_tags=1;break;
        case 11:m.b.on_field=true;break;
        case 12:run.run.has_challenges=true;break;
        case 13:run.run.mode=GAME_MODES_DAILY;break;
        case 14:m.b.species=SPECIES_ID_KELDEO;break;
        case 15:run.battle_type=BATTLE_TYPE_WILD;break;
        case 16:gc_mon_set_move(&m,0,MOVE_ID_TACKLE);break;
        case 17:m.b.level=0;break;
        case 18:m.trainer_slot=m.b.trainer_slot=3;break;
        case 19:inject_stats_failure=true;break;
        }
        gc_mon before=m;pr_rng rng=n->moves_before;
        CHECK(!gc_generate_moveset(&run,&rng,&m));
        CHECK(!memcmp(&m,&before,sizeof m));CHECK(state(&rng,&n->moves_before));
        inject_stats_failure=false;
    }
    /* A second member's constructor error must not expose its staged predecessor. */
    setup(group);int levels[2]={5,5};static gc_mon before[6];
    memcpy(before,run.enemy,sizeof before);int n_enemy=run.n_enemy;
    injected_finish_calls=1;
    CHECK(!gc_gen_trainer_party(&run,&group->tpl,group->type,TRAINER_VARIANT_DEFAULT,levels,2));
    CHECK(injected_finish_calls==3);injected_finish_calls=0;
    CHECK(run.n_enemy==n_enemy && !memcmp(before,run.enemy,sizeof before));
    CHECK(state(&run.run.rng,&group->ambient));
}
int main(void)
{
    native_cases();continuation_contract();unsupported();
    printf("Trainer constructors: %u checks, %u actual base prefixes, %u complete moves/PP/finish streams, %u whole parties (%u doubles), %u higher-level guards\n",checks,constructors,generated,parties,doubles,high_guards);
    return 0;
}
