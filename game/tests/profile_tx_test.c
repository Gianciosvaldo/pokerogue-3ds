/* profile_tx_test: the profile transaction owner (profile_tx.c, with system_data.c and custom_store.c under it) over game-core's real
 * gc_system_gacha_pull and the real save.c, on the host (task C08).
 *
 *   build/profile_tx_test [fuzz scenarios]
 *
 * Works in a temporary folder (the SD card's "sdmc:/..." paths are relative here). One fault-injecting I/O layer sits under all three
 * modules and counts every write of every file (system.sav, custom.sav, ptx.sav): a write can FAIL (nothing written, the program goes on:
 * a full SD card, an I/O error) or the power can be CUT before the n-th write (that write and every later one never happen, the program
 * is rebuilt from the files: "reboot"). For a pull and for a claim, every n from the first write to one past the last is tried, in both
 * ways, and after each the same invariants must hold:
 *   - the profile on the card is exactly the profile before or exactly the one after: never half, never twice;
 *   - the profile after exists (and the result is on record) exactly when the transaction committed;
 *   - a retry (or the boot) ends in the one committed result, equal to a clean run's, with no second pull and no second voucher;
 *   - nothing is debited, granted or advanced in memory by a transaction whose commit write failed.
 * Plus: duplicates, a refusal, the damaged / future / backup-restored ptx file, the roll-back of the ptx file to an older write (the
 * request is refused, never run twice), the gate on sysd_save, an unserializable profile, the voucher cap, and a seeded fuzz.
 * Exit 1 on any failed check. */
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "cc/codec.h"
#include "cc/model.h"
#include "custom_store.h"
#include "gc/gacha.h"
#include "profile_tx.h"
#include "save.h"
#include "system_data.h"

