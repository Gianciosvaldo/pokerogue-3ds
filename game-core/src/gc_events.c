/* Timed events in a run: the app's source of the effects open now (gc/events.h). */
#include "internal.h"

void gc_run_set_events(gc_run *run, const gc_event_source *src)
{
    gc_int(run)->events = src;
}

/* Asked at the moment of each roll an effect changes, never cached. A field outside what upstream's own events can carry is clamped, so a
 * wrong source can only be generous within the game's own limits: luck stays under the cap of 14 there, a multiplier is a whole number. */
gc_event_effects gc_event_effects_now(const gc_run *run)
{
    gc_event_effects fx;
    memset(&fx, 0, sizeof fx);
    const gc_event_source *src = gc_int((gc_run *)run)->events;
    if (src && src->effects) src->effects(src->user, &fx);
    if (fx.luck_boost > 14) fx.luck_boost = 14;
    return fx;
}
