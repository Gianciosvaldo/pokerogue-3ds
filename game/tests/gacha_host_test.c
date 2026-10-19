/* gacha_host_test: the capsule machine's purchase logic (gacha_host.c over profile_tx.c, system_data.c and the real save.c, with
 * game-core's real gc_system_gacha_pull) on the host (task C09).
 *
 *   make gacha_host_test && build/gacha_host_test [fuzz scenarios]
 *
 * Works in a temporary folder (the SD card's "sdmc:/..." paths are relative here). One fault-injecting I/O layer sits under the profile,
 * the custom file and the purchase record, as in profile_tx_test: a write can FAIL (nothing written, the program goes on) or the power
 * can be CUT before the n-th write (that write and every later one never happen; "reboot" rebuilds everything from the files).
 *
 *   - the option table (which voucher, how many, how many eggs, the capacity) equals what game-core does, read by pulling, and the
 *     screen's "can be bought / refused with key" equals game-core's own answer over a grid of edge states and a seeded fuzz;
 *   - every machine x option: one purchase = the profile game-core gives for the same inputs (eggs, debit, pity, statistics, display
 *     order), one committed result equal to it, the fixture in the DISPLAY order;
 *   - a refusal (vouchers, capacity, both) writes nothing and changes nothing; a failed write changes nothing and the retry gives the one
 *     result; two purchases cannot be started (held input, a nested call, a pending or unshown one);
 *   - a power cut before every write of a purchase and of its reveal mark, and a failed write at every one: the profile ends as a clean
 *     run's (one pull, never two), the eggs are shown once and then not again, and a lost mark repeats the same eggs, never a new pull.
 * Exit 1 on a failed check. */
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "cc/codec.h"
#include "cc/model.h"
#include "custom_store.h"
#include "gacha_host.h"
#include "gc/gacha.h"
#include "pr/rng.h"
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

#include "gacha_test_common.h"

/* ------------------------------------------------------------------ the table */

static void test_rules(void) {
  gh_rule r[GH_OPTIONS];
  int cap = 0;
  CHECK(gh_rules(r, &cap), "the rules can be read from the source");
  /* upstream's handleVoucherSelectAction / pullEggs (game-core/EGG-GACHA-VALIDATION.md): 1 regular -> 1, 10 regular -> 10, 1 plus -> 5,
   * 1 premium -> 10, 1 golden -> 25; at most 99 eggs */
  CHECK(cap == 99, "capacity %d", cap);
  for (int i = 0; i < 5; i++)
    CHECK(r[i].voucher == EXPECT[i].voucher && r[i].cost == EXPECT[i].cost && r[i].pulls == EXPECT[i].pulls, "option %d: voucher %d cost %d pulls %d", i,
          r[i].voucher, r[i].cost, r[i].pulls);
  /* the capacity edge of every option is where the source puts it */
  static gc_system s;
  for (int i = 0; i < 5; i++) {
    for (int d = -1; d <= 1; d++) {
      const int eggs = cap - r[i].pulls + d;
      if (eggs < 0) continue;
      gc_system_init_defaults(&s);
      for (int t = 0; t < 4; t++) s.voucher_counts[t] = 100;
      s.n_eggs = eggs;
      gc_gacha_ctx ctx;
      memset(&ctx, 0, sizeof ctx);
      ctx.cursor = i;
      sow(&ctx.entropy, "edge");
      sow(&ctx.scene_rng, "edge-scene");
      gc_gacha_result gr;
      const char* why = NULL;
      const bool ok = gc_system_gacha_pull(&s, &ctx, &gr, &why);
      CHECK(ok == (d <= 0) && (ok || (why && !strcmp(why, "egg:tooManyEggs"))), "option %d at %d eggs: ok %d", i, eggs, ok);
    }
  }
}

