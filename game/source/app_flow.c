/* The app's screen flow (see app_flow.h). */
#include "app_flow.h"

#include <stdio.h>
#include <string.h>

#include "capsule_screen.h"
#include "combat_observer.h"
#include "custom_store.h"
#include "gc/run.h"
#include "radar_screen.h"
#include "result_screen.h"
#include "run_saves.h"
#include "slot_screen.h"
#include "starter_host.h"
#include "system_data.h"
#include "title_screen.h"
#include "ui.h"

static struct {
  rs_config first;          /* the debug config, for the first New Game */
  bool first_used;
  uint32_t runs_started;
  rsv_info info[RSV_SLOTS]; /* what each slot holds, as of the last look */
  int slot;                 /* the slot the run on screen is saved into */
  result_args result;
  const co_report* kept;    /* the report whose custom store write failed: tried once more when the result screen is left */
  char note[CS_WHY_LEN + 64];
} A;

static void show_title(void);
static void show_radar(void);
static void show_capsule_preview(int n);
static int g_capsule_preview; /* the debug preview's result count, 0 = none (survives app_start's reset) */
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
  /* the run reads and updates the system data (the dex, the starter unlocks), written with each save of the run. The species in
   * rundriver.txt are a developer's team and may not be unlocked: that run has none. */
  if (c.n_starters == 0) {
    c.sys = sysd_get();
    c.save_sys = sysd_save;
  }
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

/* The run starts with `team` (NULL: a developer's team from rundriver.txt, no starter select). The slot's old run goes only now,
 * once the player has confirmed a team: backing out of the starter select leaves it as it was. */
static void start_new_run(int slot, const gc_starter* team, int n_team, bool from_starter_select) {
  if (!rsv_clear(slot)) {
    if (from_starter_select) show_slots(SLOTS_NEW, slot);
    tell("The old run in that slot could not be removed from the SD card.");
    return;
  }
  rs_config c = run_config(slot, true);
  if (team) {
    memcpy(c.team, team, (size_t)n_team * sizeof team[0]);
    c.n_team = n_team;
  }
  if (!rs_start(&c)) {
    if (from_starter_select) show_slots(SLOTS_NEW, slot);
    tell("The run could not be started.");
  }
}

static void on_starters_chosen(const gc_starter* team, int n, void* arg) { start_new_run((int)(long)arg, team, n, true); }

/* upstream's Exit on the starter select: back to the title */
static void on_starters_exit(void* arg) {
  (void)arg;
  show_title();
}

static void new_run_in(int slot) {
  /* the list never offers this: what can't be read is not overwritten */
  if (A.info[slot].status == RSV_INCOMPATIBLE) {
    tell("That slot holds a save this version can't read. It is not overwritten.");
    return;
  }
  /* a developer's team (rundriver.txt) starts the run without the starter select, as before it existed */
  if (!A.first_used && A.first.n_starters > 0) {
    start_new_run(slot, NULL, 0, false);
    return;
  }
  /* an unreadable system file is not repaired or overwritten: the player starts with the defaults, and is told */
  char why[SYSD_WHY_LEN], notice[SYSD_WHY_LEN + 96];
  const char* text = NULL;
  if (sysd_state(why) == SYSD_UNREADABLE) {
    snprintf(notice, sizeof notice, "%s The file is left as it is, and nothing is saved to it until it is removed.", why);
    text = notice;
  }
  const sh_events ev = {on_starters_chosen, on_starters_exit, (void*)(long)slot};
  if (!sh_open(sysd_get(), text, &ev)) tell("There is no Pokemon to start with.");
}

static void on_result_done(void* arg) {
  (void)arg;
  /* the SD card refused the report at the run's end: one more try before it is gone (the store is idempotent by run, so a report
   * that did get written cannot be stored twice) */
  if (A.kept) {
    cs_commit(A.kept);
    A.kept = NULL;
  }
  show_title();
}

