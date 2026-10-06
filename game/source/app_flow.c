/* The app's screen flow (see app_flow.h). */
#include "app_flow.h"

#include <stdio.h>
#include <string.h>

#include "gc/run.h"
#include "result_screen.h"
#include "run_saves.h"
#include "slot_screen.h"
#include "title_screen.h"
#include "ui.h"

static struct {
  rs_config first;          /* the debug config, for the first New Game */
  bool first_used;
  uint32_t runs_started;
  rsv_info info[RSV_SLOTS]; /* what each slot holds, as of the last look */
  int slot;                 /* the slot the run on screen is saved into */
  result_args result;
} A;

static void show_title(void);
static void show_slots(slots_mode mode, int cursor);
static void on_run_end(int kind, void* arg);

/* ------------------------------------------------------------------ the run */

/* The config of the run about to start in `slot`. `fresh`: a new game, which takes the debug config the first time. */
static rs_config run_config(int slot, bool fresh) {
  rs_config c;
  memset(&c, 0, sizeof c);
  if (fresh && !A.first_used) {
    c = A.first; /* seed and starters from rundriver.txt */
    A.first_used = true;
  }
  /* each run has its own randomness: the session's base, and when the player got here (frames since boot) */
  c.entropy = A.first.entropy ^ (A.runs_started * 0x9E3779B1u) ^ (ui_frame() * 2654435761u);
  A.runs_started++;
  c.save = true;
  c.slot = slot;
  c.on_end = on_run_end;
  A.slot = slot;
  return c;
}

/* A message on the slot list, with the slots as they are now (a load that did not work, a run that did not start) */
static void tell(const char* text) {
  rsv_inspect_all(A.info);
  slot_screen_refresh();
  slot_screen_message(text);
}

/* Loads the run in `slot` and shows it. When it can't, the slot list says why: after a Continue the list opens on the slot. */
static void load_slot(int slot, bool from_list) {
  rs_config c = run_config(slot, false);
  char why[RSV_WHY_LEN];
  const rsv_status st = rs_resume(&c, why);
  if (st == RSV_OK) return;
  if (!from_list) {
    rsv_inspect_all(A.info);
    show_slots(SLOTS_LOAD, slot);
  }
  tell(st == RSV_EMPTY ? "There is no run in that slot." : why);
}

static void new_run_in(int slot) {
  /* the list never offers this: what can't be read is not overwritten */
  if (A.info[slot].status == RSV_INCOMPATIBLE) {
    tell("That slot holds a save this version can't read. It is not overwritten.");
    return;
  }
  /* the run being replaced goes, with its backup: the new run's first save has no old one behind it */
  if (!rsv_clear(slot)) {
    tell("The old run in that slot could not be removed from the SD card.");
    return;
  }
  rs_config c = run_config(slot, true);
  if (!rs_start(&c)) tell("The run could not be started.");
}

static void on_result_done(void* arg) {
  (void)arg;
  show_title();
}

/* The run is over: its slot is cleared (upstream's tryClearSession) and the result screen takes over. */
static void on_run_end(int kind, void* arg) {
  (void)arg;
  memset(&A.result, 0, sizeof A.result);
  A.result.victory = kind == GC_SCREEN_CLEAR;
  rsv_summarize(rs_run(), &A.result.run);
  A.result.run.status = RSV_OK;
  A.result.on_done = on_result_done;
  rsv_clear(A.slot);
  result_screen_open(&A.result);
}

/* ------------------------------------------------------------------ the slot list */

static slots_mode g_mode;

static void on_pick(int slot, void* arg) {
  (void)arg;
  if (g_mode == SLOTS_LOAD)
    load_slot(slot, true);
  else
    new_run_in(slot);
}

static void on_delete(int slot, void* arg) {
  (void)arg;
  if (!rsv_clear(slot)) {
    tell("The run could not be deleted from the SD card.");
    return;
  }
  rsv_inspect_all(A.info);
  slot_screen_refresh();
}

static void on_back(void* arg) {
  (void)arg;
  show_title();
}

static void show_slots(slots_mode mode, int cursor) {
  g_mode = mode;
  rsv_inspect_all(A.info);
  slot_screen_open(&(slots_args){mode, A.info, cursor, on_pick, on_delete, on_back, NULL});
}

/* ------------------------------------------------------------------ the title */

static void on_title(title_choice c, void* arg) {
  (void)arg;
  switch (c) {
    case TITLE_CONTINUE: {
      const int last = rsv_last_slot();
      if (last >= 0) load_slot(last, false);
      break;
    }
    case TITLE_NEW_GAME:
      show_slots(SLOTS_NEW, 0);
      break;
    case TITLE_LOAD_GAME:
      show_slots(SLOTS_LOAD, 0);
      break;
  }
}

static void show_title(void) {
  rsv_inspect_all(A.info);
  int last = rsv_last_slot();
  /* Continue needs a file in the slot it would load; a slot that is gone (cleared, deleted by hand) is forgotten. An
   * unreadable file still counts: Continue then says why it can't load it. */
  if (last >= 0 && A.info[last].status == RSV_EMPTY) last = -1;
  title_args t;
  memset(&t, 0, sizeof t);
  t.can_continue = last >= 0;
  t.biome = last >= 0 && A.info[last].status == RSV_OK ? A.info[last].biome : -1;
  t.on_choose = on_title;
  title_screen_open(&t);
}

void app_start(const rs_config* first_run) {
  memset(&A, 0, sizeof A);
  A.first = *first_run;
  show_title();
}