static void test_view_vs_source(int fuzz) {
  uint32_t v0[4] = {0, 0, 0, 0};
  fresh(v0, 0);
  static const int eggs_grid[] = {0, 1, 60, 73, 74, 75, 84, 88, 89, 90, 94, 95, 96, 98, 99};
  static const uint32_t vgrid[] = {0, 1, 2, 5, 9, 10, 11, 100};
  uint32_t rng = 12345;
  const int total = (int)(sizeof eggs_grid / sizeof *eggs_grid) * (int)(sizeof vgrid / sizeof *vgrid) + fuzz;
  int compared = 0, bought = 0, refused_eggs = 0, refused_vouchers = 0;
  for (int c = 0; c < total; c++) {
    gc_system* s = sysd_get();
    const int ng = (int)(sizeof eggs_grid / sizeof *eggs_grid), vg = (int)(sizeof vgrid / sizeof *vgrid);
    int eggs;
    if (c < ng * vg) {
      eggs = eggs_grid[c / vg];
      for (int t = 0; t < 4; t++) s->voucher_counts[t] = vgrid[c % vg];
    } else {
      rng = rng * 1664525u + 1013904223u;
      eggs = (int)((rng >> 8) % 100);
      for (int t = 0; t < 4; t++) {
        rng = rng * 1664525u + 1013904223u;
        s->voucher_counts[t] = (rng >> 12) % 14;
      }
    }
    s->n_eggs = eggs;
    for (int i = 0; i < eggs; i++) memset(&s->eggs[i], 0, sizeof s->eggs[i]), s->eggs[i].species = 1;
    gh_view v;
    CHECK(gh_view_of(&v), "the view");
    CHECK(!v.locked && v.n_eggs == eggs && v.capacity == 99, "view totals");
    for (int t = 0; t < 4; t++) CHECK(v.vouchers[t] == s->voucher_counts[t], "balance %d", t);
    for (int m = 0; m < GH_MACHINES; m++) {
      for (int o = 0; o < 5; o++) {
        g_scratch = *s;
        gc_gacha_ctx ctx;
        memset(&ctx, 0, sizeof ctx);
        ctx.machine = m;
        ctx.cursor = o;
        ctx.timestamp_ms = g_now;
        sow(&ctx.entropy, "grid");
        sow(&ctx.scene_rng, "grid-scene");
        gc_gacha_result gr;
        const char* why = NULL;
        const bool ok = gc_system_gacha_pull(&g_scratch, &ctx, &gr, &why);
        const gh_option* op = &v.opt[o];
        compared++;
        CHECK(op->can_buy == ok, "eggs %d v %u machine %d option %d: view says %d, game-core says %d", eggs, s->voucher_counts[0], m, o, op->can_buy, ok);
        if (!ok) {
          CHECK(op->refusal_key && why && !strcmp(op->refusal_key, why), "refusal key: view \"%s\" game-core \"%s\"", op->refusal_key ? op->refusal_key : "-",
                why ? why : "-");
          if (why && !strcmp(why, "egg:tooManyEggs")) refused_eggs++;
          else refused_vouchers++;
        } else {
          bought++;
        }
        CHECK(op->have == s->voucher_counts[EXPECT[o].voucher], "have");
      }
    }
  }
  printf("gacha_host_test: view vs game-core: %d comparisons (%d buyable, %d too many eggs, %d too few vouchers)\n", compared, bought, refused_eggs,
         refused_vouchers);
  CHECK(bought > 100 && refused_eggs > 100 && refused_vouchers > 100, "the grid covers all three answers");
}

/* ------------------------------------------------------------------ a purchase equals game-core's */

