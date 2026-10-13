/* The custom content metadata on the SD card (see custom_store.h). */
#include "custom_store.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static cc_meta g_live;   /* what the SD card holds */
static cc_meta g_stage;  /* a transaction built aside: it replaces g_live only after its write succeeded */
static uint8_t g_buf[CC_WIRE_META_MAX];
static struct {
  bool loaded;
  cs_state state;
  bool from_backup;
  char why[CS_WHY_LEN];
  uint32_t nonce;
  cs_io io;
  bool have_io;
} S;

static save_result do_read(char** out, size_t* len, bool* used_backup) {
  return S.have_io ? S.io.read(CS_FILE_NAME, out, len, used_backup) : save_read(CS_FILE_NAME, out, len, used_backup);
}

static save_result do_write(const void* data, size_t len) {
  return S.have_io ? S.io.write(CS_FILE_NAME, data, len) : save_write(CS_FILE_NAME, data, len);
}

static void unreadable(const char* what) {
  snprintf(S.why, sizeof S.why, "The custom content file %s. It is left as it is, and nothing is saved to it until it is removed.", what);
  memset(&g_live, 0, sizeof g_live);
  S.state = CS_UNREADABLE;
}

static void load(void) {
  S.loaded = true;
  S.why[0] = 0;
  S.from_backup = false;
  char* buf = NULL;
  size_t len = 0;
  bool used_backup = false;
  const save_result r = do_read(&buf, &len, &used_backup);
  if (r == SAVE_NOT_FOUND) {
    cc_meta_init(&g_live, S.nonce ? S.nonce : 1u);
    S.state = CS_NEW;
    return;
  }
  if (r == SAVE_OK) {
    const cc_status s = cc_meta_decode((const uint8_t*)buf, len, &g_live);
    free(buf);
    if (s == CC_OK) {
      S.state = CS_LOADED;
      S.from_backup = used_backup;
      return;
    }
    if (s == CC_E_VERSION)
      unreadable("was made by a different version of the game");
    else {
      char what[64];
      snprintf(what, sizeof what, "is damaged (%.40s)", cc_status_str(s));
      unreadable(what);
    }
    return;
  }
  if (r == SAVE_CORRUPT)
    unreadable("is damaged (it failed its checksum)");
  else
    unreadable("could not be read from the SD card");
}

static void ensure_loaded(void) {
  if (!S.loaded) load();
}

cs_state cs_get_state(char why[CS_WHY_LEN]) {
  ensure_loaded();
  if (why) snprintf(why, CS_WHY_LEN, "%s", S.why);
  return S.state;
}

bool cs_from_backup(void) {
  ensure_loaded();
  return S.from_backup;
}

const cc_meta* cs_meta(void) {
  ensure_loaded();
  return S.state == CS_UNREADABLE ? NULL : &g_live;
}

void cs_set_nonce(uint32_t nonce) { S.nonce = nonce ? nonce : 1u; }

/* Writes g_stage; only a successful write makes it the state. */
static bool write_stage(void) {
  size_t n = 0;
  if (cc_meta_encode(&g_stage, g_buf, sizeof g_buf, &n) != CC_OK) return false;
  if (do_write(g_buf, n) != SAVE_OK) return false;
  memcpy(&g_live, &g_stage, sizeof g_live);
  S.state = CS_LOADED;
  S.from_backup = false;
  return true;
}

cc_run_uid cs_begin_run(void) {
  ensure_loaded();
  if (S.state == CS_UNREADABLE) return 0;
  memcpy(&g_stage, &g_live, sizeof g_stage);
  cc_run_uid uid = 0;
  if (cc_alloc_run_uid(&g_stage.counters, &uid) != CC_OK) return 0;
  return write_stage() ? uid : 0;   /* the counter is on the card before anyone sees the uid */
}

/* Builds the terminal summary of a report, with what a defeat's attribution adds. */
static bool build_terminal(const co_report* r, const cs_defeat* d, cc_terminal* t) {
  memset(t, 0, sizeof *t);
  t->run_uid = r->run_uid;
  t->outcome = r->outcome;
  if (r->biome < 0 || r->biome > CC_BIOME_ID_MAX || r->wave < 1 || (r->outcome != CC_OUT_DEFEAT && r->outcome != CC_OUT_CLEAR)) return false;
  t->biome = (uint8_t)r->biome;
  t->wave = r->wave;
  if (d && r->outcome == CC_OUT_DEFEAT) {
    t->finish_actor = d->finish_actor;
    t->known |= CC_TK_FINISH_ACTOR;
    if (d->has_finish_move) {
      t->finish_move = d->finish_move;
      t->known |= CC_TK_FINISH_MOVE;
    }
    if (d->has_fallen) {
      t->fallen_actor = d->fallen_actor;
      t->known |= CC_TK_FALLEN_ACTOR;
    }
  }
  return true;
}

