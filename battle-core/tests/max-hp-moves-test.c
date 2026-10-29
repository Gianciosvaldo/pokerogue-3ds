#define main retained_gas_main
#define emit retained_emit
#include "wide-substitute-frame.inc"
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
static void mon_frame(int c,const char*prefix,bool initial,bool*success){bool ok=*success;char key[16];snprintf(key,sizeof key,"%s ",prefix);
 for(int k=0;k<6;k++){double v[WIDTH+1];read_vector(key,v,WIDTH+1);if(v[0]!=k)bad();if(initial)load(&actors[k],v+1,k<3?0:2);else {pr_pokemon validation;load(&validation,v+1,k<3?0:2);}double observed[WIDTH];capture(&actors[k],observed);for(int j=0;j<WIDTH;j++)EQ(observed[j],v[j+1]);char extra[16];snprintf(extra,sizeof extra,"%sX ",prefix);double x[5];read_vector(extra,x,5);if(x[0]!=k||x[1]||x[2]||x[3]||x[4]!=-1)bad();}
 *success=ok;
}
static void message_hex(const pr_phase*p,bool present,char*hex){char rendered[1024];pr_text_source src={locale_get,NULL,"en"};size_t n=present?pr_msg_format(&p->msg,&src,rendered,sizeof rendered):0;if(n>=sizeof rendered)bad();if(!n){strcpy(hex,"-");return;}for(size_t j=0;j<n;j++)snprintf(hex+2*j,3,"%02x",(unsigned char)rendered[j]);}
int main(int argc,char**argv){if(argc!=2)return 2;input=fopen(argv[1],"r");if(!input||strcmp(line_read("MAX_HP_MOVES "),"1 1344\n"))bad();pr_closures_init();char*w=line_read("W ");w[strcspn(w,"\n")]=0;pr_seedstr wave;pr_seed_from_ascii(&wave,w);int failed=0;
 const int moves[]={MOVE_ID_STRUGGLE,MOVE_ID_FLAME_BURST,MOVE_ID_STEEL_BEAM,MOVE_ID_REST,MOVE_ID_CURSE,MOVE_ID_RECOVER,MOVE_ID_MOONLIGHT},classes[]={PR_AC_RecoilAttr,PR_AC_FlameBurstAttr,PR_AC_HalfSacrificialAttr,PR_AC_RestAttr,PR_AC_CurseAttr,PR_AC_HealAttr,PR_AC_VariableHealAttr};
 for(int c=0;c<1344;c++){
  double meta[7];read_vector("C ",meta,7);int basis=c,mode=basis%7;basis/=7;int override=basis%4;basis/=4;int levels[]={1,50,255,1000000},level=levels[basis%4];basis/=4;int species[]={SPECIES_ID_RATTATA,SPECIES_ID_BLISSEY,SPECIES_ID_SHEDINJA},sp=species[basis%3];basis/=3;int slot=basis%2;basis/=2;int side=basis%2,u=side*3+slot,t=side?0:3;if(meta[0]!=c||meta[1]!=side||meta[2]!=slot||meta[3]!=sp||meta[4]!=level||meta[5]!=override||meta[6]!=mode)bad();bool ok=true;
  static pr_battle bt;static pr_engine e;memset(&bt,0,sizeof bt);memset(&e,0,sizeof e);e.n_levels=1;e.current=-1;bt.engine=&e;bt.double_battle=true;bt.game_mode=GAME_MODES_CLASSIC;bt.wave_index=1;bt.challenge_flags_known=true;bt.ignoring_effect_source=-1;bt.max_exp_level=100;bt.wave_seed=wave;bt.turn=1;
  rng_read("R ",&bt.rng);rng_read("B ",&bt.battle_rng);bt.rng_live=bt.battle_rng_live=true;for(int side=0;side<2;side++){bt.n_party[side]=3;for(int k=0;k<3;k++)bt.party[side][k]=&actors[side*3+k];for(int k=0;k<2;k++)bt.field[side*2+k]=bt.party[side][k];}
  mon_frame(c,"I",true,&ok);for(int k=0;k<6;k++){pr_pokemon*p=&actors[k];if(p->species!=(k==u?sp:SPECIES_ID_RATTATA)||p->level!=(k==u?level:30)||p->on_field!=(k%3<2)||p->hp!=p->stats[STAT_HP]||p->n_tags)bad();double expected=override==0?0:override==1?1:override==2?2.0*p->stats[STAT_HP]+1:34359738371.;if(p->summon_stats[STAT_HP]!=expected)bad();}
  int move=moves[mode];if(!mode){pr_turn_move tm={.move=move,.use_mode=MOVE_USE_MODE_NORMAL,.result=MOVE_RESULT_SUCCESS,.n_targets=1,.targets={side?0:2}};pr_push_history_shared(&bt,&e,&actors[u],tm,-1);}
  const pr_move*mv=&pr_moves[move];int attr=-1;for(int j=0;j<mv->nattr;j++)if(pr_attrs[mv->attr0+j].cls==classes[mode])attr=mv->attr0+j;if(attr<0)bad();int ph=pr_phase_new(&e,PH_MOVE_EFFECT,pr_battler_index(&bt,&actors[u]));e.pool[ph].move_id=move;pr_apply_move_effect(&bt,&e,ph,attr,&actors[u],&actors[t]);
  mon_frame(c,"O",false,&ok);pr_rng want;rng_read("RO ",&want);EQ(rng_equal(want,bt.rng),true);rng_read("BO ",&want);EQ(rng_equal(want,bt.battle_rng),true);
  double q[3];read_vector("QCOUNT ",q,3);int n=integer(q[0],0,128);EQ(e.n_levels,integer(q[1],1,PR_MAX_LEVELS));EQ(e.deferred_active,integer(q[2],0,1));nq=0;for(int l=0;l<e.n_levels;l++)for(int j=0;j<e.level_len[l];j++)emit(&e.pool[e.levels[l][j]],'L',l,j);
  int types[PH_COUNT_],nTypes=0;for(int t=0;t<PH_COUNT_;t++)if(e.n_dyn[t])types[nTypes++]=t;for(int j=1;j<nTypes;j++)for(int k=j;k>0&&strcmp(pr_phase_name[types[k]],pr_phase_name[types[k-1]])<0;k--){int t=types[k];types[k]=types[k-1];types[k-1]=t;}for(int k=0;k<nTypes;k++)for(int j=0;j<e.n_dyn[types[k]];j++)emit(&e.pool[e.dyn[types[k]][j]],'D',k,j);
  EQ(nq,n);for(int j=0;j<n;j++){line_read("Q ");char scope,name[96],hex[2048],changes[128],extra;int group,order,b,passive,ability,person;
  if(sscanf(line,"Q %c %d %d %95s %d %d %d %2047s %127s %d %c",&scope,&group,&order,name,&b,&passive,&ability,hex,changes,&person,&extra)!=10||(scope!='L'&&scope!='D')||group<0||group>=PR_MAX_LEVELS||order<0||order>=PR_MAX_PHASES||b< -1||b>3||passive< -1||passive>1||ability< -1||ability>=PR_ABILITY_COUNT||person< -1||person>5)bad();
  bool known=false;const char*phase_name=!strncmp(name,"Marker:",7)?name+7:name;for(int t=0;t<PH_COUNT_;t++)if(pr_phase_name[t]&&!strcmp(pr_phase_name[t],phase_name))known=true;if(!known)bad();
  if(strcmp(hex,"-")&&(strlen(hex)%2||strspn(hex,"0123456789abcdef")!=strlen(hex)))bad();
  if(strcmp(changes,"-")){const char*cursor=changes;do{char*end;long stat=strtol(cursor,&end,10);if(cursor==end||*end!=':'||stat<1||stat>7)bad();cursor=end+1;long amount=strtol(cursor,&end,10);if(cursor==end||amount< -12||amount>12||(*end&&*end!=','))bad();cursor=*end?end+1:end;}while(*cursor);}
  if(j>=nq){ok=false;continue;}if(strcmp(line,queue[j])&&(c<3||c==16))fprintf(stderr,"queue C:%sJS:%s",queue[j],line);EQ(strcmp(line,queue[j]),0);}

  for(int k=0;k<6;k++){double v[5];read_vector("TD ",v,5);if(v[0]!=k)bad();for(int j=1;j<5;j++)if(v[j]<0||v[j]>INT32_MAX||floor(v[j])!=v[j])bad();EQ(actors[k].damage_taken,v[1]);EQ(actors[k].single_hit_damage_dealt,v[2]);EQ(actors[k].total_damage_dealt,v[3]);EQ(actors[k].pending_status,v[4]);double tag[2];read_vector("TAGS ",tag,2);if(tag[0]!=k)bad();int n=integer(tag[1],0,1);EQ(actors[k].n_tags,n);for(int j=0;j<n;j++){double v[4];read_vector("TAG ",v,4);if(v[0]!=BATTLER_TAG_TYPE_CURSED||v[1]<0||v[1]>1||v[2]!=MOVE_ID_CURSE||v[3]<101||v[3]>106)bad();if(j>=actors[k].n_tags){ok=false;continue;}const pr_battler_tag*tag=&actors[k].tags[j];EQ(tag->type,v[0]);EQ(tag->turn_count,v[1]);EQ(tag->source_move,v[2]);EQ(tag->source_id,v[3]);}}
  double count[1];read_vector("PAYLOADS ",count,1);int np=integer(count[0],1,32),actual=0;for(int l=0;l<e.n_levels;l++)actual+=e.level_len[l];EQ(actual,np);
  for(int j=0;j<np;j++){line_read("PAY ");int l,at,offset;char name[96];if(sscanf(line,"PAY %d %d %95s %n",&l,&at,name,&offset)!=3||l<0||l>=PR_MAX_LEVELS||at<0||at>=PR_LEVEL_CAP)bad();const pr_phase*p=l<e.n_levels&&at<e.level_len[l]?&e.pool[e.levels[l][at]]:NULL;char*data=line+offset,hex[2048],extra,observed[2048];if(!p)ok=false;
   if(!strcmp(name,"DamageAnimPhase")){double amount;int result,crit;if(sscanf(data,"%lf %d %d %c",&amount,&result,&crit,&extra)!=3||!isfinite(amount)||amount<0||amount>INT32_MAX||result<0||result>255||crit<0||crit>1)bad();if(p){EQ(p->type,PH_DAMAGE_ANIM);EQ(p->anim_amount,amount);EQ(p->anim_result,result);EQ(p->anim_critical,crit);}}
   else if(!strcmp(name,"PokemonHealPhase")){double amount;int full,skip,revive,status,prevent,pp;if(sscanf(data,"%lf %d %d %d %d %d %d %2047s %c",&amount,&full,&skip,&revive,&status,&prevent,&pp,hex,&extra)!=8||!isfinite(amount)||amount<0||floor(amount)!=amount||full<0||full>1||skip<0||skip>1||revive<0||revive>1||status<0||status>1||prevent<0||prevent>1||pp<0||pp>1)bad();if(p){EQ(p->type,PH_POKEMON_HEAL);EQ(p->heal_hp,amount);EQ(!(p->heal_flags&PR_HEAL_NO_FULL_HP_MESSAGE),full);EQ(!!(p->heal_flags&PR_HEAL_SKIP_ANIM),skip);EQ(!!(p->heal_flags&PR_HEAL_REVIVE),revive);EQ(!!(p->heal_flags&PR_HEAL_STATUS),status);EQ(!!(p->heal_flags&PR_HEAL_PREVENT_FULL),prevent);EQ(!!(p->heal_flags&PR_HEAL_RESTORE_PP),pp);message_hex(p,p->heal_flags&PR_HEAL_MESSAGE,observed);EQ(strcmp(observed,hex),0);}}
   else if(!strcmp(name,"ObtainStatusEffectPhase")){int effect,source,sleep;if(sscanf(data,"%d %d %d %2047s %c",&effect,&source,&sleep,hex,&extra)!=4||effect!=STATUS_EFFECT_SLEEP||source<0||source>5||sleep<0||sleep>3)bad();if(p){EQ(p->type,PH_OBTAIN_STATUS_EFFECT);EQ(p->ose_effect,effect);EQ(actor_index(p->source_p),source);EQ(p->ose_sleep_turns,sleep);message_hex(p,true,observed);EQ(strcmp(observed,hex),0);}}
   else if(!strcmp(name,"ResetStatusPhase")){int who,confusion;if(sscanf(data,"%d %d %c",&who,&confusion,&extra)!=2||who<0||who>5||confusion<0||confusion>1)bad();if(p){EQ(p->type,PH_RESET_STATUS);EQ(actor_index(p->pokemon),who);EQ(p->affect_confusion,confusion);}}
   else if(!strcmp(name,"DynamicPhaseMarker")){char phase[96];if(sscanf(data,"%95s %c",phase,&extra)!=1)bad();int type=-1;for(int k=0;k<PH_COUNT_;k++)if(pr_phase_name[k]&&!strcmp(phase,pr_phase_name[k]))type=k;if(type<0)bad();if(p){EQ(p->type,PH_MARKER);EQ(p->marker_of,type);}}
   else if(!strcmp(name,"MessagePhase")){if(sscanf(data,"%2047s %c",hex,&extra)!=1)bad();if(p){EQ(p->type,PH_MESSAGE);message_hex(p,true,observed);EQ(strcmp(observed,hex),0);}}
   else bad();
  }
  double ns[1];read_vector("STATUSES ",ns,1);int nStatus=integer(ns[0],0,1);EQ(e.n_dyn[PH_OBTAIN_STATUS_EFFECT],nStatus);for(int k=0;k<nStatus;k++){line_read("S ");int j,b,effect,who,sleep;char hex[2048],extra;if(sscanf(line,"S %d %d %d %d %d %2047s %c",&j,&b,&effect,&who,&sleep,hex,&extra)!=6||j!=k||b<0||b>3||effect!=STATUS_EFFECT_SLEEP||who<0||who>5||sleep<0||sleep>3)bad();if(k>=e.n_dyn[PH_OBTAIN_STATUS_EFFECT]){ok=false;continue;}const pr_phase*p=&e.pool[e.dyn[PH_OBTAIN_STATUS_EFFECT][k]];char observed[2048];EQ(p->battler,b);EQ(p->ose_effect,effect);EQ(actor_index(p->source_p),who);EQ(p->ose_sleep_turns,sleep);message_hex(p,true,observed);EQ(strcmp(observed,hex),0);}
  double nf[1];read_vector("FAINTS ",nf,1);int nFaints=integer(nf[0],0,4);EQ(e.n_dyn[PH_FAINT],nFaints);for(int k=0;k<nFaints;k++){double v[4];read_vector("F ",v,4);if(v[0]!=k||v[1]<0||v[1]>3||v[2]<0||v[2]>1||v[3]<-1||v[3]>5)bad();if(k>=e.n_dyn[PH_FAINT]){ok=false;continue;}const pr_phase*p=&e.pool[e.dyn[PH_FAINT][k]];EQ(p->battler,v[1]);EQ(p->prevent_instant_revive,v[2]);EQ(actor_index(p->source_p),v[3]);}
  if(strcmp(line_read("E"),"\n"))bad();EQ(bt.n_unported,0);EQ(e.overflow,false);if(!ok)failed++;
 }
 if(strcmp(line_read("MAX_HP_MOVES_COMPLETE "),"1344\n")||fgetc(input)!=EOF)bad();printf("Maximum HP move owners: 1344 cases, %u checks, %d mismatches\n",checks,failed);return failed?1:0;
}
