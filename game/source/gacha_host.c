/* The capsule machine's purchase logic (see gacha_host.h). */
#include "gacha_host.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "gc/gacha.h"
#include "gc/system.h"
#include "pr/rng.h"
#include "system_data.h"

#define TS_MAX 8640000000000000.0

static struct {
  bool probed, probe_ok;
  gh_rule rule[GH_OPTIONS];
  int capacity;
  gh_env env;
  bool have_env;
  uint32_t counter;
  bool in_call;        /* a purchase is being made: nothing else may start (the calls are not re-entrant) */
  uint32_t shown_txn;  /* the highest pull shown in this session, whether or not the card recorded it */
} H;

/* ------------------------------------------------------------------ the table, read from the source */

static void sow(pr_rng* r, const char* text) {
  pr_seedstr s;
  pr_seed_from_ascii(&s, text);
  pr_rng_sow(r, &s);
}

/* One pull of `cursor` on a fresh scratch profile holding `eggs` eggs and plenty of every voucher. */
static bool probe_pull(gc_system* sys, int cursor, int eggs, const char** why) {
  gc_system_init_defaults(sys);
  for (int t = 0; t < GH_VOUCHER_TYPES; t++) sys->voucher_counts[t] = 1000;
  sys->n_eggs = eggs;
  gc_gacha_ctx ctx;
  memset(&ctx, 0, sizeof ctx);
  ctx.machine = 0;
  ctx.cursor = cursor;
  ctx.timestamp_ms = 86400000ull * 20000ull;
  sow(&ctx.entropy, "gacha-host-probe");
  sow(&ctx.scene_rng, "gacha-host-probe-scene");
  gc_gacha_result res;
  memset(&res, 0, sizeof res);
  return gc_system_gacha_pull(sys, &ctx, &res, why);
}

static bool probe(void) {
  gc_system* sys = malloc(sizeof *sys);
  if (!sys) return false;
  bool ok = true;
  for (int c = 0; c < GH_OPTIONS && ok; c++) {
    if (!probe_pull(sys, c, 0, NULL)) {
      ok = false;
      break;
    }
    int debited = -1;
    for (int t = 0; t < GH_VOUCHER_TYPES; t++) {
      if (sys->voucher_counts[t] == 1000) continue;
      if (debited >= 0 || sys->voucher_counts[t] > 1000) ok = false;
      debited = t;
    }
    const int pulls = sys->n_eggs;
    if (debited < 0 || !ok || pulls < 1 || pulls > PTX_MAX_EGGS) {
      ok = false;
      break;
    }
    H.rule[c].voucher = (uint8_t)debited;
    H.rule[c].cost = (uint8_t)(1000 - sys->voucher_counts[debited]);
    H.rule[c].pulls = (uint8_t)pulls;
  }
  /* the capacity: the most eggs held before a one-egg pull is refused for being too many (binary search over the egg count) */
  if (ok) {
    int lo = 0, hi = GC_MAX_EGGS;   /* a pull at lo is accepted, at hi it is not */
    const char* why = NULL;
    if (!probe_pull(sys, 0, lo, &why) || probe_pull(sys, 0, hi, &why) || !why || strcmp(why, "egg:tooManyEggs")) ok = false;
    while (ok && hi - lo > 1) {
      const int mid = (lo + hi) / 2;
      if (probe_pull(sys, 0, mid, NULL))
        lo = mid;
      else
        hi = mid;
    }
    if (ok) H.capacity = hi;   /* the first egg count at which one more egg is refused is the capacity */
  }
  free(sys);
  return ok;
}

bool gh_rules(gh_rule out[GH_OPTIONS], int* egg_capacity) {
  if (!H.probed) {
    H.probed = true;
    H.probe_ok = probe();
  }
  if (!H.probe_ok) return false;
  if (out) memcpy(out, H.rule, sizeof H.rule);
  if (egg_capacity) *egg_capacity = H.capacity;
  return true;
}