cs_result cs_commit_defeat(const co_report* r, const cs_defeat* d) {
  cs_result out;
  memset(&out, 0, sizeof out);
  if (!r || !r->valid) {
    out.outcome = CS_BAD_REPORT;
    out.status = CC_E_NULL;
    return out;
  }
  if (!r->uid_durable) {
    out.outcome = CS_NO_DURABLE_RUN;
    return out;
  }
  ensure_loaded();
  if (S.state == CS_UNREADABLE) {
    out.outcome = CS_FILE_UNUSABLE;
    return out;
  }
  /* what the app can say about how the run ended: the outcome, where, and the last turns. Who finished it stays unknown unless the
   * caller established it (a custom run's faint hook): it is never guessed here. */
  cc_terminal t;
  if (!build_terminal(r, d, &t)) {
    out.outcome = CS_BAD_REPORT;
    out.status = CC_E_RANGE;
    return out;
  }
  cc_nemesis cand;
  const cc_nemesis* pc = NULL;
  if (d && r->outcome == CC_OUT_DEFEAT) {
    cand = d->candidate;
    cand.record_id = 0;
    cand.life = CC_LIFE_STORED;
    cand.source_run_uid = r->run_uid;
    cand.source_wave = r->wave;
    cand.source_biome = (uint8_t)r->biome;
    pc = &cand;
  }
  memcpy(&g_stage, &g_live, sizeof g_stage);
  cc_status st = cc_commit_terminal(&g_stage, &t, &r->history, pc, &out.commit);
  if (st != CC_OK && pc) {
    /* a candidate that does not validate must not cost the player the report: store the run with an unknown cause, and say so */
    cc_terminal plain;
    build_terminal(r, NULL, &plain);
    memcpy(&g_stage, &g_live, sizeof g_stage);
    memset(&out.commit, 0, sizeof out.commit);
    const cc_status again = cc_commit_terminal(&g_stage, &plain, &r->history, NULL, &out.commit);
    if (again == CC_OK) {
      out.commit.refusal = CC_REFUSE_NOT_ATTRIBUTED;
      out.status = st;   /* why the candidate was dropped */
      st = CC_OK;
    }
  }
  if (st != CC_OK) {
    out.status = st;
    out.outcome = CS_BAD_REPORT;
    return out;
  }
  if (out.commit.replayed) {
    out.outcome = CS_REPLAYED;
    return out;
  }
  out.outcome = write_stage() ? CS_STORED : CS_WRITE_FAILED;
  return out;
}

cs_result cs_commit(const co_report* r) { return cs_commit_defeat(r, NULL); }

cs_tx_outcome cs_txn(cs_txn_fn fn, void* ctx, cc_status* status) {
  cc_status dummy;
  if (!status) status = &dummy;
  *status = CC_OK;
  ensure_loaded();
  if (S.state == CS_UNREADABLE) return CS_TX_UNUSABLE;
  memcpy(&g_stage, &g_live, sizeof g_stage);
  *status = fn(&g_stage, ctx);
  if (*status != CC_OK) return CS_TX_REFUSED;
  *status = cc_meta_validate(&g_stage);
  if (*status != CC_OK) return CS_TX_REFUSED;
  if (!memcmp(&g_stage, &g_live, sizeof g_stage)) return CS_TX_UNCHANGED;
  return write_stage() ? CS_TX_DONE : CS_TX_WRITE_FAILED;
}

static const cc_nemesis* find_record(const cc_meta* m, cc_record_id id) {
  for (unsigned i = 0; i < m->roster.count; i++)
    if (m->roster.rec[i].record_id == id) return &m->roster.rec[i];
  return NULL;
}

cs_claim_outcome cs_reward_state(cc_record_id id) {
  ensure_loaded();
  if (S.state == CS_UNREADABLE) return CS_CLAIM_FILE_UNUSABLE;
  const cc_nemesis* n = find_record(&g_live, id);
  if (!n) return CS_CLAIM_UNKNOWN;
  cc_reward_entitlement e;
  if (cc_reward_entitlement_of(n, &e) != CC_OK) return CS_CLAIM_NOT_OWED;
  if (e.state == CC_RW_OWED) return CS_CLAIM_OWED;
  return e.state == CC_RW_CLAIMED ? CS_CLAIM_REPLAYED : CS_CLAIM_NOT_OWED;
}

cs_claim_outcome cs_claim_reward(cc_record_id id, cc_txn_id* txn) {
  if (txn) *txn = 0;
  const cs_claim_outcome st = cs_reward_state(id);
  if (st == CS_CLAIM_REPLAYED) {
    if (txn) *txn = find_record(&g_live, id)->reward_txn;
    return CS_CLAIM_REPLAYED;
  }
  if (st != CS_CLAIM_OWED) return st;
  memcpy(&g_stage, &g_live, sizeof g_stage);
  cc_claim_result res;
  memset(&res, 0, sizeof res);
  if (cc_reward_claim(&g_stage, id, &res) != CC_OK) return CS_CLAIM_NOT_OWED;
  if (!write_stage()) return CS_CLAIM_WRITE_FAILED;
  if (txn) *txn = res.txn_id;
  return CS_CLAIM_DONE;
}

const char* cs_note(cs_outcome o) {
  switch (o) {
    case CS_STORED:
    case CS_REPLAYED:
      return NULL;
    case CS_NO_DURABLE_RUN:
    case CS_FILE_UNUSABLE:
      return "This run's report is not kept (the custom content file is unusable).";
    case CS_BAD_REPORT:
      return "This run's report could not be stored.";
    case CS_WRITE_FAILED:
      return "This run's report could not be saved to the SD card.";
  }
  return NULL;
}

void cs_set_io(const cs_io* io) {
  S.have_io = io != NULL;
  if (io) S.io = *io;
}

void cs_reset(void) {
  const bool have = S.have_io;
  const cs_io io = S.io;
  const uint32_t nonce = S.nonce;
  memset(&S, 0, sizeof S);
  S.have_io = have;
  S.io = io;
  S.nonce = nonce;
  memset(&g_live, 0, sizeof g_live);
}

size_t cs_static_bytes(void) { return sizeof g_live + sizeof g_stage + sizeof g_buf + sizeof S; }
