/* runtest: the app's run driver (run_screen.c) over the real game-core run, on the host.
 *
 *   build/runtest [runs] [first]      N seeded runs (default 20), starting at run number `first`
 *
 * The battle screen and the menus are replaced by a fake that keeps a model of what the real screen would show
 * (who is out, their HP and level) and checks every screen event against it; the fake player answers each
 * screen at random. A run ends at the game over (or the clear), the driver starts the next one.
 *
 * It fails (exit 1) when the driver or the converter breaks the model (a summon over a Pokemon already out, a
 * hit on nobody, an HP over the maximum...), when the screen shown at a COMMAND screen is not the battle game-core
 * has, when the driver gets stuck or can't find a switch-in Pokemon, or when a run makes no progress.
 *
 *   RUNTEST_SAVE=1       the runs save into slot 0 (in a temporary folder) at their save points. Fails when a first command
 *                        is asked before the run was written, when a write fails, or when what the slot holds is not the run.
 *   RUNTEST_SAVE=resume  the same, and every 4th save point the run is dropped and continued from its slot (rs_resume, as
 *                        the title's Continue does): the loaded run must be the one that was saved, and the fake screen must
 *                        then agree with it at the next command. */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "battle_view.h"
#include "ball_menu.h"
#include "i18n.h"
#include "party_fake.h"
#include "party_screen.h"
#include "pr_data.h"
#include "pr_enums.h"
#include "reward_host.h"
#include "run_saves.h"
#include "run_screen.h"
#include "save.h"
#include "ui.h"
#include "ui_widgets.h"

/* ------------------------------------------------------------------ the fake world */

static uint32_t rs32 = 1;
static uint32_t rnd(uint32_t n) {
  rs32 ^= rs32 << 13;
  rs32 ^= rs32 >> 17;
  rs32 ^= rs32 << 5;
  return n ? rs32 % n : 0;
}

static struct {
  bv_mon mon[2];
  bool present[2];
  bool fainted_shown[2];  /* fainted and not replaced yet: its data is still on the screen (an ability bar can still name it) */
  bool begun;
  /* pending continuations (each is one thing the real UI would be waiting on) */
  void (*play_done)(void*);
  void* play_arg;
  bool playing;
  void (*on_command)(bv_command, void*);
  void* command_arg;
  bv_command_opts opts;
  bool asking;
  void (*opt_done)(int, void*);
  void* opt_arg;
  int opt_n;
  char opt_label[40][72];
  int refused_streak;
  bool opt_cancel;
  void (*confirm_done)(int, void*);
  void* confirm_arg;
  void (*party_pick)(int, int, void*);
  void* party_arg;
  bool rel_open;          /* the full-party release list (menus' party.c, through party_fake.c) is open */
  const pr_battle* party_bt;
  ps_mode party_mode;
  void (*ball_pick)(int, void*);
  void* ball_arg;
  const int* ball_counts;
  int ball_counts_copy[BALL_TYPES];
  /* saving (RUNTEST_SAVE) */
  int expected_saves;     /* save points whose first command was asked, not counting a run that was just loaded */
  int save_points;        /* the same, to pick the ones to resume from */
  bool resume_now;        /* drop the run and continue it from its slot (the main loop does it) */
  bool resumed;           /* the run was just loaded: its save point is the file it came from */
  bool sp_open;           /* this battle's first command was already asked (it is asked again after a refusal) */
} W;

/* the reward / shop screen, the biome map and the party screen in item mode (the app's own, on the stack, are not built here) */
static struct {
  bool shop, biome;
  shop_ui_args shop_args;
  gc_screen screen;
  shop_ui_events shop_ev;
  biome_map_events biome_ev;
  int n_biomes;
  void (*item_pick)(int slot, int move_slot, void* arg);
  void* item_arg;
  bool item_move;
} X;
static int n_shops, n_maps, n_item_opens;
static bool save_on, resume_on;
static int n_resumes;

static int violations, n_scenes, n_ask, n_dialogs, n_options, n_party_opens, n_balls, n_party_full_swaps;
static int n_throw_ev, n_caught_ev, n_broke_ev, n_bv_farewell, n_real_releases, n_list_backs, n_rel_checked, n_decl_checked;

/* A full-party catch: the party as the question "release one?" found it, and the Pokemon about to be caught */
static struct {
  bool armed;
  int n, releases0, kept0;
  uint32_t id[6];
  int species[6];
  uint32_t caught_id;
  int caught_species;
  int pick;               /* the place in the party the player picked in the release list (-1: none yet) */
} REL;
static char first_violation[300];

static void bad(const char* fmt, ...);
#define BAD(...) bad(__VA_ARGS__)

