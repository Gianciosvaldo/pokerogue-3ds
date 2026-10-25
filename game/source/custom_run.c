/* The optional Nemesis rematch of a custom run (see custom_run.h). */
#include "custom_run.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cc/codec.h"
#include "cc/scheduler.h"
#include "profile_tx.h"
#include "save.h"
#include "system_data.h"

/* ------------------------------------------------------------------ state */

typedef struct {
  bool valid;
  gc_custom_actor src, fallen;
  uint16_t move;
  int wave, turn;
  bool direct, source_enemy;
} cand_t;

static struct {
  bool active;
  cc_run_uid run_uid;
  cc_schedule sched;
  bool forced_done;          /* the overlay is older than the roster: the rematch was won after the save, nothing more is offered */
  bool degraded;             /* the roster could not be written: no more offers this run */
  /* the battle on the field */
  uint16_t enc_wave;
  uint8_t enc_kind, enc_flags;
  bool rematch_enemy;        /* its enemy is a rematch enemy (CC_TR_REMATCH if it ever finishes a run) */
  uint32_t rematch_enemy_id;
  bool rematch_live;         /* the record is CHALLENGED by this run and its result is not written yet */
  bool enemy_down;           /* the rematch enemy fainted */
  int result_pending;        /* a result the roster refused to take yet: 1 won, 2 lost */
  /* the offer */
  cc_nemesis offer_rec;
  bool offering;
  /* who finished the party */
  cand_t cand;
  /* rewards */
  cc_record_id reward_record;
  uint32_t reward_txn;
  bool claim_pending;        /* a voucher is owed and the card refused its claim: tried again at the next wave */
  char msg[160];
  co_carry carry;            /* what cr_load found, until cr_begin_observer takes it */
  bool have_carry;
  char error[CR_WHY_LEN];
  cc_jojo_state jojo;        /* the JoJo trainers' roll state (saved in the overlay) */
  uint8_t jojo_live;         /* the character of the battle on the field + 1 (0 = none), valid for jojo_wave */
  uint16_t jojo_wave;
  cr_stats st;
} R;

const cr_stats* cr_stats_get(void) { return &R.st; }
void cr_stats_reset(void) { memset(&R.st, 0, sizeof R.st); }
const char* cr_error(void) { return R.error; }
bool cr_active(void) { return R.active; }

static void forget_run(void) {
  const cr_stats st = R.st;
  memset(&R, 0, sizeof R);
  R.st = st;
}

static bool is_window(uint16_t wave) {
  for (int i = 0; i < CC_NUM_WINDOWS; i++)
    if (cc_window_waves[i] == wave) return true;
  return false;
}

static const cc_nemesis* rec_of(const cc_meta* m, cc_record_id id) {
  for (unsigned i = 0; i < m->roster.count; i++)
    if (m->roster.rec[i].record_id == id) return &m->roster.rec[i];
  return NULL;
}

static void say_player(const char* text) { snprintf(R.msg, sizeof R.msg, "%s", text); }

/* The identity an accepted rematch gives the enemy (gc/custom.h): the record's own, never reshaped. */
static void spec_of(const cc_nemesis* n, gc_custom_enemy* s) {
  memset(s, 0, sizeof *s);
  s->species = n->species;
  s->form = n->form;
  s->gender = n->gender;
  s->shiny = n->shiny != 0;
  s->has_nature = (n->known & CC_NK_NATURE) != 0;
  s->nature = n->nature;
  s->ability = n->ability;
  s->n_moves = n->n_moves;
  for (int i = 0; i < GC_CUSTOM_MAX_MOVES && i < CC_MAX_MOVES; i++) s->moves[i] = n->moves[i];
}

static uint8_t trait_of_kind(uint8_t kind) {
  switch (kind) {
    case GC_ENC_TRAINER: return CC_TR_TRAINER_OWNED;
    case GC_ENC_FIXED_OR_SCRIPTED: return CC_TR_FIXED_SCRIPTED;
    case GC_ENC_MYSTERY: return CC_TR_MYSTERY;
    case GC_ENC_BOSS:
    case GC_ENC_FINAL: return CC_TR_BOSS;
    default: return 0;
  }
}

static uint8_t cc_kind_of(uint8_t gc_kind) {
  switch (gc_kind) {
    case GC_ENC_ORDINARY_WILD: return CC_ENC_ORDINARY_WILD;
    case GC_ENC_TRAINER: return CC_ENC_TRAINER;
    case GC_ENC_FIXED_OR_SCRIPTED: return CC_ENC_FIXED_OR_SCRIPTED;
    case GC_ENC_MYSTERY: return CC_ENC_MYSTERY;
    case GC_ENC_BOSS: return CC_ENC_BOSS;
    case GC_ENC_FINAL: return CC_ENC_FINAL;
    default: return CC_ENC_UNSUPPORTED;
  }
}

static uint8_t cc_flags_of(uint8_t gc_flags) {
  return (uint8_t)(((gc_flags & GC_ENF_FUSED) ? CC_ENF_FUSED : 0) | ((gc_flags & GC_ENF_TERA) ? CC_ENF_TERA : 0) |
                   ((gc_flags & GC_ENF_DOUBLES) ? CC_ENF_DOUBLES : 0));
}

/* ------------------------------------------------------------------ transactions on the roster */

typedef struct {
  cc_schedule sched;         /* in: the schedule; out: the schedule after */
  cc_run_uid uid;
  uint64_t seed;
  cc_encounter enc;
  cc_proposal prop;
  cc_nemesis rec;            /* out: the chosen record as it stood */
  bool have_rec;
  int result;                /* 0 none, 1 won, 2 lost */
  bool accept;
  bool release;
} txc;

static cc_status tx_start(cc_meta* m, void* c) {
  txc* x = c;
  unsigned n = 0;
  cc_status st = cc_roster_release_except(&m->roster, x->uid, &n);
  if (st != CC_OK) return st;
  uint8_t rep[CC_MAX_NEMESES];
  memset(rep, 0, sizeof rep);
  for (unsigned i = 0; i < m->roster.count; i++) {
    gc_custom_enemy spec;
    spec_of(&m->roster.rec[i], &spec);
    rep[i] = gc_custom_enemy_check(&spec, NULL) ? 1 : 0;
  }
  return cc_rematch_start(&x->sched, &m->roster, x->uid, x->seed, rep);
}

