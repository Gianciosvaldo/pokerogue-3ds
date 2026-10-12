/* store_test: the custom content metadata on the SD card (custom_store.c) over the real save.c, on the host (task C04).
 *
 *   build/store_test
 *
 * Works in a temporary folder (the SD card's "sdmc:/..." paths are relative here). Checks: a missing file, the durable run uid
 * (written before it is handed out, never reused after a restart), storing a run's report once however often it is asked, every
 * way the file can be unusable (damaged, truncated, another schema, malformed) leaving its bytes exactly as they were, the backup
 * fallback, a failed write at each boundary of save_write leaving the previous state on the card and in memory and a retry
 * succeeding, and the storage budgets. Exit 1 on any failed check. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "custom_store.h"
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

#define NONCE 0x5A5A0001u

static co_report g_report;
static cc_meta g_decoded, g_before;

/* ------------------------------------------------------------------ files */

static void path_of(const char* ext, char* out, size_t n) { snprintf(out, n, "%s/custom%s", SAVE_DIR, ext); }
static bool exists(const char* path) { return access(path, F_OK) == 0; }

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

/* a snapshot of the three files of the store, as bytes (missing = length -1), to compare "untouched" */
typedef struct {
  long len[3];
  uint8_t* bytes[3];
} files_t;
static const char* const EXT[3] = {".sav", ".sav.bak", ".sav.tmp"};

static files_t snap_files(void) {
  files_t f;
  char p[256];
  for (int i = 0; i < 3; i++) {
    path_of(EXT[i], p, sizeof p);
    size_t n = 0;
    f.bytes[i] = NULL;
    f.len[i] = read_file(p, &f.bytes[i], &n) ? (long)n : -1;
  }
  return f;
}
static bool same_files(const files_t* a, const files_t* b) {
  for (int i = 0; i < 3; i++) {
    if (a->len[i] != b->len[i]) return false;
    if (a->len[i] > 0 && memcmp(a->bytes[i], b->bytes[i], (size_t)a->len[i]) != 0) return false;
  }
  return true;
}
static void free_files(files_t* f) {
  for (int i = 0; i < 3; i++) free(f->bytes[i]);
}

static void remove_all(void) {
  char p[256];
  for (int i = 0; i < 3; i++) {
    path_of(EXT[i], p, sizeof p);
    remove(p);
  }
}

/* the decoded content of the main file now (what the card holds, read independently of custom_store.c) */
static bool card_meta(cc_meta* out) {
  char* buf = NULL;
  size_t len = 0;
  if (save_read(CS_FILE_NAME, &buf, &len, NULL) != SAVE_OK) return false;
  const cc_status s = cc_meta_decode((const uint8_t*)buf, len, out);
  free(buf);
  return s == CC_OK;
}

/* writes arbitrary bytes as the main file through the real save transport (CRC valid) */
static void put_blob(const void* data, size_t n) {
  remove_all();
  if (save_write(CS_FILE_NAME, data, n) != SAVE_OK) {
    printf("could not write the test file\n");
    exit(1);
  }
}

/* ------------------------------------------------------------------ reports */

static void make_report(cc_run_uid uid, uint8_t outcome, uint16_t wave) {
  memset(&g_report, 0, sizeof g_report);
  g_report.valid = true;
  g_report.run_uid = uid;
  g_report.uid_durable = true;
  g_report.outcome = outcome;
  g_report.biome = 3;
  g_report.wave = wave;
  g_report.history.n_turns = 1;
  cc_turn* t = &g_report.history.turns[0];
  t->battle_seq = 1;
  t->wave = wave;
  t->turn = 2;
  t->n_events = 2;
  for (int i = 0; i < 2; i++) {
    t->events[i].seq = (uint32_t)(1 + i);
    t->events[i].kind = CC_EV_DAMAGE;
    t->events[i].target_side = CC_SIDE_PLAYER;
    t->events[i].target_species = 25;
    t->events[i].known = CC_EK_SHOWN;
    t->events[i].shown = 7 + i;
  }
}

static void fresh_card(void) {
  remove_all();
  cs_set_io(NULL);
  cs_reset();
  cs_set_nonce(NONCE);
}

/* a restart: the process forgets everything it read */
static void reboot(void) {
  cs_reset();
  cs_set_nonce(NONCE);
}

#define UID(seq) (((uint64_t)NONCE << 32) | (seq))

/* ------------------------------------------------------------------ tests */

