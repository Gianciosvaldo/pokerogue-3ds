/* Actual run-core newBattle, delegating chance to battle-core and checking the
 * native getter boundary before the scene's later post-battle cleanup. */
#define DOUBLE_CHANCE_COMMON_ONLY
#include "double-chance-test.c"
#undef DOUBLE_CHANCE_COMMON_ONLY
#include "pr/run.h"
static pr_run run;
static int calls,expected_calls,case_id;
static bool callback_ok;
static double chance_callback(void *ctx,int wave){
 int lures=*(int *)ctx;calls++;if(calls>expected_calls){fprintf(stderr,"unexpected chance callback\n");exit(1);}
 bt.rng=run.rng;bt.wave_seed=run.wave_seed;
 double chance=pr_get_double_battle_chance(&bt,wave,lures),v[1];read_vector("V ",v,1);callback_ok&=chance==v[0];
 for(int k=0;k<8;k++){double row[WIDTH+1];read_vector("O ",row,WIDTH+1);if(row[0]!=k)bad();callback_ok&=compare(&mon[k],row+1,case_id,k,'O');}
 callback_ok&=queues();pr_rng out;rng_read("RO ",&out);callback_ok&=rng_equal(out,bt.rng);rng_read("BO ",&out);callback_ok&=rng_equal(out,bt.battle_rng);
 run.rng=bt.rng;return chance;
}
int main(int argc,char **argv){
 if(argc!=2||!getenv("TURNTEST_LOCALES")||!(input=fopen(argv[1],"r")))return 2;
 pr_closures_init();double h[3];read_vector("DOUBLE_GENERATION ",h,3);int version=integer(h[0],1,4),required=(version==1||version==4)?32768:16384;int n=integer(h[1],required,required),failed=0;integer(h[2],0,INT32_MAX);
 int invoked=0,gates=0,doubles=0,wide=0;
 for(int i=0;i<n;i++){
  memset(&run,0,sizeof run);memset(&bt,0,sizeof bt);memset(&e,0,sizeof e);bt.engine=&e;e.n_levels=1;e.current=-1;bt.wave_index=1;bt.turn=1;bt.ignoring_effect_source=-1;bt.last_used_pokeball=-1;
  int id,u,single,wave,lures,mode,biome=0,trainer_chance=0,player_female=0;char seed[64],extra;
  const char *context=line_read("C ");
  int got=version==1?sscanf(context,"%d %d %d %d %d %d %63s %c",&id,&u,&single,&wave,&lures,&mode,seed,&extra):sscanf(context,"%d %d %d %d %d %d %d %d %63s %c",&id,&u,&single,&wave,&lures,&mode,&biome,&trainer_chance,seed,&extra);
  if(version==4)got=sscanf(context,"%d %d %d %d %d %d %d %d %d %63s %c",&id,&u,&single,&wave,&lures,&mode,&biome,&trainer_chance,&player_female,seed,&extra);
  if(player_female<0||player_female>1||got!=(version==1?7:version==4?10:9)||id!=i||u<0||u>7||single<0||single>1||wave<2||wave>1000000||lures<0||lures>3||biome<0||biome>=PR_BIOME_ID_LIMIT||trainer_chance<0||trainer_chance>100||(version==1?(mode!=GAME_MODES_ENDLESS&&mode!=GAME_MODES_SPLICED_ENDLESS):(mode!=GAME_MODES_CLASSIC&&mode!=GAME_MODES_CHALLENGE)))bad();
  for(size_t k=0;k<strlen(seed);k++)if(!isalnum((unsigned char)seed[k]))bad();
  pr_seed_from_ascii(&run.seed,seed);run.mode=mode;bt.game_mode=mode;bt.double_battle=!single;bt.wave_index=wave-1;
  char old_seed[512];if(sscanf(line_read("W "),"%511s %c",old_seed,&extra)!=1)bad();pr_seed_from_ascii(&bt.wave_seed,old_seed);rng_read("R ",&bt.rng);rng_read("B ",&bt.battle_rng);bt.rng_live=bt.battle_rng_live=true;
  bool ok=true;for(int k=0;k<8;k++){double v[WIDTH+1];read_vector("I ",v,WIDTH+1);if(v[0]!=k)bad();load(&mon[k],v+1,k<4?k:k<6?0:2);ok&=compare(&mon[k],v+1,i,k,'I');}
  int slots[2][4]={{0,1,4,5},{2,3,6,7}};for(int side=0;side<2;side++){bt.n_party[side]=4;for(int j=0;j<4;j++)bt.party[side][j]=&mon[slots[side][j]];}for(int k=0;k<4;k++)bt.field[k]=&mon[k];
  pr_queue_msg_deferred(&bt,&e,pr_msg0("anchor"));int current=pr_phase_new(&e,PH_MOVE,0);e.current=current;e.pool[current].move.id=MOVE_ID_SPLASH;
  double a[1];read_vector("A ",a,1);expected_calls=integer(a[0],0,1);calls=0;case_id=i;callback_ok=true;
  pr_new_battle_ctx ctx={.enable_fixed_rivals=true,.player_female=player_female,.biome=biome,.trainer_chance=trainer_chance,.prev_wave=wave-1,.n_lures=lures,.double_chance=chance_callback,.double_chance_ctx=&lures};pr_new_battle mine;
  if(version==3)ctx.me=(pr_me_save){.spawn_chance=3,.n_encountered=1,.last_encounter_wave=wave-1};
  int rc=pr_run_new_battle(&run,wave,&ctx,&mine);if(calls!=expected_calls){fprintf(stderr,"missing chance callback\n");return 1;}ok&=rc==0&&!run.unported&&calls==expected_calls&&callback_ok;
  if(version>=2){double t[5];read_vector("T ",t,5);for(int j=0;j<5;j++)integer(t[j],-1,1000);bool tok=mine.trainer_type==t[0]&&mine.variant==t[1]&&mine.template_index==t[2]&&mine.name==t[3]&&mine.partner_name==t[4];if(!tok&&failed<2)fprintf(stderr,"trainer %d/%g variant%d/%g template%d/%g name%d/%g partner%d/%g\n",mine.trainer_type,t[0],mine.variant,t[1],mine.template_index,t[2],mine.name,t[3],mine.partner_name,t[4]);ok&=tok;}
  const char *row=line_read("G ");char *end;double g[4];for(int j=0;j<4;j++){g[j]=strtod(row,&end);if(end==row||!isfinite(g[j]))bad();row=end;}
  int nw=integer(g[0],2,1000000),type=integer(g[1],0,3),dbl=integer(g[2],0,1),nl=integer(g[3],1,PR_MAX_PARTY_TEMPLATE);ok&=nw==wave&&mine.battle_type==type&&mine.double_battle==dbl&&mine.n_levels==nl;
  for(int j=0;j<nl;j++){double lev=strtod(row,&end);if(end==row||!isfinite(lev))bad();row=end;integer(lev,1,INT32_MAX);if(mine.levels[j]!=lev&&failed<2)fprintf(stderr,"level%d %d/%g\n",j,mine.levels[j],lev);ok&=mine.levels[j]==lev;wide+=lev>255;}
  if(sscanf(row,"%63s %c",seed,&extra)!=1||strlen(seed)!=16)bad();ok&=!strcmp(seed,mine.battle_seed);
  pr_rng out;rng_read("S ",&out);ok&=rng_equal(out,run.rng);if(strcmp(line_read("E"),"\n"))bad();if(bt.n_unported||bt.n_deferred||e.overflow||e.n_events)ok=false;
  invoked+=calls;gates+=!calls;doubles+=mine.double_battle;
  if(!ok){if(failed<12)fprintf(stderr,"Generation case%d calls%d/%d rc%d flags%u callback%d failed\n",i,calls,expected_calls,rc,run.unported,callback_ok);failed++;}
 }
 double end[1];read_vector("DOUBLE_GENERATION_COMPLETE ",end,1);if(end[0]!=n||fgetc(input)!=EOF)bad();printf("Double generation: %d compared, %d exact, %d failures; callbacks%d gates%d doubles%d wideLevels%d\n",n,n-failed,failed,invoked,gates,doubles,wide);return failed?1:0;
}