static int verbose_bad;
static char trail[120][80];
static int trail_n;
static void trail_add(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(trail[trail_n++ % 120], 80, fmt, ap);
  va_end(ap);
}
static void bad(const char* fmt, ...) {
  char msg[260];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(msg, sizeof msg, fmt, ap);
  va_end(ap);
  char full[300];
  snprintf(full, sizeof full, "run %d wave %d: %s", rs_stats_get()->runs, rs_run()->wave, msg);
  if (!violations++) snprintf(first_violation, sizeof first_violation, "%s", full);
  if (verbose_bad) {
    printf("VIOLATION %s\n", full);
    for (int i = trail_n > 120 ? trail_n - 120 : 0; i < trail_n; i++) printf("    %s\n", trail[i % 120]);
    const gc_run* r = rs_run();
    printf("  battle-core events of this drain (%d):\n", r->n_battle_events);
    for (int i = 0; i < r->n_battle_events; i++) {
      const pr_event* e = &r->battle_events[i];
      printf("    ev type=%d b=%d t=%d id=%d v=%d%s%s", e->type, e->battler, e->target, e->id, e->value, e->msg.key ? " key=" : "", e->msg.key ? e->msg.key : "");
      if (e->battler >= 0 && e->battler < 4) printf(" snap sp=%d slot=%d hp=%d/%d", e->field[e->battler].species, e->field[e->battler].party_slot, e->field[e->battler].hp, e->field[e->battler].max_hp);
      printf("\n");
    }
  }
}

/* ---- stubs for the app's other modules ---- */
const char* i18n_language(void) { return "en"; }
const char* i18n_species_name(int id) {
  static char name[8][32];
  static int k;
  k = (k + 1) & 7;
  snprintf(name[k], sizeof name[k], "Species_%d", id);
  return name[k];
}
const char* i18n_move_name(int id) {
  static char name[8][32];
  static int k;
  k = (k + 1) & 7;
  snprintf(name[k], sizeof name[k], "Move_%d", id);
  return name[k];
}
const char* i18n_t(char* out, int n, const char* key, ...) {
  snprintf(out, (size_t)n, "%s", key);
  return out;
}
void* pak_read(const char* key, size_t* len) {
  (void)key;
  if (len) *len = 0;
  return NULL;
}
void ba_preload_move(int move) { (void)move; }
void ba_drop_moves(void) {}
void img_drop_prefix(const char* prefix) { (void)prefix; }
int ui_depth(void) { return W.begun ? 1 : 0; }
bool ui_busy(void) { return false; }
/* one postponed step at most (run_screen.c keeps one): it runs when the fake player next steps */
static struct {
  void (*fn)(void*);
  void* arg;
  int id;
} T;
static int timer_ids;
int ui_after(float ms, void (*fn)(void* arg), void* arg) {
  (void)ms;
  T.fn = fn;
  T.arg = arg;
  return T.id = ++timer_ids;
}
void ui_timer_cancel(int id) {
  if (id && id == T.id) T.fn = NULL;
}
void ui_fade(float out_ms, float hold_ms, float in_ms, void (*at_black)(void* arg), void* arg) {
  (void)out_ms, (void)hold_ms, (void)in_ms;
  if (at_black) at_black(arg);
}

/* ---- the battle screen ---- */
static bool side_ok(int battler, const char* what) {
  const int s = battler >= 2 ? 1 : 0;
  if (!W.present[s]) {
    BAD("%s on side %d with nobody out", what, s);
    return false;
  }
  return true;
}

void bv_begin(const bv_setup* setup) {
  trail_add("--- bv_begin biome %d trainer %d present %d", setup->biome, setup->trainer, setup->player_present);
  W.begun = true;
  n_scenes++;
  W.mon[0] = setup->player;
  W.mon[1] = setup->enemy;
  W.present[0] = setup->player_present;
  W.present[1] = false;
  W.fainted_shown[0] = W.fainted_shown[1] = false;
  W.playing = W.asking = false;
  W.sp_open = false;
}

bv_mon* bv_mon_of(int battler) { return &W.mon[battler >= 2 ? 1 : 0]; }

