/* The app's screen flow (see app_flow.h). */
#include "app_flow.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "capsule_screen.h"
#include "gacha_screen.h"
#include "profile_tx.h"
#include "combat_observer.h"
#include "custom_run.h"
#include "custom_store.h"
#include "gc/run.h"
#include "history_screen.h"
#include "i18n.h"
#include "name_entry.h"
#include "radar_screen.h"
#include "result_screen.h"
#include "run_history.h"
#include "run_saves.h"
#include "run_settings.h"
#include "slot_screen.h"
#include "starter_host.h"
#include "football_screen.h"
#include "system_data.h"
#include "title_screen.h"
#include "ui.h"
#include "ui_widgets.h"
#include "win_gains.h"

static struct {
  rs_config first;          /* the debug config, for the first New Game */
  bool first_used;
  uint32_t runs_started;
  rsv_info info[RSV_SLOTS]; /* what each slot holds, as of the last look */
  int slot;                 /* the slot the run on screen is saved into */
  result_args result;
  const co_report* kept;    /* the report whose custom store write failed: tried once more when the result screen is left */
  bool custom;              /* the run on screen (or about to start) is a custom one (csession_<n>, custom_run.h) */
  bool kind[RSV_SLOTS];     /* what each slot of the list holds: true = a custom run (csession_<n>), false = an ordinary one */
  rset_values settings;     /* the run settings screen's last values */
  char note[CS_WHY_LEN + 64];
} A;

static void show_title(void);
static void show_radar(void);
static void show_gacha(void);
static void show_history(void);
static void show_capsule_preview(int n);
static int g_capsule_preview; /* the debug preview's result count, 0 = none (survives app_start's reset) */
static void show_slots(slots_mode mode, int cursor);
static void on_run_end(int kind, void* arg);
static void on_run_quit(void* arg);

/* ------------------------------------------------------------------ the run */

/* The config of the run about to start in `slot`. `fresh`: a new game, which takes the debug config the first time. */
static rs_config run_config(int slot, bool fresh) {
  rs_config c;
  memset(&c, 0, sizeof c);
  if (fresh && !A.first_used && !A.custom) {
    c = A.first; /* seed and starters from rundriver.txt (an ordinary run's) */
    A.first_used = true;
  }
  c.custom = A.custom;
  /* each run has its own randomness: the session's base, and when the player got here (frames since boot) */
  c.entropy = A.first.entropy ^ (A.runs_started * 0x9E3779B1u) ^ (ui_frame() * 2654435761u);
  A.runs_started++;
  c.save = true;
  c.slot = slot;
  c.on_end = on_run_end;
  c.on_quit = on_run_quit;
  /* the run reads and updates the system data (the dex, the starter unlocks), written with each save of the run. The species in
   * rundriver.txt are a developer's team and may not be unlocked: that run has none. */
  if (c.n_starters == 0) {
    c.sys = sysd_get();
    c.save_sys = sysd_save;
  }
  /* what a Classic win will give is the difference between the system data as it is now and as the win leaves it (win_gains.h) */
  wg_snapshot(c.sys);
  A.slot = slot;
  return c;
}

/* the file name of a slot: the ordinary run's or the custom run's */
static const char* slot_file(int slot, bool custom, char out[24]) {
  if (custom)
    snprintf(out, 24, CR_FILE_PREFIX "%d", slot);
  else
    rsv_slot_name(slot, out);
  return out;
}
static const char* slot_file_of(int slot, char out[24]) { return slot_file(slot, A.kind[slot], out); }

/* The five slots hold ordinary and custom runs alike (lorenzo, 2026-10-19: one Load Game, each run labelled Classic or Custom). A slot
 * shows its ordinary run, else its custom one; both only exist from before the lists were one, and then the ordinary run shows and the
 * custom one comes back once that is deleted. */
static void look(void) {
  static rsv_info custom[RSV_SLOTS];
  rsv_inspect_all(A.info);
  cr_inspect_all(custom);
  for (int i = 0; i < RSV_SLOTS; i++) {
    A.kind[i] = A.info[i].status == RSV_EMPTY && custom[i].status != RSV_EMPTY;
    if (A.kind[i]) A.info[i] = custom[i];
    A.info[i].custom = A.kind[i];
    char file[24];
    if (A.info[i].status == RSV_OK) rsv_name_get(slot_file_of(i, file), A.info[i].run_key, A.info[i].name);
  }
}

static void tell(const char* text) {
  look();
  slot_screen_refresh();
  slot_screen_message(text);
}

/* Loads the run in `slot` and shows it. When it can't, the slot list says why: after a Continue the list opens on the slot. */
static void load_slot(int slot, bool from_list) {
  look();
  A.custom = A.kind[slot];
  rs_config c = run_config(slot, false);
  char why[RSV_WHY_LEN];
  const rsv_status st = rs_resume(&c, why);
  if (st == RSV_OK) {
    if (A.custom) rsv_set_last_slot(slot); /* Continue's slot (an ordinary load sets it itself) */
    return;
  }
  if (!from_list) {
    look();
    show_slots(SLOTS_LOAD, slot);
  }
  tell(st == RSV_EMPTY ? "There is no run in that slot." : why);
}