static void test_missing_file(void) {
  fresh_card();
  char why[CS_WHY_LEN];
  CHECK(cs_get_state(why) == CS_NEW && !why[0], "a missing file is a new install: %d \"%s\"", cs_get_state(NULL), why);
  CHECK(cs_meta() && cs_meta()->roster.count == 0 && !cs_meta()->has_report && cs_meta()->counters.next_run_seq == 1, "the empty state");
  CHECK(cs_meta()->counters.install_nonce == NONCE, "its nonce is the one given");
  char p[256];
  path_of(".sav", p, sizeof p);
  CHECK(!exists(p), "looking at it writes nothing");
  CHECK(!cs_from_backup(), "no backup");
}

static void test_run_uids(void) {
  fresh_card();
  char p[256];
  const cc_run_uid u1 = cs_begin_run();
  CHECK(u1 == UID(1), "the first uid is the nonce and 1: %llx", (unsigned long long)u1);
  path_of(".sav", p, sizeof p);
  CHECK(exists(p), "the first run created the file");
  CHECK(card_meta(&g_decoded) && g_decoded.counters.next_run_seq == 2 && g_decoded.counters.install_nonce == NONCE,
        "the counter was on the card before the uid was returned");
  CHECK(cs_get_state(NULL) == CS_LOADED, "written: loaded");
  CHECK(cs_begin_run() == UID(2) && cs_begin_run() == UID(3), "uids count up");
  CHECK(card_meta(&g_decoded) && g_decoded.counters.next_run_seq == 4, "each one was written before it was handed out");
  /* a restart cannot hand one out twice */
  reboot();
  CHECK(cs_get_state(NULL) == CS_LOADED && cs_begin_run() == UID(4), "after a restart the counter continues");
  reboot();
  cs_set_nonce(0x7777u);                   /* a different nonce is ignored for a file that exists */
  CHECK(cs_begin_run() == UID(5), "an existing file keeps its own nonce");
  /* distinct and increasing over many starts */
  cc_run_uid last = UID(5);
  bool ok = true;
  for (int i = 0; i < 40; i++) {
    const cc_run_uid u = cs_begin_run();
    ok = ok && u == last + 1;
    last = u;
  }
  CHECK(ok && last == UID(45), "40 more uids in sequence");
  CHECK(card_meta(&g_decoded) && g_decoded.counters.next_run_seq == 46, "and on the card");
  /* a nonce of 0 is turned into 1: no uid is ever 0 or collides with a session counter at wave of small numbers */
  fresh_card();
  cs_set_nonce(0);
  CHECK(cs_begin_run() == (((uint64_t)1 << 32) | 1), "nonce 0 becomes 1");
}

