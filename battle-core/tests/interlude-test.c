/* Strict replay of actual Source CheckInterludePhase and hierarchical queues. */
#define main gas_replay_main
#include "gas-lifecycle-test.c"
#undef main
static const pr_phase_type kinds[]={PH_WEATHER_EFFECT,PH_BERRY,PH_CHECK_STATUS_EFFECT,PH_POSITIONAL_TAG,PH_TURN_END,PH_POST_TURN_STATUS_EFFECT,PH_HIDE_ABILITY,PH_MESSAGE};
int main(int argc,char **argv){
 if(argc!=2)return 2;
 input=fopen(argv[1],"r");if(!input)return 2;pr_closures_init();double header[3];read_vector("INTERLUDE ",header,3);if(header[0]!=1)bad();int n=integer(header[1],512,100000),failed=0,changed=0;
 for(int i=0;i<n;i++){
  static pr_battle bt;static pr_engine e;memset(&bt,0,sizeof bt);memset(&e,0,sizeof e);bt.engine=&e;e.current=-1;double c[3];read_vector("C ",c,3);if(c[0]!=i)bad();bt.wave_index=integer(c[1],0,1000000);int count=integer(c[2],0,6);bt.n_party[1]=count;pr_pokemon enemies[6]={0};
  if(count){double hp[6];read_vector("H ",hp,count);for(int j=0;j<count;j++){enemies[j].hp=integer(hp[j],-1000,1000);enemies[j].stats[STAT_HP]=1000;bt.party[1][j]=&enemies[j];if(j<2)bt.field[2+j]=&enemies[j];}}
  else if(strcmp(line_read("H "),"-\n"))bad();
  rng_read("R ",&bt.rng);rng_read("B ",&bt.battle_rng);double q[1];read_vector("I ",q,1);e.n_levels=integer(q[0],1,4);int total=0;
  for(int l=0;l<e.n_levels;l++){
   double level[2];read_vector("L ",level,2);if(level[0]!=l)bad();int size=integer(level[1],0,20);e.level_len[l]=size;
   for(int k=0;k<size;k++){double p[3];read_vector("P ",p,3);if(p[0]!=total++)bad();int kind=integer(p[1],0,7),flag=integer(p[2],0,1);if(flag&&kind!=4)bad();int ph=pr_phase_new(&e,kinds[kind],-1);if(ph!=p[0])bad();e.pool[ph].upcoming_interlude=flag;e.levels[l][k]=ph;}
  }
  int ph=pr_phase_new(&e,PH_CHECK_INTERLUDE,-1);e.current=ph;pr_phase_run(&bt,&e,ph);bool exact=!bt.n_unported&&!e.overflow;
  read_vector("O ",q,1);if(q[0]!=e.n_levels)exact=false;int remaining=0;
  for(int l=0;l<e.n_levels;l++){
   double level[2];read_vector("L ",level,2);if(level[0]!=l)bad();int size=integer(level[1],0,20);exact&=size==e.level_len[l];remaining+=size;
   for(int k=0;k<size;k++){double p[3];read_vector("P ",p,3);int id=integer(p[0],0,total-1),kind=integer(p[1],0,7),flag=integer(p[2],0,1);if(k>=e.level_len[l]){exact=false;continue;}int got=e.levels[l][k];exact&=got==id&&e.pool[got].type==kinds[kind]&&e.pool[got].upcoming_interlude==flag;}
  }
  changed+=remaining!=total;pr_rng rng;rng_read("RO ",&rng);exact&=rng_equal(rng,bt.rng);rng_read("BO ",&rng);exact&=rng_equal(rng,bt.battle_rng);if(strcmp(line_read("E"),"\n"))bad();if(!exact){if(failed<12)fprintf(stderr,"interlude case%d wave%d party%d unported%d\n",i,bt.wave_index,count,bt.n_unported);failed++;}
 }
 double end[1];read_vector("INTERLUDE_COMPLETE ",end,1);if(end[0]!=n||fgetc(input)!=EOF)bad();printf("Interlude: %d complete, %d failures, %d changed queues\n",n,failed,changed);return failed?1:0;
}
