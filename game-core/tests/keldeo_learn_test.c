/* Native LearnMove.start/text callback -> game-core learn/queued form caller.
 * Compare complete actor frames, cached form target, text, profile bits and RNG.
 * Scene evolution presentation queues are covered separately by the raw proof. */
#include "../src/internal.h"
#define main frame_replay_main
#include "wide-substitute-frame.inc"
#undef main
static gc_run run;static gc_system sys;static pr_pokemon *actors[8];
static void text_hex(const pr_msg *m,char *out){char s[1024];pr_text_source src={locale_get,NULL,"en"};size_t n=pr_msg_format(m,&src,s,sizeof s);if(n>=sizeof s)bad();if(!n){strcpy(out,"-");return;}for(size_t j=0;j<n;j++)sprintf(out+2*j,"%02x",(unsigned char)s[j]);}
static void profile_read(const char *key,uint64_t *seen,uint64_t *caught){
 char *p=line_read(key),*end;uint64_t values[2];
 for(int k=0;k<2;k++){while(isspace((unsigned char)*p))p++;if(!isdigit((unsigned char)*p))bad();errno=0;values[k]=strtoull(p,&end,10);if(errno||p==end)bad();p=end;}
 while(isspace((unsigned char)*p))p++;if(*p)bad();*seen=values[0];*caught=values[1];
}
int main(int argc,char **argv){
 if(argc!=2||!getenv("TURNTEST_LOCALES")||!(input=fopen(argv[1],"r")))return 2;pr_closures_init();double h[3];read_vector("KELDEO_LEARN ",h,3);int version=integer(h[0],1,2);int n=integer(h[1],16384,100000),failed=0;integer(h[2],0,INT32_MAX);
 for(int i=0;i<n;i++){
  memset(&run,0,sizeof run);gc_system_init_defaults(&sys);run.api_version=GC_API_VERSION;run.sys=&sys;run.n_party=run.n_enemy=4;run.wave=1;run.in_battle=true;gc_rq_reset(&gc_int(&run)->rq);
  run.eng.n_levels=1;run.eng.current=-1;pr_battle *bt=&run.bt;bt->engine=&run.eng;bt->wave_index=1;bt->turn=1;bt->double_battle=true;bt->ignoring_effect_source=-1;bt->last_used_pokeball=-1;
  double c[6]={0};read_vector("C ",c,version==1?5:6);bool pending=version==2&&integer(c[5],0,1);if(c[0]!=i)bad();int u=integer(c[1],0,7),mode=integer(c[2],0,1),move=integer(c[4],1,PR_MOVE_COUNT-1);if(u!=0&&u!=1&&u!=4&&u!=5)bad();bt->game_mode=run.run.mode=integer(c[3],0,GAME_MODES_CHALLENGE);bt->challenge_flags_known=true;
  char wave[512],extra;if(sscanf(line_read("W "),"%511s %c",wave,&extra)!=1)bad();pr_seed_from_ascii(&bt->wave_seed,wave);rng_read("R ",&bt->rng);rng_read("B ",&bt->battle_rng);bt->rng_live=bt->battle_rng_live=true;run.run.rng=bt->rng;
  int slots[2][4]={{0,1,4,5},{2,3,6,7}};for(int side=0;side<2;side++){bt->n_party[side]=4;for(int k=0;k<4;k++){gc_mon *m=side?&run.enemy[k]:&run.party[k];actors[slots[side][k]]=&m->b;bt->party[side][k]=&m->b;run.party_order[k]=k;}}for(int k=0;k<4;k++)bt->field[k]=actors[k];
  bool ok=true;for(int k=0;k<8;k++){double frame[WIDTH+1];read_vector("I ",frame,WIDTH+1);if(frame[0]!=k)bad();load(actors[k],frame+1,k<4?k:k<6?0:2);ok&=compare(actors[k],frame+1,i,k,'I');}
  for(int k=0;k<8;k++){double m[4];read_vector("M ",m,4);if(m[0]!=k)bad();pr_pokemon *p=actors[k];p->ability_lookup_known=true;p->ability_index=integer(m[1],0,2);p->custom_ability=integer(m[2],-1,PR_ABILITY_COUNT-1);p->custom_passive=integer(m[3],-1,PR_ABILITY_COUNT-1);p->fusion_custom_ability=-1;p->fusion_ability_index=-1;p->lookup_ability_override=p->daily_boss_ability=-1;pr_refresh_ability(p);}
  for(int k=0;k<4;k++){run.party[k].ability_index=run.party[k].b.ability_index;run.party[k].variant=run.party[k].b.variant;run.enemy[k].ability_index=run.enemy[k].b.ability_index;run.enemy[k].variant=run.enemy[k].b.variant;}
  double tags[8];read_vector("IT ",tags,8);for(int k=0;k<8;k++){int count=integer(tags[k],0,3);if(count){pr_battler_tag *t=&actors[k]->tags[actors[k]->n_tags++];*t=(pr_battler_tag){.type=BATTLER_TAG_TYPE_AUTOTOMIZED,.turn_count=1,.source_move=MOVE_ID_AUTOTOMIZE,.source_id=-1,.autotomize_count=count};}}
  profile_read("D ",&sys.dex[SPECIES_ID_KELDEO].seen_attr,&sys.dex[SPECIES_ID_KELDEO].caught_attr);
  int party=u<2?u:u-2;if(pending)gc_rq_push(&run,gc_rp(RP_LEARN_MOVE,party,MOVE_ID_GROWL,0));gc_rphase learn=gc_rp(RP_LEARN_MOVE,party,move,0);ok&=gc_phase_start(&run,&learn);
  double f[1];read_vector("F ",f,1);int nf=integer(f[0],0,1);gc_rqueue *rq=&gc_int(&run)->rq;int count=0;for(int l=0;l<rq->n_levels;l++)for(int j=0;j<rq->n[l];j++){const gc_rphase *p=&rq->q[l][j];if(pending&&l==0&&j==0){ok&=p->type==RP_LEARN_MOVE&&p->a==party&&p->b==MOVE_ID_GROWL;continue;}if(count++>=nf){ok=false;continue;}double target[4];read_vector("FORM ",target,version==1?2:4);integer(target[0],0,7);integer(target[1],0,1);ok&=p->type==RP_REWARD_FORM&&p->a==party&&target[0]==u&&p->b==target[1];if(version==2)ok&=target[2]==l&&target[3]==j;}
  while(count<nf){double target[4];read_vector("FORM ",target,version==1?2:4);count++;ok=false;}ok&=count==nf;
  if(mode&&nf){gc_rphase form;if(pending&&!gc_rq_next(rq,&form))bad();if(!gc_rq_next(rq,&form))bad();ok&=gc_phase_start(&run,&form);}
  for(int k=0;k<8;k++){double frame[WIDTH+1];read_vector("O ",frame,WIDTH+1);if(frame[0]!=k)bad();ok&=compare(actors[k],frame+1,i,k,'O');}
  read_vector("OT ",tags,8);for(int k=0;k<8;k++){integer(tags[k],0,3);const pr_battler_tag *t=pr_get_battler_tag(actors[k],BATTLER_TAG_TYPE_AUTOTOMIZED);ok&=(t?t->autotomize_count:0)==tags[k];}
  uint64_t seen,caught;profile_read("DO ",&seen,&caught);ok&=sys.dex[SPECIES_ID_KELDEO].seen_attr==seen&&sys.dex[SPECIES_ID_KELDEO].caught_attr==caught;
  read_vector("T ",f,1);int nt=integer(f[0],1,2),texts=0,animations=0;gc_event ev;while(gc_run_next_event(&run,&ev)){if(ev.kind==GC_EV_EVOLVED)animations++;if(ev.kind!=GC_EV_MESSAGE)continue;if(texts++>=nt){ok=false;continue;}char expected[2048],actual[2048];if(sscanf(line_read("TEXT "),"%2047s %c",expected,&extra)!=1)bad();text_hex(&ev.msg,actual);ok&=!strcmp(expected,actual);}
  while(texts<nt){line_read("TEXT ");texts++;ok=false;}ok&=texts==nt&&animations==(mode&&nf);
  pr_rng rng;rng_read("RO ",&rng);ok&=rng_equal(rng,bt->rng);rng_read("BO ",&rng);ok&=rng_equal(rng,bt->battle_rng);if(strcmp(line_read("E"),"\n"))bad();if(bt->n_unported||bt->n_deferred||run.eng.overflow||rq->overflow)ok=false;
  if(!ok){if(failed<12)fprintf(stderr,"Keldeo LearnMove case%d actor%d mode%d unsupported%d profile%llu/%llu native%llu/%llu failed\n",i,u,mode,bt->n_unported,(unsigned long long)sys.dex[SPECIES_ID_KELDEO].seen_attr,(unsigned long long)sys.dex[SPECIES_ID_KELDEO].caught_attr,(unsigned long long)seen,(unsigned long long)caught);failed++;}
 }
 double end[1];read_vector("KELDEO_LEARN_COMPLETE ",end,1);if(end[0]!=n||fgetc(input)!=EOF)bad();printf("Keldeo LearnMove caller: %d compared, %d exact, %d failures\n",n,n-failed,failed);return failed?1:0;
}
