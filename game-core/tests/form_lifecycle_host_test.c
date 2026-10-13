/* Indexed form changes at a real turn boundary and command save. */
#include "../src/internal.h"
#include <stdio.h>
#include <stdlib.h>
static gc_run run,loaded;static unsigned char data[GC_SAVE_MAX];static int checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"form lifecycle host line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
int main(void){
 const char *why=NULL;gc_run_new(&run,GAME_MODES_CLASSIC,"form-lifecycle-save",777);
 gc_starter starter={.species=SPECIES_ID_BULBASAUR,.tera_type=-1,.moves={MOVE_ID_SPLASH}};memset(starter.ivs,15,sizeof starter.ivs);
 CHECK(gc_run_set_starters(&run,&starter,1,&why));gc_event ev;while(gc_run_next_event(&run,&ev)){}CHECK(gc_run_can_save(&run));
 gc_mon *m=&run.party[run.party_order[0]];pr_pokemon *p=&m->b;
 starter.species=SPECIES_ID_GENGAR;starter.ability_index=2;gc_mon_new_player(&run,m,&starter,50);p->on_field=true;run.bt.field[0]=p;run.bt.party[0][0]=p;
 CHECK(m->ability_index==2&&p->ability_index==2&&p->base_ability==ABILITY_ID_CURSED_BODY);
 p->terastallized=true;p->stellar_types_boosted=6;p->queued_mega=true;
 /* TurnStart calls the actual Mega helper, then engine_returned syncs the bridge. */
 /* The game-core menu currently skips pre-turn Mega input. Supply that existing
  * internal engine input explicitly; this does not invent a Mega UI choice. */
 run.bt.commands[0]=(struct pr_turn_command){.command=COMMAND_FIGHT,.move=MOVE_ID_SPLASH,.targets={0},.n_targets=1};
 run.bt.commands[2]=(struct pr_turn_command){.command=COMMAND_FIGHT,.move=MOVE_ID_SPLASH,.targets={2},.n_targets=1};
 run.bt.pre_commands[0]=(struct pr_turn_command){.command=COMMAND_MEGA};
 gc_battle_run(&run,-2);
 CHECK(pr_is_mega(p)&&p->ability_index==1&&m->ability_index==1);
 CHECK(p->base_ability==ABILITY_ID_SHADOW_TAG&&p->ability==ABILITY_ID_SHADOW_TAG);
 CHECK(!p->terastallized&&p->stellar_types_boosted==0);
 while(gc_run_next_event(&run,&ev)){}CHECK(!gc_run_can_save(&run));
 /* Mid-turn saves remain unavailable. Test retained Mega form/index at the
  * separately legal pre-turn command boundary using the existing constructor. */
 gc_run_new(&run,GAME_MODES_CLASSIC,"form-lifecycle-save",777);
 gc_starter initial={.species=SPECIES_ID_BULBASAUR,.tera_type=-1,.moves={MOVE_ID_SPLASH}};memset(initial.ivs,15,sizeof initial.ivs);
 CHECK(gc_run_set_starters(&run,&initial,1,&why));while(gc_run_next_event(&run,&ev)){}
 m=&run.party[run.party_order[0]];p=&m->b;starter.form=1;starter.ability_index=1;gc_mon_new_player(&run,m,&starter,50);p->on_field=true;run.bt.field[0]=p;run.bt.party[0][0]=p;
 CHECK(gc_run_can_save(&run)&&pr_is_mega(p));
 size_t n=gc_run_save(&run,data,sizeof data);CHECK(n>0);CHECK(gc_run_load(&loaded,data,n,&why));pr_pokemon *q=loaded.bt.field[0];
 CHECK(q&&q->ability_index==1&&loaded.party[loaded.party_order[0]].ability_index==1&&q->ability==ABILITY_ID_SHADOW_TAG);
 pr_reset_summon_data(q);CHECK(q->ability==ABILITY_ID_SHADOW_TAG&&q->base_ability==ABILITY_ID_SHADOW_TAG);
 /* The ordinary primary slot becomes Shadow Tag, including its two-name message. */
 starter.form=0;starter.ability_index=0;gc_mon_new_player(&run,m,&starter,50);p->on_field=true;run.bt.field[0]=p;pr_mega_evolve(&run.bt,p);
 CHECK(p->ability==ABILITY_ID_SHADOW_TAG&&p->ability_index==0);
 printf("Form lifecycle turn/bridge/save/reset: %d checks passed\n",checks);return 0;
}
