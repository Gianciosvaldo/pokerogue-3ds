/* Copied native restrictions through real commands and legal active-state saves. */
#include "../src/internal.h"
#include <stdio.h>
#include <stdlib.h>
static gc_run run,loaded;static unsigned char saved[GC_SAVE_MAX];static int checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"transform restriction host line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
static void drain(void){gc_event ev;while(gc_run_next_event(&run,&ev)){} }
static void fight(int slot){const char *why=NULL;gc_choice c={GC_CH_FIGHT,slot,-1,0};CHECK(gc_run_choose(&run,&c,&why));drain();}
int main(void)
{
 const int species[]={SPECIES_ID_MIMIKYU,SPECIES_ID_MORPEKO,SPECIES_ID_MINIOR,SPECIES_ID_EISCUE};
 const int abilities[]={ABILITY_ID_DISGUISE,ABILITY_ID_HUNGER_SWITCH,ABILITY_ID_SHIELDS_DOWN,ABILITY_ID_ICE_FACE};const char *why=NULL;
 for(int k=0;k<4;k++){
  gc_run_new(&run,GAME_MODES_CLASSIC,"transformed-restriction-save",777);gc_starter s={.species=SPECIES_ID_BULBASAUR,.tera_type=-1,.moves={MOVE_ID_TRANSFORM,MOVE_ID_WATER_GUN}};memset(s.ivs,15,sizeof s.ivs);CHECK(gc_run_set_starters(&run,&s,1,&why));drain();
  pr_pokemon *p=run.bt.field[0],*foe=run.bt.field[2];CHECK(p&&foe);p->species=SPECIES_ID_DITTO;p->form=0;p->gender=-1;p->level=22;p->custom_ability=ABILITY_ID_BALL_FETCH;p->ability_override=-1;pr_refresh_ability(p);pr_recalculate_stats(&run.bt,p);p->hp=p->stats[STAT_HP];run.party[run.party_order[0]].exp=gc_level_total_exp(22,gc_species(SPECIES_ID_DITTO)->growth_rate);
  foe->species=species[k];foe->form=0;foe->level=22;foe->custom_ability=abilities[k];foe->ability_override=-1;pr_refresh_ability(foe);pr_recalculate_stats(&run.bt,foe);foe->hp=foe->stats[STAT_HP];run.enemy[0].exp=gc_level_total_exp(22,gc_species(species[k])->growth_rate);foe->n_moves=2;foe->moves[0]=MOVE_ID_SPLASH;foe->moves[1]=MOVE_ID_WATER_GUN;for(int j=0;j<2;j++){foe->pp_used[j]=foe->pp_up[j]=0;foe->max_pp_override[j]=-1;}
  fight(0);CHECK(p->transformed&&p->ability==abilities[k]);CHECK(!pr_can_apply_ability(&run.bt,p,p->ability));CHECK(!run.bt.n_unported&&!run.eng.overflow);
  foe->hp=1;
  for(int step=0;step<10&&run.screen.kind==GC_SCREEN_COMMAND&&run.wave==1;step++)fight(1);
  for(int step=0;step<25&&!gc_run_can_save(&run);step++){
   gc_choice c={0};switch(run.screen.kind){case GC_SCREEN_REWARD:c.kind=GC_CH_SKIP;break;case GC_SCREEN_LEARN_MOVE:c.kind=GC_CH_FORGET;c.a=-1;break;case GC_SCREEN_EVOLVE:c.kind=GC_CH_CANCEL;c.a=1;break;case GC_SCREEN_BIOME:c.kind=GC_CH_BIOME;c.a=0;break;default:CHECK(false);}
   CHECK(gc_run_choose(&run,&c,&why));drain();
  }
  CHECK(gc_run_can_save(&run));p=run.bt.field[0];CHECK(p&&p->transformed&&p->ability==abilities[k]);CHECK(!pr_can_apply_ability(&run.bt,p,p->ability));size_t n=gc_run_save(&run,saved,sizeof saved);CHECK(n>0);CHECK(gc_run_load(&loaded,saved,n,&why));pr_pokemon *q=loaded.bt.field[0];CHECK(q&&q->transformed&&q->ability==abilities[k]);CHECK(!pr_can_apply_ability(&loaded.bt,q,q->ability));CHECK(q->hp==p->hp&&q->transform_species==p->transform_species&&q->transform_form==p->transform_form);
 }
 printf("transformed restriction command/save: %d checks passed\n",checks);return 0;
}
