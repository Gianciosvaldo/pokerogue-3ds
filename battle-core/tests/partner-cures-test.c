/* Native leaves and real queued phase/controller execution, with explicit projected state. */
#define main gas_replay_main
#define emit legacy_emit
#include "gas-lifecycle-test.c"
#undef emit
#undef main
static pr_pokemon mons[12];static int failures_seen;
static int identity(const pr_pokemon *p){for(int i=0;i<12;i++)if(p==&mons[i])return i;return -1;}
static void tohex(const char *s,char *out,size_t cap){size_t n=strlen(s);if(!n){strcpy(out,"-");return;}if(2*n+1>cap)bad();for(size_t k=0;k<n;k++)snprintf(out+2*k,3,"%02x",(unsigned char)s[k]);}
static void message_hex(const pr_msg *m,char *out,size_t cap){char text[1024];pr_text_source src={locale_get,NULL,NULL};size_t n=pr_msg_format(m,&src,text,sizeof text);if(n>=sizeof text)bad();tohex(text,out,cap);}
static void emit(const pr_phase *p,char scope,int group,int order){
 char name[96],text[2048]="-";int b=-1,person=-1,a=-1,c=-1,d=-1;double value=-1;
 snprintf(name,sizeof name,"%s",pr_phase_name[p->type]);
 if(p->type==PH_MARKER)snprintf(name,sizeof name,"Marker:%s",pr_phase_name[p->marker_of]);
 if(p->type==PH_RESET_STATUS){person=identity(p->pokemon);value=p->affect_confusion;a=0;}
 if(p->type==PH_POKEMON_HEAL){b=p->battler;value=p->heal_hp;a=p->heal_flags;if(a&PR_HEAL_MESSAGE)message_hex(&p->msg,text,sizeof text);}
 if(p->type==PH_SHOW_ABILITY){b=p->battler;value=p->anim_result;a=p->anim;c=b>=0&&b<4;}
 if(p->type==PH_MESSAGE)message_hex(&p->msg,text,sizeof text);
 if(p->type==PH_COMMON_ANIM){b=p->battler;value=p->anim;a=p->anim_target;}
 if(nq>=128)bad();snprintf(queue[nq++],sizeof queue[0],"Q %c %d %d %s %d %d %.17g %d %d %d %s\n",scope,group,order,name,b,person,value,a,c,d,text);
}
static void frame(pr_battle *bt,const char *key,int c,bool initial,bool *ok){
 const int tags[]={BATTLER_TAG_TYPE_PROTECTED,BATTLER_TAG_TYPE_HEAL_BLOCK,BATTLER_TAG_TYPE_CONFUSED,BATTLER_TAG_TYPE_NIGHTMARE};char prefix[8];snprintf(prefix,sizeof prefix,"%s ",key);
 for(int k=0;k<12;k++){
  double v[WIDTH+1];read_vector(prefix,v,WIDTH+1);if(v[0]!=k)bad();if(initial)load(&mons[k],v+1,k<6?0:2);
  bool same=compare(&mons[k],v+1,c,k,key[0]);if(!same){if(failures_seen<5){double got[WIDTH];capture(&mons[k],got);for(int j=0;j<WIDTH;j++)if(got[j]!=v[j+1])fprintf(stderr,"case%d %s actor%d field%d C%g JS%g\n",c,key,k,j,got[j],v[j+1]);}*ok=false;}
  snprintf(prefix,sizeof prefix,"%sX ",key);double x[5];read_vector(prefix,x,5);if(x[0]!=k)bad();int mask=integer(x[1],0,15),toxic=integer(x[2],-1,127),sleep=integer(x[3],-1,127),freeze=integer(x[4],-1,127);
  if(initial){mons[k].status_toxic_turns=toxic;mons[k].status_sleep_turns=sleep;mons[k].status_freeze_turns=freeze;for(int t=0;t<4;t++)if(mask&(1<<t))mons[k].tags[mons[k].n_tags++]=(pr_battler_tag){.type=tags[t],.turn_count=1,.source_id=mons[k].id};}
  else{int actual=0;for(int t=0;t<4;t++)if(pr_get_battler_tag(&mons[k],tags[t]))actual|=1<<t;
   if(actual!=mask||mons[k].status_toxic_turns!=toxic||mons[k].status_sleep_turns!=sleep||mons[k].status_freeze_turns!=freeze){if(failures_seen<5)fprintf(stderr,"case%d %s actor%d extras C%d/%d/%d/%d JS%d/%d/%d/%d\n",c,key,k,actual,mons[k].status_toxic_turns,mons[k].status_sleep_turns,mons[k].status_freeze_turns,mask,toxic,sleep,freeze);*ok=false;}}
  snprintf(prefix,sizeof prefix,"%s ",key);
 }
 (void)bt;
}
static void fields(pr_battle *bt){for(int side=0;side<2;side++)for(int slot=0;slot<2;slot++)bt->field[side*2+slot]=slot<bt->n_party[side]&&(slot==0||bt->double_battle)?bt->party[side][slot]:NULL;}
/* Validate integer tokens before scanf conversions, including overflow. */
static void partner_queue_numbers(void) {
 const char *p=line;
 for(int col=0;col<12;col++) {
  while(*p==' '||*p=='\t')p++;
  const char *start=p;while(*p&&*p!=' '&&*p!='\t'&&*p!='\n')p++;
  if(start==p)bad();
  if(col==2||col==3||(col>=5&&col<=10)) {
   char *end;errno=0;long long v=strtoll(start,&end,10);
   if(errno||end!=p||v<INT_MIN||v>INT_MAX)bad();
  }
 }
}
static void compare_queue(pr_battle *bt,pr_engine *e,int c,bool *ok){
 double qr[3];read_vector("QCOUNT ",qr,3);int n=integer(qr[0],0,128),levels=integer(qr[1],1,PR_MAX_LEVELS),deferred=integer(qr[2],0,1);if(levels!=e->n_levels||deferred!=e->deferred_active)*ok=false;nq=0;
 for(int l=0;l<e->n_levels;l++)for(int j=0;j<e->level_len[l];j++)emit(&e->pool[e->levels[l][j]],'L',l,j);
 int types[PH_COUNT_],nt=0;for(int t=0;t<PH_COUNT_;t++)if(e->n_dyn[t])types[nt++]=t;for(int j=1;j<nt;j++)for(int k=j;k>0&&strcmp(pr_phase_name[types[k]],pr_phase_name[types[k-1]])<0;k--){int t=types[k];types[k]=types[k-1];types[k-1]=t;}for(int k=0;k<nt;k++)for(int j=0;j<e->n_dyn[types[k]];j++)emit(&e->pool[e->dyn[types[k]][j]],'D',k,j);
 if(nq!=n)*ok=false;
 for(int j=0;j<n;j++){
  line_read("Q ");partner_queue_numbers();char scope,name[96],hex[2048],extra;int group,order,b,person,a,d,f;double value;
  if(sscanf(line,"Q %c %d %d %95s %d %d %lf %d %d %d %2047s %c",&scope,&group,&order,name,&b,&person,&value,&a,&d,&f,hex,&extra)!=11||(scope!='L'&&scope!='D')||group<0||group>=PR_MAX_LEVELS||order<0||order>=PR_MAX_PHASES||b< -1||b>3||person< -1||person>11||!isfinite(value)||value!=floor(value)||value< -1||value>INT32_MAX||a< -1||d< -1||d>1||f!= -1)bad();
  bool known=false;const char *type=!strncmp(name,"Marker:",7)?name+7:name;for(int t=0;t<PH_COUNT_;t++)if(pr_phase_name[t]&&!strcmp(pr_phase_name[t],type))known=true;if(!known)bad();
  if(strcmp(hex,"-")){size_t n=strlen(hex);if(n%2)bad();for(size_t k=0;k<n;k++)if(!isxdigit((unsigned char)hex[k]))bad();}
  if(j>=nq||strcmp(line,queue[j])){if(failures_seen<5)fprintf(stderr,"case%d queue%d C:%sJS:%s",c,j,j<nq?queue[j]:"missing\n",line);*ok=false;}
 }
 (void)bt;
}
int main(int argc,char **argv){
 if(argc!=2||!getenv("TURNTEST_LOCALES"))return 2;input=fopen(argv[1],"r");if(!input)return 2;pr_closures_init();
 double h[3];read_vector("PARTNERCURES ",h,3);if(h[0]!=1)bad();int n=integer(h[1],12800,100000),seed=integer(h[2],0,INT32_MAX),fail=0;
 const int moves[]={MOVE_ID_HEAL_BELL,MOVE_ID_AROMATHERAPY,MOVE_ID_POLLEN_PUFF,MOVE_ID_HEAL_PULSE};int attrs[4]={-1,-1,-1,-1};for(int k=0;k<4;k++){const pr_move *m=&pr_moves[moves[k]];for(int j=0;j<m->nattr;j++)if(pr_attrs[m->attr0+j].cls==(k==3?PR_AC_HealAttr:k==2?PR_AC_HealOnAllyAttr:PR_AC_PartyStatusCureAttr))attrs[k]=m->attr0+j;if(attrs[k]<0)bad();}
 for(int c=0;c<n;c++){
  static pr_battle bt;static pr_engine e;memset(&bt,0,sizeof bt);memset(&e,0,sizeof e);bt.engine=&e;e.current=-1;e.n_levels=1;bt.game_mode=GAME_MODES_CLASSIC;bt.wave_index=3;bt.turn=1;bt.ignoring_effect_source=-1;bt.last_player_involved=bt.last_enemy_involved=-1;
  char wave[64],battle[64];snprintf(wave,sizeof wave,"partner-cures-wave-%d",seed);snprintf(battle,sizeof battle,"partner-cures-%d-%d",seed,c);pr_seed_from_ascii(&bt.wave_seed,wave);pr_seed_from_ascii(&bt.battle_seed,battle);
  double x[8];read_vector("C ",x,8);if(x[0]!=c)bad();int mode=integer(x[1],0,3),u=integer(x[2],0,11),t=integer(x[3],0,11);bt.double_battle=integer(x[4],0,1);bt.n_party[0]=integer(x[5],1,6);bt.n_party[1]=integer(x[6],1,6);bool swap=integer(x[7],0,1);if(mode!=(c<n-512?c%3:c<n-256?3:1))bad();
  for(int side=0;side<2;side++)for(int k=0;k<bt.n_party[side];k++)bt.party[side][k]=&mons[side*6+k];fields(&bt);
  double g[2];read_vector("G ",g,2);int gas=integer(g[0],0,2);if(g[1]<0||g[1]>UINT32_MAX||floor(g[1])!=g[1])bad();if(gas){bt.n_arena_tags=1;bt.arena_tags[0]=(pr_arena_tag){.type=ARENA_TAG_TYPE_NEUTRALIZING_GAS,.side=ARENA_TAG_SIDE_BOTH,.source_count=gas,.source_id=(int64_t)g[1],.layers=1};}
  rng_read("R ",&bt.rng);rng_read("B ",&bt.battle_rng);bt.rng_live=bt.battle_rng_live=true;bool ok=true;frame(&bt,"I",c,true,&ok);
  if(mode==2){double cond[1];read_vector("COND ",cond,1);integer(cond[0],0,1);const pr_move *m=&pr_moves[moves[2]];if(m->conditions.n!=1)bad();bool got=pr_closure_call(&bt,m->conditions.v[0],PR_AC_HealOnAllyAttr,0,&mons[u],&mons[t],moves[2]);if(got!=cond[0]){if(failures_seen<5)fprintf(stderr,"case%d condition C%d JS%g\n",c,got,cond[0]);ok=false;}}
  int ph=pr_phase_new(&e,PH_MOVE_EFFECT,0);e.pool[ph].move_id=moves[mode];pr_apply_move_effect(&bt,&e,ph,attrs[mode],&mons[u],&mons[t]);
  double ret[1];read_vector("RESULT ",ret,1);integer(ret[0],0,1);frame(&bt,"O",c,false,&ok);compare_queue(&bt,&e,c,&ok);
  if(swap){int side=mons[u].is_player?0:1;if(bt.n_party[side]>1){pr_pokemon *p=bt.party[side][0];bt.party[side][0]=bt.party[side][1];bt.party[side][1]=p;}fields(&bt);}
  /* This production controller drains the supplied queue to its TurnInit sentinel. */
  pr_run_player_cleanup(&bt,&e);
  frame(&bt,"Z",c,false,&ok);double world[3];read_vector("WORLD ",world,3);integer(world[0],0,1);integer(world[1],-1,1);integer(world[2],-1,1);if(world[0]!=bt.ability_bar_visible||world[1]!=bt.last_player_involved||world[2]!=bt.last_enemy_involved){if(failures_seen<5)fprintf(stderr,"case%d world C%d/%d/%d JS%g/%g/%g\n",c,bt.ability_bar_visible,bt.last_player_involved,bt.last_enemy_involved,world[0],world[1],world[2]);ok=false;}
  double texts[1];read_vector("TEXTS ",texts,1);int count=integer(texts[0],0,128),got=0;
  for(int j=0;j<e.n_events;j++)if(e.events[j].type==PR_EV_TEXT){if(got>=count){got++;ok=false;continue;}char actual[2048];message_hex(&e.events[j].msg,actual,sizeof actual);char *expected=line_read("T ");size_t l=strlen(expected);if(!l||expected[l-1]!='\n')bad();expected[l-1]=0;if(strcmp(actual,expected)){if(failures_seen<5)fprintf(stderr,"case%d text%d C:%s JS:%s\n",c,got,actual,expected);ok=false;}got++;}
  if(got!=count){if(failures_seen<5)fprintf(stderr,"case%d texts C%d JS%d\n",c,got,count);ok=false;if(got<count)for(int j=got;j<count;j++)line_read("T ");}
  pr_rng want;rng_read("RO ",&want);ok&=rng_equal(want,bt.rng);rng_read("BO ",&want);ok&=rng_equal(want,bt.battle_rng);
  if(strcmp(line_read("E"),"\n"))bad();if(bt.n_unported||bt.n_deferred||e.overflow){if(failures_seen<5)fprintf(stderr,"case%d unsupported%d(%s) deferred%d overflow%d\n",c,bt.n_unported,bt.n_unported?pr_unported_name(bt.unported[0]):"none",bt.n_deferred,e.overflow);ok=false;}
  if(!ok){if(fail<15)fprintf(stderr,"partner case%d mode%d user%d target%d failed\n",c,mode,u,t);fail++;failures_seen=fail;}
 }
 double end[1];read_vector("PARTNERCURES_COMPLETE ",end,1);if(end[0]!=n||fgetc(input)!=EOF)bad();printf("Partner cures: %d complete, %d exact, %d failures\n",n,n-fail,fail);return fail?1:0;
}
