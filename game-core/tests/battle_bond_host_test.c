/* Actual KO/wave progression, legal saves and the new-biome Ash cleanup caller. */
#include "../src/internal.h"
#include <stdio.h>
#include <stdlib.h>
static gc_run run,loaded;static unsigned char saved[GC_SAVE_MAX];static int checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"Battle Bond host line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
static bool applied(const pr_pokemon *p){for(int i=0;i<p->n_ab_applied_summon;i++)if(p->ab_applied_summon[i]==ABILITY_ID_BATTLE_BOND)return true;return false;}
int main(void){
 const char *why=NULL;gc_run_new(&run,GAME_MODES_CLASSIC,"battle-bond-save",777);
 gc_starter s={.species=SPECIES_ID_BULBASAUR,.tera_type=-1,.moves={MOVE_ID_TACKLE}};memset(s.ivs,15,sizeof s.ivs);
 CHECK(gc_run_set_starters(&run,&s,1,&why));gc_event ev;while(gc_run_next_event(&run,&ev)){}CHECK(gc_run_can_save(&run));
 pr_pokemon *p=run.bt.field[0],*enemy=run.bt.field[2];CHECK(p&&enemy);
 p->ability_override=ABILITY_ID_BATTLE_BOND;pr_refresh_ability(p);
 enemy->species=SPECIES_ID_RATTATA;enemy->form=0;enemy->custom_ability=ABILITY_ID_BALL_FETCH;enemy->ability_override=-1;pr_refresh_ability(enemy);enemy->hp=1;
 gc_choice fight={GC_CH_FIGHT,0,-1,0};CHECK(gc_run_choose(&run,&fight,&why));while(gc_run_next_event(&run,&ev)){}
 CHECK(applied(p)&&p->stat_stages[0]==1&&p->stat_stages[2]==1&&p->stat_stages[4]==1);
 CHECK(!gc_run_can_save(&run));
 for(int step=0;step<20&&!gc_run_can_save(&run);step++){
  gc_choice pick={0};
  if(run.screen.kind==GC_SCREEN_REWARD)pick.kind=GC_CH_SKIP;
  else if(run.screen.kind==GC_SCREEN_LEARN_MOVE){pick.kind=GC_CH_FORGET;pick.a=-1;}
  else if(run.screen.kind==GC_SCREEN_EVOLVE){pick.kind=GC_CH_CANCEL;pick.a=1;}
  else if(run.screen.kind==GC_SCREEN_BIOME){pick.kind=GC_CH_BIOME;pick.a=0;}
  else{fprintf(stderr,"unexpected battle continuation screen%d\n",run.screen.kind);return 1;}
  CHECK(gc_run_choose(&run,&pick,&why));while(gc_run_next_event(&run,&ev)){}
 }
 CHECK(gc_run_can_save(&run)&&run.wave==2&&applied(p));
 size_t n=gc_run_save(&run,saved,sizeof saved);CHECK(n>0);CHECK(gc_run_load(&loaded,saved,n,&why));pr_pokemon *q=loaded.bt.field[0];
 CHECK(q&&applied(q)&&q->ability==ABILITY_ID_BATTLE_BOND&&q->stat_stages[0]==1&&q->stat_stages[2]==1&&q->stat_stages[4]==1);
 /* A saved used flag refuses another activation; switching clears summon data. */
 loaded.bt.engine=&loaded.eng;pr_ab_params pp={.pokemon=q,.move=-1};int events=loaded.eng.n_events;
 pr_apply_ab_attrs(&loaded.bt,PR_AC_PostVictoryAbAttr,&pp);CHECK(loaded.eng.n_events==events&&applied(q));
 pr_reset_summon_data(q);CHECK(!applied(q)&&q->ability==q->base_ability);
 /* Source newBattle dispatches PostBattleInit only when resetArenaState is true.
  * Ordinary continuation keeps Ash. The reset-arena controller follows below. */
 gc_mon *m=&run.party[run.party_order[0]];s.species=SPECIES_ID_BATTLE_BOND_GRENINJA;s.form=1;s.ability_index=0;
 gc_mon_new_player(&run,m,&s,50);m->b.on_field=true;run.bt.field[0]=&m->b;run.bt.party[0][0]=&m->b;
 CHECK(m->b.form==1&&m->b.ability==ABILITY_ID_BATTLE_BOND);
 CHECK(!gc_battle_setup_field(&run,false));CHECK(m->b.form==1&&m->b.ability==ABILITY_ID_BATTLE_BOND);
 /* The previous trainer type does not recall the team before an incoming
  * ordinary wild wave: the unchanged source tests the new battle type. */
 gc_int(&run)->last_battle_type=BATTLE_TYPE_TRAINER;run.wave=1;m->b.wave_endured=true;m->b.temp_wave_turn_count=8;
 gc_new_battle(&run);
 CHECK(run.wave==2&&run.battle_type==BATTLE_TYPE_WILD&&m->b.form==1);
 CHECK(!m->b.wave_endured&&m->b.temp_wave_turn_count==1);
 CHECK(m->b.on_field&&!m->b.switch_out_status);
 /* The real new-battle controller enters the next biome and recalls Ash only
  * after its init callback has queued and executed the base-form change. */
 run.wave=10;run.n_battle_events=0;m->b.terastallized=true;m->b.tera_type=POKEMON_TYPE_WATER;m->b.stellar_types_boosted=(1u<<1)|(1u<<2);
 gc_new_battle(&run);
 CHECK(!m->b.terastallized&&!m->b.stellar_types_boosted);
 CHECK(run.wave==11&&m->b.form==0&&m->b.ability==ABILITY_ID_BATTLE_BOND);
 CHECK(m->b.on_field&&!m->b.switch_out_status&&!applied(&m->b));
 int recalls=0;for(int i=0;i<run.n_battle_events;i++)if(run.battle_events[i].type==PR_EV_SWITCH_OUT){
  CHECK(run.battle_events[i].field[0].species==SPECIES_ID_BATTLE_BOND_GRENINJA&&run.battle_events[i].field[0].form==0);recalls++;
 }
 CHECK(recalls==1);
 while(gc_run_next_event(&run,&ev)){}
 CHECK(gc_run_can_save(&run));n=gc_run_save(&run,saved,sizeof saved);CHECK(n>0);
 CHECK(gc_run_load(&loaded,saved,n,&why));q=loaded.bt.field[0];
 CHECK(q&&!q->terastallized&&!q->stellar_types_boosted);
 CHECK(q&&q->form==0&&q->ability==ABILITY_ID_BATTLE_BOND&&!applied(q));
 printf("Battle Bond KO/wave/save/new-biome cleanup: %d checks passed\n",checks);return 0;
}
