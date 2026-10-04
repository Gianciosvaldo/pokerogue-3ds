/*
 * battle_events: battle-core's turn events (pr/events.h) as battle screen events (battle_view.h).
 *
 *   TEXT          -> a message, rendered by pr_msg_format from the locale files in data.pak
 *                    (messages battle-core has no key for yet are skipped)
 *   MOVE_ANIM     -> the move animation (a stand-in lunge until the animation interpreter)
 *   DAMAGE / HP   -> the hit and the HP bar, to the HP in the event's field snapshot
 *   STAT_STAGES   -> one stat stage event per stat changed
 *   ABILITY_SHOW  -> the ability bar
 *   FAINT         -> the faint
 *   SWITCH_OUT/IN -> withdraw / send out (the new Pokemon from the party)
 * Status icons follow each event's field snapshot. Anything the events didn't show (battle-core
 * doesn't emit every event yet) is caught up at the end from the battle itself: HP bars move to
 * the final HP, and a Pokemon at 0 HP faints.
 */
#ifndef GM_BATTLE_EVENTS_H
#define GM_BATTLE_EVENTS_H

#include "battle_view.h"
#include "pr/battle.h"
#include "pr/engine.h"
#include "pr/text.h"

/* The text source for pr_msg_format: locales/<language>/<file>.json from data.pak, falling back
 * to English as upstream's i18next does. */
const pr_text_source* be_text_source(void);

/* A battle-core Pokemon as the screen shows it. */
void be_mon(const pr_battle* bt, const pr_pokemon* p, bv_mon* out);

/* What the screen shows of both sides before some events: the Pokemon as bv_mon_of gives them, and
 * whether each is on the field. be_convert_events leaves the presence as the events made it. */
typedef struct {
  bv_mon mon[2];                              /* [0] the player's, [1] the enemy's */
  bool present[2];
  float (*exp_ratio)(const pr_pokemon* p);    /* the EXP bar of a player's Pokemon being sent out (NULL = empty) */
  int not_found;                              /* out: switch-ins whose Pokemon couldn't be found in the party */
  const pr_pokemon* sent[2];                  /* out: the last Pokemon sent out on each side (NULL: none, or not found) */
} be_view;

/* Converts battle-core events (a turn's, or the rest of one after a switch) into screen events, at most
 * max. `view` is the screen before the events. `bt` is where a switched-in Pokemon is looked up; with
 * `settled` it is also the battle as it stands once the events have happened, and what they left out
 * (HP, status, a faint) is caught up from it. Not settled (the run went on past the events: a victory,
 * experience, the next wave) the events alone are shown. Returns the number of screen events. Texts live
 * in static storage until the next call (bv_play copies them). */
int be_convert_events(const pr_battle* bt, const pr_event* events, int n_events, bool settled, be_view* view,
                      bv_event* out, int max);

/* The events of the turn that just ran (e->events), both Pokemon on the field and settled. */
int be_convert(const pr_battle* bt, const pr_engine* e, const bv_mon* shown_player, const bv_mon* shown_enemy,
               bv_event* out, int max);

#endif
