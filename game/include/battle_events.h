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

/* Converts the events of the turn that just ran (e->events) into out (at most max). `shown` is
 * the screen's view before the turn: battler 0 and 2 (bv_mon_of), used to catch up what the
 * events left out. Returns the number of screen events. Texts live in static storage until the
 * next call (bv_play copies them). */
int be_convert(const pr_battle* bt, const pr_engine* e, const bv_mon* shown_player, const bv_mon* shown_enemy,
               bv_event* out, int max);

#endif
