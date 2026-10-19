/*
 * win_gains: what a Classic win gave the player, for the screens that tell them (upstream's GameOverPhase.handleGameOver, handleUnlocks,
 * awardFirstClassicCompletion; UnlockPhase, RibbonModifierRewardPhase, GameOverModifierRewardPhase; AchvBar).
 *
 * game-core does the bookkeeping (gc_classic_finish updates the system data as the run's win is handled: the achievements, the vouchers, the
 * unlocks, the ribbons and the win counts) and tells the app nothing but "the run was cleared". This module takes a copy of the few fields
 * of the system data that the win changes when a run starts or loads (wg_snapshot), and when the run ends in a win it diffs them against
 * the system data as it is then (wg_collect): what is different is what the win gave. Nothing here changes the system data.
 *
 * The items come out in the order upstream shows them:
 *   - achievement bars: Undefeated (the first Classic win) and the Golden Egg Voucher it unlocks, Bring Your Child To Work Day (an unevolved
 *     party member), the ribbon milestones (10, 25, 50, 75, 100 ribbons);
 *   - messages: Endless Mode, Endless (Spliced) Mode, Mini Black Hole, Eviolite unlocked; one "beat Classic for the first time, you
 *     received an Egg Voucher Plus" per species line that got its first Classic ribbon (the party's, then its starter's, in party order);
 *     "You received an Egg Voucher Premium" for a win that is not the first.
 *
 * The words come from the locale files (battle:unlockedSomething, battle:beatModeFirstTime, battle:rewardGain, achv:*, voucher:*,
 * modifierType:*, gameMode:*); the bar's drawing is menus/toast_view.c, the pages are result_screen.c's.
 */
#ifndef GM_WIN_GAINS_H
#define GM_WIN_GAINS_H

#include <stdbool.h>
#include <stdint.h>

#include "gc/system.h"

#ifdef __cplusplus
extern "C" {
#endif

struct gc_run;

typedef enum {
  WG_ACHV,           /* an achievement bar: id = GC_ACHV_* */
  WG_VOUCHER_TOAST,  /* a voucher bar (the Golden Egg Voucher of the first win): id = GC_VOUCHER_* */
  WG_UNLOCK,         /* "<thing> has been unlocked.": id = WG_U_* */
  WG_FIRST_RIBBON,   /* "<species> beat Classic Mode for the first time! You received 1x Egg Voucher Plus!": id = species */
  WG_PREMIUM,        /* "You received 1x Egg Voucher Premium!" */
} wg_kind;

typedef enum { WG_U_ENDLESS, WG_U_SPLICED_ENDLESS, WG_U_MINI_BLACK_HOLE, WG_U_EVIOLITE } wg_unlock;

typedef struct {
  uint8_t kind;  /* wg_kind */
  uint16_t id;
} wg_item;

#define WG_MAX 40  /* 7 achievements, 1 voucher bar, 4 unlocks, 12 ribbon species (6 party members, each with a starter), 1 premium voucher */
typedef struct {
  int n;
  wg_item item[WG_MAX];
} wg_gains;

/* Takes the copy of the system data's win fields. NULL: there is none (a developer's run from rundriver.txt has no system data), and
 * wg_collect then finds nothing. Call it when a run starts or is loaded, before its win can happen. */
void wg_snapshot(const gc_system* sys);

/* Diffs the system data after the win against the snapshot. Zero items for a run that is not a Classic win, with no snapshot, or whose win
 * changed nothing. Returns the number of items. */
int wg_collect(const gc_system* after, const struct gc_run* run, wg_gains* out);

/* ---- what the screens show of an item ---- */

/* An achievement or voucher bar (WG_ACHV, WG_VOUCHER_TOAST). */
typedef struct {
  char title[96];
  char desc[160];
  char score[16];       /* "+250pt"; empty for a voucher */
  const char* icon;     /* frame of the "items" atlas */
  int tier;             /* 0..4: achv_bar, achv_bar_2 .. achv_bar_5 */
} wg_toast;
bool wg_is_toast(const wg_item* it);
void wg_toast_of(const wg_item* it, wg_toast* out);

/* The text of a message item (WG_UNLOCK, WG_FIRST_RIBBON, WG_PREMIUM), with upstream's line break. */
void wg_message_of(const wg_item* it, char* out, int n);

#ifdef __cplusplus
}
#endif
#endif
