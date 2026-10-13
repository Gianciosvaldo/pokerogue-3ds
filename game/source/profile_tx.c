/* The profile transaction owner (see profile_tx.h and docs/CUSTOM-CONTENT-CONTRACTS.md, section 2.14). */
#include "profile_tx.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "custom_store.h"
#include "gc/gacha.h"
#include "system_data.h"

#define PROFILE_MAX (1024 * 1024)       /* the bound system_data.c uses for a serialized profile */
#define ENV_FIXED (32 * 1024)           /* the envelope's own fields, with room to spare (about 10 KB are used) */
#define VERSION 2   /* 2 added revealed_txn (task C09); version 1 files are still read */
#define TS_MAX 8640000000000000.0

enum { PEND_NONE = 0, PEND_PREPARED = 1, PEND_COMMITTED = 2 };

/* Everything the envelope holds besides the candidate profile bytes. */
typedef struct {
  uint32_t next_txn;                    /* the next transaction id, >= 1 */
  uint32_t revealed_txn;                /* every pull up to this id has been shown to the player (a watermark; 0 = none) */
  bool scene_seeded;
  pr_rng scene;                         /* the gacha scene RNG: its own stream, never a run's */
  int n_results;
  ptx_result results[PTX_RESULTS];      /* oldest first */
  uint8_t pend;
  uint8_t pend_kind;
  uint32_t pend_txn;
  ptx_pull_request req;                 /* PULL: what was supplied once */
  uint32_t claim_record;                /* CLAIM */
  uint64_t pre_hash, post_hash;         /* canonical digests of the profile before and after (COMMITTED) */
  uint32_t profile_len;                 /* COMMITTED: the candidate profile bytes */
} core;

static core P;           /* what the card holds */
static core N;           /* a transaction built aside: it replaces P only after its write succeeded */
static uint8_t* g_profile;   /* the candidate bytes of the committed pending transaction (P.profile_len) */
static struct {
  bool loaded;
  ptx_state state;
  char why[PTX_WHY_LEN];
  bool in_settle;
  bool fatal;
  char seed[PTX_SEED_MAX + 1];
  bool have_seed;
  ptx_io io;
  bool have_io;
  size_t cap;           /* test hook: a smaller serialization bound (0 = PROFILE_MAX) */
} S;

static size_t bound(void) { return S.cap ? S.cap : PROFILE_MAX; }

static save_result do_read(char** out, size_t* len, bool* used_backup) {
  return S.have_io ? S.io.read(PTX_FILE_NAME, out, len, used_backup) : save_read(PTX_FILE_NAME, out, len, used_backup);
}
static save_result do_write(const void* data, size_t len) {
  return S.have_io ? S.io.write(PTX_FILE_NAME, data, len) : save_write(PTX_FILE_NAME, data, len);
}

/* ------------------------------------------------------------------ envelope bytes */

typedef struct {
  uint8_t* p;
  size_t n, cap;
  bool bad;
} wr;
typedef struct {
  const uint8_t* p;
  size_t n, i;
  bool bad;
} rd;

static void w_bytes(wr* w, const void* d, size_t n) {
  if (w->bad || n > w->cap - w->n) {
    w->bad = true;
    return;
  }
  memcpy(w->p + w->n, d, n);
  w->n += n;
}
static void w8(wr* w, uint8_t v) { w_bytes(w, &v, 1); }
static void w16(wr* w, uint16_t v) {
  const uint8_t b[2] = {(uint8_t)v, (uint8_t)(v >> 8)};
  w_bytes(w, b, 2);
}
static void w32(wr* w, uint32_t v) {
  uint8_t b[4];
  for (int i = 0; i < 4; i++) b[i] = (uint8_t)(v >> (8 * i));
  w_bytes(w, b, 4);
}
static void w64(wr* w, uint64_t v) {
  uint8_t b[8];
  for (int i = 0; i < 8; i++) b[i] = (uint8_t)(v >> (8 * i));
  w_bytes(w, b, 8);
}

