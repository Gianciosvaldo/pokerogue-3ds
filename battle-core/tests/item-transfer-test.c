#define main gas_replay_main
#include "gas-lifecycle-test.c"
#undef main
static const char *keys[]={"LEFTOVERS","SHELL_BELL","FOCUS_BAND","QUICK_CLAW","KINGS_ROCK","WHITE_HERB","MINI_BLACK_HOLE","BERRY","WIDE_LENS","MULTI_LENS","SCOPE_LENS","TOXIC_ORB","FLAME_ORB","SOUL_DEW","GOLDEN_PUNCH","ATTACK_TYPE_BOOSTER","REVIVER_SEED"};
static const int classes[]={PR_MOD_TURN_HEAL,PR_MOD_HIT_HEAL,PR_MOD_SURVIVE_DAMAGE,PR_MOD_BYPASS_SPEED_CHANCE,PR_MOD_FLINCH_CHANCE,PR_MOD_RESET_NEGATIVE_STAT_STAGE,PR_MOD_TURN_HELD_TRANSFER,PR_MOD_BERRY,PR_MOD_MOVE_ACCURACY_BOOSTER,PR_MOD_MULTI_HIT,PR_MOD_CRIT_BOOSTER,PR_MOD_TURN_STATUS_EFFECT,PR_MOD_TURN_STATUS_EFFECT,PR_MOD_NATURE_WEIGHT,PR_MOD_DAMAGE_MONEY_REWARD,PR_MOD_ATTACK_TYPE_BOOSTER,PR_MOD_INSTANT_REVIVE};
static int keyclass(const char *s){for(unsigned k=0;k<sizeof classes/sizeof *classes;k++)if(!strcmp(s,keys[k]))return classes[k];bad();return -1;}
static uint32_t actor_ids[4];
static pr_modifier mod_read(bool output){
 char key[80],extra;int arg,owner,stack,virtual,transfer,max;
 if(sscanf(line_read("M "),"%79s %d %d %d %d %d %d %c",key,&arg,&owner,&stack,&virtual,&transfer,&max,&extra)!=7||owner<0||owner>3||stack<(output?0:1)||stack>99||virtual!=0||transfer<0||transfer>1||max<1||max>99)bad();
 pr_modifier m={.cls=keyclass(key),.pokemon_id=actor_ids[owner],.stack_count=stack,.virtual_stack_count=virtual,.is_transferable=transfer};
 if(m.cls==PR_MOD_BERRY){if(arg<0||arg>10)bad();m.berry_type=arg;}
 else if(m.cls==PR_MOD_ATTACK_TYPE_BOOSTER){if(arg<0||arg>17)bad();m.move_type=arg;m.mult=.2;}
 else if(arg!=-1)bad();
 if(m.cls==PR_MOD_MOVE_ACCURACY_BOOSTER)m.num=5;
 if(m.cls==PR_MOD_CRIT_BOOSTER)m.num=1;
 if(m.cls==PR_MOD_TURN_STATUS_EFFECT)m.status_effect=!strcmp(key,"TOXIC_ORB")?STATUS_EFFECT_TOXIC:STATUS_EFFECT_BURN;
 return m;
}
static void msghex(const pr_msg *m,char *out){pr_text_source src={locale_get,NULL,"en"};char s[2048];size_t n=pr_msg_format(m,&src,s,sizeof s);if(n>=sizeof s)bad();if(!n){strcpy(out,"-");return;}for(size_t k=0;k<n;k++)snprintf(out+k*2,3,"%02x",(unsigned char)s[k]);}
int main(int argc,char **argv){
 bool turn_end=argc==3&&!strcmp(argv[2],"--turn-end");if(argc!=2&&!turn_end)return 2;input=fopen(argv[1],"r");if(!input||!getenv("TURNTEST_LOCALES"))return 2;pr_closures_init();double h[2];read_vector("ITEMTRANSFER ",h,2);if(h[0]!=1)bad();int n=integer(h[1],512,100000),fail=0,success=0;
 for(int i=0;i<n;i++){
  static pr_battle bt;static pr_engine e;memset(&bt,0,sizeof bt);memset(&e,0,sizeof e);bt.engine=&e;bt.double_battle=true;bt.wave_index=1;bt.last_used_pokeball=-1;bt.ignoring_effect_source=-1;e.current=-1;e.n_levels=1;pr_pokemon actors[4]={0};double c[4];read_vector("C ",c,4);if(c[0]!=i)bad();int holder=integer(c[1],0,3),should=integer(c[2],0,1),result=integer(c[3],0,1);
  {char *seed=line_read("W ");seed[strcspn(seed,"\n")]=0;if(!*seed||strlen(seed)>64||strchr(seed,' '))bad();pr_seed_from_ascii(&bt.wave_seed,seed);double turn[1];read_vector("TURN ",turn,1);bt.turn=integer(turn[0],1,100000);}
  if(turn_end){char *seed=line_read("SEED ");seed[strcspn(seed,"\n")]=0;if(!*seed||strlen(seed)>64||strchr(seed,' '))bad();pr_seed_from_ascii(&bt.battle_seed,seed);}
  for(int k=0;k<4;k++){
   double all[19];read_vector("MON ",all,19);if(all[0]<0||all[0]>UINT32_MAX||floor(all[0])!=all[0])bad();pr_pokemon *p=&actors[k];p->id=actor_ids[k]=(uint32_t)all[0];double *m=all+1;p->species=integer(m[0],1,65535);if(p->species!=SPECIES_ID_BULBASAUR&&p->species!=SPECIES_ID_CHARMANDER&&p->species!=SPECIES_ID_MAGIKARP)bad();p->form=integer(m[1],0,255);p->level=integer(m[2],1,255);p->nature=integer(m[3],0,24);p->hp=integer(m[4],0,100000);p->ability=p->base_ability=integer(m[5],0,PR_ABILITY_COUNT-1);p->ability_override=-1;p->custom_passive=0;p->is_player=k<2;p->on_field=true;p->added_type=-1;for(int s=0;s<6;s++){p->ivs[s]=integer(m[6+s],0,31);p->stats[s]=integer(m[12+s],1,100000);}bt.field[k]=bt.party[k/2][k%2]=p;bt.n_party[k/2]=2;
  }
  rng_read("R ",&bt.rng);rng_read("B ",&bt.battle_rng);bt.rng_live=bt.battle_rng_live=true;
  for(int side=0;side<2;side++){double a[2];read_vector("I ",a,2);if(a[0]!=side)bad();bt.n_mods[side]=integer(a[1],0,PR_MAX_MODIFIERS);for(int k=0;k<bt.n_mods[side];k++)bt.mods[side][k]=mod_read(false);}
  pr_modifier *black=NULL;for(int k=0;k<bt.n_mods[holder/2];k++){pr_modifier *m=&bt.mods[holder/2][k];if(m->cls==PR_MOD_TURN_HELD_TRANSFER&&m->pokemon_id==(int64_t)actor_ids[holder]){if(black)bad();black=m;}}if(!black)bad();bool got=false;if(turn_end){int ph=pr_phase_new(&e,PH_TURN_END,-1);e.current=ph;pr_phase_run(&bt,&e,ph);}else got=pr_apply_turn_held_item_transfer(&bt,&e,&actors[holder],black);success+=got;if(turn_end)for(int l=0;l<e.n_levels;l++)for(int k=0;k<e.level_len[l];k++){pr_phase *p=&e.pool[e.levels[l][k]];if(p->type==PH_MESSAGE&&p->msg.key&&!strcmp(p->msg.key,"modifier:turnHeldItemTransferApply"))success++;}bool exact=got==result&&should==(actors[holder].hp>0)&&!bt.n_unported&&!e.overflow;
  for(int side=0;side<2;side++){double a[2];read_vector("O ",a,2);if(a[0]!=side)bad();int count=integer(a[1],0,PR_MAX_MODIFIERS);exact&=count==bt.n_mods[side];for(int k=0;k<count;k++){pr_modifier expected=mod_read(true);if(k>=bt.n_mods[side]){exact=false;continue;}pr_modifier *m=&bt.mods[side][k];exact&=m->cls==expected.cls&&m->pokemon_id==expected.pokemon_id&&m->stack_count==expected.stack_count&&m->virtual_stack_count==expected.virtual_stack_count&&m->is_transferable==expected.is_transferable&&m->berry_type==expected.berry_type&&m->move_type==expected.move_type&&m->status_effect==expected.status_effect&&m->num==expected.num&&m->mult==expected.mult;}}
  double levels[1];read_vector("LEVELS ",levels,1);int nl=integer(levels[0],1,PR_MAX_LEVELS);exact&=nl==e.n_levels;
  for(int l=0;l<nl;l++){double a[2];read_vector("L ",a,2);if(a[0]!=l)bad();int count=integer(a[1],0,PR_LEVEL_CAP);exact&=l<e.n_levels&&count==e.level_len[l];for(int k=0;k<count;k++){char name[80],hex[4096],extra;int b;if(sscanf(line_read("P "),"%79s %d %4095s %c",name,&b,hex,&extra)!=3||b<-1||b>3)bad();if(strcmp(hex,"-")&&(strlen(hex)%2||strspn(hex,"0123456789abcdef")!=strlen(hex)))bad();if(l>=e.n_levels||k>=e.level_len[l]){exact=false;continue;}pr_phase *p=&e.pool[e.levels[l][k]];char gothex[4096]="-";if(p->type==PH_MESSAGE)msghex(&p->msg,gothex);exact&=!strcmp(name,pr_phase_name[p->type])&&b==p->battler&&!strcmp(hex,gothex);}}
  for(int k=0;k<4;k++){char *row=line_read("A "),*end;double v[8];for(int j=0;j<8;j++){v[j]=strtod(row,&end);if(end==row)bad();row=end;}exact&=actors[k].hp==v[0];for(int j=0;j<6;j++)exact&=actors[k].stats[j]==v[1+j];int nt=integer(v[7],0,PR_MAX_BATTLER_TAGS);exact&=nt==actors[k].n_tags;for(int j=0;j<nt;j++){char tag[80];int used=0;if(sscanf(row," %79s%n",tag,&used)!=1||strcmp(tag,"UNBURDEN"))bad();row+=used;if(j>=actors[k].n_tags||actors[k].tags[j].type!=BATTLER_TAG_TYPE_UNBURDEN)exact=false;}if(strcmp(row,"\n"))bad();}
  if(turn_end){double counters[8];read_vector("COUNTERS ",counters,8);for(int k=0;k<4;k++)exact&=actors[k].temp_turn_count==counters[2*k]&&actors[k].temp_wave_turn_count==counters[2*k+1];}
  bool battle_live=true;if(turn_end){double live[1];read_vector("BATTLE_LIVE ",live,1);battle_live=integer(live[0],0,1);exact&=bt.battle_rng_live==battle_live;}
  pr_rng rng;rng_read("RO ",&rng);exact&=rng_equal(bt.rng,rng);rng_read("BO ",&rng);exact&=!battle_live||rng_equal(bt.battle_rng,rng);if(strcmp(line_read("E"),"\n"))bad();if(!exact){if(fail<12)fprintf(stderr,"transfer%d holder%d result%d/%d mods%d/%d levels%d/%d unported%d\n",i,holder,got,result,bt.n_mods[0],bt.n_mods[1],e.n_levels,nl,bt.n_unported);if(fail<2){for(int z=0;z<bt.n_unported;z++)fprintf(stderr,"feature %s\n",pr_unported_name(bt.unported[z]));for(int z=0;z<4;z++)fprintf(stderr,"actor%d hp%d tags%d\n",z,actors[z].hp,actors[z].n_tags);}
fail++;}
 }
 double footer[1];read_vector("ITEMTRANSFER_COMPLETE ",footer,1);if(footer[0]!=n||fgetc(input)!=EOF)bad();printf("Held transfer%s: %d complete, %d successful, %d failures\n",turn_end?" TurnEnd":"",n,success,fail);return fail?1:0;
}
