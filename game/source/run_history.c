/* The run history on the SD card (see run_history.h). */
#include "run_history.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "save.h"

#define FILE_NAME "run_history"
#define MAGIC "RHI"
#define VERSION 1

static rh_entry g_e[RH_LIMIT];   /* oldest first; not on the stack: an entry is nearly a kilobyte */
static rh_entry g_scratch[RH_LIMIT]; /* a history being decoded or built: g_e changes only once it is whole */
static int g_n;

/* ------------------------------------------------------------------ the byte format */

typedef struct {
  uint8_t* p;
  size_t n, cap;
  bool bad;
} wbuf;

static void w8(wbuf* w, unsigned v) {
  if (w->n >= w->cap) {
    w->bad = true;
    return;
  }
  w->p[w->n++] = (uint8_t)v;
}
static void w16(wbuf* w, unsigned v) { w8(w, v & 0xff), w8(w, (v >> 8) & 0xff); }
static void w32(wbuf* w, uint32_t v) { w16(w, v & 0xffff), w16(w, v >> 16); }
static void w64(wbuf* w, uint64_t v) { w32(w, (uint32_t)v), w32(w, (uint32_t)(v >> 32)); }
static void wstr(wbuf* w, const char* s, size_t max) { /* a length byte, then the bytes (max includes the NUL) */
  const size_t len = strnlen(s, max); /* a string without its NUL inside the array is refused, not read past */
  if (len >= max) {
    w->bad = true;
    return;
  }
  w8(w, (unsigned)len);
  for (size_t i = 0; i < len; i++) w8(w, (uint8_t)s[i]);
}

typedef struct {
  const uint8_t* p;
  size_t n, i;
  bool bad;
} rbuf;

static unsigned r8(rbuf* r) {
  if (r->i >= r->n) {
    r->bad = true;
    return 0;
  }
  return r->p[r->i++];
}
static unsigned r16(rbuf* r) {
  const unsigned lo = r8(r);
  return lo | (r8(r) << 8);
}
static uint32_t r32(rbuf* r) {
  const uint32_t lo = r16(r);
  return lo | ((uint32_t)r16(r) << 16);
}
static uint64_t r64(rbuf* r) {
  const uint64_t lo = r32(r);
  return lo | ((uint64_t)r32(r) << 32);
}
static void rstr(rbuf* r, char* out, size_t max) {
  const size_t len = r8(r);
  if (len >= max || r->i + len > r->n) {
    r->bad = true;
    out[0] = 0;
    return;
  }
  memcpy(out, r->p + r->i, len);
  out[len] = 0;
  r->i += len;
  for (size_t i = 0; i < len; i++) /* text for a screen: no control characters */
    if ((uint8_t)out[i] < 0x20 || out[i] == 0x7f) r->bad = true;
}

static void put_mon(wbuf* w, const rh_mon* m) {
  w16(w, m->species);
  w8(w, m->form);
  w8(w, (uint8_t)m->gender);
  w8(w, (m->shiny ? 1u : 0u) | (m->fainted ? 2u : 0u) | (m->has_passive ? 4u : 0u));
  w8(w, m->variant);
  w32(w, m->level);
  w32(w, m->hp);
  w8(w, m->nature);
  w16(w, m->ability);
  w16(w, m->passive);
  w8(w, m->n_moves);
  for (int i = 0; i < 4; i++) w16(w, m->moves[i]);
  for (int i = 0; i < 4; i++) w8(w, m->pp_used[i]);
  for (int i = 0; i < 4; i++) w8(w, m->pp_max[i]);
  w8(w, (uint8_t)m->types[0]);
  w8(w, (uint8_t)m->types[1]);
  for (int i = 0; i < 6; i++) w32(w, m->stats[i]);
  for (int i = 0; i < 6; i++) w8(w, m->ivs[i]);
  w8(w, m->friendship);
  w8(w, (uint8_t)m->tera_type);
  w8(w, m->luck);
}

