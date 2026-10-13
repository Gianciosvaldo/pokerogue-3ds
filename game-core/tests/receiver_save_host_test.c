/* Command-save integration for a copied temporary primary ability. */
#include "../src/internal.h"
#include <stdio.h>
#include <stdlib.h>
static gc_run run,loaded;
static unsigned char data[GC_SAVE_MAX];
static int checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"Receiver save line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
int main(void){
 const char *why=NULL;gc_run_new(&run,GAME_MODES_CLASSIC,"receiver-save",777);
 gc_starter starter={.species=SPECIES_ID_BULBASAUR,.tera_type=-1,.moves={MOVE_ID_SPLASH}};
 memset(starter.ivs,15,sizeof starter.ivs);CHECK(gc_run_set_starters(&run,&starter,1,&why));
 gc_event ev;while(gc_run_next_event(&run,&ev)){}CHECK(gc_run_can_save(&run));
 pr_pokemon *p=run.bt.field[0];CHECK(p!=NULL);p->base_ability=p->ability=ABILITY_ID_RECEIVER;p->ability_override=-1;
 pr_pokemon victim={.is_player=true,.ability=ABILITY_ID_BALL_FETCH,.base_ability=ABILITY_ID_BALL_FETCH,.species=SPECIES_ID_RATTATA,.hp=0};
 run.bt.engine=&run.eng;pr_ab_params pp={.pokemon=p,.victim=&victim,.move=-1};
 pr_apply_ab_attrs(&run.bt,PR_AC_PostKnockOutAbAttr,&pp);run.bt.engine=NULL;
 CHECK(p->ability==ABILITY_ID_BALL_FETCH&&p->ability_override==ABILITY_ID_BALL_FETCH&&p->base_ability==ABILITY_ID_RECEIVER);
 CHECK(p->n_ab_applied_summon==1&&p->ab_applied_summon[0]==ABILITY_ID_RECEIVER);
 size_t n=gc_run_save(&run,data,sizeof data);CHECK(n>0);CHECK(gc_run_load(&loaded,data,n,&why));
 pr_pokemon *q=loaded.bt.field[0];CHECK(q&&q->ability==ABILITY_ID_BALL_FETCH&&q->ability_override==ABILITY_ID_BALL_FETCH&&q->base_ability==ABILITY_ID_RECEIVER);
 CHECK(q->n_ab_applied_summon==1&&q->ab_applied_summon[0]==ABILITY_ID_RECEIVER);
 pr_reset_summon_data(q);CHECK(q->ability==ABILITY_ID_RECEIVER&&q->ability_override==-1&&q->n_ab_applied_summon==0);
 /* Passive activation replaces the primary, and both permanent identities survive. */
 p->ability=p->base_ability=ABILITY_ID_BALL_FETCH;p->ability_override=-1;p->passive=ABILITY_ID_POWER_OF_ALCHEMY;p->passive_flag=true;p->custom_passive=ABILITY_ID_POWER_OF_ALCHEMY;
 victim.ability=ABILITY_ID_SERENE_GRACE;run.bt.engine=&run.eng;pr_apply_ab_attrs(&run.bt,PR_AC_PostKnockOutAbAttr,&pp);run.bt.engine=NULL;
 CHECK(p->ability==ABILITY_ID_SERENE_GRACE&&p->passive==ABILITY_ID_POWER_OF_ALCHEMY&&p->base_ability==ABILITY_ID_BALL_FETCH);
 n=gc_run_save(&run,data,sizeof data);CHECK(n>0);CHECK(gc_run_load(&loaded,data,n,&why));q=loaded.bt.field[0];
 CHECK(q->ability==ABILITY_ID_SERENE_GRACE&&q->passive==ABILITY_ID_POWER_OF_ALCHEMY&&q->base_ability==ABILITY_ID_BALL_FETCH);
 pr_reset_summon_data(q);CHECK(q->ability==ABILITY_ID_BALL_FETCH&&q->passive==ABILITY_ID_POWER_OF_ALCHEMY);
 printf("Receiver command/save/reset: %d checks passed\n",checks);return 0;
}
