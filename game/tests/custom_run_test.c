/* custom_run_test: the optional Nemesis rematch of a custom run (task C10): custom_run.c over the real game-core (with its faint observer and
 * encounter hooks), the real custom store, profile transactions and save slots, on the host.
 *
 *   build/custom_run_test [more seeds]
 *
 * The driver below plays a real game-core run the way run_screen.c does (events drained and given to the combat observer, the controller
 * told of every screen, the run ended with co_finish / cr_on_run_end / cs_commit_defeat), with a bot that answers screens. Works in a
 * temporary folder (the SD card's "sdmc:/..." paths are relative here). What is checked, in order:
 *   1  a real defeat by an ordinary wild enemy creates a Nemesis whose every field is the enemy's as it stood; an ordinary run creates none
 *   2  the finisher's gate: trainer, boss, mystery, fixed, fused, Tera, doubles, a rematch enemy, an unknown cause, a form change...
 *   3  the four windows, skips, exhaustion, decline, accept, a refused build, a write the card refuses, no roster, a record the game cannot build
 *   4  a real rematch at wave 27, from a Nemesis a real defeat made: offered, built at the ordinary level through the game's own paths,
 *      fought, won, recorded and rewarded once; uninterrupted and with the run saved and loaded at every boundary
 *   5  save / load before and after the offer, decline, accept, victory, defeat and the reward; a crash between the two files; a run whose
 *      reservation another run took; a damaged overlay; custom bytes are never an ordinary session
 *   6  the reward exactly once, under a refused claim and under repeats and reloads
 *   7  custom off: the same seeded run, with and without the controller, is identical
 * Exit 1 on any failed check. */
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../../game-core/src/internal.h"
#include "cc/codec.h"
#include "cc/model.h"
#include "cc/scheduler.h"
#include "combat_observer.h"
#include "custom_run.h"
#include "custom_store.h"
#include "profile_tx.h"
#include "run_saves.h"
#include "save.h"
#include "system_data.h"