/* The run starts with `team` (NULL: a developer's team from rundriver.txt, no starter select). The slot's old run goes only now,
 * once the player has confirmed a team: backing out of the starter select leaves it as it was. */
static void start_new_run(int slot, const gc_starter* team, int n_team, bool from_starter_select) {
  /* the slot is one place for either kind of run: whatever it held goes */
  if (!(cr_clear(slot) && rsv_clear(slot))) {
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
    tell(A.custom && cr_error()[0] ? cr_error() : "The run could not be started.");
    return;
  }
  if (A.custom) rsv_set_last_slot(slot);
}

static void on_starters_chosen(const gc_starter* team, int n, void* arg) { start_new_run((int)(long)arg, team, n, true); }

/* upstream's Exit on the starter select: back to the title */
static void on_starters_exit(void* arg) {
  (void)arg;
  show_title();
}

/* The custom file cannot be used: its reason is long ("... It is left as it is, and nothing is saved to it ..."), the message box holds two
 * lines, so the player gets the first sentence. */
static void tell_custom_unusable(const char* why) {
  char t[CR_WHY_LEN];
  snprintf(t, sizeof t, "%s", why && why[0] ? why : "The custom content file cannot be used.");
  for (char* p = t; *p; p++)
    if (*p == '.' && (p[1] == ' ' || p[1] == 0)) {
      p[1] = 0;
      break;
    }
  tell(t);
}

