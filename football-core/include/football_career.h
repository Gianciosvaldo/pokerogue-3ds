#ifndef FOOTBALL_CAREER_H
#define FOOTBALL_CAREER_H
#include "football_live.h"
typedef enum { FB_CAREER_DRAFT,FB_CAREER_LINEUP,FB_CAREER_READY,FB_CAREER_MATCH,FB_CAREER_RESULT,FB_CAREER_EXP,FB_CAREER_EVOLVE,FB_CAREER_LEARN,FB_CAREER_SHOP,FB_CAREER_END,FB_CAREER_ERROR } FootballCareerPhase;
typedef struct {
    FootballRoster roster;
    FootballLive live;
    gc_run *scratch;
    gc_system *profile;
    FootballCareerPhase phase;
    unsigned round,wins,wallet,seed,exp_member,evolution_count;
    uint16_t evolutions[GC_FOOTBALL_MAX_EVOLUTIONS];
    unsigned prices[3];bool sold[3];int stone_target,stone_item,mega;
    char message[160];
} FootballCareer;
bool football_career_new(FootballCareer *c,gc_run *scratch,gc_system *profile,unsigned seed);
bool football_career_start(FootballCareer *c);
void football_career_tick(FootballCareer *c,unsigned ms,FootballLiveInput input);
bool football_career_continue(FootballCareer *c);
bool football_career_evolve(FootballCareer *c,int choice);
bool football_career_learn(FootballCareer *c,int slot);
bool football_career_buy(FootballCareer *c,unsigned offer,unsigned member);
/* Safe checkpoints: lineup, round hub, playcall, shop and final result. */
bool football_career_save_ready(const FootballCareer *c);
#define FB_CAREER_SAVE_MAX (512u*1024u)
size_t football_career_save(FootballCareer *c,void *bytes,size_t capacity);
bool football_career_load(FootballCareer *c,gc_run *scratch,gc_system *profile,const void *bytes,size_t length);
#endif