static void test_commit_once(void) {
  fresh_card();
  const cc_run_uid u = cs_begin_run();
  make_report(u, CC_OUT_DEFEAT, 5);
  char p[256];
  cs_result r = cs_commit(&g_report);
  CHECK(r.outcome == CS_STORED && r.status == CC_OK && !r.commit.replayed && r.commit.outcome == CC_TXO_REPORT_ONLY && r.commit.record_id == 0,
        "stored: %d", r.outcome);
  CHECK(r.commit.refusal == CC_REFUSE_NOT_ATTRIBUTED, "no enemy was identified, so no record: %s", cc_refusal_str(r.commit.refusal));
  CHECK(card_meta(&g_decoded), "the card decodes");
  CHECK(g_decoded.has_report && g_decoded.report.run_uid == u && g_decoded.report.txn_id == 1 && g_decoded.report.outcome == CC_OUT_DEFEAT &&
            g_decoded.report.wave == 5 && g_decoded.report.biome == 3 && g_decoded.report.known == 0,
        "the report is on the card, its cause unknown");
  CHECK(g_decoded.history.n_turns == 1 && g_decoded.history.turns[0].n_events == 2 && g_decoded.history.turns[0].events[1].shown == 8, "with its last turns");
  CHECK(g_decoded.ledger.n == 1 && g_decoded.ledger.e[0].run_uid == u && g_decoded.ledger.e[0].outcome == CC_TXO_REPORT_ONLY, "and one ledger entry");
  CHECK(g_decoded.roster.count == 0 && g_decoded.counters.next_record_id == 1, "no Nemesis, no record id spent");
  CHECK(cs_meta()->report.run_uid == u, "memory agrees with the card");

  /* the same end again and again (callback retried, result screen reopened): the card is not even touched */
  files_t before = snap_files();
  for (int i = 0; i < 3; i++) {
    r = cs_commit(&g_report);
    CHECK(r.outcome == CS_REPLAYED && r.commit.replayed, "replayed %d", r.outcome);
  }
  files_t after = snap_files();
  CHECK(same_files(&before, &after), "the files are byte for byte what they were");
  free_files(&before);
  free_files(&after);
  /* ... and after a restart */
  reboot();
  r = cs_commit(&g_report);
  CHECK(r.outcome == CS_REPLAYED, "a restart cannot store it twice: %d", r.outcome);
  CHECK(card_meta(&g_decoded) && g_decoded.ledger.n == 1 && g_decoded.counters.next_txn_id == 2, "one transaction in all");
  path_of(".sav.tmp", p, sizeof p);
  CHECK(!exists(p), "no temporary file is left behind");

  /* the next run replaces the report; the ledger grows */
  const cc_run_uid u2 = cs_begin_run();
  make_report(u2, CC_OUT_CLEAR, 200);
  g_report.outcome = CC_OUT_CLEAR;
  r = cs_commit(&g_report);
  CHECK(r.outcome == CS_STORED, "second report: %d", r.outcome);
  CHECK(card_meta(&g_decoded) && g_decoded.report.run_uid == u2 && g_decoded.report.outcome == CC_OUT_CLEAR && g_decoded.report.wave == 200 &&
            g_decoded.report.known == 0 && g_decoded.ledger.n == 2 && g_decoded.report.txn_id == 2,
        "a clear is a report too, and the most recent one is kept");
  /* eight more runs: the ledger keeps the last 8 */
  for (int i = 0; i < 8; i++) {
    const cc_run_uid x = cs_begin_run();
    make_report(x, CC_OUT_DEFEAT, (uint16_t)(10 + i));
    CHECK(cs_commit(&g_report).outcome == CS_STORED, "run %d", i);
  }
  CHECK(card_meta(&g_decoded) && g_decoded.ledger.n == 8 && g_decoded.counters.next_txn_id == 11, "the ledger is bounded at 8, the counters go on");
}

static void test_bad_reports(void) {
  fresh_card();
  const cc_run_uid u = cs_begin_run();
  files_t before = snap_files();
  cs_result r;

  make_report(u, CC_OUT_DEFEAT, 5);
  g_report.uid_durable = false;
  r = cs_commit(&g_report);
  CHECK(r.outcome == CS_NO_DURABLE_RUN, "a session uid is never stored: %d", r.outcome);
  g_report.uid_durable = true;
  g_report.valid = false;
  r = cs_commit(&g_report);
  CHECK(r.outcome == CS_BAD_REPORT && r.status == CC_E_NULL, "an invalid report: %d", r.outcome);
  CHECK(cs_commit(NULL).outcome == CS_BAD_REPORT, "no report");
  make_report(u, CC_OUT_DEFEAT, 0);
  CHECK(cs_commit(&g_report).outcome == CS_BAD_REPORT, "wave 0");
  make_report(u, CC_OUT_DEFEAT, 5);
  g_report.biome = 51;
  CHECK(cs_commit(&g_report).outcome == CS_BAD_REPORT, "biome over 50");
  g_report.biome = -1;
  CHECK(cs_commit(&g_report).outcome == CS_BAD_REPORT, "biome -1");
  make_report(u, CC_OUT_NONE, 5);
  CHECK(cs_commit(&g_report).outcome == CS_BAD_REPORT, "no outcome");
  make_report(u, CC_OUT_ABANDON, 5);
  CHECK(cs_commit(&g_report).outcome == CS_BAD_REPORT, "an abandoned run has no such flow in the app: refused");
  make_report(u + 10, CC_OUT_DEFEAT, 5);
  r = cs_commit(&g_report);
  CHECK(r.outcome == CS_BAD_REPORT && r.status == CC_E_CONSISTENCY, "a uid this install never issued: %d %d", r.outcome, r.status);
  make_report(u, CC_OUT_DEFEAT, 5);
  g_report.history.turns[0].events[1].seq = g_report.history.turns[0].events[0].seq;
  r = cs_commit(&g_report);
  CHECK(r.outcome == CS_BAD_REPORT && r.status == CC_E_DUPLICATE, "a history that breaks its own rules: %d %d", r.outcome, r.status);
  files_t after = snap_files();
  CHECK(same_files(&before, &after), "none of them touched the card");
  free_files(&before);
  free_files(&after);
  CHECK(!cs_meta()->has_report, "nor the memory");
}