/* The source's own admission order: capacity first, then the cost. NULL = it can be bought. */
static const char* preflight(const gc_system* s, int option) {
  const gh_rule* r = &H.rule[option];
  if (s->n_eggs < 0 || s->n_eggs + (int)r->pulls > H.capacity) return "egg:tooManyEggs";
  if (s->voucher_counts[r->voucher] < r->cost) return "egg:notEnoughVouchers";
  return NULL;
}

/* ------------------------------------------------------------------ the view */

bool gh_view_of(gh_view* v) {
  memset(v, 0, sizeof *v);
  if (!gh_rules(NULL, NULL)) {
    v->locked = true;
    snprintf(v->lock_text, sizeof v->lock_text, "The machine's rules could not be read. Nothing can be bought.");
    return false;
  }
  const gc_system* s = sysd_get();
  for (int t = 0; t < GH_VOUCHER_TYPES; t++) v->vouchers[t] = s->voucher_counts[t];
  v->n_eggs = s->n_eggs;
  v->capacity = H.capacity;
  char why[PTX_WHY_LEN];
  char swhy[SYSD_WHY_LEN];
  if (sysd_state(swhy) == SYSD_UNREADABLE) {
    v->locked = true;
    snprintf(v->lock_text, sizeof v->lock_text, "%s Nothing can be bought.", swhy);
  } else if (ptx_get_state(why) == PTX_UNUSABLE) {
    v->locked = true;
    snprintf(v->lock_text, sizeof v->lock_text, "%s", why);
  }
  for (int i = 0; i < GH_OPTIONS; i++) {
    gh_option* o = &v->opt[i];
    o->rule = H.rule[i];
    o->have = s->voucher_counts[H.rule[i].voucher];
    o->refusal_key = preflight(s, i);
    o->can_buy = !o->refusal_key && !v->locked;
  }
  return true;
}

/* ------------------------------------------------------------------ environment */

void gh_set_env(const gh_env* env) {
  H.have_env = env != NULL;
  if (env) H.env = *env;
}

static uint64_t env_now(void) {
  if (H.have_env && H.env.now_ms) return H.env.now_ms();
  return (uint64_t)time(NULL) * 1000u;
}

static void env_seed(char out[PTX_SEED_MAX + 1]) {
  if (H.have_env && H.env.seed_text) {
    H.env.seed_text(out);
    out[PTX_SEED_MAX] = 0;
    return;
  }
  snprintf(out, PTX_SEED_MAX + 1, "%llx-%lx-%x", (unsigned long long)time(NULL), (unsigned long)clock(), (unsigned)++H.counter);
}

/* ------------------------------------------------------------------ the purchase */

static void clear_outcome(gh_outcome* o, gh_status st) {
  memset(o, 0, sizeof *o);
  o->status = st;
}

static void fill_unusable(gh_outcome* o) {
  char why[PTX_WHY_LEN];
  ptx_get_state(why);
  o->status = GH_UNUSABLE;
  snprintf(o->why, sizeof o->why, "%s", why[0] ? why : "The purchase record cannot be used.");
}

/* ptx_execute's answer as the outcome of a purchase. */
static void from_exec(ptx_exec x, const ptx_result* r, gh_outcome* o) {
  switch (x) {
    case PTX_X_DONE:
    case PTX_X_DONE_APPLY_PENDING:
      if (r->kind == PTX_KIND_PULL && r->outcome == PTX_R_DONE) {
        o->status = GH_BOUGHT;
        o->result = *r;
        o->apply_pending = x == PTX_X_DONE_APPLY_PENDING;
      } else {
        o->status = GH_SETTLED;
      }
      break;
    case PTX_X_REFUSED:
      o->status = GH_REFUSED;
      snprintf(o->key, sizeof o->key, "%s", r->key);
      break;
    case PTX_X_IO:
      o->status = GH_RETRY_IO;
      break;
    case PTX_X_FATAL:
      o->status = GH_FATAL;
      snprintf(o->why, sizeof o->why, "The purchase could not be saved and the system data could not be put back. Restart the game.");
      break;
    case PTX_X_UNKNOWN:
    case PTX_X_UNUSABLE:
      fill_unusable(o);
      break;
  }
}

