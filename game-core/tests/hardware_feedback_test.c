#define main retained_doubles_main
#include "doubles_host_test.c"
#undef main
static gc_system sys,loaded;
static void undo_commands(void){
 setup(1);for(int k=0;k<4;k++){pr_pokemon *p=r.bt.field[k];p->hp=p->stats[0]=100000;p->n_moves=1;p->moves[0]=k?MOVE_ID_SPLASH:MOVE_ID_TACKLE;}
 pr_rng before=r.bt.rng,battle=r.bt.battle_rng;const char *why=NULL;
 CHECK(!gc_run_choose(&r,&(gc_choice){GC_CH_CANCEL,0,0,0},&why));
 for(int k=0;k<4;k++){
  choose(&r,(gc_choice){GC_CH_FIGHT,0,-1,0});choose(&r,(gc_choice){GC_CH_TARGET,3,0,0});
  CHECK(r.screen.slot==1&&r.screen.can_undo&&!gc_run_can_save(&r));
  choose(&r,(gc_choice){GC_CH_CANCEL,0,0,0});
  CHECK(r.screen.slot==0&&!r.screen.can_undo&&r.bt.commands[0].command==-1&&r.bt.commands[1].command==-1);
  CHECK(rngsame(before,r.bt.rng)&&rngsame(battle,r.bt.battle_rng));CHECK(gc_run_can_save(&r));
 }
 choose(&r,(gc_choice){GC_CH_FIGHT,0,-1,0});choose(&r,(gc_choice){GC_CH_TARGET,2,0,0});
 choose(&r,(gc_choice){GC_CH_FIGHT,0,-1,0});
 CHECK(r.bt.turn==2&&r.bt.field[0]->pp_used[0]==1&&r.bt.field[1]->pp_used[0]==1);
}
static void undo_random_target(void){
 setup(1);pr_pokemon *p=r.bt.field[0];p->moves[0]=MOVE_ID_OUTRAGE;p->n_moves=1;
 pr_rng before=r.bt.battle_rng;bool live=r.bt.battle_rng_live;
 choose(&r,(gc_choice){GC_CH_FIGHT,0,-1,0});CHECK(r.screen.slot==1&&r.bt.battle_rng_live);
 int target=r.bt.commands[0].targets[0];pr_rng after=r.bt.battle_rng;
 choose(&r,(gc_choice){GC_CH_CANCEL,0,0,0});CHECK(r.bt.battle_rng_live==live&&rngsame(r.bt.battle_rng,before));
 choose(&r,(gc_choice){GC_CH_FIGHT,0,-1,0});CHECK(r.bt.commands[0].targets[0]==target&&rngsame(r.bt.battle_rng,after));
}
static void sacrifice(void){
 setup(1);for(int k=0;k<4;k++){pr_pokemon *p=r.bt.field[k];p->hp=p->stats[0]=10000;p->n_moves=1;p->moves[0]=k?MOVE_ID_SPLASH:MOVE_ID_SELF_DESTRUCT;p->ability=ABILITY_ID_NONE;p->custom_ability=ABILITY_ID_NONE;p->passive=0;}
 choose(&r,(gc_choice){GC_CH_FIGHT,0,-1,0});
 if(r.screen.kind==GC_SCREEN_COMMAND&&r.screen.slot==1)choose(&r,(gc_choice){GC_CH_FIGHT,0,-1,0});
 CHECK(r.party[0].b.hp==0);
 /* The source's explicit ignoreSegments must also KO a boss user. */
 setup(2);pr_pokemon *boss=r.bt.field[2];boss->hp=boss->stats[0]=10000;boss->is_boss=true;boss->boss_segments=4;boss->boss_segment_index=3;
 int ph=pr_phase_new(&r.eng,PH_MOVE_EFFECT,2);CHECK(ph>=0);r.eng.pool[ph].move_id=MOVE_ID_SELF_DESTRUCT;
 const pr_move *m=&pr_moves[MOVE_ID_SELF_DESTRUCT];int found=0;
 for(int k=0;k<m->nattr;k++)if(pr_attrs[m->attr0+k].cls==PR_AC_SacrificialAttr){pr_apply_move_effect(&r.bt,&r.eng,ph,m->attr0+k,boss,NULL);found++;}
 CHECK(found==1&&boss->hp==0);
}
static void starter_native(const char *file){
 FILE *f=fopen(file,"r");CHECK(f);char tag[32];int version,n,p;
 CHECK(fscanf(f,"%31s%d%d%d",tag,&version,&n,&p)==4&&!strcmp(tag,"STARTER_FEEDBACK")&&version==1&&n==572&&p==64);
 gc_system_init_defaults(&sys);
 for(int k=0;k<n;k++){
  int id,base,cost[2];double value[2];CHECK(fscanf(f,"%31s%d%d%d%d%lf%lf",tag,&id,&base,&cost[0],&cost[1],&value[0],&value[1])==7&&!strcmp(tag,"C"));
  CHECK(gc_starter_cost(id)==base);sys.dex[id].caught_attr=157;sys.starters[id].value_reduction=0;
  for(int j=0;j<2;j++){
   CHECK(gc_system_starter_reduction_cost(&sys,id)==cost[j]);sys.starters[id].candy_count=cost[j]-1;
   gc_starter_entry old=sys.starters[id];CHECK(!gc_system_buy_starter_reduction(&sys,id)&&!memcmp(&old,&sys.starters[id],sizeof old));
   sys.starters[id].candy_count=cost[j];CHECK(gc_system_buy_starter_reduction(&sys,id));
   CHECK(sys.starters[id].value_reduction==(unsigned)j+1&&sys.starters[id].candy_count==0&&gc_starter_value(&sys,id)==value[j]);
  }
  sys.starters[id].candy_count=1000;gc_starter_entry old=sys.starters[id];
  CHECK(gc_system_starter_reduction_cost(&sys,id)==-1&&!gc_system_buy_starter_reduction(&sys,id)&&!memcmp(&old,&sys.starters[id],sizeof old));
 }
 for(int k=0;k<p;k++){
  unsigned long long ms;uint16_t expected[3],actual[3];unsigned ids[3];
  CHECK(fscanf(f,"%31s%llu%u%u%u",tag,&ms,&ids[0],&ids[1],&ids[2])==5&&!strcmp(tag,"P"));
  for(int j=0;j<3;j++)expected[j]=ids[j];
  gc_pokerus_starters(ms,actual);CHECK(!memcmp(expected,actual,sizeof actual));
  gc_pokerus_starters(ms+10799999,actual);CHECK(!memcmp(expected,actual,sizeof actual));
  CHECK(actual[0]!=actual[1]&&actual[0]!=actual[2]&&actual[1]!=actual[2]);
 }
 int a,b;CHECK(fscanf(f,"%31s%d%d",tag,&a,&b)==3&&!strcmp(tag,"COMPLETE")&&a==572&&b==64&&fscanf(f,"%31s",tag)==EOF);fclose(f);
 const char *why=NULL;size_t size=gc_system_save(&sys,save,sizeof save);CHECK(size&&gc_system_load(&loaded,save,size,&why));
 for(int k=0;k<gc_starter_cost_count;k++){int id=gc_starter_costs[k].id;CHECK(loaded.starters[id].value_reduction==2&&loaded.starters[id].candy_count==1000);}
}
static void sacrifice_native(const char *file){
 FILE *f=fopen(file,"r");CHECK(f);char tag[32];int version,n;
 CHECK(fscanf(f,"%31s%d%d",tag,&version,&n)==3&&!strcmp(tag,"SACRIFICE_HP")&&version==1&&n==24);
 for(int k=0;k<n;k++){
  int move,boss,hp,applied,end,taken;CHECK(fscanf(f,"%31s%d%d%d%d%d%d",tag,&move,&boss,&hp,&applied,&end,&taken)==7&&!strcmp(tag,"H")&&applied==1);
  setup(2);pr_pokemon *p=r.bt.field[2];p->hp=hp;p->stats[0]=100;p->damage_taken=0;p->is_boss=boss;p->boss_segments=boss?4:0;p->boss_segment_index=boss?3:0;
  int ph=pr_phase_new(&r.eng,PH_MOVE_EFFECT,2);CHECK(ph>=0);r.eng.pool[ph].move_id=move;
  int found=0;const pr_move *m=&pr_moves[move];
  for(int j=0;j<m->nattr;j++)if(pr_attrs[m->attr0+j].cls==PR_AC_SacrificialAttr){pr_apply_move_effect(&r.bt,&r.eng,ph,m->attr0+j,p,NULL);found++;}
  CHECK(found==1&&p->hp==end&&p->damage_taken==taken);
 }
 CHECK(fscanf(f,"%31s%d",tag,&n)==2&&!strcmp(tag,"COMPLETE")&&n==24&&fscanf(f,"%31s",tag)==EOF);fclose(f);
}
int main(int argc,char **argv){CHECK(argc==3);undo_commands();undo_random_target();sacrifice();starter_native(argv[1]);sacrifice_native(argv[2]);printf("Hardware feedback controller/native starter checks: %d passed\n",checks);return 0;}
