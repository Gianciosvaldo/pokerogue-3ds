/* Raw continuation/storage boundaries; native gameplay evidence is separate. */
#include "pr/engine.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static pr_battle bt;static pr_engine e;static pr_pokemon foe;static int checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"trace guard line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
int main(void){pr_closures_init();
 for(int initial=PR_MAX_TRACE-2;initial<=PR_MAX_TRACE;initial++){
  memset(&bt,0,sizeof bt);memset(&e,0,sizeof e);memset(&foe,0,sizeof foe);
  foe.hp=100;foe.on_field=true;bt.field[2]=&foe;bt.battle_type=BATTLE_TYPE_WILD;
  e.n_levels=1;e.current=-1;e.stop=PH_VICTORY;e.stop_phase.battler=2;e.n_trace=initial;
  for(int i=0;i<initial;i++)snprintf(e.trace[i],40,"retained:%d",i);
  int ph=pr_phase_new(&e,PH_MESSAGE,-1);e.pool[ph].msg=pr_msg0("battle:hitCount");pr_phase_push(&bt,&e,ph);
  pr_continue(&bt,&e);
  CHECK(e.stop==PH_TURN_INIT&&!e.overflow&&!bt.n_unported);
  CHECK(e.n_trace==PR_MAX_TRACE&&e.trace_overflow==(initial+2>PR_MAX_TRACE));
  CHECK(e.n_events==1&&e.events[0].type==PR_EV_TEXT);
  for(int i=0;i<initial;i++){char wanted[40];snprintf(wanted,40,"retained:%d",i);CHECK(!strcmp(wanted,e.trace[i]));}
  if(initial<PR_MAX_TRACE)CHECK(!strcmp(e.trace[initial],"VictoryPhase:2"));
  if(initial+1<PR_MAX_TRACE)CHECK(!strcmp(e.trace[initial+1],"MessagePhase"));
 }
 /* Victory is also logged when it is the only continuation step. */
 memset(&e,0,sizeof e);e.n_levels=1;e.current=-1;e.stop=PH_VICTORY;e.n_trace=PR_MAX_TRACE;
 pr_continue(&bt,&e);CHECK(e.trace_overflow&&e.n_trace==PR_MAX_TRACE);
 CHECK(!e.overflow&&!bt.n_unported&&e.stop==PH_TURN_INIT&&!e.n_events);
 /* Diagnostic exhaustion must not suppress the genuine phase-storage error. */
 memset(&e,0,sizeof e);for(int i=0;i<PR_MAX_PHASES;i++)CHECK(pr_phase_new(&e,PH_MESSAGE,-1)>=0);
 CHECK(pr_phase_new(&e,PH_MESSAGE,-1)<0&&e.overflow);
 printf("Trace continuation/prefix and genuine engine-limit guards: %d checks passed\n",checks);return 0;
}
