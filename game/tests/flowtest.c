/* flowtest: the app's screen flow (app_flow.c) with fake screens and a fake run driver, over the real save slots (run_saves.c,
 * save.c) and real game-core runs, on the host.
 *
 *   build/flowtest
 *
 * The fakes record what the flow asks the screens for and answer as the player would (choose an entry, pick a slot, finish a
 * run). The fake run driver starts a real game-core run, saves it at once as the real driver does at the first save point,
 * and loads slots through the real rsv_load. Checks the flow: Continue, New Game with a slot (and the overwrite), Load Game,
 * game over and clear (the slot is cleared, back to the title), a slot that can't be loaded (it says why and is never
 * overwritten), Delete Run, and that the debug config (seed, starters) is used for the first New Game only. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "app_flow.h"
#include "pr_data.h"
#include "pr_enums.h"
#include "result_screen.h"
#include "run_saves.h"
#include "save.h"
#include "slot_screen.h"
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

static result_args R;
static int n_result;
void result_screen_open(const result_args* a) {
  R = *a;
  n_result++;
}

static uint32_t frames = 100;
uint32_t ui_frame(void) { return frames; }

static gc_run RUN;
static rs_config CFG;
static int n_start, n_resume;
static bool fail_start, slot_empty_at_start;

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

bool rs_start(const rs_config* c) {
  CFG = *c;
  n_start++;
  rsv_info in;
  slot_empty_at_start = rsv_inspect(c->slot, &in) == RSV_EMPTY;
  if (fail_start) return false;
  new_run(&RUN, c->seed);
  CHECK(c->save, "a new run must save");
  rsv_save(c->slot, &RUN); /* the first save point */
  return true;
}

rsv_status rs_resume(const rs_config* c, char why[RSV_WHY_LEN]) {
  CFG = *c;
  n_resume++;
  return rsv_load(c->slot, &RUN, why);
}

const gc_run* rs_run(void) { return &RUN; }

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

int main(void) {
  char dir[64] = "/tmp/flow-test-XXXXXX";
  if (!mkdtemp(dir) || chdir(dir) != 0 || mkdir("sdmc:", 0777) != 0 || !save_init()) {
    printf("FAILED: no folder for the saves\n");
    return 1;
  }
  char p[256], p2[256];
  rs_config first = first_cfg();

  /* ---- a fresh install: the title has no Continue; New Game asks for a slot; the run is saved in it */
  app_start(&first);
  CHECK(n_title == 1 && !T.can_continue && T.biome == -1, "fresh title: continue %d biome %d", T.can_continue, T.biome);
  T.on_choose(TITLE_NEW_GAME, T.arg);
  CHECK(n_slots == 1 && S.mode == SLOTS_NEW && S.cursor == 0, "New Game opens the slot list in SAVE mode");
  for (int i = 0; i < RSV_SLOTS; i++) CHECK(S_info[i].status == RSV_EMPTY, "slot %d empty", i);
  S.on_pick(1, S.arg);
  CHECK(n_start == 1 && CFG.slot == 1 && CFG.save && CFG.on_end, "the run starts in slot 1 with saving on");
  CHECK(CFG.seed && !strcmp(CFG.seed, "flowtestseed000000000001") && CFG.n_starters == 1 && CFG.starters[0] == 25, "the debug config is used");
  const uint32_t entropy1 = CFG.entropy;
  CHECK(status_of(1) == RSV_OK && rsv_last_slot() == 1, "the run is in slot 1, the last slot");

  /* the run ends in a game over: the slot is cleared and the result screen shows the run */
  CFG.on_end(GC_SCREEN_GAME_OVER, CFG.end_arg);
  CHECK(n_result == 1 && !R.victory && R.run.status == RSV_OK && R.run.wave == RUN.wave && R.run.n_party == 2, "game over screen");
  CHECK(status_of(1) == RSV_EMPTY && rsv_last_slot() == -1, "the slot of a finished run is cleared and forgotten");
  R.on_done(R.arg);
  CHECK(n_title == 2 && !T.can_continue, "back to the title, nothing to continue");

  /* ---- a second run: the debug config is not used again, and its randomness differs */
  frames += 357;
  T.on_choose(TITLE_NEW_GAME, T.arg);
  S.on_pick(0, S.arg);
  CHECK(n_start == 2 && CFG.slot == 0 && !CFG.seed && CFG.n_starters == 0, "the second New Game has no debug seed or starters");
  CHECK(CFG.entropy != entropy1, "each run has its own entropy");
  const int biome0 = RUN.biome;

  /* ---- quit and boot again: Continue loads the last slot; the title shows its biome */
  app_start(&first);
  CHECK(T.can_continue && T.biome == biome0, "Continue is offered, with the biome %d (%d)", T.biome, biome0);
  T.on_choose(TITLE_CONTINUE, T.arg);
  CHECK(n_resume == 1 && CFG.slot == 0 && CFG.save && CFG.on_end, "Continue loads slot 0 with saving on");
  CHECK(!CFG.seed && CFG.n_starters == 0, "a loaded run takes no debug config");

  /* ---- Load Game: the list in LOAD mode; picking a run loads it */
  T.on_choose(TITLE_LOAD_GAME, T.arg);
  CHECK(S.mode == SLOTS_LOAD && S_info[0].status == RSV_OK && S_info[1].status == RSV_EMPTY, "the load list shows slot 0");
  S.on_pick(0, S.arg);
  CHECK(n_resume == 2 && CFG.slot == 0, "Load Game loads the slot picked");
  S.on_back(S.arg);
  CHECK(T.can_continue, "back to the title");

  /* ---- New Game over a run: the old run goes (with its backup) before the new one starts */
  rsv_save(0, &RUN); /* a second save of slot 0: it now has a backup */
  slot_path(0, ".sav.bak", p, sizeof p);
  CHECK(exists(p), "slot 0 has a backup");
  T.on_choose(TITLE_NEW_GAME, T.arg);
  CHECK(S_info[0].status == RSV_OK, "the list shows the run in slot 0");
  S.on_pick(0, S.arg); /* the list asked "Overwrite?" and the answer was yes */
  CHECK(n_start == 3 && slot_empty_at_start, "the old run was removed before the new run started");
  slot_path(0, ".sav.bak", p, sizeof p);
  CHECK(!exists(p), "the new run has no backup of the old one");

  /* ---- clear: the same, with the clear flag */
  CFG.on_end(GC_SCREEN_CLEAR, CFG.end_arg);
  CHECK(n_result == 2 && R.victory && status_of(0) == RSV_EMPTY, "clear screen, slot cleared");
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
  app_start(&first);
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

  /* ---- a run that can't be set up: told, nothing started */
  fail_start = true;
  S.on_back(S.arg);
  T.on_choose(TITLE_NEW_GAME, T.arg);
  S.on_pick(4, S.arg);
  CHECK(last_msg[0] && status_of(4) == RSV_EMPTY, "told: \"%s\"", last_msg);

  printf("%d checks, %d failed\n", checks, failed);
  printf(failed ? "FAILED\n" : "OK\n");
  if (chdir("/") == 0) {
    char cmd[128];
    snprintf(cmd, sizeof cmd, "rm -rf %s", dir);
    if (system(cmd) != 0) printf("(could not remove %s)\n", dir);
  }
  return failed ? 1 : 0;
}