static void test_buy_matrix(void) {
  int pulls_made = 0;
  for (int m = 0; m < GH_MACHINES; m++) {
    for (int o = 0; o < GH_OPTIONS; o++) {
      fresh_all(30, 3);
      g_before = *sysd_get();
      const uint64_t scene0 = ptx_scene_digest();
      CHECK(scene0 == 0, "no scene stream before the first purchase");
      gh_outcome out;
      gh_buy(m, o, &out);
      CHECK(out.status == GH_BOUGHT && !out.apply_pending, "machine %d option %d: status %d", m, o, out.status);
      if (out.status != GH_BOUGHT) continue;
      pulls_made++;
      const ptx_result* r = &out.result;
      CHECK(r->kind == PTX_KIND_PULL && r->outcome == PTX_R_DONE && r->n == EXPECT[o].pulls, "n %d", r->n);
      /* the profile is exactly what game-core gives for the same inputs: the first seed text is the scene stream's, the second the request's */
      gc_gacha_result gr;
      const char* why;
      const bool ok = oracle(&g_before, m, o, g_seeds[2], g_seeds[1], &g_scratch, &gr, &why);
      CHECK(ok, "the oracle pull");
      CHECK(live_digest() == sys_digest(&g_scratch), "machine %d option %d: the live profile is game-core's profile", m, o);
      CHECK(card_digest() == live_digest(), "and the card holds it");
      for (int i = 0; i < r->n; i++) {
        CHECK(egg_eq(&r->eggs[i], &sysd_get()->eggs[g_before.n_eggs + i]), "the result's egg %d is the profile's egg", i);
        CHECK(r->order[i] == gr.order[i], "display order %d", i);
        CHECK(r->eggs[i].timestamp_ms == g_now && r->eggs[i].source_type == m, "egg %d carries the clock and the machine", i);
      }
      CHECK(sysd_get()->n_eggs == g_before.n_eggs + EXPECT[o].pulls, "eggs added");
      for (int t = 0; t < 4; t++) {
        const uint32_t want = g_before.voucher_counts[t] - (t == EXPECT[o].voucher ? EXPECT[o].cost : 0);
        CHECK(sysd_get()->voucher_counts[t] == want, "voucher %d: %u want %u", t, sysd_get()->voucher_counts[t], want);
      }
      CHECK(sysd_get()->stats.eggs_pulled == g_before.stats.eggs_pulled + EXPECT[o].pulls, "pull statistic");
      /* the result is eggs and nothing else: no species caught or seen, no candy, no unlock, no voucher unlock, no achievement */
      CHECK(!memcmp(sysd_get()->dex, g_before.dex, sizeof g_before.dex), "the dex is untouched (no species caught or seen)");
      CHECK(!memcmp(sysd_get()->starters, g_before.starters, sizeof g_before.starters), "starters and candy are untouched");
      CHECK(!memcmp(&sysd_get()->unlocks, &g_before.unlocks, sizeof g_before.unlocks) &&
                !memcmp(sysd_get()->voucher_unlocks, g_before.voucher_unlocks, sizeof g_before.voucher_unlocks) &&
                !memcmp(sysd_get()->achievement_flags, g_before.achievement_flags, sizeof g_before.achievement_flags) &&
                sysd_get()->classic_voucher_unlocked == g_before.classic_voucher_unlocked,
            "unlocks, voucher unlocks and achievements are untouched");
      CHECK(sysd_get()->stats.total_hatches == g_before.stats.total_hatches && sysd_get()->stats.total_catches == g_before.stats.total_catches, "nothing hatched or caught");
      /* ptx kept the same immutable result */
      ptx_result kept;
      CHECK(ptx_result_of(r->txn, &kept) && result_eq(&kept, r), "the kept result is the one returned");
      CHECK(ptx_pending() == 0, "nothing pending");
      /* the fixture follows the display order, not the storage order */
      cap_fixture f;
      CHECK(gh_fixture(r, &f) && f.n == r->n, "fixture");
      for (int k = 0; k < r->n; k++) {
        char want[CAP_LABEL];
        snprintf(want, sizeof want, "Egg %06u", (unsigned)(r->eggs[r->order[k]].id % 1000000u));
        CHECK(!strcmp(f.item[k].label, want) && f.item[k].tier == r->eggs[r->order[k]].tier, "fixture item %d is egg %d in display order", k, r->order[k]);
      }
      if (EXPECT[o].pulls > 1) {
        bool differs = false;
        for (int k = 0; k < r->n; k++)
          if (r->order[k] != k) differs = true;
        if (EXPECT[o].pulls >= 10) CHECK(differs, "a %d-egg result is displayed in a shuffled order", r->n);
        CHECK(ptx_scene_digest() != 0, "the scene stream exists");
      }
    }
  }
  CHECK(pulls_made == 15, "all fifteen machine / option pairs bought");
}

/* ------------------------------------------------------------------ refusals */

