/* saves_test: the run's save slots (run_saves.c) over real game-core runs and the real save.c, on the host.
 *
 *   build/saves_test
 *
 * Works in a temporary folder (the SD card's "sdmc:/..." paths are relative here). Checks: a save at a save point loads back as
 * the same run (the loaded run saves to the same bytes), the summary, the backup fallback, a file that can't be loaded is
 * INCOMPATIBLE and is never changed by looking at it or trying to load it, saving outside a save point and a failed write leave
 * the previous save as it was, clearing, and the last slot. Exit 1 on the first failed check. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "gc/run.h"
#include "pr_data.h"
#include "pr_enums.h"
#include "run_saves.h"
#include "save.h"

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

static gc_run A, B;
static uint8_t buf_a[GC_SAVE_MAX], buf_b[GC_SAVE_MAX];

/* a new run with a small team, played up to its first save point (the COMMAND screen of wave 1) */
static void new_run(gc_run* r, const char* seed, int n_team) {
  gc_run_new(r, GAME_MODES_CLASSIC, seed, 1234);
  gc_starter team[6];
  int n = 0;
  int budget = gc_starter_budget(GAME_MODES_CLASSIC);
  for (int i = 0; i < PR_SPECIES_COUNT && n < n_team; i++) {
    const pr_species* sp = &pr_species_table[i];
    const int cost = gc_starter_cost(sp->id);
    if (cost <= 0 || cost > budget) continue;
    gc_starter* s = &team[n++];
    memset(s, 0, sizeof *s);
    s->species = (uint16_t)sp->id;
    s->tera_type = -1;
    for (int k = 0; k < 6; k++) s->ivs[k] = 20;
    budget -= cost;
  }
  const char* why = NULL;
  if (!n || !gc_run_set_starters(r, team, n, &why)) {
    printf("could not set up a run: %s\n", why ? why : "?");
    exit(1);
  }
  gc_event e;
  while (gc_run_next_event(r, &e)) {
  }
}

static bool read_file(const char* path, uint8_t** out, size_t* len) {
  FILE* f = fopen(path, "rb");
  if (!f) return false;
  fseek(f, 0, SEEK_END);
  *len = (size_t)ftell(f);
  fseek(f, 0, SEEK_SET);
  *out = malloc(*len + 1);
  const bool ok = fread(*out, 1, *len, f) == *len;
  fclose(f);
  return ok;
}

static bool exists(const char* path) { return access(path, F_OK) == 0; }

static void slot_path(int slot, const char* ext, char* out, size_t n) { snprintf(out, n, "%s/session_%d%s", SAVE_DIR, slot, ext); }

/* the bytes of a slot's file now, to compare after something that must not change it */
static bool same_file(const char* path, const uint8_t* want, size_t want_len) {
  uint8_t* got;
  size_t len;
  if (!read_file(path, &got, &len)) return false;
  const bool same = len == want_len && !memcmp(got, want, len);
  free(got);
  return same;
}

static void write_raw(const char* path, const void* data, size_t n) {
  FILE* f = fopen(path, "wb");
  if (f) {
    fwrite(data, 1, n, f);
    fclose(f);
  }
}

