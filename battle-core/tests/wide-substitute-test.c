/* Compile the actual private applyMoveDamage implementation into this oracle. */
#define main gas_replay_main
#include "wide-substitute-frame.inc"
#undef main
#include "../src/phases.c"
#include "../../ai-core/src/ai_internal.h"
static void text_hex(const pr_msg *m,char *out){char text[1024];pr_text_source src={locale_get,NULL,"en"};size_t n=pr_msg_format(m,&src,text,sizeof text);if(n>=sizeof text)bad();if(!n){strcpy(out,"-");return;}for(size_t i=0;i<n;i++)sprintf(out+2*i,"%02x",(unsigned char)text[i]);}
int main(int argc,char **argv){
 if(argc!=2||!getenv("TURNTEST_LOCALES")||!(input=fopen(argv[1],"r")))return 2;pr_closures_init();double h[3];read_vector("WIDE_SUBSTITUTE ",h,3);int version=integer(h[0],2,3);int n=integer(h[1],8192,100000),failures=0,wide=0,broken=0,criticals=0,ai_wide=0;int move_seen[6]={0};const int required[]={MOVE_ID_TACKLE,MOVE_ID_DRAGON_PULSE,MOVE_ID_SEISMIC_TOSS,MOVE_ID_DRAGON_RAGE,MOVE_ID_ERUPTION,MOVE_ID_STORED_POWER};integer(h[2],0,INT32_MAX);
 for(int i=0;i<n;i++){
  static pr_battle bt;static pr_engine e;static pr_pokemon actors[4];memset(&bt,0,sizeof bt);memset(&e,0,sizeof e);bt.engine=&e;bt.double_battle=true;bt.game_mode=GAME_MODES_ENDLESS;bt.turn=1;bt.last_used_pokeball=-1;bt.ignoring_effect_source=-1;e.n_levels=1;e.current=-1;
  double c[7];read_vector("C ",c,7);if(c[0]!=i)bad();int u=integer(c[1],0,3),t=integer(c[2],0,3),move=integer(c[3],1,PR_MOVE_COUNT-1);double eff=c[4];if(u==t||(eff!=.5&&eff!=1&&eff!=2))bad();bt.wave_index=integer(c[5],1,INT32_MAX);bt.max_exp_level=integer(c[6],1,INT32_MAX);
  char seed[512],extra_seed;char *seed_row=line_read("W ");if(sscanf(seed_row,"%511s %c",seed,&extra_seed)!=1)bad();pr_seed_from_ascii(&bt.wave_seed,seed);
  rng_read("R ",&bt.rng);rng_read("B ",&bt.battle_rng);bt.rng_live=bt.battle_rng_live=true;bool ok=true;
  for(int k=0;k<4;k++){double v[WIDTH+1];read_vector("I ",v,WIDTH+1);if(v[0]!=k)bad();load(&actors[k],v+1,k);pr_reset_turn_data(&actors[k]);bt.field[k]=bt.party[k/2][k%2]=&actors[k];bt.n_party[k/2]=2;ok&=compare(&actors[k],v+1,i,k,'I');}
  double hp[1];read_vector("H ",hp,1);integer(hp[0],1,INT32_MAX);pr_battler_tag *sub=(pr_battler_tag *)pr_get_battler_tag(&actors[t],BATTLER_TAG_TYPE_SUBSTITUTE);if(!sub)bad();sub->hp=hp[0];sub->source_move=MOVE_ID_SUBSTITUTE;sub->source_id=actors[t].id;
  int ph=pr_phase_new(&e,PH_MOVE_EFFECT,u);e.current=ph;e.pool[ph].move_id=move;e.pool[ph].move.id=move;
  if(version>=3){double sim[6];read_vector("SIM ",sim,6);for(int j=0;j<4;j++)integer(sim[j],0,1);integer(sim[4],0,100);if(sim[5]<0)bad();pr_rng before_r=bt.rng,before_b=bt.battle_rng;pr_damage_result estimate=ai_attack_damage(&bt,&actors[t],&actors[u],move,sim[0],sim[1],sim[2]);ok&=estimate.cancelled==sim[3]&&estimate.result==sim[4]&&estimate.damage==sim[5]&&rng_equal(before_r,bt.rng)&&rng_equal(before_b,bt.battle_rng);ai_wide+=sim[5]>INT32_MAX;}
  move_damage result=apply_move_damage(&bt,&e,ph,&actors[u],&actors[t],eff);
  double out[3];read_vector("RESULT ",out,3);integer(out[0],0,100);integer(out[2],0,1);ok&=out[0]==result.result&&out[1]==result.damage&&out[2]==result.critical;
  double ho[4];read_vector("HOUT ",ho,4);ok&=ho[0]==sub->hp&&ho[1]==actors[u].total_damage_dealt&&ho[2]==actors[u].single_hit_damage_dealt&&ho[3]==actors[t].damage_taken;wide+=ho[0]<INT32_MIN;broken+=ho[0]<=0;criticals+=out[2]!=0;for(int m=0;m<6;m++)if(move==required[m])move_seen[m]++;
  for(int k=0;k<4;k++){double v[WIDTH+1];read_vector("O ",v,WIDTH+1);if(v[0]!=k)bad();ok&=compare(&actors[k],v+1,i,k,'O');}
  double q[3];read_vector("QCOUNT ",q,3);int count=integer(q[0],0,32),seen=0;if(integer(q[1],1,PR_MAX_LEVELS)!=e.n_levels||integer(q[2],0,1)!=e.deferred_active)ok=false;
  for(int l=0;l<e.n_levels;l++)for(int k=0;k<e.level_len[l];k++){
   int ix=e.levels[l][k];if(e.pool[ix].type!=PH_MESSAGE)bad();if(seen++>=count){ok=false;continue;}char *row=line_read("Q "),text[2048],expected[2048],extra;int level,order;if(sscanf(row,"%d %d %2047s %c",&level,&order,expected,&extra)!=3||level<0||level>=PR_MAX_LEVELS||order<0||order>=PR_MAX_PHASES)bad();text_hex(&e.pool[ix].msg,text);if(level!=l||order!=k||strcmp(expected,text))ok=false;
  }
  while(seen<count){line_read("Q ");seen++;ok=false;}
  pr_rng want;rng_read("RO ",&want);ok&=rng_equal(want,bt.rng);rng_read("BO ",&want);ok&=rng_equal(want,bt.battle_rng);if(strcmp(line_read("E"),"\n"))bad();if(bt.n_unported||bt.n_deferred||e.overflow)ok=false;
  if(!ok){if(failures<20)fprintf(stderr,"case%d Substitute HP C%.17g JS%.17g damage C%.17g JS%.17g queue%d/%d unported%d %s\n",i,(double)sub->hp,ho[0],result.damage,out[1],seen,count,bt.n_unported,bt.n_unported?pr_unported_name(bt.unported[0]):"none");failures++;}
 }
 double end[1];read_vector("WIDE_SUBSTITUTE_COMPLETE ",end,1);if(end[0]!=n||fgetc(input)!=EOF)bad();if(!failures){if(wide<100||broken<100||criticals<100||(version>=3&&ai_wide<100))bad();for(int m=0;m<6;m++)if(move_seen[m]<100)bad();}printf("Wide Substitute leaf: %d complete, %d exact, %d failures, wide=%d broken=%d aiWide=%d\n",n,n-failures,failures,wide,broken,ai_wide);return failures?1:0;
}
