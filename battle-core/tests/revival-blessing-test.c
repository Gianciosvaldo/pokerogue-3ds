/* Exact native raw callback/choice state and complete pending phase payloads. */
#define main gas_replay_main
#include "wide-substitute-frame.inc"
#undef main
static pr_pokemon mon[8];
static pr_battle bt;static pr_engine eng;
typedef struct { char kind;int v[7];char text[2048]; } wire;
static int calls,ranges[16],mins[16];
static bool callback_result;static int callback_calls;
bool __real_pr_revival_blessing(pr_battle *,pr_engine *,pr_pokemon *);
bool __wrap_pr_revival_blessing(pr_battle *b,pr_engine *e,pr_pokemon *u)
{
 callback_calls++;callback_result=__real_pr_revival_blessing(b,e,u);return callback_result;
}
int __real_pr_battle_rand_seed_int(pr_battle *,int,int);
int __wrap_pr_battle_rand_seed_int(pr_battle *b,int range,int min){if(calls<16){ranges[calls]=range;mins[calls]=min;}calls++;return __real_pr_battle_rand_seed_int(b,range,min);}
static int actor(const pr_pokemon *p){for(int i=0;i<8;i++)if(p==&mon[i])return i;return -1;}
static void validate_wire(wire w)
{
 int used=0;
 if(w.kind=='K'){if(w.v[0]<0||w.v[0]>1)bad();used=1;}
 else if(w.kind=='T'){if(w.v[0]<0||w.v[0]>1)bad();used=1;}
 else if(w.kind=='V'){if(w.v[0]<0||w.v[0]>7)bad();used=1;}
 else if(w.kind=='R'){if(w.v[0]<0||w.v[0]>7||w.v[1]<0||w.v[1]>1||w.v[2]<0||w.v[2]>1)bad();used=3;}
 else if(w.kind=='S'){if(w.v[0]<0||w.v[0]>4||w.v[1]<0||w.v[1]>1||w.v[2]<0||w.v[2]>5||w.v[3]<0||w.v[3]>1||w.v[4]<0||w.v[4]>1)bad();used=5;}
 else if(w.kind=='M'){
  for(int j=0;j<6;j++)if(w.v[j]!=-1)bad();if(w.v[6]<0||w.v[6]>1)bad();
  if(strcmp(w.text,"-")&&(strlen(w.text)%2||strspn(w.text,"0123456789abcdef")!=strlen(w.text)))bad();return;
 } else if(w.kind=='P'){
  if(w.v[0]<0||w.v[0]>3||w.v[1]<=0||w.v[1]>=PR_MOVE_COUNT||w.v[2]<1||w.v[2]>6||(w.v[3]<0||w.v[3]>4)&&w.v[3]!=255||w.v[4]<0||w.v[4]>2||w.v[5]<0||w.v[5]>1||w.v[6]!=1)bad();
  int target;char ex;if(sscanf(w.text,"%d%c",&target,&ex)!=1||target<0||target>3)bad();return;
 }else bad();
 for(int j=used;j<7;j++)if(w.v[j]!=-1)bad();if(strcmp(w.text,"-"))bad();
}
static wire read_wire(const char *prefix,int expected,int level)
{
 wire w;char extra;int a,b;char *s=line_read(prefix);int used=0;
 if(level>=0){if(sscanf(s,"%d %d %n",&a,&b,&used)!=2||a!=level||b!=expected)bad();}
 else if(sscanf(s,"%d %n",&a,&used)!=1||a!=expected)bad();
 if(sscanf(s+used," %c %d %d %d %d %d %d %d %2047s %c",&w.kind,&w.v[0],&w.v[1],&w.v[2],&w.v[3],&w.v[4],&w.v[5],&w.v[6],w.text,&extra)!=9)bad();
 for(int i=0;i<7;i++)if(w.v[i]<-1||w.v[i]>65535)bad();validate_wire(w);return w;
}
static int make_wire(wire w)
{
 int ph=-1;
 if(w.kind=='K'){if(w.v[0]<0||w.v[0]>1)bad();ph=pr_phase_new(&eng,PH_MARKER,-1);eng.pool[ph].marker_of=w.v[0]?PH_SWITCH_SUMMON:PH_MOVE;}
 else if(w.kind=='S'){
  if(w.v[0]<0||w.v[0]>4||w.v[1]<0||w.v[1]>1||w.v[2]<0||w.v[2]>5||w.v[3]<0||w.v[3]>1||w.v[4]<0||w.v[4]>1)bad();
  ph=pr_phase_new(&eng,PH_SWITCH_SUMMON,-1);pr_phase *p=&eng.pool[ph];p->sw_type=w.v[0];p->sw_field_index=w.v[1];p->sw_slot=w.v[2];p->sw_do_return=w.v[3];p->sw_player=w.v[4];
 } else if(w.kind=='P'){
  if(w.v[0]<0||w.v[0]>3||w.v[1]<=0||w.v[1]>=PR_MOVE_COUNT||w.v[2]<1||w.v[2]>6||w.v[3]<0||w.v[3]>255||w.v[4]<0||w.v[4]>4||w.v[5]<0||w.v[5]>1||w.v[6]!=1)bad();
  int target;char extra;if(sscanf(w.text,"%d%c",&target,&extra)!=1||target<0||target>3)bad();
  ph=pr_phase_new(&eng,PH_MOVE,w.v[0]);pr_phase *p=&eng.pool[ph];p->pokemon=&mon[w.v[0]];p->move.id=w.v[1];p->move.slot=-1;p->entry.move=w.v[1];p->entry.use_mode=w.v[2];p->entry.result=w.v[3];p->timing=w.v[4];p->cancelled=w.v[5];p->entry.n_targets=1;p->entry.targets[0]=target;
  int ti=eng.n_target_lists++;if(ti>=PR_MAX_TARGET_LISTS)bad();eng.target_lists[ti].n=1;eng.target_lists[ti].v[0]=target;p->targets=ti;
 } else bad();if(ph<0)bad();return ph;
}
static void text_hex(const pr_msg *m,char *out)
{
 char s[1024];pr_text_source src={locale_get,NULL,"en"};size_t n=pr_msg_format(m,&src,s,sizeof s);if(n>=sizeof s)bad();if(!n){strcpy(out,"-");return;}for(size_t i=0;i<n;i++)sprintf(out+2*i,"%02x",(unsigned char)s[i]);
}
static bool same_wire(const pr_phase *p,wire w)
{
 int v[7]={-1,-1,-1,-1,-1,-1,-1};char kind='?',text[2048]="-";
 switch(p->type){
 case PH_MARKER:kind='K';v[0]=p->marker_of==PH_MOVE?0:p->marker_of==PH_SWITCH_SUMMON?1:-1;break;
 case PH_MESSAGE:kind='M';v[6]=p->message_prompt;text_hex(&p->msg,text);break;
 case PH_RESET_STATUS:kind='R';v[0]=actor(p->pokemon);v[1]=p->affect_confusion;v[2]=0;break;
 case PH_REVIVAL_BLESSING:kind='V';v[0]=actor(p->pokemon);break;
 case PH_TOGGLE_DOUBLE_POSITION:kind='T';v[0]=p->position_double;break;
 case PH_SWITCH_SUMMON:kind='S';v[0]=p->sw_type;v[1]=p->sw_field_index;v[2]=p->sw_slot;v[3]=p->sw_do_return;v[4]=p->sw_player;break;
 case PH_MOVE:kind='P';v[0]=actor(p->pokemon);v[1]=p->move.id;v[2]=p->entry.use_mode;v[3]=p->entry.result;v[4]=p->timing;v[5]=p->cancelled;v[6]=p->entry.n_targets;if(v[6]==1)snprintf(text,sizeof text,"%d",p->entry.targets[0]);break;
 default:break;
 }
 bool ok=kind==w.kind&&!strcmp(text,w.text);for(int i=0;i<7;i++)ok&=v[i]==w.v[i];return ok;
}
static bool queues(bool set)
{
 double v[4];read_vector(set?"IQ ":"OQ ",v,4);int n=integer(v[0],0,32),levels=integer(v[1],1,PR_MAX_LEVELS),deferred=integer(v[2],0,1),current=integer(v[3],0,PR_MAX_LEVELS-1);if(current>=levels)bad();bool ok=true;
 if(set){eng.n_levels=levels;eng.deferred_active=deferred;eng.current_level=current;}
 else ok&=eng.n_levels==levels&&eng.deferred_active==deferred&&eng.current_level==current;
 if(set){int last=-1,order=0;for(int j=0;j<n;j++){
  char *row=line_read("IP "),extra;int l,k,used;if(sscanf(row,"%d %d %n",&l,&k,&used)!=2||l<0||l>=levels||k<0||k>=PR_MAX_PHASES)bad();
  wire w;if(sscanf(row+used," %c %d %d %d %d %d %d %d %2047s %c",&w.kind,&w.v[0],&w.v[1],&w.v[2],&w.v[3],&w.v[4],&w.v[5],&w.v[6],w.text,&extra)!=9)bad();if(l<last)bad();if(l!=last){last=l;order=0;}if(k!=order++)bad();validate_wire(w);int ph=make_wire(w);eng.levels[l][eng.level_len[l]++]=ph;
 }}else{
  int seen=0;for(int l=0;l<eng.n_levels;l++)for(int k=0;k<eng.level_len[l];k++){if(seen++>=n){ok=false;continue;}wire w=read_wire("OP ",k,l);ok&=same_wire(&eng.pool[eng.levels[l][k]],w);}while(seen<n){line_read("OP ");seen++;ok=false;}
 }
 double dc[1];read_vector(set?"IDC ":"ODC ",dc,1);int nd=integer(dc[0],0,16),at[PH_COUNT_]={0};
 for(int j=0;j<nd;j++){wire w=read_wire(set?"ID ":"OD ",j,-1);int type=w.kind=='P'?PH_MOVE:w.kind=='S'?PH_SWITCH_SUMMON:-1;if(type<0)bad();int k=at[type]++;
  if(set){int ph=make_wire(w);if(k>=PR_DYN_CAP)bad();eng.dyn[type][eng.n_dyn[type]++]=ph;}
  else if(k>=eng.n_dyn[type])ok=false;else ok&=same_wire(&eng.pool[eng.dyn[type][k]],w);
 }
 for(int t=0;t<PH_COUNT_;t++)if(!set)ok&=eng.n_dyn[t]==at[t];return ok;
}
static bool details(pr_pokemon *p,int k,bool set)
{
 double s[5],t[7],b[5];read_vector(set?"SI ":"SO ",s,5);read_vector(set?"TI ":"TO ",t,7);read_vector(set?"IB ":"OB ",b,5);if(s[0]!=k||t[0]!=k||b[0]!=k)bad();
 for(int j=1;j<5;j++)integer(s[j],j==1?0:-1,j==1?7:INT16_MAX);for(int j=1;j<7;j++)integer(t[j],0,j==5?1:INT32_MAX);for(int j=1;j<5;j++)integer(b[j],0,j==1||j==4?1:j==2?8:7);
 if(set){p->status=s[1];p->status_toxic_turns=s[2];p->status_sleep_turns=s[3];p->status_freeze_turns=s[4];p->damage_taken=t[1];p->hit_count=t[2];p->pending_status=t[3];p->combining_pledge=t[4];p->acted=t[5];p->n_form_change_abilities=t[6];if(t[6]!=1)bad();p->form_change_abilities[0]=ABILITY_ID_BALL_FETCH;p->is_boss=b[1];p->boss_segments=b[2];p->boss_segment_index=b[3];p->has_trainer=b[4];p->trainer_slot=p->has_trainer?1:0;}
 return p->status==s[1]&&(p->status?p->status_toxic_turns:0)==s[2]&&(p->status?p->status_sleep_turns:-1)==s[3]&&(p->status?p->status_freeze_turns:-1)==s[4]&&p->damage_taken==t[1]&&p->hit_count==t[2]&&p->pending_status==t[3]&&p->combining_pledge==t[4]&&p->acted==t[5]&&p->n_form_change_abilities==t[6]&&p->is_boss==b[1]&&p->boss_segments==b[2]&&p->boss_segment_index==b[3]&&p->has_trainer==b[4];
}
int main(int argc,char **argv)
{
 if(argc!=2||!getenv("TURNTEST_LOCALES")||!(input=fopen(argv[1],"r")))return 2;pr_closures_init();double h[3];read_vector("REVIVAL ",h,3);integer(h[0],1,4);int n=integer(h[1],h[0]==4?8704:h[0]==3?8448:8192,100000);integer(h[2],0,INT32_MAX);int failures=0,enemy=0,players=0,choices=0,denied=0;
 for(int i=0;i<n;i++){
  memset(&bt,0,sizeof bt);memset(&eng,0,sizeof eng);bt.engine=&eng;bt.double_battle=true;bt.game_mode=GAME_MODES_ENDLESS;bt.wave_index=201;bt.turn=1;bt.ignoring_effect_source=-1;eng.n_levels=1;eng.current=-1;
  double c[4];read_vector("C ",c,4);if(c[0]!=i)bad();int mode=integer(c[1],0,1),u=integer(c[2],0,3),selected=integer(c[3],-1,5);if(mode&&u>1)bad();bool ok=true;
  char seed[512],extra;if(sscanf(line_read("W "),"%511s %c",seed,&extra)!=1)bad();pr_seed_from_ascii(&bt.wave_seed,seed);rng_read("R ",&bt.rng);rng_read("B ",&bt.battle_rng);bt.rng_live=bt.battle_rng_live=true;
  for(int k=0;k<8;k++){double v[WIDTH+1];read_vector("I ",v,WIDTH+1);if(v[0]!=k)bad();int b=k<4?k:k<6?0:2;load(&mon[k],v+1,b);ok&=compare(&mon[k],v+1,i,b,'I');ok&=details(&mon[k],k,true);}
  int slots[2][4]={{0,1,4,5},{2,3,6,7}};for(int side=0;side<2;side++){bt.n_party[side]=4;for(int k=0;k<4;k++)bt.party[side][k]=&mon[slots[side][k]];}for(int k=0;k<4;k++)bt.field[k]=&mon[k];
  ok&=queues(true);int cur=pr_phase_new(&eng,mode?PH_REVIVAL_BLESSING:PH_MOVE_EFFECT,mode?-1:u);eng.current=cur;eng.pool[cur].pokemon=&mon[u];eng.pool[cur].move_id=MOVE_ID_REVIVAL_BLESSING;
  double cond[1];read_vector("COND ",cond,1);ok&=integer(cond[0],0,1)==pr_move_apply_conditions(&bt,MOVE_ID_REVIVAL_BLESSING,&mon[u],&mon[u],4);
  double ui[5];read_vector("UI ",ui,5);if(mode){ok&=integer(ui[0],0,1)==u;for(int k=0;k<4;k++)ok&=integer(ui[k+1],0,1)==(bt.party[0][k]->hp<=0);}else{if(ui[0]!=-9)bad();for(int k=1;k<5;k++)if(ui[k])bad();}
  calls=0;callback_calls=0;callback_result=false;bool result=false;
  if(mode){pr_phase_run(&bt,&eng,cur);ok&=eng.awaiting_pick;pr_finish_revival_choice(&bt,&eng,&mon[u],selected);choices++;}
  else {const pr_move *mv=&pr_moves[MOVE_ID_REVIVAL_BLESSING];int attr=-1;for(int k=0;k<mv->nattr;k++)if(pr_attrs[mv->attr0+k].cls==PR_AC_RevivalBlessingAttr)attr=mv->attr0+k;if(attr<0)bad();pr_apply_move_effect(&bt,&eng,cur,attr,&mon[u],&mon[u]);result=callback_result;ok&=callback_calls<=1;}
  double ret[1];read_vector("RET ",ret,1);if(mode){if(ret[0]!=-1)bad();}else{ok&=integer(ret[0],0,1)==result;enemy+=u>=2&&result;players+=u<2&&result;denied+=!result;}
  if(h[0]>=2){double nc[1];read_vector("CALLS ",nc,1);int want=integer(nc[0],0,16);ok&=want==calls;for(int j=0;j<want;j++){double call[3];read_vector("CALL ",call,3);ok&=integer(call[0],0,7)==u;int range=integer(call[1],1,6),min=integer(call[2],0,0);if(j<calls)ok&=ranges[j]==range&&mins[j]==min;}}
  for(int k=0;k<8;k++){double v[WIDTH+1];read_vector("O ",v,WIDTH+1);if(v[0]!=k)bad();int b=k<4?k:k<6?0:2;ok&=compare(&mon[k],v+1,i,b,'O');ok&=details(&mon[k],k,false);}
  ok&=queues(false);pr_rng want;rng_read("RO ",&want);ok&=rng_equal(want,bt.rng);rng_read("BO ",&want);ok&=rng_equal(want,bt.battle_rng);if(strcmp(line_read("E"),"\n"))bad();if(bt.n_unported||bt.n_deferred||eng.overflow)ok=false;
  if(!ok){if(failures<20)fprintf(stderr,"revival case%d mode%d user%d pick%d unported%d failed\n",i,mode,u,selected,bt.n_unported);failures++;}
 }
 double foot[1];read_vector("REVIVAL_COMPLETE ",foot,1);if(foot[0]!=n||fgetc(input)!=EOF)bad();if(!failures&&(enemy<100||players<100||denied<100||choices<100))bad();printf("Revival: %d complete, %d exact, %d failures; enemy=%d player=%d denied=%d choices=%d\n",n,n-failures,failures,enemy,players,denied,choices);return failures?1:0;
}