static cc_status tx_window(cc_meta* m, void* c) {
  txc* x = c;
  const cc_status st = cc_rematch_window(&x->sched, &m->roster, &x->enc, &x->prop);
  if (st != CC_OK) return st;
  const cc_nemesis* r = rec_of(m, x->sched.chosen_id);
  if (r) {
    x->rec = *r;
    x->have_rec = true;
  }
  return CC_OK;
}

static cc_status tx_decide(cc_meta* m, void* c) {
  txc* x = c;
  return cc_rematch_decide(&x->sched, &m->roster, x->accept, NULL);
}

/* A result (if any), then the run lets go of what it holds. */
static cc_status tx_result(cc_meta* m, void* c) {
  txc* x = c;
  if (x->result) {
    const cc_status st = cc_rematch_result(&x->sched, &m->roster, x->result == 1, NULL);
    if (st != CC_OK) return st;
  }
  if (x->release) return cc_roster_release(&m->roster, x->uid, NULL);
  return CC_OK;
}

/* Runs a transaction; the schedule becomes x->sched only when the roster took it (or did not need to change). */
static bool run_tx(cs_txn_fn fn, txc* x, cs_tx_outcome* out) {
  cc_status st;
  const cs_tx_outcome o = cs_txn(fn, x, &st);
  if (out) *out = o;
  if (o == CS_TX_DONE || o == CS_TX_UNCHANGED) {
    R.sched = x->sched;
    return true;
  }
  return false;
}

static void degrade(const char* why) {
  if (!R.degraded) {
    R.degraded = true;
    R.st.degraded++;
    say_player(why);
  }
}

/* ------------------------------------------------------------------ rewards */

/* The voucher a won rematch is owed: profile_tx claims the record (custom claim + profile in one write-ahead envelope) and a repeat or a
 * reload finds it claimed. Returns true when the voucher is the player's now. */
static bool settle_record(cc_record_id id, bool announce) {
  uint32_t pending = ptx_pending();
  if (pending) {   /* a claim a crash left prepared (the gacha scene does the same for whatever it finds) */
    ptx_result res;
    ptx_execute(pending, &res);
  }
  if (cs_reward_state(id) != CS_CLAIM_OWED) return cs_reward_state(id) == CS_CLAIM_REPLAYED;
  /* The claim's recovery decides from the profile on the card: the card has to hold the profile as it is in memory when the claim begins,
   * or a crash between the two would leave "the system data changed while a purchase was waiting" and block the wallet. (The run's
   * dex entries and unlocks are in memory between its saves.) A card that refuses: the voucher stays owed and is tried again. */
  if (!sysd_save()) {
    R.claim_pending = true;
    return false;
  }
  uint32_t txn = 0;
  if (ptx_begin_claim(id, &txn) != PTX_OK) {
    R.claim_pending = true;
    return false;
  }
  ptx_result res;
  const ptx_exec x = ptx_execute(txn, &res);
  if (x != PTX_X_DONE && x != PTX_X_DONE_APPLY_PENDING) {
    R.claim_pending = true;
    return false;
  }
  if (res.outcome != PTX_R_DONE) return false;
  R.claim_pending = false;
  R.st.claims++;
  R.reward_record = id;
  R.reward_txn = txn;
  if (announce) say_player("You got an Egg Voucher for beating the rematch!");
  return true;
}

unsigned cr_settle_owed(void) {
  const cc_meta* m = cs_meta();
  if (!m) return 0;
  cc_record_id owed[CC_MAX_NEMESES];
  unsigned n = 0, claimed = 0;
  for (unsigned i = 0; i < m->roster.count; i++)
    if (m->roster.rec[i].life == CC_LIFE_DEFEATED) owed[n++] = m->roster.rec[i].record_id;
  for (unsigned i = 0; i < n; i++) {
    R.st.claim_retries++;
    if (settle_record(owed[i], R.active)) claimed++;
  }
  return claimed;
}

/* ------------------------------------------------------------------ the rematch's result */

/* Writes the result the run reached (1 won, 2 lost). On a refusal it stays pending and is tried again at the next wave, screen and run end. */
static void write_result_ex(int result, bool count);
static void write_result(int result) { write_result_ex(result, true); }
static void write_result_ex(int result, bool count) {
  if (!R.active) return;
  const cc_record_id id = R.sched.chosen_id;
  txc x;
  memset(&x, 0, sizeof x);
  x.sched = R.sched;
  x.uid = R.run_uid;
  x.result = result;
  if (!run_tx(tx_result, &x, NULL)) {
    R.result_pending = result;
    return;
  }
  R.result_pending = 0;
  R.rematch_live = false;
  R.enemy_down = false;
  if (result == 1) {
    R.st.wins++;
    settle_record(id, true);
  } else if (count) {
    R.st.losses++;
  }
}

static void retry_result(void) {
  if (R.result_pending) write_result(R.result_pending);
}

void cr_on_screen(const gc_run* run, int kind) {
  (void)run;
  if (!R.active) return;
  retry_result();
  if (R.rematch_live && R.enemy_down && kind != GC_SCREEN_COMMAND && kind != GC_SCREEN_GAME_OVER && kind != GC_SCREEN_CLEAR)
    write_result(1);
}

/* ------------------------------------------------------------------ the hooks */

static bool hk_encounter(void* user, const struct gc_run* run, const gc_custom_encounter* e) {
  (void)user;
  (void)run;
  if (!R.active) return false;
  R.st.waves++;
  /* the battle before this wave is over: whatever it left unresolved is settled now (the enemy fell: won; anything else: lost) */
  if (R.rematch_live) write_result(R.enemy_down ? 1 : 2);
  retry_result();
  if (R.claim_pending) {
    R.claim_pending = false;
    cr_settle_owed();
  }
  R.cand.valid = false;
  R.enemy_down = false;
  R.rematch_enemy = false;
  R.rematch_enemy_id = 0;
  R.offering = false;
  R.enc_wave = (uint16_t)e->wave;
  R.enc_kind = e->kind;
  R.enc_flags = e->flags;
  if (R.forced_done || R.degraded || !is_window((uint16_t)e->wave)) return false;
  R.st.windows++;
  txc x;
  memset(&x, 0, sizeof x);
  x.sched = R.sched;
  x.uid = R.run_uid;
  x.enc.wave = (uint16_t)e->wave;
  x.enc.kind = cc_kind_of(e->kind);
  x.enc.flags = cc_flags_of(e->flags);
  cs_tx_outcome o;
  if (!run_tx(tx_window, &x, &o)) {
    /* ORDER (a window was never reported) or a write the card refused: the rematch of this run stops, the wave is ordinary */
    degrade("The rematch could not be saved to the SD card, so it is skipped.");
    return false;
  }
  if (x.prop.kind != CC_PROP_OFFER || !x.have_rec) return false;   /* a skipped window, or one answered before (RESUME is never asked twice) */
  R.offer_rec = x.rec;
  R.offering = true;
  R.st.offers++;
  return true;
}