static const uint8_t* r_take(rd* r, size_t n) {
  if (r->bad || n > r->n - r->i) {
    r->bad = true;
    return NULL;
  }
  const uint8_t* q = r->p + r->i;
  r->i += n;
  return q;
}
static uint8_t r8(rd* r) {
  const uint8_t* q = r_take(r, 1);
  return q ? q[0] : 0;
}
static uint16_t r16(rd* r) {
  const uint8_t* q = r_take(r, 2);
  return q ? (uint16_t)(q[0] | q[1] << 8) : 0;
}
static uint32_t r32(rd* r) {
  const uint8_t* q = r_take(r, 4);
  uint32_t v = 0;
  if (q)
    for (int i = 0; i < 4; i++) v |= (uint32_t)q[i] << (8 * i);
  return v;
}
static uint64_t r64(rd* r) {
  const uint8_t* q = r_take(r, 8);
  uint64_t v = 0;
  if (q)
    for (int i = 0; i < 8; i++) v |= (uint64_t)q[i] << (8 * i);
  return v;
}

static void put_rng(wr* w, const pr_rng* g) {
  const double d[5] = {g->c, g->s0, g->s1, g->s2, g->n};
  for (int i = 0; i < 5; i++) {
    uint64_t b;
    memcpy(&b, &d[i], 8);
    w64(w, b);
  }
}
static void get_rng(rd* r, pr_rng* g) {
  double d[5];
  for (int i = 0; i < 5; i++) {
    const uint64_t b = r64(r);
    memcpy(&d[i], &b, 8);
  }
  g->c = d[0];
  g->s0 = d[1];
  g->s1 = d[2];
  g->s2 = d[3];
  g->n = d[4];
}

static void put_egg(wr* w, const gc_egg* e) {
  w32(w, e->id);
  w32(w, (uint32_t)e->hatch_waves);
  w8(w, e->tier);
  w16(w, e->species);
  w8(w, e->source_type);
  w8(w, e->variant_tier);
  w8(w, e->egg_move_index);
  w8(w, e->is_shiny);
  w8(w, e->override_hidden_ability);
  w64(w, e->timestamp_ms);
}
static void get_egg(rd* r, gc_egg* e) {
  memset(e, 0, sizeof *e);
  e->id = r32(r);
  e->hatch_waves = (int32_t)r32(r);
  e->tier = r8(r);
  e->species = r16(r);
  e->source_type = r8(r);
  e->variant_tier = r8(r);
  e->egg_move_index = r8(r);
  e->is_shiny = r8(r) != 0;
  e->override_hidden_ability = r8(r) != 0;
  e->timestamp_ms = r64(r);
}

static void put_result(wr* w, const ptx_result* x) {
  w32(w, x->txn);
  w8(w, x->kind);
  w8(w, x->outcome);
  w_bytes(w, x->key, PTX_KEY_LEN);
  w8(w, x->n);
  w_bytes(w, x->order, PTX_MAX_EGGS);
  for (int i = 0; i < PTX_MAX_EGGS; i++) put_egg(w, &x->eggs[i]);
  w32(w, x->record);
  w8(w, x->vouchers);
}
static void get_result(rd* r, ptx_result* x) {
  memset(x, 0, sizeof *x);
  x->txn = r32(r);
  x->kind = r8(r);
  x->outcome = r8(r);
  const uint8_t* k = r_take(r, PTX_KEY_LEN);
  if (k) memcpy(x->key, k, PTX_KEY_LEN);
  x->n = r8(r);
  const uint8_t* o = r_take(r, PTX_MAX_EGGS);
  if (o) memcpy(x->order, o, PTX_MAX_EGGS);
  for (int i = 0; i < PTX_MAX_EGGS; i++) get_egg(r, &x->eggs[i]);
  x->record = r32(r);
  x->vouchers = r8(r);
}

