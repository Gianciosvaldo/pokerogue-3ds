/* A real Revival Blessing command pauses for a fainted-party pick, then saves the revived member. */
#include "../src/internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static gc_run run,loaded;static unsigned char bytes[GC_SAVE_MAX];static unsigned checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"Revival host line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
static void drain(void){gc_event e;while(gc_run_next_event(&run,&e)){}}
static bool choose(uint8_t kind,int a){const char *why=NULL;gc_choice c={kind,a,-1,0};bool ok=gc_run_choose(&run,&c,&why);if(!ok)fprintf(stderr,"choice %u/%d: %s\n",kind,a,why?why:"?");drain();return ok;}
int main(void)
{
 const char *why=NULL;gc_run_new(&run,GAME_MODES_CLASSIC,"revival-host",777);
 gc_starter starters[3]={{.species=SPECIES_ID_CHARMANDER,.tera_type=-1,.moves={MOVE_ID_TACKLE}},{.species=SPECIES_ID_BULBASAUR,.tera_type=-1,.moves={MOVE_ID_SPLASH}},{.species=SPECIES_ID_SQUIRTLE,.tera_type=-1,.moves={MOVE_ID_SPLASH}}};
 for(int i=0;i<3;i++)memset(starters[i].ivs,15,sizeof starters[i].ivs);
 CHECK(gc_run_set_starters(&run,starters,3,&why));drain();CHECK(run.screen.kind==GC_SCREEN_COMMAND);
 gc_starter paw={.species=SPECIES_ID_PAWMOT,.tera_type=-1,.moves={MOVE_ID_REVIVAL_BLESSING}};memset(paw.ivs,15,sizeof paw.ivs);
 gc_mon *m=&run.party[run.party_order[0]];gc_mon_new_player(&run,m,&paw,50);m->b.on_field=true;run.bt.field[0]=&m->b;run.bt.party[0][0]=&m->b;run.bt.participants[0]=m->b.id;run.bt.n_participants=1;
 pr_pokemon *lead=run.bt.field[0],*enemy=run.bt.field[2];CHECK(lead&&enemy);
 lead->level=50;pr_recalculate_stats(&run.bt,lead);lead->hp=pr_get_stat(lead,STAT_HP,true);lead->moves[0]=MOVE_ID_REVIVAL_BLESSING;lead->pp_used[0]=0;
 enemy->moves[0]=MOVE_ID_SPLASH;enemy->n_moves=1;enemy->custom_ability=ABILITY_ID_BALL_FETCH;enemy->ability_override=-1;pr_refresh_ability(enemy);
 int chosen=run.party_order[1];pr_pokemon *p=&run.party[chosen].b;p->level=50;pr_recalculate_stats(&run.bt,p);p->hp=0;p->status=STATUS_EFFECT_FAINT;p->damage_taken=37;p->pending_status=STATUS_EFFECT_POISON;p->hit_count=3;
 int32_t expected=pr_get_stat(p,STAT_HP,true)/2;if(expected<1)expected=1;
 CHECK(choose(GC_CH_FIGHT,0));CHECK(run.screen.kind==GC_SCREEN_SWITCH&&run.screen.revival_blessing&&run.eng.stop==PH_REVIVAL_BLESSING);
 CHECK(run.screen.pick_ok[chosen]&&!run.screen.pick_ok[run.party_order[0]]&&!run.screen.can_skip&&!gc_run_can_save(&run));
 gc_choice bad={GC_CH_PICK,run.party_order[0],-1,0};CHECK(!gc_run_choose(&run,&bad,&why));CHECK(why&&!strcmp(why,"partyUiHandler:hasEnergy"));
 bad.a=-1;CHECK(!gc_run_choose(&run,&bad,&why));
 CHECK(choose(GC_CH_PICK,chosen));CHECK(!run.eng.awaiting_pick&&p->hp==expected&&p->status==0&&p->damage_taken==0&&p->pending_status==0&&p->hit_count==0);
 CHECK(run.screen.kind==GC_SCREEN_COMMAND&&!run.screen.revival_blessing);
 /* A normal victory reaches a legal save point. */
 lead->moves[0]=MOVE_ID_TACKLE;lead->pp_used[0]=0;enemy->hp=1;CHECK(choose(GC_CH_FIGHT,0));
 for(int i=0;i<20&&!gc_run_can_save(&run);i++){
  if(run.screen.kind==GC_SCREEN_REWARD)CHECK(choose(GC_CH_SKIP,0));
  else if(run.screen.kind==GC_SCREEN_LEARN_MOVE)CHECK(choose(GC_CH_FORGET,-1));
  else if(run.screen.kind==GC_SCREEN_EVOLVE)CHECK(choose(GC_CH_CANCEL,1));
  else if(run.screen.kind==GC_SCREEN_BIOME)CHECK(choose(GC_CH_BIOME,0));
  else {fprintf(stderr,"unexpected continuation %d\n",run.screen.kind);return 1;}
 }
 CHECK(gc_run_can_save(&run));size_t n=gc_run_save(&run,bytes,sizeof bytes);CHECK(n>0&&gc_run_load(&loaded,bytes,n,&why));
 pr_pokemon *q=NULL;for(int i=0;i<loaded.n_party;i++)if(loaded.party[i].b.id==p->id)q=&loaded.party[i].b;
 CHECK(q&&q->hp==expected&&q->status==0&&!loaded.screen.revival_blessing);
 printf("Revival Blessing command/pick/save: %u checks passed\n",checks);return 0;
}