void gh_buy(int machine, int option, gh_outcome* out) {
  clear_outcome(out, GH_BUSY);
  if (H.in_call) return;
  H.in_call = true;
  do {
    if (machine < 0 || machine >= GH_MACHINES || option < 0 || option >= GH_OPTIONS) {
      out->status = GH_BAD;
      break;
    }
    if (ptx_pending() || gh_unrevealed(NULL)) break;   /* GH_BUSY: the one before it is not done */
    if (!gh_rules(NULL, NULL)) {
      out->status = GH_UNUSABLE;
      snprintf(out->why, sizeof out->why, "The machine's rules could not be read. Nothing can be bought.");
      break;
    }
    char why[PTX_WHY_LEN];
    char swhy[SYSD_WHY_LEN];
    if (sysd_state(swhy) == SYSD_UNREADABLE) {   /* the defaults in memory are not the player's wallet: say so, do not refuse as "no vouchers" */
      out->status = GH_UNUSABLE;
      snprintf(out->why, sizeof out->why, "%s Nothing can be bought.", swhy);
      break;
    }
    if (ptx_get_state(why) == PTX_UNUSABLE) {
      fill_unusable(out);
      break;
    }
    const char* refusal = preflight(sysd_get(), option);
    if (refusal) {   /* the source would refuse it the same way; nothing is written to find that out */
      out->status = GH_REFUSED;
      snprintf(out->key, sizeof out->key, "%s", refusal);
      break;
    }
    char seed[PTX_SEED_MAX + 1];
    env_seed(seed);
    ptx_set_scene_seed(seed);   /* used only when the file is new, once */
    env_seed(seed);
    const uint64_t now = env_now();
    ptx_pull_request req;
    memset(&req, 0, sizeof req);
    req.machine = (uint8_t)machine;
    req.option = (uint8_t)option;
    req.timestamp_ms = (double)now <= TS_MAX ? now : 0;
    req.seed_len = (uint8_t)strlen(seed);
    if (req.seed_len < 1) {
      seed[0] = '0';
      seed[1] = 0;
      req.seed_len = 1;
    }
    memcpy(req.seed, seed, req.seed_len);
    uint32_t txn = 0;
    switch (ptx_begin_pull(&req, &txn)) {
      case PTX_OK:
        break;
      case PTX_E_IO:
      case PTX_E_NO_MEMORY:
        out->status = GH_RETRY_IO;
        goto done;
      case PTX_E_BUSY:
        out->status = GH_BUSY;
        goto done;
      case PTX_E_BAD_REQUEST:
      case PTX_E_NO_SEED:
        out->status = GH_BAD;
        goto done;
      case PTX_E_UNUSABLE:
      case PTX_E_NOT_OWED:
        fill_unusable(out);
        goto done;
    }
    ptx_result res;
    memset(&res, 0, sizeof res);
    const ptx_exec x = ptx_execute(txn, &res);
    from_exec(x, &res, out);
  done:;
  } while (0);
  H.in_call = false;
}

void gh_resume(gh_outcome* out) {
  clear_outcome(out, GH_NOTHING);
  if (H.in_call) {
    out->status = GH_BUSY;
    return;
  }
  const uint32_t txn = ptx_pending();
  if (!txn) return;
  H.in_call = true;
  ptx_result res;
  memset(&res, 0, sizeof res);
  const ptx_exec x = ptx_execute(txn, &res);
  from_exec(x, &res, out);
  H.in_call = false;
}

bool gh_unrevealed(ptx_result* out) {
  for (int guard = 0; guard <= PTX_RESULTS; guard++) {
    ptx_result r;
    const uint32_t u = ptx_unrevealed(&r);
    if (!u) return false;
    if (u > H.shown_txn) {
      if (out) *out = r;
      return true;
    }
    /* shown in this session but the card did not take the mark: try again, and do not show it again meanwhile */
    if (!ptx_mark_revealed(u)) return false;
  }
  return false;
}

