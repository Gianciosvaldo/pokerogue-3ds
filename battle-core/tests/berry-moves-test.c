/* Native leaves and real queued phase/controller execution, with explicit projected state. */
#define main gas_replay_main
#define emit legacy_emit
#include "gas-lifecycle-test.c"
#undef emit
#undef main
static pr_pokemon mons[4];static int failures_seen;
static int callback_trace[32][2],n_callbacks;
void __real_pr_apply_ab_attrs(pr_battle *bt,uint16_t cls,pr_ab_params *pp);
static int identity(const pr_pokemon *p){for(int i=0;i<4;i++)if(p==&mons[i])return i;return -1;}
void __wrap_pr_apply_ab_attrs(pr_battle *bt,uint16_t cls,pr_ab_params *pp){
 int kind=cls==PR_AC_PostItemLostAbAttr?0:cls==PR_AC_HealFromBerryUseAbAttr?1:cls==PR_AC_BlockItemTheftAbAttr?2:cls==PR_AC_DoubleBerryEffectAbAttr?3:-1;
 if(kind>=0){if(n_callbacks>=32)bad();callback_trace[n_callbacks][0]=kind;callback_trace[n_callbacks++][1]=identity(pp->pokemon);}
 __real_pr_apply_ab_attrs(bt,cls,pp);
}
static void tohex(const char *s,char *out,size_t cap){size_t n=strlen(s);if(!n){strcpy(out,"-");return;}if(2*n+1>cap)bad();for(size_t k=0;k<n;k++)snprintf(out+2*k,3,"%02x",(unsigned char)s[k]);}
static void message_hex(const pr_msg *m,char *out,size_t cap){char text[1024];pr_text_source src={locale_get,NULL,NULL};size_t n=pr_msg_format(m,&src,text,sizeof text);if(n>=sizeof text)bad();tohex(text,out,cap);}
static void emit(const pr_phase *p,char scope,int group,int order){
 char name[96],text[2048]="-";int b=-1,person=-1,a=-1,c=-1,d=-1;double value=-1;
 snprintf(name,sizeof name,"%s",pr_phase_name[p->type]);
 if(p->type==PH_MARKER)snprintf(name,sizeof name,"Marker:%s",pr_phase_name[p->marker_of]);
 if(p->type==PH_RESET_STATUS){person=identity(p->pokemon);value=p->affect_confusion;a=0;}
 if(p->type==PH_POKEMON_HEAL){b=p->battler;value=p->heal_hp;a=p->heal_flags;if(a&PR_HEAL_MESSAGE)message_hex(&p->msg,text,sizeof text);}
 if(p->type==PH_STAT_STAGE_CHANGE){b=p->battler;person=identity(p->source_p);if(p->n_ssc!=1||p->ssc_source_type)bad();value=p->ssc_stat[0];a=p->ssc_stages[0];c=p->n_ssc;d=p->ssc_message+2*p->ssc_ignore_abilities+4*p->ssc_processed+8*p->ssc_self_target;}
 if(p->type==PH_SHOW_ABILITY){b=p->battler;value=p->anim_result;a=p->anim;c=b>=0&&b<4;}
 if(p->type==PH_MESSAGE)message_hex(&p->msg,text,sizeof text);
 if(p->type==PH_COMMON_ANIM){b=p->battler;value=p->anim;a=p->anim_target;}
 if(nq>=128)bad();snprintf(queue[nq++],sizeof queue[0],"Q %c %d %d %s %d %d %.17g %d %d %d %s\n",scope,group,order,name,b,person,value,a,c,d,text);
}
static const int extra_tags[]={BATTLER_TAG_TYPE_HEAL_BLOCK,BATTLER_TAG_TYPE_CONFUSED,BATTLER_TAG_TYPE_NIGHTMARE,BATTLER_TAG_TYPE_CRIT_BOOST,BATTLER_TAG_TYPE_UNBURDEN};
static void frame(pr_battle *bt,const char *key,int c,bool initial,bool *ok){
 char prefix[8];snprintf(prefix,sizeof prefix,"%s ",key);
 for(int k=0;k<4;k++){
  double v[WIDTH+1];read_vector(prefix,v,WIDTH+1);if(v[0]!=k)bad();if(initial)load(&mons[k],v+1,k<2?0:2);
  if(!compare(&mons[k],v+1,c,k,key[0])){if(failures_seen<5){double got[WIDTH];capture(&mons[k],got);for(int j=0;j<WIDTH;j++)if(got[j]!=v[j+1])fprintf(stderr,"berry%d %s actor%d field%d C%g JS%g\n",c,key,k,j,got[j],v[j+1]);}*ok=false;}
  snprintf(prefix,sizeof prefix,"%sX ",key);double x[48];read_vector(prefix,x,48);if(x[0]!=k)bad();int mask=integer(x[1],0,31);pr_pokemon *p=&mons[k];
  if(initial)p->n_tags=0;int actual=0;
  for(int t=0;t<5;t++){
   int turns=integer(x[2+t*3],-32768,32767),move=integer(x[3+t*3],0,PR_MOVE_COUNT-1);double source=x[4+t*3];if(source< -1||source>UINT32_MAX||source!=floor(source))bad();
   if(initial&&(mask&(1<<t)))p->tags[p->n_tags++]=(pr_battler_tag){.type=extra_tags[t],.turn_count=turns,.source_move=move,.source_id=(int64_t)source,.source_index=-1};
   const pr_battler_tag *tag=pr_get_battler_tag(p,extra_tags[t]);if(tag)actual|=1<<t;
   if(!initial&&((tag?tag->turn_count:0)!=turns||(tag?tag->source_move:0)!=move||(tag?tag->source_id:-1)!=(int64_t)source)){if(failures_seen<5)fprintf(stderr,"berry%d %s actor%d tag%d metadata differs\n",c,key,k,t);*ok=false;}
  }
  int toxic=integer(x[17],-1,127),sleep=integer(x[18],-1,127),freeze=integer(x[19],-1,127),has=integer(x[20],0,1);
  if(initial){p->status_toxic_turns=toxic;p->status_sleep_turns=sleep;p->status_freeze_turns=freeze;p->has_eaten_berry=has;}
  else if(actual!=mask||p->n_tags!=__builtin_popcount((unsigned)mask)||p->status_toxic_turns!=toxic||p->status_sleep_turns!=sleep||p->status_freeze_turns!=freeze||p->has_eaten_berry!=has){if(failures_seen<5)fprintf(stderr,"berry%d %s actor%d status/tag/consumption differs\n",c,key,k);*ok=false;}
  for(int h=0;h<3;h++){
   int start=21+h*9,count=integer(x[start],0,8);uint8_t *berries=h==0?p->battle_berries:h==1?p->turn_berries:p->last_berries;
   if(initial){if(h==0)p->n_battle_berries=count;else if(h==1)p->n_turn_berries=count;else p->n_last_berries=count;}
   int length=h==0?p->n_battle_berries:h==1?p->n_turn_berries:p->n_last_berries;if(length!=count)*ok=false;
   for(int j=0;j<8;j++){int berry=integer(x[start+1+j],0,10);if(j>=count&&berry)bad();if(initial)berries[j]=berry;else if(j<count&&berries[j]!=berry){if(failures_seen<5)fprintf(stderr,"berry%d %s actor%d history%d slot%d differs\n",c,key,k,h,j);*ok=false;}}
  }
  snprintf(prefix,sizeof prefix,"%s ",key);
 }
 (void)bt;
}
static void inventory(pr_battle *bt,const char *prefix,bool initial,bool *ok){
 for(int side=0;side<2;side++){
  double v[2];read_vector(prefix,v,2);if(v[0]!=side)bad();int n=integer(v[1],0,PR_MAX_MODIFIERS);if(initial)bt->n_mods[side]=n;else if(bt->n_mods[side]!=n)*ok=false;
  for(int j=0;j<n;j++){
   char key[80],extra;int owner,berry,stack,virtual,transfer;if(sscanf(line_read("M "),"%79s %d %d %d %d %d %c",key,&owner,&berry,&stack,&virtual,&transfer,&extra)!=6||owner< -1||owner>3||stack<1||stack>3||virtual<0||virtual>1||stack+virtual>3||transfer<0||transfer>1)bad();
   bool pouch=!strcmp(key,"BERRY_POUCH");if((pouch&&(owner!=-1||berry!=-1))||(!pouch&&(strcmp(key,"BERRY")||owner<0||berry<0||berry>10)))bad();
   pr_modifier m={.cls=pouch?PR_MOD_PRESERVE_BERRY:PR_MOD_BERRY,.pokemon_id=owner<0?-1:(int64_t)mons[owner].id,.stack_count=stack,.virtual_stack_count=virtual,.is_transferable=transfer};if(!pouch)m.berry_type=berry;
   if(initial)bt->mods[side][j]=m;else if(j<bt->n_mods[side]){const pr_modifier *got=&bt->mods[side][j];if(got->cls!=m.cls||got->pokemon_id!=m.pokemon_id||got->stack_count!=m.stack_count||got->virtual_stack_count!=m.virtual_stack_count||got->is_transferable!=m.is_transferable||got->berry_type!=m.berry_type){if(failures_seen<5)fprintf(stderr,"berry stock side%d index%d differs\n",side,j);*ok=false;}}
  }
 }
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
  if(sscanf(line,"Q %c %d %d %95s %d %d %lf %d %d %d %2047s %c",&scope,&group,&order,name,&b,&person,&value,&a,&d,&f,hex,&extra)!=11||(scope!='L'&&scope!='D')||group<0||group>=PR_MAX_LEVELS||order<0||order>=PR_MAX_PHASES||b< -1||b>3||person< -1||person>3||!isfinite(value)||value!=floor(value)||value< -1||value>INT32_MAX||a< -1||d< -1||d>7||f< -1||f>15)bad();
  bool known=false;const char *type=!strncmp(name,"Marker:",7)?name+7:name;for(int t=0;t<PH_COUNT_;t++)if(pr_phase_name[t]&&!strcmp(pr_phase_name[t],type))known=true;if(!known)bad();
  if(strcmp(hex,"-")){size_t n=strlen(hex);if(n%2)bad();for(size_t k=0;k<n;k++)if(!isxdigit((unsigned char)hex[k]))bad();}
  if(j>=nq||strcmp(line,queue[j])){if(failures_seen<5)fprintf(stderr,"case%d queue%d C:%sJS:%s",c,j,j<nq?queue[j]:"missing\n",line);*ok=false;}
 }
 (void)bt;
}
int main(int argc,char **argv){
 if(argc!=2||!getenv("TURNTEST_LOCALES"))return 2;input=fopen(argv[1],"r");if(!input)return 2;pr_closures_init();
 double h[3];read_vector("BERRYMOVES ",h,3);if(h[0]!=1&&h[0]!=2&&h[0]!=3)bad();int version=h[0];int n=integer(h[1],16384,100000),seed=integer(h[2],0,INT32_MAX),fail=0;int calls[4][2]={{0}},successes[4]={0};
 const int moves[]={MOVE_ID_STUFF_CHEEKS,MOVE_ID_TEATIME,MOVE_ID_BUG_BITE,MOVE_ID_PLUCK};int attrs[4]={-1,-1,-1,-1};for(int k=0;k<4;k++){const pr_move *m=&pr_moves[moves[k]];for(int j=0;j<m->nattr;j++)if(pr_attrs[m->attr0+j].cls==(k<2?PR_AC_EatBerryAttr:PR_AC_StealEatBerryAttr))attrs[k]=m->attr0+j;if(attrs[k]<0)bad();}
 for(int c=0;c<n;c++){
  static pr_battle bt;static pr_engine e;memset(&bt,0,sizeof bt);memset(&e,0,sizeof e);bt.engine=&e;e.current=-1;e.n_levels=1;bt.game_mode=GAME_MODES_CLASSIC;bt.wave_index=3;bt.turn=1;bt.ignoring_effect_source=-1;bt.last_player_involved=bt.last_enemy_involved=-1;
  char wave[64],battle[64];snprintf(wave,sizeof wave,"berry-moves-wave-%d",seed);snprintf(battle,sizeof battle,"berry-moves-%d-%d",seed,c);pr_seed_from_ascii(&bt.wave_seed,wave);pr_seed_from_ascii(&bt.battle_seed,battle);
  double x[5];read_vector("C ",x,5);if(x[0]!=c)bad();int mode=integer(x[1],0,3),call=integer(x[2],0,1),u=integer(x[3],0,3),t=integer(x[4],0,3);if(mode!=c%4)bad();bt.double_battle=true;bt.n_party[0]=bt.n_party[1]=2;for(int side=0;side<2;side++)for(int k=0;k<2;k++)bt.party[side][k]=&mons[side*2+k];fields(&bt);
  double g[2];read_vector("G ",g,2);int gas=integer(g[0],0,2);if(g[1]<0||g[1]>UINT32_MAX||floor(g[1])!=g[1])bad();if(gas){bt.n_arena_tags=1;bt.arena_tags[0]=(pr_arena_tag){.type=ARENA_TAG_TYPE_NEUTRALIZING_GAS,.side=ARENA_TAG_SIDE_BOTH,.source_count=gas,.source_id=(int64_t)g[1],.layers=1};}
  rng_read("R ",&bt.rng);rng_read("B ",&bt.battle_rng);bt.rng_live=bt.battle_rng_live=true;bool ok=true;frame(&bt,"I",c,true,&ok);
  inventory(&bt,"MI ",true,&ok);
  if(version>=3){char text[2048],extra;int select,condition,restricted;if(sscanf(line_read("CHECK "),"%d %d %d %2047s %c",&select,&condition,&restricted,text,&extra)!=4||select<0||select>1||condition<0||condition>1||restricted<0||restricted>1)bad();
   pr_msg why;bool allowed=pr_move_selectable(&bt,&mons[u],MOVE_ID_STUFF_CHEEKS,&why);char actual[2048]="-";if(why.key)message_hex(&why,actual,sizeof actual);const pr_move *stuff=&pr_moves[MOVE_ID_STUFF_CHEEKS];if(stuff->conditions_seq3.n!=1)bad();bool cond=pr_closure_call(&bt,stuff->conditions_seq3.v[0],PR_AC_EatBerryAttr,0,&mons[u],&mons[t],MOVE_ID_STUFF_CHEEKS);int fn=-1;for(int f=0;f<PR_CLOSURE_COUNT;f++)if(!strcmp(pr_closures_meta[f].owner,"move:747")&&!strcmp(pr_closures_meta[f].hash,"0de748961568"))fn=f;if(fn<0)bad();bool denied=pr_closure_call(&bt,fn,PR_AC_EatBerryAttr,0,&mons[u],&mons[t],MOVE_ID_STUFF_CHEEKS);
   if(allowed!=select||cond!=condition||denied!=restricted||strcmp(text,actual)){if(failures_seen<5)fprintf(stderr,"berry%d Stuff Cheeks query differs\n",c);ok=false;}
  }
  bool raw_result=false;calls[mode][call]++;n_callbacks=0;
  if(!call)raw_result=pr_apply_eat_berry_move(&bt,&e,&mons[u],&mons[t],mode==0,mode>=2);else{int ph=pr_phase_new(&e,PH_MOVE_EFFECT,0);e.pool[ph].move_id=moves[mode];pr_apply_move_effect(&bt,&e,ph,attrs[mode],&mons[u],&mons[t]);}
  double ret[1];read_vector("RESULT ",ret,1);int result=integer(ret[0],-1,1);if((call&&result!=-1)||(!call&&result<0))bad();if(!call&&raw_result!=result)ok=false;successes[mode]+=raw_result;
  if(version>=2){double count[1];read_vector("CALLS ",count,1);int n=integer(count[0],0,32);if(n!=n_callbacks)ok=false;for(int j=0;j<n;j++){double ab[2];read_vector("AB ",ab,2);int kind=integer(ab[0],0,3),who=integer(ab[1],0,3);if(j>=n_callbacks||kind!=callback_trace[j][0]||who!=callback_trace[j][1]){if(failures_seen<5)fprintf(stderr,"berry%d callback%d differs\n",c,j);ok=false;}}}
  frame(&bt,"O",c,false,&ok);inventory(&bt,"MO ",false,&ok);compare_queue(&bt,&e,c,&ok);
  /* This production controller drains the supplied queue to its TurnInit sentinel. */
  pr_run_player_cleanup(&bt,&e);
  frame(&bt,"Z",c,false,&ok);inventory(&bt,"MZ ",false,&ok);double world[3];read_vector("WORLD ",world,3);integer(world[0],0,1);integer(world[1],-1,1);integer(world[2],-1,1);if(world[0]!=bt.ability_bar_visible||world[1]!=bt.last_player_involved||world[2]!=bt.last_enemy_involved){if(failures_seen<5)fprintf(stderr,"case%d world C%d/%d/%d JS%g/%g/%g\n",c,bt.ability_bar_visible,bt.last_player_involved,bt.last_enemy_involved,world[0],world[1],world[2]);ok=false;}
  double texts[1];read_vector("TEXTS ",texts,1);int count=integer(texts[0],0,128),got=0;
  for(int j=0;j<e.n_events;j++)if(e.events[j].type==PR_EV_TEXT){if(got>=count){got++;ok=false;continue;}char actual[2048];message_hex(&e.events[j].msg,actual,sizeof actual);char *expected=line_read("T ");size_t l=strlen(expected);if(!l||expected[l-1]!='\n')bad();expected[l-1]=0;if(strcmp(actual,expected)){if(failures_seen<5)fprintf(stderr,"case%d text%d C:%s JS:%s\n",c,got,actual,expected);ok=false;}got++;}
  if(got!=count){if(failures_seen<5)fprintf(stderr,"case%d texts C%d JS%d\n",c,got,count);ok=false;if(got<count)for(int j=got;j<count;j++)line_read("T ");}
  pr_rng want;rng_read("RO ",&want);ok&=rng_equal(want,bt.rng);rng_read("BO ",&want);ok&=rng_equal(want,bt.battle_rng);
  if(strcmp(line_read("E"),"\n"))bad();if(bt.n_unported||bt.n_deferred||e.overflow){if(failures_seen<5)fprintf(stderr,"case%d unsupported%d(%s) deferred%d overflow%d\n",c,bt.n_unported,bt.n_unported?pr_unported_name(bt.unported[0]):"none",bt.n_deferred,e.overflow);ok=false;}
  if(!ok){if(fail<15)fprintf(stderr,"berry case%d mode%d user%d target%d failed\n",c,mode,u,t);fail++;failures_seen=fail;}
 }
 double end[1];read_vector("BERRYMOVES_COMPLETE ",end,1);if(end[0]!=n||fgetc(input)!=EOF)bad();if(!fail)for(int m=0;m<4;m++){if(!successes[m])bad();for(int c=0;c<2;c++)if(calls[m][c]<2048)bad();}printf("Berry moves: %d complete, %d exact, %d failures\n",n,n-fail,fail);return fail?1:0;
}