void bv_play(const bv_event* ev, int n, void (*on_done)(void*), void* arg) {
  if (W.playing) BAD("bv_play while the last one is still playing");
  for (int i = 0; i < n; i++) {
    const bv_event* e = &ev[i];
    const int s = e->battler >= 2 ? 1 : 0;
    if (!(e->type == BEV_MESSAGE && e->text && !strncmp(e->text, "modifierSelectUiHandler", 23)))
      trail_add("bev %d b=%d a=%d b=%d%s%s", e->type, e->battler, e->a, e->b, e->text ? " t=" : "", e->text ? e->text : "");
    switch (e->type) {
      case BEV_MESSAGE:
        if (!e->text) BAD("a message with no text");
        else {
          /* game-core's copy of the farewell must not reach the screen: the party screen said it (party_fake.c counts those) */
          static const char* const K[] = {"goodbye", "byebye", "farewell", "soLong", "thisIsWhereWePart",
                                          "illMissYou", "illNeverForgetYou", "untilWeMeetAgain", "sayonara", "smellYaLater"};
          for (size_t k = 0; k < sizeof K / sizeof *K; k++)
            if (!strcmp(e->text, K[k])) n_bv_farewell++;
        }
        break;
      case BEV_CATCH: {
        n_throw_ev++;
        if (e->battler != BV_ENEMY || !W.present[1]) BAD("a ball thrown at nobody (battler %d, enemy out %d)", e->battler, W.present[1]);
        if (e->a < 0 || e->a >= BALL_TYPES) BAD("a ball of type %d", e->a);
        if (e->caught ? e->b != 3 : (e->b < 0 || e->b > 2)) BAD("a ball that %s shows %d shakes", e->caught ? "holds" : "breaks free", e->b);
        /* the throw says what the run decided: "Gotcha!" (battle:pokemonCaught) comes with it when it held, and only then */
        bool gotcha = false;
        for (int j = i + 1; j < n; j++)
          if (ev[j].type == BEV_MESSAGE && ev[j].text && strstr(ev[j].text, "pokemonCaught")) gotcha = true;
        if (gotcha != e->caught) BAD("the ball %s but the messages %s Gotcha!", e->caught ? "held" : "broke free", gotcha ? "have" : "have no");
        if (e->caught) n_caught_ev++;
        else n_broke_ev++;
        if (W.mon[1].hp <= 0) BAD("a ball thrown at a Pokemon with no HP");
        break;
      }
      case BEV_INTRO_WILD:
        if (e->a == 0) {
          if (W.mon[1].species <= 0) BAD("wild intro with no enemy");
          W.present[1] = true;
        }
        break;
      case BEV_SUMMON:
        if (W.present[s]) BAD("summon over a Pokemon already out (side %d)", s);
        if (!e->mon || e->mon->species <= 0) {
          BAD("summon with no Pokemon");
          break;
        }
        if (e->mon->hp <= 0) BAD("summon of a fainted Pokemon (side %d, species %d)", s, e->mon->species);
        if (e->mon->max_hp <= 0 || e->mon->hp > e->mon->max_hp) BAD("summon with HP %d/%d", e->mon->hp, e->mon->max_hp);
        W.mon[s] = *e->mon;
        W.present[s] = true;
        W.fainted_shown[s] = false;
        break;
      case BEV_WITHDRAW:
        if (side_ok(e->battler, "withdraw")) W.present[s] = false;
        break;
      case BEV_DAMAGE:
        if (side_ok(e->battler, "damage")) {
          if (e->a < 0 || e->a > W.mon[s].max_hp) BAD("damage to HP %d of %d", e->a, W.mon[s].max_hp);
          W.mon[s].hp = e->a;
        }
        break;
      case BEV_HEAL:
        if (side_ok(e->battler, "heal")) {
          if (e->a < 0 || e->a > W.mon[s].max_hp) BAD("heal to HP %d of %d", e->a, W.mon[s].max_hp);
          W.mon[s].hp = e->a;
        }
        break;
      case BEV_FAINT:
        if (side_ok(e->battler, "faint")) {
          if (W.mon[s].hp != 0) BAD("faint at HP %d (side %d)", W.mon[s].hp, s);
          W.present[s] = false;
          W.fainted_shown[s] = true;
        }
        break;
      case BEV_STATUS:
        if (side_ok(e->battler, "status")) W.mon[s].status = e->a;
        break;
      case BEV_ABILITY:
        /* an ability bar names the Pokemon that has it, and one can trigger as it faints (Air Lock ending,
         * Aftermath); the bar is an overlay, the screen still has the fainted Pokemon's data */
        if (!W.fainted_shown[s]) side_ok(e->battler, "ability bar");
        break;
      case BEV_STAT_STAGES:
      case BEV_POKEMON_ANIM:
      case BEV_VISIBLE:
        side_ok(e->battler, "effect");
        break;
      case BEV_MOVE_ANIM:
        side_ok(e->battler, "move animation");
        break;
      case BEV_EXP:
        if (side_ok(e->battler, "exp")) {
          if (e->a < 0 || e->a > 1000) BAD("exp ratio %d", e->a);
          if (e->b > 0) {
            if (e->b <= W.mon[s].level) BAD("level up to %d from %d", e->b, W.mon[s].level);
            W.mon[s].level = e->b;
            if (e->mon) {
              W.mon[s].max_hp = e->mon->max_hp;
              W.mon[s].hp = e->mon->hp;
            }
          }
        }
        break;
      case BEV_MAX_HP:
        if (side_ok(e->battler, "max HP change")) {
          if (e->a <= 0) BAD("max HP changed to %d", e->a);
          W.mon[s].max_hp = e->a;
        }
        break;
      case BEV_FORM_CHANGE:
        if (side_ok(e->battler, "form change")) {
          W.mon[s].species = e->a;
          W.mon[s].form = e->b;
        }
        break;
      case BEV_WAIT:
        break;
      default:
        BAD("unknown screen event %d", e->type);
    }
  }
  W.playing = true;
  W.play_done = on_done;
  W.play_arg = arg;
}

bool bv_playing(void) { return W.playing; }

