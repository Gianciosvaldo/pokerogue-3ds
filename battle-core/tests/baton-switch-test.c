#define main gas_replay_main
#include "gas-lifecycle-test.c"
#undef main
static const char *keys[]={"BATON"};
static const int classes[]={PR_MOD_SWITCH_EFFECT_TRANSFER};
static int keyclass(const char *s){for(unsigned k=0;k<sizeof classes/sizeof *classes;k++)if(!strcmp(s,keys[k]))return classes[k];bad();return -1;}
static uint32_t actor_ids[6];
static pr_modifier mod_read_prefix(const char *prefix){
 char key[80],extra;int arg,owner,stack,virtual,transfer,max,tier;
 if(sscanf(line_read(prefix),"%79s %d %d %d %d %d %d %d %c",key,&arg,&owner,&stack,&virtual,&transfer,&max,&tier,&extra)!=8||owner< -1||owner>5||stack<0||stack>100||virtual<0||virtual>3||transfer<0||transfer>1||max<0||max>99||tier<-1||tier>5)bad();
 pr_modifier m={.cls=keyclass(key),.pokemon_id=owner<0?0:actor_ids[owner],.stack_count=stack,.virtual_stack_count=virtual,.is_transferable=transfer,.type_tier=tier+1};
 if(m.cls==PR_MOD_BERRY){if(arg<0||arg>10)bad();m.berry_type=arg;}
 else if(m.cls==PR_MOD_ATTACK_TYPE_BOOSTER){if(arg<0||arg>17)bad();m.move_type=arg;m.mult=.2;}
 else if(arg!=-1)bad();
 if(m.cls==PR_MOD_MOVE_ACCURACY_BOOSTER)m.num=5;
 if(m.cls==PR_MOD_CRIT_BOOSTER)m.num=1;
 if(m.cls==PR_MOD_TURN_STATUS_EFFECT)m.status_effect=!strcmp(key,"TOXIC_ORB")?STATUS_EFFECT_TOXIC:STATUS_EFFECT_BURN;
 return m;
}
static bool mod_equal(const pr_modifier *m,const pr_modifier *other){
 return m->type_tier==other->type_tier&&m->cls==other->cls&&m->pokemon_id==other->pokemon_id
  &&m->stack_count==other->stack_count&&m->virtual_stack_count==other->virtual_stack_count
  &&m->is_transferable==other->is_transferable&&m->berry_type==other->berry_type
  &&m->move_type==other->move_type&&m->status_effect==other->status_effect
  &&m->num==other->num&&m->mult==other->mult;
}

