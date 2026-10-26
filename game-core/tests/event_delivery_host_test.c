/* Ordinary doubles commands and the actual core-to-controller event handoff. */
#define main retained_doubles_main
#include "doubles_host_test.c"
#undef main
static int battle_notices(gc_run*run){gc_event ev;int next=0,count=0;while(gc_run_next_event(run,&ev)){if(ev.kind!=GC_EV_BATTLE)continue;CHECK(ev.a==next&&ev.b>0);next+=ev.b;count++;}CHECK(next==run->n_battle_events&&!run->ev_overflow);return count;}
static void turn_with_weak_armor(gc_run*run){
 choose(run,(gc_choice){GC_CH_FIGHT,0,-1,0});CHECK(run->screen.kind==GC_SCREEN_TARGET);choose(run,(gc_choice){GC_CH_TARGET,2,0,0});CHECK(run->screen.slot==1);choose(run,(gc_choice){GC_CH_FIGHT,1,-1,0});CHECK(run->screen.kind==GC_SCREEN_TARGET);const char*why=NULL;CHECK(gc_run_choose(run,&(gc_choice){GC_CH_TARGET,0,0,0},&why));
 CHECK(run->screen.kind==GC_SCREEN_COMMAND&&run->screen.slot==0&&run->bt.turn==2);CHECK(!run->eng.overflow&&!run->eng.events_overflow&&!run->battle_events_overflow&&!run->bt.n_unported);CHECK(run->n_battle_events>PR_MAX_EVENTS&&run->n_battle_events<=GC_MAX_BATTLE_EVENTS);CHECK(battle_notices(run)==2);
 int hits=0,total=0;for(int i=0;i<run->n_battle_events;i++){const pr_event*e=&run->battle_events[i];if(e->type==PR_EV_DAMAGE&&e->battler==2){hits++;total+=e->value;CHECK(e->field[2].pokemon_id_known&&e->field[2].pokemon_id==33);}}
 CHECK(hits==20&&total==2760-run->bt.field[2]->hp);CHECK(run->party[0].b.pp_used[0]==2&&run->party[1].b.pp_used[1]==1);CHECK(!gc_run_can_save(run)&&!gc_run_save(run,save,sizeof save));
 int events=run->n_battle_events;gc_ev_battle(run);CHECK(run->n_battle_events==events);CHECK(!gc_run_next_event(run,&(gc_event){0}));
}
static void early_reveal_and_downstream_limit(void){setup(1);gc_ai_reset(&r);pr_ai_mon*m=pr_ai_mon_of(&gc_int(&r)->ai,r.bt.field[2]);CHECK(m);m->ability_revealed=false;memset(&r.eng,0,sizeof r.eng);r.n_battle_events=0;r.battle_events_overflow=false;
 CHECK(pr_event_add(&r.bt,&r.eng,PR_EV_ABILITY_SHOW,2,-1,r.bt.field[2]->ability,0,NULL));for(int i=1;i<PR_MAX_EVENTS;i++)CHECK(pr_event_add(&r.bt,&r.eng,PR_EV_NONE,-1,-1,0,i,NULL));CHECK(pr_event_add(&r.bt,&r.eng,PR_EV_NONE,-1,-1,0,PR_MAX_EVENTS,NULL));CHECK(m->ability_revealed&&r.n_battle_events==PR_MAX_EVENTS);
 for(int i=PR_MAX_EVENTS+1;i<GC_MAX_BATTLE_EVENTS;i++){CHECK(pr_event_add(&r.bt,&r.eng,PR_EV_NONE,-1,-1,0,i,NULL));}
 gc_ev_battle(&r);CHECK(r.n_battle_events==GC_MAX_BATTLE_EVENTS&&!r.battle_events_overflow);CHECK(pr_event_add(&r.bt,&r.eng,PR_EV_NONE,-1,-1,0,999,NULL));gc_ev_battle(&r);CHECK(r.n_battle_events==GC_MAX_BATTLE_EVENTS&&r.battle_events_overflow&&!r.eng.events_overflow);
}
int main(void){early_reveal_and_downstream_limit();setup(1);CHECK(r.double_battle);r.n_items=0;gc_items_to_battle(&r);
 for(int k=0;k<4;k++){gc_mon*m=k<2?&r.party[k]:&r.enemy[k-2];if(k<2){gc_starter s={.species=SPECIES_ID_BLISSEY,.female=true,.tera_type=-1,.moves={MOVE_ID_POPULATION_BOMB,MOVE_ID_INSTRUCT,MOVE_ID_SPLASH}};memset(s.ivs,15,6);gc_mon_new_player(&r,m,&s,1000);}else{pr_enemy_gen g={.id=11u*(k+1),.gender=-1,.friendship=70,.tera_type=-1};for(int i=0;i<6;i++)g.ivs[i]=15;gc_mon_new_enemy(&r,m,SPECIES_ID_REGIROCK,1000,&g,0);gc_mon_set_move(m,0,MOVE_ID_SPLASH);}
  pr_pokemon*p=&m->b;pr_calculate_stats(&r.bt,p);p->id=11*(k+1);p->on_field=true;p->ability=p->base_ability=p->custom_ability=k<2?ABILITY_ID_NATURAL_CURE:ABILITY_ID_WEAK_ARMOR;p->ability_override=-1;p->passive_flag=false;p->stat_stages[STAT_SPD-1]=(int[]){0,-1,-3,-2}[k];r.bt.field[k]=r.bt.party[k/2][k%2]=p;
 }
 r.party[0].b.stat_stages[STAT_ATK-1]=-6;r.party[0].b.stat_stages[STAT_ACC-1]=6;r.bt.weather=WEATHER_TYPE_SANDSTORM;r.bt.weather_turns_left=r.bt.weather_max_duration=0;r.bt.rng=r.run.rng;
 size_t n=gc_run_save(&r,save,sizeof save);const char*why=NULL;CHECK(n&&gc_run_load(&back,save,n,&why)&&!back.load_dropped);drain(&back);CHECK(back.bt.event_sink&&back.bt.event_sink_ctx==&back);turn_with_weak_armor(&r);turn_with_weak_armor(&back);CHECK(r.n_battle_events==back.n_battle_events&&rngsame(r.bt.rng,back.bt.rng)&&rngsame(r.bt.battle_rng,back.bt.battle_rng));for(int i=0;i<r.n_battle_events;i++){const pr_event*a=&r.battle_events[i],*b=&back.battle_events[i];CHECK(a->type==b->type&&a->battler==b->battler&&a->id==b->id&&a->value==b->value);for(int k=0;k<4;k++)CHECK(a->field[k].hp==b->field[k].hp&&a->field[k].species==b->field[k].species);}
 printf("Event delivery controller: normal twenty-hit turn, chunks, save/rebind, reveal and explicit downstream limit; %d checks passed\n",checks);return 0;}