/* Encodes c (and the candidate bytes when c->pend == PEND_COMMITTED). Returns a malloc'd buffer or NULL. */
static uint8_t* encode(const core* c, const uint8_t* profile, size_t* len) {
  const size_t cap = ENV_FIXED + (c->pend == PEND_COMMITTED ? c->profile_len : 0);
  wr w = {malloc(cap), 0, cap, false};
  if (!w.p) return NULL;
  w_bytes(&w, "PTX1", 4);
  w16(&w, VERSION);
  w16(&w, 0);
  w32(&w, c->next_txn);
  w32(&w, c->revealed_txn);
  w8(&w, c->scene_seeded);
  put_rng(&w, &c->scene);
  w8(&w, (uint8_t)c->n_results);
  for (int i = 0; i < c->n_results; i++) put_result(&w, &c->results[i]);
  w8(&w, c->pend);
  w8(&w, c->pend_kind);
  w32(&w, c->pend_txn);
  w8(&w, c->req.machine);
  w8(&w, c->req.option);
  w8(&w, c->req.seed_len);
  w64(&w, c->req.timestamp_ms);
  w_bytes(&w, c->req.seed, PTX_SEED_MAX);
  w32(&w, c->claim_record);
  w64(&w, c->pre_hash);
  w64(&w, c->post_hash);
  w32(&w, c->pend == PEND_COMMITTED ? c->profile_len : 0);
  if (c->pend == PEND_COMMITTED) w_bytes(&w, profile, c->profile_len);
  if (w.bad) {
    free(w.p);
    return NULL;
  }
  *len = w.n;
  return w.p;
}

/* Decodes into c and (for a committed pending transaction) *profile (malloc'd). 0 = ok, 1 = damaged, 2 = another version. */
static int decode(const uint8_t* buf, size_t len, core* c, uint8_t** profile) {
  rd r = {buf, len, 0, false};
  const uint8_t* m = r_take(&r, 4);
  if (!m || memcmp(m, "PTX1", 4)) return 1;
  const uint16_t ver = r16(&r);
  r16(&r);
  if (r.bad) return 1;
  if (ver != VERSION && ver != 1) return ver > VERSION ? 2 : 1;
  memset(c, 0, sizeof *c);
  *profile = NULL;
  c->next_txn = r32(&r);
  /* version 1 had no watermark and no screen could show a result: every result it kept counts as shown */
  c->revealed_txn = ver == 1 ? (c->next_txn > 0 ? c->next_txn - 1 : 0) : r32(&r);
  c->scene_seeded = r8(&r) != 0;
  get_rng(&r, &c->scene);
  const int nr = r8(&r);
  if (nr > PTX_RESULTS) return 1;
  c->n_results = nr;
  for (int i = 0; i < nr; i++) get_result(&r, &c->results[i]);
  c->pend = r8(&r);
  c->pend_kind = r8(&r);
  c->pend_txn = r32(&r);
  c->req.machine = r8(&r);
  c->req.option = r8(&r);
  c->req.seed_len = r8(&r);
  c->req.timestamp_ms = r64(&r);
  const uint8_t* sd = r_take(&r, PTX_SEED_MAX);
  if (sd) memcpy(c->req.seed, sd, PTX_SEED_MAX);
  c->claim_record = r32(&r);
  c->pre_hash = r64(&r);
  c->post_hash = r64(&r);
  c->profile_len = r32(&r);
  if (r.bad) return 1;
  /* every field must make sense, not only parse */
  if (c->next_txn < 1 || c->revealed_txn >= c->next_txn) return 1;
  uint32_t last = 0;
  for (int i = 0; i < nr; i++) {
    const ptx_result* x = &c->results[i];
    if (x->txn == 0 || x->txn >= c->next_txn || x->txn <= last) return 1;
    last = x->txn;
    if (x->kind != PTX_KIND_PULL && x->kind != PTX_KIND_CLAIM) return 1;
    if (x->outcome != PTX_R_DONE && x->outcome != PTX_R_REFUSED) return 1;
    if (!memchr(x->key, 0, PTX_KEY_LEN)) return 1;
    if (x->n > PTX_MAX_EGGS) return 1;
    for (int j = 0; j < x->n; j++)
      if (x->order[j] >= x->n) return 1;
  }
  if (c->pend > PEND_COMMITTED) return 1;
  if (c->pend != PEND_NONE) {
    if (c->pend_kind != PTX_KIND_PULL && c->pend_kind != PTX_KIND_CLAIM) return 1;
    if (c->pend_txn == 0 || c->pend_txn >= c->next_txn) return 1;
    /* prepared: a transaction not decided yet, newer than every kept result; committed: its result is the newest one kept */
    if (c->pend == PEND_PREPARED ? c->pend_txn <= last : (nr == 0 || c->pend_txn != last || c->results[nr - 1].kind != c->pend_kind)) return 1;
    if (c->pend_kind == PTX_KIND_PULL) {
      if (c->req.machine > 2 || c->req.option > 4 || c->req.seed_len < 1 || c->req.seed_len > PTX_SEED_MAX) return 1;
      if (!(c->req.timestamp_ms <= (uint64_t)TS_MAX)) return 1;
    } else if (c->claim_record == 0) {
      return 1;
    }
    if (c->pend == PEND_COMMITTED) {
      if (c->profile_len == 0 || c->profile_len > PROFILE_MAX || c->profile_len > len - r.i) return 1;
      if (c->scene_seeded == false && c->pend_kind == PTX_KIND_PULL) return 1;
    } else if (c->profile_len != 0) {
      return 1;
    }
  } else if (c->profile_len != 0) {
    return 1;
  }
  if (c->pend == PEND_COMMITTED) {
    const uint8_t* b = r_take(&r, c->profile_len);
    if (!b) return 1;
    *profile = malloc(c->profile_len);
    if (!*profile) return 1;
    memcpy(*profile, b, c->profile_len);
  }
  if (r.i != len) {
    free(*profile);
    *profile = NULL;
    return 1;
  }
  return 0;
}