/* every way the file can be unusable: the bytes stay as they were, nothing is created, nothing is written */
static void expect_preserved(const char* what, const char* word) {
  files_t before = snap_files();
  reboot();
  char why[CS_WHY_LEN];
  CHECK(cs_get_state(why) == CS_UNREADABLE, "%s: unreadable (%d)", what, cs_get_state(NULL));
  CHECK(strstr(why, word) && strstr(why, "left as it is"), "%s: the player is told: \"%s\"", what, why);
  CHECK(cs_meta() == NULL, "%s: no state to use", what);
  CHECK(cs_begin_run() == 0, "%s: no uid", what);
  make_report(UID(1), CC_OUT_DEFEAT, 5);
  CHECK(cs_commit(&g_report).outcome == CS_FILE_UNUSABLE, "%s: nothing stored", what);
  g_report.uid_durable = false;
  CHECK(cs_commit(&g_report).outcome == CS_NO_DURABLE_RUN, "%s: a session uid says so first", what);
  files_t after = snap_files();
  CHECK(same_files(&before, &after), "%s: every file is byte for byte as it was", what);
  CHECK(after.len[2] == -1 || before.len[2] != -1, "%s: no temporary file appeared", what);
  free_files(&before);
  free_files(&after);
}

static void test_unusable_files(void) {
  fresh_card();
  CHECK(cs_begin_run() != 0, "a good file to start from");
  static uint8_t good[CC_WIRE_META_MAX];
  size_t n = 0;
  CHECK(cc_meta_encode(cs_meta(), good, sizeof good, &n) == CC_OK && n > 0, "encode");
  char p[256];

  /* garbage that is not even a save: it fails the transport's checksum */
  remove_all();
  path_of(".sav", p, sizeof p);
  FILE* f = fopen(p, "wb");
  fwrite("this is not a custom file at all, just text", 1, 43, f);
  fclose(f);
  expect_preserved("garbage", "damaged");

  /* a good file whose bytes were damaged: the checksum fails, the backup is absent */
  put_blob(good, n);
  {
    uint8_t* raw = NULL;
    size_t len = 0;
    path_of(".sav", p, sizeof p);
    CHECK(read_file(p, &raw, &len) && len > 40, "read the file");
    raw[len - 5] ^= 0x10;
    f = fopen(p, "wb");
    fwrite(raw, 1, len, f);
    fclose(f);
    free(raw);
  }
  expect_preserved("flipped byte", "damaged");

  /* a blob from another version (schema 2, 3, 0xFFFF): CRC-valid, so only the version check refuses it */
  static const uint16_t schemas[] = {0, 2, 3, 0xFFFF};
  for (unsigned i = 0; i < sizeof schemas / sizeof schemas[0]; i++) {
    static uint8_t blob[CC_WIRE_META_MAX];
    memcpy(blob, good, n);
    blob[4] = (uint8_t)schemas[i];
    blob[5] = (uint8_t)(schemas[i] >> 8);
    put_blob(blob, n);
    expect_preserved("another schema", "different version");
  }

  /* CRC-valid but not a blob: wrong magic, truncated, trailing byte, a count over its limit, a malformed record, an empty file */
  {
    static uint8_t blob[CC_WIRE_META_MAX + 8];
    memcpy(blob, good, n);
    blob[0] ^= 1;
    put_blob(blob, n);
    expect_preserved("wrong magic", "damaged");
    put_blob(good, n - 1);
    expect_preserved("truncated by a byte", "damaged");
    put_blob(good, 20);
    expect_preserved("truncated to 20 bytes", "damaged");
    put_blob(good, 0);
    expect_preserved("empty", "damaged");
    memcpy(blob, good, n);
    blob[n] = 0;
    put_blob(blob, n + 1);
    expect_preserved("trailing byte", "damaged");
    memcpy(blob, good, n);
    blob[32] = 200;                         /* n_ledger */
    put_blob(blob, n);
    expect_preserved("ledger count over its limit", "damaged");
    memcpy(blob, good, n);
    blob[12] = 0; blob[13] = 0; blob[14] = 0; blob[15] = 0;   /* next_record_id 0 */
    put_blob(blob, n);
    expect_preserved("counter 0", "damaged");
    static uint8_t huge[CC_WIRE_META_MAX + 4096];
    memset(huge, 0xAB, sizeof huge);
    put_blob(huge, sizeof huge);
    expect_preserved("larger than any blob", "damaged");
  }

  /* a damaged main file WITH a backup that is also damaged: both stay as they are */
  fresh_card();
  CHECK(cs_begin_run() == UID(1) && cs_begin_run() == UID(2), "two writes, so a backup exists");
  for (int which = 0; which < 2; which++) {
    uint8_t* raw = NULL;
    size_t len = 0;
    path_of(EXT[which], p, sizeof p);
    CHECK(read_file(p, &raw, &len) && len > 40, "read %s", EXT[which]);
    raw[len - 3] ^= 0x01;
    f = fopen(p, "wb");
    fwrite(raw, 1, len, f);
    fclose(f);
    free(raw);
  }
  expect_preserved("main and backup damaged", "damaged");
}