void bv_ask_command(const bv_command_opts* opts, void (*on_command)(bv_command, void*), void* arg) {
  n_ask++;
  const gc_run* r = rs_run();
  const gc_mon* me = gc_field_mon(r, 0);
  const gc_mon* foe = gc_field_mon(r, 1);
  if (!W.present[0] || !W.present[1]) BAD("asked for a command with %s out", !W.present[0] ? "no player" : "no enemy");
  if (!me || !foe) {
    BAD("asked for a command with no Pokemon on the field in the run");
  } else {
    /* the screen must show the battle game-core has */
    const bv_mon* a = &W.mon[0];
    const bv_mon* b = &W.mon[1];
    if (a->species != me->b.species || a->level != me->b.level || a->hp != me->b.hp || a->max_hp != me->b.stats[STAT_HP])
      BAD("screen player %d L%d %d/%d, run %d L%d %d/%d", a->species, a->level, a->hp, a->max_hp, me->b.species, me->b.level,
          me->b.hp, me->b.stats[STAT_HP]);
    if (b->species != foe->b.species || b->hp != foe->b.hp)
      BAD("screen enemy %d %d/%d, run %d %d/%d", b->species, b->hp, b->max_hp, foe->b.species, foe->b.hp, foe->b.stats[STAT_HP]);
    if (a->n_moves != me->b.n_moves) BAD("fight menu has %d moves, the Pokemon %d", a->n_moves, me->b.n_moves);
    for (int i = 0; i < me->b.n_moves && i < 4; i++) {
      int max = me->b.max_pp_override[i] >= 0 ? me->b.max_pp_override[i] : pr_moves[me->b.moves[i]].pp;
      if (a->moves[i] != me->b.moves[i] || a->pp_left[i] != max - me->b.pp_used[i])
        BAD("fight menu move %d is %d PP %d, the Pokemon's %d PP %d", i, a->moves[i], a->pp_left[i], me->b.moves[i], max - me->b.pp_used[i]);
    }
  }
  if (save_on && gc_run_can_save(r)) {
    /* the first command of a battle: the run must be in its slot by now, and the slot must hold this run */
    const bool just_resumed = W.resumed;
    if (W.resumed) {
      W.resumed = false;
      W.sp_open = true;
    } else if (!W.sp_open) {
      W.sp_open = true;
      W.expected_saves++;
      W.save_points++;
    }
    const rs_stats* st = rs_stats_get();
    if (st->saves != W.expected_saves) BAD("a first command was asked with %d saves written, %d expected", st->saves, W.expected_saves);
    rsv_info info;
    if (rsv_inspect(0, &info) != RSV_OK) {
      BAD("the slot does not load after a save: %s", info.why);
    } else {
      rsv_info now;
      memset(&now, 0, sizeof now);
      rsv_summarize(r, &now);
      if (info.wave != now.wave || info.score != now.score || info.money != now.money || info.n_party != now.n_party ||
          info.biome != now.biome || memcmp(info.party, now.party, sizeof(rsv_mon) * (size_t)now.n_party))
        BAD("the slot holds wave %d score %d, the run is at wave %d score %d", info.wave, info.score, now.wave, now.score);
    }
    if (resume_on && !just_resumed && W.save_points % 4 == 0) {
      W.resume_now = true; /* the player quits to the title and continues: the main loop does it */
      return;
    }
  }
  W.opts = *opts;
  W.on_command = on_command;
  W.command_arg = arg;
  W.asking = true;
}

/* ---- menus ---- */
void ui_push_options(const char* const* labels, int n, bool can_cancel, void (*on_done)(int, void*), void* arg) {
  n_options++;
  if (n <= 0) BAD("an option list with no options");
  for (int i = 0; i < n; i++)
    if (!labels[i] || !labels[i][0]) BAD("option %d of %d has no label", i, n);
  W.opt_done = on_done;
  W.opt_arg = arg;
  W.opt_n = n;
  for (int i = 0; i < n && i < 40; i++) snprintf(W.opt_label[i], sizeof W.opt_label[i], "%s", labels[i]);
  W.opt_cancel = can_cancel;
}

void ui_push_confirm(void (*on_done)(int yes, void* arg), void* arg) {
  W.confirm_done = on_done;
  W.confirm_arg = arg;
  /* On a switch screen the only yes / no is the full-party question: the party and the Pokemon about to be caught are noted now */
  const gc_run* r = rs_run();
  const gc_screen* sc = gc_run_screen(r);
  if (sc->kind == GC_SCREEN_SWITCH && !REL.armed) {
    if (r->n_party != 6 || sc->party < 0 || sc->party >= r->n_enemy) {
      BAD("a switch screen asked yes / no with %d in the party and the caught Pokemon %d", r->n_party, sc->party);
      return;
    }
    REL.armed = true;
    REL.pick = -1;
    REL.n = r->n_party;
    REL.releases0 = rs_stats_get()->releases;
    REL.kept0 = rs_stats_get()->kept_out;
    for (int i = 0; i < REL.n; i++) {
      const pr_pokemon* p = &r->party[r->party_order[i]].b;
      REL.id[i] = p->id;
      REL.species[i] = p->species;
    }
    REL.caught_id = r->enemy[sc->party].b.id;
    REL.caught_species = r->enemy[sc->party].b.species;
    for (int i = 0; i < REL.n; i++)
      for (int j = 0; j < i; j++)
        if (REL.id[i] == REL.id[j] || REL.id[i] == REL.caught_id) BAD("two Pokemon with the id %u", REL.id[i]);
  }
}

/* The question was answered: the party must be the one before, with the released member's place taken by the caught Pokemon (game-core
 * splices it in at the cursor, except that releasing the last place puts it one before: the party screen's RELEASE mode), or, when it
 * was let go, the same party */