/* ------------------------------------------------------------------ the file */

static void unusable(const char* what) {
  snprintf(S.why, sizeof S.why, "The purchase record %s. It is left as it is, and no purchase can be made until it is removed.", what);
  memset(&P, 0, sizeof P);
  free(g_profile);
  g_profile = NULL;
  S.state = PTX_UNUSABLE;
}

static void load(void) {
  S.loaded = true;
  S.why[0] = 0;
  char* buf = NULL;
  size_t len = 0;
  bool used_backup = false;
  const save_result r = do_read(&buf, &len, &used_backup);
  memset(&P, 0, sizeof P);
  free(g_profile);
  g_profile = NULL;
  if (r == SAVE_NOT_FOUND) {
    P.next_txn = 1;
    S.state = PTX_NEW;
    return;
  }
  if (r != SAVE_OK) {
    unusable(r == SAVE_CORRUPT ? "is damaged (it failed its checksum)" : "could not be read from the SD card");
    return;
  }
  const int d = decode((const uint8_t*)buf, len, &P, &g_profile);
  free(buf);
  if (d == 0) {
    S.state = PTX_LOADED;
    return;
  }
  unusable(d == 2 ? "was made by a different version of the game" : "is damaged");
}

static void ensure_loaded(void) {
  if (!S.loaded) load();
}

ptx_state ptx_get_state(char why[PTX_WHY_LEN]) {
  ensure_loaded();
  if (why) snprintf(why, PTX_WHY_LEN, "%s", S.why);
  return S.fatal ? PTX_UNUSABLE : S.state;
}

static bool usable(void) {
  ensure_loaded();
  return S.state != PTX_UNUSABLE && !S.fatal;
}

/* Writes N (with the candidate bytes if it is committed). Only a successful write makes it the state: P takes N's fields; the candidate
 * buffer is taken over by the caller when N is committed, freed when it is not. */
static bool write_core(const core* c, const uint8_t* profile) {
  size_t n = 0;
  uint8_t* buf = encode(c, profile, &n);
  if (!buf) return false;
  const bool ok = do_write(buf, n) == SAVE_OK;
  free(buf);
  return ok;
}

static void sow_text(pr_rng* g, const uint8_t* text, size_t n) {
  pr_seedstr s;
  memset(&s, 0, sizeof s);
  for (size_t i = 0; i < n && i < 64; i++) s.u[i] = text[i];
  s.len = n < 64 ? n : 64;
  pr_rng_sow(g, &s);
}

void ptx_set_scene_seed(const char* ascii) {
  S.have_seed = ascii && ascii[0];
  snprintf(S.seed, sizeof S.seed, "%s", ascii ? ascii : "");
}

/* ------------------------------------------------------------------ roll-forward */