static int checks, failed;
#define CHECK(cond, ...)                              \
  do {                                                \
    checks++;                                         \
    if (!(cond)) {                                    \
      failed++;                                       \
      if (failed < 40) {                              \
        printf("FAIL line %d: %s: ", __LINE__, #cond); \
        printf(__VA_ARGS__);                          \
        printf("\n");                                 \
      }                                               \
    }                                                 \
  } while (0)

#define SEED "scene-seed-1"

/* ------------------------------------------------------------------ the fault-injecting card */

static int g_w;            /* writes attempted since it was reset */
static int g_fail_at;      /* that write fails, nothing written (0: none) */
static int g_cut_before;   /* the power is cut before that write: it and every later I/O never happen (0: none) */
static bool g_dead;

static save_result f_read(const char* name, char** out, size_t* len, bool* used_backup) {
  if (g_dead) return SAVE_IO_ERROR;
  return save_read(name, out, len, used_backup);
}
static save_result f_write(const char* name, const void* data, size_t len) {
  g_w++;
  if (g_dead) return SAVE_IO_ERROR;
  if (g_cut_before && g_w == g_cut_before) {
    g_dead = true;
    return SAVE_IO_ERROR;
  }
  if (g_fail_at && g_w == g_fail_at) return SAVE_IO_ERROR;
  return save_write(name, data, len);
}
static void faults(int fail_at, int cut_before) {
  g_w = 0;
  g_fail_at = fail_at;
  g_cut_before = cut_before;
  g_dead = false;
}
static void no_faults(void) { faults(0, 0); }

/* ------------------------------------------------------------------ files */

typedef struct {
  char name[256];
  uint8_t* bytes;
  size_t len;
} snap_file;
typedef struct {
  snap_file f[24];
  int n;
} snap;

static snap take(void) {
  snap s;
  memset(&s, 0, sizeof s);
  DIR* d = opendir(SAVE_DIR);
  struct dirent* e;
  while (d && (e = readdir(d)) && s.n < 24) {
    if (e->d_name[0] == '.') continue;
    snap_file* f = &s.f[s.n++];
    snprintf(f->name, sizeof f->name, "%s", e->d_name);
    char path[600];
    snprintf(path, sizeof path, "%s/%s", SAVE_DIR, e->d_name);
    FILE* fp = fopen(path, "rb");
    fseek(fp, 0, SEEK_END);
    f->len = (size_t)ftell(fp);
    fseek(fp, 0, SEEK_SET);
    f->bytes = malloc(f->len + 1);
    if (fread(f->bytes, 1, f->len, fp) != f->len) f->len = 0;
    fclose(fp);
  }
  if (d) closedir(d);
  return s;
}
static void drop(snap* s) {
  for (int i = 0; i < s->n; i++) free(s->f[i].bytes);
  s->n = 0;
}
static void wipe(void) {
  DIR* d = opendir(SAVE_DIR);
  struct dirent* e;
  while (d && (e = readdir(d))) {
    if (e->d_name[0] == '.') continue;
    char path[600];
    snprintf(path, sizeof path, "%s/%s", SAVE_DIR, e->d_name);
    remove(path);
  }
  if (d) closedir(d);
}
static void restore(const snap* s) {
  wipe();
  for (int i = 0; i < s->n; i++) {
    char path[600];
    snprintf(path, sizeof path, "%s/%s", SAVE_DIR, s->f[i].name);
    FILE* fp = fopen(path, "wb");
    fwrite(s->f[i].bytes, 1, s->f[i].len, fp);
    fclose(fp);
  }
}
static bool same_files(const snap* a, const snap* b) {
  if (a->n != b->n) return false;
  for (int i = 0; i < a->n; i++) {
    bool found = false;
    for (int j = 0; j < b->n; j++)
      if (!strcmp(a->f[i].name, b->f[j].name)) found = a->f[i].len == b->f[j].len && !memcmp(a->f[i].bytes, b->f[j].bytes, a->f[i].len);
    if (!found) return false;
  }
  return true;
}
static const snap_file* find_file(const snap* s, const char* name) {
  for (int i = 0; i < s->n; i++)
    if (!strcmp(s->f[i].name, name)) return &s->f[i];
  return NULL;
}

/* ------------------------------------------------------------------ the modules' state */

static void reboot(void) {
  no_faults();
  sysd_reset();
  cs_reset();
  ptx_reset();
  ptx_set_scene_seed(SEED);
  ptx_boot();
}

static uint8_t g_buf[1024 * 1024];
static gc_system g_card;   /* the card's profile, loaded aside */

static uint64_t live_digest(void) {
  const size_t n = sysd_serialize(g_buf, sizeof g_buf);
  return n ? sysd_digest(g_buf, n) : 0;
}
/* the digest of system.sav as the card holds it (0: no file) */
static uint64_t card_digest(void) {
  char* b = NULL;
  size_t n = 0;
  if (save_read("system", &b, &n, NULL) != SAVE_OK) return 0;
  const char* why;
  const bool ok = gc_system_load(&g_card, b, n, &why);
  free(b);
  if (!ok) return 1;
  const size_t m = gc_system_save(&g_card, g_buf, sizeof g_buf);
  return m ? sysd_digest(g_buf, m) : 2;
}

static void make_profile(unsigned vouchers) {
  sysd_reset();
  gc_system* s = sysd_get();
  gc_system_init_defaults(s);
  for (int i = 0; i < 4; i++) s->voucher_counts[i] = vouchers;
  s->dex[25].seen_attr = s->dex[25].caught_attr = 3;
  s->dex[25].caught_count = 2;
  s->starters[25].candy_count = 7;
  s->unlocks.endless_mode = true;
  s->stats.highest_wave = 77;
  s->egg_pity[1] = 4;
  gc_system_add_egg(s, 11, 10, 1, 25);
  gc_system_add_egg(s, 12, 5, 0, 1);
  CHECK(sysd_save(), "the first profile save");
}

/* a custom file with three records: 5 won (reward owed), 7 claimed, 9 only stored */
static cc_nemesis rec(cc_record_id id, uint8_t life) {
  cc_nemesis n;
  memset(&n, 0, sizeof n);
  n.record_id = id;
  n.life = life;
  n.known = CC_NK_ABILITY | CC_NK_PASSIVE | CC_NK_MOVES | CC_NK_IVS | CC_NK_NATURE | CC_NK_LOADOUT | CC_NK_FINISH_MOVE | CC_NK_FALLEN |
            CC_NK_FINISHER_VERIFIED;
  n.source_run_uid = (cc_run_uid)0x0000000700000002ull;
  n.source_pokemon_id = 1000 + id;
  n.source_wave = 60;
  n.source_biome = 11;
  n.species = 143;
  n.gender = -1;
  n.orig_level = 35;
  n.nature = (uint8_t)(id % 25);
  n.ability = 9;
  for (int i = 0; i < 6; i++) n.ivs[i] = (uint8_t)((id * 7 + i * 5) % 32);
  n.n_moves = 2;
  n.moves[0] = 33;
  n.moves[1] = 85;
  n.finish_move = 85;
  n.fallen_species = 25;
  if (life >= CC_LIFE_SCHEDULED && life != CC_LIFE_STORED) n.sched_run_uid = (cc_run_uid)0x0000000700000003ull;
  if (life >= CC_LIFE_CHALLENGED && life != CC_LIFE_STORED) n.challenge_wave = 77;
  if (life == CC_LIFE_REWARD_CLAIMED) n.reward_txn = 4;
  return n;
}
static cc_meta g_meta;
static void make_custom(void) {
  memset(&g_meta, 0, sizeof g_meta);
  cc_meta_init(&g_meta, 7);
  for (int i = 0; i < 12; i++) {
    cc_record_id r;
    cc_alloc_record_id(&g_meta.counters, &r);
  }
  for (int i = 0; i < 8; i++) {
    cc_txn_id t;
    cc_alloc_txn_id(&g_meta.counters, &t);
  }
  for (int i = 0; i < 12; i++) {
    cc_run_uid u;
    cc_alloc_run_uid(&g_meta.counters, &u);
  }
  g_meta.roster.rec[g_meta.roster.count++] = rec(5, CC_LIFE_DEFEATED);
  g_meta.roster.rec[g_meta.roster.count++] = rec(7, CC_LIFE_REWARD_CLAIMED);
  g_meta.roster.rec[g_meta.roster.count++] = rec(9, CC_LIFE_STORED);
  static uint8_t buf[CC_WIRE_META_MAX];
  size_t n = 0;
  CHECK(cc_meta_encode(&g_meta, buf, sizeof buf, &n) == CC_OK, "encode the custom file");
  CHECK(save_write("custom", buf, n) == SAVE_OK, "write the custom file");
}

static ptx_pull_request req(int machine, int option, const char* seed, uint64_t ts) {
  ptx_pull_request r;
  memset(&r, 0, sizeof r);
  r.machine = (uint8_t)machine;
  r.option = (uint8_t)option;
  r.timestamp_ms = ts;
  r.seed_len = (uint8_t)strlen(seed);
  memcpy(r.seed, seed, r.seed_len);
  return r;
}

static bool egg_eq(const gc_egg* a, const gc_egg* b) {
  return a->id == b->id && a->hatch_waves == b->hatch_waves && a->tier == b->tier && a->species == b->species &&
         a->source_type == b->source_type && a->variant_tier == b->variant_tier && a->egg_move_index == b->egg_move_index &&
         a->is_shiny == b->is_shiny && a->override_hidden_ability == b->override_hidden_ability && a->timestamp_ms == b->timestamp_ms;
}
static bool result_eq(const ptx_result* a, const ptx_result* b) {
  if (a->txn != b->txn || a->kind != b->kind || a->outcome != b->outcome || strcmp(a->key, b->key) || a->n != b->n ||
      a->record != b->record || a->vouchers != b->vouchers)
    return false;
  for (int i = 0; i < a->n; i++)
    if (a->order[i] != b->order[i] || !egg_eq(&a->eggs[i], &b->eggs[i])) return false;
  return true;
}

/* ------------------------------------------------------------------ a clean pull, for the golden values */

typedef struct {
  uint64_t pre, post, scene;
  ptx_result res;
  int writes;
  int eggs_before, eggs_after;
} golden;

static const ptx_pull_request REQ_A = {.machine = 0, .option = 2, .timestamp_ms = 1791504000000ull, .seed_len = 5, .seed = "e-one"};

static golden run_clean_pull(const snap* s0, const ptx_pull_request* r) {
  restore(s0);
  reboot();
  golden g;
  memset(&g, 0, sizeof g);
  g.pre = live_digest();
  g.eggs_before = sysd_get()->n_eggs;
  faults(0, 0);
  uint32_t txn = 0;
  CHECK(ptx_begin_pull(r, &txn) == PTX_OK && txn != 0, "clean begin");
  CHECK(ptx_execute(txn, &g.res) == PTX_X_DONE, "clean execute");
  g.writes = g_w;
  g.post = live_digest();
  g.scene = ptx_scene_digest();
  g.eggs_after = sysd_get()->n_eggs;
  CHECK(g.pre != g.post, "a pull changed the profile");
  return g;
}

/* ------------------------------------------------------------------ the basic path */

static gc_system g_model;   /* a separate profile the real pull is run on, to compare with */

static void test_pull_matches_the_core(const snap* s0) {
  const golden g = run_clean_pull(s0, &REQ_A);
  CHECK(g.writes == 3 + 0 || g.writes >= 3, "writes %d", g.writes);   /* begin, commit, then the roll-forward's files */
  /* the same pull, run directly on the profile as it was: same eggs, same order, same profile */
  restore(s0);
  reboot();
  uint8_t* pre = malloc(sizeof g_buf);
  const size_t n = sysd_serialize(pre, sizeof g_buf);
  const char* why;
  CHECK(gc_system_load(&g_model, pre, n, &why), "load the pre-image aside");
  free(pre);
  gc_gacha_ctx ctx;
  memset(&ctx, 0, sizeof ctx);
  ctx.machine = REQ_A.machine;
  ctx.cursor = REQ_A.option;
  ctx.timestamp_ms = REQ_A.timestamp_ms;
  pr_seedstr s;
  memset(&s, 0, sizeof s);
  for (int i = 0; i < REQ_A.seed_len; i++) s.u[i] = REQ_A.seed[i];
  s.len = REQ_A.seed_len;
  pr_rng_sow(&ctx.entropy, &s);
  memset(&s, 0, sizeof s);
  for (size_t i = 0; i < strlen(SEED); i++) s.u[i] = (uint16_t)SEED[i];
  s.len = strlen(SEED);
  pr_rng_sow(&ctx.scene_rng, &s);
  gc_gacha_result gr;
  memset(&gr, 0, sizeof gr);
  CHECK(gc_system_gacha_pull(&g_model, &ctx, &gr, &why), "the direct pull succeeds");
  const size_t m = gc_system_save(&g_model, g_buf, sizeof g_buf);
  CHECK(sysd_digest(g_buf, m) == g.post, "the transaction's profile is the real pull's profile");
  CHECK(gr.n == g.res.n, "same number of eggs (%d / %d)", gr.n, g.res.n);
  for (int i = 0; i < gr.n && i < g.res.n; i++) {
    CHECK(gr.order[i] == g.res.order[i], "same display order");
    CHECK(egg_eq(&g_model.eggs[g.eggs_before + i], &g.res.eggs[i]), "same egg %d", i);
  }
  CHECK(g.res.outcome == PTX_R_DONE && g.res.kind == PTX_KIND_PULL && g.res.n >= 1, "a pull result");
}

static void test_done_and_idempotent(const snap* s0) {
  const golden g = run_clean_pull(s0, &REQ_A);
  /* the card holds the profile, the ptx file holds the result, nothing is pending */
  CHECK(card_digest() == g.post, "the card holds the profile after the pull");
  CHECK(ptx_pending() == 0 && !ptx_apply_pending(), "nothing pending");
  ptx_result r;
  CHECK(ptx_result_of(g.res.txn, &r) && result_eq(&r, &g.res), "the result is on record");
  /* asking again: the same result, no second pull */
  const uint64_t d = live_digest(), sc = ptx_scene_digest();
  const int w = g_w;
  for (int i = 0; i < 3; i++) {
    ptx_result again;
    memset(&again, 0, sizeof again);
    CHECK(ptx_execute(g.res.txn, &again) == PTX_X_DONE && result_eq(&again, &g.res), "again: the committed result");
  }
  CHECK(live_digest() == d && ptx_scene_digest() == sc && g_w == w, "no second pull: profile, scene RNG and writes unchanged");
  /* and after a restart */
  reboot();
  ptx_result after;
  CHECK(ptx_result_of(g.res.txn, &after) && result_eq(&after, &g.res), "the result survives a restart");
  CHECK(ptx_execute(g.res.txn, &after) == PTX_X_DONE && live_digest() == g.post, "and asking after the restart pulls nothing");
  CHECK(ptx_scene_digest() == g.scene, "the scene RNG is the saved one");
  /* an unknown id */
  CHECK(ptx_execute(g.res.txn + 50, &after) == PTX_X_UNKNOWN, "an unknown transaction");
  /* a second pull is a new transaction with a new id and a new result */
  uint32_t t2 = 0;
  const ptx_pull_request r2 = req(0, 2, "e-two", 1791504000000ull);
  CHECK(ptx_begin_pull(&r2, &t2) == PTX_OK && t2 > g.res.txn, "a new transaction id");
  uint32_t t3 = 0;
  CHECK(ptx_begin_pull(&r2, &t3) == PTX_E_BUSY && t3 == 0, "no second begin while one is pending");
  CHECK(ptx_pending() == t2, "pending names it");
  ptx_result x2;
  CHECK(ptx_execute(t2, &x2) == PTX_X_DONE && x2.txn == t2 && live_digest() != g.post, "it runs");
  CHECK(ptx_scene_digest() != g.scene, "the scene RNG advanced");
}

static void test_refusal(const snap* s0) {
  /* not enough vouchers: a refusal changes nothing but the pending record, and is itself a kept result */
  restore(s0);
  make_profile(0);
  reboot();
  const uint64_t pre = live_digest();
  snap before = take();
  uint32_t txn = 0;
  CHECK(ptx_begin_pull(&REQ_A, &txn) == PTX_OK, "begin");
  const uint64_t sc = ptx_scene_digest();
  ptx_result r;
  CHECK(ptx_execute(txn, &r) == PTX_X_REFUSED && r.outcome == PTX_R_REFUSED && !strcmp(r.key, "egg:notEnoughVouchers"), "refused with the source key (%s)", r.key);
  CHECK(live_digest() == pre && card_digest() == pre && ptx_scene_digest() == sc, "profile, card and scene RNG unchanged");
  const snap_file* sys_before = find_file(&before, "system.sav");
  snap after = take();
  const snap_file* sys_after = find_file(&after, "system.sav");
  CHECK((!sys_before && !sys_after) || (sys_before && sys_after && sys_before->len == sys_after->len &&
                                         !memcmp(sys_before->bytes, sys_after->bytes, sys_before->len)),
        "system.sav untouched by a refusal");
  CHECK(ptx_pending() == 0, "nothing pending");
  ptx_result again;
  CHECK(ptx_execute(txn, &again) == PTX_X_REFUSED && result_eq(&r, &again), "asking again: the same refusal");
  drop(&before);
  drop(&after);
  /* capacity */
  restore(s0);
  make_profile(50);
  sysd_get()->n_eggs = 0;
  for (int i = 0; i < 98; i++) gc_system_add_egg(sysd_get(), 100 + (uint32_t)i, 10, 0, 1);
  CHECK(sysd_save(), "save a nearly full profile");
  reboot();
  const ptx_pull_request big = req(0, 4, "e-big", 1791504000000ull);
  CHECK(ptx_begin_pull(&big, &txn) == PTX_OK, "begin");
  CHECK(ptx_execute(txn, &r) == PTX_X_REFUSED && !strcmp(r.key, "egg:tooManyEggs"), "capacity refusal (%s)", r.key);
}

static void test_begin_refusals(const snap* s0) {
  restore(s0);
  ptx_reset();
  sysd_reset();
  cs_reset();
  uint32_t txn = 9;
  /* no scene seed supplied: the first begin is refused */
  ptx_set_scene_seed(NULL);
  CHECK(ptx_begin_pull(&REQ_A, &txn) == PTX_E_NO_SEED && txn == 0, "no scene seed");
  ptx_set_scene_seed(SEED);
  ptx_pull_request bad = REQ_A;
  bad.machine = 3;
  CHECK(ptx_begin_pull(&bad, &txn) == PTX_E_BAD_REQUEST, "machine 3");
  bad = REQ_A;
  bad.option = 5;
  CHECK(ptx_begin_pull(&bad, &txn) == PTX_E_BAD_REQUEST, "option 5");
  bad = REQ_A;
  bad.seed_len = 0;
  CHECK(ptx_begin_pull(&bad, &txn) == PTX_E_BAD_REQUEST, "no entropy seed");
  bad = REQ_A;
  bad.seed_len = PTX_SEED_MAX + 1;
  CHECK(ptx_begin_pull(&bad, &txn) == PTX_E_BAD_REQUEST, "too long a seed");
  bad = REQ_A;
  bad.timestamp_ms = 8640000000000001ull;
  CHECK(ptx_begin_pull(&bad, &txn) == PTX_E_BAD_REQUEST, "a clock out of range");
  CHECK(ptx_begin_pull(NULL, &txn) == PTX_E_BAD_REQUEST, "NULL");
  CHECK(ptx_begin_claim(0, &txn) == PTX_E_BAD_REQUEST, "record 0");
  snap clean = take();
  CHECK(!find_file(&clean, "ptx.sav"), "none of these wrote a file");
  drop(&clean);

  /* an unreadable profile is not a wallet: the defaults are never debited and nothing is written over the file */
  restore(s0);
  FILE* f = fopen(SAVE_DIR "/system.sav", "wb");
  fwrite("garbage that fails the checksum", 1, 31, f);
  fclose(f);
  remove(SAVE_DIR "/system.sav.bak");
  reboot();
  snap damaged = take();
  CHECK(ptx_begin_pull(&REQ_A, &txn) == PTX_E_UNUSABLE && txn == 0, "an unreadable profile is refused");
  CHECK(ptx_begin_claim(5, &txn) == PTX_E_UNUSABLE, "and a claim");
  snap damaged2 = take();
  CHECK(same_files(&damaged, &damaged2), "the files are exactly as they were");
  drop(&damaged);
  drop(&damaged2);
}

/* ------------------------------------------------------------------ the fault matrix: a pull */

static void check_after_boot(const golden* g, const snap* s0, const char* what, int n, int mode) {
  const uint64_t c = card_digest();
  CHECK(c == g->pre || c == g->post, "%s n=%d mode=%d: the card holds the profile before or after, never half (%llx)", what, n, mode, (unsigned long long)c);
  if (ptx_result_of(g->res.txn, NULL)) CHECK(c == g->post && live_digest() == g->post, "%s n=%d mode=%d: a committed result means the profile after (card and memory)", what, n, mode);
  if (c == g->post) CHECK(ptx_result_of(g->res.txn, NULL), "%s n=%d mode=%d: a profile after without its committed result", what, n, mode);
  (void)s0;
}

static void finish_pull(const golden* g, const ptx_pull_request* r, const char* what, int n, int mode) {
  /* whatever state it is in: pending -> run that transaction; a result already -> ask for it; nothing -> start over */
  ptx_result res;
  memset(&res, 0, sizeof res);
  ptx_exec x;
  const uint32_t p = ptx_pending();
  if (ptx_result_of(g->res.txn, NULL) && !p) {
    x = ptx_execute(g->res.txn, &res);
  } else if (p) {
    x = ptx_execute(p, &res);
  } else {
    uint32_t t = 0;
    CHECK(ptx_begin_pull(r, &t) == PTX_OK, "%s n=%d: begin again", what, n);
    x = ptx_execute(t, &res);
  }
  CHECK(x == PTX_X_DONE, "%s n=%d mode=%d: the retry ends done (%d)", what, n, mode, (int)x);
  CHECK(live_digest() == g->post && card_digest() == g->post, "%s n=%d mode=%d: exactly one pull (profile = the clean run's)", what, n, mode);
  CHECK(ptx_scene_digest() == g->scene, "%s n=%d mode=%d: the scene RNG advanced exactly once", what, n, mode);
  CHECK(res.n == g->res.n && res.outcome == PTX_R_DONE, "%s n=%d mode=%d: the result", what, n, mode);
  for (int i = 0; i < res.n && i < g->res.n; i++)
    CHECK(egg_eq(&res.eggs[i], &g->res.eggs[i]) && res.order[i] == g->res.order[i], "%s n=%d mode=%d: the same eggs as a clean run", what, n, mode);
  CHECK(ptx_pending() == 0 && !ptx_apply_pending(), "%s n=%d mode=%d: nothing left pending", what, n, mode);
  /* and a last restart sees the same */
  reboot();
  CHECK(live_digest() == g->post && card_digest() == g->post && ptx_pending() == 0, "%s n=%d mode=%d: after a restart", what, n, mode);
}

static void test_pull_matrix(const snap* s0) {
  const golden g = run_clean_pull(s0, &REQ_A);
  int cuts = 0, fails = 0;
  for (int n = 1; n <= g.writes + 1; n++) {
    /* the power is cut before write n */
    restore(s0);
    reboot();
    faults(0, n);
    uint32_t txn = 0;
    ptx_result r;
    const ptx_status b = ptx_begin_pull(&REQ_A, &txn);
    if (b == PTX_OK) ptx_execute(txn, &r);
    reboot();
    if (getenv("PTXDBG")) { char w[200]; printf("n=%d state=%d (%s) pending=%u apply=%d result=%d card==post %d card==pre %d live==post %d\n", n, (int)ptx_get_state(w), w, ptx_pending(), ptx_apply_pending(), ptx_result_of(g.res.txn, NULL), card_digest()==g.post, card_digest()==g.pre, live_digest()==g.post); }
    check_after_boot(&g, s0, "cut", n, 1);
    finish_pull(&g, &REQ_A, "cut", n, 1);
    cuts++;
    if (n > g.writes) continue;
    /* write n fails and the program goes on */
    restore(s0);
    reboot();
    faults(n, 0);
    txn = 0;
    ptx_exec x = PTX_X_UNKNOWN;
    const ptx_status b2 = ptx_begin_pull(&REQ_A, &txn);
    memset(&r, 0, sizeof r);
    if (b2 == PTX_OK) x = ptx_execute(txn, &r);
    const uint64_t live = live_digest();
    if (b2 != PTX_OK) {
      CHECK(b2 == PTX_E_IO && live == g.pre, "fail n=%d: a failed begin changes nothing", n);
    } else if (x == PTX_X_IO) {
      CHECK(live == g.pre, "fail n=%d: the commit write failed: the live profile is as it was (nothing debited or granted)", n);
      CHECK(ptx_result_of(txn, NULL) == false, "fail n=%d: no result on record", n);
      CHECK(r.n == 0 && r.txn == 0, "fail n=%d: nothing was revealed to the caller", n);
    } else if (x == PTX_X_DONE_APPLY_PENDING) {
      CHECK(live == g.post && result_eq(&r, &g.res), "fail n=%d: committed: the result is the real one", n);
      CHECK(ptx_apply_pending(), "fail n=%d: the roll-forward is pending", n);
    } else {
      CHECK(x == PTX_X_DONE, "fail n=%d: %d", n, (int)x);
    }
    no_faults();
    /* the same program retries (no restart) */
    ptx_settle();
    finish_pull(&g, &REQ_A, "fail", n, 0);
    fails++;
  }
  printf("profile_tx_test: pull matrix: %d writes in a clean run, %d power cuts and %d failed writes, all ended in the clean run's result\n", g.writes, cuts, fails);
}

/* ------------------------------------------------------------------ the fault matrix: the Nemesis reward */

static unsigned voucher0(void) { return sysd_get()->voucher_counts[GC_VOUCHER_REGULAR]; }

typedef struct {
  uint64_t pre, post;
  int writes;
  uint32_t txn;
} claim_golden;

static claim_golden run_clean_claim(const snap* s0) {
  restore(s0);
  reboot();
  claim_golden g;
  memset(&g, 0, sizeof g);
  g.pre = live_digest();
  faults(0, 0);
  CHECK(ptx_begin_claim(5, &g.txn) == PTX_OK, "claim begin");
  ptx_result r;
  CHECK(ptx_execute(g.txn, &r) == PTX_X_DONE && r.outcome == PTX_R_DONE && r.kind == PTX_KIND_CLAIM && r.record == 5 && r.vouchers == 1, "claim executes");
  g.writes = g_w;
  g.post = live_digest();
  return g;
}

static void finish_claim(const claim_golden* g, const char* what, int n, int mode, unsigned v0) {
  ptx_result r;
  memset(&r, 0, sizeof r);
  const uint32_t p = ptx_pending();
  ptx_exec x;
  if (p) {
    x = ptx_execute(p, &r);
  } else if (ptx_result_of(g->txn, NULL)) {
    x = ptx_execute(g->txn, &r);
  } else {
    uint32_t t = 0;
    const ptx_status b = ptx_begin_claim(5, &t);
    CHECK(b == PTX_OK, "%s n=%d: begin the claim again (%d)", what, n, (int)b);
    x = b == PTX_OK ? ptx_execute(t, &r) : PTX_X_UNKNOWN;
  }
  CHECK(x == PTX_X_DONE, "%s n=%d mode=%d: the retry ends done (%d)", what, n, mode, (int)x);
  CHECK(voucher0() == v0 + 1, "%s n=%d mode=%d: exactly one voucher (%u -> %u)", what, n, mode, v0, voucher0());
  CHECK(cs_reward_state(5) == CS_CLAIM_REPLAYED, "%s n=%d mode=%d: the record is claimed", what, n, mode);
  CHECK(card_digest() == g->post && live_digest() == g->post, "%s n=%d mode=%d: the profile is the clean run's", what, n, mode);
  CHECK(ptx_pending() == 0, "%s n=%d mode=%d: nothing pending", what, n, mode);
  uint32_t t2 = 0;
  CHECK(ptx_begin_claim(5, &t2) == PTX_E_NOT_OWED, "%s n=%d mode=%d: a second claim is refused", what, n, mode);
  reboot();
  CHECK(voucher0() == v0 + 1 && cs_reward_state(5) == CS_CLAIM_REPLAYED && card_digest() == g->post, "%s n=%d mode=%d: after a restart", what, n, mode);
}

static void test_claim_matrix(const snap* s0) {
  restore(s0);
  reboot();
  const unsigned v0 = voucher0();
  CHECK(cs_reward_state(5) == CS_CLAIM_OWED, "record 5 is owed in the fixture");
  const claim_golden g = run_clean_claim(s0);
  int cuts = 0, fails = 0;
  for (int n = 1; n <= g.writes + 1; n++) {
    restore(s0);
    reboot();
    faults(0, n);
    uint32_t txn = 0;
    ptx_result r;
    if (ptx_begin_claim(5, &txn) == PTX_OK) ptx_execute(txn, &r);
    reboot();
    /* after the boot: both applied or neither, never one */
    const bool voucher = voucher0() == v0 + 1;
    const bool claimed = cs_reward_state(5) == CS_CLAIM_REPLAYED;
    CHECK(voucher == claimed, "cut n=%d: the voucher and the claimed flag go together (voucher %d, claimed %d)", n, voucher, claimed);
    CHECK(voucher == (card_digest() == g.post), "cut n=%d: the card agrees", n);
    CHECK(voucher || voucher0() == v0, "cut n=%d: never a partial debit or grant", n);
    finish_claim(&g, "cut", n, 1, v0);
    cuts++;
    if (n > g.writes) continue;
    restore(s0);
    reboot();
    faults(n, 0);
    txn = 0;
    const ptx_status b = ptx_begin_claim(5, &txn);
    ptx_exec x = PTX_X_UNKNOWN;
    memset(&r, 0, sizeof r);
    if (b == PTX_OK) x = ptx_execute(txn, &r);
    if (x == PTX_X_IO || b != PTX_OK) CHECK(voucher0() == v0 && live_digest() == g.pre, "fail n=%d: a failed commit grants nothing in memory", n);
    if (x == PTX_X_DONE_APPLY_PENDING) CHECK(ptx_apply_pending(), "fail n=%d: roll-forward pending", n);
    no_faults();
    ptx_settle();
    finish_claim(&g, "fail", n, 0, v0);
    fails++;
  }
  printf("profile_tx_test: claim matrix: %d writes in a clean run, %d power cuts and %d failed writes, always one voucher with one claim\n", g.writes, cuts, fails);
}

static void test_claim_rules(const snap* s0) {
  restore(s0);
  reboot();
  uint32_t t = 0;
  CHECK(ptx_begin_claim(7, &t) == PTX_E_NOT_OWED, "an already claimed record");
  CHECK(ptx_begin_claim(9, &t) == PTX_E_NOT_OWED, "a record that was only stored");
  CHECK(ptx_begin_claim(99, &t) == PTX_E_NOT_OWED, "no such record");
  CHECK(ptx_pending() == 0, "none of them began anything");
  /* the cap: a full voucher count is a refusal that leaves the record owed */
  restore(s0);
  sysd_reset();
  sysd_get()->voucher_counts[GC_VOUCHER_REGULAR] = UINT32_MAX;
  CHECK(sysd_save(), "save a capped profile");
  reboot();
  CHECK(ptx_begin_claim(5, &t) == PTX_OK, "begin");
  ptx_result r;
  CHECK(ptx_execute(t, &r) == PTX_X_REFUSED && !strcmp(r.key, "profile:voucherCap"), "the cap refuses (%s)", r.key);
  CHECK(cs_reward_state(5) == CS_CLAIM_OWED && voucher0() == UINT32_MAX, "the reward is still owed, nothing granted");
  /* the record is claimed by another path between begin and execute: refused, nothing granted */
  restore(s0);
  reboot();
  const unsigned v0 = voucher0();
  CHECK(ptx_begin_claim(5, &t) == PTX_OK, "begin");
  CHECK(cs_claim_reward(5, NULL) == CS_CLAIM_DONE, "someone else claims it");
  CHECK(ptx_execute(t, &r) == PTX_X_REFUSED && !strcmp(r.key, "cc:notOwed") && voucher0() == v0, "refused (%s), no voucher", r.key);
  /* a corrupt custom file: refused at begin, files untouched */
  restore(s0);
  FILE* f = fopen(SAVE_DIR "/custom.sav", "wb");
  fwrite("not a custom file at all....", 1, 28, f);
  fclose(f);
  remove(SAVE_DIR "/custom.sav.bak");
  reboot();
  snap a = take();
  CHECK(ptx_begin_claim(5, &t) == PTX_E_UNUSABLE, "an unusable custom file");
  snap b = take();
  CHECK(same_files(&a, &b), "nothing written");
  drop(&a);
  drop(&b);
}

/* ------------------------------------------------------------------ the gate, the damaged file, roll-backs */

static void test_gate(const snap* s0) {
  const golden g = run_clean_pull(s0, &REQ_A);
  /* the roll-forward's system.sav write fails: the result is committed and returned, and the profile cannot be saved by anyone else
   * until the transaction is finished */
  restore(s0);
  reboot();
  int sys_write = 0;
  {
    /* which write is system.sav's: begin = 1, commit = 2, the custom file is not touched by a pull, so system.sav is the third */
    sys_write = 3;
  }
  faults(sys_write, 0);
  uint32_t txn = 0;
  CHECK(ptx_begin_pull(&REQ_A, &txn) == PTX_OK, "begin");
  ptx_result r;
  CHECK(ptx_execute(txn, &r) == PTX_X_DONE_APPLY_PENDING && result_eq(&r, &g.res), "committed, roll-forward failed");
  CHECK(live_digest() == g.post && card_digest() == g.pre, "memory has the pull, the card does not yet");
  /* a run in the meantime changes the profile in memory; saving it must wait for the transaction (and cannot overwrite the candidate) */
  sysd_get()->stats.highest_wave = 100;
  no_faults();
  g_fail_at = 0;
  const snap_file* none = NULL;
  (void)none;
  faults(1, 0);   /* the first write of the settle fails again: sysd_save must report it */
  CHECK(!sysd_save(), "sysd_save fails while the committed transaction cannot be rolled forward");
  CHECK(card_digest() == g.pre, "and it did not write the profile over the candidate");
  no_faults();
  CHECK(sysd_save(), "once the card works, sysd_save finishes the transaction and saves");
  CHECK(!ptx_apply_pending() && ptx_pending() == 0, "nothing pending");
  const uint64_t c = card_digest();
  CHECK(c == live_digest() && c != g.post, "the later change is on the card too (%llx)", (unsigned long long)c);
  reboot();
  CHECK(sysd_get()->stats.highest_wave == 100 && sysd_get()->n_eggs == g.eggs_after, "a restart: the pull and the later change");
  CHECK(ptx_result_of(g.res.txn, NULL), "and the result is still on record");

  /* a crash in the same window: the candidate is installed at boot, then progress made after it is kept */
  restore(s0);
  reboot();
  faults(0, 3);   /* power cut before the third write (system.sav) */
  CHECK(ptx_begin_pull(&REQ_A, &txn) == PTX_OK, "begin");
  ptx_execute(txn, &r);
  reboot();
  CHECK(live_digest() == g.post && card_digest() == g.post && ptx_pending() == 0, "boot installed the candidate and finished");
}

static void write_main_ptx(const void* data, size_t len) {
  FILE* f = fopen(SAVE_DIR "/ptx.sav", "wb");
  fwrite(data, 1, len, f);
  fclose(f);
}

static void test_ptx_file_states(const snap* s0) {
  const golden g = run_clean_pull(s0, &REQ_A);
  snap done = take();
  const snap_file* ptx_main = find_file(&done, "ptx.sav");
  CHECK(ptx_main != NULL, "the ptx file exists");

  /* damaged beyond the backup: reported, preserved, nothing can be bought, the profile untouched */
  restore(&done);
  remove(SAVE_DIR "/ptx.sav.bak");
  write_main_ptx("PTX1-and-then-nonsense", 22);
  snap damaged = take();
  reboot();
  char why[PTX_WHY_LEN];
  CHECK(ptx_get_state(why) == PTX_UNUSABLE && why[0], "a damaged file is reported (%s)", why);
  uint32_t t = 0;
  CHECK(ptx_begin_pull(&REQ_A, &t) == PTX_E_UNUSABLE && ptx_begin_claim(5, &t) == PTX_E_UNUSABLE, "and nothing can begin");
  CHECK(ptx_execute(g.res.txn, NULL) == PTX_X_UNUSABLE, "nor run");
  snap after = take();
  CHECK(same_files(&damaged, &after), "the files are exactly as they were");
  CHECK(live_digest() == g.post, "the profile is still readable");
  drop(&damaged);
  drop(&after);

  /* a ptx file from a newer version: the same */
  restore(&done);
  remove(SAVE_DIR "/ptx.sav.bak");
  {
    size_t n = ptx_main->len;
    uint8_t* b = malloc(n);
    memcpy(b, ptx_main->bytes, n);
    FILE* f = fopen(SAVE_DIR "/ptx.sav", "rb");
    /* the file is a save.c envelope: 12 bytes of header, then the payload; rewrite it through save_write with a bumped version */
    fclose(f);
    char* payload = NULL;
    size_t plen = 0;
    CHECK(save_read("ptx", &payload, &plen, NULL) == SAVE_OK, "read the payload");
    payload[4] = 9;   /* version 9 */
    remove(SAVE_DIR "/ptx.sav");
    CHECK(save_write("ptx", payload, plen) == SAVE_OK, "write the newer payload");
    remove(SAVE_DIR "/ptx.sav.bak");
    free(payload);
    free(b);
  }
  snap newer = take();
  reboot();
  CHECK(ptx_get_state(why) == PTX_UNUSABLE && strstr(why, "different version"), "a newer version is named (%s)", why);
  CHECK(ptx_begin_pull(&REQ_A, &t) == PTX_E_UNUSABLE, "nothing can begin");
  snap newer2 = take();
  CHECK(same_files(&newer, &newer2), "left exactly as it was");
  drop(&newer);
  drop(&newer2);

  /* the main file damaged with a good backup: the backup (the write before) is used, and it is a consistent state */
  restore(&done);
  {
    char* payload = NULL;
    size_t plen = 0;
    (void)payload;
    (void)plen;
    FILE* f = fopen(SAVE_DIR "/ptx.sav", "r+b");
    fseek(f, 20, SEEK_SET);
    fputc(0xFF, f);
    fclose(f);
  }
  reboot();
  /* the backup holds the committed-but-not-cleared envelope: the card already has the profile, the boot finishes the clear */
  CHECK(ptx_get_state(why) != PTX_UNUSABLE, "the backup is used (%s)", why);
  CHECK(live_digest() == g.post && ptx_pending() == 0, "and finishes: no second pull");
  ptx_result r;
  CHECK(ptx_result_of(g.res.txn, &r) && result_eq(&r, &g.res), "the result is still there");
  drop(&done);
}

static void test_rollback_never_runs_twice(const snap* s0) {
  /* the ptx file is put back to the moment after begin (a silent restore of an older copy) after the pull completed: the request is
   * refused, never run again on the profile it was not made for */
  restore(s0);
  reboot();
  uint32_t txn = 0;
  CHECK(ptx_begin_pull(&REQ_A, &txn) == PTX_OK, "begin");
  snap prepared = take();
  ptx_result r;
  CHECK(ptx_execute(txn, &r) == PTX_X_DONE, "execute");
  const uint64_t post = live_digest();
  snap finished = take();
  const snap_file* old_ptx = find_file(&prepared, "ptx.sav");
  restore(&finished);
  remove(SAVE_DIR "/ptx.sav");
  remove(SAVE_DIR "/ptx.sav.bak");
  FILE* f = fopen(SAVE_DIR "/ptx.sav", "wb");
  fwrite(old_ptx->bytes, 1, old_ptx->len, f);
  fclose(f);
  reboot();
  CHECK(ptx_pending() == txn, "the old record says the request is pending");
  ptx_result r2;
  CHECK(ptx_execute(txn, &r2) == PTX_X_REFUSED && !strcmp(r2.key, "ptx:profileChanged"), "it is refused (%s)", r2.key);
  CHECK(live_digest() == post && card_digest() == post, "the profile was not pulled on again");
  CHECK(ptx_pending() == 0, "and the request is closed");
  drop(&prepared);
  drop(&finished);
}

static void test_no_serialization(const snap* s0) {
  restore(s0);
  reboot();
  const uint64_t pre = live_digest();
  uint32_t txn = 0;
  CHECK(ptx_begin_pull(&REQ_A, &txn) == PTX_OK, "begin");
  ptx_set_profile_cap(8);   /* the profile no longer fits: gc_system_save refuses */
  ptx_result r;
  CHECK(ptx_execute(txn, &r) == PTX_X_IO, "a profile that cannot be serialized: nothing is done");
  ptx_set_profile_cap(0);
  CHECK(live_digest() == pre && card_digest() == pre && ptx_pending() == txn && !ptx_result_of(txn, NULL), "profile, card and record unchanged");
  CHECK(ptx_execute(txn, &r) == PTX_X_DONE, "and the same transaction runs once it can");
  /* begin refuses the same way */
  ptx_set_profile_cap(8);
  CHECK(ptx_begin_pull(&REQ_A, &txn) == PTX_E_IO, "begin: not serializable");
  ptx_set_profile_cap(0);
}

static void test_ring_and_ids(const snap* s0) {
  restore(s0);
  make_profile(60);
  sysd_get()->n_eggs = 0;
  CHECK(sysd_save(), "save");
  reboot();
  uint32_t ids[10];
  int done = 0;
  for (int i = 0; i < 10; i++) {
    const ptx_pull_request r = req(0, 0, "ring", 1791504000000ull + (uint64_t)i);
    uint32_t t = 0;
    CHECK(ptx_begin_pull(&r, &t) == PTX_OK, "begin %d", i);
    ptx_result x;
    CHECK(ptx_execute(t, &x) == PTX_X_DONE, "execute %d", i);
    ids[done++] = t;
    if (i) CHECK(t > ids[i - 1], "ids strictly increase");
  }
  CHECK(!ptx_result_of(ids[0], NULL) && !ptx_result_of(ids[1], NULL), "the two oldest results fell off a ring of %d", PTX_RESULTS);
  for (int i = 2; i < 10; i++) CHECK(ptx_result_of(ids[i], NULL), "result %d kept", i);
  reboot();
  for (int i = 2; i < 10; i++) CHECK(ptx_result_of(ids[i], NULL), "result %d kept across a restart", i);
  CHECK(sysd_get()->n_eggs >= 10, "eggs were added (%d)", sysd_get()->n_eggs);
}


static void test_prepared_survives_restart(const snap* s0) {
  const golden g = run_clean_pull(s0, &REQ_A);
  restore(s0);
  reboot();
  uint32_t txn = 0;
  CHECK(ptx_begin_pull(&REQ_A, &txn) == PTX_OK, "begin");
  const uint64_t pre = live_digest();
  reboot();
  /* the clock and the entropy seed were supplied once, at begin: the restart has them, the caller is not asked again */
  CHECK(ptx_pending() == txn && live_digest() == pre, "the request is pending after a restart, the profile untouched");
  ptx_result r;
  CHECK(ptx_execute(txn, &r) == PTX_X_DONE && live_digest() == g.post && result_eq(&r, &g.res), "it runs from the saved inputs to the clean run's result");
}

static void test_divergent(const snap* s0) {
  const golden g = run_clean_pull(s0, &REQ_A);
  /* committed, the power cut before the profile reached the card, and then the card's profile was changed by something else */
  restore(s0);
  reboot();
  faults(0, 3);
  uint32_t txn = 0;
  ptx_result r;
  CHECK(ptx_begin_pull(&REQ_A, &txn) == PTX_OK, "begin");
  ptx_execute(txn, &r);
  no_faults();
  sysd_reset();
  gc_system* s = sysd_get();
  s->stats.highest_wave = 999;   /* something else played in between (an older build, a restored copy) */
  {
    const size_t n = sysd_serialize(g_buf, sizeof g_buf);   /* written straight to the card: not through this build's gate */
    CHECK(n && save_write("system", g_buf, n) == SAVE_OK, "the other change reaches the card");
  }
  const uint64_t other = card_digest();
  snap before = take();
  reboot();
  char why[PTX_WHY_LEN];
  CHECK(ptx_get_state(why) == PTX_UNUSABLE && strstr(why, "changed while a purchase"), "the divergence is reported (%s)", why);
  snap after = take();
  CHECK(same_files(&before, &after) && card_digest() == other, "nothing was overwritten: the card is as the other change left it");
  CHECK(ptx_begin_pull(&REQ_A, &txn) == PTX_E_UNUSABLE, "and no purchase is made on top of it");
  (void)g;
  drop(&before);
  drop(&after);
}

static void test_unreadable_profile_at_boot(const snap* s0) {
  /* committed, the power cut before the profile reached the card, and then the profile file turned out damaged (both copies) */
  restore(s0);
  reboot();
  faults(0, 3);
  uint32_t txn = 0;
  ptx_result r;
  CHECK(ptx_begin_pull(&REQ_A, &txn) == PTX_OK, "begin");
  ptx_execute(txn, &r);
  no_faults();
  FILE* f = fopen(SAVE_DIR "/system.sav", "wb");
  fwrite("damaged damaged damaged damaged", 1, 31, f);
  fclose(f);
  remove(SAVE_DIR "/system.sav.bak");
  snap before = take();
  reboot();
  char why[PTX_WHY_LEN];
  ptx_get_state(why);
  CHECK(ptx_apply_pending() && ptx_pending() == txn, "the transaction stays committed, waiting");
  CHECK(ptx_recover() == PTX_E_UNUSABLE, "the boot cannot finish it");
  ptx_get_state(why);
  CHECK(strstr(why, "system data cannot be read") != NULL, "and says why in words (%s)", why);
  snap after = take();
  CHECK(same_files(&before, &after), "no file was written: the damaged profile is not overwritten, the record is kept");
  uint32_t t2 = 0;
  CHECK(ptx_begin_pull(&REQ_A, &t2) == PTX_E_UNUSABLE, "and nothing new can begin");
  /* the sysd_save gate does not turn an unreadable profile into a failure at every save */
  CHECK(sysd_save(), "sysd_save still reports nothing to write for an unreadable profile");
  snap again = take();
  CHECK(same_files(&before, &again), "still untouched");
  drop(&before);
  drop(&after);
  drop(&again);
}

static void test_refusal_matrix(const snap* s0) {
  restore(s0);
  make_profile(0);
  snap base = take();
  restore(&base);
  reboot();
  const uint64_t pre = live_digest();
  int writes = 0;
  {
    faults(0, 0);
    uint32_t t;
    ptx_result r;
    CHECK(ptx_begin_pull(&REQ_A, &t) == PTX_OK && ptx_execute(t, &r) == PTX_X_REFUSED, "a clean refusal");
    writes = g_w;
  }
  for (int n = 1; n <= writes + 1; n++) {
    for (int mode = 0; mode < 2; mode++) {
      if (mode == 1 && n > writes) continue;
      restore(&base);
      reboot();
      faults(mode ? n : 0, mode ? 0 : n);
      uint32_t t = 0;
      ptx_result r;
      memset(&r, 0, sizeof r);
      ptx_exec x = PTX_X_UNKNOWN;
      const ptx_status b = ptx_begin_pull(&REQ_A, &t);
      if (b == PTX_OK) x = ptx_execute(t, &r);
      if (mode == 0) reboot();
      else if (b == PTX_OK && n == 2) CHECK(x == PTX_X_IO && ptx_pending() == t && !ptx_result_of(t, NULL), "refusal write failed: still pending, no result");
      no_faults();
      CHECK(live_digest() == pre && card_digest() == pre, "refusal n=%d mode=%d: the profile never moves", n, mode);
      const uint32_t p = ptx_pending();
      if (p) {
        CHECK(ptx_execute(p, &r) == PTX_X_REFUSED && !strcmp(r.key, "egg:notEnoughVouchers"), "refusal n=%d: the retry is the same refusal", n);
      } else if (!ptx_result_of(t, NULL)) {
        CHECK(ptx_begin_pull(&REQ_A, &t) == PTX_OK && ptx_execute(t, &r) == PTX_X_REFUSED, "refusal n=%d: begin again", n);
      }
      CHECK(live_digest() == pre && ptx_pending() == 0, "refusal n=%d mode=%d: finished, profile unchanged", n, mode);
    }
  }
  drop(&base);
}

/* ------------------------------------------------------------------ fuzz */

static uint32_t rng_state = 12345;
static uint32_t rnd(void) {
  rng_state = rng_state * 1664525u + 1013904223u;
  return rng_state >> 8;
}

static void test_fuzz(const snap* s0, int scenarios) {
  int pulls_done = 0, cuts = 0, fails = 0, claims_done = 0;
  for (int sc = 0; sc < scenarios; sc++) {
    restore(s0);
    make_profile(80);
    sysd_get()->n_eggs = 0;
    CHECK(sysd_save(), "fuzz profile");
    reboot();
    const unsigned v0 = voucher0();
    int eggs_expected = 0;
    bool claimed = false;
    const int ops = 2 + (int)(rnd() % 4);
    for (int op = 0; op < ops; op++) {
      const bool claim = !claimed && rnd() % 4 == 0;
      const int mode = (int)(rnd() % 3);              /* 0 none, 1 fail, 2 cut */
      const int at = 1 + (int)(rnd() % 5);
      uint32_t txn = 0;
      ptx_result res;
      memset(&res, 0, sizeof res);
      const ptx_pull_request r = req((int)(rnd() % 3), (int)(rnd() % 3), "fz", 1791504000000ull + rnd());
      if (mode == 1) faults(at, 0);
      if (mode == 2) faults(0, at);
      ptx_status b = claim ? ptx_begin_claim(5, &txn) : ptx_begin_pull(&r, &txn);
      ptx_exec x = PTX_X_UNKNOWN;
      if (b == PTX_OK) x = ptx_execute(txn, &res);
      if (mode == 2) {
        reboot();
        cuts++;
      } else if (mode == 1) {
        fails++;
        no_faults();
      }
      /* retry until the program has an answer for the transaction it began (or none was begun) */
      for (int tries = 0; tries < 6; tries++) {
        if (x == PTX_X_DONE || x == PTX_X_REFUSED) break;
        const uint32_t p = ptx_pending();
        if (p) {
          x = ptx_execute(p, &res);
          txn = p;
        } else if (b != PTX_OK || ptx_result_of(txn, NULL) == false) {
          b = claim ? ptx_begin_claim(5, &txn) : ptx_begin_pull(&r, &txn);
          if (b == PTX_E_NOT_OWED) {
            x = PTX_X_REFUSED;
            break;
          }
          if (b == PTX_OK) x = ptx_execute(txn, &res);
        } else {
          x = ptx_execute(txn, &res);
        }
      }
      CHECK(x == PTX_X_DONE || x == PTX_X_REFUSED, "fuzz: the transaction ended (%d)", (int)x);
      if (x == PTX_X_DONE) {
        if (claim) {
          claimed = true;
          claims_done++;
        } else {
          eggs_expected += res.n;
          pulls_done++;
        }
      }
      CHECK(!ptx_apply_pending(), "fuzz: nothing left to apply");
    }
    reboot();
    CHECK(sysd_get()->n_eggs == eggs_expected, "fuzz: eggs on the card = the sum of the committed pulls (%d / %d)", sysd_get()->n_eggs, eggs_expected);
    CHECK(card_digest() == live_digest(), "fuzz: the card and memory agree after a restart");
    if (claimed) CHECK(cs_reward_state(5) == CS_CLAIM_REPLAYED, "fuzz: the claimed record");
    else CHECK(cs_reward_state(5) == CS_CLAIM_OWED, "fuzz: an unclaimed record is still owed");
    (void)v0;
  }
  printf("profile_tx_test: fuzz: %d scenarios, %d pulls and %d claims committed, %d power cuts and %d failed writes\n", scenarios, pulls_done, claims_done, cuts, fails);
}

/* ------------------------------------------------------------------ main */

int main(int argc, char** argv) {
  const int scenarios = argc > 1 ? atoi(argv[1]) : 150;
  char dir[] = "/tmp/ptxtestXXXXXX";
  if (!mkdtemp(dir) || chdir(dir) != 0 || mkdir("sdmc:", 0777) != 0 || !save_init()) {
    printf("cannot make the temporary SD folder\n");
    return 1;
  }
  const sysd_io sio = {f_read, f_write};
  const cs_io cio = {f_read, f_write};
  const ptx_io pio = {f_read, f_write};
  sysd_set_io(&sio);
  cs_set_io(&cio);
  ptx_set_io(&pio);
  cs_set_nonce(7);

  make_profile(20);
  make_custom();
  snap s0 = take();
  CHECK(s0.n >= 2, "the base fixture has files");

  test_begin_refusals(&s0);
  test_pull_matches_the_core(&s0);
  test_done_and_idempotent(&s0);
  test_refusal(&s0);
  test_pull_matrix(&s0);
  test_claim_matrix(&s0);
  test_claim_rules(&s0);
  test_gate(&s0);
  test_ptx_file_states(&s0);
  test_rollback_never_runs_twice(&s0);
  test_no_serialization(&s0);
  test_prepared_survives_restart(&s0);
  test_divergent(&s0);
  test_unreadable_profile_at_boot(&s0);
  test_refusal_matrix(&s0);
  test_ring_and_ids(&s0);
  test_fuzz(&s0, scenarios);

  printf("profile_tx_test: static bytes %zu (ptx), the profile buffers are allocated per transaction (2 x 1 MiB)\n", ptx_static_bytes());
  drop(&s0);
  wipe();
  if (chdir("/") == 0) {
    char cmd[128];
    snprintf(cmd, sizeof cmd, "rm -rf %s", dir);
    if (system(cmd)) {}
  }
  printf("profile_tx_test: %d checks, %d failed\n", checks, failed);
  return failed ? 1 : 0;
}