/* The run is over: its slot is cleared (upstream's tryClearSession) and the result screen takes over. */
static void on_run_end(int kind, void* arg) {
  (void)arg;
  memset(&A.result, 0, sizeof A.result);
  A.result.victory = kind == GC_SCREEN_CLEAR;
  rsv_summarize(rs_run(), &A.result.run);
  A.result.run.status = RSV_OK;
  A.result.report = co_report_get(); /* taken by the run driver as the run ended, before the slot is cleared below */
  A.result.on_done = on_result_done;
  /* The custom store takes the report BEFORE the slot is cleared. If the app stops between the two, Continue reloads the save from the
   * start of the lost encounter and the player plays the wave again, with a stored report of the defeat they undid (and no Nemesis: the
   * app never inserts one). The other order would lose the report of a defeat that stood. A report is also only ever a post-mortem: the
   * worse outcome of the first order is a stale report, the worse outcome of the second is a missing one. */
  A.kept = NULL;
  A.note[0] = 0;
  if (A.result.report) {
    const cs_result stored = cs_commit(A.result.report);
    if (stored.outcome == CS_WRITE_FAILED) A.kept = A.result.report;
    const char* note = cs_note(stored.outcome);
    if (stored.outcome == CS_FILE_UNUSABLE || stored.outcome == CS_NO_DURABLE_RUN) {
      char why[CS_WHY_LEN];
      if (cs_get_state(why) == CS_UNREADABLE) { /* the run had no durable uid because the file cannot be used: say why */
        snprintf(A.note, sizeof A.note, "%s", why);
        note = NULL;
      }
    }
    if (note) snprintf(A.note, sizeof A.note, "%s", note);
    A.result.note = A.note[0] ? A.note : NULL;
  }
  rsv_clear(A.slot);
  /* what the run won or caught (a Classic clear is recorded by game-core as the run ends) goes to the SD card now. A refusal is not
   * lost: the next save point of the next run writes it again. */
  if (rs_run()->sys) sysd_save();
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
    case TITLE_RADAR:
      show_radar();
      break;
  }
}

/* ------------------------------------------------------------------ the Nemesis Radar */

static void on_radar_exit(void* arg) {
  (void)arg;
  show_title();
}

/* A read-only look at the custom file's roster. The app has no stored schedule yet (the live adapter keeps it), so none is passed. The
 * screen copies what it is given; nothing here writes, and an unusable file is shown with its reason and left as it is. */
static void show_radar(void) {
  char why[CS_WHY_LEN];
  why[0] = 0;
  const bool usable = cs_get_state(why) != CS_UNREADABLE;
  radar_args r;
  memset(&r, 0, sizeof r);
  r.meta = usable ? cs_meta() : NULL;
  r.why = why;
  r.on_exit = on_radar_exit;
  if (!radar_open(&r)) {
    /* a roster that does not validate cannot come out of cs_meta (the decoder checks it); say so rather than showing nothing */
    snprintf(why, sizeof why, "The remembered enemies could not be shown.");
    r.meta = NULL;
    radar_open(&r);
  }
}

/* ------------------------------------------------------------------ the capsule preview (debug) */

static cap_fixture g_capsule_fixture; /* the screen copies it when it opens */

static void on_capsule_exit(caps_exit how, void* arg) {
  (void)how;
  (void)arg;
  show_title();
}

static void show_capsule_preview(int n) {
  caps_args c;
  memset(&c, 0, sizeof c);
  if (!capsule_debug_fixture(n, &g_capsule_fixture)) {
    show_title();
    return;
  }
  c.fixture = &g_capsule_fixture;
  c.on_exit = on_capsule_exit;
  if (!capsule_screen_open(&c)) show_title();
}

void app_debug_capsule(int n) { g_capsule_preview = n >= 1 && n <= CAP_MAX ? n : 0; }

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
  cs_set_nonce(A.first.entropy); /* only used by a new custom file; an existing one keeps its own */
  co_set_uid_source(cs_begin_run);
  if (g_capsule_preview > 0)
    show_capsule_preview(g_capsule_preview);
  else
    show_title();
}
