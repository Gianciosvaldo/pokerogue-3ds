/* Complete native frames plus explicit underlying/custom/effective nature. */
#define DOUBLE_CHANCE_COMMON_ONLY
#include "../../battle-core/tests/double-chance-test.c"
#undef DOUBLE_CHANCE_COMMON_ONLY
#include "../src/internal.h"
static gc_run run;
static pr_pokemon *actors[8];
static gc_mon *storage[8];
static bool nature_rows(const char *key,bool initial){
 bool ok=true;
 for(int k=0;k<8;k++){
  double v[4];read_vector(key,v,4);if(v[0]!=k)bad();int raw=integer(v[1],0,24),custom=integer(v[2],-1,24),effective=integer(v[3],0,24);if(effective!=(custom<0?raw:custom))bad();gc_mon *m=storage[k];
  if(initial){m->has_custom_nature=custom>=0;m->base_nature=raw;}
  ok&=m->b.nature==effective&&(m->has_custom_nature?m->base_nature:m->b.nature)==raw&&(m->has_custom_nature?m->b.nature:-1)==custom;
 }
 return ok;
}
int main(int argc,char **argv){
 if(argc!=2||!getenv("TURNTEST_LOCALES")||!(input=fopen(argv[1],"r")))return 2;
 pr_closures_init();double h[3];read_vector("SYNC_NATURE ",h,3);int version=integer(h[0],1,3),required=version==1?102400:1536;int n=integer(h[1],required,required),failed=0;integer(h[2],0,INT32_MAX);
 for(int i=0;i<n;i++){
  memset(&run,0,sizeof run);memset(&bt,0,sizeof bt);memset(&e,0,sizeof e);bt.engine=&e;e.n_levels=1;e.current=-1;bt.wave_index=1;bt.game_mode=GAME_MODES_CLASSIC;bt.turn=1;bt.double_battle=true;bt.ignoring_effect_source=-1;bt.last_used_pokeball=-1;
  run.n_party=run.n_enemy=4;for(int k=0;k<8;k++){int slot=k<4?k%2:k%2+2;storage[k]=k%4<2?&run.party[slot]:&run.enemy[slot];actors[k]=&storage[k]->b;}
  double c[7]={0};read_vector("C ",c,version==1?5:version==2?6:7);if(c[0]!=i)bad();int u=integer(c[1],version==1?0:-1,version==1?7:-1),t=integer(c[2],0,7),sim=integer(c[3],0,1),scene=integer(c[4],version==1?0:1,1);if(version>=2&&(sim||t<2||t>3))bad();if(version==3){if(c[6]!=GAME_MODES_CHALLENGE)bad();bt.game_mode=GAME_MODES_CHALLENGE;bt.challenge_flags_known=true;}bt.double_battle=!integer(c[5],0,1);
  char seed[512],extra;if(sscanf(line_read("W "),"%511s %c",seed,&extra)!=1)bad();pr_seed_from_ascii(&bt.wave_seed,seed);rng_read("R ",&bt.rng);rng_read("B ",&bt.battle_rng);bt.rng_live=bt.battle_rng_live=true;
  bool ok=true;for(int k=0;k<8;k++){double v[WIDTH+1];read_vector("I ",v,WIDTH+1);if(v[0]!=k)bad();load(actors[k],v+1,k<4?k:k<6?0:2);ok&=compare(actors[k],v+1,i,k,'I');}ok&=nature_rows("NI ",true);if(!scene&&storage[t]->has_custom_nature)bad();
  int slots[2][4]={{0,1,4,5},{2,3,6,7}};for(int side=0;side<2;side++){bt.n_party[side]=4;for(int k=0;k<4;k++)bt.party[side][k]=actors[slots[side][k]];}for(int k=0;k<4;k++)bt.field[k]=actors[k];
  pr_queue_msg_deferred(&bt,&e,pr_msg0("anchor"));int current=pr_phase_new(&e,PH_MOVE,0);e.current=current;e.pool[current].move.id=MOVE_ID_SPLASH;
  pr_ab_params pp={.pokemon=u<0?NULL:actors[u],.target=actors[t],.move=-1,.simulated=sim,.set_nature=scene?gc_set_encounter_nature:NULL,.nature_ctx=&run};if(version==1)pr_apply_ab_attrs(&bt,PR_AC_SyncEncounterNatureAbAttr,&pp);else pr_sync_encounter_nature(&bt,actors[t],gc_set_encounter_nature,&run);
  for(int k=0;k<8;k++){double v[WIDTH+1];read_vector("O ",v,WIDTH+1);if(v[0]!=k)bad();ok&=compare(actors[k],v+1,i,k,'O');}ok&=nature_rows("NO ",false);ok&=queues();
  pr_rng out;rng_read("RO ",&out);ok&=rng_equal(out,bt.rng);rng_read("BO ",&out);ok&=rng_equal(out,bt.battle_rng);if(strcmp(line_read("E"),"\n"))bad();if(bt.n_unported||bt.n_deferred||e.overflow||e.n_events)ok=false;
  if(!ok){if(failed<12)fprintf(stderr,"Sync nature case%d user%d target%d sim%d scene%d flags%d failed\n",i,u,t,sim,scene,bt.n_unported);failed++;}
 }
 double end[1];read_vector("SYNC_NATURE_COMPLETE ",end,1);if(end[0]!=n||fgetc(input)!=EOF)bad();printf("Sync nature: %d compared, %d exact, %d failures\n",n,n-failed,failed);return failed?1:0;
}
