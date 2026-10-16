#define main gas_replay_main
#include "gas-lifecycle-test.c"
#undef main
static int phase_type(const char *name){for(int j=0;j<PH_COUNT_;j++)if(pr_phase_name[j]&&!strcmp(name,pr_phase_name[j]))return j;bad();return -1;}
static bool compare_phase(const pr_phase *p,const pr_battle *bt){
 char name[80],marker[80],extra;int b,field,party,player,dbl,use;
 if(sscanf(line_read("Q "),"%79s %d %d %d %d %d %d %79s %c",name,&b,&field,&party,&player,&dbl,&use,marker,&extra)!=8)bad();
 phase_type(name);if(b<-1||b>3||field<-1||field>1||party<-1||party>1||player<-1||player>1||dbl<-1||dbl>1||use<-1||use>1)bad();
 if(strcmp(marker,"-"))phase_type(marker);if(!p)return false;
 bool member=p->type==PH_SUMMON||p->type==PH_RETURN,pokemon=p->type==PH_SHINY_SPARKLE;
 int gotb=member?-1:p->battler;
 int gotfield=member?(p->sw_field_index<(bt->double_battle?2:1)?p->sw_field_index:-1):p->type==PH_CHECK_SWITCH?p->sw_field_index:pokemon?p->battler%2:-1;
 return !strcmp(name,pr_phase_name[p->type])&&b==gotb&&field==gotfield&&party==(member?p->sw_field_index:-1)
  &&player==(member?p->sw_player:pokemon?p->battler<2:-1)&&dbl==(p->type==PH_TOGGLE_DOUBLE_POSITION?p->position_double:-1)
  &&use==(p->type==PH_CHECK_SWITCH?p->sw_use_name:-1)&&!strcmp(marker,p->type==PH_MARKER?pr_phase_name[p->marker_of]:"-");
}
int main(int argc,char **argv){
 if(argc!=2)return 2;input=fopen(argv[1],"r");if(!input)return 2;pr_closures_init();double h[2];read_vector("ENCOUNTEREND ",h,2);if(h[0]!=2)bad();int n=integer(h[1],512,100000),fail=0,doubles=0,loaded=0;
 for(int i=0;i<n;i++){
  static pr_battle bt;static pr_engine e;memset(&bt,0,sizeof bt);memset(&e,0,sizeof e);bt.engine=&e;e.n_levels=1;e.current=-1;double a[9];read_vector("C ",a,9);if(a[0]!=i)bad();bt.double_battle=integer(a[1],0,1);bool load=integer(a[2],0,1);bt.n_party[0]=integer(a[3],1,3);bt.n_party[1]=integer(a[4],1,2);bt.game_mode=integer(a[5],0,4);bt.battle_type=integer(a[6],0,2);bt.wave_index=integer(a[7],1,1000);bool common=integer(a[8],0,1);doubles+=bt.double_battle;loaded+=load;pr_pokemon players[3]={0},enemies[2]={0};
  for(int k=0;k<bt.n_party[0];k++){double p[3];read_vector("P ",p,3);players[k].hp=integer(p[0],0,100000);players[k].on_field=integer(p[1],0,1);players[k].switch_out_status=integer(p[2],0,1);players[k].is_player=true;bt.party[0][k]=&players[k];if(k<2)bt.field[k]=&players[k];}
  for(int k=0;k<bt.n_party[1];k++){double p[2];read_vector("N ",p,2);enemies[k].hp=integer(p[1],0,100000);enemies[k].shiny=integer(p[0],0,1);bt.party[1][k]=&enemies[k];bt.field[2+k]=&enemies[k];}
  rng_read("R ",&bt.rng);rng_read("B ",&bt.battle_rng);bt.rng_live=bt.battle_rng_live=true;if(common)pr_queue_trainer_summons(&bt,&e);pr_queue_encounter_end(&bt,&e,load);bool exact=!bt.n_unported&&!e.overflow;
  double lev[1];read_vector("LEVELS ",lev,1);int nl=integer(lev[0],1,PR_MAX_LEVELS);exact&=nl==e.n_levels;
  for(int l=0;l<nl;l++){double v[2];read_vector("L ",v,2);if(v[0]!=l)bad();int count=integer(v[1],0,PR_LEVEL_CAP);exact&=l<e.n_levels&&count==e.level_len[l];for(int k=0;k<count;k++){
   const pr_phase *p=l<e.n_levels&&k<e.level_len[l]?&e.pool[e.levels[l][k]]:NULL;exact&=compare_phase(p,&bt);
  }}
  double dn[1];read_vector("DYNTYPES ",dn,1);int nd=integer(dn[0],0,PH_COUNT_),actual=0;bool seen[PH_COUNT_]={false};for(int t=0;t<PH_COUNT_;t++)actual+=e.n_dyn[t]>0;exact&=nd==actual;
  for(int j=0;j<nd;j++){
   char name[80],extra;int count;if(sscanf(line_read("D "),"%79s %d %c",name,&count,&extra)!=2||count<1||count>PR_DYN_CAP)bad();int t=phase_type(name);if(seen[t])bad();seen[t]=true;exact&=count==e.n_dyn[t];
   for(int k=0;k<count;k++){const pr_phase *p=k<e.n_dyn[t]?&e.pool[e.dyn[t][k]]:NULL;exact&=compare_phase(p,&bt);}
  }

  pr_rng r;rng_read("RO ",&r);exact&=rng_equal(bt.rng,r);rng_read("BO ",&r);exact&=rng_equal(bt.battle_rng,r);if(strcmp(line_read("E"),"\n"))bad();if(!exact){if(fail<10)fprintf(stderr,"encounter%d double%d loaded%d party%d mode%d type%d wave%d levels%d/%d unported%d\n",i,bt.double_battle,load,bt.n_party[0],bt.game_mode,bt.battle_type,bt.wave_index,e.n_levels,nl,bt.n_unported);fail++;}
 }
 double end[1];read_vector("ENCOUNTEREND_COMPLETE ",end,1);if(end[0]!=n||fgetc(input)!=EOF)bad();printf("Encounter end: %d complete (%d doubles, %d loaded), %d failures\n",n,doubles,loaded,fail);return fail?1:0;
}