static bool hk_answer(void* user, const struct gc_run* run, bool accepted, gc_custom_enemy* spec) {
  (void)user;
  (void)run;
  if (!R.active || !R.offering) return false;
  R.offering = false;
  txc x;
  memset(&x, 0, sizeof x);
  x.sched = R.sched;
  x.uid = R.run_uid;
  x.accept = accepted;
  if (!run_tx(tx_decide, &x, NULL)) {
    degrade("The answer could not be saved to the SD card, so the rematch is skipped.");
    return false;
  }
  if (!accepted) {
    R.st.declines++;
    return false;
  }
  R.st.accepts++;
  R.rematch_live = true;   /* the roster says CHALLENGED; built() says whether the enemy exists */
  spec_of(&R.offer_rec, spec);
  return true;
}

static void hk_built(void* user, const struct gc_run* run, bool ok, const char* why) {
  (void)user;
  if (!R.active) return;
  if (ok && run->n_enemy == 1) {
    R.st.built++;
    R.rematch_enemy = true;
    R.rematch_enemy_id = run->enemy[0].b.id;
    return;
  }
  /* an explicit refusal: the enemy stays ordinary, the rematch is not a combat, the record is released at once */
  (void)why;
  R.st.refused++;
  write_result_ex(2, false);   /* the record is free again; a refusal is not a lost fight */
}

static void keep_actor(gc_custom_actor* d, const gc_custom_actor* s) { *d = *s; }

static void hk_faint(void* user, const struct gc_run* run, const gc_custom_faint* f) {
  (void)user;
  (void)run;
  if (!R.active) return;
  if (f->side == 1) {
    if (R.rematch_live && R.rematch_enemy && f->fainted.id == R.rematch_enemy_id) R.enemy_down = true;
    return;
  }
  /* a Pokemon of the party fell: it only matters when it was the last one */
  R.cand.valid = false;
  if (f->remaining != 0) return;
  cand_t* c = &R.cand;
  c->wave = f->wave;
  c->turn = f->turn;
  c->direct = f->direct;
  c->move = f->move;
  keep_actor(&c->fallen, &f->fainted);
  c->source_enemy = f->has_source && f->source_side == 1;
  if (f->has_source) keep_actor(&c->src, &f->source);
  c->valid = true;
}

/* The character's team in game-core's terms. False when game-core cannot build it. */
bool cr_jojo_team(int idx, gc_custom_team* team) {
  if (idx < 0 || (unsigned)idx >= cc_jojo_count) return false;
  const cc_jojo_char* c = &cc_jojo_chars[idx];
  memset(team, 0, sizeof *team);
  team->name = c->name;
  team->passive = c->passive;
  team->n = c->n_members;
  for (int i = 0; i < c->n_members && i < GC_CUSTOM_MAX_TEAM; i++) {
    const cc_jojo_member* m = &c->member[i];
    gc_custom_team_member* t = &team->member[i];
    t->species = m->species;
    t->form = m->form;
    t->ability_slot = m->ability_slot;
    t->shiny = m->shiny != 0;
    t->variant = m->variant;
    for (int k = 0; k < GC_CUSTOM_MAX_MOVES && k < 4; k++) t->moves[k] = m->moves[k];
    t->n_items = m->n_items;
    for (int k = 0; k < m->n_items && k < GC_CUSTOM_MAX_TEAM_ITEMS; k++) {
      t->items[k].key = m->items[k].key;
      t->items[k].arg = m->items[k].arg;
      t->items[k].stack = m->items[k].stack;
    }
  }
  return gc_custom_team_check(team, NULL);
}

/* An eligible trainer wave (game-core decides which: gc/custom.h): the JoJo roll. A hit fills the character's team; game-core builds it. */
static bool hk_trainer(void* user, const struct gc_run* run, const gc_custom_encounter* e, gc_custom_team* team) {
  (void)user;
  (void)run;
  if (!R.active) return false;
  R.st.jojo_waves++;
  const int idx = cc_jojo_roll(&R.jojo, e->biome);
  if (idx == CC_JOJO_NONE) return false;
  if (!cr_jojo_team(idx, team)) return false;   /* a table game-core cannot build: the wave stays ordinary (the host test checks the table) */
  R.jojo_live = (uint8_t)(idx + 1);   /* valid for jojo_wave only: a later wave's battle is never taken for it */
  R.jojo_wave = (uint16_t)e->wave;
  R.st.jojo_fights++;
  return true;
}

const cc_jojo_char* cr_jojo_foe(const gc_run* run) {
  if (!R.active || !R.jojo_live || R.jojo_live > cc_jojo_count || R.jojo_wave != run->wave || run->battle_type != BATTLE_TYPE_TRAINER) return NULL;
  return &cc_jojo_chars[R.jojo_live - 1];
}

static const gc_custom_hooks HOOKS = {NULL, hk_encounter, hk_answer, hk_built, hk_faint, hk_trainer};
const gc_custom_hooks* cr_hooks(void) { return &HOOKS; }

int cr_offer_species(void) { return R.offering ? R.offer_rec.species : 0; }

const char* cr_take_message(void) {
  static char out[160];
  if (!R.msg[0]) return NULL;
  snprintf(out, sizeof out, "%s", R.msg);
  R.msg[0] = 0;
  return out;
}

/* ------------------------------------------------------------------ the defeat's finisher */

