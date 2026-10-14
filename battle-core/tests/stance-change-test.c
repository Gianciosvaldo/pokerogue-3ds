/* Strict source scene-trigger and held Quiet-phase differential. */
#define main gas_replay_main
#include "gas-lifecycle-test.c"
#undef main
int main(int argc,char **argv)
{
 if(argc!=2)return 2;input=fopen(argv[1],"r");if(!input||!getenv("TURNTEST_LOCALES"))return 2;pr_closures_init();
 double h[3];read_vector("STANCE ",h,3);if(h[0]!=2)bad();int n=integer(h[1],1024,100000),failures=0;integer(h[2],0,INT32_MAX);
 for(int i=0;i<n;i++){
  static pr_battle bt;static pr_engine e;memset(&bt,0,sizeof bt);memset(&e,0,sizeof e);bt.engine=&e;e.n_levels=1;e.current=-1;bt.game_mode=GAME_MODES_CLASSIC;bt.ignoring_effect_source=-1;bt.last_used_pokeball=-1;pr_seed_from_ascii(&bt.wave_seed,"stance");
  double c[9];read_vector("C ",c,9);if(c[0]!=i)bad();int b=integer(c[1],0,3),kind=integer(c[2],0,1),delayed=integer(c[3],0,1),cmd=integer(c[4],0,PR_MOVE_COUNT-1),custom=integer(c[5],0,PR_ABILITY_COUNT-1),passive=integer(c[6],0,PR_ABILITY_COUNT-1),passive_flag=integer(c[7],0,1),suppressed=integer(c[8],0,1);if(b!=(i%2?2:0)||kind!=(i/2)%2)bad();
  rng_read("R ",&bt.rng);rng_read("B ",&bt.battle_rng);bt.rng_live=bt.battle_rng_live=true;
  double v[WIDTH];read_vector("I ",v,WIDTH);pr_pokemon p;load(&p,v,b);if(p.species!=SPECIES_ID_AEGISLASH)bad();p.ability_lookup_known=true;p.custom_ability=custom;p.fusion_custom_ability=p.lookup_ability_override=p.daily_boss_ability=-1;p.passive=passive;p.custom_passive=passive;p.passive_flag=passive_flag;p.ability_suppressed=suppressed;bt.field[b]=bt.party[b/2][0]=&p;bt.n_party[b/2]=1;
  bool ok=compare(&p,v,i,b,'I');bt.commands[b].command=cmd?COMMAND_FIGHT:-1;bt.commands[b].no_move=!cmd;bt.commands[b].move=cmd;
  pr_queue_msg_deferred(&bt,&e,pr_msg0("anchor"));int current=pr_phase_new(&e,PH_MOVE,b);e.current=current;e.pool[current].move.id=cmd==MOVE_ID_TACKLE?MOVE_ID_KINGS_SHIELD:MOVE_ID_TACKLE;
  if(delayed)pr_form_change_delayed(&bt,&p,kind?PR_FCT_SpeciesFormChangeActiveTrigger:PR_FCT_SpeciesFormChangePreMoveTrigger);else pr_form_change(&bt,&p,kind?PR_FCT_SpeciesFormChangeActiveTrigger:PR_FCT_SpeciesFormChangePreMoveTrigger);
  double q[4];read_vector("Q ",q,4);int count=integer(q[0],1,32),seen=0,quiet[32],nquiet=0;if(integer(q[1],1,PR_MAX_LEVELS)!=e.n_levels||integer(q[2],0,PR_MAX_LEVELS-1)!=e.current_level||integer(q[3],0,1)!=e.deferred_active)ok=false;
  for(int l=0;l<e.n_levels;l++)for(int k=0;k<e.level_len[l];k++){
   int ix=e.levels[l][k];pr_phase *ph=&e.pool[ix];if(ph->type!=PH_MESSAGE&&ph->type!=PH_QUIET_FORM_CHANGE)bad();
   if(seen++>=count){ok=false;continue;}char *row=line_read("P "),name[64],extra;int form,level,order;if(sscanf(row,"%d %d %63s %d %c",&level,&order,name,&form,&extra)!=4||level<0||level>=PR_MAX_LEVELS||order<0||order>=PR_MAX_PHASES||form<-1||form>1||(strcmp(name,"QuietFormChangePhase")&&strcmp(name,"MessagePhase")))bad();if(level!=l||order!=k||strcmp(name,pr_phase_name[ph->type])||form!=(ph->type==PH_QUIET_FORM_CHANGE?ph->move_id:-1)){if(i<1100)fprintf(stderr,"case%d queue%d C%s/%d JS%s/%d\n",i,seen-1,pr_phase_name[ph->type],ph->type==PH_QUIET_FORM_CHANGE?ph->move_id:-1,name,form);ok=false;}
   if(ph->type==PH_QUIET_FORM_CHANGE)quiet[nquiet++]=ix;
  }
  while(seen<count){line_read("P ");seen++;ok=false;}
  read_vector("A ",v,WIDTH);ok&=compare(&p,v,i,b,'A');
  for(int k=0;k<nquiet;k++){e.current=quiet[k];pr_phase_run(&bt,&e,quiet[k]);}
  read_vector("O ",v,WIDTH);ok&=compare(&p,v,i,b,'O');pr_rng want;rng_read("RO ",&want);ok&=rng_equal(want,bt.rng);rng_read("BO ",&want);ok&=rng_equal(want,bt.battle_rng);
  double f[17];read_vector("F ",f,17);int nf=integer(f[0],0,16);if(nf!=p.n_form_change_abilities)ok=false;for(int k=0;k<16;k++){integer(f[k+1],0,PR_ABILITY_COUNT-1);if(f[k+1]!=(k<p.n_form_change_abilities?p.form_change_abilities[k]:0))ok=false;}
  char text[2048]="-";int nt=0;pr_text_source src={locale_get,NULL,"en"};for(int k=0;k<e.n_events;k++)if(e.events[k].type==PR_EV_TEXT){if(++nt>1)bad();char rendered[1024];size_t len=pr_msg_format(&e.events[k].msg,&src,rendered,sizeof rendered);if(len>=sizeof rendered)bad();for(size_t j=0;j<len;j++)snprintf(text+2*j,3,"%02x",(unsigned char)rendered[j]);text[2*len]=0;}
  char *expected=line_read("T ");expected[strcspn(expected,"\n")]=0;if(strcmp(expected,"-")&&(strlen(expected)%2||strspn(expected,"0123456789abcdef")!=strlen(expected)))bad();if(strcmp(text,expected)){if(i<100)fprintf(stderr,"case%d text C%s JS%s\n",i,text,expected);ok=false;}
  if(strcmp(line_read("E"),"\n"))bad();if(bt.n_unported||e.overflow)ok=false;if(!ok){if(failures<24)fprintf(stderr,"case%d b%d kind%d delay%d cmd%d form%d q%d/%d F%d/%d unported%d %s\n",i,b,kind,delayed,cmd,p.form,seen,count,p.n_form_change_abilities,nf,bt.n_unported,bt.n_unported?pr_unported_name(bt.unported[0]):"none");failures++;}
 }
 double end[1];read_vector("STANCE_COMPLETE ",end,1);if(end[0]!=n||fgetc(input)!=EOF)bad();printf("Stance registered/Quiet phases: %d complete, %d exact, %d failures\n",n,n-failures,failures);return failures?1:0;
}
