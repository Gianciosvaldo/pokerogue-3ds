#define main retained_gas_main
#define emit retained_emit
#include "wide-substitute-frame.inc"
#undef emit
#undef main
static pr_pokemon actors[6];static unsigned checks;
#define EQ(a,b) do{checks++;if((a)!=(b)){fprintf(stderr,"case%d line%d %s actual%.17g expected%.17g\n",c,__LINE__,#a,(double)(a),(double)(b));ok=false;}}while(0)
static int actor_index(const pr_pokemon*p){for(int i=0;i<6;i++)if(p==&actors[i])return i;return -1;}
static void emit(const pr_phase*p,char scope,int group,int order){
 char text[2048]="-",stages[128]="-",name[96];
 if(p->type==PH_MESSAGE){char rendered[1024];pr_text_source src={locale_get,NULL,"en"};size_t len=pr_msg_format(&p->msg,&src,rendered,sizeof rendered);if(len>=sizeof rendered)bad();for(size_t i=0;i<len;i++)snprintf(text+2*i,3,"%02x",(unsigned char)rendered[i]);if(!len)strcpy(text,"-");}
 if(p->type==PH_STAT_STAGE_CHANGE){stages[0]=0;size_t n=0;for(int i=0;i<p->n_ssc;i++)n+=(size_t)snprintf(stages+n,sizeof stages-n,"%s%d:%d",i?",":"",p->ssc_stat[i],p->ssc_stages[i]);}
 if(p->type==PH_MARKER)snprintf(name,sizeof name,"Marker:%s",pr_phase_name[p->marker_of]);else snprintf(name,sizeof name,"%s",pr_phase_name[p->type]);
 if(nq>=128)bad();snprintf(queue[nq++],sizeof queue[0],"Q %c %d %d %s %d %d %d %s %s %d\n",scope,group,order,name,p->type==PH_MARKER?-1:p->battler,p->type==PH_SHOW_ABILITY?p->anim_result:-1,p->type==PH_SHOW_ABILITY?(p->anim==UINT16_MAX?-1:p->anim):-1,text,stages,p->type==PH_STAT_STAGE_CHANGE?actor_index(p->source_p):-1);
}
static void mon_frame(int c,const char*prefix,bool initial,bool*success){bool ok=*success;char key[16];snprintf(key,sizeof key,"%s ",prefix);
 for(int k=0;k<6;k++){double v[WIDTH+1];read_vector(key,v,WIDTH+1);if(v[0]!=k)bad();if(initial)load(&actors[k],v+1,k<3?0:2);else {pr_pokemon validation;load(&validation,v+1,k<3?0:2);}double observed[WIDTH];capture(&actors[k],observed);for(int j=0;j<WIDTH;j++)EQ(observed[j],v[j+1]);char extra[16];snprintf(extra,sizeof extra,"%sX ",prefix);double x[5];read_vector(extra,x,5);if(x[0]!=k||x[1]||x[2]||x[3]||x[4]!=-1)bad();}
 *success=ok;
}
static bool exceptional;
static void tags_frame(int c,const char*prefix,bool initial,bool*success){bool ok=*success;char key[20],item[20];snprintf(key,sizeof key,"%sTAGS ",prefix);snprintf(item,sizeof item,"%sTAG ",prefix);
 for(int k=0;k<6;k++){double count[2];read_vector(key,count,2);if(count[0]!=k)bad();int n=integer(count[1],0,2);if(initial)actors[k].n_tags=n;EQ(actors[k].n_tags,n);
 for(int j=0;j<n;j++){line_read(item);int type,turn,move;long long id;char index[32],hp[32],focus[32],extra;if(sscanf(line+strlen(item),"%d %d %d %lld %31s %31s %31s %c",&type,&turn,&move,&id,index,hp,focus,&extra)!=7||type<0||type>=BATTLER_TAG_TYPE_COUNT_||turn<0||turn>100||move<0||move>=PR_MOVE_COUNT||id< -1||id>UINT32_MAX)bad();
 bool index_defined=strcmp(index,"u")!=0,hp_defined=strcmp(hp,"u")!=0,focus_defined=strcmp(focus,"u")!=0;int source_index=-1;double hp_value=0;int focus_value=0;char*end;
 if(index_defined){source_index=strtol(index,&end,10);if(*end||source_index< -1||source_index>3)bad();}
 if(hp_defined){hp_value=strtod(hp,&end);if(*end||!isfinite(hp_value)||floor(hp_value)!=hp_value||hp_value<0||hp_value>INT32_MAX)bad();}
 if(focus_defined){focus_value=strtol(focus,&end,10);if(*end||focus_value<0||focus_value>1)bad();}
 if(type!=BATTLER_TAG_TYPE_SEEDED&&index_defined)bad();if(type!=BATTLER_TAG_TYPE_SUBSTITUTE&&(hp_defined||focus_defined))bad();if(type==BATTLER_TAG_TYPE_SUBSTITUTE&&hp_defined!=focus_defined)bad();
 if(initial){if(type!=BATTLER_TAG_TYPE_SUBSTITUTE||!hp_defined||!focus_defined)bad();actors[k].tags[j]=(pr_battler_tag){.type=type,.turn_count=turn,.source_move=move,.source_id=id,.source_index=source_index,.hp=hp_value,.in_focus=focus_value};}
 if(j>=actors[k].n_tags){ok=false;continue;}const pr_battler_tag*t=&actors[k].tags[j];EQ(t->type,type);EQ(t->turn_count,turn);EQ(t->source_move,move);EQ(t->source_id,id);EQ(t->source_index,source_index);
 if(hp_defined){EQ(t->hp,hp_value);EQ(t->in_focus,focus_value);}else if(type==BATTLER_TAG_TYPE_SUBSTITUTE){if(!exceptional)bad();}else{EQ(t->hp,0);EQ(t->in_focus,false);}
 }}*success=ok;
}
static void context_guards(void){
 const int types[]={BATTLER_TAG_TYPE_TRAPPED,BATTLER_TAG_TYPE_OCTOLOCK,BATTLER_TAG_TYPE_SEEDED,BATTLER_TAG_TYPE_SUBSTITUTE};
 const int moves[]={MOVE_ID_MEAN_LOOK,MOVE_ID_OCTOLOCK,MOVE_ID_LEECH_SEED,MOVE_ID_SUBSTITUTE};
 for(int mode=0;mode<4;mode++)for(int shape=0;shape<3;shape++){
  static pr_battle bt;static pr_engine e;pr_pokemon source=actors[0],target=actors[3];memset(&bt,0,sizeof bt);memset(&e,0,sizeof e);source.n_tags=target.n_tags=0;source.id=0;target.id=200;source.hp=source.stats[STAT_HP];target.hp=target.stats[STAT_HP];source.n_summon_types=target.n_summon_types=0;
  e.n_levels=1;e.current=-1;bt.engine=&e;bt.field[0]=&source;bt.field[2]=&target;bt.ignoring_effect_source=-1;bt.challenge_flags_known=true;
  if(shape==1){bt.n_party[0]=1;bt.party[0][0]=&source;}
  bool ret=pr_add_tag(&bt,&e,&target,types[mode],3,moves[mode],shape==2?0:999999);
  if(bt.n_unported!=(shape==2?0:1)||(shape!=2&&bt.unported[0]!=PR_FEAT_PARTY)||ret!=(shape==2||mode>=2)||e.overflow){fprintf(stderr,"context guard failure mode%d shape%d\n",mode,shape);exit(1);}
 }
 puts("Tag party sources: 12 partial-party guard/field-fallback controls passed");
}
int main(int argc,char**argv){if(argc!=2)return 2;input=fopen(argv[1],"r");if(!input||strcmp(line_read("TAG_PARTY_SOURCES "),"1 768\n"))bad();pr_closures_init();char*w=line_read("W ");w[strcspn(w,"\n")]=0;pr_seedstr wave;pr_seed_from_ascii(&wave,w);int failed=0,guarded=0;
 const int types[]={BATTLER_TAG_TYPE_TRAPPED,BATTLER_TAG_TYPE_OCTOLOCK,BATTLER_TAG_TYPE_SEEDED,BATTLER_TAG_TYPE_SUBSTITUTE},moves[]={MOVE_ID_MEAN_LOOK,MOVE_ID_OCTOLOCK,MOVE_ID_LEECH_SEED,MOVE_ID_SUBSTITUTE};
 for(int c=0;c<768;c++){
  double meta[7];read_vector("C ",meta,7);int basis=c,caster_level=basis%2?50:30;basis/=2;int condition=basis%4;basis/=4;int id_mode=basis%3;basis/=3;int shape=basis%4;basis/=4;int side=basis%2;basis/=2;int mode=basis%4,u=(1-side)*3,origin=side*3;if(meta[0]!=c||meta[1]!=mode||meta[2]!=side||meta[3]!=shape||meta[4]!=id_mode||meta[5]!=condition||meta[6]!=caster_level)bad();bool ok=true;exceptional=mode==3&&(shape==3||id_mode==2)&&condition!=3;guarded+=exceptional;
  double order[6];read_vector("P ",order,6);for(int k=0;k<6;k++){int expected=k;if((shape==1||shape==2)&&k/3==side)expected=side*3+(k%3+1)%3;if(order[k]!=expected)bad();}
  double source[2];read_vector("SOURCE ",source,2);int64_t sid=id_mode==2?-1:shape==3?(id_mode==1?0:999999):id_mode==1?0:101+origin;if(source[0]!=sid||source[1]!=(shape==3||id_mode==2?-1:origin))bad();
  static pr_battle bt;static pr_engine e;memset(&bt,0,sizeof bt);memset(&e,0,sizeof e);e.n_levels=1;e.current=-1;bt.engine=&e;bt.double_battle=true;bt.game_mode=GAME_MODES_CLASSIC;bt.wave_index=1;bt.challenge_flags_known=true;bt.ignoring_effect_source=-1;bt.max_exp_level=100;bt.wave_seed=wave;bt.turn=1;
  rng_read("R ",&bt.rng);rng_read("B ",&bt.battle_rng);bt.rng_live=bt.battle_rng_live=true;for(int side=0;side<2;side++){bt.n_party[side]=3;for(int k=0;k<3;k++)bt.party[side][k]=&actors[(int)order[side*3+k]];for(int k=0;k<2;k++)bt.field[side*2+k]=bt.party[side][k];}
  mon_frame(c,"I",true,&ok);for(int k=0;k<6;k++){pr_pokemon*p=&actors[k];bool active=bt.field[0]==p||bt.field[1]==p||bt.field[2]==p||bt.field[3]==p;uint32_t id=id_mode==1&&shape!=3&&k==origin?0:101+k;if(p->species!=SPECIES_ID_RATTATA||p->level!=(k==origin?caster_level:30)||p->id!=id||p->on_field!=active||p->hp!=((shape==2&&k==origin)?0:p->stats[STAT_HP]))bad();}
  if(actors[u].n_summon_types!=(condition==1||condition==2?1:0)||(condition==1&&actors[u].summon_types[0]!=POKEMON_TYPE_GHOST)||(condition==2&&actors[u].summon_types[0]!=POKEMON_TYPE_GRASS))bad();
  tags_frame(c,"I",true,&ok);for(int k=0;k<6;k++)if(actors[k].n_tags!=((k==u&&condition==3)?1:0))bad();
  bool result=pr_add_tag(&bt,&e,&actors[u],types[mode],3,moves[mode],sid);double ret[1];read_vector("RET ",ret,1);integer(ret[0],0,1);EQ(result,ret[0]);
  mon_frame(c,"O",false,&ok);tags_frame(c,"O",false,&ok);pr_rng want;rng_read("RO ",&want);EQ(rng_equal(want,bt.rng),true);rng_read("BO ",&want);EQ(rng_equal(want,bt.battle_rng),true);
  double q[3];read_vector("QCOUNT ",q,3);int n=integer(q[0],0,128);EQ(e.n_levels,integer(q[1],1,PR_MAX_LEVELS));EQ(e.deferred_active,integer(q[2],0,1));nq=0;for(int l=0;l<e.n_levels;l++)for(int j=0;j<e.level_len[l];j++)emit(&e.pool[e.levels[l][j]],'L',l,j);
  int types[PH_COUNT_],nTypes=0;for(int t=0;t<PH_COUNT_;t++)if(e.n_dyn[t])types[nTypes++]=t;for(int j=1;j<nTypes;j++)for(int k=j;k>0&&strcmp(pr_phase_name[types[k]],pr_phase_name[types[k-1]])<0;k--){int t=types[k];types[k]=types[k-1];types[k-1]=t;}for(int k=0;k<nTypes;k++)for(int j=0;j<e.n_dyn[types[k]];j++)emit(&e.pool[e.dyn[types[k]][j]],'D',k,j);
  EQ(nq,n);for(int j=0;j<n;j++){line_read("Q ");char scope,name[96],hex[2048],changes[128],extra;int group,order,b,passive,ability,person;
  if(sscanf(line,"Q %c %d %d %95s %d %d %d %2047s %127s %d %c",&scope,&group,&order,name,&b,&passive,&ability,hex,changes,&person,&extra)!=10||(scope!='L'&&scope!='D')||group<0||group>=PR_MAX_LEVELS||order<0||order>=PR_MAX_PHASES||b< -1||b>3||passive< -1||passive>1||ability< -1||ability>=PR_ABILITY_COUNT||person< -1||person>5)bad();
  bool known=false;const char*phase_name=!strncmp(name,"Marker:",7)?name+7:name;for(int t=0;t<PH_COUNT_;t++)if(pr_phase_name[t]&&!strcmp(pr_phase_name[t],phase_name))known=true;if(!known)bad();
  if(strcmp(hex,"-")&&(strlen(hex)%2||strspn(hex,"0123456789abcdef")!=strlen(hex)))bad();
  if(strcmp(changes,"-")){const char*cursor=changes;do{char*end;long stat=strtol(cursor,&end,10);if(cursor==end||*end!=':'||stat<1||stat>7)bad();cursor=end+1;long amount=strtol(cursor,&end,10);if(cursor==end||amount< -12||amount>12||(*end&&*end!=','))bad();cursor=*end?end+1:end;}while(*cursor);}
  if(j>=nq){ok=false;continue;}if(strcmp(line,queue[j])&&(c<3||c==16))fprintf(stderr,"queue C:%sJS:%s",queue[j],line);EQ(strcmp(line,queue[j]),0);}
  double a[1];read_vector("ANIMS ",a,1);int na=integer(a[0],0,1),actual=0;for(int l=0;l<e.n_levels;l++)for(int j=0;j<e.level_len[l];j++)if(e.pool[e.levels[l][j]].type==PH_POKEMON_ANIM)actual++;EQ(actual,na);
  for(int k=0;k<na;k++){double v[2];read_vector("ANIM ",v,2);if(v[0]<0||v[0]>5||v[1]!=POKEMON_ANIM_TYPE_SUBSTITUTE_ADD)bad();int seen=0;const pr_phase*p=NULL;for(int l=0;l<e.n_levels;l++)for(int j=0;j<e.level_len[l];j++)if(e.pool[e.levels[l][j]].type==PH_POKEMON_ANIM){if(seen++==k)p=&e.pool[e.levels[l][j]];}if(!p){ok=false;continue;}EQ(actor_index(p->pokemon?p->pokemon:bt.field[p->battler]),v[0]);EQ(p->anim,v[1]);}
  if(strcmp(line_read("E"),"\n"))bad();EQ(bt.n_unported,exceptional?1:0);if(exceptional)EQ(bt.unported[0],PR_FEAT_PARTY);EQ(e.overflow,false);if(!ok)failed++;
 }
 if(strcmp(line_read("TAG_PARTY_SOURCES_COMPLETE "),"768\n")||fgetc(input)!=EOF||guarded!=72)bad();context_guards();printf("Tag party sources: 768 cases (696 compared, 72 undefined-HP guarded), %u checks, %d mismatches\n",checks,failed);return failed?1:0;
}
