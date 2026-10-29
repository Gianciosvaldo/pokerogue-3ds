#define main retained_gas_main
#define emit retained_emit
#include "gas-lifecycle-test.c"
#undef emit
#undef main
static pr_pokemon actors[6];static unsigned checks;
#define EQ(a,b) do{checks++;if((a)!=(b)){fprintf(stderr,"case%d line%d %s actual%.17g expected%.17g\n",c,__LINE__,#a,(double)(a),(double)(b));ok=false;}}while(0)
static int actor_index(const pr_pokemon*p){for(int i=0;i<6;i++)if(p==&actors[i])return i;return -1;}
static void frame(pr_battle*bt,int c,const char*prefix,bool initial,bool*success){bool ok=*success;char key[16];snprintf(key,sizeof key,"%s ",prefix);
 for(int k=0;k<6;k++){
  double v[WIDTH+1];read_vector(key,v,WIDTH+1);if(v[0]!=k)bad();pr_pokemon validation;if(!initial)load(&validation,v+1,k<3?0:2);if(initial){load(&actors[k],v+1,k<3?0:2);if(actors[k].id!=101u+k||actors[k].species!=SPECIES_ID_RATTATA||actors[k].level!=30)bad();}
  double observed[WIDTH];capture(&actors[k],observed);for(int j=0;j<WIDTH;j++){if(observed[j]!=v[j+1]&&c<3)fprintf(stderr,"actor%d %s field%d C%g JS%g\n",k,prefix,j,observed[j],v[j+1]);EQ(observed[j],v[j+1]);}
  char extra[16];snprintf(extra,sizeof extra,"%sX ",prefix);double x[5];read_vector(extra,x,5);if(x[0]!=k)bad();bool has=integer(x[1],0,1);int turn=integer(x[2],0,32767),move=integer(x[3],0,PR_MOVE_COUNT-1);if(x[4]<-1||x[4]>UINT32_MAX||floor(x[4])!=x[4])bad();
  if(initial&&has)actors[k].tags[actors[k].n_tags++]=(pr_battler_tag){.type=BATTLER_TAG_TYPE_CHARGED,.turn_count=turn,.source_move=move,.source_id=(int64_t)x[4]};
  const pr_battler_tag*t=pr_get_battler_tag(&actors[k],BATTLER_TAG_TYPE_CHARGED);EQ(!!t,has);EQ(t?t->turn_count:0,turn);EQ(t?t->source_move:0,move);EQ(t?t->source_id:-1,(int64_t)x[4]);
 }
 *success=ok;(void)bt;
}
static void emit(const pr_phase*p,char scope,int group,int order){
 char text[2048]="-",stages[128]="-",name[96];
 if(p->type==PH_MESSAGE){char rendered[1024];pr_text_source src={locale_get,NULL,"en"};size_t len=pr_msg_format(&p->msg,&src,rendered,sizeof rendered);if(len>=sizeof rendered)bad();for(size_t i=0;i<len;i++)snprintf(text+2*i,3,"%02x",(unsigned char)rendered[i]);if(!len)strcpy(text,"-");}
 if(p->type==PH_STAT_STAGE_CHANGE){stages[0]=0;size_t n=0;for(int i=0;i<p->n_ssc;i++)n+=(size_t)snprintf(stages+n,sizeof stages-n,"%s%d:%d",i?",":"",p->ssc_stat[i],p->ssc_stages[i]);}
 if(p->type==PH_MARKER)snprintf(name,sizeof name,"Marker:%s",pr_phase_name[p->marker_of]);else snprintf(name,sizeof name,"%s",pr_phase_name[p->type]);
 if(nq>=128)bad();snprintf(queue[nq++],sizeof queue[0],"Q %c %d %d %s %d %d %d %s %s %d\n",scope,group,order,name,p->type==PH_MARKER?-1:p->battler,p->type==PH_SHOW_ABILITY?p->anim_result:-1,p->type==PH_SHOW_ABILITY?(p->anim==UINT16_MAX?-1:p->anim):-1,text,stages,p->type==PH_STAT_STAGE_CHANGE?actor_index(p->source_p):-1);
}
#ifndef TAILWIND_OWNER_NO_MAIN
int main(int argc,char**argv){if(argc!=2)return 2;input=fopen(argv[1],"r");if(!input||strcmp(line_read("TAILWIND_OWNER "),"1 2048\n"))bad();int failed=0;char*w=line_read("W ");w[strcspn(w,"\n")]=0;pr_seedstr wave;pr_seed_from_ascii(&wave,w);
 const pr_move*move=&pr_moves[MOVE_ID_COURT_CHANGE];int attr=-1;for(int i=0;i<move->nattr;i++)if(pr_attrs[move->attr0+i].cls==PR_AC_SwapArenaTagsAttr)attr=move->attr0+i;if(attr<0)bad();
 for(int c=0;c<2048;c++){
  double v[9];read_vector("C ",v,9);if(v[0]!=c)bad();int sourceSide=integer(v[1],0,1),tagSide=integer(v[2],0,1),shape=integer(v[3],0,3),mode=integer(v[4],0,1),ability=integer(v[5],0,3),charged=integer(v[6],0,1),tie=integer(v[7],0,1),stage=integer(v[8],-6,6);
  int basis=c,stages[]={-6,0,5,6};if(stage!=stages[basis%4])bad();basis/=4;if(tie!=basis%2)bad();basis/=2;if(charged!=basis%2)bad();basis/=2;if(ability!=basis%4)bad();basis/=4;if(mode!=basis%2)bad();basis/=2;if(shape!=basis%4)bad();basis/=4;if(tagSide!=basis%2)bad();basis/=2;if(sourceSide!=basis%2)bad();
  double order[6];read_vector("P ",order,6);bool used[6]={false};for(int i=0;i<6;i++){int k=integer(order[i],0,5),expected=i;if((shape==1||shape==3)&&i/3==sourceSide)expected=sourceSide*3+(i%3+1)%3;if(k!=expected||used[k]||(i<3)!=(k<3))bad();used[k]=true;}
  static pr_battle bt;static pr_engine e;memset(&bt,0,sizeof bt);memset(&e,0,sizeof e);e.n_levels=1;e.current=-1;bt.engine=&e;bt.double_battle=true;bt.game_mode=GAME_MODES_CLASSIC;bt.wave_index=1;bt.challenge_flags_known=true;bt.ignoring_effect_source=-1;bt.max_exp_level=100;bt.wave_seed=wave;bt.turn=1;
  rng_read("R ",&bt.rng);rng_read("B ",&bt.battle_rng);bt.rng_live=bt.battle_rng_live=true;
  for(int side=0;side<2;side++){bt.n_party[side]=3;for(int k=0;k<3;k++)bt.party[side][k]=&actors[(int)order[side*3+k]];for(int k=0;k<2;k++)bt.field[side*2+k]=bt.party[side][k];}
  bool ok=true;frame(&bt,c,"I",true,&ok);int64_t source=shape==2?999999:101+sourceSide*3;
  for(int k=0;k<6;k++){
   pr_pokemon*p=&actors[k];int position=-1;for(int j=0;j<6;j++)if((int)order[j]==k)position=j%3;
   const bool active=position<2;
   int expected=ability==0?ABILITY_ID_NONE:ability==3?ABILITY_ID_WIND_RIDER:(position%2?ABILITY_ID_WIND_RIDER:ABILITY_ID_WIND_POWER);
   if(!active)expected=ABILITY_ID_NONE;
   if(p->stats[STAT_SPD]!=(tie?100:10+k*10)||p->stat_stages[0]!=stage||p->on_field!=active||p->hp!=((shape==3&&k==sourceSide*3)?0:p->stats[STAT_HP]))bad();
   if(p->ability!=(ability==2?ABILITY_ID_NONE:expected)||p->passive!=((active&&ability==2)?expected:ABILITY_ID_NONE)||p->passive_flag!=(active&&ability==2))bad();
   if(!!pr_get_battler_tag(p,BATTLER_TAG_TYPE_CHARGED)!=(active&&charged))bad();
  }
  double initial_count[1];read_vector("ITAGS ",initial_count,1);if(initial_count[0]!=mode)bad();
  if(mode){double t[7];read_vector("ITAG ",t,7);if(t[0]!=ARENA_TAG_TYPE_TAILWIND||t[1]!=tagSide+1||t[2]!=4||t[3]!=4||t[4]!=MOVE_ID_TAILWIND||t[5]!=source||t[6]!=1)bad();bt.n_arena_tags=1;bt.arena_tags[0]=(pr_arena_tag){.type=t[0],.side=t[1],.turn_count=t[2],.max_duration=t[3],.source_move=t[4],.source_id=(int64_t)t[5],.layers=t[6]};int ph=pr_phase_new(&e,PH_MOVE_EFFECT,0);e.pool[ph].move_id=MOVE_ID_COURT_CHANGE;pr_apply_move_effect(&bt,&e,ph,attr,bt.field[0],bt.field[0]);}
  else pr_arena_add_tag(&bt,&e,ARENA_TAG_TYPE_TAILWIND,4,MOVE_ID_TAILWIND,source,tagSide+1,true);
  frame(&bt,c,"O",false,&ok);double count[1];read_vector("TAGS ",count,1);int nt=integer(count[0],0,PR_MAX_ARENA_TAGS);EQ(bt.n_arena_tags,nt);for(int i=0;i<nt;i++){double tag[7];read_vector("TAG ",tag,7);for(int j=0;j<7;j++)if(!isfinite(tag[j])||floor(tag[j])!=tag[j])bad();if(i>=bt.n_arena_tags){ok=false;continue;}const pr_arena_tag*t=&bt.arena_tags[i];double got[]={t->type,t->side,t->turn_count,t->max_duration,t->source_move,(double)t->source_id,t->layers};for(int j=0;j<7;j++)EQ(got[j],tag[j]);}
  pr_rng want;rng_read("RO ",&want);EQ(rng_equal(want,bt.rng),true);rng_read("BO ",&want);EQ(rng_equal(want,bt.battle_rng),true);
  double q[3];read_vector("QCOUNT ",q,3);int n=integer(q[0],0,128);EQ(e.n_levels,integer(q[1],1,PR_MAX_LEVELS));EQ(e.deferred_active,integer(q[2],0,1));nq=0;for(int l=0;l<e.n_levels;l++)for(int j=0;j<e.level_len[l];j++)emit(&e.pool[e.levels[l][j]],'L',l,j);
  int types[PH_COUNT_],nTypes=0;for(int t=0;t<PH_COUNT_;t++)if(e.n_dyn[t])types[nTypes++]=t;for(int j=1;j<nTypes;j++)for(int k=j;k>0&&strcmp(pr_phase_name[types[k]],pr_phase_name[types[k-1]])<0;k--){int t=types[k];types[k]=types[k-1];types[k-1]=t;}for(int k=0;k<nTypes;k++)for(int j=0;j<e.n_dyn[types[k]];j++)emit(&e.pool[e.dyn[types[k]][j]],'D',k,j);
  EQ(nq,n);for(int j=0;j<n;j++){line_read("Q ");char scope,name[96],hex[2048],changes[128],extra;int group,order,b,passive,ability,person;
  if(sscanf(line,"Q %c %d %d %95s %d %d %d %2047s %127s %d %c",&scope,&group,&order,name,&b,&passive,&ability,hex,changes,&person,&extra)!=10||(scope!='L'&&scope!='D')||group<0||group>=PR_MAX_LEVELS||order<0||order>=PR_MAX_PHASES||b< -1||b>3||passive< -1||passive>1||ability< -1||ability>=PR_ABILITY_COUNT||person< -1||person>5)bad();
  bool known=false;const char*phase_name=!strncmp(name,"Marker:",7)?name+7:name;for(int t=0;t<PH_COUNT_;t++)if(pr_phase_name[t]&&!strcmp(pr_phase_name[t],phase_name))known=true;if(!known)bad();
  if(strcmp(hex,"-")&&(strlen(hex)%2||strspn(hex,"0123456789abcdef")!=strlen(hex)))bad();
  if(strcmp(changes,"-")){const char*cursor=changes;do{char*end;long stat=strtol(cursor,&end,10);if(cursor==end||*end!=':'||stat<1||stat>7)bad();cursor=end+1;long amount=strtol(cursor,&end,10);if(cursor==end||amount< -12||amount>12||(*end&&*end!=','))bad();cursor=*end?end+1:end;}while(*cursor);}
  if(j>=nq){ok=false;continue;}if(strcmp(line,queue[j])&&(c<3||c==16))fprintf(stderr,"queue C:%sJS:%s",queue[j],line);EQ(strcmp(line,queue[j]),0);}if(strcmp(line_read("E"),"\n"))bad();EQ(bt.n_unported,0);EQ(e.overflow,false);if(!ok)failed++;
 }
 if(strcmp(line_read("TAILWIND_OWNER_COMPLETE "),"2048\n")||fgetc(input)!=EOF)bad();static pr_battle missing;static pr_engine engine;engine.n_levels=1;engine.current=-1;missing.engine=&engine;
 pr_arena_add_tag(&missing,&engine,ARENA_TAG_TYPE_TAILWIND,4,MOVE_ID_TAILWIND,999999,ARENA_TAG_SIDE_PLAYER,true);
 if(missing.n_unported!=1||missing.unported[0]!=PR_FEAT_PARTY){fprintf(stderr,"unknown party context admitted\n");return 1;}
 static pr_battle absent;static pr_engine no_source;no_source.n_levels=1;no_source.current=-1;absent.engine=&no_source;
 pr_arena_add_tag(&absent,&no_source,ARENA_TAG_TYPE_TAILWIND,4,MOVE_ID_TAILWIND,-1,ARENA_TAG_SIDE_PLAYER,true);
 if(absent.n_unported){fprintf(stderr,"undefined source spuriously flagged\n");return 1;}
 printf("Tailwind owner: 2048 cases, %u checks, %d mismatches\n",checks,failed);return failed?1:0;
}

#endif