static void get_mon(rbuf* r, rh_mon* m) {
  memset(m, 0, sizeof *m);
  m->species = (uint16_t)r16(r);
  m->form = (uint8_t)r8(r);
  m->gender = (int8_t)r8(r);
  const unsigned flags = r8(r);
  m->shiny = flags & 1u;
  m->fainted = flags & 2u;
  m->has_passive = flags & 4u;
  if (flags > 7u) r->bad = true;
  m->variant = (uint8_t)r8(r);
  m->level = r32(r);
  m->hp = r32(r);
  m->nature = (uint8_t)r8(r);
  m->ability = (uint16_t)r16(r);
  m->passive = (uint16_t)r16(r);
  m->n_moves = (uint8_t)r8(r);
  if (m->n_moves > 4) r->bad = true;
  for (int i = 0; i < 4; i++) m->moves[i] = (uint16_t)r16(r);
  for (int i = 0; i < 4; i++) m->pp_used[i] = (uint8_t)r8(r);
  for (int i = 0; i < 4; i++) m->pp_max[i] = (uint8_t)r8(r);
  m->types[0] = (int8_t)r8(r);
  m->types[1] = (int8_t)r8(r);
  for (int i = 0; i < 6; i++) m->stats[i] = r32(r);
  for (int i = 0; i < 6; i++) m->ivs[i] = (uint8_t)r8(r);
  m->friendship = (uint8_t)r8(r);
  m->tera_type = (int8_t)r8(r);
  m->luck = (uint8_t)r8(r);
}

static void put_entry(wbuf* w, const rh_entry* e) {
  w64(w, (uint64_t)e->time);
  w8(w, e->victory ? 1u : 0u);
  w8(w, (unsigned)e->mode);
  w32(w, (uint32_t)e->wave);
  w16(w, (unsigned)e->biome);
  w32(w, (uint32_t)e->score);
  w64(w, (uint64_t)e->money);
  if (e->n_party > RH_PARTY || e->n_enemy > RH_ENEMIES) w->bad = true;
  w8(w, e->n_party);
  for (int i = 0; i < e->n_party && i < RH_PARTY; i++) put_mon(w, &e->party[i]);
  w8(w, e->defeat);
  w8(w, e->n_enemy);
  for (int i = 0; i < e->n_enemy && i < RH_ENEMIES; i++) put_mon(w, &e->enemy[i]);
  w8(w, (e->trainer_double ? 1u : 0u) | (e->trainer_female ? 2u : 0u));
  wstr(w, e->trainer, sizeof e->trainer);
  wstr(w, e->name, sizeof e->name);
}

static void get_entry(rbuf* r, rh_entry* e) {
  memset(e, 0, sizeof *e);
  e->time = (int64_t)r64(r);
  const unsigned v = r8(r);
  if (v > 1) r->bad = true;
  e->victory = v == 1;
  e->mode = (int)r8(r);
  e->wave = (int)r32(r);
  e->biome = (int16_t)r16(r);
  e->score = (int)r32(r);
  e->money = (int64_t)r64(r);
  e->n_party = (uint8_t)r8(r);
  if (e->n_party > RH_PARTY) {
    r->bad = true;
    return;
  }
  for (int i = 0; i < e->n_party; i++) get_mon(r, &e->party[i]);
  e->defeat = (uint8_t)r8(r);
  if (e->defeat > RH_DEFEAT_RIVAL) r->bad = true;
  e->n_enemy = (uint8_t)r8(r);
  if (e->n_enemy > RH_ENEMIES) {
    r->bad = true;
    return;
  }
  for (int i = 0; i < e->n_enemy; i++) get_mon(r, &e->enemy[i]);
  const unsigned tf = r8(r);
  if (tf > 3u) r->bad = true;
  e->trainer_double = tf & 1u;
  e->trainer_female = tf & 2u;
  rstr(r, e->trainer, sizeof e->trainer);
  rstr(r, e->name, sizeof e->name);
}

