/*
 * party_screen: game/menus' party and summary screens (party.c, summary.c) as a ui_screen on the
 * bottom screen, over the battle, filled from battle-core Pokemon.
 */
#ifndef GM_PARTY_SCREEN_H
#define GM_PARTY_SCREEN_H

#include <stdbool.h>

#include "pr/battle.h"

/* (menus.h's party_mode and PartyOption values, so callers needn't include menus.h, whose text
 * style names clash with ui_text.h's) */
typedef enum { PS_SWITCH = 0, PS_FAINT_SWITCH = 1, PS_REVIVAL_BLESSING = 3, PS_RELEASE = 10 } ps_mode;
/* on_pick's option: PS_RELEASE_PICK is what PS_RELEASE gives for the member to release (menus' PO_RELEASE) */
enum { PS_CANCEL = -1, PS_SEND_OUT = 0, PS_REVIVE_PICK = 2, PS_RELEASE_PICK = 11 };

void ps_init(void); /* once at boot, after ui_init */

/* The run (a gc_run) the screens show: its trainer id is the summary's ID No., and its party members' met place, level and wave (where
 * they were caught) are the summary's trainer memo. NULL = none known. The run must outlive the screens. */
void ps_set_run(const void* gc_run_ptr);

/* Opens the party screen for the player's party. on_pick gets the slot and the PartyOption
 * (PS_SEND_OUT), or slot 6 / PS_CANCEL when the player backs out (never in FAINT_SWITCH). The
 * screen stays open until ps_close. */
void ps_open(const pr_battle* bt, ps_mode mode, void (*on_pick)(int slot, int option, void* arg), void* arg);
void ps_close(void);

/* Opens the party screen in item mode (e.g. PARTY_MODIFIER, PARTY_MOVE_MODIFIER) over a gc_run party.
 * tm_move is the TM move ID for PARTY_TM_MODIFIER (0 for other modes).
 * on_pick gets slot (0..5), move_slot (-1 or 0..3), or slot 6 / cancel when backed out. */
/* Reward keys select TM, relearning and Tera-shard filters. */
void ps_open_reward(const void* gc_run_ptr,const char* key,bool target_move,int item_arg,void (*on_pick)(int slot,int move_slot,void* arg),void* arg);
void ps_open_item(const void* gc_run_ptr, int pmode, bool target_move, int tm_move, void (*on_pick)(int slot, int move_slot, void* arg), void* arg);

/* Opens the party screen in MODIFIER_TRANSFER mode over a gc_run party (the reward screen's Manage Items). on_move gets the item's
 * index in the run's item list, how many copies and the party index that gets them (-1 = discard them), and must apply the transfer to the run before
 * it returns (the screen then shows the run again). on_done runs when the player backs out; the screen stays open until ps_close. */
void ps_open_transfer(const void* gc_run_ptr, void (*on_move)(int item_src, int count, int to, void* arg), void (*on_done)(void* arg),
                      void* arg);

/* Reward Check Team: summaries and native eligible form-item actions, including
 * fainted and benched owners. The host applies changes; form uses a global index. */
void ps_open_check(const void* gc_run_ptr, int cursor,
                   void (*on_form)(int party, int item, void* arg),
                   void (*on_pause)(int party, bool paused, void* arg),
                   void (*on_done)(void* arg), void* arg);

/* ---- the learn-move dialog (menus' learn_move_start) over the battle ----
 * The dialog's messages and yes/no box belong to the caller's screen (the battle's message box); the
 * summary page that picks the move to forget is menus' own, and this puts a screen over the battle
 * for as long as it is open. Callbacks are upstream's LearnMovePhase steps, with menus.h's meaning: */
typedef struct {
  /* show text (with "$" page breaks and @d / @s tags); with prompt wait for A; then done(done_ctx) */
  void (*show_text)(void* host, const char* text, bool prompt, void (*done)(void*), void* done_ctx);
  /* Yes / No box: yes(ctx) or no(ctx) */
  void (*confirm)(void* host, void (*yes)(void*), void (*no)(void*), void* ctx);
  /* the answer: the move slot to forget (0-3), or -1 = not learned. The dialog goes on with its last messages */
  void (*on_result)(void* host, int slot);
  /* after the last message */
  void (*on_end)(void* host);
  void* host;
} ps_learn_host;

/* party slot = battle order (bt->party[0][slot]); the Pokemon wants to learn `move` and knows four.
 * trainer_id is gameData.trainerId (the summary's ID No.) */
void ps_learn_move(const pr_battle* bt, int party_slot, int move, int trainer_id, const ps_learn_host* host);

#endif
