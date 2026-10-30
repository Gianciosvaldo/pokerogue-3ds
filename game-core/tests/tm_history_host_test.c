/* Native TM callbacks, complete memory choices and compatible command saves. */
#include "../src/internal.h"
#include "gc/reward.h"
#include "fixtures/tm-history.native.inc"
#include "fixtures/tm-history.legacy-64.inc"
#include <stdio.h>
#include <stdlib.h>
static gc_run run,loaded;
static unsigned char save[GC_SAVE_MAX];
static size_t saved_n;
static unsigned step,checks;
#define CHECK(x)do{checks++;if(!(x)){fprintf(stderr,"TM history step%u line%d: %s\n",step,__LINE__,#x);exit(1);}}while(0)
static bool same(pr_rng a,pr_rng b){return a.c==b.c&&a.s0==b.s0&&a.s1==b.s1&&a.s2==b.s2;}
static void compare(gc_run*r,const native_tm_frame*f)
{
 const gc_mon*m=&r->party[0];CHECK(m->n_used_tms==f->n);
 for(int i=0;i<f->n;i++)CHECK(m->used_tms[i]==f->used[i]);
 CHECK(m->b.n_moves==4);
 for(int i=0;i<4;i++){CHECK(m->b.moves[i]==f->moves[i][0]);CHECK(m->b.pp_used[i]==f->moves[i][1]);CHECK(m->b.pp_up[i]==f->moves[i][2]);CHECK(m->b.max_pp_override[i]==f->moves[i][3]);}
 CHECK(same(r->run.rng,f->rng[0])&&same(r->bt.rng,f->rng[0])&&same(r->bt.battle_rng,f->rng[1]));
 pr_reward_ctx*ctx=gc_reward_ctx(r);CHECK(ctx->party[0].n_used_tms==f->n);
 for(int i=0;i<f->n;i++)CHECK(ctx->party[0].used_tms[i]==f->used[i]);
 pr_reward_memory_move rows[PR_REWARD_MAX_LEVEL_MOVES+PR_REWARD_MAX_USED_TMS+4];
 int n=pr_reward_memory_moves(ctx,0,rows,sizeof rows/sizeof *rows);CHECK(n==f->nmemory);
 for(int i=0;i<n;i++){CHECK(rows[i].level==f->memory[i][0]);CHECK(rows[i].move==f->memory[i][1]);CHECK(rows[i].source==f->memory[i][2]);}
 CHECK(gc_memory_moves(r,0,rows,sizeof rows/sizeof *rows)==f->nmemory);
 for(int i=0;i<n;i++){CHECK(rows[i].level==f->memory[i][0]);CHECK(rows[i].move==f->memory[i][1]);CHECK(rows[i].source==f->memory[i][2]);}
 CHECK(r->stubs[GC_STUB_REWARD_INPUT]==0);
}
static void roundtrip(const native_tm_frame*f)
{
 CHECK(gc_run_can_save(&run));saved_n=gc_run_save(&run,save,sizeof save);CHECK(saved_n>0);
 const char*why=NULL;CHECK(gc_run_load(&loaded,save,saved_n,&why));CHECK(!loaded.load_dropped);compare(&loaded,f);
}
int main(int argc,char**argv)
{
 if(argc==3&&!strcmp(argv[1],"--reject-save")){
  FILE*f=fopen(argv[2],"rb");CHECK(f);size_t n=fread(save,1,sizeof save,f);fclose(f);const char*why=NULL;CHECK(!gc_run_load(&loaded,save,n,&why));return 0;
 }
 int count=0;for(int i=0;i<pr_tm_tier_count;i++)if(pr_tm_tier[i]>=0){CHECK(count<sizeof native_tm_catalog/sizeof *native_tm_catalog);CHECK(native_tm_catalog[count++]==i);}
 CHECK(count==PR_TM_HISTORY_CAPACITY&&count==PR_REWARD_MAX_USED_TMS&&count==322);
 gc_run_new(&run,GAME_MODES_CLASSIC,"tm-history",777);gc_starter starter={.species=SPECIES_ID_MEW,.tera_type=-1};
 for(int i=0;i<6;i++)starter.ivs[i]=15;
 const char*why=NULL;CHECK(gc_run_set_starters(&run,&starter,1,&why));gc_event event;while(gc_run_next_event(&run,&event)){}
 for(int i=0;i<4;i++)gc_mon_set_move(&run.party[0],i,native_tm_initial.moves[i][0]);
 run.run.rng=run.bt.rng=native_tm_initial.rng[0];run.bt.battle_rng=native_tm_initial.rng[1];run.bt.rng_live=run.bt.battle_rng_live=true;
 compare(&run,&native_tm_initial);roundtrip(&native_tm_initial);
 for(step=0;step<sizeof native_tm_cases/sizeof *native_tm_cases;step++){
  gc_rphase ph=gc_rp(RP_LEARN_MOVE,0,native_tm_cases[step].move,1);bool ok=false;
  CHECK(gc_phase_choose(&run,&ph,&(gc_choice){GC_CH_FORGET,0,0,0},&why,&ok)&&ok);
  compare(&run,&native_tm_cases[step].after);roundtrip(&native_tm_cases[step].after);
  while(gc_run_next_event(&run,&event)){}CHECK(!run.ev_overflow);
 }
 CHECK(step==313&&run.party[0].n_used_tms==312);
 CHECK(gc_run_load(&loaded,native_legacy_tm_save,sizeof native_legacy_tm_save,&why));CHECK(!loaded.load_dropped);compare(&loaded,&native_tm_cases[63].after);
 for(int i=64;i<PR_TM_HISTORY_CAPACITY;i++)CHECK(loaded.party[0].used_tms[i]==0);
 if(argc==3&&!strcmp(argv[1],"--dump-save")){FILE*f=fopen(argv[2],"wb");CHECK(f);CHECK(fwrite(save,1,saved_n,f)==saved_n);fclose(f);}
 printf("Native TM history: 313 callbacks, 314 save roundtrips, 322 catalog moves, %u checks\n",checks);return 0;
}