/* The candidate record for the run that is ending, from what the faint hook named. false: nothing reliable (nothing is handed over). */
static bool build_defeat(const gc_run* run, cs_defeat* out) {
  const cand_t* c = &R.cand;
  if (!c->valid || !c->direct || !c->source_enemy || c->wave != run->wave || R.enc_wave != run->wave) return false;
  if (R.enc_kind == GC_ENC_UNSUPPORTED) return false;   /* no trait says "unsupported": unknown stays unknown */
  const gc_custom_actor* a = &c->src;
  /* a battle form change would make the species in the report differ from the one the record names: unknown stays unknown */
  if (a->species != a->start_species || a->form != a->start_form) return false;
  if (a->id == 0 || a->start_species == 0 || a->level == 0) return false;
  memset(out, 0, sizeof *out);
  cc_nemesis* n = &out->candidate;
  n->life = CC_LIFE_STORED;
  n->traits = trait_of_kind(R.enc_kind);
  if (R.enc_flags & GC_ENF_FUSED) n->traits |= CC_TR_FUSED;
  if (R.enc_flags & GC_ENF_TERA) n->traits |= CC_TR_TERA;
  if (R.enc_flags & GC_ENF_DOUBLES) n->traits |= CC_TR_DOUBLES;
  if (a->is_boss) n->traits |= CC_TR_BOSS;
  if (a->fused) n->traits |= CC_TR_FUSED;
  if (a->tera) n->traits |= CC_TR_TERA;
  if (a->trainer_owned) n->traits |= CC_TR_TRAINER_OWNED;
  if (R.rematch_enemy && a->id == R.rematch_enemy_id) n->traits |= CC_TR_REMATCH;
  n->source_pokemon_id = a->id;
  n->species = a->start_species;
  n->form = a->start_form;
  n->gender = a->gender;
  n->shiny = a->shiny ? 1 : 0;
  n->orig_level = a->level;
  n->known |= CC_NK_NATURE | CC_NK_PASSIVE | CC_NK_IVS;
  n->nature = a->nature;
  n->passive = a->passive;
  memcpy(n->ivs, a->ivs, 6);
  if (a->ability >= 1) {
    n->known |= CC_NK_ABILITY;
    n->ability = a->ability;
  }
  if (a->n_moves >= 1) {
    n->known |= CC_NK_MOVES;
    n->n_moves = a->n_moves;
    for (int i = 0; i < a->n_moves && i < CC_MAX_MOVES; i++) n->moves[i] = a->moves[i];
  }
  n->known |= CC_NK_LOADOUT;
  n->n_items = 0;
  for (int i = 0; i < a->n_items && n->n_items < CC_MAX_LOADOUT_ITEMS; i++) {
    cc_item* it = &n->items[n->n_items++];
    snprintf(it->key, sizeof it->key, "%s", a->items[i].key ? a->items[i].key : "");
    it->arg = a->items[i].arg;
    it->stack = a->items[i].stack;
  }
  if (a->n_items >= GC_CUSTOM_MAX_ITEMS) n->known |= CC_NK_LOADOUT_TRUNCATED;   /* game-core reports at most 8: there may be more */
  if (c->move >= 1 && c->move <= CC_MOVE_ID_MAX) {
    n->known |= CC_NK_FINISH_MOVE;
    n->finish_move = c->move;
    out->has_finish_move = true;
    out->finish_move = c->move;
  }
  if (c->fallen.species >= 1) {
    n->known |= CC_NK_FALLEN;
    n->fallen_species = c->fallen.species;
    n->fallen_form = c->fallen.form;
    out->has_fallen = true;
    out->fallen_actor = (cc_actor){c->fallen.id, c->fallen.species, c->fallen.form, CC_SIDE_PLAYER, c->fallen.level};
  }
  n->known |= CC_NK_FINISHER_VERIFIED;
  out->finish_actor = (cc_actor){a->id, a->start_species, a->start_form, CC_SIDE_ENEMY, a->level};
  return true;
}

bool cr_on_run_end(const gc_run* run, int kind, cs_defeat* out) {
  if (!R.active) return false;
  retry_result();
  if (R.rematch_live) write_result(kind != GC_SCREEN_GAME_OVER && R.enemy_down ? 1 : 2);
  /* the run is over: whatever it still holds (a record scheduled and never offered) is given back */
  txc x;
  memset(&x, 0, sizeof x);
  x.sched = R.sched;
  x.uid = R.run_uid;
  x.release = true;
  run_tx(tx_result, &x, NULL);
  cr_settle_owed();
  bool have = false;
  if (kind == GC_SCREEN_GAME_OVER) {
    have = build_defeat(run, out);
    if (have)
      R.st.candidates++;
    else
      R.st.candidates_none++;
  }
  return have;
}

void cr_end(void) {
  forget_run();
}

/* ------------------------------------------------------------------ the begin of a run */

bool cr_usable(char why[CR_WHY_LEN]) {
  char w[CS_WHY_LEN];
  const bool ok = cs_get_state(w) != CS_UNREADABLE;
  if (why) snprintf(why, CR_WHY_LEN, "%s", ok ? "" : w);
  return ok;
}

static uint64_t mix_seed(uint32_t entropy, cc_run_uid uid) {
  uint64_t s = ((uint64_t)entropy * 0xD1B54A32D192ED03ull) ^ uid;
  return cc_rng_v1_next(&s);
}

bool cr_begin(gc_run* run, uint32_t entropy) {
  forget_run();
  R.error[0] = 0;
  if (!cr_usable(R.error)) return false;
  const cc_run_uid uid = cs_begin_run();
  if (!uid) {
    snprintf(R.error, sizeof R.error, "The custom run could not be reserved on the SD card.");
    return false;
  }
  txc x;
  memset(&x, 0, sizeof x);
  x.uid = uid;
  x.seed = mix_seed(entropy, uid);
  cs_tx_outcome o;
  if (!run_tx(tx_start, &x, &o)) {
    snprintf(R.error, sizeof R.error, o == CS_TX_WRITE_FAILED ? "The rematch could not be reserved on the SD card." : "The remembered enemies could not be read.");
    forget_run();
    return false;
  }
  R.active = true;
  R.run_uid = uid;
  cc_jojo_start(&R.jojo, mix_seed(entropy, uid));
  R.st.runs_started++;
  gc_run_set_custom(run, &HOOKS);
  return true;
}

/* ------------------------------------------------------------------ the overlay (wire format) */

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

