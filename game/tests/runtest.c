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
 *                        then agree with it at the next command.
 *   RUNTEST_SAVE=quit    the same, and at every 7th command (and every 7th reward screen) the player presses START: the in-run menu's
 *                        Save and Quit is answered Yes (the run is dropped and continued from its slot, which must hold the save of
 *                        the battle's start: the run that comes back is the slot's) or backed out of (the run goes on), alternately.
 *                        Fails when the menu is not offered, is told another turn than the battle's, or the driver goes on after a quit.
 * The music is the app's own (bgm.c, bgm_choose.c, bgm_player.c) over a fake DSP (snd_fake.c). The runs fail when a track is started
 * while a battle animation plays (the SD card reads would stall it), when a scene starts with other music than upstream's
 * Battle.getBgmOverride gives (checked against bgm_choose, not the app's own scene code), when a command is asked with other music
 * playing (a trainer's battle music must have replaced its encounter music by then), when a trainer's reward screen is not under its
 * victory music, or when the music does not fade at the end of a run.
 *
 *   RUNTEST_CUSTOM=1     the runs are custom runs (task C10): saved into the custom slot, with the rematch hooks, over a real custom store, profile
 *                        transactions and system data in a temporary folder; every defeat is committed as the app does (a Nemesis when the
 *                        faint hook named the finisher). The fake player answers the offer at random. RUNTEST_CUSTOM=strong: once a
 *                        Nemesis is remembered the party is level 100 and healed at every command until wave 36 (so the runs reach the
 *                        rematch windows), then it is left at 1 HP. Fails when an offer is left unanswered, a rematch is paid other
 *                        than once, a voucher is not a claim, or the roster stops validating. With RUNTEST_SAVE=resume the custom
 *                        slot is dropped and continued from as well.
 *   RUNTEST_SYS=1        the runs have system data (gc_run.sys), as the app gives them: a new save's (the nine default starters),
 *                        and every run starts with a team the starter select would offer. RUNTEST_SYS=all: every starter unlocked.
 *                        The system data lives across the runs (what one catches the next one can pick), and with RUNTEST_SAVE
 *                        each save point serializes it and loads it back: the copy must be byte for byte the same. */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "gc/data.h"
#include "../../game-core/src/internal.h"   /* gc_mon_recalc, for the strong party of RUNTEST_CUSTOM=strong */
#include "battle_view.h"
#include "gm_item_ui.h"
#include "ball_menu.h"
#include "combat_observer.h"
#include "custom_run.h"
#include "custom_store.h"
#include "profile_tx.h"
#include "system_data.h"
#include "bgm.h"
#include "bgm_choose.h"
#include "i18n.h"
#include "party_fake.h"
#include "party_screen.h"
#include "pr_data.h"
#include "pr_enums.h"
#include "reward_host.h"
#include "run_history.h"
#include "run_menu.h"
#include "run_saves.h"
#include "run_screen.h"
#include "save.h"
#include "snd_fake.h"
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
  int biome;
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
  /* the in-run menu (RUNTEST_SAVE=quit) */
  int menu_asks;          /* commands and reward screens that could have opened it */
  bool menu_try;          /* press START at the next step */
  bool quit_now;          /* Save and Quit was answered Yes: the main loop drops the run and continues it from its slot */
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
  void (*menu)(void* arg);   /* rh_set_menu: START on the reward screen */
  void* menu_arg;
} X;
/* Manage Items: the transfer screen's callbacks while it is open, and what went through */
static struct {
  void (*move)(int, int, int, void*);
  void (*done)(void*);
  void* arg;
} MI;
static int n_manage, n_transfers, n_transfer_copies, n_transfer_refused, n_discards;
static int n_shops, n_maps, n_item_opens, n_seamless, n_level_ups, n_level_windows, n_foe_bars, n_foe_hidden, n_key_bars, n_trays, n_fainted_balls;
static bool save_on, resume_on, quit_on;
static int n_menu_opened, n_menu_yes, n_menu_no, n_quit_callbacks;
/* RUNTEST_CUSTOM */
static bool custom_on, custom_strong;
static unsigned vouchers0;
static struct {
  int commits, nemeses, store_refused, no_finisher;
  int refusal[CC_REFUSE_COUNT_];
} CU;
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

extern int gc_evolution_moves(const gc_mon *m, uint16_t *out, int max);

/* An evolution prompt: party member, species before, target species, the moves it knew, the player's answer */
static struct {
  bool armed;
  int party_idx;
  int species_before;
  int species_target;
  int n_moves_before;
  uint16_t moves_before[4];
  int choice;
} EVO;
/* After an accepted evolution: the new species' evolution moves the party member did not know (game-core skips the known ones and the
 * unimplemented ones). Each must be learned straight away (room in the moveset) or offered in the learn dialog (a full moveset). */
static struct {
  bool pending;
  int party_idx;
  int n_moves_before;
  int n;
  uint16_t move[16];
  bool offered[16];
} EVM;
static int n_evo_checked, n_cancel_checked, n_evo_moves_checked;

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
    for (int p = 0; p < r->n_party; p++) {
      printf("  party %d id %u species %d hp %d/%d:", p, (unsigned)r->party[p].b.id, r->party[p].b.species, r->party[p].b.hp, r->party[p].b.stats[0]);
      for (int i = 0; i < r->n_items; i++)
        if (r->items[i].pokemon_id == r->party[p].b.id) printf(" %s/%d x%d", r->items[i].key, r->items[i].arg, r->items[i].stack);
      printf("\n");
    }
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

/* ---- the music ---- */
static struct {
  bool have;
  char battle[BGM_NAME_LEN]; /* what this wave's battle plays once it is going, as the chooser answers for the run when the scene is set up */
  int scenes, encounter_scenes, commands, victories, run_end_fades, starts;
  char seen[400][BGM_NAME_LEN];
  int n_seen;
} M;

/* every track start is SD card reading on the main thread: never while a battle animation plays */
static void snd_start(const char* key) {
  if (W.playing) BAD("the music track %s was started while a battle animation plays", key);
  M.starts++;
}

/* what upstream's Battle.getBgmOverride / the biome's music gives for the run as it is now (bgm_choose, not the app's scene code) */
static const char* expect_track(const gc_run* r, bool started, char buf[BGM_NAME_LEN]) {
  bgm_scene sc;
  memset(&sc, 0, sizeof sc);
  sc.wave = r->wave;
  sc.biome = r->biome;
  sc.battle_type = r->battle_type;
  sc.me_default = r->battle_type == BATTLE_TYPE_MYSTERY_ENCOUNTER;
  sc.started = started;
  sc.final_boss = r->run.mode == GAME_MODES_CLASSIC && r->wave == 200;
  sc.all_gens = true;
  sc.trainer_type = r->trainer_type;
  sc.trainer_variant = r->trainer_variant;
  for (int i = 0; i < r->n_enemy && i < 6; i++) {
    sc.enemy[i].species = r->enemy[i].b.species;
    sc.enemy[i].form = r->enemy[i].b.form;
    sc.n_enemy++;
  }
  return bgm_scene_track(&sc, buf);
}

static void music_seen(const char* name) {
  for (int i = 0; i < M.n_seen; i++)
    if (!strcmp(M.seen[i], name)) return;
  if (M.n_seen < 400) snprintf(M.seen[M.n_seen++], BGM_NAME_LEN, "%s", name);
}

/* a scene was just set up (at black): its music is upstream's, with the trainer's encounter music before its first send-out */
static void music_scene(void) {
  const gc_run* r = rs_run();
  char b1[BGM_NAME_LEN], b2[BGM_NAME_LEN];
  const char* first = expect_track(r, false, b1);
  const char* battle = expect_track(r, true, b2);
  if (strcmp(bgm_current(), first)) BAD("the wave starts with the music \"%s\", upstream plays \"%s\"", bgm_current(), first);
  char key[BGM_NAME_LEN + 16];
  snprintf(key, sizeof key, "audio/bgm/%s", first);
  if (strcmp(SND.key, key) || SND.volume != 1.0f) BAD("the DSP plays \"%s\" at volume %.2f, the wave wants \"%s\"", SND.key, SND.volume, key);
  snprintf(M.battle, sizeof M.battle, "%s", battle);
  M.have = true;
  M.scenes++;
  if (strcmp(first, battle)) M.encounter_scenes++;
  music_seen(first);
}

/* a command is asked: the battle is on, with the music it plays from here on */
static void music_command(void) {
  if (!M.have) return;
  M.commands++;
  char key[BGM_NAME_LEN + 16];
  snprintf(key, sizeof key, "audio/bgm/%s", M.battle);
  if (strcmp(bgm_current(), M.battle)) BAD("a command was asked with the music \"%s\", the battle plays \"%s\"", bgm_current(), M.battle);
  if (strcmp(SND.key, key) || SND.volume != 1.0f) BAD("a command was asked with the DSP on \"%s\" at volume %.2f, want \"%s\"", SND.key, SND.volume, key);
  music_seen(M.battle);
}

/* the reward screen: a won trainer battle has its victory music, a won wild battle the music it had */
static void music_reward(void) {
  if (!M.have) return;
  const gc_run* r = rs_run();
  if (r->battle_type == BATTLE_TYPE_TRAINER) {
    const char* v = bgm_trainer_victory(r->trainer_type);
    if (strcmp(bgm_current(), v)) BAD("the reward screen after trainer %d plays \"%s\", its victory music is \"%s\"", r->trainer_type, bgm_current(), v);
    M.victories++;
    music_seen(v);
  } else if (strcmp(bgm_current(), M.battle)) {
    BAD("the reward screen after a wild battle plays \"%s\", the battle played \"%s\"", bgm_current(), M.battle);
  }
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
  /* the next wave of the same biome with the lead out (no fade, the player's platform stays): only ever with the lead out and under
   * the biome that was on screen */
  if (setup->next_encounter) {
    n_seamless++;
    if (!W.begun || !setup->player_present || setup->biome != W.biome) BAD("a next-encounter scene with the lead %s, biome %d after %d", setup->player_present ? "out" : "away", setup->biome, W.biome);
  }
  W.begun = true;
  W.biome = setup->biome;
  n_scenes++;
  W.mon[0] = setup->player;
  W.mon[1] = setup->enemy;
  W.present[0] = setup->player_present;
  W.present[1] = false;
  W.fainted_shown[0] = W.fainted_shown[1] = false;
  W.playing = W.asking = false;
  W.sp_open = false;
  music_scene();
}

bv_mon* bv_mon_of(int battler) { return &W.mon[battler >= 2 ? 1 : 0]; }
int bv_biome(void) { return W.begun ? W.biome : -1; }

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
      case BEV_LEVEL_UP:
        /* "X grew to Lv. N!" and the stat window: the gains can't be negative at a higher level of the same species */
        n_level_ups++;
        if (!e->text || !e->text[0]) BAD("level up without its message");
        if (e->stats) {
          n_level_windows++;
          for (int k = 0; k < 6; k++)
            if (e->stats[k] <= 0 || e->stats[6 + k] < e->stats[k])
              BAD("level up stat %d went from %d to %d", k, e->stats[k], e->stats[6 + k]);
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
          if (e->c) W.mon[s].gender = e->c - 2;
        }
        break;
      case BEV_TRANSFORM:
        if (side_ok(e->battler, "Transform")) {
          if (!e->mon || e->mon->species <= 0 || e->mon->n_moves < 0 || e->mon->n_moves > 4) {
            BAD("Transform without a valid display payload");
            break;
          }
          if (e->mon->hp != W.mon[s].hp || e->mon->max_hp != W.mon[s].max_hp
              || e->mon->level != W.mon[s].level || strcmp(e->mon->name, W.mon[s].name)
              || e->mon->shiny != W.mon[s].shiny || e->mon->variant != W.mon[s].variant)
            BAD("Transform changed permanent HP, level, name or shiny identity");
          W.mon[s] = *e->mon;
        }
        break;
      case BEV_ILLUSION:
        /* PR_DEV_ILLUSION: a disguise starts or ends: the look, name and types change; HP, level and shininess stay */
        if (side_ok(e->battler, "Illusion")) {
          if (!e->mon || e->mon->species <= 0) {
            BAD("Illusion without a valid display payload");
            break;
          }
          if (e->mon->hp != W.mon[s].hp || e->mon->max_hp != W.mon[s].max_hp || e->mon->level != W.mon[s].level
              || e->mon->shiny != W.mon[s].shiny || e->mon->variant != W.mon[s].variant)
            BAD("Illusion changed permanent HP, level or shiny identity");
          W.mon[s] = *e->mon;
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
  music_command();
  const gc_run* r = rs_run();
  /* the bottom screen's foe item bar: one icon per enemy item the item table can draw, each with its own stack */
  {
    int drawable = 0;
    for (int i = 0; i < r->n_enemy_items; i++) {
      const gc_item* it = &r->enemy_items[i];
      if (!it->key || it->stack <= 0) continue;
      if (gm_item_ui_find(it->key, it->arg) || gm_item_ui_find(it->key, -1)) drawable++;
      else n_foe_hidden++;
    }
    const int want = drawable < BV_MAX_FOE_ITEMS ? drawable : BV_MAX_FOE_ITEMS;
    if (!opts || opts->n_foe_items != want) BAD("foe item bar has %d items, the enemy %d", opts ? opts->n_foe_items : -1, want);
    else if (want) {
      n_foe_bars++;
      for (int i = 0; i < want; i++) {
        bool found = false;
        for (int j = 0; j < r->n_enemy_items && !found; j++) found = r->enemy_items[j].stack == opts->foe_items[i].count;
        if (!opts->foe_items[i].icon || !found) BAD("foe item %d: icon %s, stack %d", i, opts->foe_items[i].icon ? "set" : "missing", opts->foe_items[i].count);
      }
    }
  }
  /* the top screen's second bar: the player's own items no Pokemon holds; and the party tray: one ball per slot */
  {
    int key = 0;
    for (int i = 0; i < r->n_items; i++) {
      const gc_item* it = &r->items[i];
      if (!it->key || it->stack <= 0 || it->pokemon_id || it->detached) continue;
      if (gm_item_ui_find(it->key, it->arg) || gm_item_ui_find(it->key, -1)) key++;
    }
    const int want = key < BV_MAX_FOE_ITEMS ? key : BV_MAX_FOE_ITEMS;
    if (!opts || opts->n_key_items != want) BAD("key item bar has %d items, the player %d", opts ? opts->n_key_items : -1, want);
    else if (want) {
      n_key_bars++;
      for (int i = 0; i < want; i++)
        if (!opts->key_items[i].icon || !opts->key_items[i].name || opts->key_items[i].count <= 0) BAD("key item %d has no icon, name or stack", i);
    }
    if (!opts || !opts->has_party_balls) {
      BAD("no party tray");
    } else {
      n_trays++;
      for (int i = 0; i < 6; i++) {
        const int exp = i >= r->n_party ? BV_BALL_EMPTY : r->party[i].b.hp > 0 ? BV_BALL_ALIVE : BV_BALL_FAINTED;
        if (opts->party_balls[i] != exp) BAD("party tray slot %d shows %d, the party has %d", i, opts->party_balls[i], exp);
        if (opts->party_balls[i] == BV_BALL_FAINTED) n_fainted_balls++;
      }
    }
  }
  const gc_mon* me = gc_field_mon(r, 0);
  const gc_mon* foe = gc_field_mon(r, 1);
  if (!W.present[0] || !W.present[1]) BAD("asked for a command with %s out", !W.present[0] ? "no player" : "no enemy");
  if (!me || !foe) {
    BAD("asked for a command with no Pokemon on the field in the run");
  } else {
    /* the screen must show the battle game-core has: what it looks like, a disguise included (PR_DEV_ILLUSION) */
    const bv_mon* a = &W.mon[0];
    const bv_mon* b = &W.mon[1];
    const pr_form* player_form = pr_shown_form(&r->bt, &me->b);
    const pr_form* enemy_form = pr_shown_form(&r->bt, &foe->b);
    const int player_gender = pr_shows_illusion(&r->bt, &me->b) ? pr_apparent_gender(&me->b, true) : pr_get_gender(&me->b);
    if (a->species != player_form->species || a->level != me->b.level || a->hp != me->b.hp || a->max_hp != me->b.stats[STAT_HP])
      BAD("screen player %d L%d %d/%d, run %d L%d %d/%d", a->species, a->level, a->hp, a->max_hp, player_form->species, me->b.level,
          me->b.hp, me->b.stats[STAT_HP]);
    if (b->species != enemy_form->species || b->hp != foe->b.hp)
      BAD("screen enemy %d %d/%d, run %d %d/%d", b->species, b->hp, b->max_hp, enemy_form->species, foe->b.hp, foe->b.stats[STAT_HP]);
    if (a->form != player_form->form_index || a->gender != player_gender || a->shiny != me->b.shiny || a->variant != me->variant)
      BAD("the displayed form, gender or shiny identity differs from the effective battle Pokemon");
    if (a->n_moves != PR_N_MOVES(&me->b)) BAD("fight menu has %d moves, the Pokemon %d", a->n_moves, PR_N_MOVES(&me->b));
    for (int i = 0; i < PR_N_MOVES(&me->b) && i < 4; i++) {
      int base = pr_moves[PR_MOVES(&me->b)[i]].pp;
      int step = base / 5 < 1 ? 1 : base / 5;
      int max = PR_MAX_PP(&me->b)[i] > 0 ? PR_MAX_PP(&me->b)[i] : base + PR_PP_UP(&me->b)[i] * step;
      if (a->moves[i] != PR_MOVES(&me->b)[i] || a->pp_left[i] != max - PR_PP_USED(&me->b)[i])
        BAD("fight menu move %d is %d PP %d, the Pokemon's %d PP %d", i, a->moves[i], a->pp_left[i], PR_MOVES(&me->b)[i], max - PR_PP_USED(&me->b)[i]);
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
    if ((custom_on ? cr_inspect(0, &info) : rsv_inspect(0, &info)) != RSV_OK) {
      BAD("the slot does not load after a save: %s", info.why);
    } else {
      rsv_info now;
      memset(&now, 0, sizeof now);
      rsv_summarize(r, &now);
      if (info.wave != now.wave || info.score != now.score || info.money != now.money || info.n_party != now.n_party ||
          info.biome != now.biome || memcmp(info.party, now.party, sizeof(rsv_mon) * (size_t)now.n_party))
        BAD("the slot holds wave %d score %d, the run is at wave %d score %d (money %d/%d, biome %d/%d, party %d/%d, party bytes %s)", info.wave, info.score,
            now.wave, now.score, (int)info.money, (int)now.money, (int)info.biome, (int)now.biome, (int)info.n_party, (int)now.n_party,
            memcmp(info.party, now.party, sizeof(rsv_mon) * (size_t)now.n_party) ? "differ" : "same");
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
  if (quit_on) {
    if (!opts->on_menu) BAD("a command was asked with no menu hook (START would do nothing) in a run that has an app around");
    if (++W.menu_asks % 7 == 0) W.menu_try = true;
  }
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
  if (sc->kind == GC_SCREEN_EVOLVE && !EVO.armed) {
    EVO.armed = true;
    EVO.party_idx = sc->party;
    EVO.species_before = r->party[sc->party].b.species;
    EVO.species_target = sc->species;
    EVO.n_moves_before = r->party[sc->party].b.n_moves;
    for (int i = 0; i < EVO.n_moves_before && i < 4; i++) EVO.moves_before[i] = r->party[sc->party].b.moves[i];
    EVO.choice = -1;
  }
}

static void evo_check(void) {
  if (!EVO.armed || EVO.choice == -1) return;
  EVO.armed = false;
  const gc_run* r = rs_run();
  if (EVO.party_idx < 0 || EVO.party_idx >= r->n_party) {
    BAD("evolution party index %d out of bounds (n_party %d)", EVO.party_idx, r->n_party);
    return;
  }
  int current_species = r->party[EVO.party_idx].b.species;
  if (EVO.choice == 1) {
    if (current_species != EVO.species_target) {
      BAD("party slot %d holds species %d after evolution, expected target species %d",
          EVO.party_idx, current_species, EVO.species_target);
      return;
    }
    n_evo_checked++;
    uint16_t mv[16];
    const int n = gc_evolution_moves(&r->party[EVO.party_idx], mv, 16);
    EVM.pending = !custom_strong;   /* a party the strong mode set to level 100 gets moves the evolution check does not expect */
    EVM.party_idx = EVO.party_idx;
    EVM.n_moves_before = EVO.n_moves_before;
    EVM.n = 0;
    for (int i = 0; i < n; i++) {
      bool known = false;
      for (int k = 0; k < EVO.n_moves_before; k++) known |= EVO.moves_before[k] == mv[i];
      if (pr_moves[mv[i]].unimplemented || (known && mv[i] != MOVE_ID_SKETCH)) continue;
      EVM.move[EVM.n] = mv[i];
      EVM.offered[EVM.n++] = false;
    }
  } else {
    if (current_species != EVO.species_before) {
      BAD("party slot %d holds species %d after cancelled evolution, expected original species %d",
          EVO.party_idx, current_species, EVO.species_before);
      return;
    }
    n_cancel_checked++;
  }
}

/* Once the screen has moved past the learn dialogs: every evolution move must have been learned or offered */
static void evo_move_check(void) {
  if (!EVM.pending) return;
  const gc_run* r = rs_run();
  const gc_screen* sc = gc_run_screen(r);
  if (sc->kind == GC_SCREEN_LEARN_MOVE || sc->kind == GC_SCREEN_NONE) return;
  EVM.pending = false;
  const pr_pokemon* p = &r->party[EVM.party_idx].b;
  for (int i = 0; i < EVM.n; i++) {
    bool in_moves = false;
    for (int k = 0; k < p->n_moves; k++) in_moves |= p->moves[k] == EVM.move[i];
    /* a full moveset always gets the dialog; with room the move is simply learned */
    const bool ok = EVM.offered[i] || (EVM.n_moves_before < 4 && in_moves);
    if (!ok) {
      BAD("evolution move %d of party slot %d (species %d, %d moves before) was neither learned nor offered in the learn dialog",
          (int)EVM.move[i], EVM.party_idx, p->species, EVM.n_moves_before);
      return;
    }
    n_evo_moves_checked++;
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
void ps_set_run(const void* gc_run_ptr) { (void)gc_run_ptr; }
void ps_close(void) {
  W.party_pick = NULL;
  if (W.rel_open) pf_close();
  W.rel_open = false;
}

void rh_open_shop(const shop_ui_args* args, const shop_ui_events* ev) {
  n_shops++;
  music_reward();
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

void rh_close(void) {
  X.shop = X.biome = false;
  X.menu = NULL;
}
void rh_set_menu(void (*fn)(void*), void* arg) {
  X.menu = fn;
  X.menu_arg = arg;
}

/* The in-run menu (run_menu.c has its own host test): answered here at once, Yes and No alternately. It checks what the driver tells it. */
static bool g_menu_open;
/* Yes is only answered at a wave other than the one the last Yes was at: a quit sends the run back to the start of its battle, so a
 * Yes at every 14th prompt of a long battle would replay that battle forever. */
static int g_last_quit_wave = -1;
bool run_menu_is_open(void) { return g_menu_open; }
bool run_menu_open(const runmenu_args* a) {
  const gc_run* r = rs_run();
  n_menu_opened++;
  if (!quit_on) BAD("the in-run menu was opened in a run that is not testing it");
  if (a->turn != r->bt.turn) BAD("the menu was told turn %d, the battle is at turn %d", a->turn, r->bt.turn);
  if (a->unsaved) BAD("the menu was told the save failed, but every write went through");
  if (!a->on_quit) BAD("the menu has no Save and Quit to call");
  if (n_menu_opened % 2 == 0 || r->wave == g_last_quit_wave) {
    n_menu_no++;
    return true; /* backed out with B: the menu is gone and nothing else happens */
  }
  n_menu_yes++;
  g_last_quit_wave = r->wave;
  a->on_quit(a->arg);
  return true;
}

void ps_open_item(const void* run, int pmode, bool target_move, void (*on_pick)(int slot, int move_slot, void* arg), void* arg) {
  (void)run, (void)pmode;
  n_item_opens++;
  X.item_pick = on_pick;
  X.item_arg = arg;
  X.item_move = target_move;
}

void ps_open_transfer(const void* run, void (*on_move)(int, int, int, void*), void (*on_done)(void*), void* arg) {
  (void)run;
  n_manage++;
  MI.move = on_move;
  MI.done = on_done;
  MI.arg = arg;
}

static int held_copies(const gc_run* r, const char* key, int16_t arg, uint32_t id) {
  int n = 0;
  for (int i = 0; i < r->n_items; i++)
    if (r->items[i].pokemon_id == id && !strcmp(r->items[i].key, key) && r->items[i].arg == arg) n += r->items[i].stack;
  return n;
}

/* one transfer (or discard) through the screen's callback, checked against tryTransferHeldItemModifier's rules, then out */
static void drive_manage_items(void) {
  const gc_run* r = rs_run();
  int cand[GC_MAX_ITEMS], nc = 0;
  for (int i = 0; i < r->n_items; i++) {
    const gc_item* it = &r->items[i];
    if (!it->pokemon_id || it->non_transferable || it->stack <= 0) continue;
    for (int p = 0; p < r->n_party; p++)
      if (r->party[p].b.id == it->pokemon_id) cand[nc++] = i;
  }
  if (!nc) BAD("Manage Items was offered with no transferable held item in the party");
  if (nc && r->n_party > 1) {
    const int src = cand[rnd((uint32_t)nc)];
    const gc_item it = r->items[src];
    int to = (int)rnd((uint32_t)r->n_party);
    if (r->party[to].b.id == it.pokemon_id) to = (to + 1) % r->n_party;
    const int qty = 1 + (int)rnd((uint32_t)it.stack);
    const uint32_t to_id = r->party[to].b.id;
    const int had_from = held_copies(r, it.key, it.arg, it.pokemon_id), had_to = held_copies(r, it.key, it.arg, to_id);
    const int max = gc_item_max_held(r, it.key, it.arg, &r->party[to]);
    const int n_before = r->n_items;
    if (rnd(100) < 15) {
      MI.move(src, 1, -1, MI.arg);
      n_discards++;
      if (held_copies(r, it.key, it.arg, it.pokemon_id) != had_from - 1) BAD("a discard of 1 %s left the wrong count", it.key);
    } else {
      MI.move(src, qty, to, MI.arg);
      trail_add("manage: %d %s/%d from id %u to party %d (had %d)", qty, it.key, it.arg, (unsigned)it.pokemon_id, to, had_to);
      const int now_from = held_copies(r, it.key, it.arg, it.pokemon_id), now_to = held_copies(r, it.key, it.arg, to_id);
      const int moved = had_from - now_from;
      if (now_to - had_to != moved) BAD("a transfer of %s lost or made copies (%d left, %d arrived)", it.key, moved, now_to - had_to);
      if (had_to > 0 && had_to < max && moved != (qty < max - had_to ? qty : max - had_to))
        BAD("a transfer of %d %s onto a stack of %d (max %d) moved %d", qty, it.key, had_to, max, moved);
      if (had_to > 0 && had_to >= max && moved) BAD("a transfer onto a full stack of %s moved %d", it.key, moved);
      if (!had_to && moved != qty) BAD("a transfer of %d %s to a Pokemon without it moved %d", qty, it.key, moved);
      if (moved) {
        n_transfers++;
        n_transfer_copies += moved;
        if (r->items[r->n_items - 1].pokemon_id != to_id || strcmp(r->items[r->n_items - 1].key, it.key))
          BAD("the transferred %s is not at the end of the list (addModifier)", it.key);
      } else {
        n_transfer_refused++;
        if (r->n_items != n_before) BAD("a refused transfer changed the item list");
      }
    }
  }
  void (*d)(void*) = MI.done;
  MI.move = NULL;
  MI.done = NULL;
  d(MI.arg);
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
static int learn_forget_ally_only = -1;
static bool learn_decline_ally_only;
static void dlg_end_text_done(void* c) {
  (void)c;
  learn_forget_ally_only = -1;
  learn_decline_ally_only = false;
  LH->on_end(LH->host);
}
static void dlg_yes(void* c) {
  (void)c;
  int forget = (int)rnd(5) - 1;
  if (learn_forget_ally_only >= 0) forget = learn_forget_ally_only;
  LH->on_result(LH->host, forget);
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
  (void)trainer_id;
  learn_forget_ally_only = -1;
  learn_decline_ally_only = !bt->double_battle && pr_moves[move].target == MOVE_TARGET_NEAR_ALLY;
  n_dialogs++;
  if (party_slot < 0 || party_slot >= bt->n_party[0]) BAD("learn dialog for slot %d of %d", party_slot, bt->n_party[0]);
  if (EVM.pending) {
    /* the dialog's slot is the battle place of the party member (party_order), not its party index */
    const gc_run* r = rs_run();
    for (int k = 0; k < r->n_party; k++) {
      if (r->party_order[k] != EVM.party_idx || k != party_slot) continue;
      for (int i = 0; i < EVM.n; i++)
        if (EVM.move[i] == (uint16_t)move && !EVM.offered[i]) {
          EVM.offered[i] = true;
          break;
        }
    }
  }
  /* In singles, replace an ally-only move when learning a move that can
   * target the user or opponent. Helping Hand with no ally spends no PP in
   * the pinned source and can otherwise leave both sides in a stalemate. */
  if (!bt->double_battle && party_slot >= 0 && party_slot < bt->n_party[0]
      && pr_moves[move].target != MOVE_TARGET_NEAR_ALLY) {
    const pr_pokemon* p = bt->party[0][party_slot];
    for (int i = 0; i < p->n_moves; i++)
      if (pr_moves[p->moves[i]].target == MOVE_TARGET_NEAR_ALLY) {
        learn_forget_ally_only = i;
        break;
      }
  }
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

/* RUNTEST_CUSTOM=strong: a run that holds a Nemesis for a rematch is played by a party that cannot lose before wave 36 (and cannot win after) */
static void strong_cheat(void) {
  if (!custom_on || !custom_strong) return;
  cr_view v;
  cr_view_get(&v);
  if (!v.active || v.sched.state == CC_SCHED_NO_CANDIDATE) return;
  gc_run* r = (gc_run*)rs_run();
  if (save_on && gc_run_can_save(r)) return;   /* the first turn is in the slot as it is: the slot check compares the two at every ask */
  if (r->wave > 36) {
    for (int i = 0; i < r->n_party; i++) r->party[i].b.hp = r->party[i].b.hp > 1 ? 1 : r->party[i].b.hp;
    const gc_mon* f = gc_field_mon(r, 0);
    if (f && W.present[0] && W.mon[0].species == f->b.species) W.mon[0].hp = f->b.hp;
    return;
  }
  for (int i = 0; i < r->n_party; i++) {
    if (r->party[i].b.level < 100) {
      r->party[i].b.level = 100;
      gc_mon_recalc(r, &r->party[i]);
    }
    r->party[i].b.hp = r->party[i].b.stats[0];
  }
  /* the fake screen's model of the Pokemon on the field follows (it is built from events, and this edit has none) */
  const gc_mon* f = gc_field_mon(r, 0);
  if (f && W.present[0] && W.mon[0].species == f->b.species) {
    W.mon[0].level = f->b.level;
    W.mon[0].max_hp = f->b.stats[0];
    W.mon[0].hp = f->b.hp;
  }
}

/* A custom run just ended: what the app does at its end (the defeat's report, the Nemesis if the faint hook named the finisher, the slot) */
static void custom_commit(void) {
  const cs_result res = cs_commit_defeat(co_report_get(), rs_custom_defeat());
  CU.commits++;
  if (res.outcome != CS_STORED && res.outcome != CS_REPLAYED) BAD("a custom run's report was not stored (outcome %d)", (int)res.outcome);
  if (res.outcome == CS_STORED && res.commit.record_id && res.commit.refusal == CC_SUPPORT_OK) CU.nemeses++;
  else if (rs_custom_defeat()) {
    CU.store_refused++;
    if (res.commit.refusal > 0 && res.commit.refusal < CC_REFUSE_COUNT_) CU.refusal[res.commit.refusal]++;
  }
  else CU.no_finisher++;
  cr_clear(0);
  const cc_meta* m = cs_meta();
  if (!m || cc_meta_validate(m) != CC_OK) BAD("the remembered enemies no longer validate after a run");
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
  if (quit_on && X.shop && !W.menu_try && ++W.menu_asks % 7 == 0) W.menu_try = true;
  if (W.menu_try) {
    /* START: on the command menu through the battle screen's hook, on the reward screen through the reward host's */
    W.menu_try = false;
    if (W.asking && W.opts.on_menu) {
      W.opts.on_menu(W.command_arg);
      return true;
    }
    if (X.shop) {
      if (!X.menu) {
        BAD("the reward screen has no menu hook (START would do nothing)");
      } else {
        X.menu(X.menu_arg);
        return true;
      }
    }
  }
  if (MI.done) {
    drive_manage_items();
    return true;
  }
  if (X.shop) {
    const gc_screen* sc = &X.screen;
    const shop_ui_events* ev = &X.shop_ev;
    {
      bool any = false;
      const gc_run* r = rs_run();
      for (int i = 0; i < r->n_items; i++)
        for (int p = 0; p < r->n_party; p++)
          if (r->items[i].pokemon_id && r->items[i].pokemon_id == r->party[p].b.id && !r->items[i].non_transferable &&
              !r->items[i].detached && r->items[i].stack > 0)
            any = true;
      if (any != X.shop_args.party_has_held_item) BAD("Manage Items shows %s, the party %s a transferable held item",
                                                      X.shop_args.party_has_held_item ? "up" : "not", any ? "has" : "has no");
    }
    if (X.shop_args.party_has_held_item && ev->on_manage_items && rnd(100) < 12) {
      X.shop = false;
      ev->on_manage_items(ev->ctx);
      return true;
    }
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
    int yes;
    if (EVO.armed && EVO.choice == -1) {
      yes = (int)rnd(2);
      EVO.choice = yes;
    } else {
      yes = (int)rnd(3) != 0;
      if (learn_forget_ally_only >= 0) yes = 1;
      if (learn_decline_ally_only) yes = 0;
    }
    f(yes, W.confirm_arg);
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
    if(W.party_mode==PS_REVIVAL_BLESSING) {
      /* The real party screen filters for fainted members and exposes Revive, not Send Out. */
      for(int i=0;i<bt->n_party[0];i++)if(bt->party[0][i]->hp<=0)ok[n++]=i;
      if(!n){BAD("Revival Blessing picker has no fainted member");return true;}
      f(ok[rnd((uint32_t)n)],PS_REVIVE_PICK,W.party_arg);
      return true;
    }
    for (int i = 0; i < bt->n_party[0]; i++)
      if (bt->party[0][i]->hp > 0 && !bt->party[0][i]->on_field) ok[n++] = i;
    if (W.party_mode == PS_SWITCH && rnd(6) == 0) f(6, PS_CANCEL, W.party_arg);
    else if (n && rnd(10)) f(ok[rnd((uint32_t)n)], PS_SEND_OUT, W.party_arg);
    else f((int)rnd((uint32_t)bt->n_party[0]), PS_SEND_OUT, W.party_arg); /* maybe not allowed: game-core refuses */
    return true;
  }
  if (W.asking) {
    W.asking = false;
    strong_cheat();
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
          int pw = pr_moves[PR_MOVES(&me->b)[ok[k]]].power;
          if (pw > best) best = pw, c.move_slot = ok[k];
        }
      }
    }
    W.on_command(c, W.command_arg);
    return true;
  }
  return false;
}

/* ------------------------------------------------------------------ system data (RUNTEST_SYS) */

static gc_system g_sys, g_sys_back; /* 724 KB each */
static int n_sys_saves, caught_at_start;
static void unlock_all_starters(gc_system* sys) {
  gc_system_init_defaults(sys);
  for (int i = 0; i < gc_starter_cost_count; i++) {
    const uint16_t id = gc_starter_costs[i].id;
    sys->dex[id].caught_attr = gc_species_full_unlocks_data(id) | 1ull;
    sys->dex[id].seen_attr = sys->dex[id].caught_attr;
    sys->dex[id].nature_attr = 0x3FFFFFEu;
    sys->starters[id].ability_attr = 7;
    sys->starters[id].passive_attr = 3;
    for (int k = 0; k < 6; k++) sys->dex[id].ivs[k] = 15;
  }
}
/* the app's sysd_save, with a check instead of the SD card: the system data serialized and loaded back is the same */
static bool save_sys_check(void) {
  const size_t cap = 1024 * 1024;
  uint8_t* buf = malloc(cap);
  if (!buf) return false;
  const size_t n = gc_system_save(&g_sys, buf, cap);
  const char* why = NULL;
  if (!n) BAD("the system data could not be serialized");
  else if (!gc_system_load(&g_sys_back, buf, n, &why)) BAD("the system data did not load back: %s", why ? why : "?");
  else if (memcmp(&g_sys, &g_sys_back, sizeof g_sys)) BAD("the system data loaded back is not the one that was saved");
  free(buf);
  n_sys_saves++;
  return n > 0;
}

/* ------------------------------------------------------------------ the Run History, over real runs */

/* When a run has just ended the entry the app would write for it (rhist_record) must describe it: the result, the party as it ended, and for
 * a defeat what beat the player in the way upstream tells battles apart (wild Pokemon with a species and a level, a trainer with its name,
 * the rival). It must encode, decode and encode to the same bytes. */
static struct {
  long runs, wins, wild, wild_double, trainer, trainer_double, rival, none, mystery;
} HIST;

static void hist_check(const gc_run* r, bool cleared) {
  static rh_entry e, back;
  static uint8_t buf1[RH_ENTRY_MAX + 8], buf2[RH_ENTRY_MAX + 8];
  rhist_record(r, cleared, 1760000000, NULL, &e);
  HIST.runs++;
  if (e.victory != cleared || e.wave != r->wave || e.biome != r->biome || e.score != r->score || e.mode != r->run.mode) BAD("the history entry has the wrong result");
  if (e.n_party != (r->n_party < RH_PARTY ? r->n_party : RH_PARTY)) BAD("the history entry has %d party members, the run %d", e.n_party, r->n_party);
  for (int i = 0; i < e.n_party; i++) {
    const gc_mon* m = &r->party[r->party_order[i] >= 0 && r->party_order[i] < r->n_party ? r->party_order[i] : i];
    if (e.party[i].species != m->b.species || e.party[i].level != (uint32_t)m->b.level || e.party[i].n_moves != m->b.n_moves ||
        e.party[i].stats[0] != (uint32_t)m->b.stats[0] || e.party[i].fainted != (m->b.hp <= 0))
      BAD("the history entry's party member %d is not the run's", i);
    if (!e.party[i].species || e.party[i].level < 1) BAD("party member %d of the history entry has no species or level", i);
  }
  if (cleared) {
    HIST.wins++;
    if (e.defeat != RH_DEFEAT_NONE || e.n_enemy || e.trainer[0]) BAD("a win names what beat the player");
  } else {
    switch (e.defeat) {
      case RH_DEFEAT_WILD:
        HIST.wild++;
        HIST.wild_double += e.n_enemy == 2;
        HIST.mystery += r->battle_type == BATTLE_TYPE_MYSTERY_ENCOUNTER;
        if (e.n_enemy < 1 || e.n_enemy > RH_ENEMIES) BAD("a wild defeat with %d enemies", e.n_enemy);
        for (int i = 0; i < e.n_enemy; i++)
          if (!e.enemy[i].species || e.enemy[i].level < 1) BAD("wild enemy %d of the history entry has no species or level", i);
        if (r->trainer_type >= 0 && r->battle_type == BATTLE_TYPE_TRAINER) BAD("a trainer battle recorded as wild");
        break;
      case RH_DEFEAT_TRAINER:
      case RH_DEFEAT_RIVAL:
        if (e.defeat == RH_DEFEAT_RIVAL) HIST.rival++;
        else HIST.trainer++;
        HIST.trainer_double += e.trainer_double;
        if (r->trainer_type < 0) BAD("a trainer defeat with no trainer type");
        if ((e.defeat == RH_DEFEAT_RIVAL) != (r->trainer_type >= TRAINER_TYPE_RIVAL)) BAD("rival or not: type %d", r->trainer_type);
        if (e.defeat == RH_DEFEAT_TRAINER && !e.trainer[0]) BAD("a defeat by a trainer with no name (type %d)", r->trainer_type);
        if (e.n_enemy) BAD("a trainer defeat keeps wild enemies");
        break;
      default:
        HIST.none++;
        BAD("a game over that names nothing (battle type %d, trainer %d, %d enemies)", r->battle_type, r->trainer_type, r->n_enemy);
        break;
    }
  }
  const size_t n1 = rhist_encode(&e, 1, buf1, sizeof buf1);
  if (!n1) {
    BAD("the history entry does not encode");
    return;
  }
  if (rhist_decode(buf1, n1, &back, 1) != 1) {
    BAD("the history entry does not decode");
    return;
  }
  const size_t n2 = rhist_encode(&back, 1, buf2, sizeof buf2);
  if (n2 != n1 || memcmp(buf1, buf2, n1)) BAD("the history entry changes when it is written and read");
}

/* ------------------------------------------------------------------ the combat observer, over real runs */

/* The driver hands the observer every drain of every run. When a run has just ended its report must be there, describe that run, hold a
 * history custom-core accepts, and show that the real game-core events were all taken: nothing refused, dropped or lost. */
static bool g_run_resumed;   /* the run on screen was continued from its slot at some point */
static struct {
  long runs, ranges, batches, events, turns, faint_known, defeats, clears, resumed, empty_ranges, ignored, evicted, full_turns;
} OBS;

static void obs_check(const gc_run* r, bool cleared) {
  const co_report* rep = co_report_get();
  if (!rep) {
    BAD("no combat report when the run ended");
    return;
  }
  const co_stats* o = &rep->stats;
  if (rep->outcome != (cleared ? CC_OUT_CLEAR : CC_OUT_DEFEAT)) BAD("the combat report says outcome %d for a %s", rep->outcome, cleared ? "clear" : "game over");
  if (rep->wave != r->wave) BAD("the combat report says wave %u, the run ended on %d", (unsigned)rep->wave, r->wave);
  if (rep->biome != r->biome) BAD("the combat report says biome %d, the run ended in %d", (int)rep->biome, r->biome);
  if (cc_history_validate(&rep->history) != CC_OK) BAD("the combat report's history is not a valid cc_history");
  for (int t = 0; t < rep->history.n_turns; t++)
    if (cc_turn_validate(&rep->history.turns[t]) != CC_OK) BAD("turn %d of the combat report is not valid", t);
  if (o->rejected || o->unrepresentable || o->bad_range || o->unkeyed || o->skipped_drains || o->replayed_drains)
    BAD("the observer refused or lost real events: rejected %u, unrepresentable %u, bad range %u, unkeyed %u, skipped drains %u, replayed %u (last status %d)",
        o->rejected, o->unrepresentable, o->bad_range, o->unkeyed, o->skipped_drains, o->replayed_drains, o->last_status);
  if (o->loss_flagged || rep->untracked_loss) BAD("the observer flagged a loss in a real run (%u batches flagged)", o->loss_flagged);
  if (!o->drains || !o->ranges || !o->batches || !o->events_in) BAD("the observer saw nothing of the run (drains %u, ranges %u, events %u)", o->drains, o->ranges, o->events_in);
  if (o->ranges != o->batches + o->empty_ranges) BAD("%u battle ranges but %u batches and %u empty", o->ranges, o->batches, o->empty_ranges);
  if (rep->history.n_turns < 1) BAD("a finished run left no turn in the combat history");
  if (rep->began_mid != g_run_resumed) BAD("began_mid %d, the run was%s resumed", rep->began_mid, g_run_resumed ? "" : " not");
  if (!cleared && !rep->fallen_known) BAD("a game over with no last-fallen Pokemon in the combat history");
  if (cleared && rep->fallen_known) BAD("a clear names a Pokemon that fell last");
  OBS.runs++;
  OBS.ranges += o->ranges;
  OBS.batches += o->batches;
  OBS.events += o->events_in;
  OBS.turns += rep->history.n_turns;
  OBS.faint_known += rep->fallen_known;
  OBS.defeats += !cleared;
  OBS.clears += cleared;
  OBS.resumed += rep->began_mid;
  OBS.empty_ranges += o->empty_ranges;
  OBS.ignored += o->ignored;
  OBS.evicted += o->evicted;
  for (int t = 0; t < rep->history.n_turns; t++) OBS.full_turns += rep->history.turns[t].flags == 0;
}

/* ------------------------------------------------------------------ main */

/* app_flow.c's on_run_quit: the driver has stopped and faded; the app goes to the title (the main loop continues from the slot) */
static void quit_cb(void* arg) {
  (void)arg;
  n_quit_callbacks++;
  W.quit_now = true;
}

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
  quit_on = save_on && !strcmp(save_mode, "quit");
  if (quit_on) {
    cfg.on_quit = quit_cb;
    cfg.quit_arg = NULL;
  }
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
  cfg.n_starters = 0;
  const char* sys_mode = getenv("RUNTEST_SYS");
  const bool sys_on = sys_mode && *sys_mode;
  if (sys_on) {
    if (!strcmp(sys_mode, "all"))
      unlock_all_starters(&g_sys);
    else
      gc_system_init_defaults(&g_sys);
    cfg.sys = &g_sys;
    cfg.save_sys = save_sys_check;
    for (int i = 0; i <= GC_MAX_SPECIES_ID; i++) caught_at_start += g_sys.dex[i].caught_attr != 0;
  }
  const char* custom_mode = getenv("RUNTEST_CUSTOM");
  custom_on = custom_mode && *custom_mode;
  custom_strong = custom_on && !strcmp(custom_mode, "strong");
  if (custom_on) {
    /* custom runs need the whole stack the app gives them: a folder, the custom store, profile transactions and the system data */
    if (!save_dir[0]) {
      snprintf(save_dir, sizeof save_dir, "/tmp/runtest-saves-XXXXXX");
      if (!mkdtemp(save_dir) || chdir(save_dir) != 0 || mkdir("sdmc:", 0777) != 0 || !save_init()) {
        printf("FAILED: no folder for the saves\n");
        return 1;
      }
    }
    cfg.custom = true;
    cfg.save = true;
    cfg.slot = 0;
    gc_system* sys = sysd_get();
    unlock_all_starters(sys);
    cfg.sys = sys;
    cfg.save_sys = sysd_save;
    sysd_save();
    ptx_set_scene_seed("runtest-custom");
    ptx_boot();
    vouchers0 = sys->voucher_counts[GC_VOUCHER_REGULAR];
    co_set_uid_source(cs_begin_run);
    cr_settle_owed();
  }
  SND.on_start = snd_start;
  rs_start(&cfg);

  const rs_stats* st = rs_stats_get();
  uint32_t digest = 0;
  int ended = 0, clears = 0, last_ended = 0, idle = 0, deadlock = 0, no_progress = 0;
  long steps = 0, wave_sum = 0;
  int wave_hist[21] = {0};
  int last_waves = 0, last_nf = 0;
  while (ended < runs) {
    if (W.quit_now) {
      /* Save and Quit, answered Yes: nothing may go on, and Continue must bring back what the slot holds (the save of the battle's start,
       * not the battle in progress) */
      W.quit_now = false;
      const int quits_before = st->quits;
      rsv_info slot;
      memset(&slot, 0, sizeof slot);
      if ((custom_on ? cr_inspect(0, &slot) : rsv_inspect(0, &slot)) != RSV_OK) BAD("Save and Quit with nothing in the slot: %s", slot.why);
      if (!quits_before) BAD("the driver did not count the quit");
      W.playing = W.asking = false;
      W.play_done = NULL;
      X.shop = X.biome = false;
      if (T.fn) BAD("a postponed step was left after the quit");
      char why[RSV_WHY_LEN] = "";
      if (rs_resume(&cfg, why) != RSV_OK) {
        BAD("could not continue from the slot after Save and Quit: %s", why);
        break;
      }
      rsv_info back;
      memset(&back, 0, sizeof back);
      rsv_summarize(rs_run(), &back);
      if (back.wave != slot.wave || back.score != slot.score || back.money != slot.money || back.n_party != slot.n_party ||
          memcmp(back.party, slot.party, sizeof(rsv_mon) * (size_t)slot.n_party))
        BAD("Continue after Save and Quit brought back wave %d, the slot held wave %d", back.wave, slot.wave);
      W.resumed = true;
      g_run_resumed = true;
      n_resumes++;
      continue;
    }
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
      g_run_resumed = true;
      n_resumes++;
      continue;
    }
    if (!step()) {
      deadlock = 1;
      break;
    }
    steps++;
    bgm_tick(16.0f);
    rel_check();
    evo_check();
    evo_move_check();
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
      /* the music fades out at the end of a run (bgm_tick: a little time passes and the volume is below full) */
      bgm_tick(1.0f);
      if (SND.key[0] && SND.volume >= 1.0f) BAD("the music did not fade at the end of the run (volume %.4f)", SND.volume);
      M.run_end_fades++;
      wave_sum += r->wave;
      wave_hist[r->wave / 10 > 20 ? 20 : r->wave / 10]++;
      obs_check(r, st->clears != clears);
      hist_check(r, st->clears != clears);
      if (custom_on) custom_commit();
      g_run_resumed = false;
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
  printf("run history: %ld entries recorded: %ld wins, %ld defeats by wild Pokemon (%ld doubles, %ld in a mystery encounter), %ld by a trainer (%ld pairs), %ld by the rival, %ld naming nothing\n",
         HIST.runs, HIST.wins, HIST.wild, HIST.wild_double, HIST.mystery, HIST.trainer, HIST.trainer_double, HIST.rival, HIST.none);
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
  printf("evolution: %d evolutions completed, %d cancelled; party species checked after %d evolutions and %d cancels; %d post-evolution moves checked (learned or offered)\n",
         st->evolves, st->cancels, n_evo_checked, n_cancel_checked, n_evo_moves_checked);
  if (EVO.armed) BAD("an evolution prompt was left unanswered");
  if (EVM.pending) BAD("an evolution's moves were left unchecked");
  if (st->evolves != n_evo_checked || st->cancels != n_cancel_checked)
    BAD("evolution count mismatch (driver %d / %d, checked %d / %d)", st->evolves, st->cancels, n_evo_checked, n_cancel_checked);
  /* Evolutions are rare in a random run (about 0.7 per 100 runs): the coverage floor needs the documented 1000 runs. Below that it is
   * reported as not checked, never skipped silently. */
  if (runs >= 1000) {
    if (st->evolves < 5) BAD("fewer than 5 evolutions completed (%d)", st->evolves);
    if (st->cancels < 5) BAD("fewer than 5 evolutions cancelled (%d)", st->cancels);
    if (n_evo_moves_checked < 3) BAD("fewer than 3 post-evolution moves learned or offered (%d)", n_evo_moves_checked);
  } else {
    printf("evolution coverage floor (5 evolved, 5 cancelled, 3 moves) NOT checked: it needs 1000+ runs, this one has %d\n", runs);
  }
  printf("combat observer: %ld reports checked (%ld defeats, %ld clears, %ld from a resumed run), %ld batches, %ld observed events, %ld turns kept "
         "(%ld complete), %ld last-fallen named, %ld empty ranges, %ld HP-0 events ignored, %ld events evicted\n",
         OBS.runs, OBS.defeats, OBS.clears, OBS.resumed, OBS.batches, OBS.events, OBS.turns, OBS.full_turns, OBS.faint_known, OBS.empty_ranges,
         OBS.ignored, OBS.evicted);
  /* every GC_EV_BATTLE the screen played the observer took in at its drain, once (a run dropped and continued from its slot leaves its first
   * part out of the reports, so this holds for runs played through) */
  if (!resume_on && !quit_on && OBS.ranges != st->events[GC_EV_BATTLE])
    BAD("the observer took in %ld battle ranges, the screen played %d", OBS.ranges, st->events[GC_EV_BATTLE]);
  if (OBS.runs != ended) BAD("%ld combat reports checked for %d runs", OBS.runs, ended);
  printf("reward screens: %d shops, %d item targets, %d biome maps\n", n_shops, n_item_opens, n_maps);
  printf("scenes: %d, of them %d next waves of the same biome (no fade, the player's platform stays)\n", n_scenes, n_seamless);
  if (!n_seamless && n_scenes > 50) BAD("no scene was a next wave of the same biome");
  printf("level ups: %d, of them %d with the stat window\n", n_level_ups, n_level_windows);
  printf("commands with the foe's item bar: %d; enemy items with no icon row: %d\n", n_foe_bars, n_foe_hidden);
  printf("commands with the key item bar: %d; party trays: %d (%d fainted balls)\n", n_key_bars, n_trays, n_fainted_balls);
  printf("Manage Items: opened %d, transfers %d (%d copies), refused %d, discards %d\n", n_manage, n_transfers,
         n_transfer_copies, n_transfer_refused, n_discards);
  if (!n_foe_bars && n_scenes > 50) BAD("no command showed the foe's items");
  if (!n_key_bars && n_scenes > 50) BAD("no command showed key items");
  if (!n_fainted_balls && n_scenes > 50) BAD("no party tray showed a fainted member");
  if ((!n_manage || !n_transfers || !n_discards) && n_scenes > 50) BAD("Manage Items was never used (opened %d, transfers %d, discards %d)", n_manage, n_transfers, n_discards);
  if (n_level_windows < n_level_ups) BAD("%d level ups without the stat window", n_level_ups - n_level_windows);
  if (!n_level_ups && n_scenes > 50) BAD("no level up was shown");
  printf("music: %d scenes under upstream's track (%d with encounter music before the trainer's first send-out), %d commands under the battle's "
         "music, %d trainer victories under their victory music, %d run ends faded; %d track starts, %d different tracks heard\n",
         M.scenes, M.encounter_scenes, M.commands, M.victories, M.run_end_fades, M.starts, M.n_seen);
  if (M.scenes != st->waves) BAD("%d scenes set up, the driver counted %d waves", M.scenes, st->waves);
  /* a trainer is beaten in about one run in nine: the floor needs a few hundred runs */
  if (runs >= 300) {
    if (!M.victories) BAD("no trainer victory reached the reward screen");
    if (!M.encounter_scenes) BAD("no trainer with encounter music came up");
  } else {
    printf("music coverage floor (a trainer victory, a trainer with encounter music) NOT checked: it needs 300+ runs, this one has %d\n", runs);
  }
  if (save_on) {
    printf("saving: %d save points, %d written, %d failed, %d resumed from the slot\n", W.expected_saves, st->saves, st->save_failed,
           n_resumes);
    if (st->save_failed) BAD("%d saves failed", st->save_failed);
    if (st->saves != W.expected_saves) BAD("%d saves written for %d save points", st->saves, W.expected_saves);
    if (!W.expected_saves) BAD("no save points were reached");
    if (resume_on && !n_resumes) BAD("no run was resumed");
    if (quit_on) {
      printf("in-run menu: %d opened, %d Save and Quit answered Yes (%d continued from the slot), %d backed out of\n", n_menu_opened, n_menu_yes, n_resumes,
             n_menu_no);
      if (!n_menu_yes || !n_menu_no) BAD("the menu was not both quit and backed out of (%d / %d)", n_menu_yes, n_menu_no);
      if (st->quits != n_menu_yes || n_quit_callbacks != n_menu_yes || n_resumes != n_menu_yes)
        BAD("%d Yes answers, %d counted by the driver, %d app callbacks, %d resumes", n_menu_yes, st->quits, n_quit_callbacks, n_resumes);
    }
  }
  if (custom_on) {
    const cr_stats* cs = cr_stats_get();
    unsigned regular = sysd_get()->voucher_counts[GC_VOUCHER_REGULAR];
    const cc_meta* m = cs_meta();
    unsigned claimed = 0, stored = 0;
    for (unsigned i = 0; m && i < m->roster.count; i++) {
      claimed += m->roster.rec[i].life == CC_LIFE_REWARD_CLAIMED;
      stored += m->roster.rec[i].life == CC_LIFE_STORED;
    }
    printf("custom: %d runs ended and committed (%d Nemeses made, %d finishers the store refused, %d defeats with no reliable finisher); %u waves reported, %u windows; "
           "%u offers (%u accepted, %u declined), %u enemies built, %u refused; %u won, %u lost, %u vouchers claimed (%u in the wallet); roster %u (%u claimed, %u stored); "
           "%u runs resumed, %u candidates handed over, %u without; %u stopped for the card\n",
           CU.commits, CU.nemeses, CU.store_refused, CU.no_finisher, cs->waves, cs->windows, cs->offers, cs->accepts, cs->declines, cs->built, cs->refused, cs->wins,
           cs->losses, cs->claims, regular, m ? m->roster.count : 0, claimed, stored, cs->runs_resumed, cs->candidates, cs->candidates_none, cs->degraded);
    printf("custom: finishers the store refused, by reason:");
    for (int k = 1; k < CC_REFUSE_COUNT_; k++)
      if (CU.refusal[k]) printf(" %s %d,", cc_refusal_str(k), CU.refusal[k]);
    printf("\n");
    if (CU.commits != ended) BAD("%d custom runs ended, %d committed", ended, CU.commits);
    /* the runs' own rewards (trainer vouchers) are in the wallet too: it holds at least the claims */
    if (regular - vouchers0 < cs->claims) BAD("%u vouchers claimed, %u more in the wallet", cs->claims, regular - vouchers0);
    if (cs->wins != cs->claims) BAD("%u rematches won, %u vouchers claimed", cs->wins, cs->claims);
    if (cs->offers != cs->accepts + cs->declines) BAD("%u offers, %u answers", cs->offers, cs->accepts + cs->declines);
    if (cs->built + cs->refused != cs->accepts) BAD("%u accepted, %u built or refused", cs->accepts, cs->built + cs->refused);
    if (cs->degraded) BAD("%u runs lost their rematch to a card that refused a write", cs->degraded);
    if (m && cc_meta_validate(m) != CC_OK) BAD("the remembered enemies do not validate");
    if (ptx_pending()) BAD("a transaction is left pending");
    if (custom_strong && runs >= 100) {
      if (!cs->offers) BAD("no rematch was offered");
      if (!cs->wins) BAD("no rematch was won");
    }
  }
  if (sys_on) {
    int caught = 0, seen = 0;
    for (int i = 0; i <= GC_MAX_SPECIES_ID; i++) {
      caught += g_sys.dex[i].caught_attr != 0;
      seen += g_sys.dex[i].seen_attr != 0;
    }
    if (save_on && !n_sys_saves) BAD("the system data was never saved");
    save_sys_check(); /* and once more at the end */
    printf("system data (%s): %d species caught at the start, %d at the end (%d seen); %d serialized and loaded back\n", sys_mode,
           caught_at_start, caught, seen, n_sys_saves);
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