static void test_backup_fallback(void) {
  fresh_card();
  const cc_run_uid u1 = cs_begin_run();          /* write 1 */
  const cc_run_uid u2 = cs_begin_run();          /* write 2: write 1 is now the backup */
  CHECK(u1 == UID(1) && u2 == UID(2), "two writes");
  char p[256];
  path_of(".sav.bak", p, sizeof p);
  CHECK(exists(p), "the first write is the backup");
  /* the main file is damaged */
  path_of(".sav", p, sizeof p);
  uint8_t* raw = NULL;
  size_t len = 0;
  CHECK(read_file(p, &raw, &len), "read main");
  raw[len / 2] ^= 0xFF;
  FILE* f = fopen(p, "wb");
  fwrite(raw, 1, len, f);
  fclose(f);
  free(raw);
  reboot();
  CHECK(cs_get_state(NULL) == CS_LOADED && cs_from_backup(), "the backup is used");
  CHECK(cs_meta()->counters.next_run_seq == 2, "it is the older state (one write back): the counter says 2");
  /* the next run replaces the damaged main file (what save_write always does with a file that fails its checksum), the backup stays */
  CHECK(cs_begin_run() == UID(2), "the uid the lost write had handed out is handed out again: it was never used");
  CHECK(!cs_from_backup() && card_meta(&g_decoded) && g_decoded.counters.next_run_seq == 3, "and the card is whole again");
}

/* ---- failed writes ---- */

static int g_fail_step;          /* 0 = the real write; 1..4 = stop at that boundary of save_write */
static int g_writes;

/* The boundaries of save_write (save.c): 1 the temp file is written, 2 the old backup is removed, 3 main is renamed to the backup,
 * 4 the temp file is renamed to main. A failure at 1..3 leaves what a crash there leaves; 4 is "the rename failed": same as 3. The new
 * bytes are made by the real save_write under another name, so the temp file is a real one. */
static save_result stop_write(const char* name, const void* data, size_t len) {
  g_writes++;
  if (g_fail_step == 0) return save_write(name, data, len);
  char tmp[256], main_p[256], bak_p[256], stage[256];
  snprintf(tmp, sizeof tmp, "%s/%s.sav.tmp", SAVE_DIR, name);
  snprintf(main_p, sizeof main_p, "%s/%s.sav", SAVE_DIR, name);
  snprintf(bak_p, sizeof bak_p, "%s/%s.sav.bak", SAVE_DIR, name);
  snprintf(stage, sizeof stage, "%s/%s_stage.sav", SAVE_DIR, name);
  if (g_fail_step == 1 && len == 0) return SAVE_IO_ERROR;
  if (save_write("custom_stage", data, len) != SAVE_OK) return SAVE_IO_ERROR;
  rename(stage, tmp);
  if (g_fail_step >= 2) remove(bak_p);
  if (g_fail_step >= 3 && exists(main_p)) rename(main_p, bak_p);
  return SAVE_IO_ERROR;
}

static const cs_io STOPPING = {save_read, stop_write};

