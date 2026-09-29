/*
 * menutest: standalone app to run the menu screens in Azahar or on a console
 * before the game app exists. No game logic: a made-up party, and every pick
 * the screens report is written to the bottom screen and to
 * sdmc:/3ds/pokerogue/menutest.log (the emulator test reads it back).
 *
 * Controls: D-pad/A/B drive the screen. L and R switch the party mode,
 * SELECT toggles a double battle, START quits.
 */
#include <3ds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "menus.h"
#include "pak.h"
#include "rr.h"
#include "snd.h"

static FILE* g_log;
static char g_last[4][96];

static void logf_(const char* fmt, ...) {
  char buf[96];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof buf, fmt, ap);
  va_end(ap);
  memmove(g_last[1], g_last[0], sizeof g_last[0] * 3);
  snprintf(g_last[0], sizeof g_last[0], "%s", buf);
  if (g_log) {
    fprintf(g_log, "%s\n", buf);
    fflush(g_log);
  }
}

/* ------------------------------------------------------------ a made-up party */

static menu_mon g_party[6];
static int g_n;

static menu_mon make(uint16_t species, const char* name, int level, int hp, int max_hp, int status, int gender) {
  menu_mon m;
  memset(&m, 0, sizeof m);
  m.species = species;
  snprintf(m.name, sizeof m.name, "%s", name);
  m.level = level;
  m.hp = hp;
  m.max_hp = max_hp;
  m.status = status;
  m.gender = gender;
  m.allowed_in_battle = true;
  m.fusion_gender = MG_GENDERLESS;
  return m;
}

static void add_item(menu_mon* m, const char* name, const char* icon, int count, int max) {
  menu_held_item* it = &m->items[m->nitems++];
  snprintf(it->name, sizeof it->name, "%s", name);
  it->icon = icon;
  it->count = count;
  it->max = max;
  it->transferable = true;
}

static void party_reset(void) {
  g_n = 6;
  g_party[0] = make(257, "Blaziken", 54, 131, 170, MS_NONE, MG_MALE);
  g_party[0].shiny = true;
  g_party[0].variant = 1;
  g_party[0].has_evolutions = false;
  g_party[0].moves[0] = (menu_move){394, 3, 15}; /* Flare Blitz */
  g_party[0].moves[1] = (menu_move){370, 0, 5};  /* Close Combat */
  g_party[0].moves[2] = (menu_move){14, 10, 20}; /* Swords Dance */
  g_party[0].nmoves = 3;
  add_item(&g_party[0], "Leftovers", "leftovers", 2, 4);
  add_item(&g_party[0], "Quick Claw", "quick_claw", 1, 3);
  g_party[1] = make(445, "Garchomp", 55, 0, 190, MS_NONE, MG_FEMALE);
  g_party[2] = make(25, "Pikachu", 30, 20, 61, MS_PARALYSIS, MG_FEMALE);
  g_party[2].has_evolutions = true;
  g_party[2].tm_state = 1;
  add_item(&g_party[2], "Leftovers", "leftovers", 4, 4);
  g_party[3] = make(94, "Gengar", 51, 45, 140, MS_POISON, MG_MALE);
  g_party[3].fusion = true;
  g_party[3].fusion_species = 6;
  g_party[3].tm_state = 2;
  snprintf(g_party[3].name, sizeof g_party[3].name, "Genzard");
  g_party[4] = make(149, "Dragonite", 60, 200, 201, MS_BURN, MG_MALE);
  g_party[5] = make(129, "Magikarp the Mighty", 100, 11, 250, MS_NONE, MG_FEMALE);
  g_party[5].has_evolutions = true;
  g_party[5].relearn[0] = 150; /* Splash */
  g_party[5].relearn_level[0] = 1;
  g_party[5].relearn[1] = 33; /* Tackle */
  g_party[5].relearn_level[1] = 15;
  g_party[5].relearn[2] = 175; /* Flail */
  g_party[5].relearn_level[2] = 30;
  g_party[5].nrelearn = 3;
}

/* ------------------------------------------------------------ events */

static const char* g_mode_names[] = {"SWITCH", "FAINT_SWITCH", "POST_BATTLE_SWITCH", "REVIVAL_BLESSING", "MODIFIER",
                                     "MOVE_MODIFIER", "TM_MODIFIER", "REMEMBER_MOVE_MODIFIER", "MODIFIER_TRANSFER",
                                     "SPLICE", "RELEASE", "CHECK", "SELECT", "DISCARD"};
static party_mode g_mode;
static bool g_double;

static const char* filter_non_fainted(void* ctx, int slot) {
  (void)ctx;
  static char buf[160];
  if (g_party[slot].hp == 0)
    return mui_tf(buf, sizeof buf, "partyUiHandler:noEnergy", "pokemonName", g_party[slot].name, NULL);
  return NULL;
}

