/* Actual raw override state/queues/RNG. The C effect interface returns void. */
#define main gas_replay_main
#define emit gas_emit
#include "gas-lifecycle-test.c"
#undef emit
#undef main
static int meta[128][4];static pr_pokemon *actors;
static void emit(const pr_phase *p,char scope,int level,int j){
 int index=nq;gas_emit(p,scope,level,j);for(int k=0;k<4;k++)meta[index][k]=-1;
 if(p->type==PH_DAMAGE_ANIM){meta[index][0]=p->anim_amount;meta[index][1]=p->anim_result;meta[index][2]=p->anim_critical;}
 if(p->type==PH_SWITCH){meta[index][0]=p->sw_type;meta[index][1]=p->sw_field_index;meta[index][2]=p->sw_modal;meta[index][3]=p->sw_do_return;}
 if(p->type==PH_FAINT){meta[index][0]=p->prevent_instant_revive;meta[index][1]=p->source_p?(int)(p->source_p-actors):-1;meta[index][2]=0;}
 if(p->type==PH_STAT_STAGE_CHANGE){meta[index][0]=p->source_p?(int)(p->source_p-actors):-1;meta[index][1]=p->ssc_ignore_abilities;meta[index][2]=p->ssc_self_target;}
}
static void extras(pr_pokemon *p,int b,bool initial,bool *ok){
 double x[7];read_vector(initial?"IX ":"OX ",x,7);if(x[0]!=b)bad();int segments=integer(x[1],0,5),index=integer(x[2],0,4),enduring=integer(x[3],0,1),sub=integer(x[4],-1,INT32_MAX);
 if((b<2&&segments)||index>=(segments?segments:1))bad();
 int hit_count=integer(x[5],0,127),hits_left=integer(x[6],-1,127);
 if(initial){p->hit_count=hit_count;p->hits_left=hits_left;p->is_boss=segments>0;p->boss_segments=segments;p->boss_segment_index=index;
  if(enduring)p->tags[p->n_tags++]=(pr_battler_tag){.type=BATTLER_TAG_TYPE_ENDURING,.turn_count=1,.source_move=MOVE_ID_ENDURE,.source_id=p->id};
  pr_battler_tag *tag=(pr_battler_tag *)pr_get_battler_tag(p,BATTLER_TAG_TYPE_SUBSTITUTE);if((sub>=0)!=!!tag)bad();if(tag)tag->hp=sub;
 }else{
  const pr_battler_tag *tag=pr_get_battler_tag(p,BATTLER_TAG_TYPE_SUBSTITUTE);
  if(p->hit_count!=hit_count||p->hits_left!=hits_left)*ok=false;
  if(p->boss_segments!=segments||p->boss_segment_index!=index||!!pr_get_battler_tag(p,BATTLER_TAG_TYPE_ENDURING)!=enduring||(tag?tag->hp:-1)!=sub)*ok=false;
 }
}
static void modifiers(pr_battle *bt,bool initial,bool *ok){
 double n[1];read_vector("MODS ",n,1);int count=integer(n[0],0,3),seen[2]={0,0};
 for(int k=0;k<count;k++){double m[4];read_vector("M ",m,4);int side=integer(m[0],0,1),stack=integer(m[2],1,2),stat=integer(m[3],STAT_ATK,STAT_SPD);if(m[1]<0||m[1]>UINT32_MAX||floor(m[1])!=m[1])bad();
  if(initial){bt->mods[side][bt->n_mods[side]++]=(pr_modifier){.cls=PR_MOD_BASE_STAT,.pokemon_id=(int64_t)m[1],.stack_count=stack,.stat=stat};}
  else{int index=seen[side];if(index>=bt->n_mods[side])*ok=false;else{const pr_modifier *mod=&bt->mods[side][index];if(mod->cls!=PR_MOD_BASE_STAT||mod->pokemon_id!=(int64_t)m[1]||mod->stack_count!=stack||mod->stat!=stat)*ok=false;}}
  seen[side]++;
 }
 if(!initial&&(seen[0]!=bt->n_mods[0]||seen[1]!=bt->n_mods[1]))*ok=false;
}
int main(int argc,char **argv){
 if(argc!=2||!getenv("TURNTEST_LOCALES"))return 2;input=fopen(argv[1],"r");if(!input)return 2;pr_closures_init();
 double h[3];read_vector("FLAMEBURST ",h,3);if(h[0]!=1)bad();int n=integer(h[1],16384,100000),seed=integer(h[2],0,INT32_MAX),failures=0;
 const pr_move *m=&pr_moves[MOVE_ID_FLAME_BURST];int attr=-1;for(int j=0;j<m->nattr;j++)if(pr_attrs[m->attr0+j].cls==PR_AC_FlameBurstAttr)attr=m->attr0+j;if(attr<0)bad();
 for(int c=0;c<n;c++){
  static pr_battle bt;static pr_engine e;static pr_pokemon mons[4];memset(&bt,0,sizeof bt);memset(&e,0,sizeof e);bt.engine=&e;e.n_levels=1;e.current=-1;bt.game_mode=GAME_MODES_CLASSIC;bt.ignoring_effect_source=-1;bt.wave_index=3;bt.turn=1;bt.last_used_pokeball=-1;actors=mons;
  char wave[64],battle[64];snprintf(wave,sizeof wave,"flame-burst-wave-%d",seed);snprintf(battle,sizeof battle,"flame-burst-%d-%d",seed,c);pr_seed_from_ascii(&bt.wave_seed,wave);pr_seed_from_ascii(&bt.battle_seed,battle);
  double row[4];read_vector("C ",row,4);if(row[0]!=c)bad();int holder=integer(row[1],0,3),target=integer(row[2],0,3);bt.double_battle=integer(row[3],0,1);
  double g[2];read_vector("G ",g,2);int initial=integer(g[0],0,3);if(g[1]<0||g[1]>UINT32_MAX||floor(g[1])!=g[1])bad();
  if(initial){bt.n_arena_tags=1;bt.arena_tags[0]=(pr_arena_tag){.type=ARENA_TAG_TYPE_NEUTRALIZING_GAS,.side=ARENA_TAG_SIDE_BOTH,.source_id=(int64_t)g[1],.source_count=initial,.layers=1};}
  bool ok=true;modifiers(&bt,true,&ok);
  rng_read("R ",&bt.rng);rng_read("B ",&bt.battle_rng);bt.rng_live=bt.battle_rng_live=true;
  for(int b=0;b<4;b++){double v[WIDTH+1];read_vector("I ",v,WIDTH+1);if(v[0]!=b)bad();load(&mons[b],v+1,b);bt.field[b]=b%2&&!bt.double_battle?NULL:&mons[b];bt.party[b/2][b%2]=&mons[b];bt.n_party[b/2]++;ok&=compare(&mons[b],v+1,c,b,'I');extras(&mons[b],b,true,&ok);}
  bt.participants[0]=mons[0].id;bt.participants[1]=mons[1].id;bt.n_participants=2;
  int ph=pr_phase_new(&e,PH_MOVE_EFFECT,holder);e.pool[ph].move_id=MOVE_ID_FLAME_BURST;pr_apply_move_effect(&bt,&e,ph,attr,&mons[holder],&mons[target]);
  double ret[1];read_vector("RESULT ",ret,1);integer(ret[0],0,1); /* recorded source boolean; C exposes state/queues */
  double world[11];read_vector("WORLD ",world,11);const pr_arena_tag *gas=pr_arena_get_tag_on_side(&bt,ARENA_TAG_TYPE_NEUTRALIZING_GAS,ARENA_TAG_SIDE_BOTH);uint32_t ids[2]={mons[0].id,mons[1].id};if(ids[0]>ids[1]){uint32_t v=ids[0];ids[0]=ids[1];ids[1]=v;}
  double actual[]={bt.n_arena_tags,gas?gas->source_count:0,gas?gas->being_removed:0,bt.player_faints,bt.enemy_faints,bt.battle_score,bt.post_battle_loot,bt.ability_bar_visible,bt.n_participants,ids[0],ids[1]};for(int k=0;k<11;k++)if(actual[k]!=world[k]){if(failures<4)fprintf(stderr,"case%d world%d C%g JS%g\n",c,k,actual[k],world[k]);ok=false;}
  modifiers(&bt,false,&ok);
  for(int b=0;b<4;b++){double v[WIDTH+1];read_vector("O ",v,WIDTH+1);if(v[0]!=b)bad();bool same=compare(&mons[b],v+1,c,b,'O');if(!same&&failures<4){double got[WIDTH];capture(&mons[b],got);for(int k=0;k<WIDTH;k++)if(got[k]!=v[k+1])fprintf(stderr,"case%d actor%d field%d C%g JS%g\n",c,b,k,got[k],v[k+1]);}ok&=same;extras(&mons[b],b,false,&ok);}
  pr_rng want;rng_read("RO ",&want);if(!rng_equal(want,bt.rng)){if(failures<4)fprintf(stderr,"case%d global RNG differs\n",c);ok=false;}rng_read("BO ",&want);if(!rng_equal(want,bt.battle_rng)){if(failures<4)fprintf(stderr,"case%d battle RNG differs\n",c);ok=false;}
  double qr[3];read_vector("QCOUNT ",qr,3);int count=integer(qr[0],0,128),levels=integer(qr[1],1,PR_MAX_LEVELS),deferred=integer(qr[2],0,1);if(levels!=e.n_levels||deferred!=e.deferred_active)ok=false;nq=0;
  for(int l=0;l<e.n_levels;l++)for(int j=0;j<e.level_len[l];j++)emit(&e.pool[e.levels[l][j]],'L',l,j);
  int types[PH_COUNT_],nt=0;for(int t=0;t<PH_COUNT_;t++)if(e.n_dyn[t])types[nt++]=t;for(int j=1;j<nt;j++)for(int k=j;k>0&&strcmp(pr_phase_name[types[k]],pr_phase_name[types[k-1]])<0;k--){int t=types[k];types[k]=types[k-1];types[k-1]=t;}for(int k=0;k<nt;k++)for(int j=0;j<e.n_dyn[types[k]];j++)emit(&e.pool[e.dyn[types[k]][j]],'D',k,j);
  if(nq!=count)ok=false;
  for(int j=0;j<count;j++){
   line_read("Q ");queue_numbers();char scope,name[96],hex[2048],stages[128],extra;int group,order,battler,passive;
   if(sscanf(line,"Q %c %d %d %95s %d %d %2047s %127s %c",&scope,&group,&order,name,&battler,&passive,hex,stages,&extra)!=8||(scope!='L'&&scope!='D')||group<0||group>=PR_MAX_PHASES||order<0||order>=PR_MAX_PHASES||battler< -1||battler>3)bad();
   const char *phase=!strncmp(name,"Marker:",7)?name+7:name;bool known=false;for(int t=0;t<PH_COUNT_;t++)if(pr_phase_name[t]&&!strcmp(phase,pr_phase_name[t]))known=true;if(!known)bad();
   if(!strcmp(name,"ShowAbilityPhase")){if(passive<0||passive>1)bad();}else if(passive!=-1)bad();
   if(strcmp(hex,"-")){size_t n=strlen(hex);if(n%2)bad();for(size_t k=0;k<n;k++)if(!isxdigit((unsigned char)hex[k]))bad();}
   if(strcmp(stages,"-")){const char *at=stages;while(*at){char *end;long stat=strtol(at,&end,10);if(end==at||*end!=':'||stat<STAT_ATK||stat>STAT_EVA)bad();at=end+1;long amount=strtol(at,&end,10);if(end==at||amount< -128||amount>127||(*end&&*end!=','))bad();at=*end?end+1:end;if(*end==','&&!*at)bad();}}
   if(j>=nq||strcmp(line,queue[j])){if(failures<4)fprintf(stderr,"case%d queue%d C:%sJS:%s",c,j,j<nq?queue[j]:"missing\n",line);ok=false;}
   double data[4];read_vector("P ",data,4);
   if(!strcmp(name,"DamageAnimPhase")){integer(data[0],0,INT32_MAX);integer(data[1],HIT_RESULT_INDIRECT,HIT_RESULT_INDIRECT);integer(data[2],0,0);integer(data[3],-1,-1);}
   else if(!strcmp(name,"FaintPhase")){integer(data[0],0,1);integer(data[1],-1,3);integer(data[2],0,0);integer(data[3],-1,-1);}
   else if(!strcmp(name,"StatStageChangePhase")){integer(data[0],-1,3);integer(data[1],0,1);integer(data[2],0,1);integer(data[3],-1,-1);}
   else if(!strcmp(name,"SwitchPhase")){integer(data[0],SWITCH_TYPE_SWITCH,SWITCH_TYPE_SWITCH);integer(data[1],0,1);integer(data[2],0,1);integer(data[3],0,1);}
   else for(int k=0;k<4;k++)integer(data[k],-1,-1);
   for(int k=0;k<4;k++){integer(data[k],-1,INT32_MAX);if(j>=nq||meta[j][k]!=data[k]){if(failures<4)fprintf(stderr,"case%d phase%d meta%d C%d JS%g\n",c,j,k,j<nq?meta[j][k]:-999,data[k]);ok=false;}}
  }
  if(strcmp(line_read("E"),"\n"))bad();
  if(bt.n_unported||bt.n_deferred||e.overflow){if(failures<4)fprintf(stderr,"case%d unsupported%d deferred%d overflow%d\n",c,bt.n_unported,bt.n_deferred,e.overflow);ok=false;}
  if(!ok){if(failures<15)fprintf(stderr,"Flame Burst case%d holder%d target%d failed\n",c,holder,target);failures++;}
 }
 double end[1];read_vector("FLAMEBURST_COMPLETE ",end,1);if(end[0]!=n||fgetc(input)!=EOF)bad();printf("Flame Burst API: %d complete, %d exact, %d failures\n",n,n-failures,failures);return failures?1:0;
}
