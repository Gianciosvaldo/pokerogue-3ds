/* Strict public arena owner replay; native captures, not generated expectations. */
#define TAILWIND_OWNER_NO_MAIN
#include "tailwind-owner-test.c"
#undef main
static void tags(int c,pr_battle*bt,bool*success){
 bool ok=*success;double count[1];read_vector("TAGS ",count,1);int n=integer(count[0],0,48);EQ(bt->n_arena_tags,n);
 for(int i=0;i<n;i++){double a[10];read_vector("TAG ",a,10);for(int j=0;j<10;j++)if(!isfinite(a[j])||floor(a[j])!=a[j])bad();
 if(a[0]<1||a[0]>ARENA_TAG_TYPE_PENDING_HEAL||a[1]<0||a[1]>2||a[2]<0||a[2]>5||a[3]<0||a[3]>5||a[4]<0||a[4]>=PR_MOVE_COUNT||a[5]<-1||a[5]>UINT32_MAX||(a[6]<1||a[6]>3)||a[7]<0||a[7]>1||a[8]!=0||a[9]!=0)bad();
 if(i>=bt->n_arena_tags){ok=false;continue;}const pr_arena_tag*t=&bt->arena_tags[i];double got[]={t->type,t->side,t->turn_count,t->max_duration,t->source_move,(double)t->source_id,t->layers,t->source_count,t->being_removed,t->n_pending_heals};for(int j=0;j<10;j++)EQ(got[j],a[j]);}
 *success=ok;
}
int main(int argc,char**argv){
 if(argc!=2)return 2;input=fopen(argv[1],"r");if(!input||strcmp(line_read("ARENA_CAPACITY "),"1 32\n"))bad();char*w=line_read("W ");w[strcspn(w,"\n")]=0;pr_seedstr wave;pr_seed_from_ascii(&wave,w);int failed=0;
 const pr_move*move=&pr_moves[MOVE_ID_COURT_CHANGE];int attr=-1;for(int j=0;j<move->nattr;j++)if(pr_attrs[move->attr0+j].cls==PR_AC_SwapArenaTagsAttr)attr=move->attr0+j;if(attr<0)bad();
 for(int c=0;c<32;c++){
  double meta[4];read_vector("C ",meta,4);if(meta[0]!=c||meta[1]!=c/8||meta[2]!=(c/4)%2||meta[3]!=c%4)bad();int mode=meta[3];bool ok=true;
  static pr_battle bt;static pr_engine e;memset(&bt,0,sizeof bt);memset(&e,0,sizeof e);e.n_levels=1;e.current=-1;bt.engine=&e;bt.double_battle=true;bt.game_mode=GAME_MODES_CLASSIC;bt.wave_index=1;bt.challenge_flags_known=true;bt.ignoring_effect_source=-1;bt.max_exp_level=100;bt.wave_seed=wave;bt.turn=1;
  rng_read("R ",&bt.rng);rng_read("B ",&bt.battle_rng);bt.rng_live=bt.battle_rng_live=true;
  for(int side=0;side<2;side++){bt.n_party[side]=3;for(int k=0;k<3;k++)bt.party[side][k]=&actors[side*3+k];for(int k=0;k<2;k++)bt.field[side*2+k]=bt.party[side][k];}
  frame(&bt,c,"I",true,&ok);
  for(int k=0;k<6;k++)if(actors[k].ability!=ABILITY_ID_NONE||actors[k].passive_flag||actors[k].on_field!=(k%3<2)||actors[k].hp!=actors[k].stats[STAT_HP]||actors[k].stats[STAT_SPD]!=10+k*10||actors[k].n_tags)bad();
  double count[1];read_vector("OPS ",count,1);if(count[0]!=54)bad();unsigned seen[29][3]={{0}};int32_t all_types[28];for(int k=0;k<28;k++)all_types[k]=k+1;
  for(int k=0;k<54;k++){
   double a[4];read_vector("ADD ",a,4);int type=integer(a[0],1,28),side=integer(a[1],0,2);if(a[2]!=(side==ARENA_TAG_SIDE_ENEMY?104:101)||a[3]!=(seen[type][side]?0:1))bad();seen[type][side]++;if(seen[type][side]>(type==ARENA_TAG_TYPE_SPIKES?3:type==ARENA_TAG_TYPE_TOXIC_SPIKES?2:1))bad();
   bool got=pr_arena_add_tag(&bt,&e,type,5,MOVE_ID_SPLASH,(int64_t)a[2],side,true);EQ(got,a[3]);
  }
  for(int type=1;type<=28;type++)if(!(seen[type][0]?!seen[type][1]&&!seen[type][2]:seen[type][1]&&seen[type][2]))bad();
  tags(c,&bt,&ok);
  if(mode==1){int ph=pr_phase_new(&e,PH_MOVE_EFFECT,0);e.pool[ph].move_id=MOVE_ID_COURT_CHANGE;pr_apply_move_effect(&bt,&e,ph,attr,bt.field[0],bt.field[0]);}
  if(mode==2)pr_arena_remove_tags_on_side(&bt,&e,all_types,28,ARENA_TAG_SIDE_PLAYER,true);
  if(mode==3)pr_arena_remove_all_tags(&bt,&e);
  frame(&bt,c,"O",false,&ok);
  for(int k=0;k<6;k++){double a[2];read_vector("BTAGS ",a,2);if(a[0]!=k)bad();int n=integer(a[1],0,PR_MAX_BATTLER_TAGS);EQ(actors[k].n_tags,n);for(int j=0;j<n;j++){double b[4];read_vector("BTAG ",b,4);if(b[0]!=BATTLER_TAG_TYPE_IMPRISON||b[1]<0||b[1]>1||b[2]!=MOVE_ID_IMPRISON||b[3]<101||b[3]>106)bad();if(j>=actors[k].n_tags){ok=false;continue;}const pr_battler_tag*t=&actors[k].tags[j];EQ(t->type,b[0]);EQ(t->turn_count,b[1]);EQ(t->source_move,b[2]);EQ(t->source_id,b[3]);}}
  tags(c,&bt,&ok);pr_rng want;rng_read("RO ",&want);EQ(rng_equal(want,bt.rng),true);rng_read("BO ",&want);EQ(rng_equal(want,bt.battle_rng),true);
  double q[3];read_vector("QCOUNT ",q,3);int n=integer(q[0],0,128);EQ(e.n_levels,integer(q[1],1,PR_MAX_LEVELS));EQ(e.deferred_active,integer(q[2],0,1));nq=0;for(int l=0;l<e.n_levels;l++)for(int j=0;j<e.level_len[l];j++)emit(&e.pool[e.levels[l][j]],'L',l,j);
  int types[PH_COUNT_],nTypes=0;for(int t=0;t<PH_COUNT_;t++)if(e.n_dyn[t])types[nTypes++]=t;for(int j=1;j<nTypes;j++)for(int k=j;k>0&&strcmp(pr_phase_name[types[k]],pr_phase_name[types[k-1]])<0;k--){int t=types[k];types[k]=types[k-1];types[k-1]=t;}for(int k=0;k<nTypes;k++)for(int j=0;j<e.n_dyn[types[k]];j++)emit(&e.pool[e.dyn[types[k]][j]],'D',k,j);
  EQ(nq,n);for(int j=0;j<n;j++){line_read("Q ");char scope,name[96],hex[2048],changes[128],extra;int group,order,b,passive,ability,person;
  if(sscanf(line,"Q %c %d %d %95s %d %d %d %2047s %127s %d %c",&scope,&group,&order,name,&b,&passive,&ability,hex,changes,&person,&extra)!=10||(scope!='L'&&scope!='D')||group<0||group>=PR_MAX_LEVELS||order<0||order>=PR_MAX_PHASES||b< -1||b>3||passive< -1||passive>1||ability< -1||ability>=PR_ABILITY_COUNT||person< -1||person>5)bad();
  bool known=false;const char*phase_name=!strncmp(name,"Marker:",7)?name+7:name;for(int t=0;t<PH_COUNT_;t++)if(pr_phase_name[t]&&!strcmp(pr_phase_name[t],phase_name))known=true;if(!known)bad();
  if(strcmp(hex,"-")&&(strlen(hex)%2||strspn(hex,"0123456789abcdef")!=strlen(hex)))bad();
  if(strcmp(changes,"-")){const char*cursor=changes;do{char*end;long stat=strtol(cursor,&end,10);if(cursor==end||*end!=':'||stat<1||stat>7)bad();cursor=end+1;long amount=strtol(cursor,&end,10);if(cursor==end||amount< -12||amount>12||(*end&&*end!=','))bad();cursor=*end?end+1:end;}while(*cursor);}
  if(j>=nq){ok=false;continue;}if(strcmp(line,queue[j])&&(c<3||c==16))fprintf(stderr,"queue C:%sJS:%s",queue[j],line);EQ(strcmp(line,queue[j]),0);}if(strcmp(line_read("E"),"\n"))bad();EQ(bt.n_unported,0);EQ(e.overflow,false);if(!ok)failed++;
 }
 if(strcmp(line_read("ARENA_CAPACITY_COMPLETE "),"32\n")||fgetc(input)!=EOF)bad();
 /* Imported duplicates can exceed the public key basis: keep overflow explicit. */
 static pr_battle invalid;static pr_engine guard;guard.n_levels=1;guard.current=-1;invalid.engine=&guard;invalid.n_arena_tags=PR_MAX_ARENA_TAGS;
 for(int k=0;k<PR_MAX_ARENA_TAGS;k++)invalid.arena_tags[k]=(pr_arena_tag){.type=ARENA_TAG_TYPE_SPIKES,.side=ARENA_TAG_SIDE_PLAYER,.layers=1};
 bool added=pr_arena_add_tag(&invalid,&guard,ARENA_TAG_TYPE_MIST,5,MOVE_ID_MIST,-1,ARENA_TAG_SIDE_PLAYER,true);
 if(added||invalid.n_arena_tags!=PR_MAX_ARENA_TAGS||!guard.overflow||invalid.n_unported!=1||invalid.unported[0]!=PR_FEAT_ENGINE_LIMIT){fprintf(stderr,"invalid imported arena lost overflow guard\n");return 1;}
 printf("Arena capacity: 32 cases, %u checks, %d mismatches\n",checks,failed);return failed?1:0;
}
