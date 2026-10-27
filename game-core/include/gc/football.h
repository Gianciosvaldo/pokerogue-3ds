/* Football-only access to existing roster/EXP/evolution phases. */
#ifndef GC_FOOTBALL_H
#define GC_FOOTBALL_H
#include "gc/run.h"
#include "pr_data.h"
#define GC_FOOTBALL_MAX_EVOLUTIONS 16
#ifndef GC_FOOTBALL_EXP_FACTOR
#define GC_FOOTBALL_EXP_FACTOR 2000
#endif

typedef enum { GC_FB_BUSY, GC_FB_WAIT, GC_FB_DONE, GC_FB_ERROR } gc_football_progress;
/* Caller owns a dedicated gc_run, kept static or on the heap, not the stack.
 * Do not pass a classic run or use gc_run_choose on this isolated queue. */
/* Create one validated base-form member in an isolated scratch run; no team budget. */
bool gc_football_create_member(gc_run *scratch,const gc_starter *starter,const char **why);
void gc_football_new(gc_run *run, gc_system *profile, const char *seed, uint32_t entropy);
bool gc_football_validate_draft(const gc_system *profile, const gc_starter *team, int count, const char **why);
bool gc_football_draft(gc_run *run, const gc_starter *team, int count, const char **why);
bool gc_football_award_exp(gc_run *run, unsigned round, const char **why);
/* Drain gc_run_next_event between ticks. Work is bounded to 8 phases/tick. */
gc_football_progress gc_football_tick(gc_run *run);
/* Evolution choice targets correspond to the active GC_SCREEN_EVOLVE.
 * Nincada chooses one result (Ninjask or Shedinja); no extra roster member. */
int gc_football_evolution_choices(gc_run *run, uint16_t out[GC_FOOTBALL_MAX_EVOLUTIONS]);
bool gc_football_choose(gc_run *run, const gc_choice *choice, unsigned evolution_choice,
                        const char **why);
/* Evolution items (the classic game's EVOLUTION_ITEM and RARE_EVOLUTION_ITEM, rare above id 50) that would evolve a party member now,
 * each with the species it brings: at most one per item, in the species table's order. */
#define GC_FOOTBALL_MAX_ITEM_EVOLUTIONS 8
typedef struct { uint8_t item; uint16_t species; } gc_football_item_evolution;
int gc_football_item_evolutions(gc_run *run, unsigned party, gc_football_item_evolution out[GC_FOOTBALL_MAX_ITEM_EVOLUTIONS]);
/* Evolves a party member with an item through the evolution phase and the moves it brings, answering every prompt itself: forget_slot
 * names the slot a new move replaces (NULL or -1 leaves the move unlearned). False, with nothing changed, when the item does not
 * evolve it. Same isolated scratch run as the rest of this header. */
bool gc_football_item_evolve(gc_run *run, unsigned party, int item, int (*forget_slot)(const pr_pokemon *), const char **why);
/* Read classic form-change item catalog for a compatible rare shop offer. */
int gc_football_mega_item(const gc_run *run, unsigned party);
/* Returns a supported mega form only for a matching, applicable item. */
const pr_form *gc_football_mega_form(const gc_run *run, unsigned party, int item);
const pr_form *gc_football_owned_mega(const gc_run *run, unsigned party);
bool gc_football_give_mega(gc_run *run, unsigned party, int item);
/* Dedicated non-battle roster codec; never accepted by gc_run_load. */
size_t gc_football_roster_save(const gc_run *run, void *buf, size_t cap);
bool gc_football_roster_load(gc_run *run, gc_system *profile, const void *buf, size_t len);
#endif
