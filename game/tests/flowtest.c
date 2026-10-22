/* flowtest: the app's screen flow (app_flow.c) with fake screens and a fake run driver, over the real save slots (run_saves.c,
 * save.c) and real game-core runs, on the host.
 *
 *   build/flowtest
 *
 * The fakes record what the flow asks the screens for and answer as the player would (choose an entry, pick a slot, finish a
 * run). The fake run driver starts a real game-core run, saves it at once as the real driver does at the first save point,
 * and loads slots through the real rsv_load. Checks the flow: Continue, New Game with a slot (and the overwrite), Load Game,
 * game over and clear (the slot is cleared, back to the title), a slot that can't be loaded (it says why and is never
 * overwritten), Delete Run, and that the debug config (seed, starters) is used for the first New Game only. New Game goes
 * through the starter select (fake here): the old run in the slot stays until a team is confirmed, Exit goes back to the title, the
 * team confirmed is the run's, and the run reads and saves the system data (system_data.c, real) unless it is a developer's. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "app_flow.h"
#include "name_entry.h"
#include "capsule_screen.h"
#include "gacha_screen.h"
#include "profile_tx.h"
#include "system_data.h"
#include "combat_observer.h"
#include "custom_run.h"
#include "custom_store.h"
#include "pr_data.h"
#include "pr_enums.h"
#include "radar_screen.h"
#include "history_screen.h"
#include "result_screen.h"
#include "run_history.h"
#include "run_saves.h"
#include "run_settings.h"
#include "save.h"
#include "slot_screen.h"
#include "starter_data.h"
#include "starter_host.h"
#include "system_data.h"
#include "title_screen.h"

static int checks, failed;
#define CHECK(cond, ...)                                  \
  do {                                                    \
    checks++;                                             \
    if (!(cond)) {                                        \
      failed++;                                           \
      printf("FAIL line %d: %s: ", __LINE__, #cond);      \
      printf(__VA_ARGS__);                                \
      printf("\n");                                       \
    }                                                     \
  } while (0)

/* ------------------------------------------------------------------ fakes */

static title_args T;
static int n_title;
void title_screen_open(const title_args* a) {
  T = *a;
  n_title++;
}

/* the run settings before New Game's slot list: started at once with what the test picks (the real screen: run_settings_screen.c) */
static rset_args RS;
static int n_rset;
static bool rset_pick_custom;
static float game_speed;
void rset_open(const rset_args* a) {
  RS = *a;
  n_rset++;
  rset_values v = a->values;
  v.custom = rset_pick_custom;
  if (a->on_start) a->on_start(&v, a->arg);
}
void ui_set_game_speed(float scale) { game_speed = scale; }

static slots_args S;
static rsv_info S_info[RSV_SLOTS]; /* the list's own copy of the caller's info at the last open / refresh */
static int n_slots, n_refresh;
static char last_msg[200];
static void copy_info(void) { memcpy(S_info, S.info, sizeof S_info); }
void slot_screen_open(const slots_args* a) {
  S = *a;
  n_slots++;
  last_msg[0] = 0;
  copy_info();
}
void slot_screen_refresh(void) {
  n_refresh++;
  last_msg[0] = 0;
  copy_info();
}
void slot_screen_message(const char* t) { snprintf(last_msg, sizeof last_msg, "%s", t); }

/* the Nemesis Radar: records how it was opened (and what it was given, copied: the real screen copies it too) */
static radar_args RA;
static char ra_why[200];
static cc_meta ra_meta;
static int n_radar;
static int radar_refusals; /* the next N opens are refused */
bool radar_open(const radar_args* a) {
  n_radar++;
  if (radar_refusals > 0) {
    radar_refusals--;
    return false;
  }
  RA = *a;
  snprintf(ra_why, sizeof ra_why, "%s", a->why ? a->why : "");
  RA.why = ra_why;
  if (a->meta) {
    ra_meta = *a->meta;
    RA.meta = &ra_meta;
  }
  return true;
}

/* the capsule preview (debug): records how it was opened */
static caps_args CP;
static int n_caps, last_caps_n;
bool capsule_debug_fixture(int n, cap_fixture* out) {
  if (n < 1 || n > CAP_MAX) return false;
  memset(out, 0, sizeof *out);
  out->n = (uint8_t)n;
  for (int i = 0; i < n; i++) {
    out->item[i].tier = 0;
    snprintf(out->item[i].label, CAP_LABEL, "r%d", i);
  }
  return true;
}
bool capsule_screen_open(const caps_args* a) {
  CP = *a;
  last_caps_n = a->fixture ? a->fixture->n : 0;
  n_caps++;
  return true;
}

/* the capsule machine (C09): records how it was opened */
static gachas_args GS;
static int n_gacha;
bool gacha_screen_open(const gachas_args* a) {
  GS = *a;
  n_gacha++;
  return true;
}

/* the name entry (Rename Run): records how it was opened; the test types as the player would */
static name_entry_args NE;
static int n_name_entry;
void name_entry_open(const name_entry_args* a) {
  NE = *a;
  n_name_entry++;
}

/* the Run History: records how it was opened */
static hist_args HS;
static int n_history;
void history_screen_open(const hist_args* a) {
  HS = *a;
  n_history++;
}

static result_args R;
static int n_result;
void result_screen_open(const result_args* a) {
  R = *a;
  n_result++;
}

/* the starter select: records how it was opened; the test answers as the player would */
static sh_events SH;
static char sh_notice[200];
static int n_sh;
static bool sh_has_species = true;
bool sh_open(gc_system* sys, const char* notice, const sh_events* ev) {
  (void)sys;
  if (!sh_has_species) return false;
  SH = *ev;
  n_sh++;
  snprintf(sh_notice, sizeof sh_notice, "%s", notice ? notice : "");
  return true;
}
/* the player confirms the first `n` species the system data lists */
static void sh_confirm(int n) {
  static sm_entry list[SS_MAX_ENTRIES];
  uint16_t pick[6];
  const int total = ss_list(sysd_get(), list, SS_MAX_ENTRIES, NULL);
  for (int i = 0; i < n && i < total; i++) pick[i] = list[i].species;
  gc_starter team[6];
  const char* why = NULL;
  if (!ss_team(sysd_get(), pick, n, team, &why)) {
    printf("the fake player's team was refused: %s\n", why ? why : "?");
    exit(1);
  }
  SH.on_start(team, n, SH.arg);
}

const pr_species* menu_species(uint16_t id);
/* win_gains.c's words are not looked at here: the key is enough */
const char* i18n_t(char* out, int n, const char* key, ...) {
  snprintf(out, (size_t)n, "[%s]", key);
  return out;
}
const char* i18n_species_name(int id) {
  const pr_species* sp = menu_species((uint16_t)id);
  return sp ? sp->name : "?";
}

static uint32_t frames = 100;
uint32_t ui_frame(void) { return frames; }

static gc_run RUN;
static rs_config CFG;
static int n_start, n_resume;
static bool fail_start, slot_empty_at_start, sys_saved_ok;
static gc_system snap; /* 724 KB: not on the stack */

