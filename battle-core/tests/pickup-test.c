/* Source-observation transport: every required row, stock, queue and RNG state. */
#define main retained_gas_replay
#include "gas-lifecycle-test.c"
#undef main
static int version;
static pr_battle bt;static pr_engine e;static pr_pokemon actors[4];static uint32_t ids[4];
static const char *keys[]={"LEFTOVERS","SHELL_BELL","FOCUS_BAND","QUICK_CLAW","KINGS_ROCK","WHITE_HERB","MINI_BLACK_HOLE","BERRY","WIDE_LENS","MULTI_LENS","SCOPE_LENS","TOXIC_ORB","FLAME_ORB","SOUL_DEW","GOLDEN_PUNCH","ATTACK_TYPE_BOOSTER","REVIVER_SEED","BATON","MYSTICAL_ROCK","LEEK","LUCKY_EGG","GOLDEN_EGG","SOOTHE_BELL"};
static const int classes[]={PR_MOD_TURN_HEAL,PR_MOD_HIT_HEAL,PR_MOD_SURVIVE_DAMAGE,PR_MOD_BYPASS_SPEED_CHANCE,PR_MOD_FLINCH_CHANCE,PR_MOD_RESET_NEGATIVE_STAT_STAGE,PR_MOD_TURN_HELD_TRANSFER,PR_MOD_BERRY,PR_MOD_MOVE_ACCURACY_BOOSTER,PR_MOD_MULTI_HIT,PR_MOD_CRIT_BOOSTER,PR_MOD_TURN_STATUS_EFFECT,PR_MOD_TURN_STATUS_EFFECT,PR_MOD_NATURE_WEIGHT,PR_MOD_DAMAGE_MONEY_REWARD,PR_MOD_ATTACK_TYPE_BOOSTER,PR_MOD_INSTANT_REVIVE,PR_MOD_SWITCH_EFFECT_TRANSFER,PR_MOD_FIELD_EFFECT,PR_MOD_SPECIES_CRIT_BOOSTER,PR_MOD_EXP_BOOSTER,PR_MOD_EXP_BOOSTER,PR_MOD_FRIENDSHIP_BOOSTER};
static pr_modifier item(void){
 char key[80],cls[80],extra;int ref,arg,owner,stack,virtual,transfer,max;
 if(sscanf(line_read("M "),"%d %79s %d %d %d %d %d %d %79s %c",&ref,key,&arg,&owner,&stack,&virtual,&transfer,&max,cls,&extra)!=9||ref<0||ref>PR_MAX_MODIFIERS||owner< -1||owner>3||stack<0||stack>100||virtual<0||virtual>3||transfer<0||transfer>1||max<0||max>99)bad();
 int found=-1;for(unsigned i=0;i<sizeof classes/sizeof *classes;i++)if(!strcmp(keys[i],key))found=(int)i;if(found<0)bad();
 pr_modifier m={.cls=classes[found],.loot_ref=ref,.pokemon_id=owner<0?0:ids[owner],.stack_count=stack,.virtual_stack_count=virtual,.is_transferable=transfer};
 if(strcmp(cls,pr_mod_class_name[m.cls]))bad();
 if(m.cls==PR_MOD_BERRY){if(arg<0||arg>10)bad();m.berry_type=arg;}
 else if(m.cls==PR_MOD_ATTACK_TYPE_BOOSTER){if(arg<0||arg>17)bad();m.move_type=arg;m.mult=.2;}
 else if(arg!=-1)bad();
 if(m.cls==PR_MOD_MOVE_ACCURACY_BOOSTER)m.num=5;
 if(m.cls==PR_MOD_CRIT_BOOSTER)m.num=1;
 if(m.cls==PR_MOD_TURN_STATUS_EFFECT)m.status_effect=!strcmp(key,"TOXIC_ORB")?STATUS_EFFECT_TOXIC:STATUS_EFFECT_BURN;
 if(m.cls==PR_MOD_SPECIES_CRIT_BOOSTER){m.num=2;m.n_species=3;m.species[0]=SPECIES_ID_FARFETCHD;m.species[1]=SPECIES_ID_GALAR_FARFETCHD;m.species[2]=SPECIES_ID_SIRFETCHD;}
 if(m.cls==PR_MOD_EXP_BOOSTER)m.mult=!strcmp(key,"LUCKY_EGG")?.4:1.0;
 return m;
}
static bool equal(const pr_modifier *m,const pr_modifier *n){
 return m->cls==n->cls&&m->loot_ref==n->loot_ref&&m->pokemon_id==n->pokemon_id&&m->stack_count==n->stack_count&&m->virtual_stack_count==n->virtual_stack_count&&m->is_transferable==n->is_transferable&&m->berry_type==n->berry_type&&m->move_type==n->move_type&&m->status_effect==n->status_effect&&m->num==n->num&&m->mult==n->mult&&m->n_species==n->n_species&&!memcmp(m->species,n->species,sizeof m->species);
}
static long token(char **p,int lo,int hi){char *end;errno=0;long v=strtol(*p,&end,10);if(end==*p||errno||v<lo||v>hi)bad();*p=end;return v;}
static void end_tokens(char *p){while(isspace((unsigned char)*p))p++;if(*p)bad();}
static bool history(pr_pokemon *p,char **row,bool set){
 bool ok=true;int n=(int)token(row,0,4);if(set)p->n_ab_applied_wave=n;else ok&=p->n_ab_applied_wave==n;
 for(int i=0;i<n;i++){int ab=(int)token(row,0,PR_ABILITY_COUNT-1);if(set)p->ab_applied_wave[i]=ab;else ok&=p->ab_applied_wave[i]==ab;}
 n=(int)token(row,0,4);if(set)p->n_ab_applied_summon=n;else ok&=p->n_ab_applied_summon==n;
 for(int i=0;i<n;i++){int ab=(int)token(row,0,PR_ABILITY_COUNT-1);if(set)p->ab_applied_summon[i]=ab;else ok&=p->ab_applied_summon[i]==ab;}return ok;
}
static bool stock_frame(char prefix,bool set){
 bool ok=true;char label[8];
 if(version==2){snprintf(label,sizeof label,"%cM ",prefix);double money[1];read_vector(label,money,1);if(!isfinite(money[0]))bad();if(set)bt.money_scattered=money[0];else ok&=bt.money_scattered==money[0];}
 snprintf(label,sizeof label,"%c ",prefix);
 for(int side=0;side<2;side++){
  double v[2];read_vector(label,v,2);if(v[0]!=side)bad();int count=integer(v[1],0,PR_MAX_MODIFIERS);
  if(set)bt.n_mods[side]=count;else ok&=bt.n_mods[side]==count;
  for(int k=0;k<count;k++){pr_modifier m=item();if(set){bt.mods[side][k]=m;if(m.loot_ref>bt.loot_ref_max)bt.loot_ref_max=m.loot_ref;}else if(k>=bt.n_mods[side]||!equal(&bt.mods[side][k],&m))ok=false;}
 }
 snprintf(label,sizeof label,"%cL ",prefix);char *row=line_read(label);int count=(int)token(&row,0,PR_MAX_MODIFIERS);
 if(set)bt.post_battle_loot=count;else ok&=bt.post_battle_loot==count;
 for(int k=0;k<count;k++){int ref=(int)token(&row,1,PR_MAX_MODIFIERS);if(set)bt.post_battle_loot_refs[k]=ref;else ok&=bt.post_battle_loot_refs[k]==ref;}end_tokens(row);
 snprintf(label,sizeof label,"%cC ",prefix);double cache[1];read_vector(label,cache,1);int ref=integer(cache[0],0,PR_MAX_MODIFIERS);if(set)bt.pickup_cached_ref=ref;else ok&=bt.pickup_cached_ref==ref;
 pr_rng rng;snprintf(label,sizeof label,"%cR ",prefix);rng_read(label,&rng);if(set)bt.rng=rng;else ok&=rng_equal(bt.rng,rng);
 snprintf(label,sizeof label,"%cB ",prefix);rng_read(label,&rng);if(set)bt.battle_rng=rng;else ok&=rng_equal(bt.battle_rng,rng);
 return ok;
}
static bool queues(void){
 bool ok=true;double v[2];read_vector("LEVELS ",v,1);int nl=integer(v[0],1,PR_MAX_LEVELS);ok&=e.n_levels==nl;
 for(int l=0;l<nl;l++){read_vector("L ",v,2);if(v[0]!=l)bad();int count=integer(v[1],0,PR_LEVEL_CAP);ok&=l<e.n_levels&&e.level_len[l]==count;
  for(int j=0;j<count;j++){
   char name[80],hex[4096],extra;int who,passive,prompt;
   if(sscanf(line_read("P "),"%79s%d%d%d%4095s %c",name,&who,&passive,&prompt,hex,&extra)!=5||who< -1||who>3||passive< -1||passive>1||prompt< -1||prompt>1)bad();
   if(l>=e.n_levels||j>=e.level_len[l]){ok=false;continue;}
   pr_phase *p=&e.pool[e.levels[l][j]];char actual[2048],got[4096]="-";int cp=-1,cpr=-1;
   if(p->type==PH_MESSAGE){pr_text_source src={locale_get,NULL,"en"};size_t n=pr_msg_format(&p->msg,&src,actual,sizeof actual);if(n>=sizeof actual)bad();if(n)for(size_t k=0;k<n;k++)sprintf(got+2*k,"%02x",(unsigned char)actual[k]);cpr=p->message_prompt;}
   if(p->type==PH_SHOW_ABILITY)cp=p->anim_result;
   /* Absent MessagePhase.prompt and false have the same native default. */
   if(p->type==PH_MESSAGE&&prompt<0)prompt=0;
   bool match=!strcmp(name,pr_phase_name[p->type])&&who==p->battler&&passive==cp&&prompt==cpr&&!strcmp(hex,got);
   if(!match&&lineno<120)fprintf(stderr,"queue native%s %d %d %d %s C%s %d %d %d %s\n",name,who,passive,prompt,hex,pr_phase_name[p->type],p->battler,cp,cpr,got);
   ok&=match;
  }
 }return ok;
}
int main(int argc,char **argv){
 if(argc!=2||!getenv("TURNTEST_LOCALES"))return 2;input=fopen(argv[1],"r");if(!input)return 2;pr_closures_init();double h[2];read_vector("PICKUP ",h,2);version=integer(h[0],1,2);int total=version==1?4000:4400;if(h[1]!=total)bad();int fail=0,modes[4]={0},shapes[8]={0};
 if(version==2)for(int k=0;k<23;k++){
  if(classes[k]==PR_MOD_BERRY||classes[k]==PR_MOD_ATTACK_TYPE_BOOSTER)continue;
  char key[80],extra;int wild,trainer;if(sscanf(line_read("T "),"%79s%d%d %c",key,&wild,&trainer,&extra)!=3||strcmp(key,keys[k]))bad();
  pr_modifier m={.cls=classes[k]};if(m.cls==PR_MOD_EXP_BOOSTER)m.mult=k==20?.4:1.0;
  if(pr_held_item_tier_for_pool(&m,false)!=wild||pr_held_item_tier_for_pool(&m,true)!=trainer){fprintf(stderr,"tier %s native %d/%d C %d/%d\n",key,wild,trainer,pr_held_item_tier_for_pool(&m,false),pr_held_item_tier_for_pool(&m,true));fail++;}
 }
 for(int i=0;i<total;i++){
  memset(&bt,0,sizeof bt);memset(&e,0,sizeof e);memset(actors,0,sizeof actors);bt.engine=&e;bt.double_battle=true;bt.last_used_pokeball=-1;bt.ignoring_effect_source=-1;bt.rng_live=bt.battle_rng_live=true;e.current=-1;e.n_levels=1;
  double c[6];read_vector("C ",c,6);if(c[0]!=i||(i<4000?(c[1]!=(i/4)%20||c[2]!=i%4):(c[1]!=1||c[2]!=3)))bad();int mode=integer(c[2],0,3),victory=integer(c[3],0,1),simulated=integer(c[4],0,1),result=integer(c[5],-1,1);modes[mode]++;
  char variant[80],extra;int vararg;if(sscanf(line_read("V "),"%79s%d %c",variant,&vararg,&extra)!=2)bad();
  int vi=i<4000?i/80:(i-4000)/8;int ordinal=0;const char *wanted=NULL;int arg=-1;for(unsigned k=0;k<sizeof classes/sizeof *classes;k++){int n=classes[k]==PR_MOD_BERRY?11:classes[k]==PR_MOD_ATTACK_TYPE_BOOSTER?18:1;if(vi>=ordinal&&vi<ordinal+n){wanted=keys[k];arg=n>1?vi-ordinal:-1;break;}ordinal+=n;}if(!wanted||strcmp(wanted,variant)||arg!=vararg)bad();
  char *seed=line_read("W ");seed[strcspn(seed,"\n")]=0;pr_seed_from_ascii(&bt.wave_seed,seed);double turn[1];read_vector("TURN ",turn,1);bt.turn=integer(turn[0],1,100000);
  int shape=-1;if(version==2){double x[2];read_vector("X ",x,2);shape=integer(x[0],-1,7);if((i<4000&&shape!=-1)||(i>=4000&&shape!=(i-4000)%8))bad();if(shape>=0)shapes[shape]++;bt.wave_index=integer(x[1],1,5850);}
  for(int k=0;k<4;k++){
   double m[20];read_vector("MON ",m,20);pr_pokemon *p=&actors[k];if(m[0]<0||m[0]>UINT32_MAX||m[0]!=floor(m[0]))bad();p->id=ids[k]=(uint32_t)m[0];p->species=integer(m[1],1,65535);p->form=integer(m[2],0,255);p->level=integer(m[3],1,255);p->nature=integer(m[4],0,24);p->hp=integer(m[5],0,100000);p->ability=p->base_ability=integer(m[6],0,PR_ABILITY_COUNT-1);p->has_trainer=integer(m[7],0,1);p->is_player=k<2;p->on_field=true;p->ability_override=-1;p->added_type=-1;for(int n=0;n<6;n++){p->ivs[n]=integer(m[n+8],0,31);p->stats[n]=integer(m[n+14],1,100000);}bt.field[k]=bt.party[k/2][k%2]=p;bt.n_party[k/2]=2;
   if(version==2){double f[3];read_vector("F ",f,3);p->passive=integer(f[0],0,PR_ABILITY_COUNT-1);p->passive_flag=p->passive!=0;p->custom_passive=p->passive;p->ability_suppressed=integer(f[1],0,1);p->on_field=integer(f[2],0,1);}
   char *row=line_read("H ");history(p,&row,true);end_tokens(row);
  }
  if(shape==6){bt.party[0][0]=bt.field[0]=&actors[1];bt.party[0][1]=bt.field[1]=&actors[0];}
  stock_frame('I',true);int got=-1;
  if(mode==0)got=pr_pickup_can_apply(&bt,&actors[0],simulated,victory);
  else if(mode==1)pr_pickup_apply(&bt,&e,&actors[0]);
  else if(mode==2){pr_ab_params p={.pokemon=&actors[0],.move=-1,.simulated=simulated,.victory=victory};pr_apply_ab_attrs(&bt,PR_AC_PostBattleAbAttr,&p);}
  else pr_prepare_post_battle_money(&bt,&e,victory);
  bool exact=(mode?result==-1:got==result)&&!bt.n_unported&&!bt.n_deferred&&!e.overflow;
  bool stocks=stock_frame('O',false),queue=queues();exact&=stocks&&queue;
  for(int k=0;k<4;k++){char *row=line_read("A ");if(token(&row,0,3)!=k)bad();pr_pokemon *p=&actors[k];exact&=p->hp==token(&row,0,100000);for(int n=0;n<6;n++)exact&=p->stats[n]==token(&row,1,100000);exact&=history(p,&row,false);end_tokens(row);}
  if(strcmp(line_read("E"),"\n"))bad();
  if(!exact){if(fail<15)fprintf(stderr,"pickup%d %s scenario%d mode%d result%d/%d stocks%d queue%d unported%d cache%u loot%d\n",i,variant,(int)c[1],mode,got,result,stocks,queue,bt.n_unported,bt.pickup_cached_ref,bt.post_battle_loot);fail++;if(getenv("PICKUP_FAIL_FAST"))return 1;}
 }
 double footer[1];read_vector("PICKUP_COMPLETE ",footer,1);if(footer[0]!=total||fgetc(input)!=EOF)bad();for(int m=0;m<4;m++)if(modes[m]!=(1000+(m==3?total-4000:0)))bad();if(version==2)for(int shape=0;shape<8;shape++)if(shapes[shape]!=50)bad();printf("Pickup: %d required native cases, %d failures\n",total,fail);return fail?1:0;
}