static bool settle_inner(void) {
  if (P.pend != PEND_COMMITTED) return true;
  if (P.pend_kind == PTX_KIND_CLAIM) {
    const cs_claim_outcome o = cs_claim_reward(P.claim_record, NULL);   /* idempotent by record: a second call is a replay */
    if (o != CS_CLAIM_DONE && o != CS_CLAIM_REPLAYED) {
      if (o == CS_CLAIM_FILE_UNUSABLE || o == CS_CLAIM_NOT_OWED || o == CS_CLAIM_UNKNOWN)
        snprintf(S.why, sizeof S.why, "A reward is waiting to be added, but the custom content file cannot be used. Nothing was lost.");
      return false;
    }
  }
  if (!sysd_write_bytes(g_profile, P.profile_len)) return false;
  memcpy(&N, &P, sizeof N);
  N.pend = PEND_NONE;
  N.pend_kind = 0;
  N.pend_txn = 0;
  memset(&N.req, 0, sizeof N.req);
  N.claim_record = 0;
  N.pre_hash = N.post_hash = 0;
  N.profile_len = 0;
  if (!write_core(&N, NULL)) return false;
  memcpy(&P, &N, sizeof P);
  free(g_profile);
  g_profile = NULL;
  return true;
}

bool ptx_settle(void) {
  if (!usable()) return false;
  if (P.pend != PEND_COMMITTED) return true;
  if (S.in_settle) return true;
  S.in_settle = true;
  const bool ok = settle_inner();
  S.in_settle = false;
  return ok;
}

/* The gate on sysd_save: while a committed transaction is not rolled forward, nothing else may write the profile. */
static bool gate(void) {
  if (!S.loaded || S.state == PTX_UNUSABLE || P.pend != PEND_COMMITTED) return true;
  return ptx_settle();
}

ptx_status ptx_recover(void) {
  if (!usable()) return PTX_E_UNUSABLE;
  if (P.pend != PEND_COMMITTED) return PTX_OK;
  /* A crash (or a failed write) left a committed transaction half applied. Memory has nothing of it: decide from the card's own profile.
   *   the card holds the profile this transaction started from -> install the candidate (memory, then the card)
   *   the card holds the candidate                                 -> it was applied; finish the rest
   *   anything else                                                -> something else changed it: do not guess, report, change nothing */
  char why[SYSD_WHY_LEN];
  if (sysd_state(why) == SYSD_UNREADABLE) {
    snprintf(S.why, sizeof S.why, "A purchase is waiting to be finished, but the system data cannot be read. Nothing was lost.");
    return PTX_E_UNUSABLE;
  }
  uint64_t d = 0;
  if (!sysd_card_digest(&d)) return PTX_E_UNUSABLE;
  if (d == P.pre_hash && d != P.post_hash) {
    if (!sysd_restore(g_profile, P.profile_len)) {
      S.fatal = true;
      snprintf(S.why, sizeof S.why, "A purchase could not be finished: its profile does not load. Nothing was changed on the card.");
      return PTX_E_UNUSABLE;
    }
  } else if (d != P.post_hash) {
    S.state = PTX_UNUSABLE;
    snprintf(S.why, sizeof S.why, "The system data was changed while a purchase was waiting to be finished. Both are left as they are.");
    return PTX_E_UNUSABLE;
  }
  ptx_settle();   /* a failure leaves it committed: the gate and the next boot retry */
  return PTX_OK;
}

ptx_status ptx_boot(void) {
  sysd_set_gate(gate);
  return ptx_recover();
}

/* ------------------------------------------------------------------ transactions */

static const ptx_result* find_result(uint32_t txn) {
  for (int i = 0; i < P.n_results; i++)
    if (P.results[i].txn == txn) return &P.results[i];
  return NULL;
}

bool ptx_result_of(uint32_t txn, ptx_result* out) {
  if (!usable()) return false;
  const ptx_result* x = find_result(txn);
  if (!x) return false;
  if (out) *out = *x;
  return true;
}

uint32_t ptx_pending(void) {
  if (!usable()) return 0;
  return P.pend == PEND_NONE ? 0 : P.pend_txn;
}

bool ptx_apply_pending(void) {
  if (!usable()) return false;
  return P.pend == PEND_COMMITTED;
}

/* The oldest kept pull that was committed and not shown yet (a watermark: txn > revealed_txn). */
static const ptx_result* unrevealed(void) {
  for (int i = 0; i < P.n_results; i++) {
    const ptx_result* x = &P.results[i];
    if (x->txn > P.revealed_txn && x->kind == PTX_KIND_PULL && x->outcome == PTX_R_DONE) return x;
  }
  return NULL;
}

uint32_t ptx_unrevealed(ptx_result* out) {
  if (!usable()) return 0;
  const ptx_result* x = unrevealed();
  if (!x) return 0;
  if (out) *out = *x;
  return x->txn;
}

