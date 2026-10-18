/* Full native suppression state/queue transport. The dispatcher has a void
 * C interface; query mode additionally compares getCondition's boolean. */
#define main gas_replay_main
#include "gas-lifecycle-test.c"
#undef main
int main(int argc,char **argv){
 if(argc!=2||!getenv("TURNTEST_LOCALES"))return 2;input=fopen(argv[1],"r");if(!input)return 2;pr_closures_init();
 double h[3];read_vector("SUPPRESSION ",h,3);if(h[0]!=1)bad();int n=integer(h[1],16384,100000),seed=integer(h[2],0,INT32_MAX),failures=0;
 int attrs[2]={-1,-1};const int moves[2]={MOVE_ID_GASTRO_ACID,MOVE_ID_CORE_ENFORCER};
 for(int k=0;k<2;k++){const pr_move *m=&pr_moves[moves[k]];for(int j=0;j<m->nattr;j++)if(pr_attrs[m->attr0+j].cls==(k?PR_AC_SuppressAbilitiesIfActedAttr:PR_AC_SuppressAbilitiesAttr))attrs[k]=m->attr0+j;if(attrs[k]<0)bad();}
 for(int c=0;c<n;c++){
  static pr_battle bt;static pr_engine e;static pr_pokemon mons[4];memset(&bt,0,sizeof bt);memset(&e,0,sizeof e);bt.engine=&e;e.n_levels=1;e.current=-1;bt.double_battle=true;bt.game_mode=GAME_MODES_CLASSIC;bt.ignoring_effect_source=-1;bt.wave_index=3;bt.turn=1;bt.last_used_pokeball=-1;
  char wave[64],battle[64];snprintf(wave,sizeof wave,"suppression-wave-%d",seed);snprintf(battle,sizeof battle,"suppression-%d-%d",seed,c);pr_seed_from_ascii(&bt.wave_seed,wave);pr_seed_from_ascii(&bt.battle_seed,battle);
  double row[5];read_vector("C ",row,5);if(row[0]!=c)bad();int mode=integer(row[1],0,2),holder=integer(row[2],0,3),target=integer(row[3],0,3),acted=integer(row[4],0,1);
  double g[2];read_vector("G ",g,2);int initial=integer(g[0],0,3);if(g[1]<-1||g[1]>UINT32_MAX||floor(g[1])!=g[1])bad();int64_t source_id=(int64_t)g[1];
  if(initial){bt.n_arena_tags=1;bt.arena_tags[0]=(pr_arena_tag){.type=ARENA_TAG_TYPE_NEUTRALIZING_GAS,.side=ARENA_TAG_SIDE_BOTH,.source_id=source_id,.source_count=initial,.layers=1};}
  double w[3];read_vector("W ",w,3);bt.weather=integer(w[0],0,WEATHER_TYPE_STRONG_WINDS);bt.weather_turns_left=integer(w[1],0,100);bt.weather_max_duration=integer(w[2],0,100);
  double cond[1];read_vector("CAN ",cond,1);int want_condition=integer(cond[0],0,1);
  double ac[8];read_vector("AC ",ac,8);for(int j=0;j<8;j++)integer(ac[j],0,1);
  rng_read("R ",&bt.rng);rng_read("B ",&bt.battle_rng);bt.rng_live=bt.battle_rng_live=true;bool ok=true;
  for(int b=0;b<4;b++){double v[WIDTH+1];read_vector("I ",v,WIDTH+1);if(v[0]!=b)bad();load(&mons[b],v+1,b);bt.field[b]=&mons[b];bt.party[b/2][b%2]=&mons[b];bt.n_party[b/2]++;ok&=compare(&mons[b],v+1,c,b,'I');}
  mons[target].acted=acted;bt.participants[0]=mons[0].id;bt.participants[1]=mons[1].id;bt.n_participants=2;
  for(int b=0;b<4;b++){bool primary=pr_can_apply_ability(&bt,&mons[b],mons[b].ability),passive=mons[b].passive_flag&&pr_can_apply_ability(&bt,&mons[b],mons[b].passive);if(ac[2*b]!=primary||ac[2*b+1]!=passive){if(c<200)fprintf(stderr,"case%d before canApply%d C%d/%d JS%g/%g\n",c,b,primary,passive,ac[2*b],ac[2*b+1]);ok=false;}}
  if(pr_can_suppress_ability(&mons[target])!=want_condition){if(c<200)fprintf(stderr,"case%d condition C%d JS%d\n",c,pr_can_suppress_ability(&mons[target]),want_condition);ok=false;}
  int condition=pr_can_suppress_ability(&mons[target]);
  if(mode!=2){int ph=pr_phase_new(&e,PH_MOVE_EFFECT,holder);e.pool[ph].move_id=moves[mode];pr_apply_move_effect(&bt,&e,ph,attrs[mode],&mons[holder],&mons[target]);}
  double ret[1];read_vector("RESULT ",ret,1);int source_result=integer(ret[0],0,1);if(mode==2)ok&=source_result==condition;
  double ga[3];read_vector("GA ",ga,3);const pr_arena_tag *gas=pr_arena_get_tag_on_side(&bt,ARENA_TAG_TYPE_NEUTRALIZING_GAS,ARENA_TAG_SIDE_BOTH);
  double actual_gas[]={bt.n_arena_tags,gas?gas->source_count:0,gas?gas->being_removed:0};for(int j=0;j<3;j++)if(ga[j]!=actual_gas[j]){if(c<200)fprintf(stderr,"case%d gas%d C%g JS%g\n",c,j,actual_gas[j],ga[j]);ok=false;}
  double ao[8];read_vector("ACO ",ao,8);for(int b=0;b<4;b++){
   bool primary=pr_can_apply_ability(&bt,&mons[b],mons[b].ability),passive=mons[b].passive_flag&&pr_can_apply_ability(&bt,&mons[b],mons[b].passive);
   if(ao[2*b]!=primary||ao[2*b+1]!=passive){if(c<200)fprintf(stderr,"case%d after canApply%d C%d/%d JS%g/%g\n",c,b,primary,passive,ao[2*b],ao[2*b+1]);ok=false;}
  }
  double world[14];read_vector("WORLD ",world,14);uint32_t ids[2]={0,0};for(int k=0;k<bt.n_participants&&k<2;k++)ids[k]=bt.participants[k];if(bt.n_participants==2&&ids[0]>ids[1]){uint32_t id=ids[0];ids[0]=ids[1];ids[1]=id;}
  double actual_world[]={bt.weather,bt.weather_turns_left,bt.weather_max_duration,bt.terrain,bt.terrain_turns_left,bt.terrain_max_duration,bt.player_faints,bt.enemy_faints,bt.battle_score,bt.post_battle_loot,bt.ability_bar_visible,bt.n_participants,ids[0],ids[1]};for(int k=0;k<14;k++)if(actual_world[k]!=world[k]){if(c<200)fprintf(stderr,"case%d world%d C:%.17g JS:%.17g\n",c,k,actual_world[k],world[k]);ok=false;}
  for(int b=0;b<4;b++){double v[WIDTH+1];read_vector("O ",v,WIDTH+1);if(v[0]!=b)bad();ok&=compare(&mons[b],v+1,c,b,'O');}
  pr_rng want; rng_read("RO ",&want);if(!rng_equal(want,bt.rng))ok=false;rng_read("BO ",&want);if(!rng_equal(want,bt.battle_rng))ok=false;
  double qr[3];read_vector("QCOUNT ",qr,3);int count=integer(qr[0],0,128),levels=integer(qr[1],1,PR_MAX_LEVELS),deferred=integer(qr[2],0,1);if(levels!=e.n_levels||deferred!=e.deferred_active)ok=false;nq=0;
  for(int l=0;l<e.n_levels;l++)for(int j=0;j<e.level_len[l];j++)emit(&e.pool[e.levels[l][j]],'L',l,j);
  int types[PH_COUNT_],nt=0;for(int t=0;t<PH_COUNT_;t++)if(e.n_dyn[t])types[nt++]=t;for(int j=1;j<nt;j++)for(int k=j;k>0&&strcmp(pr_phase_name[types[k]],pr_phase_name[types[k-1]])<0;k--){int t=types[k];types[k]=types[k-1];types[k-1]=t;}for(int k=0;k<nt;k++)for(int j=0;j<e.n_dyn[types[k]];j++)emit(&e.pool[e.dyn[types[k]][j]],'D',k,j);
  if(nq!=count)ok=false;
  for(int j=0;j<count;j++){
   line_read("Q ");queue_numbers();char scope,name[96],hex[2048],stages[128],extra;int group,order,battler,passive;
   if(sscanf(line,"Q %c %d %d %95s %d %d %2047s %127s %c",&scope,&group,&order,name,&battler,&passive,hex,stages,&extra)!=8 || (scope!='L'&&scope!='D') || group<0 || group>=PR_MAX_PHASES || order<0 || order>=PR_MAX_PHASES || battler< -1 || battler>3)bad();
   const char *phase=!strncmp(name,"Marker:",7)?name+7:name;bool known=false;for(int t=0;t<PH_COUNT_;t++)if(pr_phase_name[t]&&!strcmp(phase,pr_phase_name[t]))known=true;if(!known)bad();
   if(!strcmp(name,"ShowAbilityPhase")){if(passive<0||passive>1)bad();}else if(passive!=-1)bad();
   if(strcmp(hex,"-")){size_t n=strlen(hex);if(n%2)bad();for(size_t k=0;k<n;k++)if(!isxdigit((unsigned char)hex[k]))bad();}
   if(!strcmp(name,"QuietFormChangePhase")){if(strncmp(stages,"F:",2))bad();char *end;errno=0;long target=strtol(stages+2,&end,10);if(errno||end==stages+2||*end||target<0||target>255)bad();}
   else if(strcmp(stages,"-")){const char *at=stages;while(*at){char *end;long stat=strtol(at,&end,10);if(end==at||*end!=':'||stat<STAT_ATK||stat>STAT_EVA)bad();at=end+1;long amount=strtol(at,&end,10);if(end==at||amount< -128||amount>127||(*end&&*end!=','))bad();at=*end?end+1:end;if(*end==','&&!*at)bad();}}
   if(j>=nq||strcmp(line,queue[j])){if(c<200)fprintf(stderr,"case%d queue%d C:%sJS:%s",c,j,j<nq?queue[j]:"missing\n",line);ok=false;}}
  for(int b=0;b<4;b++){pr_reset_summon_data(&mons[b]);double v[WIDTH+1];read_vector("Z ",v,WIDTH+1);if(v[0]!=b)bad();ok&=compare(&mons[b],v+1,c,b,'Z');}
  if(strcmp(line_read("E"),"\n"))bad();
  if(bt.n_unported||bt.n_deferred||e.overflow){if(c<200)fprintf(stderr,"case%d unsupported=%d deferred=%d overflow=%d\n",c,bt.n_unported,bt.n_deferred,e.overflow);ok=false;}
  if(!ok){if(failures<12)fprintf(stderr,"suppression case%d mode%d target%d failed\n",c,mode,target);failures++;}
 }
 double end[1];read_vector("SUPPRESSION_COMPLETE ",end,1);if(end[0]!=n||fgetc(input)!=EOF)bad();fclose(input);
 printf("Suppression API: %d complete cases, %d exact, %d failures\n",n,n-failures,failures);return failures?1:0;
}