static void test_refusals(void) {
  for (int o = 0; o < GH_OPTIONS; o++) {
    for (int mode = 0; mode < 3; mode++) {  /* 0 vouchers short, 1 capacity, 2 both */
      uint32_t v[4] = {50, 50, 50, 50};
      int eggs = 5;
      if (mode != 1) v[EXPECT[o].voucher] = EXPECT[o].cost - 1;
      if (mode != 0) eggs = 99 - EXPECT[o].pulls + 1;
      fresh(v, eggs);
      const snap s0 = take();
      const uint64_t d0 = live_digest();
      faults(0, 0);   /* counts writes */
      gh_outcome out;
      gh_buy(1, o, &out);
      const char* want = mode == 0 ? "egg:notEnoughVouchers" : "egg:tooManyEggs";   /* capacity first when both, as the source */
      CHECK(out.status == GH_REFUSED && !strcmp(out.key, want), "option %d mode %d: status %d key \"%s\"", o, mode, out.status, out.key);
      CHECK(g_w == 0, "a refusal writes nothing (%d writes)", g_w);
      snap s1 = take();
      CHECK(same_files(&s0, &s1), "every file on the card is exactly as it was");
      CHECK(live_digest() == d0 && ptx_scene_digest() == 0 && ptx_pending() == 0 && !gh_unrevealed(NULL), "profile, scene stream and record unchanged");
      CHECK(find_file(&s1, "ptx.sav") == NULL, "no purchase record was created");
      char text[200];
      gh_message(&out, text, sizeof text);
      CHECK(strstr(text, "Nothing was spent") != NULL, "the player is told nothing was spent: \"%s\"", text);
      drop(&s1);
      drop((snap*)&s0);
    }
  }
  /* the source's own refusal after the table let it through: the profile changed between begin and execute */
  fresh_all(20, 3);
  uint32_t txn = 0;
  ptx_pull_request req;
  memset(&req, 0, sizeof req);
  req.machine = 0;
  req.option = 0;
  req.timestamp_ms = g_now;
  req.seed_len = 3;
  memcpy(req.seed, "abc", 3);
  ptx_set_scene_seed("scene");
  CHECK(ptx_begin_pull(&req, &txn) == PTX_OK, "begun");
  sysd_get()->voucher_counts[0] += 1;   /* something else changed the profile */
  const uint64_t d1 = live_digest();
  gh_outcome out;
  gh_resume(&out);
  CHECK(out.status == GH_REFUSED && !strcmp(out.key, "ptx:profileChanged"), "a request outliving a change is refused: status %d \"%s\"", out.status, out.key);
  CHECK(live_digest() == d1 && !gh_unrevealed(NULL) && ptx_pending() == 0, "and shows no eggs");
  char text[200];
  gh_message(&out, text, sizeof text);
  CHECK(strstr(text, "refused") != NULL, "\"%s\"", text);
  gh_buy(0, 0, &out);
  CHECK(out.status == GH_BOUGHT, "the machine works again afterwards");

  gh_buy(9, 0, &out);
  CHECK(out.status == GH_BAD, "a machine that does not exist");
  gh_buy(0, -1, &out);
  CHECK(out.status == GH_BAD, "an option that does not exist");
}

/* ------------------------------------------------------------------ no second purchase */

static void test_busy(void) {
  fresh_all(30, 3);
  gh_outcome out;
  gh_buy(0, 3, &out);
  CHECK(out.status == GH_BOUGHT, "first purchase");
  const uint32_t txn = out.result.txn;
  const uint64_t d1 = live_digest();
  const snap s1 = take();
  /* held / repeated: fifty more requests before the eggs are shown */
  faults(0, 0);
  for (int i = 0; i < 50; i++) {
    gh_buy(0, 3, &out);
    CHECK(out.status == GH_BUSY, "request %d while the eggs are not shown: %d", i, out.status);
  }
  CHECK(g_w == 0 && live_digest() == d1, "no write and no change from the repeats");
  snap s2 = take();
  CHECK(same_files(&s1, &s2), "files unchanged");
  drop(&s2);
  ptx_result nope;
  CHECK(!ptx_result_of(txn + 1, &nope) && ptx_pending() == 0, "there is no second transaction");
  CHECK(gh_unrevealed(&nope) && nope.txn == txn, "the unshown pull is the first one");
  CHECK(gh_reveal_done(txn), "shown");
  CHECK(!gh_unrevealed(NULL), "and it is not offered again");
  gh_buy(0, 3, &out);
  CHECK(out.status == GH_BOUGHT && out.result.txn == txn + 1, "now the next purchase is the next transaction");
  gh_reveal_done(out.result.txn);

  /* an unfinished transaction blocks a new one */
  fresh_all(30, 3);
  uint32_t t = 0;
  ptx_pull_request req;
  memset(&req, 0, sizeof req);
  req.timestamp_ms = g_now;
  req.seed_len = 1;
  req.seed[0] = 'x';
  ptx_set_scene_seed("s");
  CHECK(ptx_begin_pull(&req, &t) == PTX_OK, "begun aside");
  gh_buy(0, 0, &out);
  CHECK(out.status == GH_BUSY, "an unfinished purchase blocks a new one");
  gh_resume(&out);
  CHECK(out.status == GH_BOUGHT && out.result.txn == t, "and is finished, as itself");
  gh_resume(&out);
  CHECK(out.status == GH_NOTHING, "nothing left to finish");

  /* a call made from inside a purchase (the clock hook) is refused, not run */
  fresh_all(30, 3);
  g_nested = 1;
  g_nested_status = GH_NOTHING;
  gh_buy(0, 0, &out);
  g_nested = 0;
  CHECK(out.status == GH_BOUGHT && g_nested_status == GH_BUSY, "a nested purchase: outer %d nested %d", out.status, g_nested_status);
  ptx_result r2;
  CHECK(!ptx_result_of(out.result.txn + 1, &r2), "only one transaction exists");
  drop((snap*)&s1);
}