static void w8(wr* w, uint8_t v) {
  if (w->n + 1 > w->cap) { w->bad = true; return; }
  w->p[w->n++] = v;
}
static void w16(wr* w, uint16_t v) { w8(w, (uint8_t)v); w8(w, (uint8_t)(v >> 8)); }
static void w32(wr* w, uint32_t v) { w16(w, (uint16_t)v); w16(w, (uint16_t)(v >> 16)); }
static void w64(wr* w, uint64_t v) { w32(w, (uint32_t)v); w32(w, (uint32_t)(v >> 32)); }

static uint8_t r8(rd* r) {
  if (r->i + 1 > r->n) { r->bad = true; return 0; }
  return r->p[r->i++];
}
static uint16_t r16(rd* r) { const uint16_t a = r8(r); return (uint16_t)(a | (uint16_t)r8(r) << 8); }
static uint32_t r32(rd* r) { const uint32_t a = r16(r); return a | (uint32_t)r16(r) << 16; }
static uint64_t r64(rd* r) { const uint64_t a = r32(r); return a | (uint64_t)r32(r) << 32; }

#define OV_VERSION 2u   /* 2 adds the JoJo block; version 1 files (from before) still load, with a fresh JoJo state */
#define OV_MAX 16384u

static void w_sched(wr* w, const cc_schedule* s) {
  w8(w, s->state);
  w8(w, s->rng_algo);
  w8(w, s->n_frozen);
  w8(w, s->n_windows);
  w64(w, s->run_uid);
  w64(w, s->rng_seed);
  w64(w, s->rng_state);
  w32(w, s->chosen_id);
  for (int i = 0; i < s->n_frozen && i < CC_MAX_FROZEN; i++) w32(w, s->frozen_ids[i]);
  for (int i = 0; i < s->n_windows && i < CC_NUM_WINDOWS; i++) {
    w16(w, s->windows[i].wave);
    w8(w, s->windows[i].state);
    w8(w, s->windows[i].reason);
  }
}

static bool r_sched(rd* r, cc_schedule* s) {
  memset(s, 0, sizeof *s);
  s->state = r8(r);
  s->rng_algo = r8(r);
  s->n_frozen = r8(r);
  s->n_windows = r8(r);
  s->run_uid = r64(r);
  s->rng_seed = r64(r);
  s->rng_state = r64(r);
  s->chosen_id = r32(r);
  if (s->n_frozen > CC_MAX_FROZEN || s->n_windows > CC_NUM_WINDOWS) return false;
  for (int i = 0; i < s->n_frozen; i++) s->frozen_ids[i] = r32(r);
  for (int i = 0; i < s->n_windows; i++) {
    s->windows[i].wave = r16(r);
    s->windows[i].state = r8(r);
    s->windows[i].reason = r8(r);
  }
  return !r->bad && cc_schedule_validate(s) == CC_OK;
}

static void w_event(wr* w, const cc_event* e) {
  w32(w, e->seq);
  w8(w, e->kind);
  w8(w, e->target_side);
  w8(w, e->source_side);
  w8(w, e->target_form);
  w8(w, e->source_form);
  w8(w, e->result);
  w16(w, e->known);
  w16(w, e->target_species);
  w16(w, e->source_species);
  w16(w, e->move_id);
  w32(w, e->target_id);
  w32(w, e->source_id);
  w32(w, (uint32_t)e->shown);
  w32(w, (uint32_t)e->hp_delta);
  w32(w, (uint32_t)e->hp_after);
}

static void r_event(rd* r, cc_event* e) {
  e->seq = r32(r);
  e->kind = r8(r);
  e->target_side = r8(r);
  e->source_side = r8(r);
  e->target_form = r8(r);
  e->source_form = r8(r);
  e->result = r8(r);
  e->known = r16(r);
  e->target_species = r16(r);
  e->source_species = r16(r);
  e->move_id = r16(r);
  e->target_id = r32(r);
  e->source_id = r32(r);
  e->shown = (int32_t)r32(r);
  e->hp_delta = (int32_t)r32(r);
  e->hp_after = (int32_t)r32(r);
}

static void w_carry(wr* w, const co_carry* c) {
  w32(w, c->next_seq);
  w32(w, c->battle_seq);
  w8(w, c->hist.n_turns);
  w8(w, c->hist.flags);
  for (int t = 0; t < c->hist.n_turns && t < CC_HISTORY_TURNS; t++) {
    const cc_turn* u = &c->hist.turns[t];
    w32(w, u->battle_seq);
    w16(w, u->wave);
    w16(w, u->turn);
    w16(w, u->dropped);
    w8(w, u->flags);
    w8(w, u->n_events);
    for (int k = 0; k < u->n_events && k < CC_MAX_EVENTS_PER_TURN; k++) w_event(w, &u->events[k]);
  }
}

static bool r_carry(rd* r, co_carry* c) {
  memset(c, 0, sizeof *c);
  c->next_seq = r32(r);
  c->battle_seq = r32(r);
  c->hist.n_turns = r8(r);
  c->hist.flags = r8(r);
  if (c->hist.n_turns > CC_HISTORY_TURNS) return false;
  for (int t = 0; t < c->hist.n_turns; t++) {
    cc_turn* u = &c->hist.turns[t];
    u->battle_seq = r32(r);
    u->wave = r16(r);
    u->turn = r16(r);
    u->dropped = r16(r);
    u->flags = r8(r);
    u->n_events = r8(r);
    if (u->n_events > CC_MAX_EVENTS_PER_TURN) return false;
    for (int k = 0; k < u->n_events; k++) r_event(r, &u->events[k]);
  }
  return !r->bad && cc_history_validate(&c->hist) == CC_OK;
}

enum { OF_DONE = 1, OF_LIVE = 2, OF_REMATCH_ENEMY = 4, OF_ALL = 7 };