bool ptx_mark_revealed(uint32_t txn) {
  if (!usable()) return false;
  const ptx_result* x = find_result(txn);
  if (!x || x->kind != PTX_KIND_PULL || x->outcome != PTX_R_DONE) return false;
  if (txn <= P.revealed_txn) return true;   /* already marked: nothing to write */
  memcpy(&N, &P, sizeof N);
  N.revealed_txn = txn;
  /* a committed transaction that is not rolled forward yet keeps its candidate bytes in this write */
  if (!write_core(&N, P.pend == PEND_COMMITTED ? g_profile : NULL)) return false;
  memcpy(&P, &N, sizeof P);
  return true;
}

static ptx_status profile_ready(void) {
  char why[SYSD_WHY_LEN];
  if (sysd_state(why) == SYSD_UNREADABLE) {
    snprintf(S.why, sizeof S.why, "The system data cannot be read, so it is not used as a wallet.");
    return PTX_E_UNUSABLE;
  }
  return PTX_OK;
}

/* The digest of the profile as it is now: recorded with the request, so a request that outlives a change of the profile (a roll-back to
 * the backup of this file, a run played in between) is refused instead of being run on a profile it was not made for. */
static ptx_status profile_digest(uint64_t* d) {
  uint8_t* buf = malloc(PROFILE_MAX);
  if (!buf) return PTX_E_NO_MEMORY;
  const size_t n = sysd_serialize(buf, bound());
  if (n) *d = sysd_digest(buf, n);
  free(buf);
  return n ? PTX_OK : PTX_E_IO;
}

static ptx_status begin_common(uint8_t kind, uint32_t* txn) {
  if (!usable()) return PTX_E_UNUSABLE;
  if (P.pend != PEND_NONE) return PTX_E_BUSY;
  if (P.next_txn == UINT32_MAX) return PTX_E_UNUSABLE;
  memcpy(&N, &P, sizeof N);
  N.pend = PEND_PREPARED;
  N.pend_kind = kind;
  N.pend_txn = N.next_txn++;
  *txn = N.pend_txn;
  return PTX_OK;
}

ptx_status ptx_begin_pull(const ptx_pull_request* req, uint32_t* txn) {
  if (txn) *txn = 0;
  if (!req || req->machine > 2 || req->option > 4 || req->seed_len < 1 || req->seed_len > PTX_SEED_MAX ||
      !(req->timestamp_ms <= (uint64_t)TS_MAX))
    return PTX_E_BAD_REQUEST;
  if (!usable()) return PTX_E_UNUSABLE;
  ptx_status st = profile_ready();
  if (st != PTX_OK) return st;
  if (P.pend != PEND_NONE) return PTX_E_BUSY;
  if (!P.scene_seeded && !S.have_seed) return PTX_E_NO_SEED;
  uint64_t d = 0;
  st = profile_digest(&d);
  if (st != PTX_OK) return st;
  uint32_t id = 0;
  st = begin_common(PTX_KIND_PULL, &id);
  if (st != PTX_OK) return st;
  N.pre_hash = d;
  memset(&N.req, 0, sizeof N.req);
  N.req = *req;
  memset(N.req.seed + req->seed_len, 0, PTX_SEED_MAX - req->seed_len);
  if (!N.scene_seeded) {
    sow_text(&N.scene, (const uint8_t*)S.seed, strlen(S.seed));   /* supplied once, saved with this write */
    N.scene_seeded = true;
  }
  if (!write_core(&N, NULL)) return PTX_E_IO;
  memcpy(&P, &N, sizeof P);
  if (txn) *txn = id;
  return PTX_OK;
}

ptx_status ptx_begin_claim(cc_record_id record, uint32_t* txn) {
  if (txn) *txn = 0;
  if (record == 0) return PTX_E_BAD_REQUEST;
  if (!usable()) return PTX_E_UNUSABLE;
  ptx_status st = profile_ready();
  if (st != PTX_OK) return st;
  if (P.pend != PEND_NONE) return PTX_E_BUSY;
  const cs_claim_outcome o = cs_reward_state(record);
  if (o == CS_CLAIM_FILE_UNUSABLE) return PTX_E_UNUSABLE;
  if (o != CS_CLAIM_OWED) return PTX_E_NOT_OWED;
  uint64_t d = 0;
  st = profile_digest(&d);
  if (st != PTX_OK) return st;
  uint32_t id = 0;
  st = begin_common(PTX_KIND_CLAIM, &id);
  if (st != PTX_OK) return st;
  N.pre_hash = d;
  memset(&N.req, 0, sizeof N.req);
  N.claim_record = record;
  if (!write_core(&N, NULL)) return PTX_E_IO;
  memcpy(&P, &N, sizeof P);
  if (txn) *txn = id;
  return PTX_OK;
}