/* ------------------------------------------------------------------ restarts and failed writes */

typedef struct {
  ptx_result res;
  uint64_t after_digest, before_digest, scene_after, scene_before;
  uint32_t eggs_pulled_before, eggs_pulled_after;
  int writes_buy, writes_total;
} clean_t;

#define MACH 1
#define OPT 3

static void start_case(void) {
  uint32_t v[4] = {5, 5, 5, 5};
  fresh(v, 4);
}

static clean_t clean_run(void) {
  clean_t c;
  memset(&c, 0, sizeof c);
  start_case();
  c.before_digest = live_digest();
  c.scene_before = ptx_scene_digest();
  c.eggs_pulled_before = sysd_get()->stats.eggs_pulled;
  faults(0, 0);
  gh_outcome out;
  gh_buy(MACH, OPT, &out);
  CHECK(out.status == GH_BOUGHT, "clean run");
  c.writes_buy = g_w;
  c.res = out.result;
  gh_reveal_done(out.result.txn);
  c.writes_total = g_w;
  c.after_digest = live_digest();
  c.scene_after = ptx_scene_digest();
  c.eggs_pulled_after = sysd_get()->stats.eggs_pulled;
  return c;
}

/* What the screen does when it opens (gacha_screen_open), without the drawing: finish an unfinished purchase, then show a result never shown. */
static bool open_effect(ptx_result* shown, gh_status* st) {
  gh_outcome o;
  gh_resume(&o);
  if (st) *st = o.status;
  if (o.status == GH_BOUGHT) {
    *shown = o.result;
    return true;
  }
  if (o.status != GH_RETRY_IO && o.status != GH_UNUSABLE && o.status != GH_FATAL && gh_unrevealed(shown)) return true;
  return false;
}

static void converge_and_check(const clean_t* c, const char* what, bool expect_pull, bool expect_shown) {
  /* finish whatever is left, show once, mark, restart: the end state is the clean run's */
  for (int i = 0; i < 3 && ptx_apply_pending(); i++) ptx_settle();
  ptx_result shown;
  gh_status st;
  const bool did = open_effect(&shown, &st);
  if (expect_shown) {
    CHECK(did, "%s: a purchase that was made is shown after the restart", what);
    if (did) {
      CHECK(result_eq(&shown, &c->res), "%s: the result is the clean run's", what);
      CHECK(gh_reveal_done(shown.txn), "%s: marked", what);
    }
  } else {
    CHECK(!did, "%s: nothing to show", what);
  }
  reboot();
  ptx_result again;
  CHECK(!open_effect(&again, NULL), "%s: after the mark and a restart nothing is shown again", what);
  CHECK(ptx_pending() == 0 && !ptx_apply_pending(), "%s: nothing pending", what);
  if (expect_pull) {
    CHECK(live_digest() == c->after_digest && card_digest() == c->after_digest, "%s: profile is the clean run's, in memory and on the card", what);
    CHECK(sysd_get()->stats.eggs_pulled == c->eggs_pulled_after, "%s: one pull, not two (%u)", what, sysd_get()->stats.eggs_pulled);
    CHECK(ptx_scene_digest() == c->scene_after, "%s: the scene stream advanced once", what);
  } else {
    CHECK(live_digest() == c->before_digest && sysd_get()->stats.eggs_pulled == c->eggs_pulled_before, "%s: the profile is untouched", what);
  }
}

static void test_power_cuts(void) {
  clean_t c = clean_run();
  CHECK(c.writes_buy == 4 && c.writes_total == 5, "a purchase is 4 writes and the mark one more (%d, %d)", c.writes_buy, c.writes_total);
  for (int k = 1; k <= c.writes_total + 1; k++) {
    char what[64];
    snprintf(what, sizeof what, "power cut before write %d", k);
    start_case();
    const snap s0 = take();
    faults(0, k);
    gh_outcome out;
    gh_buy(MACH, OPT, &out);
    if (out.status == GH_BOUGHT) gh_reveal_done(out.result.txn);
    reboot();
    if (k == 1) {   /* nothing was saved: the purchase never happened */
      snap s1 = take();
      CHECK(same_files(&s0, &s1), "%s: the card is exactly as before", what);
      drop(&s1);
      CHECK(ptx_pending() == 0 && !gh_unrevealed(NULL), "%s: nothing pending, nothing to show", what);
      converge_and_check(&c, what, false, false);
    } else {
      converge_and_check(&c, what, true, k <= c.writes_total);   /* the last case has no cut: the mark was made */
    }
    drop((snap*)&s0);
    /* and the machine works afterwards */
    gh_buy(MACH, 0, &out);
    CHECK(out.status == GH_BOUGHT, "%s: a new purchase afterwards: %d", what, out.status);
  }
}