static void new_run(gc_run* r, const char* seed) {
  gc_run_new(r, GAME_MODES_CLASSIC, seed, 99);
  gc_starter team[6];
  int n = 0, budget = gc_starter_budget(GAME_MODES_CLASSIC);
  for (int i = 0; i < PR_SPECIES_COUNT && n < 2; i++) {
    const int cost = gc_starter_cost(pr_species_table[i].id);
    if (cost <= 0 || cost > budget) continue;
    memset(&team[n], 0, sizeof team[n]);
    team[n].species = (uint16_t)pr_species_table[i].id;
    team[n].tera_type = -1;
    for (int k = 0; k < 6; k++) team[n].ivs[k] = 20;
    budget -= cost;
    n++;
  }
  const char* why = NULL;
  if (!gc_run_set_starters(r, team, n, &why)) exit(1);
  gc_event e;
  while (gc_run_next_event(r, &e)) {
  }
}

/* What the real driver does around a run, with a tiny battle: it starts observing with the run, sees one turn in which the player's lead
 * faints, and takes the report as the run ends (run_screen.c: drain() and end_run()). */
static uint32_t obs_drain;
static void observe_start(bool resumed) {
  co_begin_run(resumed);
  obs_drain = 0;
  pr_event be[2];
  memset(be, 0, sizeof be);
  be[0].type = PR_EV_DAMAGE;
  be[0].battler = 0;
  be[0].id = 1;
  be[0].value = 20;
  be[0].field[0] = (pr_event_field){.species = 25, .hp = 0, .max_hp = 20};
  be[1] = be[0];
  be[1].type = PR_EV_FAINT;
  be[1].value = 0;
  be[1].id = 0;
  gc_event q[3];
  memset(q, 0, sizeof q);
  q[0].kind = GC_EV_WAVE;
  q[0].a = 4;
  q[1].kind = GC_EV_TURN;
  q[1].a = 1;
  q[2].kind = GC_EV_BATTLE;
  q[2].a = 0;
  q[2].b = 2;
  co_observe(++obs_drain, q, 3, be, 2, false, false);
}
static const cs_defeat* g_defeat; /* what a custom run's fake driver names as the finisher (NULL: none) */
static cs_defeat g_defeat_store;
static void driver_end(int kind) {
  co_finish(kind == GC_SCREEN_CLEAR, RUN.biome, RUN.wave);
  if (CFG.custom) {   /* run_screen.c's end_run: the controller settles the rematch and may name the finisher */
    g_defeat = cr_on_run_end(&RUN, kind, &g_defeat_store) ? &g_defeat_store : NULL;
    cr_end();
  }
  CFG.on_end(kind, CFG.end_arg);
}

bool rs_start(const rs_config* c) {
  CFG = *c;
  n_start++;
  rsv_info in;
  slot_empty_at_start = rsv_inspect(c->slot, &in) == RSV_EMPTY;
  if (fail_start) return false;
  if (c->n_team > 0) {
    /* the run takes the team the player confirmed, as the real driver does */
    gc_run_new(&RUN, GAME_MODES_CLASSIC, c->seed, 99);
    RUN.sys = c->sys;
    if (c->custom) {   /* run_screen.c's new_run: the controller starts with the run, before its starters */
      co_begin_run(false);
      if (!cr_begin(&RUN, c->entropy)) return false;
    }
    const char* why = NULL;
    if (!gc_run_set_starters(&RUN, c->team, c->n_team, &why)) {
      printf("game-core refused the team: %s\n", why ? why : "?");
      exit(1);
    }
    gc_event e;
    while (gc_run_next_event(&RUN, &e)) {
    }
  } else {
    new_run(&RUN, c->seed);
  }
  CHECK(c->save, "a new run must save");
  observe_start(false);
  if (c->custom)
    CHECK(cr_save(c->slot, &RUN) == RSV_SAVED, "the custom run's first save point");
  else
    rsv_save(c->slot, &RUN); /* the first save point */
  sys_saved_ok = c->sys && c->save_sys ? c->save_sys() : true; /* the driver writes it with the run */
  return true;
}

rsv_status rs_resume(const rs_config* c, char why[RSV_WHY_LEN]) {
  CFG = *c;
  n_resume++;
  const rsv_status st = c->custom ? cr_load(c->slot, &RUN, why) : rsv_load(c->slot, &RUN, why);
  if (st == RSV_OK) {
    RUN.sys = c->sys; /* a saved run holds none: the driver attaches the player's */
    if (c->custom)
      cr_begin_observer();
    else
      observe_start(true);
  }
  return st;
}

const gc_run* rs_run(void) { return &RUN; }
const cs_defeat* rs_custom_defeat(void) { return g_defeat; }

/* ------------------------------------------------------------------ helpers */

static void slot_path(int slot, const char* ext, char* out, size_t n) { snprintf(out, n, "%s/session_%d%s", SAVE_DIR, slot, ext); }
static bool exists(const char* p) { return access(p, F_OK) == 0; }
static size_t file_size(const char* p) {
  struct stat st;
  return stat(p, &st) == 0 ? (size_t)st.st_size : 0;
}
static void write_raw(const char* path, const void* data, size_t n) {
  FILE* f = fopen(path, "wb");
  if (f) {
    fwrite(data, 1, n, f);
    fclose(f);
  }
}
static rsv_status status_of(int slot) {
  rsv_info in;
  return rsv_inspect(slot, &in);
}

static rs_config first_cfg(void) {
  rs_config c;
  memset(&c, 0, sizeof c);
  c.entropy = 7;
  c.seed = "flowtestseed000000000001";
  c.starters[0] = 25;
  c.n_starters = 1;
  return c;
}

/* The custom store's writes can be made to fail (the SD card refusing), to check what the flow does with a report it could not keep. */
static bool refuse_custom_writes;
static save_result flow_write(const char* name, const void* data, size_t len) {
  if (refuse_custom_writes) return SAVE_IO_ERROR;
  return save_write(name, data, len);
}
static const cs_io FLOW_IO = {save_read, flow_write};

static bool ledger_has(cc_run_uid uid) {
  return cs_meta() && cc_ledger_find(&cs_meta()->ledger, CC_TXN_TERMINAL, uid, 0, NULL);
}

static save_result fail_system_write(const char* name, const void* data, size_t len) {
  if (!strcmp(name, "system")) return SAVE_IO_ERROR;
  return save_write(name, data, len);
}