static void test_failed_writes(void) {
  for (int step = 1; step <= 4; step++) {
    /* a begin-run write that fails: no uid, the old state stays, a retry works */
    fresh_card();
    CHECK(cs_begin_run() == UID(1), "step %d: a first state on the card", step);
    CHECK(cs_begin_run() == UID(2), "step %d: and a second, so there is a backup", step);
    CHECK(card_meta(&g_before), "read the card");
    cs_set_io(&STOPPING);
    g_fail_step = step;
    CHECK(cs_begin_run() == 0, "step %d: a failed write gives no uid", step);
    CHECK(cs_meta()->counters.next_run_seq == 3, "step %d: memory is as it was (next uid 3), not %u", step, cs_meta()->counters.next_run_seq);
    /* what a restart finds is the old state, from main or from the backup */
    reboot();
    cs_set_io(NULL);
    CHECK(cs_get_state(NULL) == CS_LOADED, "step %d: still readable", step);
    CHECK(cs_meta()->counters.next_run_seq == 3 && cs_meta()->counters.install_nonce == NONCE, "step %d: the old counters, %u", step, cs_meta()->counters.next_run_seq);
    CHECK(step < 3 ? !cs_from_backup() : cs_from_backup(), "step %d: read from %s", step, cs_from_backup() ? "the backup" : "main");
    /* a retry with the real write works and does not skip or reuse anything */
    CHECK(cs_begin_run() == UID(3), "step %d: the retry hands out the uid the failure never delivered", step);
    CHECK(card_meta(&g_decoded) && g_decoded.counters.next_run_seq == 4, "step %d: on the card now", step);

    /* a commit that fails: the old durable state and memory remain, the retry stores it once */
    make_report(UID(3), CC_OUT_DEFEAT, 9);
    cs_set_io(&STOPPING);
    g_fail_step = step;
    cs_result r = cs_commit(&g_report);
    CHECK(r.outcome == CS_WRITE_FAILED, "step %d: the commit says the write failed: %d", step, r.outcome);
    CHECK(!cs_meta()->has_report && cs_meta()->ledger.n == 0 && cs_meta()->counters.next_txn_id == 1, "step %d: memory holds no report, ledger or txn", step);
    reboot();
    cs_set_io(NULL);
    CHECK(cs_get_state(NULL) == CS_LOADED && !cs_meta()->has_report && cs_meta()->counters.next_run_seq == 4, "step %d: the card holds the old state", step);
    r = cs_commit(&g_report);
    CHECK(r.outcome == CS_STORED, "step %d: the retry stores it: %d", step, r.outcome);
    CHECK(card_meta(&g_decoded) && g_decoded.has_report && g_decoded.report.run_uid == UID(3) && g_decoded.ledger.n == 1 && g_decoded.report.txn_id == 1,
          "step %d: once", step);
    CHECK(cs_commit(&g_report).outcome == CS_REPLAYED, "step %d: and asking again changes nothing", step);
  }
  /* the very first write fails: nothing on the card, a new install still */
  for (int step = 1; step <= 3; step++) {
    fresh_card();
    cs_set_io(&STOPPING);
    g_fail_step = step;
    CHECK(cs_begin_run() == 0, "first write, step %d: no uid", step);
    CHECK(cs_get_state(NULL) == CS_NEW && cs_meta()->counters.next_run_seq == 1, "first write, step %d: still new in memory", step);
    reboot();
    cs_set_io(NULL);
    CHECK(cs_get_state(NULL) == CS_NEW, "first write, step %d: a restart finds nothing (a stranded temp file is never read)", step);
    CHECK(cs_begin_run() == UID(1), "first write, step %d: the retry", step);
  }
  /* a failure while no write is expected to be asked for: a replay does not write, so it cannot fail */
  fresh_card();
  const cc_run_uid u = cs_begin_run();
  make_report(u, CC_OUT_DEFEAT, 3);
  CHECK(cs_commit(&g_report).outcome == CS_STORED, "stored");
  cs_set_io(&STOPPING);
  g_fail_step = 1;
  g_writes = 0;
  CHECK(cs_commit(&g_report).outcome == CS_REPLAYED && g_writes == 0, "a replay never touches the card");
  cs_set_io(NULL);
}

/* ---- a read that fails ---- */
static save_result bad_read(const char* name, char** out, size_t* len, bool* used_backup) {
  (void)name; (void)out; (void)len;
  if (used_backup) *used_backup = false;
  return SAVE_IO_ERROR;
}
static save_result no_write(const char* name, const void* data, size_t len) {
  (void)name; (void)data; (void)len;
  g_writes++;
  return SAVE_OK;
}

