#ifndef GM_EGG_HATCH_SCREEN_H
#define GM_EGG_HATCH_SCREEN_H
#include "gc/run.h"
/* Read-only presentation of an already committed hatch; never generates rewards. */
void egg_hatch_open(const gc_event *event,void (*done)(void *),void *arg);
#endif