int main(void) {
  char dir[64] = "/tmp/flow-test-XXXXXX";
  if (!mkdtemp(dir) || chdir(dir) != 0 || mkdir("sdmc:", 0777) != 0 || !save_init()) {
    printf("FAILED: no folder for the saves\n");
    return 1;
  }
  char p[256], p2[256];
  rs_config first = first_cfg();
  rs_config plain = first_cfg();
  plain.n_starters = 0;
  plain.seed = NULL;

  /* ---- a fresh install: the title has no Continue; New Game asks for a slot; the run is saved in it */
  app_start(&first);
  CHECK(n_title == 1 && !T.can_continue && T.biome == -1, "fresh title: continue %d biome %d", T.can_continue, T.biome);
  T.on_choose(TITLE_NEW_GAME, T.arg);
  CHECK(n_slots == 1 && S.mode == SLOTS_NEW && S.cursor == 0, "New Game opens the slot list in SAVE mode");
  for (int i = 0; i < RSV_SLOTS; i++) CHECK(S_info[i].status == RSV_EMPTY, "slot %d empty", i);
  S.on_pick(1, S.arg);
  CHECK(n_start == 1 && CFG.slot == 1 && CFG.save && CFG.on_end, "the run starts in slot 1 with saving on");
  CHECK(CFG.seed && !strcmp(CFG.seed, "flowtestseed000000000001") && CFG.n_starters == 1 && CFG.starters[0] == 25, "the debug config is used");
  CHECK(n_sh == 0 && !CFG.sys && CFG.n_team == 0, "a developer's team skips the starter select and runs without system data");
  const uint32_t entropy1 = CFG.entropy;
  CHECK(status_of(1) == RSV_OK && rsv_last_slot() == 1, "the run is in slot 1, the last slot");

  /* the run ends in a game over: the slot is cleared and the result screen shows the run */
  CHECK(co_report_get() == NULL, "the run is on: no report yet");
  CHECK(rhist_load() == RH_OK && rhist_count() == 0, "nothing in the Run History yet");
  {
    rsv_info k;
    CHECK(rsv_inspect(1, &k) == RSV_OK && rsv_name_set("session_1", k.run_key, "Named run"), "the run is given a name");
  }
  const int64_t before_end = (int64_t)time(NULL);
  driver_end(GC_SCREEN_GAME_OVER);
  CHECK(n_result == 1 && !R.victory && R.run.status == RSV_OK && R.run.wave == RUN.wave && R.run.n_party == 2, "game over screen");
  CHECK(R.gains == NULL && !R.ending, "a game over is given no gains and no ending");
  CHECK(R.report && R.report == co_report_get() && R.report->valid && R.report->outcome == CC_OUT_DEFEAT, "the result screen gets the run's combat report");
  CHECK(R.report && R.report->wave == RUN.wave && R.report->biome == RUN.biome && R.report->fallen_known && R.report->fallen_species == 25 &&
            R.report->history.n_turns == 1 && R.report->history.turns[0].wave == 4,
        "the report describes the run that ended");
  CHECK(status_of(1) == RSV_EMPTY && rsv_last_slot() == -1, "the slot of a finished run is cleared and forgotten");
  CHECK(R.report && R.report->valid && R.report->history.n_turns == 1, "and the report is still whole after the slot is cleared");
  CHECK(R.report && R.report->uid_durable && cs_meta() && cs_meta()->has_report && cs_meta()->report.run_uid == R.report->run_uid &&
            ledger_has(R.report->run_uid) && cs_meta()->ledger.n == 1 && !R.note,
        "the run's end stored one report under its durable uid, and the result screen has nothing to apologise for");
  /* the Run History has the run: its result, wave, party, time and the name it was given */
  CHECK(rhist_load() == RH_OK && rhist_count() == 1, "the run ended: one entry in the Run History (%d)", rhist_count());
  {
    const rh_entry* h = rhist_get(0);
    CHECK(h && !h->victory && h->wave == RUN.wave && h->biome == RUN.biome && h->score == RUN.score && h->mode == RUN.run.mode, "the entry has the run's result");
    CHECK(h && h->n_party == 2 && h->party[0].species == R.run.party[0].species && h->party[0].level == (uint32_t)R.run.party[0].level, "and its party");
    CHECK(h && h->time >= before_end && h->time <= (int64_t)time(NULL), "and when it ended");
    CHECK(h && !strcmp(h->name, "Named run"), "and the name the player gave it: \"%s\"", h ? h->name : "(none)");
  }
  R.on_done(R.arg);
  CHECK(n_title == 2 && !T.can_continue, "back to the title, nothing to continue");

  /* the title's Run History opens the history, read from the card, and B comes back to the title */
  T.on_choose(TITLE_HISTORY, T.arg);
  CHECK(n_history == 1 && HS.status == RH_OK && HS.on_exit && rhist_count() == 1, "Run History opens the screen with the entries");
  HS.on_exit(HS.arg);
  CHECK(n_title == 3, "leaving it shows the title again");
  n_title--; /* the counts below were written without this detour */

  /* ---- a second run: the debug config is not used again, and its randomness differs */
  frames += 357;
  T.on_choose(TITLE_NEW_GAME, T.arg);
  S.on_pick(0, S.arg);
  CHECK(n_sh == 1 && n_start == 1 && !sh_notice[0], "the slot is picked, then the starter select opens (no run yet, no notice)");
  sh_confirm(2);
  CHECK(n_start == 2 && CFG.slot == 0 && !CFG.seed && CFG.n_starters == 0, "the second New Game has no debug seed or starters");
  CHECK(CFG.n_team == 2 && CFG.team[0].species && CFG.sys == sysd_get() && CFG.save_sys == sysd_save, "it has the team chosen and the system data");
  CHECK(RUN.n_party == 2 && RUN.party[0].b.species == CFG.team[0].species && RUN.sys == sysd_get(), "the run is the team's");
  CHECK(sys_saved_ok && file_size((snprintf(p, sizeof p, "%s/system.sav", SAVE_DIR), p)) > 0, "the system data was written with the run's first save");
  CHECK(CFG.entropy != entropy1, "each run has its own entropy");
  const int biome0 = RUN.biome;

  /* ---- quit and boot again: Continue loads the last slot; the title shows its biome */
  app_start(&plain); /* boots again with no developer team */
  CHECK(T.can_continue && T.biome == biome0, "Continue is offered, with the biome %d (%d)", T.biome, biome0);
  T.on_choose(TITLE_CONTINUE, T.arg);
  CHECK(n_resume == 1 && CFG.slot == 0 && CFG.save && CFG.on_end, "Continue loads slot 0 with saving on");
  CHECK(!CFG.seed && CFG.n_starters == 0, "a loaded run takes no debug config");
  CHECK(CFG.sys == sysd_get() && RUN.sys == sysd_get() && CFG.save_sys == sysd_save, "a loaded run takes the player's system data");

  /* ---- Load Game: the list in LOAD mode; picking a run loads it */
  T.on_choose(TITLE_LOAD_GAME, T.arg);
  CHECK(S.mode == SLOTS_LOAD && S_info[0].status == RSV_OK && S_info[1].status == RSV_EMPTY, "the load list shows slot 0");
  S.on_pick(0, S.arg);
  CHECK(n_resume == 2 && CFG.slot == 0, "Load Game loads the slot picked");
  S.on_back(S.arg);
  CHECK(T.can_continue, "back to the title");

  /* ---- Save and Quit (the in-run menu): the driver calls on_quit once it has faded to black; the app shows the title, the slot holds
   * the save of the battle's start (Continue loads it), and what the battle changed in memory and never wrote is gone */
  T.on_choose(TITLE_CONTINUE, T.arg);
  CHECK(CFG.on_quit && n_resume == 3, "a loaded run has a Save and Quit to call");
  {
    gc_mon mewtwo;
    memset(&mewtwo, 0, sizeof mewtwo);
    mewtwo.b.species = 150;
    mewtwo.b.nature = 3;
    CHECK(sysd_get()->dex[150].caught_attr == 0, "no Mewtwo in the system data on the card");
    gc_system_set_pokemon_caught(RUN.sys, &mewtwo, true, false);
    CHECK(sysd_get()->dex[150].caught_attr != 0 && RUN.sys == sysd_get(), "the battle caught one, in memory only");
    const int titles_q = n_title, results_q = n_result;
    CFG.on_quit(CFG.quit_arg);
    CHECK(n_title == titles_q + 1 && n_result == results_q, "Save and Quit shows the title and no result screen: %d titles, %d results", n_title - titles_q,
          n_result - results_q);
    CHECK(T.can_continue && status_of(0) == RSV_OK && rsv_last_slot() == 0, "the slot is still there and Continue offers it");
    CHECK(sysd_state(NULL) == SYSD_LOADED && sysd_get()->dex[150].caught_attr == 0, "what the battle caught in memory went with the battle (the card's system data is read again)");
    T.on_choose(TITLE_CONTINUE, T.arg);
    CHECK(n_resume == 4 && RUN.sys == sysd_get(), "Continue loads the run again, with the card's system data");
    CHECK(sysd_get()->dex[150].caught_attr == 0, "and the Mewtwo is still not caught");
  }

  /* ---- Rename Run: the list offers it, the entry is given upstream's words and the 20 character limit, the name is kept beside the slot and
   * shows on the list; cancelling or an empty name change nothing; a refusing card is said; the name goes with the run */
  {
    T.on_choose(TITLE_LOAD_GAME, T.arg);
    CHECK(S.on_rename && S_info[0].status == RSV_OK && S_info[0].name[0] == '\0', "the list takes Rename Run, and the run has no name yet");
    const int opens = n_name_entry, lists = n_slots;
    S.on_rename(0, S.arg);
    CHECK(n_name_entry == opens + 1 && !strcmp(NE.title_key, "menu:renameRun") && !strcmp(NE.label_key, "menu:runName") &&
              !strcmp(NE.ok_key, "menu:rename") && !strcmp(NE.cancel_key, "menu:cancel") && NE.max_chars == 20 && NE.on_done,
          "the entry opens with upstream's words and 20 characters");
    char np[300], np2[300];
    snprintf(np, sizeof np, "%s/session_0_name.sav", SAVE_DIR);
    CHECK(!exists(np), "no name file yet");
    NE.on_done("Jojo Run", NE.arg);
    CHECK(n_slots == lists + 1 && S.mode == SLOTS_LOAD && S.cursor == 0, "the list is back on the slot");
    CHECK(!strcmp(S_info[0].name, "Jojo Run") && exists(np), "the name shows on the list and is on the card: \"%s\"", S_info[0].name);
    CHECK(status_of(0) == RSV_OK, "the run is still a run");
    /* the next save of the run does not take the name away */
    rsv_save(0, &RUN);
    S.on_back(S.arg);
    T.on_choose(TITLE_LOAD_GAME, T.arg);
    CHECK(!strcmp(S_info[0].name, "Jojo Run"), "the name survives the run's next save: \"%s\"", S_info[0].name);
    /* cancelled, and an empty name (upstream's renameSession does nothing for it) */
    S.on_rename(0, S.arg);
    NE.on_done(NULL, NE.arg);
    CHECK(!strcmp(S_info[0].name, "Jojo Run") && n_slots > lists && !last_msg[0], "cancelled: the name is as it was, no message");
    S.on_rename(0, S.arg);
    NE.on_done("", NE.arg);
    CHECK(!strcmp(S_info[0].name, "Jojo Run") && !last_msg[0], "an empty name changes nothing and says nothing");
    /* a new name replaces it */
    S.on_rename(0, S.arg);
    NE.on_done("Second", NE.arg);
    CHECK(!strcmp(S_info[0].name, "Second"), "a new name replaces the old one: \"%s\"", S_info[0].name);
    /* the card refuses the write: told, the name is as it was */
    snprintf(np2, sizeof np2, "%s.tmp", np);
    CHECK(mkdir(np2, 0777) == 0, "block the name's temp file");
    S.on_rename(0, S.arg);
    NE.on_done("Blocked", NE.arg);
    CHECK(last_msg[0] && strstr(last_msg, "name") && !strcmp(S_info[0].name, "Second"), "a refusing card is told and the name is as it was: \"%s\"", last_msg);
    rmdir(np2);
    /* a run in another slot does not take the name; a slot that holds a run with another seed does not show it */
    new_run(&RUN, "flowtestseed00000000000N");
    CHECK(rsv_save(4, &RUN) == RSV_SAVED, "another run in slot 4");
    S.on_back(S.arg);
    T.on_choose(TITLE_LOAD_GAME, T.arg);
    CHECK(S_info[4].status == RSV_OK && S_info[4].name[0] == '\0', "another slot has no name");
    snprintf(np2, sizeof np2, "%s/session_4_name.sav", SAVE_DIR);
    CHECK(!exists(np2), "and no file");
    CHECK(rsv_name_set("session_4", S_info[0].run_key, "Wrong run"), "a name file for another run's key is left in slot 4");
    S.on_back(S.arg);
    T.on_choose(TITLE_LOAD_GAME, T.arg);
    CHECK(S_info[4].name[0] == '\0', "it is not shown for the run that is in slot 4: \"%s\"", S_info[4].name);
    /* Delete Run takes the name with the run */
    S.on_delete(4, S.arg);
    CHECK(status_of(4) == RSV_EMPTY && !exists(np2), "Delete Run on slot 4 removed the run and the name file it had");
    S.on_delete(0, S.arg);
    CHECK(status_of(0) == RSV_EMPTY && !exists(np), "Delete Run on slot 0 removed the run and its name");
    new_run(&RUN, "flowtestseed000000000001");
    rsv_save(0, &RUN); /* slot 0 holds a run again, for what follows */
    S.on_back(S.arg);
  }

  /* ---- New Game over a run: the old run goes (with its backup) before the new one starts */
  rsv_save(0, &RUN); /* a second save of slot 0: it now has a backup */
  slot_path(0, ".sav.bak", p, sizeof p);
  CHECK(exists(p), "slot 0 has a backup");
  T.on_choose(TITLE_NEW_GAME, T.arg);
  CHECK(S_info[0].status == RSV_OK, "the list shows the run in slot 0");
  S.on_pick(0, S.arg); /* the list asked "Overwrite?" and the answer was yes */
  CHECK(n_sh == 2 && n_start == 2 && status_of(0) == RSV_OK && exists(p), "the starter select opens; the old run and its backup are still there (%d %d %d %d)", n_sh, n_start, status_of(0), exists(p));
  const int titles = n_title;
  SH.on_exit(SH.arg); /* "Do you want to exit?" Yes */
  CHECK(n_title == titles + 1 && n_start == 2 && status_of(0) == RSV_OK && exists(p), "Exit goes back to the title and the old run is untouched");
  CHECK(T.can_continue, "and Continue still offers it");
  T.on_choose(TITLE_NEW_GAME, T.arg);
  S.on_pick(0, S.arg);
  CHECK(n_sh == 3 && n_start == 2 && status_of(0) == RSV_OK, "the second time: starter select again, still no new run");
  sh_confirm(1); /* Begin */
  CHECK(n_start == 3 && slot_empty_at_start && CFG.n_team == 1, "the old run was removed once the team was confirmed, before the new run started");
  slot_path(0, ".sav.bak", p, sizeof p);
  CHECK(!exists(p), "the new run has no backup of the old one");

  /* ---- clear: the same, with the clear flag; what the run caught is on the SD card when it ends */
  {
    gc_mon pikachu;
    memset(&pikachu, 0, sizeof pikachu);
    pikachu.b.species = 25;
    pikachu.b.nature = 11;
    pikachu.b.gender = 0;
    gc_system_set_pokemon_caught(RUN.sys, &pikachu, true, false);
  }
  CHECK(sysd_get()->dex[25].caught_attr != 0, "the run caught a Pikachu");
  /* what game-core's finish writes into the system data when a win unlocks things: the app told the run's start (wg_snapshot in run_config)
   * and hands the difference to the result screen */
  sysd_get()->unlocks.eviolite = true;
  sysd_get()->voucher_counts[GC_VOUCHER_PREMIUM]++;
  memcpy(&snap, sysd_get(), sizeof snap);
  driver_end(GC_SCREEN_CLEAR);
  CHECK(n_result == 2 && R.victory && status_of(0) == RSV_EMPTY, "clear screen, slot cleared");
  CHECK(R.ending, "a Classic clear opens on the rival's ending");
  CHECK(R.gains && R.gains->n == 2 && R.gains->item[0].kind == WG_UNLOCK && R.gains->item[0].id == WG_U_EVIOLITE && R.gains->item[1].kind == WG_PREMIUM,
        "the clear's result screen is given what the win changed in the system data: %d items", R.gains ? R.gains->n : -1);
  CHECK(rhist_load() == RH_OK && rhist_count() == 2 && rhist_get(0) && rhist_get(0)->victory && rhist_get(0)->defeat == RH_DEFEAT_NONE && !rhist_get(0)->name[0] &&
            rhist_get(1) && !rhist_get(1)->victory && !strcmp(rhist_get(1)->name, "Named run"),
        "the clear is the newest entry, with no name; the first run's is behind it");
  CHECK(R.report && R.report->outcome == CC_OUT_CLEAR && !R.report->fallen_known, "a clear's report says clear and names nobody");
  sysd_reset(); /* quit and boot again */
  CHECK(sysd_state(NULL) == SYSD_LOADED && !memcmp(&snap, sysd_get(), sizeof snap), "the run's catch is in the system data on the SD card");
  R.on_done(R.arg);

  /* ---- a slot that can't be loaded: Continue says why, the file is never touched, never overwritten */
  new_run(&RUN, "flowtestseed000000000002");
  CHECK(rsv_save(2, &RUN) == RSV_SAVED, "save");
  slot_path(2, ".sav", p, sizeof p);
  const size_t good = file_size(p);
  char junk[400];
  memset(junk, 0x5a, sizeof junk);
  write_raw(p, junk, sizeof junk); /* damaged: not a save any more */
  slot_path(2, ".sav.bak", p2, sizeof p2);
  remove(p2);
  CHECK(good > 0 && file_size(p) == sizeof junk, "the damaged file");
  app_start(&plain); /* boots again with no developer team */
  CHECK(T.can_continue, "an unreadable slot still offers Continue (to say why)");
  const int slots_before = n_slots, resumes_before = n_resume;
  T.on_choose(TITLE_CONTINUE, T.arg);
  CHECK(n_resume == resumes_before + 1, "Continue tried");
  CHECK(n_slots == slots_before + 1 && S.mode == SLOTS_LOAD && S.cursor == 2, "the load list opens on the slot");
  CHECK(last_msg[0] && strstr(last_msg, "damaged"), "the message says why: \"%s\"", last_msg);
  CHECK(S_info[2].status == RSV_INCOMPATIBLE, "the slot shows as incompatible");
  CHECK(file_size(p) == sizeof junk, "the file is untouched");
  /* New Game on it: refused, no run starts, the file stays */
  S.on_back(S.arg);
  T.on_choose(TITLE_NEW_GAME, T.arg);
  const int starts_before = n_start;
  S.on_pick(2, S.arg);
  CHECK(n_start == starts_before, "a New Game must not start over an unreadable slot");
  CHECK(last_msg[0] && file_size(p) == sizeof junk, "it said so, and the file is untouched: \"%s\"", last_msg);
  /* Delete Run is how it goes */
  S.on_back(S.arg);
  T.on_choose(TITLE_LOAD_GAME, T.arg);
  S.on_delete(2, S.arg);
  CHECK(!exists(p) && status_of(2) == RSV_EMPTY && S_info[2].status == RSV_EMPTY, "Delete Run removes it");
  S.on_back(S.arg);
  CHECK(!T.can_continue, "nothing left to continue");

  /* ---- a slot that was fine on the list and is not when loaded: the list says why and shows it as it is now */
  new_run(&RUN, "flowtestseed000000000003");
  rsv_save(3, &RUN);
  T.on_choose(TITLE_LOAD_GAME, T.arg);
  CHECK(S_info[3].status == RSV_OK, "listed as a run");
  slot_path(3, ".sav", p, sizeof p);
  write_raw(p, junk, sizeof junk);
  slot_path(3, ".sav.bak", p2, sizeof p2);
  remove(p2);
  const int refresh_before = n_refresh;
  S.on_pick(3, S.arg);
  CHECK(n_refresh == refresh_before + 1 && last_msg[0] && S_info[3].status == RSV_INCOMPATIBLE, "the list was refreshed and says why: \"%s\"", last_msg);
  CHECK(file_size(p) == sizeof junk, "untouched");

  /* ---- a system file that can't be read: the starter select says so, the file is never written over */
  snprintf(p, sizeof p, "%s/system.sav", SAVE_DIR);
  write_raw(p, junk, sizeof junk);
  snprintf(p2, sizeof p2, "%s/system.sav.bak", SAVE_DIR);
  remove(p2);
  sysd_reset();
  S.on_back(S.arg);
  T.on_choose(TITLE_NEW_GAME, T.arg);
  const int sh_before = n_sh;
  S.on_pick(4, S.arg);
  CHECK(n_sh == sh_before + 1 && strstr(sh_notice, "damaged") && strstr(sh_notice, "left as it is"), "the starter select tells the player: \"%s\"", sh_notice);
  sh_confirm(1); /* the defaults are enough to begin */
  CHECK(n_start == 4 && CFG.sys == sysd_get() && sys_saved_ok, "the run starts; saving its system data reports no failure (it writes nothing)");
  CHECK(file_size(p) == sizeof junk, "the damaged system file is untouched");
  CHECK(co_report_get() == NULL, "the new run has no report of the one before");
  /* the card refuses the Run History's write: the result screen says so, and the run still ends */
  CHECK(mkdir(SAVE_DIR "/run_history.sav.tmp", 0777) == 0, "block the history's temp file with a folder");
  CFG.on_end(GC_SCREEN_GAME_OVER, CFG.end_arg); /* a driver that never finished the observer: nothing stale is passed on */
  CHECK(R.note && strstr(R.note, "Run History"), "a Run History that can't be written is told: \"%s\"", R.note ? R.note : "(none)");
  rmdir(SAVE_DIR "/run_history.sav.tmp");
  CHECK(rhist_load() == RH_OK && rhist_count() == 2, "and the history is as it was (%d)", rhist_count());
  CHECK(R.report == NULL, "no report: the result screen gets none, not the old one");
  CHECK(file_size(p) == sizeof junk, "and still untouched when the run ends");
  R.on_done(R.arg);
  remove(p);
  sysd_reset();

  /* ---- nothing to start with: told, nothing opened */
  sh_has_species = false;
  T.on_choose(TITLE_NEW_GAME, T.arg);
  const int starts_none = n_start;
  S.on_pick(4, S.arg);
  CHECK(n_start == starts_none && last_msg[0], "no species: told \"%s\"", last_msg);
  sh_has_species = true;

  /* ---- a run that can't be set up: told, nothing started, and the slot list is back (the starter select is gone) */
  fail_start = true;
  S.on_back(S.arg);
  T.on_choose(TITLE_NEW_GAME, T.arg);
  S.on_pick(4, S.arg);
  const int slots_open = n_slots;
  sh_confirm(1);
  CHECK(n_slots == slots_open + 1 && S.mode == SLOTS_NEW && S.cursor == 4, "the slot list is shown again, on the slot");
  CHECK(last_msg[0] && status_of(4) == RSV_EMPTY, "told: \"%s\"", last_msg);

  /* ---- C04: the custom store through the whole flow (run start, run end, reopen, reboot, a refusing card, an unusable file) */
  fail_start = false;
  S.on_back(S.arg);
  remove((snprintf(p, sizeof p, "%s/custom.sav", SAVE_DIR), p));
  remove((snprintf(p2, sizeof p2, "%s/custom.sav.bak", SAVE_DIR), p2));
  cs_reset();
  T.on_choose(TITLE_NEW_GAME, T.arg);
  S.on_pick(4, S.arg);
  sh_confirm(1);
  CHECK(exists(p) && cs_get_state(NULL) == CS_LOADED && cs_meta()->counters.next_run_seq == 2 && !cs_meta()->has_report,
        "a run's start created the custom file with the first uid issued and no report");
  const cc_run_uid uid_a = ((cc_run_uid)cs_meta()->counters.install_nonce << 32) | 1u;
  driver_end(GC_SCREEN_GAME_OVER);
  CHECK(R.report && R.report->run_uid == uid_a && R.report->uid_durable && !R.note, "the run ended under the uid the file handed out");
  CHECK(cs_meta()->has_report && cs_meta()->report.run_uid == uid_a && cs_meta()->ledger.n == 1 && ledger_has(uid_a) && cs_meta()->roster.count == 0,
        "one report stored, one ledger entry, no Nemesis (none is ever invented)");
  CHECK(cs_meta()->history.n_turns == 1 && cs_meta()->history.turns[0].wave == 4, "with the post-mortem's last turn");
  {
    const size_t before_size = file_size(p);
    char* raw_a = NULL;
    size_t len_a = 0;
    bool used_backup = false;
    CHECK(save_read("custom", &raw_a, &len_a, &used_backup) == SAVE_OK, "read the file");
    const cc_txn_id next_txn = cs_meta()->counters.next_txn_id;
    /* the same end delivered again (a retried callback): nothing changes on the card */
    CFG.on_end(GC_SCREEN_GAME_OVER, CFG.end_arg);
    char* raw_b = NULL;
    size_t len_b = 0;
    CHECK(save_read("custom", &raw_b, &len_b, &used_backup) == SAVE_OK && len_a == len_b && !memcmp(raw_a, raw_b, len_a) && file_size(p) == before_size,
          "the same end twice leaves the file byte for byte as it was");
    CHECK(cs_meta()->ledger.n == 1 && cs_meta()->counters.next_txn_id == next_txn, "and the ledger and counters unchanged");
    free(raw_a);
    free(raw_b);
  }
  R.on_done(R.arg);

  /* quit and boot again: the card holds the report */
  cs_reset();
  CHECK(cs_get_state(NULL) == CS_LOADED && !cs_from_backup() && cs_meta()->has_report && cs_meta()->report.run_uid == uid_a && ledger_has(uid_a),
        "after a reboot the report and the ledger are read back");
  app_start(&plain);
  T.on_choose(TITLE_NEW_GAME, T.arg);
  S.on_pick(4, S.arg);
  sh_confirm(1);
  CHECK(cs_meta()->counters.next_run_seq == 3, "the next run took the next uid, even across a reboot");
  const cc_run_uid uid_b = uid_a + 1;

  /* the card refuses the write at the run's end: the player is told, the old report stays, and leaving the result screen tries once more */
  cs_set_io(&FLOW_IO);
  refuse_custom_writes = true;
  driver_end(GC_SCREEN_GAME_OVER);
  CHECK(R.report && R.report->run_uid == uid_b && R.note && strstr(R.note, "SD card"), "a refused write is told: \"%s\"", R.note ? R.note : "(none)");
  CHECK(cs_meta()->report.run_uid == uid_a && !ledger_has(uid_b), "memory still matches the card: the old report");
  refuse_custom_writes = false;
  R.on_done(R.arg);
  CHECK(cs_meta()->report.run_uid == uid_b && ledger_has(uid_b) && ledger_has(uid_a) && cs_meta()->ledger.n == 2,
        "leaving the result screen stored it (once more), next to the first");
  cs_reset();
  CHECK(cs_get_state(NULL) == CS_LOADED && cs_meta()->report.run_uid == uid_b && cs_meta()->ledger.n == 2, "and the card agrees after a reboot");

  /* a refusing card for good: the retry fails too, nothing is stored and the game goes on */
  T.on_choose(TITLE_NEW_GAME, T.arg);
  S.on_pick(4, S.arg);
  sh_confirm(1);
  const cc_run_uid uid_c = uid_b + 1;
  refuse_custom_writes = true;
  driver_end(GC_SCREEN_GAME_OVER);
  const int titles_c = n_title;
  R.on_done(R.arg);
  CHECK(n_title == titles_c + 1 && cs_meta()->report.run_uid == uid_b && !ledger_has(uid_c), "still refused after the retry: back to the title, nothing stored");
  refuse_custom_writes = false;
  cs_set_io(NULL);

  /* the custom file is unusable: nothing stored, nothing touched, the run is not affected, the player is told why */
  {
    char* raw = NULL;
    size_t len = 0;
    bool used_backup = false;
    cs_reset();
    CHECK(save_read("custom", &raw, &len, &used_backup) == SAVE_OK, "read the good file");
    free(raw);
    remove(p2);
    write_raw(p, junk, sizeof junk);
    cs_reset();
    T.on_choose(TITLE_NEW_GAME, T.arg);
    S.on_pick(4, S.arg);
    sh_confirm(1);
    CHECK(cs_get_state(NULL) == CS_UNREADABLE && cs_meta() == NULL, "an unusable custom file is not used");
    driver_end(GC_SCREEN_GAME_OVER);
    CHECK(n_result > 0 && R.report && !R.report->uid_durable, "the run goes on and ends under a session uid");
    CHECK(R.note && strstr(R.note, "damaged") && strstr(R.note, "left as it is"), "the player is told why: \"%s\"", R.note ? R.note : "(none)");
    R.on_done(R.arg);
    CHECK(file_size(p) == sizeof junk && !exists(p2), "the unusable file is exactly as it was and no backup or temporary file appeared");
    snprintf(p2, sizeof p2, "%s/custom.sav.tmp", SAVE_DIR);
    CHECK(!exists(p2), "no temporary file");

    /* the Nemesis Radar over the same unusable file: shown with its reason, handed no records, nothing written, back to the title on exit */
    {
      cs_reset();
      const int radars = n_radar, titles = n_title;
      T.on_choose(TITLE_RADAR, T.arg);
      CHECK(n_radar == radars + 1 && !RA.meta && strstr(RA.why, "damaged") && !RA.schedule && RA.on_exit, "the Radar is told the file cannot be used, and why: \"%s\"", RA.why);
      CHECK(file_size(p) == sizeof junk && !exists(p2), "and it left the unusable file exactly as it was");
      RA.on_exit(RA.arg);
      CHECK(n_title == titles + 1, "leaving the Radar is the title again");
    }
  }
  remove(p);
  cs_reset();

  /* no custom file yet: the Radar opens on an empty roster and does not create the file */
  {
    CHECK(!exists(p), "no custom file");
    const int radars = n_radar;
    T.on_choose(TITLE_RADAR, T.arg);
    CHECK(n_radar == radars + 1 && RA.meta && RA.meta->roster.count == 0 && !RA.schedule, "an absent file is an empty roster");
    CHECK(!exists(p), "and opening the Radar did not create the file");
    /* the screen refusing to open (a roster that does not validate): it is tried once more as unusable, never silently nothing */
    radar_refusals = 1;
    T.on_choose(TITLE_RADAR, T.arg);
    CHECK(n_radar == radars + 3 && !RA.meta && RA.why[0], "a refused open falls back to the unusable view with a reason: \"%s\"", RA.why);
  }
  cs_reset();

  /* ---- the run settings before New Game (Custom, Game Speed) and one slot list for both kinds of run, each labelled (lorenzo,
   * 2026-10-19: "do not keep custom load, simply there should be a quick indicator upon selecting a run saying wether the run is classic
   * or custom"). Was C10's separate Custom Run / Custom Load lists. */
  {
    sh_has_species = true;
    cs_reset();
    sysd_reset();
    ptx_reset();
    app_start(&plain);
    char cp[300];
    snprintf(cp, sizeof cp, "%s/custom.sav", SAVE_DIR);
    remove(cp);
    snprintf(cp, sizeof cp, "%s/custom.sav.bak", SAVE_DIR);
    remove(cp);
    cs_reset();
    for (int i = 0; i < RSV_SLOTS; i++) CHECK(rsv_clear(i) && cr_clear(i), "slot %d starts empty", i);
    new_run(&RUN, "flowcustomseed00000000001");
    CHECK(rsv_save(0, &RUN) == RSV_SAVED, "an ordinary run in slot 0");
    app_start(&plain);
    CHECK(T.can_continue, "the ordinary run is on the title");

    /* New Game opens the run settings first; Custom: Yes lists the same five slots, the ordinary run included */
    const int slots0 = n_slots, sh0 = n_sh, start0 = n_start, rset0 = n_rset;
    rset_pick_custom = true;
    T.on_choose(TITLE_NEW_GAME, T.arg);
    CHECK(n_rset == rset0 + 1 && n_slots == slots0 + 1 && S.mode == SLOTS_NEW, "New Game opens the run settings, then the slot list");
    CHECK(S_info[0].status == RSV_OK && !S_info[0].custom, "the list shows the ordinary run in slot 0, labelled as such");
    for (int i = 1; i < RSV_SLOTS; i++) CHECK(S_info[i].status == RSV_EMPTY, "slot %d is empty", i);
    S.on_pick(2, S.arg);
    CHECK(n_sh == sh0 + 1 && n_start == start0, "the starter select opens (a developer's team is not used for a custom run)");
    sh_confirm(1);
    CHECK(n_start == start0 + 1 && CFG.custom && CFG.slot == 2 && CFG.save && CFG.on_end && CFG.n_team == 1 && !CFG.seed, "the run starts as a custom run in slot 2");
    rsv_info inf;
    CHECK(cr_inspect(2, &inf) == RSV_OK && cr_active(), "it is saved as a custom run and the controller is on");
    CHECK(status_of(2) == RSV_EMPTY && status_of(0) == RSV_OK, "no ordinary run in slot 2, slot 0 as it was");
    snprintf(cp, sizeof cp, "%s/csession_2.sav", SAVE_DIR);
    CHECK(exists(cp), "the file is csession_2");
    slot_path(2, ".sav", p, sizeof p);
    CHECK(!exists(p), "and there is no ordinary session_2");
    CHECK(rsv_last_slot() == 2, "Continue's slot is the custom run's");

    /* Continue picks the custom run up as a custom run */
    cr_end();
    app_start(&plain);
    const int res00 = n_resume;
    CHECK(T.can_continue, "the custom run is Continue's");
    T.on_choose(TITLE_CONTINUE, T.arg);
    CHECK(n_resume == res00 + 1 && CFG.custom && CFG.slot == 2 && cr_active(), "Continue resumes it as a custom run");

    /* Load Game lists both runs, each with its kind; Rename Run on the custom one names its own file */
    cr_end();
    app_start(&plain);
    T.on_choose(TITLE_LOAD_GAME, T.arg);
    CHECK(S.mode == SLOTS_LOAD && S_info[2].status == RSV_OK && S_info[2].custom && S_info[0].status == RSV_OK && !S_info[0].custom,
          "Load Game lists the custom run (labelled Custom) beside the ordinary one");
    CHECK(S.on_rename && S_info[2].name[0] == '\0', "Rename Run is offered");
    S.on_rename(2, S.arg);
    NE.on_done("Custom one", NE.arg);
    char nc[300], no[300];
    snprintf(nc, sizeof nc, "%s/csession_2_name.sav", SAVE_DIR);
    snprintf(no, sizeof no, "%s/session_2_name.sav", SAVE_DIR);
    CHECK(S.mode == SLOTS_LOAD && !strcmp(S_info[2].name, "Custom one") && exists(nc) && !exists(no), "the custom run's name is beside the custom file: \"%s\"", S_info[2].name);

    /* picked from Load Game it resumes as a custom run, and its end clears the custom file only */
    const int res01 = n_resume;
    S.on_pick(2, S.arg);
    CHECK(n_resume == res01 + 1 && CFG.custom && CFG.slot == 2 && cr_active(), "Load Game resumes it as a custom run");
    CHECK(RUN.sys == sysd_get(), "with the player's system data");
    driver_end(GC_SCREEN_GAME_OVER);
    CHECK(!exists(nc), "the custom run's name went with it");
    CHECK(!cr_active() && cr_inspect(2, &inf) == RSV_EMPTY && !exists(cp), "the custom file of a finished run is cleared");
    CHECK(status_of(0) == RSV_OK, "the ordinary run is untouched");
    R.on_done(R.arg);

    /* Custom: No is an ordinary run */
    rset_pick_custom = false;
    T.on_choose(TITLE_NEW_GAME, T.arg);
    S.on_pick(1, S.arg);
    sh_confirm(1);
    CHECK(!CFG.custom && CFG.slot == 1 && !cr_active(), "an ordinary run: no custom flag, no controller");
    driver_end(GC_SCREEN_GAME_OVER);
    R.on_done(R.arg);

    /* a slot is one place for either kind: an ordinary New Game over a custom run replaces it */
    rset_pick_custom = true;
    T.on_choose(TITLE_NEW_GAME, T.arg);
    S.on_pick(3, S.arg);
    sh_confirm(1);
    CHECK(CFG.custom && CFG.slot == 3 && cr_inspect(3, &inf) == RSV_OK, "a custom run in slot 3");
    cr_end();
    app_start(&plain);
    rset_pick_custom = false;
    T.on_choose(TITLE_NEW_GAME, T.arg);
    CHECK(S_info[3].status == RSV_OK && S_info[3].custom, "the new-game list shows it as the custom run it is");
    S.on_pick(3, S.arg);
    sh_confirm(1);
    snprintf(cp, sizeof cp, "%s/csession_3.sav", SAVE_DIR);
    CHECK(!CFG.custom && CFG.slot == 3 && cr_inspect(3, &inf) == RSV_EMPTY && !exists(cp), "the ordinary run took the slot and the custom file went");
    CHECK(rsv_save(3, rs_run()) == RSV_SAVED && status_of(3) == RSV_OK, "the ordinary run is saved in slot 3");
    driver_end(GC_SCREEN_GAME_OVER);
    R.on_done(R.arg);

    /* Delete Run on a custom run removes its file only */
    rset_pick_custom = true;
    T.on_choose(TITLE_NEW_GAME, T.arg);
    S.on_pick(4, S.arg);
    sh_confirm(1);
    cr_end();
    app_start(&plain);
    T.on_choose(TITLE_LOAD_GAME, T.arg);
    CHECK(S_info[4].status == RSV_OK && S_info[4].custom, "a custom run in slot 4");
    S.on_delete(4, S.arg);
    snprintf(cp, sizeof cp, "%s/csession_4.sav", SAVE_DIR);
    CHECK(!exists(cp) && S_info[4].status == RSV_EMPTY && status_of(0) == RSV_OK, "Delete Run removes the custom file only");
    S.on_back(S.arg);

    /* from before the lists were one, a slot may hold both: the ordinary run shows, and the custom one once that is deleted */
    T.on_choose(TITLE_NEW_GAME, T.arg);
    S.on_pick(4, S.arg);
    sh_confirm(1);
    cr_end();
    new_run(&RUN, "flowcustomseed00000000002");
    CHECK(rsv_save(4, &RUN) == RSV_SAVED, "an ordinary run written beside the custom one in slot 4");
    app_start(&plain);
    T.on_choose(TITLE_LOAD_GAME, T.arg);
    CHECK(S_info[4].status == RSV_OK && !S_info[4].custom, "both: the ordinary run shows");
    S.on_delete(4, S.arg);
    CHECK(S_info[4].status == RSV_OK && S_info[4].custom, "deleted, the custom run shows");
    S.on_delete(4, S.arg);
    CHECK(S_info[4].status == RSV_EMPTY, "and deleted too, the slot is empty");
    S.on_back(S.arg);
    rset_pick_custom = false;

    /* the game speed is kept and applied at the start; the screen gets the saved values */
    const rset_values fast = {true, RSET_SPEED_TURBO};
    CHECK(rset_save(&fast), "the settings are written");
    game_speed = 0;
    app_start(&plain);
    CHECK(game_speed == rset_time_scale(RSET_SPEED_TURBO), "app_start applies the saved speed (%.1f)", game_speed);
    T.on_choose(TITLE_NEW_GAME, T.arg);
    CHECK(RS.values.speed == RSET_SPEED_TURBO && RS.values.custom, "the run settings open with the saved values");
    S.on_back(S.arg);
    snprintf(cp, sizeof cp, "%s/settings.sav", SAVE_DIR);
    write_raw(cp, "\x09\x07", 2);
    const rset_values bad = rset_load();
    CHECK(bad.speed == RSET_SPEED_NORMAL && !bad.custom, "an unreadable settings file gives the defaults");
    remove(cp);

    /* an unusable custom file: the list says so, no custom run starts, ordinary runs are not affected */
    snprintf(cp, sizeof cp, "%s/custom.sav", SAVE_DIR);
    write_raw(cp, junk, sizeof junk);
    snprintf(p2, sizeof p2, "%s/custom.sav.bak", SAVE_DIR);
    remove(p2);
    cs_reset();
    app_start(&plain);
    const int start1 = n_start;
    rset_pick_custom = true;
    T.on_choose(TITLE_NEW_GAME, T.arg);
    CHECK(S.mode == SLOTS_NEW && last_msg[0], "the list says the custom file cannot be used: \"%s\"", last_msg);
    last_msg[0] = 0;
    S.on_pick(2, S.arg);
    CHECK(n_start == start1 && last_msg[0], "and no custom run starts: \"%s\"", last_msg);
    CHECK(file_size(cp) == sizeof junk, "the unusable file is untouched");
    S.on_back(S.arg);
    rset_pick_custom = false;
    T.on_choose(TITLE_NEW_GAME, T.arg);
    S.on_pick(2, S.arg);
    sh_confirm(1);
    CHECK(!CFG.custom && n_start == start1 + 1, "an ordinary run still starts");
    driver_end(GC_SCREEN_GAME_OVER);
    R.on_done(R.arg);
    remove(cp);
    cs_reset();
  }

  /* the capsule preview is debug only: it opens before the title when the debug file asks for it, and leaving it is the title */
  {
    app_debug_capsule(7);
    const int caps = n_caps, titles = n_title;
    app_start(&(rs_config){0});
    CHECK(n_caps == caps + 1 && last_caps_n == 7 && n_title == titles && CP.on_exit, "capsule=7 opens the preview with 7 results, not the title");
    CP.on_exit(CAPS_EXIT_AFTER_REVEAL, CP.arg);
    CHECK(n_title == titles + 1, "leaving the preview is the title");
    app_debug_capsule(26);
    app_start(&(rs_config){0});
    CHECK(n_caps == caps + 1 && n_title == titles + 2, "26 results are ignored: the normal title");
    app_debug_capsule(0);
    app_start(&(rs_config){0});
    CHECK(n_caps == caps + 1 && n_title == titles + 3, "no capsule line: the normal title");
    app_debug_capsule(-3);
    app_start(&(rs_config){0});
    CHECK(n_caps == caps + 1 && n_title == titles + 4, "a negative count is ignored");
  }

  /* the title's Capsule Machine entry opens the machines, and leaving them is the title (the app has no other way in) */
  {
    const int g = n_gacha, titles = n_title;
    T.on_choose(TITLE_CAPSULE, T.arg);
    CHECK(n_gacha == g + 1 && GS.on_exit && n_title == titles, "the title's Capsule Machine entry opens the machines");
    GS.on_exit(GS.arg);
    CHECK(n_title == titles + 1, "leaving the machines is the title");
  }

  /* the app boots the profile transactions: a pull a crash left committed but not on the card is finished at app_start (profile_tx.h) */
  {
    remove("sdmc:/3ds/pokerogue/save/system.sav");
    remove("sdmc:/3ds/pokerogue/save/system.sav.bak");
    sysd_reset();
    ptx_reset();
    gc_system* sys = sysd_get();
    gc_system_init_defaults(sys);
    sys->voucher_counts[GC_VOUCHER_REGULAR] = 10;
    CHECK(sysd_save(), "a profile with vouchers");
    const sysd_io fio = {save_read, fail_system_write};
    sysd_set_io(&fio);
    ptx_set_scene_seed("flow");
    ptx_pull_request rq;
    memset(&rq, 0, sizeof rq);
    rq.option = 0;
    rq.timestamp_ms = 1791504000000ull;
    rq.seed_len = 4;
    memcpy(rq.seed, "flow", 4);
    uint32_t txn = 0;
    ptx_result res;
    CHECK(ptx_begin_pull(&rq, &txn) == PTX_OK && ptx_execute(txn, &res) == PTX_X_DONE_APPLY_PENDING, "the pull commits, the profile write fails");
    CHECK(ptx_apply_pending() && res.n == 1, "committed, waiting to be applied");
    sysd_set_io(NULL);
    sysd_reset();
    ptx_reset();   /* the restart */
    app_start(&plain);
    CHECK(!ptx_apply_pending() && ptx_pending() == 0, "app_start finished it");
    CHECK(sysd_get()->n_eggs == res.n && sysd_get()->voucher_counts[GC_VOUCHER_REGULAR] < 10, "the pulled egg is in the profile and the voucher is spent");
    CHECK(ptx_result_of(txn, NULL), "and the result is kept");
  }

  printf("%d checks, %d failed\n", checks, failed);
  printf(failed ? "FAILED\n" : "OK\n");
  if (chdir("/") == 0) {
    char cmd[128];
    snprintf(cmd, sizeof cmd, "rm -rf %s", dir);
    if (system(cmd) != 0) printf("(could not remove %s)\n", dir);
  }
  return failed ? 1 : 0;
}