static void test_read_error(void) {
  fresh_card();
  const cs_io io = {bad_read, no_write};
  cs_set_io(&io);
  g_writes = 0;
  char why[CS_WHY_LEN];
  CHECK(cs_get_state(why) == CS_UNREADABLE && strstr(why, "SD card") && strstr(why, "left as it is"), "an I/O error on read: \"%s\"", why);
  CHECK(cs_begin_run() == 0, "no uid");
  make_report(UID(1), CC_OUT_DEFEAT, 3);
  CHECK(cs_commit(&g_report).outcome == CS_FILE_UNUSABLE && g_writes == 0, "nothing written");
  cs_set_io(NULL);
}

static void test_notes_and_budget(void) {
  CHECK(cs_note(CS_STORED) == NULL && cs_note(CS_REPLAYED) == NULL, "nothing to say when it is stored");
  CHECK(cs_note(CS_WRITE_FAILED) && strstr(cs_note(CS_WRITE_FAILED), "SD card"), "a write that failed says so");
  CHECK(cs_note(CS_FILE_UNUSABLE) && cs_note(CS_NO_DURABLE_RUN) && cs_note(CS_BAD_REPORT), "every other outcome has a line");
  /* storage: the encoded file against 64 KiB, the runtime against 128 KiB */
  CHECK(CC_WIRE_META_MAX <= CC_META_BUDGET_BYTES, "the largest file is %u bytes", (unsigned)CC_WIRE_META_MAX);
  CHECK(cs_static_bytes() <= CC_RUNTIME_BUDGET_BYTES, "the store holds %zu bytes", cs_static_bytes());
  printf("custom_store: sizeof(cc_meta) = %zu, static storage %zu bytes (budget %u), largest file %u bytes (budget %u)\n", sizeof(cc_meta),
         cs_static_bytes(), (unsigned)CC_RUNTIME_BUDGET_BYTES, (unsigned)CC_WIRE_META_MAX, (unsigned)CC_META_BUDGET_BYTES);
  /* the busiest state the app can produce: every ledger slot used and a full 5 x 64 history */
  fresh_card();
  for (int i = 0; i < 9; i++) {
    const cc_run_uid u = cs_begin_run();
    make_report(u, CC_OUT_DEFEAT, (uint16_t)(3 + i));
    g_report.history.n_turns = CC_HISTORY_TURNS;
    for (int t = 0; t < CC_HISTORY_TURNS; t++) {
      cc_turn* turn = &g_report.history.turns[t];
      memset(turn, 0, sizeof *turn);
      turn->battle_seq = 1;
      turn->wave = (uint16_t)(3 + i);
      turn->turn = (uint16_t)(t + 1);
      turn->n_events = CC_MAX_EVENTS_PER_TURN;
      for (int e = 0; e < CC_MAX_EVENTS_PER_TURN; e++) {
        turn->events[e].seq = (uint32_t)(t * 64 + e + 1);
        turn->events[e].kind = CC_EV_DAMAGE;
        turn->events[e].target_side = CC_SIDE_PLAYER;
        turn->events[e].target_species = 25;
        turn->events[e].known = CC_EK_SHOWN;
        turn->events[e].shown = 5;
      }
    }
    CHECK(cs_commit(&g_report).outcome == CS_STORED, "full history %d", i);
  }
  char p[256];
  path_of(".sav", p, sizeof p);
  uint8_t* raw = NULL;
  size_t len = 0;
  CHECK(read_file(p, &raw, &len), "read");
  free(raw);
  printf("custom_store: a full report (5 turns x 64 events) and a full ledger: file %zu bytes including the 12-byte save header\n", len);
  CHECK(len <= CC_META_BUDGET_BYTES + 12, "the file fits the budget");
}

int main(void) {
  char dir[64] = "/tmp/store-test-XXXXXX";
  if (!mkdtemp(dir) || chdir(dir) != 0 || mkdir("sdmc:", 0777) != 0 || !save_init()) {
    printf("FAILED: no folder for the saves\n");
    return 1;
  }
  test_missing_file();
  test_run_uids();
  test_commit_once();
  test_bad_reports();
  test_unusable_files();
  test_backup_fallback();
  test_failed_writes();
  test_read_error();
  test_notes_and_budget();
  printf("%d checks, %d failed\n%s\n", checks, failed, failed ? "FAILED" : "OK");
  if (chdir("/") == 0) {
    char cmd[128];
    snprintf(cmd, sizeof cmd, "rm -rf %s", dir);
    if (system(cmd) != 0) printf("(could not remove %s)\n", dir);
  }
  return failed ? 1 : 0;
}
