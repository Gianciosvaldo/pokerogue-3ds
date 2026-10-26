/* Grow only when a turn outgrows the controller's inline presentation store. */
#include "battle_event_store.h"
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
static pr_event *arena;
static int arena_capacity;
static bool reserve_events(void *ctx,const pr_event *old_data,int used,int required,
                           pr_event **storage,int *capacity)
{
    (void)ctx;
    if(used<0||required<used||required<1)return false;
    int next=arena_capacity?arena_capacity:GC_MAX_BATTLE_EVENTS;
    while(next<required){if(next>INT_MAX/2){next=required;break;}next*=2;}
    if((size_t)next>SIZE_MAX/sizeof(pr_event))return false;
    bool was_arena=old_data==arena;
    if(next!=arena_capacity){
        pr_event *grown=realloc(arena,(size_t)next*sizeof *grown);
        if(!grown)return false;
        arena=grown;arena_capacity=next;
    }
    /* realloc already preserves an existing arena's prefix. A new/resumed run
     * initially holds its prefix in its own inline buffer. */
    if(!was_arena&&used)memcpy(arena,old_data,(size_t)used*sizeof *arena);
    *storage=arena;*capacity=arena_capacity;
    return true;
}
void bes_bind(gc_run *run){gc_run_set_battle_event_storage(run,reserve_events,NULL);}
void bes_shutdown(void){free(arena);arena=NULL;arena_capacity=0;}