bool gh_reveal_done(uint32_t txn) {
  if (txn > H.shown_txn) H.shown_txn = txn;
  return ptx_mark_revealed(txn);
}

/* ------------------------------------------------------------------ the fixture */

bool gh_fixture(const ptx_result* r, cap_fixture* f) {
  if (!r || !f) return false;
  memset(f, 0, sizeof *f);
  if (r->kind != PTX_KIND_PULL || r->outcome != PTX_R_DONE || r->n < 1 || r->n > CAP_MAX) return false;
  bool seen[PTX_MAX_EGGS] = {false};
  for (int k = 0; k < r->n; k++) {
    const int idx = r->order[k];
    if (idx >= r->n || seen[idx]) {
      memset(f, 0, sizeof *f);
      return false;
    }
    seen[idx] = true;
    const gc_egg* e = &r->eggs[idx];
    if (e->tier >= CAP_TIERS) {
      memset(f, 0, sizeof *f);
      return false;
    }
    f->item[k].tier = e->tier;
    snprintf(f->item[k].label, CAP_LABEL, "Egg %06u", (unsigned)(e->id % 1000000u));
  }
  f->n = r->n;
  return cap_fixture_valid(f);
}

/* ------------------------------------------------------------------ words */

const char* gh_machine_name(int machine) {
  static const char* const n[GH_MACHINES] = {"Move Gacha", "Legendary Gacha", "Shiny Gacha"};
  return machine >= 0 && machine < GH_MACHINES ? n[machine] : "";
}

const char* gh_machine_note(int machine) {
  static const char* const n[GH_MACHINES] = {"Better odds of a rare egg move.", "Better legendary odds, one featured each day.",
                                             "Better odds of a shiny egg."};
  return machine >= 0 && machine < GH_MACHINES ? n[machine] : "";
}

const char* gh_voucher_name(int type) {
  static const char* const n[GH_VOUCHER_TYPES] = {"Regular", "Plus", "Premium", "Golden"};
  return type >= 0 && type < GH_VOUCHER_TYPES ? n[type] : "";
}

const char* gh_refusal_text(const char* key) {
  if (key && !strcmp(key, "egg:notEnoughVouchers")) return "You don't have enough vouchers.";
  if (key && !strcmp(key, "egg:tooManyEggs")) return "You have too many eggs.";
  return "That purchase was refused. Nothing was spent.";
}

void gh_message(const gh_outcome* o, char* out, size_t n) {
  if (!n) return;
  out[0] = 0;
  switch (o->status) {
    case GH_BOUGHT:
      snprintf(out, n, o->apply_pending ? "Your eggs are safe. The SD card is updated at the next save." : "");
      break;
    case GH_SETTLED:
    case GH_NOTHING:
      break;
    case GH_REFUSED:
      snprintf(out, n, "%s Nothing was spent.", gh_refusal_text(o->key));
      break;
    case GH_BUSY:
      snprintf(out, n, "Another purchase is not finished yet.");
      break;
    case GH_RETRY_IO:
      snprintf(out, n, "The SD card did not take the save. Nothing was spent. Try again.");
      break;
    case GH_UNUSABLE:
    case GH_FATAL:
      snprintf(out, n, "%s", o->why);
      break;
    case GH_BAD:
      snprintf(out, n, "That option does not exist.");
      break;
  }
}

void gh_reset(void) {
  /* a restart forgets the session; the table read from the source is a fact about the build, so it is kept (probing again would only
   * repeat the same answer) */
  const gh_env env = H.env;
  const bool have = H.have_env;
  const bool probed = H.probed, ok = H.probe_ok;
  const int capacity = H.capacity;
  gh_rule rule[GH_OPTIONS];
  memcpy(rule, H.rule, sizeof rule);
  memset(&H, 0, sizeof H);
  H.env = env;
  H.have_env = have;
  H.probed = probed;
  H.probe_ok = ok;
  H.capacity = capacity;
  memcpy(H.rule, rule, sizeof rule);
}
