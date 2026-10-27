#ifndef BATTLE_EVENT_STORE_H
#define BATTLE_EVENT_STORE_H
#include "gc/run.h"
/* The single active run driver owns one reusable arena. Bind after new/load; this is runtime
 * presentation storage, never a part of the save. */
void bes_bind(gc_run *run);
/* After every bound run has been discarded (application shutdown). */
void bes_shutdown(void);
#endif
