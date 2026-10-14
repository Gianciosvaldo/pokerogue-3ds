/* Replay actual source Imprison calls; validates complete transport and messages. */
#define main gas_replay_main
#include "gas-lifecycle-test.c"
#undef main
int main(int argc,char **argv)
{
 if(argc!=2)return 2;input=fopen(argv[1],"r");if(!input)return 2;
 double h[2];read_vector("IMPRISON ",h,2);if(h[0]!=1)bad();int n=integer(h[1],512,100000),failures=0;
 pr_seedstr wave;char *w=line_read("W ");w[strcspn(w,"\n")]=0;pr_seed_from_ascii(&wave,w);
 for(int c=0;c<n;c++){
  static pr_battle bt;static pr_engine e;memset(&bt,0,sizeof bt);memset(&e,0,sizeof e);e.n_levels=1;e.current=-1;bt.engine=&e;bt.ignoring_effect_source=-1;bt.wave_seed=wave;
  double v[12];read_vector("C ",v,12);if(v[0]!=c)bad();int kind=integer(v[1],0,7),side=integer(v[2],0,2);bool quiet=integer(v[3],0,1);if(kind!=(c/64)%8||side!=(c%64)%3||quiet!=!!(c%64&8))bad();
  pr_pokemon p[2]={{0}};for(int b=0;b<2;b++){
   p[b].species=b?SPECIES_ID_MAGIKARP:SPECIES_ID_RATTATA;if(v[b+4]<0||v[b+4]>UINT32_MAX||v[b+4]!=floor(v[b+4]))bad();p[b].id=(uint32_t)v[b+4];p[b].hp=integer(v[b+6],0,INT32_MAX);p[b].on_field=true;p[b].is_player=!b;p[b].n_moves=1;p[b].moves[0]=MOVE_ID_SPLASH;p[b].ability=p[b].base_ability=ABILITY_ID_BALL_FETCH;p[b].ability_override=-1;p[b].added_type=POKEMON_TYPE_UNKNOWN;p[b].tera_type=-1;
   bt.field[b*2]=bt.party[b][0]=&p[b];bt.n_party[b]=1;
  }
  p[0].switch_out_status=integer(v[8],0,1);p[0].moves[0]=integer(v[10],1,PR_MOVE_COUNT-1);
  if(integer(v[9],0,1)){p[0].summon_moves_active=true;p[0].transform_n_moves=1;p[0].transform_moves[0]=MOVE_ID_SPLASH;}
  if(v[11]<-1||v[11]>UINT32_MAX||v[11]!=floor(v[11]))bad();int64_t id=v[11];
  rng_read("R ",&bt.rng);bt.rng_live=true;
  pr_arena_add_tag(&bt,&e,ARENA_TAG_TYPE_IMPRISON,0,MOVE_ID_IMPRISON,id,side,true);
  memset(&e,0,sizeof e);e.n_levels=1;e.current=-1;
  if(kind==0){pr_arena_remove_all_tags(&bt,&e);pr_arena_add_tag(&bt,&e,ARENA_TAG_TYPE_IMPRISON,0,MOVE_ID_IMPRISON,id,side,quiet);}
  if(kind==1)pr_arena_remove_all_tags(&bt,&e);
  if(kind==2)pr_arena_remove_tag_on_side(&bt,&e,ARENA_TAG_TYPE_IMPRISON,ARENA_TAG_SIDE_BOTH,quiet);
  if(kind==3)pr_arena_lapse_tags(&bt,&e);
  bool cancelled=false;
  if(kind==4||kind==5){int ph=pr_phase_new(&e,PH_MOVE,2);e.current=ph;e.pool[ph].move.id=MOVE_ID_SPLASH;pr_lapse_tag(&bt,&e,&p[1],BATTLER_TAG_TYPE_IMPRISON,kind==4?BATTLER_TAG_LAPSE_TYPE_PRE_MOVE:BATTLER_TAG_LAPSE_TYPE_AFTER_MOVE);cancelled=e.pool[ph].cancelled;}
  if(kind==6){pr_remove_tag(&bt,&e,&p[1],BATTLER_TAG_TYPE_IMPRISON);int ph=pr_phase_new(&e,PH_POST_SUMMON,2);pr_post_summon_phase(&bt,&e,ph);}
  if(kind==7)pr_arena_add_tag(&bt,&e,ARENA_TAG_TYPE_IMPRISON,0,MOVE_ID_IMPRISON,id,side,quiet);
  pr_rng expected_rng;rng_read("RO ",&expected_rng);
  double want[8];read_vector("O ",want,8);double got[8]={bt.n_arena_tags,cancelled};for(int b=0;b<2;b++){const pr_battler_tag *t=pr_get_battler_tag(&p[b],BATTLER_TAG_TYPE_IMPRISON);got[2+b*3]=!!t;got[3+b*3]=t?t->turn_count:0;got[4+b*3]=t?t->source_id:-1;}
  bool ok=!bt.n_unported&&!e.overflow&&rng_equal(expected_rng,bt.rng);if(bt.n_unported && c<512)fprintf(stderr,"case%d unported %d first%s\n",c,bt.n_unported,pr_unported_name(bt.unported[0]));for(int k=0;k<8;k++)if(got[k]!=want[k]){if(c<512)fprintf(stderr,"case%d kind%d state%d field%d C%.0f JS%.0f\n",c,kind,c%64,k,got[k],want[k]);ok=false;}
  double q[1];read_vector("Q ",q,1);int expected=integer(q[0],0,24),seen=0;
  for(int l=0;l<e.n_levels;l++)for(int j=0;j<e.level_len[l];j++){
   pr_phase *ph=&e.pool[e.levels[l][j]];if(ph->type!=PH_MESSAGE){ok=false;continue;}
   char text[1024],hex[2048];pr_text_source src={locale_get,NULL,"en"};size_t len=pr_msg_format(&ph->msg,&src,text,sizeof text);if(len>=sizeof text)bad();for(size_t k=0;k<len;k++)snprintf(hex+2*k,3,"%02x",(unsigned char)text[k]);hex[2*len]=0;
   if(seen++>=expected){ok=false;continue;}char *s=line_read("T ");s[strcspn(s,"\n")]=0;if(strlen(s)%2||strspn(s,"0123456789abcdef")!=strlen(s))bad();if(strcmp(hex,s)){if(c<512)fprintf(stderr,"case%d text C%s JS%s\n",c,hex,s);ok=false;}
  }
  while(seen<expected){line_read("T ");seen++;ok=false;}if(strcmp(line_read("E"),"\n"))bad();if(!ok)failures++;
 }
 double f[1];read_vector("IMPRISON_COMPLETE ",f,1);if(f[0]!=n||fgetc(input)!=EOF)bad();printf("Imprison: %d complete, %d exact, %d failures\n",n,n-failures,failures);return failures?1:0;
}