static void new_run_in(int slot) {
  if (A.custom) {
    char why[CR_WHY_LEN];
    if (!cr_usable(why)) {
      tell_custom_unusable(why);
      return;
    }
  }
  /* the list never offers this: what can't be read is not overwritten */
  if (A.info[slot].status == RSV_INCOMPATIBLE) {
    tell("That slot holds a save this version can't read. It is not overwritten.");
    return;
  }
  /* a developer's team (rundriver.txt) starts the run without the starter select, as before it existed */
  if (!A.custom && !A.first_used && A.first.n_starters > 0) {
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
    cs_commit_defeat(A.kept, rs_custom_defeat());
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
  /* the win's unlocks, vouchers and ribbons: game-core has already changed the system data as the run was cleared */
  A.result.ending = A.result.victory && rs_run()->run.mode == GAME_MODES_CLASSIC;
  static wg_gains gains;
  A.result.gains = A.result.victory && wg_collect(rs_run()->sys, rs_run(), &gains) > 0 ? &gains : NULL;
  /* The custom store takes the report BEFORE the slot is cleared. If the app stops between the two, Continue reloads the save from the
   * start of the lost encounter and the player plays the wave again, with a stored report of the defeat they undid (and no Nemesis: the
   * app never inserts one). The other order would lose the report of a defeat that stood. A report is also only ever a post-mortem: the
   * worse outcome of the first order is a stale report, the worse outcome of the second is a missing one. */
  A.kept = NULL;
  A.note[0] = 0;
  if (A.result.report) {
    /* a custom run's defeat comes with the enemy the faint hook named (NULL for an ordinary run: cs_commit_defeat is cs_commit then) */
    const cs_result stored = cs_commit_defeat(A.result.report, rs_custom_defeat());
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
    else if (stored.outcome == CS_STORED && stored.commit.record_id)
      snprintf(A.note, sizeof A.note, "The enemy that beat you was remembered. It may challenge you again.");
    A.result.note = A.note[0] ? A.note : NULL;
  }
  /* the run's line in the Run History (upstream's GameOverPhase: getRunHistoryEntry, saveRunHistory), with the name the player gave it. A
   * history that can't be written (or read, which it is then never overwritten for) does not stop the result screen: it says so. */
  {
    static rh_entry entry; /* not on the stack: an entry is nearly a kilobyte */
    char file[24], name[RSV_NAME_LEN];
    name[0] = 0;
    rsv_name_get(slot_file(A.slot, A.custom, file), A.result.run.run_key, name);
    rhist_record(rs_run(), A.result.victory, (int64_t)time(NULL), name, &entry);
    if (!rhist_add(&entry, NULL)) {
      const char* msg = "The run could not be added to the Run History.";
      const size_t used = strlen(A.note);
      snprintf(A.note + used, sizeof A.note - used, "%s%s", used ? " " : "", msg);
      A.result.note = A.note;
    }
  }
  if (A.custom)
    cr_clear(A.slot);
  else
    rsv_clear(A.slot);
  /* what the run won or caught (a Classic clear is recorded by game-core as the run ends) goes to the SD card now. A refusal is not
   * lost: the next save point of the next run writes it again. */
  if (rs_run()->sys) sysd_save();
  result_screen_open(&A.result);
}

/* Save and Quit from the in-run menu (run_menu.h): the run's slot holds the save of the battle's start, which is where Continue picks
 * it up again, and upstream's reset(true) brings the title. The cached system data upstream saves is what the battle's start wrote too,
 * so what the battle changed in memory and never wrote (a catch, the dex) is dropped: the next look at the system data reads the card. */
static void on_run_quit(void* arg) {
  (void)arg;
  sysd_reset();
  show_title();
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
  if (!(A.kind[slot] ? cr_clear(slot) : rsv_clear(slot))) {
    tell("The run could not be deleted from the SD card.");
    return;
  }
  look();
  slot_screen_refresh();
}

/* Rename Run (upstream's RENAME_RUN): the name is typed on name_entry.h's screen and kept beside the slot's file (run_saves.h). An empty
 * name does nothing and counts as done, as upstream's renameSession does. */
static void on_renamed(const char* text, void* arg) {
  const int slot = (int)(long)arg;
  bool failed = false;
  if (text && text[0]) {
    char file[24];
    failed = !rsv_name_set(slot_file_of(slot, file), A.info[slot].run_key, text);
  }
  show_slots(SLOTS_LOAD, slot);
  if (failed) tell("The name could not be written to the SD card.");
}

static void on_rename(int slot, void* arg) {
  (void)arg;
  const name_entry_args a = {"menu:renameRun", "menu:runName", "menu:rename", "menu:cancel", RSV_NAME_MAX, on_renamed, (void*)(long)slot};
  name_entry_open(&a);
}

static void on_back(void* arg) {
  (void)arg;
  show_title();
}

static void show_slots(slots_mode mode, int cursor) {
  g_mode = mode;
  look();
  slot_screen_open(&(slots_args){mode, A.info, cursor, on_pick, on_delete, on_rename, on_back, NULL});
}

/* ------------------------------------------------------------------ the title */

/* The run settings' Start: Custom picks the kind of run the slot list starts; a custom file that cannot be used is said so, not guessed
 * around (the list opens all the same, and no custom run starts from it) */
static void on_settings_start(const rset_values* v, void* arg) {
  (void)arg;
  A.settings = *v;
  A.custom = v->custom;
  show_slots(SLOTS_NEW, 0);
  char why[CR_WHY_LEN];
  if (A.custom && !cr_usable(why)) tell_custom_unusable(why);
}

static void on_football_exit(void *arg) { (void)arg;show_title(); }
static bool football_profile_save(void *arg) { (void)arg;return sysd_save(); }
static void show_football(void) {
  football_screen_args cfg={.profile=sysd_get(),
    .entropy=A.first.entropy ^ (++A.runs_started*0x9e3779b1u) ^ ui_frame(),
    .on_exit=on_football_exit,.save_profile=football_profile_save,
    .profile_notice=sysd_state(NULL)==SYSD_UNREADABLE ?
      "The profile cannot be read. Default starters are used and profile changes will not be saved." : NULL};
  if (!football_screen_open(&cfg)) {
    static const char *const failed[]={"Football could not start. Check memory and starter unlocks."};
    ui_push_options(failed,1,true,NULL,NULL);
  }
}

static void on_title(title_choice c, void* arg) {
  (void)arg;
  switch (c) {
    case TITLE_CONTINUE: {
      const int last = rsv_last_slot();
      if (last >= 0) load_slot(last, false);
      break;
    }
    case TITLE_NEW_GAME: {
      A.settings = rset_load(); /* as the screen last left them (B keeps a changed speed too) */
      const rset_args a = {A.settings, on_settings_start, NULL, NULL};
      rset_open(&a);
      break;
    }
    case TITLE_LOAD_GAME:
      A.custom = false;
      show_slots(SLOTS_LOAD, 0);
      break;
    case TITLE_HISTORY:
      show_history();
      break;
    case TITLE_RADAR:
      show_radar();
      break;
    case TITLE_CAPSULE:
      show_gacha();
      break;
    case TITLE_FOOTBALL:
      show_football();
      break;
  }
}

/* ------------------------------------------------------------------ the Run History */

static void on_history_exit(void* arg) {
  (void)arg;
  show_title();
}

static void show_history(void) {
  const hist_args h = {rhist_load(), on_history_exit, NULL};
  history_screen_open(&h);
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

/* ------------------------------------------------------------------ the capsule machine (task C09) */

static void on_gacha_exit(void* arg) {
  (void)arg;
  show_title();
}

/* The voucher-to-egg machines. Every purchase goes through profile_tx (one committed transaction, one immutable result); this screen
 * first finishes whatever a restart left unfinished or unshown (gacha_screen.h). */
static void show_gacha(void) {
  const gachas_args g = {on_gacha_exit, NULL, i18n_species_name};
  gacha_screen_open(&g);
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
  A.custom = false;
  look();
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
  A.settings = rset_load();
  ui_set_game_speed(rset_time_scale(A.settings.speed));
  cs_set_nonce(A.first.entropy); /* only used by a new custom file; an existing one keeps its own */
  co_set_uid_source(cs_begin_run);
  ptx_boot(); /* finishes a purchase or reward a crash left half applied, and stops the profile being saved over it (profile_tx.h) */
  cr_settle_owed(); /* the voucher of a rematch won just before a crash that never got to claim it (custom_run.h) */
  if (g_capsule_preview > 0)
    show_capsule_preview(g_capsule_preview);
  else
    show_title();
}