int main(void) {
  char dir[64] = "/tmp/saves-test-XXXXXX";
  if (!mkdtemp(dir) || chdir(dir) != 0 || mkdir("sdmc:", 0777) != 0 || !save_init()) {
    printf("FAILED: no folder for the saves\n");
    return 1;
  }
  char p[256], p2[256];

  /* ---- nothing saved yet */
  rsv_info all[RSV_SLOTS];
  rsv_inspect_all(all);
  for (int i = 0; i < RSV_SLOTS; i++) CHECK(all[i].status == RSV_EMPTY, "slot %d is %d", i, all[i].status);
  CHECK(rsv_last_slot() == -1, "last slot %d", rsv_last_slot());
  CHECK(!rsv_slot_ok(-1) && !rsv_slot_ok(RSV_SLOTS) && rsv_slot_ok(0) && rsv_slot_ok(RSV_SLOTS - 1), "slot range");

  /* ---- a save at the save point */
  new_run(&A, "savestest0000000000000001", 3);
  CHECK(gc_run_screen(&A)->kind == GC_SCREEN_COMMAND && gc_run_can_save(&A), "the test run is not at a save point");
  CHECK(rsv_save(2, &A) == RSV_SAVED, "save");
  slot_path(2, ".sav", p, sizeof p);
  CHECK(exists(p), "session_2.sav is not there");
  CHECK(rsv_last_slot() == 2, "last slot %d", rsv_last_slot());
  rsv_info in;
  CHECK(rsv_inspect(2, &in) == RSV_OK && in.status == RSV_OK, "inspect: %s", in.why);
  CHECK(in.wave == A.wave && in.score == A.score && in.money == A.money && in.biome == A.biome && in.mode == A.run.mode, "summary fields");
  CHECK(in.n_party == A.n_party && in.n_party == 3, "party size %d", in.n_party);
  for (int i = 0; i < in.n_party; i++) {
    const gc_mon* m = &A.party[A.party_order[i]];
    CHECK(in.party[i].species == m->b.species && in.party[i].level == m->b.level && !in.party[i].fainted, "party %d", i);
  }
  CHECK(!in.from_backup, "not from the backup");
  rsv_inspect_all(all);
  for (int i = 0; i < RSV_SLOTS; i++) CHECK(all[i].status == (i == 2 ? RSV_OK : RSV_EMPTY), "slot %d is %d", i, all[i].status);

  /* ---- loading: the same run (it saves to the same bytes) */
  char why[RSV_WHY_LEN];
  CHECK(rsv_load(2, &B, why) == RSV_OK, "load: %s", why);
  const size_t na = gc_run_save(&A, buf_a, sizeof buf_a), nb = gc_run_save(&B, buf_b, sizeof buf_b);
  CHECK(na && na == nb && !memcmp(buf_a, buf_b, na), "the loaded run saves to %zu bytes, the original to %zu, same: %d", nb, na,
        na == nb && !memcmp(buf_a, buf_b, na));
  CHECK(rsv_load(4, &B, why) == RSV_EMPTY, "load of an empty slot");

  /* ---- a second save keeps the first as the backup; a damaged main file falls back to it */
  gc_choice c = {.kind = GC_CH_FIGHT, .a = 0, .b = -1};
  const char* cw = NULL;
  CHECK(gc_run_choose(&A, &c, &cw), "a turn: %s", cw ? cw : "?");
  gc_event e;
  while (gc_run_next_event(&A, &e)) {
  }
  CHECK(!gc_run_can_save(&A), "after a turn the run is past its save point");
  uint8_t* before;
  size_t before_len;
  CHECK(read_file(p, &before, &before_len), "read");
  CHECK(rsv_save(2, &A) == RSV_NOT_ALLOWED, "a save after the turn must be refused");
  CHECK(same_file(p, before, before_len), "a refused save changed the slot's file");
  slot_path(2, ".sav.bak", p2, sizeof p2);
  CHECK(!exists(p2), "no backup yet");

  new_run(&A, "savestest0000000000000002", 2);
  CHECK(rsv_save(2, &A) == RSV_SAVED, "second save");
  CHECK(exists(p2), "the previous save is kept as the backup");
  uint8_t* main_now;
  size_t main_len;
  CHECK(read_file(p, &main_now, &main_len), "read");
  CHECK(rsv_inspect(2, &in) == RSV_OK && in.n_party == 2 && !in.from_backup, "the new save is the one read");
  main_now[main_len / 2] ^= 0xFF; /* damage the main file */
  write_raw(p, main_now, main_len);
  CHECK(rsv_inspect(2, &in) == RSV_OK && in.from_backup && in.n_party == 3, "the damaged main file falls back to the backup (%d)", in.n_party);

  /* ---- a slot that can't be loaded: INCOMPATIBLE, and nothing changes it */
  uint8_t* bak_bytes;
  size_t bak_len;
  CHECK(read_file(p2, &bak_bytes, &bak_len), "read bak");
  bak_bytes[bak_len / 2] ^= 0xFF;
  write_raw(p2, bak_bytes, bak_len); /* both damaged */
  uint8_t* d1;
  uint8_t* d2;
  size_t l1, l2;
  CHECK(read_file(p, &d1, &l1) && read_file(p2, &d2, &l2), "read both");
  CHECK(rsv_inspect(2, &in) == RSV_INCOMPATIBLE && in.why[0], "both damaged: %d", in.status);
  printf("  damaged: \"%s\"\n", in.why);
  CHECK(rsv_load(2, &B, why) == RSV_INCOMPATIBLE && why[0], "load of a damaged slot");
  CHECK(same_file(p, d1, l1) && same_file(p2, d2, l2), "looking at a damaged slot changed its files");

  /* newer format: a valid run save with a format number from the future, wrapped by save_write like the game's own */
  new_run(&A, "savestest0000000000000003", 3);
  const size_t n = gc_run_save(&A, buf_a, sizeof buf_a);
  CHECK(n > 20, "save size");
  buf_a[8] = 99; /* "PR3DSRUN" u16 format */
  CHECK(save_write("session_3", buf_a, n) == SAVE_OK, "write the future save");
  slot_path(3, ".sav", p, sizeof p);
  uint8_t* f1;
  size_t fl;
  CHECK(read_file(p, &f1, &fl), "read");
  CHECK(rsv_inspect(3, &in) == RSV_INCOMPATIBLE, "newer format: %d", in.status);
  CHECK(strstr(in.why, "newer version"), "newer format message: %s", in.why);
  printf("  newer: \"%s\"\n", in.why);
  CHECK(rsv_load(3, &B, why) == RSV_INCOMPATIBLE && strstr(why, "newer version"), "load of a newer save");
  CHECK(same_file(p, f1, fl), "looking at a newer save changed it");

  /* a file that is not a run save at all */
  CHECK(save_write("session_4", "hello", 5) == SAVE_OK, "write");
  CHECK(rsv_inspect(4, &in) == RSV_INCOMPATIBLE && strstr(in.why, "Not a run save"), "not a run save: %s", in.why);

  /* ---- a failed write leaves the previous save as it was */
  new_run(&A, "savestest0000000000000004", 3);
  CHECK(rsv_save(1, &A) == RSV_SAVED, "save slot 1");
  slot_path(1, ".sav", p, sizeof p);
  uint8_t* s1;
  size_t sl;
  CHECK(read_file(p, &s1, &sl), "read");
  slot_path(1, ".sav.tmp", p2, sizeof p2);
  CHECK(mkdir(p2, 0777) == 0, "block the temp file with a folder");
  CHECK(rsv_save(1, &A) == RSV_IO_ERROR, "a write that can't happen is an I/O error");
  CHECK(same_file(p, s1, sl), "a failed write changed the previous save");
  CHECK(rsv_inspect(1, &in) == RSV_OK, "the previous save still loads");
  rmdir(p2);
  CHECK(rsv_save(RSV_SLOTS, &A) == RSV_BAD_SLOT && rsv_save(-1, &A) == RSV_BAD_SLOT, "bad slots");

  /* ---- clearing, and the last slot */
  rsv_set_last_slot(1);
  CHECK(rsv_last_slot() == 1, "set last slot");
  write_raw(p2, "x", 1); /* a leftover temp file */
  CHECK(rsv_clear(0), "clearing an empty slot is fine");
  CHECK(rsv_last_slot() == 1, "clearing another slot keeps the last slot");
  CHECK(rsv_clear(1), "clear slot 1");
  slot_path(1, ".sav", p, sizeof p);
  CHECK(!exists(p) && !exists(p2), "the files are gone");
  CHECK(rsv_last_slot() == -1, "the cleared slot is no longer the last one (%d)", rsv_last_slot());
  CHECK(rsv_inspect(1, &in) == RSV_EMPTY, "empty after clear");
  CHECK(rsv_save(1, &A) == RSV_SAVED, "the slot takes a new run");
  CHECK(rsv_last_slot() == 1, "and is the last one");
  slot_path(1, ".sav.bak", p2, sizeof p2);
  CHECK(!exists(p2), "a fresh run's slot has no old backup");

  /* The slot summary is reconstructed from the full save: retain wide levels. */
  const int wide_levels[] = {256, 65536, 1000000};
  for (int w = 0; w < 3; w++) {
    new_run(&A, "wide-summary", 1);
    A.party[A.party_order[0]].b.level = wide_levels[w];
    CHECK(rsv_save(0, &A) == RSV_SAVED, "wide summary save");
    CHECK(rsv_inspect(0, &in) == RSV_OK && in.party[0].level == wide_levels[w], "wide summary %d got %d", wide_levels[w], in.party[0].level);
    CHECK(rsv_load(0, &B, why) == RSV_OK && B.party[B.party_order[0]].b.level == wide_levels[w], "wide summary load");
    CHECK(rsv_clear(0), "clear wide summary slot");
  }

  /* ---- Rename Run: a name beside the slot, kept for the run it was given to */
  {
    new_run(&A, "name-test-run-one", 2);
    CHECK(rsv_save(3, &A) == RSV_SAVED, "save for the name test");
    CHECK(rsv_inspect(3, &in) == RSV_OK && in.run_key != 0, "a run has a key");
    const uint64_t key = in.run_key;
    new_run(&B, "name-test-run-two", 2);
    rsv_info other;
    rsv_summarize(&B, &other);
    CHECK(other.run_key != key, "two seeds, two keys");
    new_run(&B, "name-test-run-one", 2);
    rsv_summarize(&B, &other);
    CHECK(other.run_key == key, "the same seed, the same key");

    char name[RSV_NAME_LEN];
    CHECK(!rsv_name_get("session_3", key, name) && name[0] == '\0', "no name before one is given");
    CHECK(rsv_name_set("session_3", key, "Jojo Run 1"), "give a name");
    CHECK(rsv_name_get("session_3", key, name) && !strcmp(name, "Jojo Run 1"), "the name is \"%s\"", name);
    CHECK(!rsv_name_get("session_3", key + 1, name) && name[0] == '\0', "another run's key does not get the name");
    CHECK(!rsv_name_get("session_4", key, name), "another slot has no name");
    /* naming does not touch the run's own save */
    CHECK(rsv_inspect(3, &in) == RSV_OK && in.run_key == key, "the slot still loads after a name");
    CHECK(rsv_name_set("session_3", key, "Second"), "rename");
    CHECK(rsv_name_get("session_3", key, name) && !strcmp(name, "Second"), "the new name is \"%s\"", name);
    /* what a name may be: 1..20 characters of valid UTF-8 with no control characters */
    const char* twenty = "12345678901234567890";
    const char* accents = "\xc3\xa0\xc3\xa8\xc3\xa9\xc3\xac\xc3\xb2\xc3\xb9\xc3\xa0\xc3\xa8\xc3\xa9\xc3\xac\xc3\xb2\xc3\xb9\xc3\xa0\xc3\xa8\xc3\xa9\xc3\xac\xc3\xb2\xc3\xb9\xc3\xa0\xc3\xa8"; /* 20 characters, 40 bytes */
    CHECK(rsv_utf8_chars(twenty) == 20 && rsv_utf8_chars(accents) == 20 && rsv_utf8_chars("") == 0 && rsv_utf8_chars("\xc3") == -1 &&
              rsv_utf8_chars("\xff") == -1 && rsv_utf8_chars("\xc3\x28") == -1 && rsv_utf8_chars("\xe2\x82") == -1,
          "character counts");
    CHECK(rsv_name_set("session_3", key, twenty) && rsv_name_get("session_3", key, name) && !strcmp(name, twenty), "20 characters fit");
    CHECK(rsv_name_set("session_3", key, accents) && rsv_name_get("session_3", key, name) && !strcmp(name, accents), "20 accented characters (40 bytes) fit");
    CHECK(!rsv_name_set("session_3", key, "123456789012345678901"), "21 characters do not");
    CHECK(!rsv_name_set("session_3", key, "") && !rsv_name_set("session_3", key, NULL), "an empty name is not a name");
    CHECK(!rsv_name_set("session_3", key, "a\nb") && !rsv_name_set("session_3", key, "a\tb") && !rsv_name_set("session_3", key, "a\x7f"), "control characters");
    CHECK(!rsv_name_set("session_3", key, "\xc3\x28") && !rsv_name_set("session_3", key, "\xff\xfe"), "invalid UTF-8");
    CHECK(rsv_name_get("session_3", key, name) && !strcmp(name, accents), "refused names changed nothing");
    /* the card refusing the write leaves the name as it was */
    char np[256];
    snprintf(np, sizeof np, "%s/session_3_name.sav.tmp", SAVE_DIR);
    CHECK(mkdir(np, 0777) == 0, "block the name's temp file");
    CHECK(!rsv_name_set("session_3", key, "Blocked"), "a refused write is reported");
    CHECK(rsv_name_get("session_3", key, name) && !strcmp(name, accents), "and the name is as it was");
    rmdir(np);
    /* a damaged file, one made by something else, and one for a longer text than a name can be: no name, no crash */
    snprintf(np, sizeof np, "%s/session_3_name.sav", SAVE_DIR);
    write_raw(np, "not a save file", 15);
    CHECK(rsv_name_get("session_3", key, name) && !strcmp(name, twenty), "a damaged file reads the previous name (the backup): \"%s\"", name);
    snprintf(np, sizeof np, "%s/session_3_name.sav.bak", SAVE_DIR);
    remove(np);
    CHECK(!rsv_name_get("session_3", key, name) && name[0] == '\0', "a file that fails its checksum, with no backup, gives no name");
    CHECK(rsv_name_set("session_3", key, "Back"), "a good write over the damaged file");
    CHECK(rsv_name_get("session_3", key, name) && !strcmp(name, "Back"), "reads again");
    CHECK(save_write("session_3_name", "XYZ12345678abc", 14) == SAVE_OK && !rsv_name_get("session_3", key, name), "a wrong magic gives no name");
    uint8_t forged[11 + 90];
    memcpy(forged, "RN1", 3);
    for (int i = 0; i < 8; i++) forged[3 + i] = (uint8_t)(key >> (8 * i));
    memset(forged + 11, 'a', 90);
    CHECK(save_write("session_3_name", forged, sizeof forged) == SAVE_OK && !rsv_name_get("session_3", key, name) && name[0] == '\0',
          "a longer text than a name can be gives no name");
    memcpy(forged + 11, "a\x01", 2);
    CHECK(save_write("session_3_name", forged, 13) == SAVE_OK && !rsv_name_get("session_3", key, name), "a stored control character gives no name");
    memcpy(forged + 11, "ok", 2);
    CHECK(save_write("session_3_name", forged, 13) == SAVE_OK && rsv_name_get("session_3", key, name) && !strcmp(name, "ok"), "the forged file is otherwise a name");
    /* a name goes with its slot */
    CHECK(rsv_name_set("session_3", key, "Gone"), "name again");
    CHECK(rsv_clear(3), "clear the named slot");
    snprintf(np, sizeof np, "%s/session_3_name.sav", SAVE_DIR);
    CHECK(!exists(np), "the name's file went with the slot");
    snprintf(np, sizeof np, "%s/session_3_name.sav.bak", SAVE_DIR);
    CHECK(!exists(np), "and its backup");
    CHECK(!rsv_name_get("session_3", key, name), "no name for the cleared slot");
    /* a new run in the slot does not inherit a name that was left behind */
    CHECK(rsv_name_set("session_3", key, "Left behind"), "name a slot that is empty");
    new_run(&A, "name-test-run-two", 2);
    CHECK(rsv_save(3, &A) == RSV_SAVED && rsv_inspect(3, &in) == RSV_OK, "another run takes the slot");
    CHECK(!rsv_name_get("session_3", in.run_key, name) && name[0] == '\0', "it does not show the old run's name");
    CHECK(rsv_name_remove("session_3") && rsv_name_remove("session_3"), "removing a name is fine whether it is there or not");
    CHECK(rsv_clear(3), "clear");
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
