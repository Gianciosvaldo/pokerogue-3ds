#include "pr/engine.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static pr_battle bt;static pr_engine e;static pr_event seen[2048];static int ns,chunks,checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"event guard line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
static void sink(const pr_event*events,int n,void*ctx){CHECK(ctx==&bt&&n>0&&ns+n<=2048);memcpy(seen+ns,events,n*sizeof *events);ns+=n;chunks++;}
static pr_event*add(int v){return pr_event_add(&bt,&e,PR_EV_NONE,-1,-1,0,v,NULL);}
static void reset(void){memset(&bt,0,sizeof bt);memset(&e,0,sizeof e);ns=chunks=0;}
int main(void){pr_closures_init();reset();for(int i=0;i<PR_MAX_EVENTS;i++)CHECK(add(i));CHECK(!add(999)&&e.events_overflow&&e.n_events==PR_MAX_EVENTS);CHECK(e.events[0].value==0&&e.events[PR_MAX_EVENTS-1].value==PR_MAX_EVENTS-1);pr_event_acknowledge(&e);CHECK(add(555)&&e.events_overflow);
 reset();bt.event_sink=sink;bt.event_sink_ctx=&bt;int total=5*PR_MAX_EVENTS+17;for(int i=0;i<total;i++){pr_event*v=add(i);CHECK(v);v->actor_ids_known=true;v->actor_id=(uint32_t)i;v->target_id=(uint32_t)i^0x80000000u;}sink(e.events,e.n_events,&bt);pr_event_acknowledge(&e);CHECK(ns==total&&chunks==6&&!e.events_overflow);for(int i=0;i<total;i++)CHECK(seen[i].value==i&&seen[i].actor_ids_known&&seen[i].actor_id==(uint32_t)i&&seen[i].target_id==((uint32_t)i^0x80000000u));
 reset();bt.event_sink=sink;bt.event_sink_ctx=&bt;for(int i=0;i<PR_MAX_EVENTS;i++)CHECK(add(i));sink(e.events,e.n_events,&bt);pr_event_acknowledge(&e);CHECK(e.n_events==PR_MAX_EVENTS&&e.events[0].value==0);CHECK(add(12345));CHECK(chunks==1&&e.n_events==1&&e.n_events_acknowledged==0);sink(e.events,e.n_events,&bt);CHECK(ns==PR_MAX_EVENTS+1&&seen[ns-1].value==12345);
 reset();for(int i=0;i<PR_MAX_EVENTS;i++)CHECK(add(i));pr_event_acknowledge(&e);CHECK(add(66)&&!e.events_overflow&&e.n_events==1);pr_event_acknowledge(&e);e.n_events=0;CHECK(add(77)&&e.n_events_acknowledged==0&&e.n_events==1);
 /* A real Tera owner annotates the event returned across a flush boundary. */
 reset();static pr_pokemon p;p=(pr_pokemon){0};p.id=123;p.species=SPECIES_ID_MEW;p.level=50;p.hp=p.stats[STAT_HP]=100;p.gender=0;p.added_type=-1;p.tera_type=POKEMON_TYPE_FIRE;p.is_player=p.on_field=true;p.ability_override=-1;bt.party[0][0]=bt.field[0]=&p;bt.n_party[0]=1;bt.player_teras_used_known=true;bt.engine=&e;bt.event_sink=sink;bt.event_sink_ctx=&bt;e.n_levels=1;e.current=-1;for(int i=0;i<PR_MAX_EVENTS;i++)CHECK(add(i));int ph=pr_phase_new(&e,PH_TERA,0);e.pool[ph].pokemon=&p;pr_phase_run(&bt,&e,ph);CHECK(ns==PR_MAX_EVENTS&&e.n_events==1&&!e.events_overflow);CHECK(e.events[0].type==PR_EV_COMMON_ANIM&&e.events[0].actor_ids_known&&e.events[0].actor_id==p.id&&e.events[0].target_id==p.id);
 printf("Event stream/default/acknowledgement/Tera guards: %d checks passed\n",checks);return 0;}
