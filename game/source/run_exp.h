/* EXP display starts from the first pending native phase's recorded state.
 * A cap can discard an award; subtracting nominal gains cannot recover it. */
#ifndef RUN_EXP_H
#define RUN_EXP_H
#include "gc/run.h"
static inline gc_mon rs_exp_before_pending(const gc_mon *actual,int party,
                                           const gc_event *pending,int count)
{
  gc_mon before=*actual;
  for(int i=0;i<count;i++)if(pending[i].kind==GC_EV_EXP&&pending[i].a==party){
    before.exp=pending[i].exp_before;
    before.b.level=pending[i].c;
    break;
  }
  return before;
}
#endif
