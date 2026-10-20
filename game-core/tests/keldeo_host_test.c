/* Actual LearnMove ownership, delayed cached forms and legal save boundaries. */
#include "../src/internal.h"
#include <stdio.h>
#include <stdlib.h>
static gc_run run,loaded;static gc_system sys;static unsigned char saved[GC_SAVE_MAX];static int checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"Keldeo host line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
static void drain(void){gc_event ev;while(gc_run_next_event(&run,&ev)){} }
static void start(void){const char *why=NULL;gc_system_init_defaults(&sys);gc_run_new(&run,GAME_MODES_CLASSIC,"keldeo-learn-save",777);gc_starter s={.species=SPECIES_ID_KELDEO,.tera_type=POKEMON_TYPE_STELLAR,.moves={MOVE_ID_SPLASH}};memset(s.ivs,15,sizeof s.ivs);CHECK(gc_run_set_starters(&run,&s,1,&why));drain();run.sys=&sys;CHECK(gc_system_set_pokemon_caught(&sys,&run.party[0],true,false));CHECK(gc_run_can_save(&run));gc_rq_clear(&run);}
int main(void){
 const char *why=NULL;start();pr_pokemon *p=run.bt.field[0];CHECK(p&&p->species==SPECIES_ID_KELDEO&&p->form==0);
 p->terastallized=true;p->stellar_types_boosted=6;p->added_type=POKEMON_TYPE_GRASS;
 p->tags[p->n_tags++]=(pr_battler_tag){.type=BATTLER_TAG_TYPE_AUTOTOMIZED,.turn_count=1,.source_move=MOVE_ID_AUTOTOMIZE,.source_id=-1,.autotomize_count=2};
 gc_rq_push(&run,gc_rp(RP_LEARN_MOVE,0,MOVE_ID_GROWL,0));
 gc_rphase learn=gc_rp(RP_LEARN_MOVE,0,MOVE_ID_SECRET_SWORD,0);CHECK(gc_phase_start(&run,&learn));CHECK(p->form==0&&p->moves[1]==MOVE_ID_SECRET_SWORD);CHECK(gc_rq_has(&run,RP_REWARD_FORM));
 gc_rphase pending;CHECK(gc_rq_next(&gc_int(&run)->rq,&pending));CHECK(pending.type==RP_LEARN_MOVE&&pending.b==MOVE_ID_GROWL);
 gc_rphase form;CHECK(gc_rq_next(&gc_int(&run)->rq,&form));CHECK(form.type==RP_REWARD_FORM&&form.a==0&&form.b==1);
 /* A queued target is cached: changing the move afterward must not re-evaluate eligibility. */
 gc_mon_set_move(&run.party[0],1,MOVE_ID_TACKLE);CHECK(gc_phase_start(&run,&form));CHECK(p->form==1&&p->moves[1]==MOVE_ID_TACKLE);CHECK(p->terastallized&&p->stellar_types_boosted==6&&p->added_type==POKEMON_TYPE_GRASS);CHECK(!pr_get_battler_tag(p,BATTLER_TAG_TYPE_AUTOTOMIZED));CHECK(!p->n_form_change_abilities);
 CHECK(sys.dex[SPECIES_ID_KELDEO].seen_attr&&sys.dex[SPECIES_ID_KELDEO].caught_attr);
 int order=0;gc_event ev;while(gc_run_next_event(&run,&ev))if(ev.kind==GC_EV_MESSAGE){if(order==0)CHECK(!strcmp(ev.msg.key,"battle:learnMove"));else if(order==1)CHECK(!strcmp(ev.msg.key,"battlePokemonForm:formChange"));order++;}CHECK(order==2);
 CHECK(gc_run_can_save(&run));size_t n=gc_run_save(&run,saved,sizeof saved);CHECK(n>0);CHECK(gc_run_load(&loaded,saved,n,&why));CHECK(!loaded.load_dropped&&loaded.bt.field[0]->form==1&&loaded.bt.field[0]->terastallized);CHECK(!pr_get_battler_tag(loaded.bt.field[0],BATTLER_TAG_TYPE_AUTOTOMIZED));
 /* A later successful learn callback notices that Secret Sword was forgotten. */
 learn.b=MOVE_ID_GROWL;CHECK(gc_phase_start(&run,&learn));CHECK(gc_rq_next(&gc_int(&run)->rq,&form));CHECK(form.b==0);CHECK(gc_phase_start(&run,&form));CHECK(p->form==0);drain();CHECK(!run.bt.n_unported);
 /* Daily rejects both form directions, even after a successful learn. */
 for(int f=0;f<2;f++){start();p=run.bt.field[0];p->form=f;run.bt.game_mode=GAME_MODES_DAILY;learn.b=f?MOVE_ID_GROWL:MOVE_ID_SECRET_SWORD;CHECK(gc_phase_start(&run,&learn));CHECK(!gc_rq_has(&run,RP_REWARD_FORM)&&p->form==f);}
 /* Known empty challenge sets are supported; unknown/passive/Flip Stat inputs stay visible. */
 start();p=run.bt.field[0];run.bt.game_mode=GAME_MODES_CHALLENGE;run.bt.challenge_flags_known=true;pr_update_passive(&run.bt,p);CHECK(!run.bt.n_unported);
 run.bt.challenge_flags_known=false;pr_update_passive(&run.bt,p);CHECK(run.bt.n_unported>0);run.bt.n_unported=0;run.bt.challenge_flags_known=true;run.bt.challenge_flags=1u<<CHALLENGES_PASSIVES;pr_update_passive(&run.bt,p);CHECK(run.bt.n_unported>0);run.bt.n_unported=0;run.bt.challenge_flags=1u<<CHALLENGES_FLIP_STAT;pr_calculate_stats(&run.bt,p);CHECK(run.bt.n_unported>0);
 start();p=run.bt.field[0];gc_mon_set_move(&run.party[0],0,MOVE_ID_SECRET_SWORD);run.bt.game_mode=-1;uint16_t missing_target;CHECK(!pr_learned_form_target(&run.bt,p,&missing_target));CHECK(run.bt.n_unported>0);
 printf("Keldeo learn/cached-form/profile/save: %d checks passed\n",checks);return 0;
}