static void push_result(core* c, const ptx_result* x) {
  if (c->n_results == PTX_RESULTS) {
    memmove(&c->results[0], &c->results[1], sizeof(ptx_result) * (PTX_RESULTS - 1));
    c->n_results--;
  }
  c->results[c->n_results++] = *x;
}

static void clear_pending(core* c) {
  c->pend = PEND_NONE;
  c->pend_kind = 0;
  c->pend_txn = 0;
  memset(&c->req, 0, sizeof c->req);
  c->claim_record = 0;
  c->pre_hash = c->post_hash = 0;
  c->profile_len = 0;
}

/* The refusal path: nothing but the pending record changes (the profile and the scene RNG are exactly as they were). */
static ptx_exec finish_refused(const char* key, uint8_t* pre, uint8_t* post, ptx_result* out) {
  ptx_result x;
  memset(&x, 0, sizeof x);
  x.txn = P.pend_txn;
  x.kind = P.pend_kind;
  x.outcome = PTX_R_REFUSED;
  x.record = P.pend_kind == PTX_KIND_CLAIM ? P.claim_record : 0;
  snprintf(x.key, sizeof x.key, "%s", key && key[0] ? key : "unknown");
  memcpy(&N, &P, sizeof N);
  push_result(&N, &x);
  clear_pending(&N);
  free(pre);
  free(post);
  if (!write_core(&N, NULL)) return PTX_X_IO;
  memcpy(&P, &N, sizeof P);
  if (out) *out = x;
  return PTX_X_REFUSED;
}

/* Puts the live profile back from the bytes it was serialized to before the pull (the bytes a restart would read). */
static bool put_back(const uint8_t* pre, size_t n) { return sysd_restore(pre, n); }

static ptx_exec commit_and_settle(ptx_result* res, uint8_t* pre, size_t n_pre, uint8_t* post, size_t n_post, const pr_rng* scene, ptx_result* out) {
  memcpy(&N, &P, sizeof N);
  push_result(&N, res);
  N.pend = PEND_COMMITTED;
  N.pre_hash = sysd_digest(pre, n_pre);
  N.post_hash = sysd_digest(post, n_post);
  N.profile_len = (uint32_t)n_post;
  if (scene) {
    N.scene = *scene;
    N.scene_seeded = true;
  }
  if (!write_core(&N, post)) {
    const bool back = put_back(pre, n_pre);
    free(pre);
    free(post);
    if (!back) {
      S.fatal = true;
      snprintf(S.why, sizeof S.why, "The purchase could not be saved and the system data could not be put back. Restart the game.");
      return PTX_X_FATAL;
    }
    return PTX_X_IO;
  }
  /* committed: the result is durable from here on */
  memcpy(&P, &N, sizeof P);
  free(g_profile);
  g_profile = post;
  free(pre);
  if (out) *out = *res;
  return ptx_settle() ? PTX_X_DONE : PTX_X_DONE_APPLY_PENDING;
}