static size_t overlay_encode(uint8_t* buf, size_t cap) {
  wr w = {buf, 0, cap, false};
  w8(&w, OV_VERSION);
  w64(&w, R.run_uid);
  w8(&w, (uint8_t)((R.forced_done ? OF_DONE : 0) | (R.rematch_live ? OF_LIVE : 0) | (R.rematch_enemy ? OF_REMATCH_ENEMY : 0)));
  w32(&w, R.rematch_enemy_id);
  w16(&w, R.enc_wave);
  w8(&w, R.enc_kind);
  w8(&w, R.enc_flags);
  w32(&w, R.reward_record);
  w32(&w, R.reward_txn);
  w_sched(&w, &R.sched);
  w64(&w, R.jojo.rng_state);
  w32(&w, R.jojo.used);
  w8(&w, R.jojo.fights);
  w8(&w, R.jojo_live);
  w16(&w, R.jojo_wave);
  static co_carry carry_out;   /* not on the stack */
  co_carry* carry = &carry_out;
  const bool have = co_carry_get(carry);
  w8(&w, have ? 1 : 0);
  if (have) w_carry(&w, carry);
  return w.bad ? 0 : w.n;
}

typedef struct {
  cc_run_uid run_uid;
  uint8_t flags;
  uint32_t rematch_enemy_id;
  uint16_t enc_wave;
  uint8_t enc_kind, enc_flags;
  uint32_t reward_record, reward_txn;
  cc_schedule sched;
  bool has_jojo;             /* version 2: the JoJo block follows */
  cc_jojo_state jojo;
  uint8_t jojo_live;
  uint16_t jojo_wave;
  bool has_carry;
} overlay;

static bool overlay_decode(const uint8_t* buf, size_t len, overlay* o, co_carry* carry) {
  rd r = {buf, len, 0, false};
  memset(o, 0, sizeof *o);
  const uint8_t version = r8(&r);
  if (version != OV_VERSION && version != 1u) return false;
  o->run_uid = r64(&r);
  o->flags = r8(&r);
  o->rematch_enemy_id = r32(&r);
  o->enc_wave = r16(&r);
  o->enc_kind = r8(&r);
  o->enc_flags = r8(&r);
  o->reward_record = r32(&r);
  o->reward_txn = r32(&r);
  if (r.bad || (o->flags & ~OF_ALL) || o->run_uid == 0 || o->enc_kind > GC_ENC_UNSUPPORTED || (o->enc_flags & ~7u)) return false;
  if (!r_sched(&r, &o->sched) || o->sched.run_uid != o->run_uid) return false;
  if (version >= 2u) {
    o->has_jojo = true;
    o->jojo.rng_state = r64(&r);
    o->jojo.used = r32(&r);
    o->jojo.fights = r8(&r);
    o->jojo_live = r8(&r);
    o->jojo_wave = r16(&r);
    if (r.bad || cc_jojo_validate(&o->jojo) != CC_OK) return false;
    if (o->jojo_live > cc_jojo_count || (o->jojo_live && !(o->jojo.used >> (o->jojo_live - 1) & 1u))) return false;
  }
  const uint8_t has = r8(&r);
  if (has > 1) return false;
  o->has_carry = has == 1;
  if (o->has_carry && !r_carry(&r, carry)) return false;
  return !r.bad && r.i == r.n;
}

/* ------------------------------------------------------------------ the custom slot file */

/* magic "PRCS" u32 | version u16 | reserved u16 | overlay_len u32 | core_len u32 | overlay | core run bytes */
#define FILE_MAGIC 0x53435250u
#define FILE_VERSION 1u
#define FILE_HEADER 16u

static const char* slot_file(int slot, char out[24]) {
  snprintf(out, 24, CR_FILE_PREFIX "%d", slot);
  return out;
}

rsv_save_result cr_save(int slot, const gc_run* run) {
  if (!rsv_slot_ok(slot)) return RSV_BAD_SLOT;
  if (!R.active || !gc_run_can_save(run)) return RSV_NOT_ALLOWED;
  uint8_t* buf = malloc(FILE_HEADER + OV_MAX + GC_SAVE_MAX);
  if (!buf) return RSV_IO_ERROR;
  const size_t n_ov = overlay_encode(buf + FILE_HEADER, OV_MAX);
  const size_t n_core = n_ov ? gc_run_save(run, buf + FILE_HEADER + n_ov, GC_SAVE_MAX) : 0;
  if (!n_ov || !n_core) {
    free(buf);
    return RSV_NOT_ALLOWED;
  }
  wr h = {buf, 0, FILE_HEADER, false};
  w32(&h, FILE_MAGIC);
  w16(&h, FILE_VERSION);
  w16(&h, 0);
  w32(&h, (uint32_t)n_ov);
  w32(&h, (uint32_t)n_core);
  char name[24];
  const save_result r = save_write(slot_file(slot, name), buf, FILE_HEADER + n_ov + n_core);
  free(buf);
  return r == SAVE_OK ? RSV_SAVED : RSV_IO_ERROR;
}

typedef enum { PF_OK, PF_EMPTY, PF_DAMAGED, PF_UNREADABLE, PF_NOT_CUSTOM, PF_VERSION, PF_BAD } pf_status;

/* Reads and splits a slot file. On PF_OK *raw is the malloc'd file and the three views point into it. */
static pf_status read_slot(int slot, char** raw, const uint8_t** ov, size_t* n_ov, const char** core, size_t* n_core, bool* backup) {
  char name[24];
  size_t len = 0;
  *raw = NULL;
  const save_result r = save_read(slot_file(slot, name), raw, &len, backup);
  if (r == SAVE_NOT_FOUND) return PF_EMPTY;
  if (r == SAVE_CORRUPT) return PF_DAMAGED;
  if (r != SAVE_OK) return PF_UNREADABLE;
  rd h = {(const uint8_t*)*raw, len, 0, false};
  pf_status st = PF_OK;
  if (len < FILE_HEADER || r32(&h) != FILE_MAGIC) {
    st = PF_NOT_CUSTOM;
  } else if (r16(&h) != FILE_VERSION) {
    st = PF_VERSION;
  } else {
    r16(&h);
    const uint32_t a = r32(&h), b = r32(&h);
    if (a == 0 || a > OV_MAX || b == 0 || b > GC_SAVE_MAX || (uint64_t)FILE_HEADER + a + b != len) {
      st = PF_BAD;
    } else {
      *ov = (const uint8_t*)*raw + FILE_HEADER;
      *n_ov = a;
      *core = *raw + FILE_HEADER + a;
      *n_core = b;
    }
  }
  if (st != PF_OK) {
    free(*raw);
    *raw = NULL;
  }
  return st;
}

