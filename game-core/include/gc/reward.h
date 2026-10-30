/* Reward party queries shared with the app; ordering matches reward application. */
#ifndef GC_REWARD_PARTY_H
#define GC_REWARD_PARTY_H
#include "gc/run.h"
#include "pr/reward.h"
#define GC_MEMORY_MOVE_CAPACITY (PR_REWARD_MAX_LEVEL_MOVES + PR_REWARD_MAX_USED_TMS + 4)
/* Storage party index, not display order. Returns -1 rather than truncating. */
int gc_memory_moves(gc_run *run,int party,pr_reward_memory_move *out,int capacity);
bool gc_mon_can_use_tera_shard(const gc_mon *mon);
#endif