ptx_exec ptx_execute(uint32_t txn, ptx_result* out) {
  if (!usable()) return PTX_X_UNUSABLE;
  /* a transaction this file already decided: return it again, never run it again */
  const ptx_result* done = find_result(txn);
  if (done) {
    if (out) *out = *done;
    if (done->outcome == PTX_R_REFUSED) return PTX_X_REFUSED;
    if (P.pend == PEND_COMMITTED && P.pend_txn == txn) return ptx_settle() ? PTX_X_DONE : PTX_X_DONE_APPLY_PENDING;
    return PTX_X_DONE;
  }
  if (P.pend != PEND_PREPARED || P.pend_txn != txn) return PTX_X_UNKNOWN;
  if (profile_ready() != PTX_OK) return PTX_X_UNUSABLE;

  uint8_t* pre = malloc(PROFILE_MAX);
  uint8_t* post = malloc(PROFILE_MAX);
  if (!pre || !post) {
    free(pre);
    free(post);
    return PTX_X_IO;
  }
  const size_t n_pre = sysd_serialize(pre, bound());
  if (!n_pre) {   /* a profile that cannot be serialized cannot be saved: nothing is done */
    free(pre);
    free(post);
    return PTX_X_IO;
  }
  if (sysd_digest(pre, n_pre) != P.pre_hash) return finish_refused("ptx:profileChanged", pre, post, out);
  gc_system* sys = sysd_get();
  ptx_result res;
  memset(&res, 0, sizeof res);
  res.txn = txn;
  res.kind = P.pend_kind;
  res.outcome = PTX_R_DONE;

  if (P.pend_kind == PTX_KIND_CLAIM) {
    if (cs_reward_state(P.claim_record) != CS_CLAIM_OWED) return finish_refused("cc:notOwed", pre, post, out);
    if (sys->voucher_counts[GC_VOUCHER_REGULAR] > UINT32_MAX - CC_REMATCH_REWARD_VOUCHERS)
      return finish_refused("profile:voucherCap", pre, post, out);
    gc_system_add_voucher_count(sys, GC_VOUCHER_REGULAR, (int)CC_REMATCH_REWARD_VOUCHERS);
    res.record = P.claim_record;
    res.vouchers = (uint8_t)CC_REMATCH_REWARD_VOUCHERS;
    const size_t n_post = sysd_serialize(post, bound());
    if (!n_post) {
      const bool back = put_back(pre, n_pre);
      free(pre);
      free(post);
      if (!back) {
        S.fatal = true;
        return PTX_X_FATAL;
      }
      return PTX_X_IO;
    }
    return commit_and_settle(&res, pre, n_pre, post, n_post, NULL, out);
  }

  /* a pull: the clock and the entropy seed are the ones saved at begin */
  gc_gacha_ctx ctx;
  memset(&ctx, 0, sizeof ctx);
  ctx.machine = P.req.machine;
  ctx.cursor = P.req.option;
  ctx.timestamp_ms = P.req.timestamp_ms;
  sow_text(&ctx.entropy, P.req.seed, P.req.seed_len);
  ctx.scene_rng = P.scene;
  const int before = sys->n_eggs;
  gc_gacha_result gr;
  memset(&gr, 0, sizeof gr);
  const char* why = NULL;
  if (!gc_system_gacha_pull(sys, &ctx, &gr, &why)) return finish_refused(why, pre, post, out);   /* atomic: profile and RNG unchanged */
  const int added = sys->n_eggs - before;
  const size_t n_post = sysd_serialize(post, bound());
  if (!n_post || added < 0 || added > PTX_MAX_EGGS || gr.n != added) {
    const bool back = put_back(pre, n_pre);
    free(pre);
    free(post);
    if (!back) {
      S.fatal = true;
      return PTX_X_FATAL;
    }
    return PTX_X_IO;
  }
  res.n = (uint8_t)added;
  for (int i = 0; i < added; i++) {
    res.eggs[i] = sys->eggs[before + i];
    res.order[i] = gr.order[i];
  }
  return commit_and_settle(&res, pre, n_pre, post, n_post, &ctx.scene_rng, out);
}

/* ------------------------------------------------------------------ for tests */

void ptx_set_io(const ptx_io* io) {
  S.have_io = io != NULL;
  if (io) S.io = *io;
}

void ptx_set_profile_cap(size_t cap) { S.cap = cap; }

void ptx_reset(void) {
  const bool have = S.have_io;
  const ptx_io io = S.io;
  const size_t cap = S.cap;
  free(g_profile);
  g_profile = NULL;
  memset(&S, 0, sizeof S);
  memset(&P, 0, sizeof P);
  S.have_io = have;
  S.io = io;
  S.cap = cap;
}

size_t ptx_static_bytes(void) { return sizeof P + sizeof N + sizeof S; }

uint64_t ptx_scene_digest(void) {
  ensure_loaded();
  if (!P.scene_seeded) return 0;
  uint8_t b[40];
  const double d[5] = {P.scene.c, P.scene.s0, P.scene.s1, P.scene.s2, P.scene.n};
  memcpy(b, d, sizeof b);
  return sysd_digest(b, sizeof b);
}