static void on_select(void* ctx, int slot, int option) {
  (void)ctx;
  logf_("select slot=%d option=%d", slot, option);
  if (option == PO_APPLY) party_show_text(mui_t("partyUiHandler:anyEffect"), true);
}
static void on_transfer(void* ctx, int from, int item, int count, int to) {
  (void)ctx;
  logf_("transfer from=%d item=%d count=%d to=%d", from, item, count, to);
}
static void on_discard(void* ctx, int slot, int item, int count) {
  (void)ctx;
  logf_("discard slot=%d item=%d count=%d", slot, item, count);
}
static void on_release(void* ctx, int slot) {
  (void)ctx;
  logf_("release slot=%d", slot);
  memmove(&g_party[slot], &g_party[slot + 1], sizeof(menu_mon) * (g_n - slot - 1));
  g_n--;
  party_refresh(g_party, g_n);
}
static void on_pause(void* ctx, int slot, bool paused) {
  (void)ctx;
  logf_("pause slot=%d paused=%d", slot, paused);
}
static void on_summary(void* ctx, int slot) {
  (void)ctx;
  logf_("summary slot=%d", slot);
  party_resume(); /* no summary screen yet */
}

static void open_party(void) {
  party_args a;
  memset(&a, 0, sizeof a);
  a.mode = g_mode;
  a.battler_count = g_double ? 2 : 1;
  a.double_battle = g_double;
  a.max_level = 100;
  a.show_move_pp = true;
  a.in_reward_phase = true;
  if (g_mode == PARTY_SWITCH || g_mode == PARTY_FAINT_SWITCH || g_mode == PARTY_POST_BATTLE_SWITCH)
    a.filter = filter_non_fainted;
  party_events ev = {on_select, on_transfer, on_discard, on_release, on_pause, on_summary, NULL};
  party_open(&a, g_party, g_n, &ev);
  logf_("open mode=%s double=%d", g_mode_names[g_mode], g_double);
}

int main(void) {
  osSetSpeedupEnable(true);
  if (!rr_init()) return 1;
  int paks = pak_mount_default();
  snd_init();
  mui_init();
  g_log = fopen("sdmc:/3ds/pokerogue/menutest.log", "w");
  logf_("menutest start, %d paks", paks);

  /* optional start mode: sdmc:/3ds/pokerogue/menutest.txt holds "<mode> <double>" */
  FILE* cfg = fopen("sdmc:/3ds/pokerogue/menutest.txt", "r");
  if (cfg) {
    int m = 0, d = 0;
    if (fscanf(cfg, "%d %d", &m, &d) >= 1 && m >= 0 && m <= PARTY_DISCARD) g_mode = (party_mode)m;
    g_double = d != 0;
    fclose(cfg);
  }
  party_reset();
  open_party();

  rr_set_origin(RR_TOP, 40, 30);
  rr_set_origin(RR_BOTTOM, 0, 0);
  u64 last = osGetTime();
  while (aptMainLoop()) {
    hidScanInput();
    u32 k = hidKeysDown();
    if (k & KEY_START) break;
    if (k & (KEY_L | KEY_R)) {
      g_mode = (party_mode)((g_mode + ((k & KEY_R) ? 1 : PARTY_DISCARD)) % (PARTY_DISCARD + 1));
      party_reset();
      open_party();
    }
    if (k & KEY_SELECT) {
      g_double = !g_double;
      party_reset();
      open_party();
    }
    static const struct { u32 key; mui_button b; } map[] = {
        {KEY_DUP, MB_UP}, {KEY_DDOWN, MB_DOWN}, {KEY_DLEFT, MB_LEFT}, {KEY_DRIGHT, MB_RIGHT},
        {KEY_A, MB_ACTION}, {KEY_B, MB_CANCEL}, {KEY_X, MB_MENU}, {KEY_Y, MB_STATS},
    };
    for (size_t i = 0; i < sizeof map / sizeof map[0]; i++)
      if (k & map[i].key) party_input(map[i].b);

    u64 now = osGetTime();
    float dt = (float)(now - last);
    if (dt > 100) dt = 100;
    last = now;
    /* fixed step keeps emulator runs deterministic */
    dt = 1000.0f / 60.0f;
    mui_tick(dt);
    party_update(dt);

    rr_frame_begin();
    rr_target(RR_TOP, RR_RGBA(0, 0, 0, 255));
    party_draw();
    rr_target(RR_BOTTOM, RR_RGBA(0x18, 0x18, 0x20, 255));
    char info[512];
    snprintf(info, sizeof info,
             "menutest: party, mode %s%s\n\nlast events:\n%s\n%s\n%s\n%s\n\nL/R: mode  SELECT: doubles  START: quit",
             g_mode_names[g_mode], g_double ? " (double)" : "", g_last[0], g_last[1], g_last[2], g_last[3]);
    rr_text_sys(4, 4, 0.45f, info, RR_RGBA(0xe0, 0xe0, 0xe0, 255));
    rr_frame_end();
  }
  if (g_log) fclose(g_log);
  snd_exit();
  pak_unmount_all();
  rr_exit();
  return 0;
}
