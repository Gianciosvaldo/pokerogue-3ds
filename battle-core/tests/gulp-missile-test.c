/* Native registered callback/tag APIs; input cloning and exact state/queue transport only. */
#define main gas_replay_main
#include "wide-substitute-frame.inc"
#undef main
static pr_battle bt;static pr_engine e;static pr_pokemon mon[8];
static int testcase,version;static bool diagnose;static const char *outprefix="O";
static bool callback_result;static int callback_calls;
bool __real_pr_gulp_missile_tag(pr_battle *,pr_engine *,pr_pokemon *,int);
bool __wrap_pr_gulp_missile_tag(pr_battle *b,pr_engine *q,pr_pokemon *p,int move)
{callback_calls++;callback_result=__real_pr_gulp_missile_tag(b,q,p,move);return callback_result;}
static int actor(const pr_pokemon *p){for(int k=0;k<8;k++)if(p==&mon[k])return k;return -1;}
static int tag_type(const char *s)
{
 if(!strcmp(s,"GULP_MISSILE_ARROKUDA"))return BATTLER_TAG_TYPE_GULP_MISSILE_ARROKUDA;
 if(!strcmp(s,"GULP_MISSILE_PIKACHU"))return BATTLER_TAG_TYPE_GULP_MISSILE_PIKACHU;
 if(!strcmp(s,"UNDERWATER"))return BATTLER_TAG_TYPE_UNDERWATER;
 if(!strcmp(s,"SUBSTITUTE"))return BATTLER_TAG_TYPE_SUBSTITUTE;
 bad();return -1;
}
static bool extras(int k,bool set)
{
 pr_pokemon *p=&mon[k];double s[4],x[12];char ps[8],px[8],pt[8],pg[8];snprintf(ps,sizeof ps,"%sS ",set?"I":outprefix);snprintf(px,sizeof px,"%sX ",set?"I":outprefix);snprintf(pt,sizeof pt,"%sT ",set?"I":outprefix);snprintf(pg,sizeof pg,"%sG ",set?"I":outprefix);read_vector(ps,s,4);read_vector(px,x,12);if(s[0]!=k||x[0]!=k)bad();
 for(int j=1;j<4;j++)integer(s[j],j==1?0:-1,INT16_MAX);for(int j=1;j<12;j++)integer(x[j],0,j==5||j==8||j==11?1:j==9?8:INT32_MAX);if(x[6]||x[7])bad();
 if(set){p->status_toxic_turns=s[1];p->status_sleep_turns=s[2];p->status_freeze_turns=s[3];p->damage_taken=x[1];p->hit_count=x[2];p->pending_status=x[3];p->combining_pledge=x[4];p->acted=x[5];p->n_form_change_abilities=0;p->n_attacks_received=0;p->is_boss=x[8];p->boss_segments=x[9];p->boss_segment_index=x[10];p->has_trainer=x[11];p->trainer_slot=p->has_trainer?1:0;}
 bool ok=(p->status?p->status_toxic_turns:0)==s[1]&&(p->status?p->status_sleep_turns:-1)==s[2]&&(p->status?p->status_freeze_turns:-1)==s[3]&&p->damage_taken==x[1]&&p->hit_count==x[2]&&p->pending_status==x[3]&&p->combining_pledge==x[4]&&p->acted==x[5]&&p->n_form_change_abilities==x[6]&&p->n_attacks_received==x[7]&&p->is_boss==x[8]&&p->boss_segments==x[9]&&p->boss_segment_index==x[10]&&p->has_trainer==x[11];
 double h[2];read_vector(pt,h,2);if(h[0]!=k)bad();int n=integer(h[1],0,PR_MAX_BATTLER_TAGS);if(set)p->n_tags=n;else ok&=p->n_tags==n;
 for(int j=0;j<n;j++){
  char type[64],extra;int a,b,turn,move,focus;long long source;double hp;
  if(sscanf(line_read(pg),"%d %d %63s %d %d %lld %lf %d %c",&a,&b,type,&turn,&move,&source,&hp,&focus,&extra)!=8||a!=k||b!=j||turn<0||turn>INT16_MAX||move<0||move>=PR_MOVE_COUNT||source< -1||source>UINT32_MAX||!isfinite(hp)||hp<0||hp>INT32_MAX||focus<0||focus>1)bad();
  int t=tag_type(type);if(set){p->tags[j]=(pr_battler_tag){.type=t,.turn_count=turn,.source_move=move,.source_id=source,.hp=hp,.in_focus=focus,.source_index=-1};}
  if(j>=p->n_tags){ok=false;continue;}const pr_battler_tag *v=&p->tags[j];ok&=v->type==t&&v->turn_count==turn&&v->source_move==move&&v->source_id==source&&v->hp==hp&&v->in_focus==focus;
 }
 if(!ok&&diagnose)fprintf(stderr,"case%d extras actor%d set%d mismatch tagsC%d JS%d\n",testcase,k,set,p->n_tags,n);
 return ok;
}
static void text_hex(const pr_msg *m,char *out)
{
 char s[1024];pr_text_source src={locale_get,NULL,"en"};size_t n=pr_msg_format(m,&src,s,sizeof s);if(n>=sizeof s)bad();if(!n){strcpy(out,"-");return;}for(size_t j=0;j<n;j++)sprintf(out+2*j,"%02x",(unsigned char)s[j]);
}
static bool phase_row(const char *prefix,int level,int order,const pr_phase *p)
{
 char name[64],text[2048],extra;int l,j,b,who;double v[7];
 if(sscanf(line_read(prefix),"%d %d %63s %d %d %lf %lf %lf %lf %lf %lf %lf %2047s %c",&l,&j,name,&b,&who,&v[0],&v[1],&v[2],&v[3],&v[4],&v[5],&v[6],text,&extra)!=13||l<0||l>=PR_MAX_LEVELS||j<0||j>=PR_MAX_PHASES||b< -1||b>3||who< -1||who>7)bad();
 for(int z=0;z<7;z++)if(!isfinite(v[z])||v[z]<-1||v[z]>INT32_MAX||v[z]!=floor(v[z]))bad();
 if(strcmp(text,"-")&&(strlen(text)%2||strspn(text,"0123456789abcdef")!=strlen(text)))bad();
 int used=0;
 if(!strcmp(name,"DynamicPhaseMarker")){used=1;integer(v[0],0,1);}
 else if(!strcmp(name,"QuietFormChangePhase")){used=1;integer(v[0],0,2);if(who<0)bad();}
 else if(!strcmp(name,"MessagePhase")||!strcmp(name,"HideAbilityPhase")){if(who!=-1||b!=-1)bad();if(version>=3&&!strcmp(name,"MessagePhase")){used=1;integer(v[0],0,1);}}
 else if(!strcmp(name,"ShowAbilityPhase")){used=3;integer(v[0],0,1);integer(v[1],-1,PR_ABILITY_COUNT-1);integer(v[2],0,1);}
 else if(!strcmp(name,"DamageAnimPhase")){used=3;integer(v[0],0,INT32_MAX);if(v[1]!=HIT_RESULT_INDIRECT||v[2]!=0)bad();}
 else if(!strcmp(name,"FaintPhase")){used=1;integer(v[0],0,1);}
 else if(!strcmp(name,"StatStageChangePhase")){used=7;integer(v[0],STAT_ATK,STAT_EVA);integer(v[1],-1,-1);for(int z=2;z<6;z++)integer(v[z],0,1);integer(v[6],0,16);}
 else if(!strcmp(name,"ObtainStatusEffectPhase")){used=2;if(v[0]!=STATUS_EFFECT_PARALYSIS||v[1]!=-1)bad();}
 else bad();
 for(int z=used;z<7;z++)if(v[z]!=-1)bad();if(strcmp(name,"MessagePhase")&&strcmp(name,"ObtainStatusEffectPhase")&&strcmp(text,"-"))bad();
 if(!p)return false;
 double c[7]={-1,-1,-1,-1,-1,-1,-1};int cw=-1;char ct[2048]="-";const char *cn=p->type==PH_MARKER?"DynamicPhaseMarker":pr_phase_name[p->type];
 switch(p->type){
 case PH_MARKER:c[0]=p->marker_of==PH_STAT_STAGE_CHANGE?0:p->marker_of==PH_OBTAIN_STATUS_EFFECT?1:-1;break;
 case PH_QUIET_FORM_CHANGE:cw=actor(p->pokemon);c[0]=p->move_id;break;
 case PH_MESSAGE:if(version>=3)c[0]=p->message_prompt;text_hex(&p->msg,ct);break;
 case PH_SHOW_ABILITY:c[0]=p->anim_result;c[1]=p->anim==UINT16_MAX?-1:p->anim;c[2]=p->battler>=0&&bt.field[p->battler]?bt.field[p->battler]->on_field:0;break;
 case PH_HIDE_ABILITY:break;
 case PH_DAMAGE_ANIM:c[0]=p->anim_amount;c[1]=p->anim_result;c[2]=p->anim_critical;break;
 case PH_FAINT:cw=actor(p->source_p);c[0]=p->prevent_instant_revive;break;
 case PH_STAT_STAGE_CHANGE:cw=actor(p->source_p);if(p->n_ssc!=1)return false;c[0]=p->ssc_stat[0];c[1]=p->ssc_stages[0];c[2]=p->ssc_ignore_abilities;c[3]=p->ssc_message;c[4]=p->ssc_processed;c[5]=p->ssc_self_target;c[6]=p->ssc_source_type;break;
 case PH_OBTAIN_STATUS_EFFECT:cw=actor(p->source_p);c[0]=p->ose_effect;c[1]=p->ose_sleep_turns;text_hex(&p->msg,ct);break;
 default:return false;
 }
 /* Native QuietFormChangePhase is a BattlePhase carrying pokemon, not a
  * PokemonPhase with a battlerIndex. The C field is internal event metadata;
  * compare the actual constructor reference and target above. */
 int cb=p->type==PH_QUIET_FORM_CHANGE?-1:p->battler;
 bool ok=l==level&&j==order&&!strcmp(name,cn)&&b==cb&&who==cw&&!strcmp(text,ct);for(int z=0;z<7;z++)ok&=v[z]==c[z];if(!ok&&diagnose){fprintf(stderr,"case%d phase C %s b%d who%d / JS %s b%d who%d\n",testcase,cn,p->battler,cw,name,b,who);for(int z=0;z<7;z++)fprintf(stderr," %d %.17g/%.17g",z,c[z],v[z]);fprintf(stderr," lv/order %d/%d vs %d/%d text %s/%s\n",level,order,l,j,ct,text);}return ok;
}
static bool queue_rows(void)
{
 double q[4];read_vector("Q ",q,4);int n=integer(q[0],0,64),levels=integer(q[1],1,PR_MAX_LEVELS),defer=integer(q[2],0,1),current=integer(q[3],0,PR_MAX_LEVELS-1);if(current>=levels)bad();bool ok=e.n_levels==levels&&e.deferred_active==defer&&e.current_level==current;int seen=0;if(!ok&&diagnose)fprintf(stderr,"case%d Qcontext C%d/%d/%d JS%d/%d/%d n%d\n",testcase,e.n_levels,e.deferred_active,e.current_level,levels,defer,current,n);
 for(int l=0;l<e.n_levels;l++)for(int j=0;j<e.level_len[l];j++){if(seen++>=n){ok=false;continue;}ok&=phase_row("P ",l,j,&e.pool[e.levels[l][j]]);}while(seen<n){phase_row("P ",0,seen,NULL);seen++;ok=false;}
 double dc[1];read_vector("DQ ",dc,1);n=integer(dc[0],0,32);seen=0;int types[]={PH_OBTAIN_STATUS_EFFECT,PH_STAT_STAGE_CHANGE};
 for(int k=0;k<2;k++)for(int j=0;j<e.n_dyn[types[k]];j++){if(seen>=n){ok=false;seen++;continue;}ok&=phase_row("D ",0,seen,&e.pool[e.dyn[types[k]][j]]);seen++;}
 while(seen<n){phase_row("D ",0,seen,NULL);seen++;ok=false;}
 for(int t=0;t<PH_COUNT_;t++)if(t!=types[0]&&t!=types[1]&&e.n_dyn[t])ok=false;return ok;
}
int main(int argc,char **argv)
{
 if(argc!=2||!getenv("TURNTEST_LOCALES")||!(input=fopen(argv[1],"r")))return 2;pr_closures_init();double h[3];read_vector("GULP ",h,3);version=integer(h[0],2,3);int n=integer(h[1],version==3?16768:16384,100000),failed=0;integer(h[2],0,INT32_MAX);
 for(int i=0;i<n;i++){
  testcase=i;diagnose=failed==0;outprefix="O";memset(&bt,0,sizeof bt);memset(&e,0,sizeof e);bt.engine=&e;e.n_levels=1;e.current=-1;bt.game_mode=GAME_MODES_ENDLESS;bt.wave_index=201;bt.turn=1;bt.double_battle=true;bt.ignoring_effect_source=-1;bt.last_used_pokeball=-1;
  int id,mode,u,attack,move,lt,fx;char tag[64],extra;if(sscanf(line_read("C "),"%d %d %d %d %d %63s %d %d %c",&id,&mode,&u,&attack,&move,tag,&lt,&fx,&extra)!=8||id!=i||mode<0||mode>(version>=3?5:4)||u<0||u>3||attack<0||attack>7||move<=0||move>=PR_MOVE_COUNT||lt<0||lt>16||fx<0||fx>3)bad();int type=tag_type(tag);if(type!=BATTLER_TAG_TYPE_GULP_MISSILE_ARROKUDA&&type!=BATTLER_TAG_TYPE_GULP_MISSILE_PIKACHU)bad();
  char seed[512];if(sscanf(line_read("W "),"%511s %c",seed,&extra)!=1)bad();pr_seed_from_ascii(&bt.wave_seed,seed);rng_read("R ",&bt.rng);rng_read("B ",&bt.battle_rng);bt.rng_live=bt.battle_rng_live=true;bool ok=true;
  for(int k=0;k<8;k++){double v[WIDTH+1];read_vector("I ",v,WIDTH+1);if(v[0]!=k)bad();load(&mon[k],v+1,k<4?k:k<6?0:2);ok&=extras(k,true);ok&=compare(&mon[k],v+1,i,k,'I');}
  int slots[2][4]={{0,1,4,5},{2,3,6,7}};for(int side=0;side<2;side++){bt.n_party[side]=4;for(int k=0;k<4;k++)bt.party[side][k]=&mon[slots[side][k]];}for(int k=0;k<4;k++)bt.field[k]=&mon[k];
  double iq[4];read_vector("IQ ",iq,4);if(iq[0])bad();e.n_levels=integer(iq[1],1,PR_MAX_LEVELS);e.deferred_active=integer(iq[2],0,1);e.current_level=integer(iq[3],0,e.n_levels-1);
  int cur=pr_phase_new(&e,fx==3?PH_TURN_END:PH_MOVE_EFFECT,attack<4?attack:-1);e.current=cur;e.pool[cur].move_id=move;e.pool[cur].user_id=fx==1?(int64_t)mon[attack].id:fx==2?4294967294LL:-1;
  callback_result=false;callback_calls=0;int ret=-1;
  if(mode==0){const pr_move *mv=&pr_moves[MOVE_ID_SURF];int attr=-1;for(int k=0;k<mv->nattr;k++)if(pr_attrs[mv->attr0+k].cls==PR_AC_GulpMissileTagAttr)attr=mv->attr0+k;if(attr<0)bad();pr_apply_move_effect(&bt,&e,cur,attr,&mon[u],&mon[u^2]);ret=callback_result;ok&=callback_calls==1;}
  else if(mode==1)ret=pr_add_tag(&bt,&e,&mon[u],type,0,move,-1);else if(mode==2||mode==5)ret=pr_lapse_tag(&bt,&e,&mon[u],type,lt);else if(mode==3)pr_remove_tag(&bt,&e,&mon[u],type);else pr_form_change(&bt,&mon[u],(i/80)%2?PR_FCT_SpeciesFormChangeAbilityTrigger:PR_FCT_SpeciesFormChangeActiveTrigger);
  double want[1];read_vector("RET ",want,1);integer(want[0],-1,1);if(want[0]!=ret&&diagnose)fprintf(stderr,"case%d RET %d/%.0f calls%d\n",i,ret,want[0],callback_calls);ok&=want[0]==ret;
  for(int k=0;k<8;k++){double v[WIDTH+1];read_vector("O ",v,WIDTH+1);if(v[0]!=k)bad();ok&=compare(&mon[k],v+1,i,k,'O');ok&=extras(k,false);}ok&=queue_rows();pr_rng rng;rng_read("RO ",&rng);if(!rng_equal(rng,bt.rng)&&diagnose)fprintf(stderr,"case%d globalRNG diff\n",i);ok&=rng_equal(rng,bt.rng);rng_read("BO ",&rng);if(!rng_equal(rng,bt.battle_rng)&&diagnose)fprintf(stderr,"case%d battleRNG diff\n",i);ok&=rng_equal(rng,bt.battle_rng);
  if(mode==5){
   double sw[2];read_vector("SWAP ",sw,2);int a=integer(sw[0],0,3),b=integer(sw[1],4,7);if(a!=attack||b!=(a<2?4:6))bad();int side=a<2?0:1,pa=-1,pb=-1;for(int j=0;j<4;j++){if(bt.party[side][j]==&mon[a])pa=j;if(bt.party[side][j]==&mon[b])pb=j;}if(pa<0||pb<0)bad();bt.party[side][pa]=&mon[b];bt.party[side][pb]=&mon[a];bt.field[a]=&mon[b];mon[a].on_field=false;mon[b].on_field=true;
   if(e.n_dyn[PH_OBTAIN_STATUS_EFFECT]!=1)bad();pr_phase_run(&bt,&e,e.dyn[PH_OBTAIN_STATUS_EFFECT][0]);outprefix="F";
   for(int k=0;k<8;k++){double v[WIDTH+1];read_vector("F ",v,WIDTH+1);if(v[0]!=k)bad();ok&=compare(&mon[k],v+1,i,k,'F');ok&=extras(k,false);}ok&=queue_rows();rng_read("FR ",&rng);ok&=rng_equal(rng,bt.rng);rng_read("FB ",&rng);ok&=rng_equal(rng,bt.battle_rng);
  }
  if(strcmp(line_read("E"),"\n"))bad();if(bt.n_unported||bt.n_deferred||e.overflow)ok=false;if(!ok){if(failed<30)fprintf(stderr,"Gulp case%d mode%d user%d attack%d fx%d unported%d failed\n",i,mode,u,attack,fx,bt.n_unported);failed++;}
 }
 double foot[1];read_vector("GULP_COMPLETE ",foot,1);if(foot[0]!=n||fgetc(input)!=EOF)bad();/* These are separate refusals for the two directly probed native exceptions,
 * outside the successful callback/queued-status comparison count. */
 memset(&bt,0,sizeof bt);memset(&e,0,sizeof e);bt.engine=&e;e.n_levels=1;e.current=-1;
 int boundary=pr_phase_new(&e,PH_DAMAGE_ANIM,-1);e.pool[boundary].anim_amount=1;e.pool[boundary].anim_result=HIT_RESULT_INDIRECT;pr_phase_run(&bt,&e,boundary);
 if(bt.n_unported!=1||bt.unported[0]!=PR_FEAT_PHASE_0+PH_DAMAGE_ANIM)return 1;
 bt.n_unported=0;boundary=pr_phase_new(&e,PH_OBTAIN_STATUS_EFFECT,-1);pr_phase_run(&bt,&e,boundary);
 if(bt.n_unported!=1||bt.unported[0]!=PR_FEAT_PHASE_0+PH_OBTAIN_STATUS_EFFECT)return 1;
 printf("Gulp: 2 native exception boundaries explicitly refused\n");
 printf("Gulp: %d complete, %d exact, %d failures\n",n,n-failed,failed);return failed?1:0;
}
