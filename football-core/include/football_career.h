#ifndef FOOTBALL_CAREER_H
#define FOOTBALL_CAREER_H
#include "football_live.h"
typedef enum { FB_CAREER_DRAFT,FB_CAREER_LINEUP,FB_CAREER_READY,FB_CAREER_MATCH,FB_CAREER_RESULT,FB_CAREER_EXP,FB_CAREER_EVOLVE,FB_CAREER_LEARN /* unused: level-up moves are learned without a prompt; kept so saved phase numbers stay valid */,FB_CAREER_SHOP,FB_CAREER_END,FB_CAREER_ERROR } FootballCareerPhase;
/* An evolution item for sale, bound to the member who can use it now. Offer numbers for football_career_buy: 0..2 are Carbos, Protein
 * and the mega stone, 3 + n is evo[n]. */
#define FB_SHOP_EVO_MAX 8
#define FB_SHOP_FIXED 3
typedef enum { FB_EVO_OFFER,FB_EVO_SOLD,FB_EVO_VOID /* the member has evolved with another item */ } FootballEvoState;
typedef struct { uint8_t member,item,state;uint16_t species,price; } FootballEvoOffer; /* item: the classic EvolutionItem id, above 50 rare; species: what it brings */
typedef struct {
    FootballRoster roster;
    FootballLive live;
    gc_run *scratch;
    gc_system *profile;
    FootballCareerPhase phase;
    unsigned round,wins,wallet,seed,exp_member,evolution_count;
    /* Species candy earned in this match (not serialized, for the result screen), counted in candy points (FB_CANDY_*): plays as
     * they happen, then the match bonus for everyone. A point is a small step of the species' candy bar, not a candy (see
     * FB_CANDY_FRIENDSHIP_PER_POINT), and the *_milli fields are what the points are worth in thousandths of a candy, for display.
     * The points wait in candy_pending per member until football_career_flush_candy moves them to the profile with a checkpoint,
     * so they are never in the profile without the checkpoint that knows about them. seen_snaps is how many dead balls have been
     * looked at for credits. */
    unsigned candy_plays,candy_team,seen_snaps,candy_pending[22],candy_plays_milli,candy_team_milli;
    uint16_t evolutions[GC_FOOTBALL_MAX_EVOLUTIONS];
    unsigned prices[3];bool sold[3];int stone_target,stone_item,mega;
    unsigned evo_count;FootballEvoOffer evo[FB_SHOP_EVO_MAX];
    char message[160];
} FootballCareer;
bool football_career_new(FootballCareer *c,gc_run *scratch,gc_system *profile,unsigned seed);
bool football_career_start(FootballCareer *c);
void football_career_tick(FootballCareer *c,unsigned ms,FootballLiveInput input);
bool football_career_continue(FootballCareer *c);
bool football_career_evolve(FootballCareer *c,int choice);
/* The move slot a level-up move replaces when four are known: the lowest power (status moves count 0), the earliest slot on a tie; -1 with no moves. */
int football_career_forget_slot(const pr_pokemon *p);
/* member is the target of Carbos and Protein; an evolution offer is bound to its own member. An evolution runs the real evolution and
 * move-learning phases on the member and refreshes the roster; the result is told in c->message. */
bool football_career_buy(FootballCareer *c,unsigned offer,unsigned member);
/* Whether an evolution offer still holds against the roster (the member can use that item and it brings that species). */
bool football_career_evo_valid(FootballCareer *c,const FootballEvoOffer *offer);
/* The player an evolution offer would make of its member (its attributes with the boosts it carries), without buying: the shop shows
 * these beside today's. The roster and the scratch run are left as they were. */
bool football_career_evo_preview(FootballCareer *c,unsigned offer,FootballPlayer *out);
/* Safe checkpoints: lineup, round hub, playcall, shop and final result. */
bool football_career_save_ready(const FootballCareer *c);
/* Gives the profile the candy points that are waiting (to the species each member started as, as steps of its candy bar) and
 * returns how many whole candies that made; the screen calls this when it saves a checkpoint, just before it saves the profile. */
unsigned football_career_flush_candy(FootballCareer *c);
/* What candy points are worth for a member, in thousandths of a candy: the bar of the species it started as is as long as its
 * friendship cap in the classic game, so the same play is worth far less to a legendary than to a common Pokemon. */
unsigned football_candy_milli(const FootballCareer *c,unsigned member,unsigned points);
/* What the match bonus (played, and won) comes to for the whole team, in thousandths of a candy, before it is paid. */
unsigned football_career_team_candy_milli(const FootballCareer *c,bool won);
#define FB_CAREER_SAVE_MAX (512u*1024u)
size_t football_career_save(FootballCareer *c,void *bytes,size_t capacity);
bool football_career_load(FootballCareer *c,gc_run *scratch,gc_system *profile,const void *bytes,size_t length);
#endif