int main(int argc,char **argv){
 if(argc!=2)return 2;input=fopen(argv[1],"r");if(!input||!getenv("TURNTEST_LOCALES"))return 2;pr_closures_init();double h[2];read_vector("BATONSWITCH ",h,2);int version=integer(h[0],1,2);int n=integer(h[1],256,256),fail=0;
 for(int i=0;i<n;i++){
  static pr_battle bt;static pr_engine e;memset(&bt,0,sizeof bt);memset(&e,0,sizeof e);bt.engine=&e;bt.double_battle=true;bt.wave_index=1;bt.last_used_pokeball=-1;bt.ignoring_effect_source=-1;e.current=-1;e.n_levels=1;pr_pokemon actors[6]={0};double c[5]={0};read_vector("C ",c,version==1?4:5);if(c[0]!=i)bad();int side=integer(c[1],0,1),field_index=integer(c[2],0,1),kind=integer(c[3],SWITCH_TYPE_SWITCH,SWITCH_TYPE_FORCE_SWITCH),can=version==1?-2:integer(c[4],-1,1);
  {char *seed=line_read("W ");seed[strcspn(seed,"\n")]=0;if(!*seed||strlen(seed)>64||strchr(seed,' '))bad();pr_seed_from_ascii(&bt.wave_seed,seed);double turn[1];read_vector("TURN ",turn,1);bt.turn=integer(turn[0],1,100000);}
  for(int k=0;k<6;k++){
   double all[20];read_vector("MON ",all,20);if(all[0]<0||all[0]>UINT32_MAX||floor(all[0])!=all[0])bad();pr_pokemon *p=&actors[k];p->id=actor_ids[k]=(uint32_t)all[0];double *m=all+1;p->species=integer(m[0],1,65535);if(p->species!=SPECIES_ID_BULBASAUR&&p->species!=SPECIES_ID_CHARMANDER&&p->species!=SPECIES_ID_MAGIKARP&&p->species!=SPECIES_ID_SQUIRTLE&&p->species!=SPECIES_ID_PIDGEY)bad();p->form=integer(m[1],0,255);p->level=integer(m[2],1,255);p->nature=integer(m[3],0,24);p->hp=integer(m[4],0,100000);p->ability=p->base_ability=integer(m[5],0,PR_ABILITY_COUNT-1);p->has_trainer=integer(m[6],0,1);p->ability_override=-1;p->custom_passive=0;p->is_player=k<3;p->on_field=true;p->added_type=-1;for(int s=0;s<6;s++){p->ivs[s]=integer(m[7+s],0,31);p->stats[s]=integer(m[13+s],1,100000);}bt.party[k/3][k%3]=p;bt.n_party[k/3]=3;p->on_field=k%3<2;if(k%3<2)bt.field[(k/3)*2+k%3]=p;
  }
  rng_read("R ",&bt.rng);rng_read("B ",&bt.battle_rng);bt.rng_live=bt.battle_rng_live=true;
  for(int side=0;side<2;side++){double a[2];read_vector("I ",a,2);if(a[0]!=side)bad();bt.n_mods[side]=integer(a[1],0,PR_MAX_MODIFIERS);for(int k=0;k<bt.n_mods[side];k++)bt.mods[side][k]=mod_read_prefix("M ");}
  pr_modifier *stock=NULL;
  for(int k=0;k<bt.n_mods[side];k++)if(bt.mods[side][k].pokemon_id==(int64_t)actors[side*3+field_index].id)stock=&bt.mods[side][k];
  if(version==2&&((stock==NULL)!=(can==-1)))bad();
  bool preflight=version==1||!stock||pr_can_transfer_held_item(&bt,stock,&actors[side*3+2],1)==can;
  if(kind==SWITCH_TYPE_BATON_PASS)
   pr_transfer_baton_on_switch(&bt,&e,&actors[side*3+field_index],&actors[side*3+2]);
  bool exact=preflight&&!bt.n_unported&&!bt.n_deferred&&!e.overflow;
  for(int side=0;side<2;side++){double a[2];read_vector("O ",a,2);if(a[0]!=side)bad();int count=integer(a[1],0,PR_MAX_MODIFIERS);exact&=count==bt.n_mods[side];for(int k=0;k<count;k++){pr_modifier expected=mod_read_prefix("M ");if(k>=bt.n_mods[side]){exact=false;continue;}pr_modifier *m=&bt.mods[side][k];exact&=mod_equal(m,&expected);}}
    for(int k=0;k<6;k++){char *row=line_read("A "),*end;double v[8];for(int j=0;j<8;j++){v[j]=strtod(row,&end);if(end==row)bad();row=end;}exact&=actors[k].hp==integer(v[0],0,100000);for(int j=0;j<6;j++)exact&=actors[k].stats[j]==integer(v[1+j],1,100000);int nt=integer(v[7],0,PR_MAX_BATTLER_TAGS);exact&=nt==actors[k].n_tags;for(int j=0;j<nt;j++){char tag[80];int used=0;if(sscanf(row," %79s%n",tag,&used)!=1||strcmp(tag,"UNBURDEN"))bad();row+=used;if(j>=actors[k].n_tags||actors[k].tags[j].type!=BATTLER_TAG_TYPE_UNBURDEN)exact=false;}if(strcmp(row,"\n"))bad();}

  pr_rng rng;rng_read("RO ",&rng);exact&=rng_equal(bt.rng,rng);rng_read("BO ",&rng);exact&=rng_equal(bt.battle_rng,rng);if(strcmp(line_read("E"),"\n"))bad();if(!exact){if(fail<12)fprintf(stderr,"baton-switch%d side%d field%d kind%d mods%d/%d unported%d\n",i,side,field_index,kind,bt.n_mods[0],bt.n_mods[1],bt.n_unported);
fail++;}
 }
 if(version==2){double tiers[3];read_vector("BATON_TIERS ",tiers,3);
 pr_modifier baton={.cls=PR_MOD_SWITCH_EFFECT_TRANSFER};
 for(int k=0;k<3;k++)if(integer(tiers[k],0,5)!=pr_held_item_tier_for_pool(&baton,k==2))fail++;}
 double footer[1];read_vector("BATONSWITCH_COMPLETE ",footer,1);if(footer[0]!=n||fgetc(input)!=EOF)bad();printf("Baton switch: %d complete, %d failures\n",n,fail);return fail?1:0;
}