static void rel_check(void) {
  const rs_stats* st = rs_stats_get();
  if (!REL.armed || (st->releases == REL.releases0 && st->kept_out == REL.kept0)) return;
  REL.armed = false;
  const gc_run* r = rs_run();
  uint32_t id[6];
  int species[6];
  if (r->n_party != REL.n) {
    BAD("the party has %d members after a full-party catch, it had %d", r->n_party, REL.n);
    return;
  }
  for (int i = 0; i < REL.n; i++) {
    const pr_pokemon* p = &r->party[r->party_order[i]].b;
    id[i] = p->id;
    species[i] = p->species;
  }
  uint32_t want_id[6];
  int want_species[6];
  memcpy(want_id, REL.id, sizeof want_id);
  memcpy(want_species, REL.species, sizeof want_species);
  if (st->releases != REL.releases0) {
    int k = -1;
    for (int i = 0; i < REL.n; i++) {
      bool still = false;
      for (int j = 0; j < REL.n; j++) still |= id[j] == REL.id[i];
      if (still) continue;
      if (k >= 0) BAD("two members left the party for one catch");
      k = i;
    }
    if (k < 0) {
      BAD("a member was released and all of them are still in the party");
      return;
    }
    if (REL.pick >= 0 && k != REL.pick) BAD("the player picked place %d in the release list, place %d left the party", REL.pick, k);
    want_id[k] = REL.caught_id;
    want_species[k] = REL.caught_species;
    if (k == REL.n - 1 && k > 0) {
      const uint32_t ti = want_id[k - 1];
      const int ts = want_species[k - 1];
      want_id[k - 1] = want_id[k];
      want_species[k - 1] = want_species[k];
      want_id[k] = ti;
      want_species[k] = ts;
    }
    n_rel_checked++;
  } else {
    n_decl_checked++;
  }
  for (int i = 0; i < REL.n; i++)
    if (id[i] != want_id[i] || species[i] != want_species[i])
      BAD("party slot %d holds species %d (id %u) after the full-party catch, expected species %d (id %u)", i, species[i], id[i],
          want_species[i], want_id[i]);
}

void ps_open(const pr_battle* bt, ps_mode mode, void (*on_pick)(int slot, int option, void* arg), void* arg) {
  n_party_opens++;
  if (mode == PS_RELEASE) {
    /* the real party screen (menus/party.c) in RELEASE mode: the player's actions go in as buttons */
    if (!REL.armed) BAD("the release list opened without the question before it");
    pf_open_release(bt, on_pick, arg);
    W.rel_open = true;
    return;
  }
  W.party_pick = on_pick;
  W.party_arg = arg;
  W.party_bt = bt;
  W.party_mode = mode;
}
void ps_close(void) {
  W.party_pick = NULL;
  if (W.rel_open) pf_close();
  W.rel_open = false;
}

void rh_open_shop(const shop_ui_args* args, const shop_ui_events* ev) {
  n_shops++;
  if (!args || !args->screen || !ev) {
    BAD("the shop opened with no screen or no callbacks");
    return;
  }
  X.shop = true;
  X.screen = *args->screen;
  X.shop_args = *args;
  X.shop_args.screen = &X.screen;
  X.shop_ev = *ev;
  if (!X.screen.n_options && !X.screen.n_shop) BAD("the reward screen opened with nothing on offer");
}

void rh_open_biome_map(const biome_map_args* args, const biome_map_events* ev) {
  n_maps++;
  if (!args || args->n_biomes <= 0 || !ev) BAD("the biome map opened with no biomes");
  X.biome = true;
  X.n_biomes = args ? args->n_biomes : 0;
  X.biome_ev = *ev;
}

void rh_close(void) { X.shop = X.biome = false; }

void ps_open_item(const void* run, int pmode, bool target_move, void (*on_pick)(int slot, int move_slot, void* arg), void* arg) {
  (void)run, (void)pmode;
  n_item_opens++;
  X.item_pick = on_pick;
  X.item_arg = arg;
  X.item_move = target_move;
}

void ball_menu_open(const int counts[BALL_TYPES], void (*on_pick)(int ball, void* arg), void* arg) {
  n_balls++;
  memcpy(W.ball_counts_copy, counts, sizeof W.ball_counts_copy);
  W.ball_pick = on_pick;
  W.ball_arg = arg;
}

/* the learn dialog: menus' learn_move_start shows a message, asks yes / no, takes the move to forget, shows a last
 * message and ends */
static const ps_learn_host* LH;
static void dlg_end_text_done(void* c) {
  (void)c;
  LH->on_end(LH->host);
}
static void dlg_yes(void* c) {
  (void)c;
  LH->on_result(LH->host, (int)rnd(5) - 1);
  LH->show_text(LH->host, "learned or not", true, dlg_end_text_done, NULL);
}
static void dlg_no(void* c) {
  (void)c;
  LH->on_result(LH->host, -1);
  LH->show_text(LH->host, "not learned", true, dlg_end_text_done, NULL);
}
static void dlg_text_done(void* c) {
  (void)c;
  LH->confirm(LH->host, dlg_yes, dlg_no, NULL);
}
void ps_learn_move(const pr_battle* bt, int party_slot, int move, int trainer_id, const ps_learn_host* host) {
  (void)bt, (void)move, (void)trainer_id;
  n_dialogs++;
  if (party_slot < 0 || party_slot >= bt->n_party[0]) BAD("learn dialog for slot %d of %d", party_slot, bt->n_party[0]);
  LH = host;
  host->show_text(host->host, "wants to learn a move", true, dlg_text_done, NULL);
}

/* ------------------------------------------------------------------ the fake player */

/* The fake player's better half: heal who needs it, revive the fainted, eat Rare Candies */
static int smart_option(int pick) {
  const gc_run* r = rs_run();
  if (!strncmp(W.opt_label[0], "Species_", 8)) {
    /* a party list (the Pokemon a reward is used on): the most hurt one that is alive */
    int best = pick;
    double worst = 2;
    for (int i = 0; i < W.opt_n && i < r->n_party; i++) {
      const pr_pokemon* p = &r->party[i].b;
      double f = p->stats[STAT_HP] ? (double)p->hp / p->stats[STAT_HP] : 1;
      if (f < worst) worst = f, best = i;
    }
    return best;
  }
  if (!strncmp(W.opt_label[0], "Move_", 5)) return pick;
  for (int i = 0; i < W.opt_n; i++)
    if (strstr(W.opt_label[i], "Revive") || strstr(W.opt_label[i], "Potion") || strstr(W.opt_label[i], "Rare Candy")) return i;
  return pick;
}