/* the scene stream as a first begin leaves it: seeded from the first seed text, not advanced */
static uint64_t scene_after_begin(void) {
  pr_rng r;
  sow(&r, "seed-1");
  const double d[5] = {r.c, r.s0, r.s1, r.s2, r.n};
  return sysd_digest((const uint8_t*)d, sizeof d);
}

static void test_failed_writes(void) {
  clean_t c = clean_run();
  for (int k = 1; k <= c.writes_total; k++) {
    char what[64];
    snprintf(what, sizeof what, "write %d fails", k);
    start_case();
    const uint64_t d0 = live_digest();
    faults(k, 0);
    gh_outcome out;
    gh_buy(MACH, OPT, &out);
    switch (k) {
      case 1:
        CHECK(out.status == GH_RETRY_IO, "%s: %d", what, out.status);
        CHECK(live_digest() == d0 && ptx_pending() == 0 && ptx_scene_digest() == 0, "%s: nothing changed, nothing begun", what);
        break;
      case 2:
        CHECK(out.status == GH_RETRY_IO, "%s: %d", what, out.status);
        CHECK(live_digest() == d0, "%s: the live profile is put back (no egg, no debit)", what);
        CHECK(ptx_scene_digest() == scene_after_begin(), "%s: the scene stream is as the begin left it (not advanced)", what);
        CHECK(!gh_unrevealed(NULL), "%s: nothing is shown", what);
        gh_buy(MACH, OPT, &out);
        CHECK(out.status == GH_BUSY, "%s: a new request waits for the unfinished one: %d", what, out.status);
        break;
      case 3:
      case 4:
        CHECK(out.status == GH_BOUGHT && out.apply_pending, "%s: committed, the card catches up later: %d", what, out.status);
        CHECK(ptx_apply_pending(), "%s: a file is waiting to catch up", what);
        CHECK(sysd_save() && !ptx_apply_pending(), "%s: the next profile save finishes it first", what);
        break;
      default:
        CHECK(out.status == GH_BOUGHT, "%s: %d", what, out.status);
        CHECK(!gh_reveal_done(out.result.txn), "%s: the mark is refused by the card", what);
        CHECK(!gh_unrevealed(NULL), "%s: but the eggs are not offered again in this session", what);
        break;
    }
    no_faults();
    if (k == 1) {
      gh_buy(MACH, OPT, &out);
      CHECK(out.status == GH_BOUGHT, "%s: the retry", what);
      /* a retry after a failed begin is a new request: its seed differs, so only check it is one pull */
      CHECK(sysd_get()->stats.eggs_pulled == c.eggs_pulled_after, "%s: one pull", what);
      gh_reveal_done(out.result.txn);
      reboot();
      ptx_result a;
      CHECK(!open_effect(&a, NULL), "%s: not shown again", what);
      continue;
    }
    if (k == 2) {
      gh_outcome r;
      gh_resume(&r);
      CHECK(r.status == GH_BOUGHT && result_eq(&r.result, &c.res), "%s: the retry is the same transaction with the clean run's result", what);
      gh_reveal_done(r.result.txn);
      reboot();
      ptx_result a;
      CHECK(!open_effect(&a, NULL), "%s: not shown again", what);
      CHECK(live_digest() == c.after_digest && sysd_get()->stats.eggs_pulled == c.eggs_pulled_after, "%s: the clean run's profile", what);
      continue;
    }
    if (k == 3 || k == 4) {
      CHECK(result_eq(&out.result, &c.res), "%s: the result is the clean run's", what);
      gh_reveal_done(out.result.txn);
      reboot();
      ptx_result a;
      CHECK(!open_effect(&a, NULL), "%s: not shown again", what);
      CHECK(live_digest() == c.after_digest && card_digest() == c.after_digest && sysd_get()->stats.eggs_pulled == c.eggs_pulled_after, "%s: the clean run's profile", what);
      continue;
    }
    /* the mark failed once: the next look at the state retries it and the card takes it, so a restart shows nothing again */
    reboot();
    ptx_result a;
    CHECK(!open_effect(&a, NULL), "%s: the retried mark was taken: nothing is shown after a restart", what);
    CHECK(live_digest() == c.after_digest && sysd_get()->stats.eggs_pulled == c.eggs_pulled_after, "%s: still the one pull", what);
  }
  /* a mark the card keeps refusing until the power goes: after the restart the SAME eggs are shown once more, and then never again */
  {
    start_case();
    gh_outcome out;
    gh_buy(MACH, OPT, &out);
    CHECK(out.status == GH_BOUGHT, "bought");
    faults(0, 1);   /* from now on every write fails */
    CHECK(!gh_reveal_done(out.result.txn) && !gh_unrevealed(NULL), "the mark is refused, the retry too; not offered again in this session");
    reboot();
    ptx_result a;
    CHECK(open_effect(&a, NULL) && result_eq(&a, &c.res), "a lost mark: the same eggs are shown again after the restart");
    CHECK(gh_reveal_done(a.txn), "marked now");
    reboot();
    CHECK(!open_effect(&a, NULL), "and now not again");
    CHECK(live_digest() == c.after_digest && sysd_get()->stats.eggs_pulled == c.eggs_pulled_after, "still the one pull");
  }
  /* a mark that keeps failing does not trap the player: the next purchase works and moves the watermark over the older one */
  start_case();
  gh_outcome out;
  gh_buy(MACH, 0, &out);
  CHECK(out.status == GH_BOUGHT, "first");
  faults(1, 0);
  CHECK(!gh_reveal_done(out.result.txn), "mark refused");
  no_faults();
  gh_buy(MACH, 0, &out);
  CHECK(out.status == GH_BOUGHT, "the second purchase is allowed (the first was shown): %d", out.status);
  gh_reveal_done(out.result.txn);
  reboot();
  ptx_result a;
  CHECK(!open_effect(&a, NULL), "the later mark covers the earlier pull");
}