static void why_of(pf_status st, char out[RSV_WHY_LEN]) {
  switch (st) {
    case PF_DAMAGED: snprintf(out, RSV_WHY_LEN, "The save is damaged (it failed its checksum)."); break;
    case PF_UNREADABLE: snprintf(out, RSV_WHY_LEN, "The save could not be read from the SD card."); break;
    case PF_NOT_CUSTOM: snprintf(out, RSV_WHY_LEN, "Not a custom run save."); break;
    case PF_VERSION: snprintf(out, RSV_WHY_LEN, "Made by a different version of the game."); break;
    case PF_BAD: snprintf(out, RSV_WHY_LEN, "The custom run save is damaged."); break;
    default: out[0] = 0; break;
  }
}

rsv_status cr_inspect(int slot, rsv_info* out) {
  memset(out, 0, sizeof *out);
  if (!rsv_slot_ok(slot)) return out->status = RSV_EMPTY;
  char* raw;
  const uint8_t* ov;
  const char* core;
  size_t n_ov, n_core;
  bool bak = false;
  const pf_status st = read_slot(slot, &raw, &ov, &n_ov, &core, &n_core, &bak);
  if (st == PF_EMPTY) return out->status = RSV_EMPTY;
  if (st != PF_OK) {
    out->status = RSV_INCOMPATIBLE;
    why_of(st, out->why);
    return RSV_INCOMPATIBLE;
  }
  static overlay o;
  static co_carry scratch;
  if (!overlay_decode(ov, n_ov, &o, &scratch)) {
    free(raw);
    out->status = RSV_INCOMPATIBLE;
    snprintf(out->why, sizeof out->why, "The custom run's own data is damaged.");
    return RSV_INCOMPATIBLE;
  }
  const rsv_status s = rsv_inspect_bytes(core, n_core, out);
  free(raw);
  if (s == RSV_OK) out->from_backup = bak;
  return s;
}

void cr_inspect_all(rsv_info out[RSV_SLOTS]) {
  for (int i = 0; i < RSV_SLOTS; i++) cr_inspect(i, &out[i]);
}

/* ------------------------------------------------------------------ reconcile */

typedef struct {
  overlay o;
  const gc_run* run;
  cc_schedule sched;
  bool live;
  bool reconciled;
  bool lost_written;
  char why[RSV_WHY_LEN];
} rec_ctx;

/* The roster (custom.sav) and the overlay (inside the run's file) are two files; a crash between their writes leaves the roster ahead of
 * the overlay. The overlay is the truth about the run's schedule and the roster about the records, so:
 *   schedule ended without a rematch (none, exhausted, declined): whatever the run holds is given back;
 *   schedule still open (active):  the record must be reserved for this run. CHALLENGED by it (the accept came after this save) goes back
 *       to SCHEDULED, so the offer is made again; STORED (given back by another run's start, or by a decline/loss after the save) is
 *       reserved again; a record already DEFEATED or REWARD_CLAIMED by this run means the rematch was won after the save: nothing more is
 *       offered (forced_done). A record another run took, or one that is gone, is a mismatch: the run is refused, not resumed;
 *   rematch accepted: CHALLENGED by this run is a fight to play again (live) -- unless the run is already past its wave or its enemy is not
 *       the one the overlay names, which is a lost fight; any other state means the result was already written, nothing is live.
 * Every change is staged, validated and written once (or not at all: a refusal leaves both files as they were). */
static cc_status tx_reconcile(cc_meta* m, void* c) {
  rec_ctx* x = c;
  const uint64_t uid = x->o.run_uid;
  x->sched = x->o.sched;
  const uint8_t state = x->sched.state;
  /* a different run's reservation is given back first: this run is the one being resumed */
  cc_status st = cc_roster_release_except(&m->roster, uid, NULL);
  if (st != CC_OK) return st;
  if (state == CC_SCHED_NO_CANDIDATE || state == CC_SCHED_EXHAUSTED || state == CC_SCHED_DECLINED) {
    return cc_roster_release(&m->roster, uid, NULL);
  }
  int slot = -1;
  for (unsigned i = 0; i < m->roster.count; i++)
    if (m->roster.rec[i].record_id == x->sched.chosen_id) slot = (int)i;
  if (state == CC_SCHED_ACTIVE) {
    if (slot < 0) {
      snprintf(x->why, sizeof x->why, "The enemy this run was going to face is no longer remembered.");
      return CC_E_CONSISTENCY;
    }
    cc_nemesis rec = m->roster.rec[slot];
    if (rec.life == CC_LIFE_CHALLENGED && rec.sched_run_uid == uid) {
      st = cc_nemesis_transition(&rec, CC_LIFE_STORED, 0, 0, 0);
      if (st != CC_OK) return st;
      x->reconciled = true;
    }
    if (rec.life == CC_LIFE_STORED) {
      st = cc_nemesis_transition(&rec, CC_LIFE_SCHEDULED, uid, 0, 0);
      if (st != CC_OK) return st;
      m->roster.rec[slot] = rec;
      x->reconciled = true;
    } else if ((rec.life == CC_LIFE_DEFEATED || rec.life == CC_LIFE_REWARD_CLAIMED) && rec.sched_run_uid == uid) {
      x->o.flags |= OF_DONE;
      x->reconciled = true;
    } else if (!(rec.life == CC_LIFE_SCHEDULED && rec.sched_run_uid == uid)) {
      snprintf(x->why, sizeof x->why, "The enemy this run was going to face is used by another run.");
      return CC_E_CONSISTENCY;
    }
    return CC_OK;
  }
  if (state == CC_SCHED_ACCEPTED) {
    if (slot < 0) return CC_OK;   /* claimed and since replaced in a full roster: nothing is live */
    cc_nemesis rec = m->roster.rec[slot];
    if (rec.life == CC_LIFE_CHALLENGED && rec.sched_run_uid == uid) {
      const gc_run* run = x->run;
      const bool same = run->n_enemy == 1 && x->o.rematch_enemy_id != 0 && run->enemy[0].b.id == x->o.rematch_enemy_id;
      if (!same || run->wave != rec.challenge_wave) {
        st = cc_nemesis_transition(&rec, CC_LIFE_STORED, 0, 0, 0);
        if (st != CC_OK) return st;
        m->roster.rec[slot] = rec;
        x->lost_written = true;
        x->reconciled = true;
      } else {
        x->live = true;
      }
    }
    return CC_OK;
  }
  /* OFFERED is never saved (no save point while the offer waits) */
  snprintf(x->why, sizeof x->why, "The custom run's schedule is in a state that is never saved.");
  return CC_E_CONSISTENCY;
}