/* One thing the UI was waiting on is answered. False when nothing is pending. */
static bool step(void) {
  {
    static int last_refused;
    const int r = rs_stats_get()->refused;
    W.refused_streak = r != last_refused ? W.refused_streak + 1 : 0;
    last_refused = r;
  }
  if (W.playing) {
    void (*f)(void*) = W.play_done;
    void* a = W.play_arg;
    W.playing = false;
    W.play_done = NULL;
    if (f) f(a);
    return true;
  }
  if (T.fn) {
    void (*f)(void*) = T.fn;
    T.fn = NULL;
    f(T.arg);
    return true;
  }
  if (X.shop) {
    const gc_screen* sc = &X.screen;
    const shop_ui_events* ev = &X.shop_ev;
    const int k = (int)rnd(100);
    X.shop = false; /* the driver closes it before it answers, as the real screen does */
    if (W.refused_streak >= 3 || (!sc->n_options && !sc->n_shop)) {
      ev->on_skip(ev->ctx);
    } else if (sc->n_options && k < 70) {
      int pick = (int)rnd((uint32_t)sc->n_options);
      if (rnd(100) < 80)
        for (int i = 0; i < sc->n_options; i++) {
          const char* key = sc->options[i].key;
          if (key && (strstr(key, "REVIVE") || strstr(key, "POTION") || strstr(key, "RARE_CANDY"))) {
            pick = i;
            break;
          }
        }
      ev->on_select_reward(ev->ctx, pick);
    } else if (sc->n_shop && k < 85) {
      ev->on_select_shop(ev->ctx, (int)rnd((uint32_t)sc->n_shop));
    } else if (k < 90) {
      ev->on_reroll(ev->ctx, false);
    } else {
      ev->on_skip(ev->ctx);
    }
    return true;
  }
  if (X.item_pick) {
    void (*f)(int, int, void*) = X.item_pick;
    X.item_pick = NULL;
    const gc_run* r = rs_run();
    int best = 0;
    double worst = 2;
    for (int i = 0; i < r->n_party; i++) {
      const pr_pokemon* p = &r->party[i].b;
      const double fr = p->stats[STAT_HP] ? (double)p->hp / p->stats[STAT_HP] : 1;
      if (fr < worst) worst = fr, best = i;
    }
    if (rnd(8) == 0)
      f(6, -1, X.item_arg); /* backs out: the driver shows the rewards again */
    else
      f(best, X.item_move ? (int)rnd(4) : -1, X.item_arg);
    return true;
  }
  if (X.biome) {
    X.biome = false;
    X.biome_ev.on_select(X.biome_ev.ctx, (int)rnd((uint32_t)X.n_biomes));
    return true;
  }
  if (W.confirm_done) {
    void (*f)(int, void*) = W.confirm_done;
    W.confirm_done = NULL;
    f((int)rnd(3) != 0, W.confirm_arg);
    return true;
  }
  if (W.opt_done) {
    void (*f)(int, void*) = W.opt_done;
    W.opt_done = NULL;
    int pick = W.opt_cancel && rnd(7) == 0 ? -2 : (int)rnd((uint32_t)W.opt_n);
    if (rnd(100) < 80 && W.refused_streak < 3) pick = smart_option(pick);
    f(pick, W.opt_arg);
    return true;
  }
  if (W.ball_pick) {
    void (*f)(int, void*) = W.ball_pick;
    W.ball_pick = NULL;
    int avail[BALL_TYPES], n = 0;
    for (int i = 0; i < BALL_TYPES; i++)
      if (W.ball_counts_copy[i] > 0) avail[n++] = i;
    f(n && rnd(5) ? avail[rnd((uint32_t)n)] : -1, W.ball_arg);
    return true;
  }
  if (W.rel_open && pf_active()) {
    /* the full-party release list: pick a member (the options list, Release, the farewell), or back out to the question */
    n_party_full_swaps++;
    const int n = rs_run()->n_party;
    const int slot = rnd(10) < 7 ? (int)rnd((uint32_t)n) : -1;
    const int said = pf_farewells();
    REL.pick = slot;
    pf_release(slot);
    if (slot >= 0) {
      n_real_releases++;
      if (pf_farewells() != said + 1) BAD("the party screen said goodbye %d times for one release", pf_farewells() - said);
    } else {
      n_list_backs++;
      if (pf_farewells() != said) BAD("the party screen said goodbye after backing out");
    }
    return true;
  }
  if (W.party_pick) {
    void (*f)(int, int, void*) = W.party_pick;
    const pr_battle* bt = W.party_bt;
    W.party_pick = NULL;
    int ok[6], n = 0;
    for (int i = 0; i < bt->n_party[0]; i++)
      if (bt->party[0][i]->hp > 0 && !bt->party[0][i]->on_field) ok[n++] = i;
    if (W.party_mode == PS_SWITCH && rnd(6) == 0) f(6, PS_CANCEL, W.party_arg);
    else if (n && rnd(10)) f(ok[rnd((uint32_t)n)], PS_SEND_OUT, W.party_arg);
    else f((int)rnd((uint32_t)bt->n_party[0]), PS_SEND_OUT, W.party_arg); /* maybe not allowed: game-core refuses */
    return true;
  }
  if (W.asking) {
    W.asking = false;
    bv_command c = {0, 0};
    const uint32_t x = rnd(100);
    const bv_command_opts* o = &W.opts;
    const gc_mon* cur = gc_field_mon(rs_run(), 0);
    if (cur && o->can_switch && cur->b.hp * 4 < cur->b.stats[STAT_HP] && rnd(100) < 60) c.command = 2;
    else if (x < 6 && o->can_ball) c.command = 1;
    else if (x < 14 && o->can_switch) c.command = 2;
    else if (x < 15 && o->can_run) c.command = 3;
    else {
      int ok[4], n = 0;
      for (int i = 0; i < 4; i++)
        if (o->move_ok[i]) ok[n++] = i;
      c.move_slot = o->struggle || !n ? 0 : ok[rnd((uint32_t)n)];
      if (n && !o->struggle && rnd(100) < 80) {
        const gc_mon* me = gc_field_mon(rs_run(), 0);
        int best = -1;
        for (int k = 0; me && k < n; k++) {
          int pw = pr_moves[me->b.moves[ok[k]]].power;
          if (pw > best) best = pw, c.move_slot = ok[k];
        }
      }
    }
    W.on_command(c, W.command_arg);
    return true;
  }
  return false;
}

