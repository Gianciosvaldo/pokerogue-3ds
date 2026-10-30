/* The actual core-event -> battle-view bridge, including the second player slot. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "battle_events.h"
const char*i18n_language(void){return "en";}
const char*i18n_species_name(int species){(void)species;return "Mew";}
char*pak_read(const char*path,size_t*length){(void)path;if(length)*length=0;return NULL;}
static pr_battle bt;static pr_engine engine;static pr_pokemon party[2];
static void shiny_cases(void){
 for(int b=0;b<4;b++)for(int variant=0;variant<3;variant++){
  memset(&bt,0,sizeof bt);memset(&engine,0,sizeof engine);engine.n_levels=1;engine.current=-1;pr_pokemon*p=&party[0];memset(p,0,sizeof *p);p->id=42;p->species=SPECIES_ID_MEW;p->level=5;p->on_field=true;p->shiny=true;p->variant=variant;p->hp=p->stats[STAT_HP]=10;p->is_player=b<2;bt.field[b]=p;
  be_view view={0};int slot=(b%2)*2+(b>=2);view.present[slot]=true;be_mon(&bt,p,&view.mon[slot]);
  int phase=pr_phase_new(&engine,PH_SHINY_SPARKLE,b);assert(phase>=0);pr_phase_run(&bt,&engine,phase);assert(engine.n_events==1&&engine.events[0].type==PR_EV_SHINY_SPARKLE);
  bv_event out[8];int n=be_convert_events(&bt,engine.events,engine.n_events,false,&view,out,8);assert(n==1&&out[0].type==BEV_SHINY_SPARKLE&&out[0].battler==b&&view.mon[slot].variant==variant);
 }
 puts("Shiny bridge: 12 actual core-phase events retained for all four field slots and three variants");
}
int main(void){pr_closures_init();int cases=0;
 const int types[]={BATTLER_TAG_TYPE_UNDERGROUND,BATTLER_TAG_TYPE_FLYING,BATTLER_TAG_TYPE_UNDERWATER,BATTLER_TAG_TYPE_HIDDEN};
 for(int width=1;width<=2;width++)for(int mask=0;mask<(1<<width);mask++)for(int t=0;t<4;t++){
  memset(&bt,0,sizeof bt);memset(&engine,0,sizeof engine);memset(party,0,sizeof party);be_view view={0};bt.n_party[0]=width;bt.double_battle=width==2;
  for(int k=0;k<width;k++){
   pr_pokemon*p=&party[k];p->id=k+1;p->species=SPECIES_ID_MEW;p->level=5;p->hp=p->stats[STAT_HP]=10;p->on_field=p->is_player=true;bt.field[k]=bt.party[0][k]=p;
   if(mask&(1<<k)){p->n_tags=1;p->tags[0]=(pr_battler_tag){.type=types[t],.source_move=MOVE_ID_DIG,.turn_count=1};}
   int screen=k?2:0;view.present[screen]=true;be_mon(&bt,p,&view.mon[screen]);/* The prior charge event left this presentation state hidden. */view.mon[screen].hidden=!!(mask&(1<<k));
  }
  for(int k=0;k<width;k++)pr_remove_tag(&bt,&engine,&party[k],types[t]);
  pr_event_add(&bt,&engine,PR_EV_NONE,-1,-1,0,0,NULL);
  bv_event output[16];int n=be_convert_events(&bt,engine.events,engine.n_events,false,&view,output,16),seen=0;
  for(int k=0;k<n;k++){assert(output[k].type==BEV_VISIBLE&&output[k].a==1&&output[k].battler>=0&&output[k].battler<width);assert(!(seen&(1<<output[k].battler)));seen|=1<<output[k].battler;}
  assert(seen==mask&&n==((mask&1)!=0)+((mask&2)!=0));cases++;
 }
 shiny_cases();assert(cases==24);printf("charge_visibility_test: 24 real bridge cases, both player slots restored without extra effects\n");return 0;}
