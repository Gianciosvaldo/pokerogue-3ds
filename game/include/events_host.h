/*
 * events_host: the app's side of the timed events (custom-core's cc/tevents.h says which are open and what they change; game-core's
 * gc/events.h asks for the effects open now at the moment of each roll).
 *
 * The console's wall clock is read at every ask, never at the run's start, so a window that opens or closes during a run is seen by the
 * next roll. The clock is the same one the title music's date uses (bgm.c: time() through localtime_r), read as the date and time the
 * console shows. The console's clock can be set by its owner, as on any offline game.
 */
#ifndef GM_EVENTS_HOST_H
#define GM_EVENTS_HOST_H

#include <stdint.h>
#include <time.h>

#include "gc/events.h"
#include "gc/run.h"

/* Seconds since 1970-01-01 00:00 of a broken-down LOCAL time, counted as if it were UTC (the module knows no time zones or DST): the day
 * number times 86400 plus the time of day. -1 when the fields are not a date (a month outside 0..11, a day outside 1..31, a year outside
 * 1970..9999, an hour outside 0..23, a minute outside 0..59 or a second outside 0..60). Pure. */
int64_t eh_local_seconds(const struct tm *tm);

/* The console's local time now in the same unit, -1 when the clock cannot be read; or what eh_use_fixed_clock set. */
int64_t eh_now(void);

/* The clock is live (the console's) unless a host test pins it: local_s >= 0 is that instant for every ask, negative = no event is ever
 * open (the clock "cannot be read"). The run driver's host test pins it to nothing so its runs do not change with the date. */
void eh_use_fixed_clock(int64_t local_s);
void eh_use_live_clock(void);

/* The effects open at local_s (all zero for a negative one) on the shipped table. Pure. */
void eh_effects_at(int64_t local_s, gc_event_effects *out);

/* The source game-core asks: eh_effects_at(eh_now()). Static, lives for the whole program. */
const gc_event_source *eh_source(void);

/* Installs the source in a run. Call after gc_run_new and after every successful gc_run_load (game-core does not save the source). */
void eh_install(gc_run *run);

#endif