/* ------------------------------------------------------------------ locks */

static void test_locks(void) {
  fresh_all(30, 3);
  /* a damaged purchase record */
  gh_outcome out;
  gh_buy(0, 0, &out);
  CHECK(out.status == GH_BOUGHT, "bought");
  gh_reveal_done(out.result.txn);
  FILE* fp = fopen(SAVE_DIR "/ptx.sav", "wb");
  fwrite("junk", 1, 4, fp);
  fclose(fp);
  remove(SAVE_DIR "/ptx.sav.bak");
  reboot();
  const snap s0 = take();
  gh_view v;
  CHECK(gh_view_of(&v) && v.locked && v.lock_text[0], "the view says nothing can be bought: \"%s\"", v.lock_text);
  for (int i = 0; i < 5; i++) CHECK(!v.opt[i].can_buy, "option %d is not buyable", i);
  faults(0, 0);
  gh_buy(0, 0, &out);
  CHECK(out.status == GH_UNUSABLE && out.why[0], "refused: %d", out.status);
  CHECK(g_w == 0, "no write");
  snap s1 = take();
  CHECK(same_files(&s0, &s1), "the damaged record is left as it is");
  drop(&s1);
  drop((snap*)&s0);

  /* an unreadable profile is never a wallet */
  fresh_all(30, 3);
  fp = fopen(SAVE_DIR "/system.sav", "wb");
  fwrite("junk", 1, 4, fp);
  fclose(fp);
  remove(SAVE_DIR "/system.sav.bak");
  reboot();
  CHECK(gh_view_of(&v) && v.locked, "the unreadable profile locks the machines: \"%s\"", v.lock_text);
  gh_buy(0, 0, &out);
  CHECK(out.status == GH_UNUSABLE, "and a purchase is refused: %d", out.status);
  snap s2 = take();
  CHECK(find_file(&s2, "ptx.sav") == NULL, "without a record being created");
  drop(&s2);
}

/* ------------------------------------------------------------------ fixture */

static const char* t_species_name(int species) {
  static char b[96];
  if (species == 999) return "";
  if (species == 998) return NULL;
  if (species == 997) return "A species name that is far too long for a label";
  snprintf(b, sizeof b, "Species %d", species);
  return b;
}