rsv_status cr_load(int slot, gc_run* run, char why[RSV_WHY_LEN]) {
  char dummy[RSV_WHY_LEN];
  if (!why) why = dummy;
  why[0] = 0;
  forget_run();
  if (!rsv_slot_ok(slot)) return RSV_EMPTY;
  char* raw;
  const uint8_t* ov;
  const char* core;
  size_t n_ov, n_core;
  bool bak = false;
  const pf_status pst = read_slot(slot, &raw, &ov, &n_ov, &core, &n_core, &bak);
  if (pst == PF_EMPTY) return RSV_EMPTY;
  if (pst != PF_OK) {
    why_of(pst, why);
    return RSV_INCOMPATIBLE;
  }
  static rec_ctx x;   /* the overlay and the staged schedule: not on the stack */
  static co_carry carry_in;
  memset(&x, 0, sizeof x);
  if (!overlay_decode(ov, n_ov, &x.o, &carry_in)) {
    free(raw);
    snprintf(why, RSV_WHY_LEN, "The custom run's own data is damaged.");
    return RSV_INCOMPATIBLE;
  }
  rsv_info info;
  if (rsv_load_bytes(core, n_core, run, &info) != RSV_OK) {
    free(raw);
    snprintf(why, RSV_WHY_LEN, "%s", info.why);
    return RSV_INCOMPATIBLE;
  }
  free(raw);
  char fw[CR_WHY_LEN];
  if (!cr_usable(fw)) {
    snprintf(why, RSV_WHY_LEN, "The custom content file cannot be used, so this run cannot continue.");
    return RSV_INCOMPATIBLE;
  }
  x.run = run;
  cc_status cst;
  const cs_tx_outcome o = cs_txn(tx_reconcile, &x, &cst);
  if (o != CS_TX_DONE && o != CS_TX_UNCHANGED) {
    R.st.resume_refused++;
    if (o == CS_TX_WRITE_FAILED)
      snprintf(why, RSV_WHY_LEN, "The custom content file could not be updated on the SD card.");
    else
      snprintf(why, RSV_WHY_LEN, "%s", x.why[0] ? x.why : "The remembered enemies do not match this run.");
    return RSV_INCOMPATIBLE;
  }
  /* the sticky run is back */
  const cr_stats st = R.st;
  memset(&R, 0, sizeof R);
  R.st = st;
  R.carry = carry_in;
  R.have_carry = x.o.has_carry;
  R.active = true;
  R.run_uid = x.o.run_uid;
  R.sched = x.sched;
  R.forced_done = (x.o.flags & OF_DONE) != 0;
  R.rematch_live = x.live;
  R.rematch_enemy = (x.o.flags & OF_REMATCH_ENEMY) != 0 && x.o.rematch_enemy_id != 0 && run->n_enemy == 1 && run->enemy[0].b.id == x.o.rematch_enemy_id;
  R.rematch_enemy_id = R.rematch_enemy ? x.o.rematch_enemy_id : 0;
  R.enc_wave = x.o.enc_wave;
  R.enc_kind = x.o.enc_kind;
  R.enc_flags = x.o.enc_flags;
  R.reward_record = x.o.reward_record;
  R.reward_txn = x.o.reward_txn;
  if (x.o.has_jojo) {
    R.jojo = x.o.jojo;
    R.jojo_live = x.o.jojo_live;
    R.jojo_wave = x.o.jojo_wave;
  } else {
    cc_jojo_start(&R.jojo, mix_seed(0, x.o.run_uid));   /* a run saved before the JoJo trainers: they start now */
  }
  R.st.runs_resumed++;
  if (x.reconciled) R.st.reconciled++;
  if (x.lost_written) R.st.losses++;
  gc_run_set_custom(run, &HOOKS);
  if (cr_jojo_foe(run)) gc_custom_set_trainer_name(run, cr_jojo_foe(run)->name);   /* the saved battle is a JoJo trainer's */
  cr_settle_owed();   /* a win whose voucher a crash left unclaimed */
  return RSV_OK;
}

void cr_begin_observer(void) {
  if (R.have_carry)
    co_begin_run_carry(&R.carry);
  else
    co_begin_run(true);
  R.have_carry = false;
}

bool cr_clear(int slot) {
  if (!rsv_slot_ok(slot)) return false;
  /* a slot that still names a reservation gives it back (best effort: a record another run holds is not this one's to release) */
  char* raw;
  const uint8_t* ov;
  const char* core;
  size_t n_ov, n_core;
  bool bak = false;
  if (read_slot(slot, &raw, &ov, &n_ov, &core, &n_core, &bak) == PF_OK) {
    static overlay o;
    static co_carry scratch;
    const bool ok = overlay_decode(ov, n_ov, &o, &scratch);
    free(raw);
    if (ok) {
      txc x;
      memset(&x, 0, sizeof x);
      x.uid = o.run_uid;
      x.release = true;
      x.sched = o.sched;
      cc_status st;
      cs_txn(tx_result, &x, &st);
    }
  }
  char name[24];
  slot_file(slot, name);
  rsv_name_remove(name);
  return rsv_remove_files(name);
}

/* ------------------------------------------------------------------ tests */

void cr_view_get(cr_view* v) {
  memset(v, 0, sizeof *v);
  v->active = R.active;
  v->run_uid = R.run_uid;
  v->sched = R.sched;
  v->forced_done = R.forced_done;
  v->rematch_live = R.rematch_live;
  v->rematch_enemy = R.rematch_enemy;
  v->enemy_down = R.enemy_down;
  v->degraded = R.degraded;
  v->rematch_enemy_id = R.rematch_enemy_id;
  v->enc_wave = R.enc_wave;
  v->enc_kind = R.enc_kind;
  v->enc_flags = R.enc_flags;
  v->reward_record = R.reward_record;
  v->have_candidate = R.cand.valid;
  v->jojo = R.jojo;
  v->jojo_live = R.jojo_live;
  v->jojo_wave = R.jojo_wave;
}