static int checks, failed;
#define CHECK(cond, ...)                                  \
  do {                                                    \
    checks++;                                             \
    if (!(cond)) {                                        \
      failed++;                                           \
      if (failed < 60) {                                  \
        printf("FAIL line %d: %s: ", __LINE__, #cond);    \
        printf(__VA_ARGS__);                              \
        printf("\n");                                     \
      }                                                   \
    }                                                     \
  } while (0)

#define SEED "scene-seed-c10"
#define NSEEDS_DEFAULT 24

/* ------------------------------------------------------------------ the SD card */

static int g_writes;             /* writes attempted since it was reset, by every module */
static int g_fail_at;            /* that write fails (nothing written); 0 = none */
static int g_fail_name;          /* 0: any file; 1: only custom; 2: only ptx; 3: only system */
static int g_fail_count;         /* writes failed so far */
static bool g_fail_all;          /* every write fails */
static int g_fail_from;          /* from this write on, every write fails (the power goes: 0 = never) */

static bool should_fail(const char* name) {
  g_writes++;
  if (g_fail_all) return true;
  if (g_fail_from && g_writes >= g_fail_from) return true;
  if (g_fail_at && g_writes == g_fail_at) return true;
  if (g_fail_name == 1 && !strcmp(name, "custom") && g_fail_at == -1) return true;
  if (g_fail_name == 2 && !strcmp(name, "ptx") && g_fail_at == -1) return true;
  return false;
}
static save_result f_read(const char* name, char** out, size_t* len, bool* bak) { return save_read(name, out, len, bak); }
static save_result f_write(const char* name, const void* data, size_t len) {
  if (should_fail(name)) {
    g_fail_count++;
    return SAVE_IO_ERROR;
  }
  return save_write(name, data, len);
}
static void faults(int at, int name, bool all) {
  g_writes = 0;
  g_fail_at = at;
  g_fail_name = name;
  g_fail_all = all;
  g_fail_count = 0;
  g_fail_from = 0;
}
static void crash_at(int n) {
  faults(0, 0, false);
  g_fail_from = n;
}
static void no_faults(void) { faults(0, 0, false); }

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

typedef struct {
  char name[96];
  uint8_t* bytes;
  size_t len;
} snap_file;
typedef struct {
  snap_file f[32];
  int n;
} snap;

static snap take(void) {
  snap s;
  memset(&s, 0, sizeof s);
  DIR* d = opendir(SAVE_DIR);
  struct dirent* e;
  while (d && (e = readdir(d)) && s.n < 32) {
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
static void drop(const snap* cs) {
  snap* s = (snap*)cs;
  for (int i = 0; i < s->n; i++) free(s->f[i].bytes);
  s->n = 0;
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
static const snap_file* find_file(const snap* s, const char* name) {
  for (int i = 0; i < s->n; i++)
    if (!strcmp(s->f[i].name, name)) return &s->f[i];
  return NULL;
}
static bool same_file(const snap* a, const snap* b, const char* name) {
  const snap_file *x = find_file(a, name), *y = find_file(b, name);
  if (!x || !y) return !x && !y;
  return x->len == y->len && !memcmp(x->bytes, y->bytes, x->len);
}
static void put_file(const char* name, const void* data, size_t len) {
  char path[600];
  snprintf(path, sizeof path, "%s/%s", SAVE_DIR, name);
  FILE* fp = fopen(path, "wb");
  fwrite(data, 1, len, fp);
  fclose(fp);
}

static unsigned vouchers(void) { return sysd_get()->voucher_counts[GC_VOUCHER_REGULAR]; }

/* A clean card, a clean set of modules, a saved default profile. */
static void sd_new(void) {
  wipe();
  no_faults();
  cr_end();
  sysd_reset();
  cs_reset();
  ptx_reset();
  static const cs_io cio = {f_read, f_write};
  static const sysd_io sio = {f_read, f_write};
  static const ptx_io pio = {f_read, f_write};
  cs_set_io(&cio);
  sysd_set_io(&sio);
  ptx_set_io(&pio);
  cs_set_nonce(0x5A5A0001u);
  ptx_set_scene_seed(SEED);
  gc_system* s = sysd_get();
  gc_system_init_defaults(s);
  for (int i = 0; i < gc_starter_cost_count; i++) { /* every starter, nature and ability open (as runtest's RUNTEST_SYS=all) */
    const uint16_t id = gc_starter_costs[i].id;
    s->dex[id].caught_attr = gc_species_full_unlocks_data(id) | 1ull;
    s->dex[id].seen_attr = s->dex[id].caught_attr;
    s->dex[id].nature_attr = 0x3FFFFFEu;
    s->starters[id].ability_attr = 7;
    s->starters[id].passive_attr = 3;
    for (int k = 0; k < 6; k++) s->dex[id].ivs[k] = 15;
  }
  CHECK(sysd_save(), "the first profile save");
  ptx_boot();
  co_set_uid_source(cs_begin_run);
  cr_stats_reset();
}

/* the same card after a restart: the modules forget what they read */
static void reboot(void) {
  no_faults();
  cr_end();
  sysd_reset();
  cs_reset();
  ptx_reset();
  ptx_set_scene_seed(SEED);
  ptx_boot();
  cr_settle_owed();
}

/* ------------------------------------------------------------------ records */

static cc_nemesis make_record(uint16_t species, int8_t gender, uint16_t ability, int n_moves, const uint16_t* moves) {
  cc_nemesis n;
  memset(&n, 0, sizeof n);
  n.life = CC_LIFE_STORED;
  n.known = CC_NK_ABILITY | CC_NK_PASSIVE | CC_NK_MOVES | CC_NK_IVS | CC_NK_NATURE | CC_NK_LOADOUT | CC_NK_FINISH_MOVE | CC_NK_FALLEN |
            CC_NK_FINISHER_VERIFIED;
  n.source_wave = 30;
  n.source_biome = 1;
  n.species = species;
  n.gender = gender;
  n.orig_level = 31;
  n.nature = 3;
  n.ability = ability;
  for (int i = 0; i < 6; i++) n.ivs[i] = (uint8_t)(10 + i);
  n.n_moves = (uint8_t)n_moves;
  for (int i = 0; i < n_moves; i++) n.moves[i] = moves[i];
  n.finish_move = moves[0];
  n.fallen_species = 1;
  return n;
}

typedef struct {
  cc_nemesis n;
  cc_record_id id;
} put_ctx;
static cc_status tx_put(cc_meta* m, void* c) {
  put_ctx* x = c;
  cc_run_uid uid;
  cc_status st = cc_alloc_run_uid(&m->counters, &uid);
  if (st != CC_OK) return st;
  cc_record_id id;
  st = cc_alloc_record_id(&m->counters, &id);
  if (st != CC_OK) return st;
  x->n.record_id = id;
  x->n.source_run_uid = uid;
  x->n.source_pokemon_id = 1000 + id;
  x->id = id;
  if (m->roster.count == CC_MAX_NEMESES) return CC_E_COUNT;
  m->roster.rec[m->roster.count++] = x->n;
  return CC_OK;
}
/* Puts a record straight into the roster (a fixture: the real way in is a real defeat, check 1). */
static cc_record_id put_record(const cc_nemesis* n) {
  put_ctx x = {*n, 0};
  cc_status st;
  const cs_tx_outcome o = cs_txn(tx_put, &x, &st);
  CHECK(o == CS_TX_DONE, "put a record (%d, %s)", (int)o, cc_status_str(st));
  return x.id;
}
static const cc_nemesis* record(cc_record_id id) {
  const cc_meta* m = cs_meta();
  for (unsigned i = 0; m && i < m->roster.count; i++)
    if (m->roster.rec[i].record_id == id) return &m->roster.rec[i];
  return NULL;
}
static int life_of(cc_record_id id) {
  const cc_nemesis* n = record(id);
  return n ? n->life : -1;
}
static cc_record_id put_default(void) {
  static const uint16_t mv[2] = {MOVE_ID_TACKLE, MOVE_ID_SCRATCH};
  /* RATTATA: Run Away (50) / Guts (62) / Hustle (55 hidden): ability 62 */
  cc_nemesis n = make_record(SPECIES_ID_RATTATA, 1, 62, 2, mv);
  return put_record(&n);
}

/* ------------------------------------------------------------------ the driver (what run_screen.c does with the run) */

static gc_run RUN, RUN2;
static gc_run* R = &RUN;
static gc_event Q[GC_MAX_EVENTS];
static uint32_t drain_no;
static bool save_due;
static int slot_used;
static uint64_t H;
static void mix(uint64_t v) {
  H ^= v + 0x9E3779B97F4A7C15ull + (H << 6) + (H >> 2);
  H *= 1099511628211ull;
}
static void mix_str(const char* s) {
  if (s)
    while (*s) mix((uint8_t)*s++);
  else
    mix(0xFF);
}
static void mix_rng(const pr_rng* g) {
  uint64_t v;
  const double d[5] = {g->c, g->s0, g->s1, g->s2, g->n};
  for (int i = 0; i < 5; i++) {
    memcpy(&v, &d[i], sizeof v);
    mix(v);
  }
}

static bool hashing;
static int n_events_total;
static void drain(void) {
  int nq = 0;
  while (nq < GC_MAX_EVENTS && gc_run_next_event(R, &Q[nq])) nq++;
  co_observe(++drain_no, Q, nq, R->battle_events, R->n_battle_events, R->battle_events_overflow, R->ev_overflow);
  for (int i = 0; i < nq; i++) {
    n_events_total++;
    if (Q[i].kind == GC_EV_SAVE_POINT) save_due = true;
    if (hashing) {
      mix(Q[i].kind);
      mix((uint64_t)(uint32_t)Q[i].a);
      mix((uint64_t)(uint32_t)Q[i].b);
      mix((uint64_t)(uint32_t)Q[i].c);
      mix_str(Q[i].msg.key);
    }
  }
  if (hashing)
    for (int i = 0; i < R->n_battle_events; i++) {
      const pr_event* e = &R->battle_events[i];
      mix(e->type);
      mix((uint64_t)(uint32_t)e->battler);
      mix((uint64_t)(uint32_t)e->id);
      mix((uint64_t)(int64_t)e->value);
      mix_str(e->msg.key);
    }
}

static void heal(gc_run* r) {
  for (int i = 0; i < r->n_party; i++) r->party[i].b.hp = r->party[i].b.stats[0];
}
static void strengthen(gc_run* r, int level) {
  for (int i = 0; i < r->n_party; i++) {
    r->party[i].b.level = (uint8_t)level;
    gc_mon_recalc(r, &r->party[i]);
    r->party[i].b.hp = r->party[i].b.stats[0];
  }
}

/* How the bot plays */
typedef struct {
  bool heal;               /* heal the party at every command (a strong party that cannot lose) */
  bool wipe;               /* play to lose: Splash, HP 1 */
  bool flee_rematch;       /* try to run from a rematch enemy */
  int offer_answer;        /* 1 accept, 0 decline */
  int save_every;          /* save into slot_used at each save point (every n-th, 0 = never) */
  int stop_wave;           /* stop the run at the COMMAND screen of this wave (0 = never) */
  bool stop_offer;         /* stop at the offer screen, unanswered */
  bool stop_after_enemy;   /* stop at the first COMMAND screen of the wave with a rematch enemy built */
  bool stop_at_save;       /* with stop_wave: only once the wave's save point has been written */
  int max_steps;
} bot_t;
static bot_t bot;
static int saves_written, save_points;
static int offers_seen;
static char last_msg[160];
static int n_msgs;

typedef enum { STEP_OK, STEP_OVER, STEP_CLEAR, STEP_STUCK, STEP_STOPPED } step_t;

static void set_one_move(gc_mon* m, int move) {
  gc_mon_set_move(m, 0, move);
  m->b.n_moves = 1;
  m->b.pp_used[0] = 0;
}

static step_t step(void) {
  const gc_screen* s = gc_run_screen(R);
  cr_on_screen(R, s->kind);
  const char* note = cr_take_message();
  if (note) {
    snprintf(last_msg, sizeof last_msg, "%s", note);
    n_msgs++;
  }
  gc_choice c;
  memset(&c, 0, sizeof c);
  const char* why = "";
  switch (s->kind) {
    case GC_SCREEN_COMMAND:
      if (save_due && gc_run_can_save(R)) {
        save_due = false;
        save_points++;
        if (bot.save_every && save_points % bot.save_every == 0) {
          CHECK(cr_save(slot_used, R) == RSV_SAVED, "the custom save at wave %d", R->wave);
          saves_written++;
        }
      }
      if (bot.stop_wave && R->wave == bot.stop_wave) return STEP_STOPPED;
      if (bot.stop_after_enemy) {
        cr_view v;
        cr_view_get(&v);
        if (v.rematch_enemy) return STEP_STOPPED;
      }
      if (bot.heal) heal(R);
      if (bot.wipe) {
        gc_mon* me = &R->party[R->party_order[0]];
        set_one_move(me, MOVE_ID_SPLASH);
        me->b.hp = 1;
      }
      c.kind = GC_CH_FIGHT;
      c.b = -1;
      for (int i = 0; i < 4; i++)
        if (s->move_ok[i]) {
          c.a = (int16_t)i;
          break;
        }
      if (bot.flee_rematch && cr_hooks() && s->can_run) {
        cr_view v;
        cr_view_get(&v);
        if (v.rematch_enemy) c.kind = GC_CH_RUN;
      }
      break;
    case GC_SCREEN_SWITCH:
      c.kind = GC_CH_PICK;
      c.a = -1;
      for (int i = 0; i < R->n_party; i++)
        if (s->pick_ok[i]) {
          c.a = (int16_t)i;
          break;
        }
      break;
    case GC_SCREEN_LEARN_MOVE: c.kind = GC_CH_FORGET; c.a = -1; break;
    case GC_SCREEN_EVOLVE: c.kind = GC_CH_CANCEL; c.a = 1; break;
    case GC_SCREEN_REWARD: c.kind = GC_CH_SKIP; break;
    case GC_SCREEN_BIOME: c.kind = GC_CH_BIOME; break;
    case GC_SCREEN_MYSTERY: c.kind = GC_CH_MYSTERY; c.b = 0; break;
    case GC_SCREEN_CHECK_SWITCH: c.kind = GC_CH_CONFIRM; c.a = 0; break;
    case GC_SCREEN_CUSTOM_OFFER:
      if (bot.stop_offer) return STEP_STOPPED;
      offers_seen++;
      c.kind = GC_CH_CONFIRM;
      c.a = (int16_t)bot.offer_answer;
      break;
    case GC_SCREEN_GAME_OVER: return STEP_OVER;
    case GC_SCREEN_CLEAR: return STEP_CLEAR;
    default: return STEP_STUCK;
  }
  if (!gc_run_choose(R, &c, &why)) return STEP_STUCK;
  drain();
  return STEP_OK;
}

static cs_result g_commit;
static bool g_have_defeat;
static cs_defeat g_defeat;
/* the end of a run, as run_screen.c's end_run and app_flow.c's on_run_end do it */
static void end_run(int kind, bool clear_slot) {
  co_finish(kind == GC_SCREEN_CLEAR, R->biome, R->wave);
  g_have_defeat = cr_active() && cr_on_run_end(R, kind, &g_defeat);
  const bool was_custom = cr_active();
  cr_end();
  const co_report* rep = co_report_get();
  g_commit = cs_commit_defeat(rep, g_have_defeat ? &g_defeat : NULL);
  if (was_custom && clear_slot) cr_clear(slot_used);
}

static step_t run_until(int max_steps) {
  for (int i = 0; i < max_steps; i++) {
    const step_t s = step();
    if (s != STEP_OK) return s;
  }
  return STEP_STUCK;
}

static const gc_starter* team3(void) {
  static gc_starter team[3];
  const int species[3] = {SPECIES_ID_BULBASAUR, SPECIES_ID_CHARMANDER, SPECIES_ID_SQUIRTLE};
  memset(team, 0, sizeof team);
  for (int i = 0; i < 3; i++) {
    team[i].species = (uint16_t)species[i];
    team[i].tera_type = -1;
    for (int k = 0; k < 6; k++) team[i].ivs[k] = 15;
  }
  return team;
}

/* a new run: custom (the controller's hooks) or ordinary. false: the custom run could not be started */
static bool new_run(const char* seed, bool custom, int level) {
  memset(R, 0, sizeof *R);
  drain_no = 0;
  save_due = false;
  save_points = 0;
  saves_written = 0;
  offers_seen = 0;
  g_have_defeat = false;
  memset(&bot, 0, sizeof bot);
  bot.offer_answer = 1;
  co_begin_run(false);
  gc_run_new(R, GAME_MODES_CLASSIC, seed, 7);
  R->sys = sysd_get();
  if (custom) {
    if (!cr_begin(R, 7)) return false;
  } else {
    cr_end();
  }
  const char* why = "";
  CHECK(gc_run_set_starters(R, team3(), 3, &why), "starters: %s", why ? why : "");
  if (level) strengthen(R, level);
  drain();
  return true;
}

/* continue a custom run from its slot into the second run object */
static rsv_status resume_run(int slot, char why[RSV_WHY_LEN]) {
  gc_run* other = R == &RUN ? &RUN2 : &RUN;
  const rsv_status st = cr_load(slot, other, why);
  if (st != RSV_OK) return st;
  R = other;
  R->sys = sysd_get();
  cr_begin_observer();
  drain_no = 0;
  save_due = false;
  drain();
  for (int i = 0; i < GC_MAX_EVENTS; i++) (void)i;
  return RSV_OK;
}

/* ------------------------------------------------------------------ 1. a defeat creates a Nemesis */

typedef struct {
  bool have;
  uint32_t id;
  uint16_t species;
  uint8_t form, level, nature;
  int8_t gender;
  bool shiny;
  uint16_t ability, passive;
  uint8_t ivs[6];
  uint8_t n_moves;
  uint16_t moves[4];
  int n_items;
  struct { char key[40]; int16_t arg; int32_t stack; } items[8];
  int wave;
} enemy_snap;

static enemy_snap snap_enemy(const gc_run* r) {
  enemy_snap e;
  memset(&e, 0, sizeof e);
  if (r->n_enemy < 1) return e;
  const pr_pokemon* p = &r->enemy[0].b;
  e.have = true;
  e.id = p->id;
  e.species = p->species;
  e.form = (uint8_t)p->form;
  e.level = (uint8_t)p->level;
  e.nature = p->nature;
  e.gender = (int8_t)p->gender;
  e.shiny = p->shiny;
  e.ability = p->base_ability;
  e.passive = p->passive;
  memcpy(e.ivs, p->ivs, 6);
  e.n_moves = p->n_moves;
  for (int i = 0; i < p->n_moves && i < 4; i++) e.moves[i] = p->moves[i];
  for (int i = 0; i < r->n_enemy_items && e.n_items < 8; i++)
    if (r->enemy_items[i].pokemon_id == p->id && r->enemy_items[i].key) {
      snprintf(e.items[e.n_items].key, sizeof e.items[0].key, "%s", r->enemy_items[i].key);
      e.items[e.n_items].arg = r->enemy_items[i].arg;
      e.items[e.n_items].stack = r->enemy_items[i].stack;
      e.n_items++;
    }
  e.wave = r->wave;
  return e;
}

static int n_created, n_unattributed, n_refused_store;
static void test_defeat_creates(int nseeds) {
  for (int s = 0; s < nseeds; s++) {
    sd_new();
    char seed[32];
    snprintf(seed, sizeof seed, "c10-defeat-%d", s);
    CHECK(new_run(seed, true, 0), "start a custom run");
    bot.wipe = true;
    enemy_snap first = snap_enemy(R);
    bool over = false;
    for (int i = 0; i < 400 && !over; i++) {
      const step_t st = step();
      if (st == STEP_OVER) over = true;
      else if (st != STEP_OK) break;
    }
    if (!over) continue;   /* an enemy with no damaging move: this seed does not end */
    end_run(GC_SCREEN_GAME_OVER, true);
    CHECK(R->wave == first.wave, "the run ended where it began");
    CHECK(g_commit.outcome == CS_STORED, "the report is stored (%d)", (int)g_commit.outcome);
    const cc_meta* m = cs_meta();
    if (g_have_defeat && g_commit.commit.refusal != CC_SUPPORT_OK) {
      /* a finisher named, but the encounter was one the store does not keep (a double roll, a fusion, a Tera...): no record */
      n_refused_store++;
      CHECK(m->roster.count == 0, "refused (%s): no record", cc_refusal_str(g_commit.commit.refusal));
      CHECK(g_commit.commit.refusal == CC_REFUSE_DOUBLES || g_commit.commit.refusal == CC_REFUSE_FUSED ||
                g_commit.commit.refusal == CC_REFUSE_TERA || g_commit.commit.refusal == CC_REFUSE_BOSS ||
                g_commit.commit.refusal == CC_REFUSE_TRAINER_OWNED || g_commit.commit.refusal == CC_REFUSE_ROSTER_FULL,
            "the refusal is one the encounter explains (%s)", cc_refusal_str(g_commit.commit.refusal));
    } else if (g_have_defeat) {
      n_created++;
      CHECK(m->roster.count == 1 && g_commit.commit.record_id == m->roster.rec[0].record_id, "one record, the one the commit named (count %u, status %d, refusal %d)", m->roster.count, (int)g_commit.status, (int)g_commit.commit.refusal);
      const cc_nemesis* n = &m->roster.rec[0];
      CHECK(n->life == CC_LIFE_STORED && n->traits == 0, "stored, no traits");
      CHECK(n->species == first.species && n->form == first.form && n->gender == first.gender && n->shiny == (first.shiny ? 1 : 0),
            "the identity is the enemy's");
      CHECK(n->orig_level == first.level, "its level as it was (%d, %d)", n->orig_level, first.level);
      CHECK((n->known & CC_NK_NATURE) && n->nature == first.nature, "nature");
      CHECK((n->known & CC_NK_ABILITY) && n->ability == first.ability, "ability (%d, %d)", n->ability, first.ability);
      CHECK((n->known & CC_NK_PASSIVE) && n->passive == first.passive, "passive");
      CHECK((n->known & CC_NK_IVS) && !memcmp(n->ivs, first.ivs, 6), "ivs");
      CHECK((n->known & CC_NK_MOVES) && n->n_moves == first.n_moves && !memcmp(n->moves, first.moves, first.n_moves * 2u), "moves");
      CHECK((n->known & CC_NK_LOADOUT) && n->n_items == first.n_items, "item count (%d, %d)", n->n_items, first.n_items);
      for (int i = 0; i < n->n_items && i < first.n_items; i++)
        CHECK(!strcmp(n->items[i].key, first.items[i].key) && n->items[i].arg == first.items[i].arg && n->items[i].stack == first.items[i].stack,
              "item %d", i);
      CHECK(n->known & CC_NK_FINISHER_VERIFIED, "the finisher is verified");
      CHECK((n->known & CC_NK_FINISH_MOVE) && n->finish_move >= 1, "the finishing move is known");
      CHECK((n->known & CC_NK_FALLEN) && n->fallen_species == R->party[R->party_order[0]].b.species, "the fallen Pokemon");
      CHECK(n->source_wave == first.wave && n->source_pokemon_id == first.id, "origin: wave, Pokemon id");
      CHECK(cc_nemesis_validate_stored(n) == CC_OK, "it validates as stored");
      CHECK(m->has_report && (m->report.known & CC_TK_FINISH_ACTOR) && m->report.finish_actor.id == first.id &&
                m->report.finish_actor.side == CC_SIDE_ENEMY,
            "the report names the finisher");
    } else {
      n_unattributed++;
      CHECK(m->roster.count == 0, "no reliable finisher: no record");
      CHECK(!(m->report.known & CC_TK_FINISH_ACTOR), "and the report says nothing about who");
    }
  }
}

/* an ordinary run's defeat creates nothing, however it ends */
static void test_ordinary_creates_none(void) {
  for (int s = 0; s < 6; s++) {
    sd_new();
    char seed[32];
    snprintf(seed, sizeof seed, "c10-defeat-%d", s);
    new_run(seed, false, 0);
    bot.wipe = true;
    step_t st = STEP_OK;
    for (int i = 0; i < 400 && st == STEP_OK; i++) st = step();
    if (st != STEP_OVER) continue;
    end_run(GC_SCREEN_GAME_OVER, false);
    CHECK(!g_have_defeat, "an ordinary run names no finisher");
    CHECK(cs_meta()->roster.count == 0, "an ordinary run's defeat creates no record");
    CHECK(g_commit.outcome == CS_STORED && !(cs_meta()->report.known & CC_TK_FINISH_ACTOR), "its report keeps an unknown cause, as before");
  }
}

/* ------------------------------------------------------------------ 4. a real rematch */

static void reboot_io(void) {
  static const cs_io cio = {f_read, f_write};
  static const sysd_io sio = {f_read, f_write};
  static const ptx_io pio = {f_read, f_write};
  cs_set_io(&cio);
  sysd_set_io(&sio);
  ptx_set_io(&pio);
}

/* A Nemesis from a real defeat (check 1's way in): its record id and a copy. false: none in `tries` seeds. */
static bool real_nemesis(int first, int tries, cc_record_id* id, cc_nemesis* copy) {
  for (int s = first; s < first + tries; s++) {
    char seed[32];
    snprintf(seed, sizeof seed, "c10-nem-%d", s);
    if (!new_run(seed, true, 0)) return false;
    bot.wipe = true;
    step_t st = STEP_OK;
    for (int i = 0; i < 400 && st == STEP_OK; i++) st = step();
    if (st != STEP_OVER) {
      cr_end();
      continue;
    }
    end_run(GC_SCREEN_GAME_OVER, true);
    if (g_have_defeat && g_commit.outcome == CS_STORED && g_commit.commit.refusal == CC_SUPPORT_OK) {
      *id = g_commit.commit.record_id;
      *copy = *record(*id);
      return true;
    }
  }
  return false;
}

enum { RL_OFFER = 1, RL_ACCEPT = 2, RL_WIN = 4, RL_ALL = 7 };
enum { FIGHT_WIN, FIGHT_LOSE, FIGHT_FLEE };

typedef struct {
  int attempt;       /* the run's seed */
  bool custom;       /* a custom run (the controller) or an ordinary one */
  int answer;        /* the offer: 1 accept, 0 decline */
  int fight;         /* FIGHT_*: how the rematch enemy is met */
  int reload;        /* RL_*: where the run is saved and loaded again */
  int end_wave;      /* stop at this wave's command screen */
  cc_record_id rid;  /* the Nemesis (0: none expected) */
} rm_cfg;

typedef struct {
  bool offered;                 /* the offer screen came */
  bool offer_stop;              /* ... and the run was stopped at it, unanswered, to load the last save */
  bool rematch;                 /* a rematch enemy was built */
  step_t end;                   /* how the run stopped: STEP_STOPPED at end_wave, STEP_OVER the run was lost */
  enemy_snap enemy27;           /* the enemy at the first command screen of wave 27 */
  uint64_t h27, rng27;          /* the events hash and the random numbers' state there */
  unsigned v_before, v_after;   /* regular vouchers */
  int life_end;                 /* the record's life when the run stopped (-1: none) */
  int wave_end;
  int msgs;                     /* the voucher announcements the player was shown */
  cr_stats stats;
  cs_result commit;             /* the report of a lost run */
  bool have_defeat;
  uint64_t rng_end;
} rm_out;

static uint64_t rng_state(void) {
  const uint64_t h = H;
  H = 7;
  mix_rng(&R->run.rng);
  mix_rng(&R->bt.rng);
  const uint64_t r = H;
  H = h;
  return r;
}

/* One seeded run to wave `end_wave`, the way run_screen.c would play it, with a party of level 100 that heals at every command (so it wins
 * the ordinary waves), the offer answered `answer` and the rematch enemy met `fight`. */
static void rm_play(const rm_cfg* c, rm_out* o) {
  memset(o, 0, sizeof *o);
  char seed[32];
  snprintf(seed, sizeof seed, "c10-rematch-%d", c->attempt);
  hashing = true;
  CHECK(new_run(seed, c->custom, 100), "start the run");
  H = 1469598103934665603ull;
  bot.heal = true;
  bot.offer_answer = c->answer;
  bot.save_every = c->reload ? 1 : 0;
  bot.stop_wave = 27;
  bot.stop_offer = (c->reload & RL_OFFER) != 0;
  cr_stats_reset();
  n_msgs = 0;
  o->v_before = vouchers();
  int phase = 0;
  bool done = false;
  o->end = STEP_STUCK;
  for (int guard = 0; guard < 60000 && !done; guard++) {
    const step_t st = step();
    if (st == STEP_OK) {
      cr_view v;
      cr_view_get(&v);
      if (v.rematch_enemy) {
        o->rematch = true;
        if (c->fight == FIGHT_LOSE) {
          bot.heal = false;
          bot.wipe = true;
        }
        if (c->fight == FIGHT_FLEE) {
          bot.flee_rematch = true;
        }
      }
      continue;
    }
    if (st == STEP_STOPPED) {
      if (gc_run_screen(R)->kind == GC_SCREEN_CUSTOM_OFFER) {
        /* the offer waits: no save is possible; a load of the last save brings the same offer back */
        o->offer_stop = true;
        CHECK(!gc_run_can_save(R), "no save while the offer waits");
        CHECK(R->wave == 27, "the offer is at the first window, wave %d", R->wave);
        bot.stop_offer = false;
        char why[RSV_WHY_LEN];
        CHECK(resume_run(slot_used, why) == RSV_OK, "load the last save: %s", why);
        continue;
      }
      if (phase == 0) {
        o->enemy27 = snap_enemy(R);
        o->h27 = H;
        o->rng27 = rng_state();
        if (c->reload & RL_ACCEPT) {
          char why[RSV_WHY_LEN];
          CHECK(resume_run(slot_used, why) == RSV_OK, "load the save of the rematch's first turn: %s", why);
          const enemy_snap again = snap_enemy(R);
          CHECK(!memcmp(&again, &o->enemy27, sizeof again), "the enemy after the load is the enemy that was saved");
          CHECK(rng_state() == o->rng27, "the random numbers after the load are those that were saved");
        }
        phase = 1;
        bot.stop_wave = 28;
        continue;
      }
      if (phase == 1) {
        if (c->reload & RL_WIN) {
          char why[RSV_WHY_LEN];
          CHECK(resume_run(slot_used, why) == RSV_OK, "load the save after the rematch: %s", why);
        }
        phase = 2;
        bot.stop_wave = c->end_wave;
        continue;
      }
      done = true;
      o->end = STEP_STOPPED;
      break;
    }
    o->end = st;
    break;
  }
  o->wave_end = R->wave;
  o->rng_end = rng_state();
  if (o->end == STEP_OVER) {
    end_run(GC_SCREEN_GAME_OVER, true);
    o->commit = g_commit;
    o->have_defeat = g_have_defeat;
  }
  o->v_after = vouchers();
  o->life_end = c->rid ? life_of(c->rid) : -1;
  o->msgs = n_msgs;
  o->stats = *cr_stats_get();
  o->offered = o->stats.offers > 0;
  hashing = false;
}


static void test_rematch_real(void) {
  int shown = 0;
  for (int attempt = 0; attempt < 16 && shown < 3; attempt++) {
    sd_new();
    cc_record_id rid = 0;
    cc_nemesis nem;
    memset(&nem, 0, sizeof nem);
    if (!real_nemesis(attempt * 40, 40, &rid, &nem)) {
      CHECK(false, "a Nemesis from a real defeat");
      return;
    }
    CHECK(nem.life == CC_LIFE_STORED, "the Nemesis is stored (%d)", nem.life);
    cr_end();
    const snap base = take();
    sysd_save();
    const snap base2 = take();   /* the profile with the first run's data */

    /* the control: an ordinary run, the same seed, nothing of the controller */
    restore(&base2);
    reboot_io();
    reboot();
    rm_cfg cfg = {attempt, false, 1, FIGHT_WIN, 0, 29, 0};
    rm_out ctl;
    rm_play(&cfg, &ctl);
    if (ctl.end != STEP_STOPPED || ctl.enemy27.species == 0) {
      drop(&base);
      drop(&base2);
      continue;
    }

    /* custom, an accepted rematch, uninterrupted */
    restore(&base2);
    reboot_io();
    reboot();
    cfg = (rm_cfg){attempt, true, 1, FIGHT_WIN, 0, 29, rid};
    rm_out acc;
    rm_play(&cfg, &acc);
    if (!acc.offered) {
      /* wave 27 was not an ordinary single wild encounter: this seed has no offer at its first window */
      drop(&base);
      drop(&base2);
      continue;
    }
    shown++;
    printf("   seed %d: Nemesis sp %d L%d -> rematch enemy sp %d L%d (control L%d sp %d), wave_end %d, vouchers %u->%u\n", attempt, nem.species, nem.orig_level, acc.enemy27.species, acc.enemy27.level, ctl.enemy27.level, ctl.enemy27.species, acc.wave_end, acc.v_before, acc.v_after);
    CHECK(acc.end == STEP_STOPPED && acc.wave_end == 29, "the accepted run went on to wave 29 (%d, %d)", (int)acc.end, acc.wave_end);
    CHECK(acc.rematch && acc.stats.offers == 1 && acc.stats.accepts == 1 && acc.stats.built == 1 && acc.stats.refused == 0,
          "one offer, accepted, built (%u %u %u %u)", acc.stats.offers, acc.stats.accepts, acc.stats.built, acc.stats.refused);
    const enemy_snap* e = &acc.enemy27;
    CHECK(e->species == nem.species && e->form == nem.form, "the enemy is the record's species and form (%d/%d, %d/%d)", e->species, e->form,
          nem.species, nem.form);
    CHECK(e->gender == nem.gender && (e->shiny ? 1 : 0) == nem.shiny, "gender and shininess");
    CHECK(e->nature == nem.nature, "nature");
    CHECK(e->ability == nem.ability, "ability (%d, %d)", e->ability, nem.ability);
    CHECK(e->n_moves == nem.n_moves && !memcmp(e->moves, nem.moves, nem.n_moves * 2u), "the record's moves, none replaced");
    CHECK(e->level == ctl.enemy27.level, "the ordinary generated level (%d, control %d)", e->level, ctl.enemy27.level);
    CHECK(e->wave == 27, "wave 27");

    /* decline: the ordinary encounter, as the control had it */
    restore(&base2);
    reboot_io();
    reboot();
    cfg = (rm_cfg){attempt, true, 0, FIGHT_WIN, 0, 29, rid};
    rm_out dec;
    rm_play(&cfg, &dec);
    CHECK(dec.offered, "declined run saw the offer");
    CHECK(dec.stats.declines == 1 && !dec.rematch && dec.stats.built == 0, "declined, nothing built");
    CHECK(dec.enemy27.species == ctl.enemy27.species && dec.enemy27.level == ctl.enemy27.level &&
              !memcmp(dec.enemy27.ivs, ctl.enemy27.ivs, 6) && dec.enemy27.nature == ctl.enemy27.nature &&
              dec.enemy27.ability == ctl.enemy27.ability && dec.enemy27.n_moves == ctl.enemy27.n_moves &&
              !memcmp(dec.enemy27.moves, ctl.enemy27.moves, 8) && dec.enemy27.n_items == ctl.enemy27.n_items,
          "the declined encounter is the ordinary one (%d L%d, control %d L%d)", dec.enemy27.species, dec.enemy27.level, ctl.enemy27.species,
          ctl.enemy27.level);
    CHECK(dec.rng27 == ctl.rng27, "and it consumed the same random numbers");
    CHECK(dec.life_end == CC_LIFE_STORED, "declined: the record went back to the roster (%d)", dec.life_end);
    CHECK(dec.v_after == dec.v_before && dec.msgs == 0, "declined: no voucher");
    CHECK(dec.wave_end == 29 && dec.rng_end == ctl.rng_end, "declined: the run goes on exactly as the control (rng at wave 29)");

    /* the control with the controller and an empty roster: identical to the ordinary run, in events and random numbers */
    sd_new();
    cfg = (rm_cfg){attempt, true, 1, FIGHT_WIN, 0, 29, 0};
    rm_out emp;
    rm_play(&cfg, &emp);
    CHECK(!emp.offered && emp.stats.offers == 0, "an empty roster: no offer");
    CHECK(emp.h27 == ctl.h27 && emp.rng27 == ctl.rng27 && emp.rng_end == ctl.rng_end, "an empty roster: the run is the ordinary run (events and rng)");
    cr_end();

    /* a won rematch pays one voucher and is recorded as done */
    CHECK(acc.life_end == CC_LIFE_REWARD_CLAIMED, "the won rematch's record is rewarded (%d)", acc.life_end);
    CHECK(acc.v_after == acc.v_before + 1 && acc.msgs == 1, "one voucher, announced once (%u -> %u, %d)", acc.v_before, acc.v_after, acc.msgs);
    CHECK(acc.stats.wins == 1 && acc.stats.losses == 0 && acc.stats.claims == 1, "one win, one claim (%u %u %u)", acc.stats.wins, acc.stats.losses,
          acc.stats.claims);

    /* a lost rematch (the party falls): the record is back, no voucher, and the rematch enemy does not make a Nemesis */
    restore(&base2);
    reboot_io();
    reboot();
    cfg = (rm_cfg){attempt, true, 1, FIGHT_LOSE, 0, 29, rid};
    rm_out lose;
    rm_play(&cfg, &lose);
    CHECK(lose.end == STEP_OVER && lose.wave_end == 27, "the party fell at the rematch (%d, %d)", (int)lose.end, lose.wave_end);
    CHECK(lose.life_end == CC_LIFE_STORED, "lost: the record is back in the roster (%d)", lose.life_end);
    CHECK(lose.v_after == lose.v_before && lose.msgs == 0, "lost: no voucher");
    CHECK(lose.stats.losses == 1 && lose.stats.wins == 0, "lost: counted once");
    CHECK(cs_meta()->roster.count == 1, "lost: no new record (the rematch enemy is never a Nemesis) (%u)", cs_meta()->roster.count);
    CHECK(!lose.have_defeat || lose.commit.commit.refusal == CC_REFUSE_REMATCH, "lost: any finisher named is refused as a rematch (%d)",
          lose.commit.commit.refusal);

    /* fled: lost as well, the run goes on */
    restore(&base2);
    reboot_io();
    reboot();
    cfg = (rm_cfg){attempt, true, 1, FIGHT_FLEE, 0, 29, rid};
    rm_out flee;
    rm_play(&cfg, &flee);
    CHECK(flee.life_end == CC_LIFE_STORED && flee.v_after == flee.v_before, "fled: the record is back, no voucher (%d)", flee.life_end);
    CHECK(flee.stats.wins == 0 && flee.stats.losses == 1, "fled: counted as lost");

    /* the same rematch with the run saved and loaded: at the offer, at the first turn, after the win, and at all of them */
    static const int masks[4] = {RL_OFFER, RL_ACCEPT, RL_WIN, RL_ALL};
    for (int k = 0; k < 4; k++) {
      restore(&base2);
      reboot_io();
      reboot();
      cfg = (rm_cfg){attempt, true, 1, FIGHT_WIN, masks[k], 29, rid};
      rm_out rl;
      rm_play(&cfg, &rl);
      CHECK(rl.end == STEP_STOPPED && rl.wave_end == 29, "reload mask %d: reached wave 29 (%d)", masks[k], rl.wave_end);
      CHECK(!!(masks[k] & RL_OFFER) == rl.offer_stop, "reload mask %d: the offer was waiting when it should", masks[k]);
      CHECK(!memcmp(&rl.enemy27, &acc.enemy27, sizeof rl.enemy27), "reload mask %d: the same enemy as the uninterrupted run", masks[k]);
      CHECK(rl.life_end == CC_LIFE_REWARD_CLAIMED, "reload mask %d: rewarded (%d)", masks[k], rl.life_end);
      CHECK(rl.v_after == rl.v_before + 1, "reload mask %d: exactly one voucher (%u -> %u)", masks[k], rl.v_before, rl.v_after);
      CHECK(rl.rng_end == acc.rng_end, "reload mask %d: the run ends where the uninterrupted one does", masks[k]);
    }
    /* declined with the run saved and loaded at the offer, after the decline, and later: the same run, no voucher, the record back */
    restore(&base2);
    reboot_io();
    reboot();
    cfg = (rm_cfg){attempt, true, 0, FIGHT_WIN, RL_ALL, 29, rid};
    rm_out dr;
    rm_play(&cfg, &dr);
    CHECK(dr.end == STEP_STOPPED && dr.wave_end == 29 && dr.offer_stop, "declined with loads: reached wave 29, the offer was waiting at the load");
    CHECK(dr.enemy27.species == ctl.enemy27.species && dr.enemy27.level == ctl.enemy27.level, "declined with loads: the ordinary enemy");
    CHECK(dr.life_end == CC_LIFE_STORED && dr.v_after == dr.v_before, "declined with loads: record stored, no voucher (%d)", dr.life_end);
    CHECK(dr.stats.offers == 2 && dr.stats.declines == 1, "declined with loads: asked again after the load of the older save (%u, %u)", dr.stats.offers,
          dr.stats.declines);

    /* lost and fled with loads at the first turn and later */
    restore(&base2);
    reboot_io();
    reboot();
    cfg = (rm_cfg){attempt, true, 1, FIGHT_LOSE, RL_ACCEPT, 29, rid};
    rm_out ll;
    rm_play(&cfg, &ll);
    CHECK(ll.end == STEP_OVER && ll.life_end == CC_LIFE_STORED && ll.v_after == ll.v_before, "lost after a load: over, record stored, no voucher (%d)", ll.life_end);
    rsv_info info;
    CHECK(cr_inspect(slot_used, &info) == RSV_EMPTY, "lost: the run's slot is gone");
    CHECK(cs_meta()->roster.count == 1, "lost after a load: still the one record (%u)", cs_meta()->roster.count);
    restore(&base2);
    reboot_io();
    reboot();
    cfg = (rm_cfg){attempt, true, 1, FIGHT_FLEE, RL_ACCEPT | RL_WIN, 29, rid};
    rm_out fl;
    rm_play(&cfg, &fl);
    CHECK(fl.life_end == CC_LIFE_STORED && fl.v_after == fl.v_before && fl.stats.wins == 0, "fled after a load: record stored, no voucher (%d)", fl.life_end);
    cr_end();
    drop(&base);
    drop(&base2);
  }
  CHECK(shown >= 2, "rematch scenarios played on %d seeds (>= 2 expected)", shown);
  printf("4  real rematch scenarios played on %d seeds\n", shown);
}

/* ------------------------------------------------------------------ 2. the finisher's gate */

typedef struct {
  const char* name;
  uint8_t kind, flags;         /* the encounter */
  bool direct, has_source;     /* the faint */
  int source_side;
  int remaining;
  int wave_off;                /* the faint's wave minus the run's */
  bool boss, fused, tera, trainer_owned;   /* the finisher */
  uint16_t changed_species;    /* the finisher's species at the end (0: unchanged) */
  uint16_t ability;
  uint8_t n_moves;
  bool wrong_species;
  bool second_faint_remaining; /* another player's Pokemon fell after it, with Pokemon left */
  /* expected */
  bool candidate;              /* the controller hands a candidate to the store */
  int refusal;                 /* ... and the store's answer (CC_SUPPORT_OK = a record) */
} gate_case;

static gc_custom_actor gate_actor(uint32_t id, uint16_t species, bool enemy) {
  gc_custom_actor a;
  memset(&a, 0, sizeof a);
  a.id = id;
  a.species = a.start_species = species;
  a.level = enemy ? 12 : 30;
  a.nature = 3;
  a.ability = enemy ? 62 : 65;
  a.passive = 0;
  for (int i = 0; i < 6; i++) a.ivs[i] = (uint8_t)(10 + i);
  a.n_moves = 2;
  a.moves[0] = MOVE_ID_TACKLE;
  a.moves[1] = MOVE_ID_SCRATCH;
  a.n_items = 1;
  a.items[0].key = "BERRY";
  a.items[0].arg = 2;
  a.items[0].stack = 1;
  return a;
}

static void test_gate(void) {
  static const gate_case T[] = {
      /* name                kind flags d  src side rem wv boss fus tera trn chg abil mv wrong second  cand refusal */
      {"ordinary wild",      GC_ENC_ORDINARY_WILD, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 62, 2, 0, 0, 1, CC_SUPPORT_OK},
      {"trainer battle",     GC_ENC_TRAINER, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 62, 2, 0, 0, 1, CC_REFUSE_TRAINER_OWNED},
      {"boss wave",          GC_ENC_BOSS, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 62, 2, 0, 0, 1, CC_REFUSE_BOSS},
      {"final wave",         GC_ENC_FINAL, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 62, 2, 0, 0, 1, CC_REFUSE_BOSS},
      {"mystery encounter",  GC_ENC_MYSTERY, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 62, 2, 0, 0, 1, CC_REFUSE_MYSTERY},
      {"fixed or scripted",  GC_ENC_FIXED_OR_SCRIPTED, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 62, 2, 0, 0, 1, CC_REFUSE_FIXED_SCRIPTED},
      {"unsupported kind",   GC_ENC_UNSUPPORTED, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 62, 2, 0, 0, 0, -1},
      {"fused wild",         GC_ENC_ORDINARY_WILD, GC_ENF_FUSED, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 62, 2, 0, 0, 1, CC_REFUSE_FUSED},
      {"terastallized wild", GC_ENC_ORDINARY_WILD, GC_ENF_TERA, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 62, 2, 0, 0, 1, CC_REFUSE_TERA},
      {"double roll",        GC_ENC_ORDINARY_WILD, GC_ENF_DOUBLES, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 62, 2, 0, 0, 1, CC_REFUSE_DOUBLES},
      {"boss actor",         GC_ENC_ORDINARY_WILD, 0, 1, 1, 1, 0, 0, 1, 0, 0, 0, 0, 62, 2, 0, 0, 1, CC_REFUSE_BOSS},
      {"fused actor",        GC_ENC_ORDINARY_WILD, 0, 1, 1, 1, 0, 0, 0, 1, 0, 0, 0, 62, 2, 0, 0, 1, CC_REFUSE_FUSED},
      {"tera actor",         GC_ENC_ORDINARY_WILD, 0, 1, 1, 1, 0, 0, 0, 0, 1, 0, 0, 62, 2, 0, 0, 1, CC_REFUSE_TERA},
      {"trainer-owned actor",GC_ENC_ORDINARY_WILD, 0, 1, 1, 1, 0, 0, 0, 0, 0, 1, 0, 62, 2, 0, 0, 1, CC_REFUSE_TRAINER_OWNED},
      {"not a direct hit",   GC_ENC_ORDINARY_WILD, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 62, 2, 0, 0, 0, -1},
      {"attacker gone",      GC_ENC_ORDINARY_WILD, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 62, 2, 0, 0, 0, -1},
      {"own side's attack",  GC_ENC_ORDINARY_WILD, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 62, 2, 0, 0, 0, -1},
      {"party not out",      GC_ENC_ORDINARY_WILD, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 62, 2, 0, 0, 0, -1},
      {"another fell after", GC_ENC_ORDINARY_WILD, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 62, 2, 0, 1, 0, -1},
      {"wave mismatch",      GC_ENC_ORDINARY_WILD, 0, 1, 1, 1, 0, 1, 0, 0, 0, 0, 0, 62, 2, 0, 0, 0, -1},
      {"species changed",    GC_ENC_ORDINARY_WILD, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 25, 62, 2, 0, 0, 0, -1},
      {"ability unknown",    GC_ENC_ORDINARY_WILD, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 1, CC_REFUSE_ABILITY_UNKNOWN},
      {"moves unknown",      GC_ENC_ORDINARY_WILD, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 62, 0, 0, 0, 1, CC_REFUSE_MOVES_UNKNOWN},
  };
  const unsigned nT = sizeof T / sizeof T[0];
  for (unsigned t = 0; t <= nT; t++) {
    /* the last pass is the first case again, but the run ends in a Classic clear: a clear names no finisher */
    const bool clear_end = t == nT;
    const gate_case* c = &T[clear_end ? 0 : t];
    const int cand = clear_end ? 0 : c->candidate;
    sd_new();
    char seed[32];
    snprintf(seed, sizeof seed, "c10-gate-%u", t);
    CHECK(new_run(seed, true, 0), "%s: start", c->name);
    const gc_custom_hooks* h = cr_hooks();
    gc_custom_encounter enc;
    memset(&enc, 0, sizeof enc);
    enc.wave = R->wave;
    enc.biome = R->biome;
    enc.kind = c->kind;
    enc.flags = c->flags;
    enc.level = 12;
    enc.species = SPECIES_ID_RATTATA;
    CHECK(!h->encounter(h->user, R, &enc), "%s: wave 1 is no window: no offer", c->name);
    gc_custom_faint f;
    memset(&f, 0, sizeof f);
    f.wave = R->wave + c->wave_off;
    f.turn = 3;
    f.side = 0;
    f.remaining = (uint8_t)c->remaining;
    f.direct = c->direct;
    f.move = MOVE_ID_TACKLE;
    f.fainted = gate_actor(7001, SPECIES_ID_BULBASAUR, false);
    f.has_source = c->has_source;
    f.source_side = (uint8_t)c->source_side;
    f.source = gate_actor(9001, SPECIES_ID_RATTATA, true);
    f.source.is_boss = c->boss;
    f.source.fused = c->fused;
    f.source.tera = c->tera;
    f.source.trainer_owned = c->trainer_owned;
    f.source.ability = c->ability;
    f.source.n_moves = c->n_moves;
    if (c->changed_species) f.source.species = c->changed_species;
    h->faint(h->user, R, &f);
    if (c->second_faint_remaining) {
      gc_custom_faint g = f;
      g.remaining = 2;
      g.fainted = gate_actor(7002, SPECIES_ID_CHARMANDER, false);
      h->faint(h->user, R, &g);
    }
    bot.wipe = false;
    end_run(clear_end ? GC_SCREEN_CLEAR : GC_SCREEN_GAME_OVER, true);
    CHECK(g_have_defeat == cand, "%s%s: candidate %d, expected %d", c->name, clear_end ? " (a clear)" : "", g_have_defeat, cand);
    CHECK(g_commit.outcome == CS_STORED || g_commit.outcome == CS_REPLAYED, "%s: the report is stored (%d)", c->name, (int)g_commit.outcome);
    const cc_meta* m = cs_meta();
    if (cand && c->refusal == CC_SUPPORT_OK) {
      CHECK(g_commit.commit.refusal == CC_SUPPORT_OK && m->roster.count == 1, "%s: a record (%s)", c->name, cc_refusal_str(g_commit.commit.refusal));
    } else if (cand) {
      CHECK(g_commit.commit.refusal == c->refusal && m->roster.count == 0, "%s: refused as %s, no record (got %s, %u)", c->name,
            cc_refusal_str(c->refusal), cc_refusal_str(g_commit.commit.refusal), m->roster.count);
    } else {
      CHECK(m->roster.count == 0, "%s: no record", c->name);
      CHECK(m->has_report && !(m->report.known & CC_TK_FINISH_ACTOR), "%s: the report does not say who finished the run", c->name);
    }
  }
  printf("2  finisher gate: %u cases and a clear\n", nT);
}

/* ------------------------------------------------------------------ 3. the windows, by hand */

/* The controller's hook for the encounter of `wave` (a window or not), the way game-core asks it. */
static bool at_wave(int wave, int kind, int flags) {
  const gc_custom_hooks* h = cr_hooks();
  gc_custom_encounter enc;
  memset(&enc, 0, sizeof enc);
  R->wave = wave;
  enc.wave = wave;
  enc.biome = R->biome;
  enc.kind = (uint8_t)kind;
  enc.flags = (uint8_t)flags;
  enc.level = 20;
  enc.species = SPECIES_ID_PIDGEY;
  return h->encounter(h->user, R, &enc);
}
static bool answer(bool yes, gc_custom_enemy* spec) {
  const gc_custom_hooks* h = cr_hooks();
  return h->answer(h->user, R, yes, spec);
}
static cr_view view(void) {
  cr_view v;
  cr_view_get(&v);
  return v;
}
static cc_record_id start_with_record(const char* seed) {
  sd_new();
  const cc_record_id id = put_default();
  CHECK(new_run(seed, true, 0), "start the run");
  return id;
}

static void test_windows(void) {
  /* no roster: the run is ordinary, and the card is not written for any of the four windows */
  {
    sd_new();
    CHECK(new_run("c10-win-none", true, 0), "start");
    CHECK(view().sched.state == CC_SCHED_NO_CANDIDATE, "no roster: no candidate (%d)", view().sched.state);
    const int w0 = g_writes;
    for (int w = 1; w <= 200; w++) CHECK(!at_wave(w, GC_ENC_ORDINARY_WILD, 0), "no roster: no offer at wave %d", w);
    CHECK(g_writes == w0, "no roster: nothing written for 200 waves (%d)", g_writes - w0);
    CHECK(cr_stats_get()->offers == 0, "no roster: no offer");
    cr_end();
  }
  /* each window offers when it is the first eligible one, and the ones before it were skipped for their reason */
  static const struct { int kinds[4]; int flags[4]; int first_offer; } W[] = {
      {{0, 0, 0, 0}, {0, 0, 0, 0}, 27},
      {{GC_ENC_TRAINER, 0, 0, 0}, {0, 0, 0, 0}, 77},
      {{GC_ENC_BOSS, GC_ENC_MYSTERY, 0, 0}, {0, 0, 0, 0}, 127},
      {{GC_ENC_FINAL, GC_ENC_FIXED_OR_SCRIPTED, GC_ENC_UNSUPPORTED, 0}, {0, 0, 0, 0}, 177},
      {{0, 0, 0, 0}, {GC_ENF_FUSED, GC_ENF_TERA, GC_ENF_DOUBLES, 0}, 177},
  };
  static const int WAVES[4] = {27, 77, 127, 177};
  for (unsigned t = 0; t < sizeof W / sizeof W[0]; t++) {
    const cc_record_id id = start_with_record("c10-win-seq");
    CHECK(view().sched.state == CC_SCHED_ACTIVE && view().sched.chosen_id == id, "window case %u: the record is chosen", t);
    CHECK(life_of(id) == CC_LIFE_SCHEDULED, "window case %u: scheduled", t);
    int offered_at = 0;
    for (int k = 0; k < 4 && !offered_at; k++) {
      /* waves in between are ordinary and never offer */
      CHECK(!at_wave(WAVES[k] - 1, GC_ENC_ORDINARY_WILD, 0), "window case %u: wave %d is no window", t, WAVES[k] - 1);
      if (at_wave(WAVES[k], W[t].kinds[k], W[t].flags[k])) offered_at = WAVES[k];
    }
    CHECK(offered_at == W[t].first_offer, "window case %u: offered at %d (expected %d)", t, offered_at, W[t].first_offer);
    CHECK(cr_offer_species() == SPECIES_ID_RATTATA, "window case %u: the offer names the species", t);
    CHECK(cr_stats_get()->offers == 1, "window case %u: exactly one offer", t);
    /* the skipped windows were skipped for the reason of their encounter */
    const cr_view v = view();
    for (int k = 0; k < 4; k++) {
      if (WAVES[k] >= offered_at) break;
      CHECK(v.sched.windows[k].state == CC_WIN_SKIPPED && v.sched.windows[k].reason != CC_SKIP_NONE, "window case %u: window %d skipped with a reason", t, k);
    }
    CHECK(life_of(id) == CC_LIFE_SCHEDULED, "window case %u: still scheduled while the player decides (%d)", t, life_of(id));
    /* decline: the record is free, nothing is offered again this run */
    gc_custom_enemy spec;
    CHECK(!answer(false, &spec), "window case %u: declined builds nothing", t);
    CHECK(life_of(id) == CC_LIFE_STORED && view().sched.state == CC_SCHED_DECLINED, "window case %u: declined: the record is stored (%d, sched %d)", t,
          life_of(id), view().sched.state);
    for (int k = 0; k < 4; k++) CHECK(!at_wave(WAVES[k], GC_ENC_ORDINARY_WILD, 0), "window case %u: nothing after a decline (wave %d)", t, WAVES[k]);
    CHECK(cr_stats_get()->offers == 1 && cr_stats_get()->declines == 1, "window case %u: counted", t);
    cr_end();
  }
  /* all four skipped: the schedule is exhausted and the record goes back */
  {
    const cc_record_id id = start_with_record("c10-win-exhaust");
    for (int k = 0; k < 4; k++) CHECK(!at_wave(WAVES[k], GC_ENC_TRAINER, 0), "exhaust: no offer at %d", WAVES[k]);
    CHECK(view().sched.state == CC_SCHED_EXHAUSTED, "exhausted (%d)", view().sched.state);
    CHECK(life_of(id) == CC_LIFE_STORED, "exhausted: the record is free (%d)", life_of(id));
    CHECK(cr_stats_get()->offers == 0, "exhausted: no offer");
    cr_end();
  }
  /* accept, then a refused build: the record is free, it is not a custom combat, no second offer */
  {
    const cc_record_id id = start_with_record("c10-win-refuse");
    CHECK(at_wave(27, GC_ENC_ORDINARY_WILD, 0), "refuse: offered at 27");
    gc_custom_enemy spec;
    CHECK(answer(true, &spec), "refuse: accepted: the spec is filled");
    CHECK(spec.species == SPECIES_ID_RATTATA && spec.ability == 62 && spec.n_moves == 2 && spec.moves[0] == MOVE_ID_TACKLE &&
              spec.moves[1] == MOVE_ID_SCRATCH && spec.has_nature && spec.nature == 3,
          "refuse: the spec is the record's identity (species %d, ability %d, %d moves)", spec.species, spec.ability, spec.n_moves);
    CHECK(life_of(id) == CC_LIFE_CHALLENGED, "refuse: challenged (%d)", life_of(id));
    const gc_custom_hooks* h = cr_hooks();
    h->built(h->user, R, false, "cannot build");
    CHECK(cr_stats_get()->refused == 1 && cr_stats_get()->built == 0 && cr_stats_get()->losses == 0 && cr_stats_get()->wins == 0,
          "refuse: counted as refused only (%u %u %u %u)", cr_stats_get()->refused, cr_stats_get()->built, cr_stats_get()->losses, cr_stats_get()->wins);
    CHECK(life_of(id) == CC_LIFE_STORED, "refuse: the record is back (%d)", life_of(id));
    CHECK(!view().rematch_live, "refuse: no rematch is live");
    CHECK(!at_wave(77, GC_ENC_ORDINARY_WILD, 0) && !at_wave(127, GC_ENC_ORDINARY_WILD, 0), "refuse: no second offer");
    CHECK(vouchers() == 0, "refuse: no voucher");
    cr_end();
  }
  /* only the rematch's own enemy winning the fight counts: another Pokemon fainting does not */
  {
    const cc_record_id id = start_with_record("c10-win-faint");
    CHECK(at_wave(27, GC_ENC_ORDINARY_WILD, 0), "faint: offered");
    gc_custom_enemy spec;
    CHECK(answer(true, &spec), "faint: accepted");
    const gc_custom_hooks* h = cr_hooks();
    R->n_enemy = 1;   /* the run's wave-1 enemy stands for the one game-core built */
    h->built(h->user, R, true, "");
    CHECK(view().rematch_enemy && view().rematch_enemy_id == R->enemy[0].b.id, "faint: the rematch enemy is the one built");
    gc_custom_faint f;
    memset(&f, 0, sizeof f);
    f.wave = 27;
    f.side = 1;
    f.fainted = gate_actor(R->enemy[0].b.id + 1000, SPECIES_ID_RATTATA, true);
    h->faint(h->user, R, &f);
    CHECK(!view().enemy_down, "faint: another enemy fainting is not the win");
    f.fainted = gate_actor(R->enemy[0].b.id, SPECIES_ID_RATTATA, true);
    h->faint(h->user, R, &f);
    CHECK(view().enemy_down, "faint: the rematch enemy fainting is");
    CHECK(life_of(id) == CC_LIFE_CHALLENGED, "faint: still challenged until the screen moves on");
    cr_end();
  }
  /* accepted at 77 (the first window skipped): later windows are quiet */
  {
    const cc_record_id id = start_with_record("c10-win-late");
    CHECK(!at_wave(27, GC_ENC_TRAINER, 0), "late: 27 skipped");
    CHECK(at_wave(77, GC_ENC_ORDINARY_WILD, 0), "late: offered at 77");
    gc_custom_enemy spec;
    CHECK(answer(true, &spec) && life_of(id) == CC_LIFE_CHALLENGED, "late: accepted");
    CHECK(!at_wave(127, GC_ENC_ORDINARY_WILD, 0) && !at_wave(177, GC_ENC_ORDINARY_WILD, 0), "late: nothing after an accept");
    cr_end();
  }
  /* the card refuses the write of a window: the run goes on without a rematch, once, and says so */
  {
    const cc_record_id id = start_with_record("c10-win-fault");
    /* an offer changes the schedule only; the roster changes (and the card is written) when the last window is skipped */
    for (int k = 0; k < 3; k++) CHECK(!at_wave(WAVES[k], GC_ENC_TRAINER, 0), "fault: skipped %d", WAVES[k]);
    faults(1, 0, false);
    CHECK(!at_wave(177, GC_ENC_TRAINER, 0), "fault: no offer when the window cannot be written");
    CHECK(view().degraded && cr_stats_get()->degraded == 1, "fault: the rematch is switched off for this run");
    const char* msg = cr_take_message();
    CHECK(msg && strstr(msg, "SD card"), "fault: the player is told (%s)", msg ? msg : "nothing");
    no_faults();
    CHECK(!at_wave(177, GC_ENC_ORDINARY_WILD, 0), "fault: and not offered later either");
    CHECK(cr_stats_get()->degraded == 1 && cr_stats_get()->offers == 0, "fault: told once, no offer");
    /* the run ends: what it held is given back */
    end_run(GC_SCREEN_GAME_OVER, true);
    CHECK(life_of(id) == CC_LIFE_STORED, "fault: after the run the record is free (%d)", life_of(id));
  }
  /* a card that refuses the answer: the same */
  {
    const cc_record_id id = start_with_record("c10-win-fault2");
    CHECK(at_wave(27, GC_ENC_ORDINARY_WILD, 0), "fault2: offered");
    faults(1, 0, false);
    gc_custom_enemy spec;
    CHECK(!answer(true, &spec), "fault2: an answer that cannot be written builds nothing");
    CHECK(view().degraded && !view().rematch_live, "fault2: switched off, nothing live");
    no_faults();
    end_run(GC_SCREEN_GAME_OVER, true);
    CHECK(life_of(id) == CC_LIFE_STORED, "fault2: the record is free (%d)", life_of(id));
  }
  /* a record the game cannot build (an ability the species does not have) is never a candidate; another one is */
  {
    sd_new();
    static const uint16_t mv[2] = {MOVE_ID_TACKLE, MOVE_ID_SCRATCH};
    cc_nemesis bad = make_record(SPECIES_ID_RATTATA, 1, 1, 2, mv);   /* ability 1 is not one of Rattata's */
    const cc_record_id bad_id = put_record(&bad);
    CHECK(new_run("c10-win-bad", true, 0), "start");
    CHECK(view().sched.state == CC_SCHED_NO_CANDIDATE, "an unbuildable record alone: no candidate (%d)", view().sched.state);
    CHECK(!at_wave(27, GC_ENC_ORDINARY_WILD, 0), "an unbuildable record alone: no offer");
    CHECK(life_of(bad_id) == CC_LIFE_STORED, "an unbuildable record stays stored");
    cr_end();
    const cc_record_id good_id = put_default();
    CHECK(new_run("c10-win-bad2", true, 0), "start again");
    CHECK(view().sched.chosen_id == good_id, "the buildable one is chosen (%u, %u)", view().sched.chosen_id, good_id);
    CHECK(at_wave(27, GC_ENC_ORDINARY_WILD, 0), "offered");
    CHECK(life_of(bad_id) == CC_LIFE_STORED, "the unbuildable one is untouched");
    cr_end();
  }
  /* a run that is left (the title) and a new run: the reservation of the old one is given back first */
  {
    const cc_record_id id = start_with_record("c10-win-abandon");
    CHECK(life_of(id) == CC_LIFE_SCHEDULED, "abandon: scheduled");
    cr_end();   /* no run end: the player quit to the title */
    CHECK(life_of(id) == CC_LIFE_SCHEDULED, "abandon: still held (no one gave it back yet)");
    CHECK(new_run("c10-win-abandon2", true, 0), "a new custom run");
    CHECK(life_of(id) == CC_LIFE_SCHEDULED && view().sched.chosen_id == id, "abandon: the new run holds it (%d)", life_of(id));
    end_run(GC_SCREEN_GAME_OVER, true);
    CHECK(life_of(id) == CC_LIFE_STORED, "abandon: given back at the run's end");
  }
  printf("3  windows: done\n");
}

/* ------------------------------------------------------------------ 5/6. save and load, crashes, the reward */

typedef struct {
  bool ok;
  int attempt;
  cc_record_id rid;
  cc_nemesis nem;
  snap base;     /* the card with the Nemesis and the profile, before any custom run */
  char seed[32];
} fixture;
static fixture FX;

static void fx_restore(void) {
  restore(&FX.base);
  reboot_io();
  reboot();
}
static bool fixture_get(void) {
  if (FX.ok) return true;
  for (int attempt = 0; attempt < 16; attempt++) {
    sd_new();
    cc_record_id rid = 0;
    cc_nemesis nem;
    if (!real_nemesis(attempt * 40, 40, &rid, &nem)) continue;
    cr_end();
    snap base = take();
    restore(&base);
    reboot_io();
    reboot();
    rm_cfg cfg = {attempt, true, 1, FIGHT_WIN, 0, 29, rid};
    rm_out o;
    rm_play(&cfg, &o);
    cr_end();
    if (!o.offered) {
      drop(&base);
      continue;
    }
    FX.ok = true;
    FX.attempt = attempt;
    FX.rid = rid;
    FX.nem = nem;
    FX.base = base;
    snprintf(FX.seed, sizeof FX.seed, "c10-rematch-%d", attempt);
    return true;
  }
  CHECK(false, "a seed whose wave 27 offers a rematch");
  return false;
}

/* A custom run on the fixture's seed (a party that cannot lose, saves at every save point). */
static void fx_run(void) {
  CHECK(new_run(FX.seed, true, 100), "the fixture's run");
  bot.heal = true;
  bot.save_every = 1;
  hashing = false;
}
static step_t go_wave(int wave) {
  bot.stop_wave = wave;
  return run_until(60000);
}
static void put_slot_files(const snap* from) {
  for (int i = 0; i < from->n; i++)
    if (strstr(from->f[i].name, CR_FILE_PREFIX)) put_file(from->f[i].name, from->f[i].bytes, from->f[i].len);
}
static void put_named(const snap* from, const char* name) {
  const snap_file* f = find_file(from, name);
  if (f) put_file(name, f->bytes, f->len);
}
static void del_named(const char* name) {
  char path[600];
  snprintf(path, sizeof path, "%s/%s", SAVE_DIR, name);
  remove(path);
}

static void test_saves(void) {
  if (!fixture_get()) return;

  /* an older save after the whole rematch was won and paid: the rematch is not offered again and the voucher is not paid again */
  fx_restore();
  fx_run();
  CHECK(go_wave(26) == STEP_STOPPED, "wave 26");
  snap s26 = take();
  CHECK(go_wave(29) == STEP_STOPPED, "wave 29");
  CHECK(vouchers() == 1 && life_of(FX.rid) == CC_LIFE_REWARD_CLAIMED, "won and paid once (%u, %d)", vouchers(), life_of(FX.rid));
  {
    /* asking again for the same record, however it is done, pays nothing */
    uint32_t txn = 0;
    CHECK(ptx_begin_claim(FX.rid, &txn) != PTX_OK && txn == 0, "a claimed record cannot be claimed again");
    CHECK(cs_claim_reward(FX.rid, NULL) != CS_CLAIM_DONE, "... nor through the store");
    CHECK(cr_settle_owed() == 0 && vouchers() == 1, "... nor by settling what is owed");
  }
  cr_end();
  put_slot_files(&s26);   /* the save of wave 26 comes back (the game was closed without the later saves, say) */
  reboot();
  {
    char why[RSV_WHY_LEN];
    CHECK(resume_run(slot_used, why) == RSV_OK, "an older save of the same run loads: %s", why);
    CHECK(view().forced_done, "the rematch is over for this run");
    cr_stats_reset();
    CHECK(go_wave(29) == STEP_STOPPED, "wave 29 again");
    CHECK(cr_stats_get()->offers == 0 && cr_stats_get()->accepts == 0, "no second offer");
    CHECK(cr_stats_get()->windows == 0 && !view().degraded && cr_stats_get()->degraded == 0, "the over rematch asks the card nothing and tells the player nothing");
    CHECK(cr_take_message() == NULL, "no message");
    CHECK(vouchers() == 1 && life_of(FX.rid) == CC_LIFE_REWARD_CLAIMED, "still exactly one voucher (%u)", vouchers());
    cr_end();
  }
  /* repeated reloads and boots never pay again */
  for (int k = 0; k < 3; k++) {
    reboot();
    CHECK(cr_settle_owed() == 0, "nothing owed after boot %d", k);
    char why[RSV_WHY_LEN];
    CHECK(resume_run(slot_used, why) == RSV_OK, "reload %d: %s", k, why);
    CHECK(vouchers() == 1, "reload %d: one voucher (%u)", k, vouchers());
    cr_end();
  }
  drop(&s26);

  /* an older save after a DECLINE: the offer is asked again (declining is not durable across a load, and frees the record) */
  fx_restore();
  fx_run();
  CHECK(go_wave(26) == STEP_STOPPED, "wave 26");
  s26 = take();
  bot.offer_answer = 0;
  CHECK(go_wave(28) == STEP_STOPPED, "declined, wave 28");
  CHECK(life_of(FX.rid) == CC_LIFE_STORED, "declined: stored");
  cr_end();
  put_slot_files(&s26);
  reboot();
  {
    char why[RSV_WHY_LEN];
    CHECK(resume_run(slot_used, why) == RSV_OK, "the older save loads: %s", why);
    CHECK(life_of(FX.rid) == CC_LIFE_SCHEDULED, "and holds the record again (%d)", life_of(FX.rid));
    cr_stats_reset();
    bot.offer_answer = 0;
    bot.save_every = 1;
    CHECK(go_wave(28) == STEP_STOPPED, "wave 28");
    CHECK(cr_stats_get()->offers == 1 && vouchers() == 0, "asked once more, no voucher");
    cr_end();
  }
  drop(&s26);

  /* an older save after the offer was ACCEPTED (the roster says challenged, the save says the offer is still to come): the record is
   * scheduled again for this run and the offer comes back, once */
  fx_restore();
  fx_run();
  CHECK(go_wave(26) == STEP_STOPPED, "wave 26");
  s26 = take();
  CHECK(go_wave(27) == STEP_STOPPED && life_of(FX.rid) == CC_LIFE_CHALLENGED, "accepted at wave 27: challenged");
  cr_end();
  put_slot_files(&s26);
  reboot();
  {
    char why[RSV_WHY_LEN];
    CHECK(resume_run(slot_used, why) == RSV_OK, "the older save loads over a challenged record: %s", why);
    CHECK(life_of(FX.rid) == CC_LIFE_SCHEDULED, "the record is scheduled again (%d)", life_of(FX.rid));
    cr_stats_reset();
    bot.offer_answer = 1;
    bot.save_every = 1;
    CHECK(go_wave(27) == STEP_STOPPED, "wave 27 again");
    CHECK(cr_stats_get()->offers == 1 && cr_stats_get()->accepts == 1 && view().rematch_live && life_of(FX.rid) == CC_LIFE_CHALLENGED,
          "the offer comes back once and the rematch is live again (%u offers)", cr_stats_get()->offers);
    cr_end();
  }
  drop(&s26);

  /* another run used the record up (won it) after this save: the older run is refused, with a reason, and nothing on the card changes */
  fx_restore();
  fx_run();
  CHECK(go_wave(26) == STEP_STOPPED, "wave 26");
  cr_end();   /* back to the title with the slot saved */
  CHECK(new_run(FX.seed, true, 100), "another custom run takes the record");
  bot.heal = true;
  bot.save_every = 0;
  CHECK(go_wave(29) == STEP_STOPPED && life_of(FX.rid) == CC_LIFE_REWARD_CLAIMED && vouchers() == 1, "and wins it");
  cr_end();
  {
    const snap before = take();
    cr_stats_reset();
    char why[RSV_WHY_LEN] = "";
    CHECK(resume_run(slot_used, why) == RSV_INCOMPATIBLE, "the first run does not resume");
    CHECK(why[0] != 0, "and says why: %s", why);
    CHECK(cr_stats_get()->resume_refused == 1, "counted");
    const snap after = take();
    bool same = before.n == after.n;
    for (int i = 0; same && i < before.n; i++) same = same_file(&before, &after, before.f[i].name);
    CHECK(same, "no file changed");
    drop(&before);
    drop(&after);
    rsv_info info;
    CHECK(cr_inspect(slot_used, &info) == RSV_OK, "the slot itself is still a good save");
    CHECK(vouchers() == 1, "and nothing was paid");
  }
  /* the other direction: a run that took over a record another run held (the title's reservation is only ever for the run played now) */
  fx_restore();
  fx_run();
  CHECK(go_wave(26) == STEP_STOPPED, "wave 26");
  cr_end();
  CHECK(new_run("c10-other-run", true, 0), "another custom run takes the record");
  CHECK(life_of(FX.rid) == CC_LIFE_SCHEDULED, "the other run holds it");
  cr_end();
  {
    char why[RSV_WHY_LEN] = "";
    CHECK(resume_run(slot_used, why) == RSV_OK, "the first run resumes and takes it back: %s", why);
    CHECK(life_of(FX.rid) == CC_LIFE_SCHEDULED && view().sched.chosen_id == FX.rid, "and holds it");
    cr_end();
  }

  /* a damaged slot is refused, and the roster is not touched */
  fx_restore();
  fx_run();
  CHECK(go_wave(26) == STEP_STOPPED, "wave 26");
  cr_end();
  {
    snap good = take();
    const char* names[8];
    int nn = 0;
    for (int i = 0; i < good.n && nn < 8; i++)
      if (strstr(good.f[i].name, CR_FILE_PREFIX)) names[nn++] = good.f[i].name;
    CHECK(nn >= 1, "the slot has files (%d)", nn);
    for (int pos = 0; pos < 3; pos++) {
      restore(&good);
      reboot_io();
      reboot();
      for (int i = 0; i < nn; i++) {
        const snap_file* f = find_file(&good, names[i]);
        uint8_t* bad = malloc(f->len);
        memcpy(bad, f->bytes, f->len);
        const size_t at = pos == 0 ? 20 : pos == 1 ? f->len / 2 : f->len - 3;   /* the header side, the middle, the end */
        bad[at] ^= 0x55;
        put_file(names[i], bad, f->len);
        free(bad);
      }
      const snap before = take();
      char why[RSV_WHY_LEN] = "";
      rsv_info info;
      CHECK(cr_inspect(slot_used, &info) != RSV_OK, "damaged slot (%d): the list does not offer it", pos);
      CHECK(resume_run(slot_used, why) != RSV_OK, "damaged slot (%d): not resumed", pos);
      const snap after = take();
      bool same = before.n == after.n;
      for (int i = 0; same && i < before.n; i++)
        if (strncmp(before.f[i].name, CR_FILE_PREFIX, strlen(CR_FILE_PREFIX))) same = same_file(&before, &after, before.f[i].name);
      CHECK(same, "damaged slot (%d): the roster and profile files are untouched", pos);
      drop(&before);
      drop(&after);
    }
    drop(&good);
  }

  /* custom bytes are never an ordinary session, and an ordinary session is not a custom one */
  fx_restore();
  fx_run();
  CHECK(go_wave(26) == STEP_STOPPED, "wave 26");
  cr_end();
  {
    char name[32];
    char base_name[16];
    rsv_slot_name(slot_used, base_name);
    snprintf(name, sizeof name, "%s.sav", base_name);
    const snap now = take();
    const snap_file* f = NULL;
    for (int i = 0; i < now.n; i++)
      if (!strcmp(now.f[i].name, CR_FILE_PREFIX "0.sav")) f = &now.f[i];
    CHECK(f != NULL, "the custom slot file exists");
    if (f) {
      put_file(name, f->bytes, f->len);   /* the custom bytes under the ordinary name */
      char bak[40];
      snprintf(bak, sizeof bak, "%s.bak", name);
      del_named(bak);
      rsv_info info;
      CHECK(rsv_inspect(slot_used, &info) != RSV_OK, "the ordinary loader does not read custom bytes");
      gc_run* other = R == &RUN ? &RUN2 : &RUN;
      char why[RSV_WHY_LEN];
      CHECK(rsv_load(slot_used, other, why) != RSV_OK, "... and does not load them (%s)", why);
      del_named(name);
    }
    drop(&now);
    /* an ordinary run's session in the custom slot */
    CHECK(new_run("c10-ordinary", false, 100), "an ordinary run");
    bot.heal = true;
    for (int i = 0; i < 40; i++) {
      if (gc_run_screen(R)->kind == GC_SCREEN_COMMAND) break;
      if (step() != STEP_OK) break;
    }
    CHECK(rsv_save(slot_used, R) == RSV_SAVED, "the ordinary save");
    const snap ord = take();
    const snap_file* of = find_file(&ord, name);
    CHECK(of != NULL, "the ordinary file exists (%s)", name);
    if (of) {
      char cname[24];
      snprintf(cname, sizeof cname, CR_FILE_PREFIX "%d.sav", slot_used);
      cr_clear(slot_used);
      put_file(cname, of->bytes, of->len);
      rsv_info info;
      CHECK(cr_inspect(slot_used, &info) != RSV_OK, "the custom list does not offer an ordinary session");
      char why[RSV_WHY_LEN];
      gc_run* other = R == &RUN ? &RUN2 : &RUN;
      CHECK(cr_load(slot_used, other, why) != RSV_OK, "... and does not load it (%s)", why);
    }
    drop(&ord);
    cr_end();
  }

  /* the roster is older than the save (a copy of the card from before the accept): nothing is live, nothing is paid */
  fx_restore();
  fx_run();
  CHECK(go_wave(26) == STEP_STOPPED, "wave 26");
  const snap early = take();
  const char* cust = "custom.sav";
  CHECK(go_wave(27) == STEP_STOPPED, "wave 27: the rematch's first turn, saved");
  CHECK(view().rematch_live && life_of(FX.rid) == CC_LIFE_CHALLENGED, "challenged");
  cr_end();
  put_named(&early, cust);   /* the roster as it was at wave 26 */
  reboot();
  {
    char why[RSV_WHY_LEN];
    CHECK(resume_run(slot_used, why) == RSV_OK, "the save loads over the older roster: %s", why);
    CHECK(!view().rematch_live, "the rematch is not live (the roster does not say it was accepted)");
    CHECK(go_wave(29) == STEP_STOPPED, "the run goes on");
    CHECK(vouchers() == 0, "and pays nothing (%u)", vouchers());
    end_run(GC_SCREEN_GAME_OVER, true);
    CHECK(life_of(FX.rid) == CC_LIFE_STORED, "the record is free at the run's end (%d)", life_of(FX.rid));
  }
  drop(&early);

  /* the roster still says the rematch is being fought, the save is from after it (the result write of the roster was lost): the fight is
   * lost, not live again, and nothing is paid twice */
  fx_restore();
  fx_run();
  CHECK(go_wave(26) == STEP_STOPPED, "wave 26");
  CHECK(go_wave(27) == STEP_STOPPED && view().rematch_live && life_of(FX.rid) == CC_LIFE_CHALLENGED, "the rematch's first turn, saved");
  const snap fighting = take();
  CHECK(go_wave(29) == STEP_STOPPED && life_of(FX.rid) == CC_LIFE_REWARD_CLAIMED && vouchers() == 1, "won and paid");
  cr_end();
  put_named(&fighting, cust);   /* the roster as it was while the fight was on */
  reboot();
  {
    char why[RSV_WHY_LEN];
    CHECK(life_of(FX.rid) == CC_LIFE_CHALLENGED, "the roster says challenged");
    CHECK(resume_run(slot_used, why) == RSV_OK, "the later save loads over it: %s", why);
    CHECK(!view().rematch_live, "the fight is not live: its enemy is gone");
    CHECK(life_of(FX.rid) == CC_LIFE_STORED, "the record is stored again, not left challenged (%d)", life_of(FX.rid));
    CHECK(vouchers() == 1 && cr_settle_owed() == 0, "nothing more is paid (%u)", vouchers());
    cr_end();
  }
  drop(&fighting);

  /* the roster still holds the record for a run whose schedule ended in a decline (the roster write after the save was lost) */
  fx_restore();
  fx_run();
  CHECK(go_wave(26) == STEP_STOPPED, "wave 26");
  const snap held = take();
  bot.offer_answer = 0;
  CHECK(go_wave(28) == STEP_STOPPED && life_of(FX.rid) == CC_LIFE_STORED, "declined, saved at wave 28");
  cr_end();
  put_named(&held, cust);   /* the roster from before the decline: scheduled for this run */
  reboot();
  {
    char why[RSV_WHY_LEN];
    CHECK(life_of(FX.rid) == CC_LIFE_SCHEDULED, "the roster still holds the record");
    CHECK(resume_run(slot_used, why) == RSV_OK, "the save loads: %s", why);
    CHECK(life_of(FX.rid) == CC_LIFE_STORED, "the finished schedule gave the record back (%d)", life_of(FX.rid));
    cr_end();
  }
  drop(&held);

  /* a resumed run keeps the turns the observation had retained */
  fx_restore();
  fx_run();
  CHECK(go_wave(26) == STEP_STOPPED, "wave 26");
  {
    static co_carry before, after;
    CHECK(co_carry_get(&before) && before.hist.n_turns > 0, "turns were retained before the save (%u)", before.hist.n_turns);
    cr_end();
    reboot();
    char why[RSV_WHY_LEN];
    CHECK(resume_run(slot_used, why) == RSV_OK, "the save loads: %s", why);
    CHECK(co_carry_get(&after), "the resumed run is observed");
    CHECK(after.hist.n_turns >= before.hist.n_turns && after.battle_seq >= before.battle_seq && after.next_seq >= before.next_seq,
          "and has the turns it had (%u, %u of %u turns)", after.hist.n_turns, after.battle_seq, before.hist.n_turns);
    cr_end();
  }

  /* a custom file that cannot be used: the custom run does not resume, says why, and no file changes */
  fx_restore();
  fx_run();
  CHECK(go_wave(26) == STEP_STOPPED, "wave 26");
  cr_end();
  {
    static const char junk[] = "this is not a custom content file, and it is long enough to be taken for one by its size alone....................";
    put_file(cust, junk, sizeof junk);
    del_named("custom.sav.bak");   /* no good backup to fall back to */
    reboot();
    const snap before = take();
    char why[RSV_WHY_LEN] = "";
    CHECK(resume_run(slot_used, why) == RSV_INCOMPATIBLE, "a custom run does not resume with an unusable custom file");
    CHECK(strstr(why, "cannot be used") != NULL, "and says that: %s", why);
    const snap after = take();
    bool same = before.n == after.n;
    for (int i = 0; same && i < before.n; i++) same = same_file(&before, &after, before.f[i].name);
    CHECK(same, "no file changed");
    drop(&before);
    drop(&after);
  }
  printf("5  saves: done\n");
}

/* ------------------------------------------------------------------ 2b. what game-core reports for each real wave */

/* game-core classifies every wave it generates for the controller. Here the classification is held against what the run itself says about
 * that wave (its battle type, its enemy, its stand-in and double-roll counters), over many real waves. */
static void test_report(int nseeds) {
  int n_wild = 0, n_trainer = 0, n_boss = 0, n_final = 0, n_double = 0, n_fused = 0, n_tera = 0, n_stand = 0, n_mystery = 0, n_waves = 0;
  for (int sd = 0; sd < nseeds; sd++) {
    sd_new();
    char seed[32];
    snprintf(seed, sizeof seed, "c10-report-%d", sd);
    CHECK(new_run(seed, true, 100), "start");
    bot.heal = true;
    bot.save_every = 0;
    uint16_t last_wave = 0;
    uint32_t dbl = 0, unp = 0;   /* wave 1 was generated by new_run */
    for (int i = 0; i < 30000; i++) {
      const step_t st = step();
      if (st != STEP_OK) break;
      const cr_view v = view();
      if (v.enc_wave != last_wave) {
        last_wave = v.enc_wave;
        n_waves++;
        const bool d_dbl = R->stubs[GC_STUB_DOUBLE_AS_SINGLE] > dbl, d_unp = R->stubs[GC_STUB_UNPORTED_ENEMY] > unp;
        dbl = R->stubs[GC_STUB_DOUBLE_AS_SINGLE];
        unp = R->stubs[GC_STUB_UNPORTED_ENEMY];
        bool boss = false, fused = false, tera = false;
        for (int k = 0; k < R->n_enemy; k++) {
          boss |= R->enemy[k].b.is_boss;
          fused |= R->enemy[k].b.fusion_species != 0;
          tera |= R->enemy[k].b.terastallized;
        }
        const int final_wave = pr_run_is_wave_final(R->run.mode, v.enc_wave);
        if (final_wave) {
          CHECK(v.enc_kind == GC_ENC_FINAL, "wave %u is the final wave of the mode (kind %d)", v.enc_wave, v.enc_kind);
          n_final++;
        } else if (boss) {
          CHECK(v.enc_kind == GC_ENC_BOSS, "wave %u has a boss (kind %d)", v.enc_wave, v.enc_kind);
          n_boss++;
        } else if (R->battle_type == BATTLE_TYPE_TRAINER) {
          CHECK(v.enc_kind == GC_ENC_TRAINER, "wave %u is a trainer's (kind %d)", v.enc_wave, v.enc_kind);
          n_trainer++;
        } else if (v.enc_kind == GC_ENC_MYSTERY) {
          n_mystery++;   /* a mystery encounter: the screen of the encounter, no wild enemy (never offered a rematch) */
        } else if (d_unp) {
          CHECK(v.enc_kind == GC_ENC_FIXED_OR_SCRIPTED, "wave %u was a stand-in for a battle run-core cannot make (kind %d)", v.enc_wave, v.enc_kind);
          n_stand++;
        } else {
          CHECK(v.enc_kind == GC_ENC_ORDINARY_WILD, "wave %u is an ordinary wild encounter (kind %d)", v.enc_wave, v.enc_kind);
          n_wild++;
          CHECK(!!(v.enc_flags & GC_ENF_DOUBLES) == d_dbl, "wave %u: the double roll is reported iff game-core played it as a single (%d, %d)", v.enc_wave,
                !!(v.enc_flags & GC_ENF_DOUBLES), d_dbl);
          CHECK(!!(v.enc_flags & GC_ENF_FUSED) == fused, "wave %u: a fusion is reported iff the enemy is one", v.enc_wave);
          CHECK(!!(v.enc_flags & GC_ENF_TERA) == tera, "wave %u: Tera is reported iff the enemy is terastallized", v.enc_wave);
          n_double += d_dbl;
          n_fused += fused;
          n_tera += tera;
        }
      }
      if (R->wave >= (sd % 12 == 0 ? 220 : 60)) break;
    }
    cr_end();
  }
  CHECK(n_wild >= 100 && n_trainer >= 10 && n_boss >= 3 && n_double >= 3, "waves seen: %d wild, %d trainer, %d boss, %d double rolls", n_wild, n_trainer, n_boss, n_double);
  printf("2b real waves classified: %d (%d ordinary wild, %d double rolls, %d fused, %d Tera, %d trainer, %d boss, %d final, %d stand-ins, %d mystery)\n", n_waves, n_wild,
         n_double, n_fused, n_tera, n_trainer, n_boss, n_final, n_stand, n_mystery);
}

/* ------------------------------------------------------------------ 4b. exact initialization fixtures */

/* Records with every identity field set, played to the rematch's first turn: the enemy has exactly that identity. */
static void test_identity(void) {
  if (!fixture_get()) return;
  static struct {
    const char* name;
    uint16_t species;
    uint8_t form;
    int8_t gender;
    uint8_t shiny, nature;
    uint16_t ability;
    int n_moves;
    uint16_t moves[4];
  } T[] = {
      {"shiny female Rattata", SPECIES_ID_RATTATA, 0, 1, 1, 5, ABILITY_ID_HUSTLE, 2, {MOVE_ID_TACKLE, MOVE_ID_QUICK_ATTACK}},
      {"male Pikachu, four moves", SPECIES_ID_PIKACHU, 0, 0, 0, 7, ABILITY_ID_STATIC, 4, {MOVE_ID_THUNDERBOLT, MOVE_ID_THUNDER_WAVE, MOVE_ID_QUICK_ATTACK, MOVE_ID_TACKLE}},
      {"shiny genderless Magnemite", SPECIES_ID_MAGNEMITE, 0, -1, 1, 11, ABILITY_ID_MAGNET_PULL, 1, {MOVE_ID_TACKLE}},
      {"hidden ability Pikachu", SPECIES_ID_PIKACHU, 0, 1, 0, 0, ABILITY_ID_LIGHTNING_ROD, 2, {MOVE_ID_THUNDERBOLT, MOVE_ID_TACKLE}},
      {"another form (chosen from the data)", 0, 1, 0, 0, 20, 0, 2, {MOVE_ID_TACKLE, MOVE_ID_QUICK_ATTACK}},
  };
  /* the last fixture is a species with a second form the game can build: the first one in the data (an alternate form of an early species) */
  for (int id = 1; id <= 151 && !T[4].species; id++) {
    const pr_species* sp = gc_species(id);
    if (!sp || sp->nforms < 2) continue;
    const pr_form* f = &pr_forms[sp->form0 + 1];
    if (f->unobtainable || !f->ability1) continue;
    T[4].species = (uint16_t)id;
    T[4].ability = (uint16_t)f->ability1;
  }
  CHECK(T[4].species != 0, "a species with a second form in the data");
  int n_ok = 0;
  for (unsigned t = 0; t < sizeof T / sizeof T[0]; t++) {
    if (!T[t].species) continue;
    sd_new();
    cc_nemesis n = make_record(T[t].species, T[t].gender, T[t].ability, T[t].n_moves, T[t].moves);
    n.form = T[t].form;
    n.shiny = T[t].shiny;
    n.nature = T[t].nature;
    const cc_record_id id = put_record(&n);
    CHECK(new_run(FX.seed, true, 100), "%s: start", T[t].name);
    bot.heal = true;
    bot.stop_wave = 27;
    const step_t st = run_until(60000);
    CHECK(st == STEP_STOPPED, "%s: wave 27 (%d)", T[t].name, (int)st);
    const cr_view v = view();
    if (!v.rematch_enemy) {
      CHECK(false, "%s: the rematch enemy was not built (offers %u, refused %u)", T[t].name, cr_stats_get()->offers, cr_stats_get()->refused);
      cr_end();
      continue;
    }
    n_ok++;
    const pr_pokemon* e = &R->enemy[0].b;
    CHECK(R->n_enemy == 1, "%s: one enemy", T[t].name);
    CHECK(e->species == T[t].species && e->form == T[t].form, "%s: species/form (%d/%d)", T[t].name, e->species, e->form);
    CHECK(e->gender == T[t].gender, "%s: gender (%d, wanted %d)", T[t].name, e->gender, T[t].gender);
    CHECK((e->shiny ? 1 : 0) == T[t].shiny, "%s: shiny (%d)", T[t].name, e->shiny);
    CHECK(e->nature == T[t].nature, "%s: nature (%d, wanted %d)", T[t].name, e->nature, T[t].nature);
    CHECK(e->base_ability == T[t].ability, "%s: ability (%d, wanted %d)", T[t].name, e->base_ability, T[t].ability);
    CHECK(e->n_moves == T[t].n_moves, "%s: %d moves (wanted %d)", T[t].name, e->n_moves, T[t].n_moves);
    for (int i = 0; i < T[t].n_moves && i < e->n_moves; i++) CHECK(e->moves[i] == T[t].moves[i], "%s: move %d", T[t].name, i);
    /* no scaling, no boost, no boss: the ordinary generated level and a plain battle state */
    CHECK(!e->is_boss && e->boss_segments == 0, "%s: no boss segments", T[t].name);
    int boosted = 0;
    for (int k = 0; k < 7; k++) boosted += e->stat_stages[k] != 0;
    CHECK(boosted == 0 && e->status == 0, "%s: no stat stage and no status at the start", T[t].name);
    CHECK(e->hp == e->stats[0] && e->hp > 0, "%s: full hp from the ordinary stat calculation (%d/%d)", T[t].name, e->hp, e->stats[0]);
    CHECK(life_of(id) == CC_LIFE_CHALLENGED, "%s: challenged", T[t].name);
    cr_end();
  }
  CHECK(n_ok == (int)(sizeof T / sizeof T[0]), "identity fixtures built: %d of %zu", n_ok, sizeof T / sizeof T[0]);
  printf("4b identity fixtures: %d of %zu built and checked\n", n_ok, sizeof T / sizeof T[0]);
}

/* The reward under a crash at every write: after the power comes back and the run is played on, exactly one voucher, never two. */
static void test_reward_crash(void) {
  if (!fixture_get()) return;
  fx_restore();
  fx_run();
  CHECK(go_wave(27) == STEP_STOPPED, "the rematch's first turn");
  CHECK(view().rematch_live, "live");
  const snap pre = take();   /* the card at the first turn: the slot just saved, the roster says challenged */
  cr_end();
  int survived = 0, max_writes = 0;
  for (int n = 1; n <= 40; n++) {
    restore(&pre);
    reboot_io();
    reboot();
    char why[RSV_WHY_LEN];
    CHECK(resume_run(slot_used, why) == RSV_OK, "n=%d: resume the first turn: %s", n, why);
    CHECK(view().rematch_live, "n=%d: live after the load", n);
    bot.heal = true;
    bot.save_every = 0;   /* the only save is the one at the first turn */
    crash_at(n);
    const step_t st = go_wave(28);   /* win, and the reward is tried through the failing card */
    const int writes = g_writes;
    if (writes > max_writes) max_writes = writes;
    cr_end();
    no_faults();
    CHECK(st == STEP_STOPPED, "n=%d: the run reached wave 28 in memory (%d)", n, (int)st);
    /* the power is back: boot recovers whatever the failed writes left */
    reboot();
    const int life = life_of(FX.rid);
    const unsigned v = vouchers();
    CHECK(life != CC_LIFE_DEFEATED, "n=%d: no win left unpaid after boot (life %d)", n, life);
    CHECK(v == (life == CC_LIFE_REWARD_CLAIMED ? 1u : 0u), "n=%d: voucher and record agree after boot (life %d, vouchers %u)", n, life, v);
    CHECK(v <= 1, "n=%d: never more than one", n);
    /* the player loads the slot again (the only save) and plays the rematch on to the end */
    CHECK(resume_run(slot_used, why) == RSV_OK, "n=%d: load the saved first turn after the crash: %s", n, why);
    bot.heal = true;
    bot.save_every = 0;
    CHECK(go_wave(29) == STEP_STOPPED, "n=%d: wave 29", n);
    CHECK(vouchers() == 1, "n=%d: exactly one voucher in the end (%u, life %d before)", n, vouchers(), life);
    CHECK(life_of(FX.rid) == CC_LIFE_REWARD_CLAIMED, "n=%d: rewarded (%d)", n, life_of(FX.rid));
    cr_end();
    reboot();
    CHECK(vouchers() == 1 && cr_settle_owed() == 0, "n=%d: and a reboot pays nothing more", n);
    if (writes < n) break;   /* the crash point is past the last write of this flow: all earlier points were covered */
    survived = n;
  }
  CHECK(max_writes >= 4, "the flow wrote at least a few times (%d), so the crash points are real", max_writes);
  drop(&pre);
  printf("6  reward under a crash at each of %d writes: done\n", survived);
}

int main(int argc, char** argv) {
  int nseeds = NSEEDS_DEFAULT + (argc > 1 ? atoi(argv[1]) : 0);
  char dir[64] = "/tmp/custom-run-test-XXXXXX";
  if (!mkdtemp(dir) || chdir(dir) != 0 || mkdir("sdmc:", 0777) != 0 || !save_init()) {
    printf("FAILED: no folder for the saves\n");
    return 1;
  }
  test_defeat_creates(nseeds);
  CHECK(n_created >= 6, "real defeats made %d Nemeses (>= 6 expected over %d seeds)", n_created, nseeds);
  printf("1  real defeats: %d made a Nemesis, %d refused by the store, %d had no reliable finisher\n", n_created, n_refused_store, n_unattributed);
  test_ordinary_creates_none();
  test_gate();
  test_report(nseeds);
  test_windows();
  test_rematch_real();
  test_saves();
  test_identity();
  test_reward_crash();
  printf("custom_run_test: %d checks, %d failed\n", checks, failed);
  if (chdir("/") == 0) {
    char cmd[128];
    snprintf(cmd, sizeof cmd, "rm -rf %s", dir);
    if (system(cmd) != 0) printf("(could not remove %s)\n", dir);
  }
  return failed ? 1 : 0;
}