static void test_fixture(void) {
  ptx_result r;
  cap_fixture f;
  memset(&r, 0, sizeof r);
  CHECK(!gh_fixture(&r, &f), "an empty result is not a fixture");
  CHECK(!gh_fixture(NULL, &f) && !gh_fixture(&r, NULL), "NULL");
  r.kind = PTX_KIND_PULL;
  r.outcome = PTX_R_DONE;
  r.n = 3;
  for (int i = 0; i < 3; i++) {
    r.eggs[i].tier = (uint8_t)i;
    r.eggs[i].id = 100u + (uint32_t)i;
    r.order[i] = (uint8_t)(2 - i);
  }
  CHECK(gh_fixture(&r, &f) && f.n == 3, "a valid result");
  CHECK(f.item[0].tier == 2 && !strcmp(f.item[0].label, "Egg 000102") && f.item[2].tier == 0 && !strcmp(f.item[2].label, "Egg 000100"), "reversed order is shown reversed");
  /* with the app's species names: the species that will hatch, in the display order; no species, an empty or NULL name, a long name */
  gh_set_species_names(t_species_name);
  r.eggs[0].species = 150;
  r.eggs[1].species = 0;
  r.eggs[2].species = 999;
  CHECK(gh_fixture(&r, &f) && f.n == 3, "a result with a namer");
  CHECK(!strcmp(f.item[2].label, "Species 150") && f.item[2].tier == 0, "the display order is kept: item 2 is egg 0, named");
  CHECK(!strcmp(f.item[1].label, "Egg 000101"), "no species: the egg's id");
  CHECK(!strcmp(f.item[0].label, "Egg 000102"), "an empty name: the egg's id");
  r.eggs[2].species = 998;
  CHECK(gh_fixture(&r, &f) && !strcmp(f.item[0].label, "Egg 000102"), "a NULL name: the egg's id");
  r.eggs[2].species = 997;
  CHECK(gh_fixture(&r, &f) && strlen(f.item[0].label) == CAP_LABEL - 1 && !strncmp(f.item[0].label, "A species name that is far too ", CAP_LABEL - 1), "a long name is cut to the label");
  CHECK(cap_fixture_valid(&f), "and the fixture stays valid");
  gh_set_species_names(NULL);
  r.eggs[0].species = 150;
  CHECK(gh_fixture(&r, &f) && !strcmp(f.item[2].label, "Egg 000100"), "without a namer: ids again");
  memset(r.eggs, 0, sizeof r.eggs);
  for (int i = 0; i < 3; i++) {
    r.eggs[i].tier = (uint8_t)i;
    r.eggs[i].id = 100u + (uint32_t)i;
  }
  r.order[1] = 2;   /* not a permutation */
  CHECK(!gh_fixture(&r, &f), "a repeated index is refused");
  r.order[1] = 1;
  r.eggs[1].tier = 4;
  CHECK(!gh_fixture(&r, &f), "a tier out of range is refused");
  r.eggs[1].tier = 1;
  r.kind = PTX_KIND_CLAIM;
  CHECK(!gh_fixture(&r, &f), "a claim is not a pull");
  r.kind = PTX_KIND_PULL;
  r.outcome = PTX_R_REFUSED;
  CHECK(!gh_fixture(&r, &f), "a refusal shows no eggs");
  r.outcome = PTX_R_DONE;
  r.n = 26;
  CHECK(!gh_fixture(&r, &f), "26 eggs");
}

static void test_words(void) {
  for (int m = 0; m < 3; m++) CHECK(gh_machine_name(m)[0] && gh_machine_note(m)[0], "machine %d has words", m);
  for (int t = 0; t < 4; t++) CHECK(gh_voucher_name(t)[0], "voucher %d has a name", t);
  CHECK(!strcmp(gh_refusal_text("egg:notEnoughVouchers"), "You don't have enough vouchers."), "vouchers text");
  CHECK(!strcmp(gh_refusal_text("egg:tooManyEggs"), "You have too many eggs."), "capacity text");
}

int main(int argc, char** argv) {
  const int fuzz = argc > 1 ? atoi(argv[1]) : 400;
  char dir[] = "/tmp/gachahostXXXXXX";
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
  const gh_env env = {t_now, t_seed};
  gh_set_env(&env);

  test_rules();
  test_view_vs_source(fuzz);
  test_buy_matrix();
  test_refusals();
  test_busy();
  test_power_cuts();
  test_failed_writes();
  test_locks();
  test_fixture();
  test_words();

  wipe();
  if (chdir("/") == 0) {
    char cmd[128];
    snprintf(cmd, sizeof cmd, "rm -rf %s", dir);
    if (system(cmd)) {}
  }
  printf("gacha_host_test: %d checks, %d failed\n", checks, failed);
  return failed ? 1 : 0;
}