/* ------------------------------------------------------------------ main */

int main(int argc, char** argv) {
  int runs = argc > 1 ? atoi(argv[1]) : 20;
  int first = argc > 2 ? atoi(argv[2]) : 0;
  if (runs < 1) runs = 1;
  verbose_bad = getenv("RUNTEST_VERBOSE") != NULL;
  rs32 = 2463534242u ^ (uint32_t)first * 2654435761u;
  rnd(1);

  rs_config cfg;
  memset(&cfg, 0, sizeof cfg);
  cfg.entropy = (uint32_t)first * 7919u + 12345u;
  char seed[32];
  snprintf(seed, sizeof seed, "runtest%017d", first);
  cfg.seed = seed;
  const char* save_mode = getenv("RUNTEST_SAVE");
  save_on = save_mode && *save_mode;
  resume_on = save_on && !strcmp(save_mode, "resume");
  char save_dir[64] = "";
  if (save_on) {
    /* the saves go to a folder of their own (the SD card's path is relative here) */
    snprintf(save_dir, sizeof save_dir, "/tmp/runtest-saves-XXXXXX");
    if (!mkdtemp(save_dir) || chdir(save_dir) != 0 || mkdir("sdmc:", 0777) != 0 || !save_init()) {
      printf("FAILED: no folder for the saves\n");
      return 1;
    }
    cfg.save = true;
    cfg.slot = 0;
  }
  rs_start(&cfg);

  const rs_stats* st = rs_stats_get();
  uint32_t digest = 0;
  int ended = 0, clears = 0, last_ended = 0, idle = 0, deadlock = 0, no_progress = 0;
  long steps = 0, wave_sum = 0;
  int wave_hist[21] = {0};
  int last_waves = 0, last_nf = 0;
  while (ended < runs) {
    if (W.resume_now) {
      /* quit to the title and Continue: what the player gets back must be the run that was saved */
      rsv_info saved;
      memset(&saved, 0, sizeof saved);
      rsv_summarize(rs_run(), &saved);
      W.resume_now = false;
      W.playing = W.asking = false;
      W.play_done = NULL;
      char why[RSV_WHY_LEN] = "";
      if (rs_resume(&cfg, why) != RSV_OK) {
        BAD("could not continue from the slot: %s", why);
        break;
      }
      rsv_info back;
      memset(&back, 0, sizeof back);
      rsv_summarize(rs_run(), &back);
      if (back.wave != saved.wave || back.score != saved.score || back.money != saved.money || back.n_party != saved.n_party ||
          memcmp(back.party, saved.party, sizeof(rsv_mon) * (size_t)saved.n_party))
        BAD("the run that came back (wave %d) is not the one that was saved (wave %d)", back.wave, saved.wave);
      W.resumed = true;
      n_resumes++;
      continue;
    }
    if (!step()) {
      deadlock = 1;
      break;
    }
    steps++;
    rel_check();
    if (st->not_found != last_nf) {
      last_nf = st->not_found;
      BAD("a switch-in Pokemon was not found in the party");
    }
    if (st->game_overs + st->clears != last_ended) {
      const gc_run* r = rs_run();
      /* a run just ended (the driver is showing the game over): sample it */
      uint32_t h = 2166136261u;
      const uint32_t v[3] = {(uint32_t)r->wave, (uint32_t)r->score, (uint32_t)(st->game_overs + st->clears)};
      for (int i = 0; i < 3; i++) {
        h ^= v[i];
        h *= 16777619u;
      }
      digest ^= h;
      wave_sum += r->wave;
      wave_hist[r->wave / 10 > 20 ? 20 : r->wave / 10]++;
      ended++;
      clears = st->clears;
      last_ended = st->game_overs + st->clears;
      idle = 0;
    }
    if (st->waves != last_waves) {
      last_waves = st->waves;
      idle = 0;
    } else if (++idle > 20000) {
      no_progress = 1;
      break;
    }
    if (steps > 200000000L) break;
  }

  printf("%d runs (%d cleared), %ld steps, %d waves shown, digest %08x\n", ended, clears, steps, st->waves, digest);
  printf("average final wave %.1f; final wave:", ended ? (double)wave_sum / ended : 0.0);
  for (int i = 0; i <= 20; i++)
    if (wave_hist[i]) printf(" %d-%d:%d", i * 10, i * 10 + 9, wave_hist[i]);
  printf("\ncommands answered: %d fight, %d ball, %d switch, %d run; %d refused by game-core\n", st->fights, st->balls, st->switches,
         st->runs_tried, st->refused);
  printf("screens: command %d, switch %d, learn move %d, evolve %d, reward %d, biome %d, mystery %d, game over %d, clear %d\n",
         st->screens[GC_SCREEN_COMMAND], st->screens[GC_SCREEN_SWITCH], st->screens[GC_SCREEN_LEARN_MOVE],
         st->screens[GC_SCREEN_EVOLVE], st->screens[GC_SCREEN_REWARD], st->screens[GC_SCREEN_BIOME], st->screens[GC_SCREEN_MYSTERY],
         st->screens[GC_SCREEN_GAME_OVER], st->screens[GC_SCREEN_CLEAR]);
  printf("fake screen: %d scenes, %d command prompts, %d option lists, %d party screens, %d ball lists, %d learn dialogs, %u screen events\n",
         n_scenes, n_ask, n_options, n_party_opens, n_balls, n_dialogs, st->bev);
  printf("catching: %d throws shown (%d held, %d broke free); full-party catches: %d release lists opened, %d members released through the party "
         "screen (%d farewells there, %d from game-core's events), %d let go, %d times backed out of the list; party checked after %d "
         "releases and %d let go\n",
         n_throw_ev, n_caught_ev, n_broke_ev, n_party_full_swaps, n_real_releases, pf_farewells(), n_bv_farewell, st->kept_out, n_list_backs,
         n_rel_checked, n_decl_checked);
  if (!n_caught_ev || !n_broke_ev) BAD("a throw outcome never came up (held %d, broke free %d)", n_caught_ev, n_broke_ev);
  if (n_throw_ev != st->throws || n_caught_ev != st->catches) BAD("the driver counted %d throws (%d held), the screen saw %d (%d)", st->throws, st->catches, n_throw_ev, n_caught_ev);
  if (!n_rel_checked) BAD("no full-party release reached");
  if (!st->kept_out || !n_decl_checked) BAD("no full-party catch was let go");
  if (!n_list_backs) BAD("backing out of the release list never came up");
  if (pf_farewells() != n_real_releases || pf_farewells() != st->releases) BAD("%d farewells for %d releases", pf_farewells(), st->releases);
  if (n_bv_farewell) BAD("game-core's farewell reached the screen %d times (the party screen already said it)", n_bv_farewell);
  if (REL.armed) BAD("a full-party question was left unanswered");
  printf("reward screens: %d shops, %d item targets, %d biome maps\n", n_shops, n_item_opens, n_maps);
  if (save_on) {
    printf("saving: %d save points, %d written, %d failed, %d resumed from the slot\n", W.expected_saves, st->saves, st->save_failed,
           n_resumes);
    if (st->save_failed) BAD("%d saves failed", st->save_failed);
    if (st->saves != W.expected_saves) BAD("%d saves written for %d save points", st->saves, W.expected_saves);
    if (!W.expected_saves) BAD("no save points were reached");
    if (resume_on && !n_resumes) BAD("no run was resumed");
  }
  printf("events handled: message %d, battle %d, wave %d, summon %d, recall %d, hp %d, exp %d, level up %d, learned %d, evolved %d, mystery %d\n",
         st->events[GC_EV_MESSAGE], st->events[GC_EV_BATTLE], st->events[GC_EV_WAVE], st->events[GC_EV_SUMMON], st->events[GC_EV_RECALL],
         st->events[GC_EV_HP], st->events[GC_EV_EXP], st->events[GC_EV_LEVEL_UP], st->events[GC_EV_MOVE_LEARNED], st->events[GC_EV_EVOLVED],
         st->events[GC_EV_MYSTERY]);
  printf("duplicate messages dropped %d; HP/status brought up to date at a command %d; switch-ins not found %d; screen/run mismatches %d; stuck %d; model violations %d\n",
         st->dup_dropped, st->hp_resynced, st->not_found, st->shown_mismatch, st->stuck, violations);
  if (violations) printf("first violation: %s\n", first_violation);
  if (deadlock) printf("DEADLOCK: nothing was pending after %ld steps (run %d, wave %d)\n", steps, st->runs, rs_run()->wave);
  if (no_progress) printf("NO PROGRESS: no new wave in 20000 steps (wave %d)\n", rs_run()->wave);
  const bool fail = violations || st->not_found || st->shown_mismatch || st->stuck || deadlock || no_progress || ended < runs;
  printf(fail ? "FAILED\n" : "OK\n");
  if (save_dir[0]) {
    /* the temporary folder this run made */
    char cmd[128];
    if (chdir("/") == 0) {
      snprintf(cmd, sizeof cmd, "rm -rf %s", save_dir);
      if (system(cmd) != 0) printf("(could not remove %s)\n", save_dir);
    }
  }
  return fail ? 1 : 0;
}