size_t rhist_encode(const rh_entry* e, int n, uint8_t* out, size_t cap) {
  if (n < 0 || n > RH_LIMIT) return 0;
  wbuf w = {out, 0, cap, false};
  for (const char* m = MAGIC; *m; m++) w8(&w, (uint8_t)*m);
  w8(&w, VERSION);
  w8(&w, (unsigned)n);
  w16(&w, 0); /* reserved */
  for (int i = 0; i < n; i++) put_entry(&w, &e[i]);
  return w.bad ? 0 : w.n;
}

int rhist_decode(const uint8_t* buf, size_t len, rh_entry* out, int cap) {
  rbuf r = {buf, len, 0, false};
  for (const char* m = MAGIC; *m; m++)
    if (r8(&r) != (uint8_t)*m) return -1;
  if (r8(&r) != VERSION) return -1;
  const int n = (int)r8(&r);
  if (r16(&r) != 0 || n > RH_LIMIT || n > cap || r.bad) return -1;
  for (int i = 0; i < n; i++) {
    get_entry(&r, &out[i]);
    if (r.bad) return -1;
  }
  if (r.i != r.n) return -1; /* nothing after the last entry */
  return n;
}

/* ------------------------------------------------------------------ the file */

rh_status rhist_load(void) {
  char* data = NULL;
  size_t len = 0;
  bool backup = false;
  const save_result sr = save_read(FILE_NAME, &data, &len, &backup);
  g_n = 0;
  if (sr == SAVE_NOT_FOUND) return RH_OK;
  if (sr != SAVE_OK) return RH_UNREADABLE;
  rh_status st = RH_OK;
  const int n = len <= RH_FILE_MAX ? rhist_decode((const uint8_t*)data, len, g_scratch, RH_LIMIT) : -1;
  if (n < 0) {
    st = RH_UNREADABLE;
  } else {
    memcpy(g_e, g_scratch, sizeof(rh_entry) * (size_t)n);
    g_n = n;
  }
  free(data);
  return st;
}

int rhist_count(void) { return g_n; }

const rh_entry* rhist_get(int i) { return i >= 0 && i < g_n ? &g_e[g_n - 1 - i] : NULL; }

bool rhist_add(const rh_entry* e, rh_status* status) {
  const rh_status st = rhist_load();
  if (status) *status = st;
  if (st != RH_OK) return false;
  /* the new history in the scratch array: the copy changes only once the file is written */
  rh_entry* next = g_scratch;
  int n = 0;
  const int skip = g_n >= RH_LIMIT ? g_n - (RH_LIMIT - 1) : 0; /* upstream drops the oldest while the history is at its limit */
  for (int i = skip; i < g_n; i++) next[n++] = g_e[i];
  next[n++] = *e;
  uint8_t* buf = (uint8_t*)malloc(RH_FILE_MAX);
  if (!buf) return false;
  const size_t len = rhist_encode(next, n, buf, RH_FILE_MAX);
  bool ok = false;
  if (len > 0 && save_write(FILE_NAME, buf, len) == SAVE_OK) {
    memcpy(g_e, next, sizeof(rh_entry) * (size_t)n);
    g_n = n;
    ok = true;
  }
  free(buf);
  return ok;
}

/* ------------------------------------------------------------------ the screens' view */

void rhist_as_info(const rh_entry* e, rsv_info* out) {
  memset(out, 0, sizeof *out);
  out->status = RSV_OK;
  out->mode = e->mode;
  out->wave = e->wave;
  out->biome = e->biome;
  out->score = e->score;
  out->money = (double)e->money;
  out->n_party = e->n_party < 6 ? e->n_party : 6;
  snprintf(out->name, sizeof out->name, "%s", e->name);
  for (int i = 0; i < out->n_party; i++) {
    const rh_mon* m = &e->party[i];
    out->party[i] = (rsv_mon){m->species, m->form, m->gender, m->shiny, m->variant, (int32_t)m->level, m->fainted};
  }
}
