/* Compare the real turn and every message against native output. */
#include "pr/engine.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "event-delivery.native.inc"
static pr_battle bt;static pr_engine e;static pr_pokemon p[4];
static pr_event collected[1024];static int nc,checks,failures,berry,stream_chunks,damage_phases;
#define CHECK(x) do{checks++;if(!(x)){if(failures<12)fprintf(stderr,"event line%d: %s\n",__LINE__,#x);failures++;}}while(0)
static const char*locale(void*ctx,const char*file,int fallback,size_t*len){(void)ctx;if(fallback)return NULL;for(unsigned i=0;i<sizeof event_locales/sizeof *event_locales;i++)if(!strcmp(file,event_locales[i].name)){*len=strlen(event_locales[i].json);return event_locales[i].json;}return NULL;}
static void collect(const pr_event*events,int n,void*ctx){(void)ctx;stream_chunks++;CHECK(n>0&&nc+n<=1024);if(nc+n>1024)return;memcpy(collected+nc,events,n*sizeof *events);nc+=n;}
static void compare_events(void){int texts=0,damage=0;pr_text_source src={locale,NULL,"en"};for(int i=0;i<nc;i++){
 const pr_event*v=&collected[i];if(v->type==PR_EV_TEXT){char actual[2048];size_t n=pr_msg_format(&v->msg,&src,actual,sizeof actual);CHECK(n<sizeof actual);CHECK(texts<104);if(texts<104){if(strcmp(actual,event_texts[texts])&&failures<12)fprintf(stderr,"text%d got[%s] expected[%s]\n",texts,actual,event_texts[texts]);CHECK(!strcmp(actual,event_texts[texts]));}texts++;}
 if(v->type==PR_EV_DAMAGE&&v->battler==2){CHECK(damage<40);if(damage<40){CHECK(v->value==event_hits[damage].amount&&v->id==event_hits[damage].result);CHECK(v->field[2].hp==event_hits[damage].hp);CHECK(v->field[2].pokemon_id_known&&v->field[2].pokemon_id==33);CHECK(v->field[2].species==SPECIES_ID_REGIROCK);}damage++;}
 }CHECK(texts==104&&damage==40);
}
void __real_pr_phase_run(pr_battle*,pr_engine*,int);
void __wrap_pr_phase_run(pr_battle*b,pr_engine*eng,int ph){__real_pr_phase_run(b,eng,ph);if(eng->pool[ph].type==PH_DAMAGE_ANIM&&eng->pool[ph].battler==2){CHECK(damage_phases<40);if(damage_phases<40)CHECK(eng->pool[ph].anim_critical==event_hits[damage_phases].critical);damage_phases++;}if(eng->pool[ph].type!=PH_BERRY)return;berry++;CHECK(damage_phases==40);CHECK(b->n_last_turn_order==7);for(int i=0;i<7;i++)CHECK(b->last_turn_order[i]==event_order[i]);
 for(int k=0;k<4;k++){const pr_pokemon*m=&p[k];CHECK(m->hp==event_after_actors[k].hp);for(int q=0;q<7;q++)CHECK(m->stat_stages[q]==event_after_actors[k].stages[q]);for(int q=0;q<2;q++)CHECK(m->pp_used[q]==event_after_actors[k].pp[q]);CHECK(m->n_move_history==event_after_actors[k].nh);for(int q=0;q<m->n_move_history;q++){const pr_turn_move*t=pr_last_move(b,m,q),*w=&event_after_actors[k].history[q];CHECK(t&&t->move==w->move&&t->result==w->result&&t->use_mode==w->use_mode&&t->n_targets==1&&t->targets[0]==w->targets[0]);}}
 CHECK(!memcmp(&b->rng,&event_after_rng[0],sizeof(pr_rng))&&!memcmp(&b->battle_rng,&event_after_rng[1],sizeof(pr_rng)));
 CHECK(b->weather==event_after_env[0]&&b->weather_turns_left==event_after_env[1]&&b->weather_max_duration==event_after_env[2]);
 CHECK(p[2].n_attacks_received==40);for(int q=0;q<32;q++){const pr_attack_record*a=&p[2].attacks_received[q],*w=&event_records[q];CHECK(a->move==w->move&&a->result==w->result&&a->critical==w->critical&&a->source_battler_index==w->source_battler_index&&a->source_id==w->source_id&&a->damage==w->damage);}
 int phases=sizeof event_phases/sizeof *event_phases;CHECK(eng->n_trace==phases);for(int i=0;i<eng->n_trace&&i<phases;i++){char name[40];snprintf(name,sizeof name,"%s",eng->trace[i]);char*colon=strchr(name,':');if(colon)*colon=0;if(strcmp(name,event_phases[i])&&failures<12)fprintf(stderr,"phase%d got%s expected%s\n",i,name,event_phases[i]);CHECK(!strcmp(name,event_phases[i]));}
}
int main(void){pr_closures_init();bt.game_mode=GAME_MODES_CLASSIC;bt.wave_index=bt.turn=1;bt.double_battle=true;bt.ignoring_effect_source=-1;bt.rng=event_before_rng[0];bt.battle_rng=event_before_rng[1];bt.rng_live=bt.battle_rng_live=true;pr_seed_from_ascii(&bt.wave_seed,"event-delivery-wave");bt.n_party[0]=bt.n_party[1]=2;bt.weather=event_before_env[0];bt.weather_turns_left=event_before_env[1];bt.weather_max_duration=event_before_env[2];bt.terrain=event_before_env[3];bt.terrain_turns_left=event_before_env[4];bt.terrain_max_duration=event_before_env[5];
#ifdef EVENT_DELIVERY_STREAM
 bt.event_sink=collect;bt.event_sink_ctx=NULL;
#endif
 for(int k=0;k<4;k++){pr_pokemon*m=&p[k];pr_reset_turn_data(m);m->id=event_before_actors[k].id;m->species=event_before_actors[k].species;m->level=1000;m->hp=event_before_actors[k].hp;m->gender=event_before_actors[k].gender;memcpy(m->stats,event_before_actors[k].stats,sizeof m->stats);for(int q=0;q<7;q++)m->stat_stages[q]=event_before_actors[k].stages[q];memset(m->ivs,15,6);m->friendship=70;m->ability=m->base_ability=m->custom_ability=event_before_actors[k].ability;m->ability_override=-1;m->tera_type=m->added_type=-1;m->on_field=true;m->is_player=k<2;m->n_moves=2;m->moves[0]=MOVE_ID_POPULATION_BOMB;m->moves[1]=MOVE_ID_INSTRUCT;for(int q=0;q<4;q++)m->max_pp_override[q]=-1;bt.field[k]=bt.party[k/2][k%2]=m;bt.commands[k]=(struct pr_turn_command){.command=COMMAND_FIGHT,.move=k?MOVE_ID_INSTRUCT:MOVE_ID_POPULATION_BOMB,.n_targets=1,.targets={k?0:2},.cursor=k?1:0,.use_mode=MOVE_USE_MODE_NORMAL};bt.pre_commands[k].command=-1;}
 pr_run_turn(&bt,&e);CHECK(berry==1&&!bt.n_unported&&!e.overflow);CHECK(!e.events_overflow);collect(e.events,e.n_events,NULL);compare_events();printf("Event delivery: %d checks, %d mismatches/uncovered; events%d chunks%d trace%d\n",checks,failures,nc,stream_chunks,e.n_trace);return failures?1:0;}
