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
 if(p->type==PH_MOVE){b=p->battler;person=identity(p->pokemon);value=p->move.id;a=p->timing;c=p->use_mode;}
 if(p->type==PH_POKEMON_ANIM){person=identity(p->pokemon);value=p->anim;a=0;}
 if(p->type==PH_STAT_STAGE_CHANGE){
  b=p->battler;person=identity(p->source_p);value=p->n_ssc;a=p->ssc_self_target+2*p->ssc_processed+4*p->ssc_ignore_abilities+8*p->ssc_message;c=p->ssc_source_type;
  char changes[128];size_t n=0;changes[0]=0;for(int j=0;j<p->n_ssc;j++)n+=(size_t)snprintf(changes+n,sizeof changes-n,"%s%d:%d",j?",":"",p->ssc_stat[j],p->ssc_stages[j]);tohex(changes,text,sizeof text);
 }
 if(p->type==PH_SHOW_ABILITY){b=p->battler;value=p->anim_result;a=p->anim==UINT16_MAX?-1:p->anim;c=b>=0&&b<4;}
 if(p->type==PH_MESSAGE)message_hex(&p->msg,text,sizeof text);
 if(p->type==PH_COMMON_ANIM){b=p->battler;value=p->anim;a=p->anim_target;}
 if(nq>=128)bad();snprintf(queue[nq++],sizeof queue[0],"Q %c %d %d %s %d %d %.17g %d %d %d %s\n",scope,group,order,name,b,person,value,a,c,d,text);
}
static void tag_numbers(void) {
 const char *p=line;
 for(int col=0;col<9;col++) {
  while(*p==' '||*p=='\t')p++;
  const char *start=p;while(*p&&*p!=' '&&*p!='\t'&&*p!='\n')p++;
  if(start==p)bad();
  if(col&&col!=7){char *end;errno=0;long long v=strtoll(start,&end,10);
   long long lo=col==4?INT16_MIN:col==6?-1:0;
   long long hi=col==1?11:col==2?15:col==3?1:col==4?INT16_MAX:col==5?PR_MOVE_COUNT-1:col==8?1:UINT32_MAX;
   if(errno||end!=p||v<lo||v>hi)bad();}
 }
}
static void frame(pr_battle *bt,const char *key,int c,bool initial,bool *ok){
 const int tags[]={BATTLER_TAG_TYPE_FLYING,BATTLER_TAG_TYPE_UNDERGROUND,BATTLER_TAG_TYPE_UNDERWATER,BATTLER_TAG_TYPE_HIDDEN};char prefix[8];snprintf(prefix,sizeof prefix,"%s ",key);
 for(int k=0;k<12;k++){
  double v[WIDTH+1];read_vector(prefix,v,WIDTH+1);if(v[0]!=k)bad();if(initial)load(&mons[k],v+1,k<6?0:2);
  bool same=compare(&mons[k],v+1,c,k,key[0]);if(!same){if(failures_seen<5){double got[WIDTH];capture(&mons[k],got);for(int j=0;j<WIDTH;j++)if(got[j]!=v[j+1])fprintf(stderr,"case%d %s actor%d field%d C%g JS%g\n",c,key,k,j,got[j],v[j+1]);}*ok=false;}
  snprintf(prefix,sizeof prefix,"%sX ",key);line_read(prefix);tag_numbers();char hex[2048],extra;int actor,mask,has,turn,move,present;double source;
  if(sscanf(line+strlen(prefix),"%d %d %d %d %d %lf %2047s %d %c",&actor,&mask,&has,&turn,&move,&source,hex,&present,&extra)!=8||present<0||present>1||actor!=k||mask<0||mask>15||has<0||has>1||turn<INT16_MIN||turn>INT16_MAX||move<0||move>=PR_MOVE_COUNT||!isfinite(source)||floor(source)!=source||source< -1||source>UINT32_MAX)bad();
  char keytext[1024];size_t hn=!strcmp(hex,"-")?0:strlen(hex);if(hn%2||hn/2>=sizeof keytext)bad();for(size_t j=0;j<hn;j++){if(!isxdigit((unsigned char)hex[j]))bad();}for(size_t j=0;j<hn/2;j++){unsigned byte;if(sscanf(hex+2*j,"%2x",&byte)!=1||!byte)bad();keytext[j]=(char)byte;}keytext[hn/2]=0;
  if(initial){
   mons[k].on_field=present;
   static const int moves[]={MOVE_ID_FLY,MOVE_ID_DIG,MOVE_ID_DIVE,MOVE_ID_PHANTOM_FORCE};
   for(int j=0;j<4;j++)if(mask&(1<<j))mons[k].tags[mons[k].n_tags++]=(pr_battler_tag){.type=tags[j],.turn_count=1,.source_move=moves[j],.source_id=-1,.source_index=-1};
   if(has){int cache=-1;if(!strcmp(keytext,"curly"))cache=0;else for(int f=0;f<PR_FORM_COUNT;f++)if(!strcmp(keytext,pr_forms[f].form_key)){cache=f+1;break;}if(cache<0)bad();mons[k].tags[mons[k].n_tags++]=(pr_battler_tag){.type=BATTLER_TAG_TYPE_COMMANDED,.turn_count=turn,.source_move=move,.source_id=(int64_t)source,.commanded_form_key=cache,.source_index=-1};}
  }else{
   int actual=0;for(int j=0;j<4;j++)if(pr_get_battler_tag(&mons[k],tags[j]))actual|=1<<j;
   const pr_battler_tag *t=pr_get_battler_tag(&mons[k],BATTLER_TAG_TYPE_COMMANDED);
   if(present!=mons[k].on_field||mask!=actual||has!=!!t||(t&&(turn!=t->turn_count||move!=t->source_move||source!=t->source_id||strcmp(keytext,pr_commanded_form_key(t))))){if(failures_seen<5)fprintf(stderr,"case%d %s actor%d tag C%s JS%s\n",c,key,k,t?pr_commanded_form_key(t):"absent",keytext);*ok=false;}
  }
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
  if(sscanf(line,"Q %c %d %d %95s %d %d %lf %d %d %d %2047s %c",&scope,&group,&order,name,&b,&person,&value,&a,&d,&f,hex,&extra)!=11||(scope!='L'&&scope!='D')||group<0||group>=PR_MAX_LEVELS||order<0||order>=PR_MAX_PHASES||b< -1||b>3||person< -1||person>11||!isfinite(value)||value!=floor(value)||value< -1||value>INT32_MAX||a< -1||d< -1||d>255||f!= -1)bad();
  bool known=false;const char *type=!strncmp(name,"Marker:",7)?name+7:name;for(int t=0;t<PH_COUNT_;t++)if(pr_phase_name[t]&&!strcmp(pr_phase_name[t],type))known=true;if(!known)bad();
  if(strcmp(hex,"-")){size_t n=strlen(hex);if(n%2)bad();for(size_t k=0;k<n;k++)if(!isxdigit((unsigned char)hex[k]))bad();}
  if(j>=nq||strcmp(line,queue[j])){if(failures_seen<5)fprintf(stderr,"case%d queue%d C:%sJS:%s",c,j,j<nq?queue[j]:"missing\n",line);*ok=false;}
 }
 (void)bt;
}
int main(int argc,char **argv){
 if(argc!=2||!getenv("TURNTEST_LOCALES"))return 2;input=fopen(argv[1],"r");if(!input)return 2;pr_closures_init();
 double h[3];read_vector("COMMANDER ",h,3);if(h[0]!=1)bad();int n=integer(h[1],16896,100000),seed=integer(h[2],0,INT32_MAX),fail=0,base=-1;
 const pr_move *m=&pr_moves[MOVE_ID_ORDER_UP];int attr=-1;for(int j=0;j<m->nattr;j++)if(pr_attrs[m->attr0+j].cls==PR_AC_OrderUpStatBoostAttr)attr=m->attr0+j;if(attr<0)bad();
 for(int c=0;c<n;c++){
  static pr_battle bt;static pr_engine e;memset(&bt,0,sizeof bt);memset(&e,0,sizeof e);bt.engine=&e;e.current=-1;e.n_levels=1;bt.game_mode=GAME_MODES_CLASSIC;bt.wave_index=3;bt.turn=1;bt.ignoring_effect_source=-1;bt.last_player_involved=bt.last_enemy_involved=-1;
  char wave[64],battle[64];snprintf(wave,sizeof wave,"commander-wave-%d",seed);snprintf(battle,sizeof battle,"commander-%d-%d",seed,c);pr_seed_from_ascii(&bt.wave_seed,wave);pr_seed_from_ascii(&bt.battle_seed,battle);
  double x[11];read_vector("C ",x,11);if(x[0]!=c)bad();int mode=integer(x[1],0,3),u=integer(x[2],0,11),t=integer(x[3],0,11);bt.double_battle=integer(x[4],0,1);bt.n_party[0]=integer(x[5],2,6);bt.n_party[1]=integer(x[6],2,6);bool simulated=integer(x[7],0,1);int lapse=integer(x[8],0,BATTLER_TAG_LAPSE_TYPE_CUSTOM),orderUser=integer(x[9],0,11);if(mode==3&&base<0){base=c;if(base<16384)bad();}if((base<0?mode!=c%3:mode!=3)||x[10]<0||x[10]>UINT32_MAX||floor(x[10])!=x[10])bad();int64_t sourceId=(int64_t)x[10];
  for(int side=0;side<2;side++)for(int k=0;k<bt.n_party[side];k++)bt.party[side][k]=&mons[side*6+k];fields(&bt);
  double g[3];read_vector("G ",g,3);int gas=integer(g[0],0,2);if(g[1]<0||g[1]>UINT32_MAX||floor(g[1])!=g[1])bad();if(gas){bt.n_arena_tags=1;bt.arena_tags[0]=(pr_arena_tag){.type=ARENA_TAG_TYPE_NEUTRALIZING_GAS,.side=ARENA_TAG_SIDE_BOTH,.source_count=gas,.source_id=(int64_t)g[1],.layers=1};}
  if(integer(g[2],0,1))bt.arena_tags[bt.n_arena_tags++]=(pr_arena_tag){.type=ARENA_TAG_TYPE_TRICK_ROOM,.side=ARENA_TAG_SIDE_BOTH,.turn_count=5,.layers=1};
  rng_read("R ",&bt.rng);rng_read("B ",&bt.battle_rng);bt.rng_live=bt.battle_rng_live=true;bool ok=true;frame(&bt,"I",c,true,&ok);
  double jobs[1];read_vector("JOBS ",jobs,1);int nj=integer(jobs[0],0,5);
  for(int j=0;j<nj;j++){double v[5];read_vector("J ",v,5);int kind=integer(v[0],0,2),actor=integer(v[1],0,11),battler=integer(v[2],0,3),move=integer(v[3],1,PR_MOVE_COUNT-1),timing=integer(v[4],0,2);int mp=pr_phase_new(&e,PH_MOVE,battler);e.pool[mp].pokemon=&mons[actor];e.pool[mp].move.id=move;e.pool[mp].timing=timing;e.pool[mp].use_mode=MOVE_USE_MODE_NORMAL;int8_t targets[]={mons[actor].is_player?2:0};e.pool[mp].targets=pr_targets_new(&e,targets,1);if(kind==2)pr_phase_add_deferred(&bt,&e,mp);else if(kind==1)pr_phase_unshift(&bt,&e,mp);else pr_phase_push(&bt,&e,mp);}
  compare_queue(&bt,&e,c,&ok);
  int ph=pr_phase_new(&e,PH_MOVE_EFFECT,pr_battler_index(&bt,&mons[u]));e.pool[ph].move_id=MOVE_ID_ORDER_UP;int result=-1;
  if(mode==0){pr_ab_params params={.pokemon=&mons[u],.simulated=simulated,.move=-1};pr_apply_ab_attrs(&bt,PR_AC_CommanderAbAttr,&params);result=!!pr_get_battler_tag(&mons[orderUser],BATTLER_TAG_TYPE_COMMANDED);}
  else if(mode==1)result=pr_add_tag(&bt,&e,&mons[t],BATTLER_TAG_TYPE_COMMANDED,9,MOVE_ID_TACKLE,sourceId);
  else if(mode==2)pr_apply_move_effect(&bt,&e,ph,attr,&mons[u],&mons[t]);
  else {int post=pr_phase_new(&e,PH_POST_SUMMON,pr_battler_index(&bt,&mons[u]));pr_post_summon_phase(&bt,&e,post);result=!!pr_get_battler_tag(&mons[orderUser],BATTLER_TAG_TYPE_COMMANDED);}
  double ret[1];read_vector("RESULT ",ret,1);integer(ret[0],0,1);if(result>=0&&result!=ret[0])ok=false;frame(&bt,"O",c,false,&ok);compare_queue(&bt,&e,c,&ok);
  double form[1];read_vector("FORM ",form,1);mons[u].form=integer(form[0],0,2);pr_recalculate_stats(&bt,&mons[u]);
  pr_apply_move_effect(&bt,&e,ph,attr,&mons[orderUser],&mons[t]);read_vector("ORDER_RESULT ",ret,1);integer(ret[0],0,1);
  frame(&bt,"P",c,false,&ok);compare_queue(&bt,&e,c,&ok);
  double trapped[1];read_vector("TRAPPED ",trapped,1);integer(trapped[0],0,1);if(pr_is_trapped(&bt,&mons[orderUser],NULL)!=trapped[0]){if(failures_seen<5)fprintf(stderr,"case%d trapping differs\n",c);ok=false;}
  pr_lapse_tags(&bt,&e,&mons[orderUser],lapse);frame(&bt,"L",c,false,&ok);compare_queue(&bt,&e,c,&ok);
  for(int j=0;j<12;j++)while(pr_remove_move_phase(&bt,&e,&mons[j])){}
  compare_queue(&bt,&e,c,&ok);pr_run_player_cleanup(&bt,&e);
  frame(&bt,"Z",c,false,&ok);double world[3];read_vector("WORLD ",world,3);integer(world[0],0,1);integer(world[1],-1,1);integer(world[2],-1,1);if(world[0]!=bt.ability_bar_visible||world[1]!=bt.last_player_involved||world[2]!=bt.last_enemy_involved){if(failures_seen<5)fprintf(stderr,"case%d world differs\n",c);ok=false;}
  double texts[1];read_vector("TEXTS ",texts,1);int count=integer(texts[0],0,128),got=0;
  for(int j=0;j<e.n_events;j++)if(e.events[j].type==PR_EV_TEXT){if(got>=count){got++;ok=false;continue;}char actual[2048];message_hex(&e.events[j].msg,actual,sizeof actual);char *expected=line_read("T ");size_t l=strlen(expected);if(!l||expected[l-1]!='\n')bad();expected[l-1]=0;if(strcmp(actual,expected)){if(failures_seen<5)fprintf(stderr,"case%d text%d C:%s JS:%s\n",c,got,actual,expected);ok=false;}got++;}
  if(got!=count){ok=false;if(got<count)for(int j=got;j<count;j++)line_read("T ");}
  pr_rng want;rng_read("RO ",&want);if(!rng_equal(want,bt.rng)){if(failures_seen<5)fprintf(stderr,"case%d global RNG differs\n",c);ok=false;}rng_read("BO ",&want);if(!rng_equal(want,bt.battle_rng)){if(failures_seen<5)fprintf(stderr,"case%d battle RNG differs\n",c);ok=false;}
  if(strcmp(line_read("E"),"\n"))bad();if(bt.n_unported||bt.n_deferred||e.overflow){if(failures_seen<5)fprintf(stderr,"case%d unsupported%d(%s) deferred%d overflow%d\n",c,bt.n_unported,bt.n_unported?pr_unported_name(bt.unported[0]):"none",bt.n_deferred,e.overflow);ok=false;}
  if(!ok){if(fail<15)fprintf(stderr,"commander case%d mode%d user%d target%d failed\n",c,mode,u,t);fail++;failures_seen=fail;}
 }
 double end[1];read_vector("COMMANDER_COMPLETE ",end,1);if(base<16384||n-base<512||end[0]!=n||fgetc(input)!=EOF)bad();printf("Commander: %d complete, %d exact, %d failures\n",n,n-fail,fail);return fail?1:0;
}
