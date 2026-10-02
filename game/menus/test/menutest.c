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

/* summary data for a member (made up, plausible) */
static void summary_fill(menu_mon* m, int t1, int t2, int ability, int passive, int nature) {
  m->types[0] = t1;
  m->types[1] = t2;
  m->ability = ability;
  m->passive = passive;
  m->has_passive = passive != 0;
  m->nature = nature;
  for (int i = 0; i < 6; i++) {
    m->stats[i] = 40 + m->level * (i + 1) / 3;
    m->ivs[i] = (uint8_t)((m->species * 7 + i * 11) % 32);
  }
  m->ivs[1] = 31;
  m->exp = m->level * m->level * m->level;
  m->next_lv_exp = (m->level + 1) * (m->level + 1) * (m->level + 1) - m->exp;
  m->exp_ratio = 0.4f;
  m->pokeball = m->species % 6;
  m->friendship = (uint8_t)(m->species * 3 % 256);
  m->met_biome = (int16_t)(m->species % 30);
  m->met_level = 5;
  m->met_wave = (uint16_t)(m->species % 50);
  m->candy_count = m->species % 120;
  m->starter_friendship = 30;
  m->friendship_cap = 75;
  m->candy_colors[0] = 0xf8a038;
  m->candy_colors[1] = 0xf8f8a0;
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
  /* types: normal 0 fighting 1 flying 2 poison 3 ground 4 ghost 7 fire 9 water 10 electric 12 dragon 15 */
  summary_fill(&g_party[0], 9, 1, 66, 3, 3);    /* Blaze, Speed Boost; Adamant */
  g_party[0].luck = 2;
  g_party[0].champion = true;
  g_party[0].pokerus = true;
  summary_fill(&g_party[1], 15, 4, 8, 24, 13);  /* Sand Veil, Rough Skin; Jolly */
  summary_fill(&g_party[2], 12, -1, 9, 0, 10);  /* Static; Timid */
  g_party[2].met_biome = -1;
  summary_fill(&g_party[3], 7, 3, 26, 0, 15);   /* Levitate; Modest */
  summary_fill(&g_party[4], 15, 2, 39, 0, 0);   /* Inner Focus; Hardy */
  summary_fill(&g_party[5], 10, -1, 33, 0, 24); /* Swift Swim; Quirky */
  g_party[5].moves[0] = (menu_move){150, 0, 40}; /* Splash */
  g_party[5].nmoves = 1;
}

/* ------------------------------------------------------------ events */

static const char* g_mode_names[] = {"SWITCH", "FAINT_SWITCH", "POST_BATTLE_SWITCH", "REVIVAL_BLESSING", "MODIFIER",
                                     "MOVE_MODIFIER", "TM_MODIFIER", "REMEMBER_MOVE_MODIFIER", "MODIFIER_TRANSFER",
                                     "SPLICE", "RELEASE", "CHECK", "SELECT", "DISCARD"};
static party_mode g_mode;
static int g_start_summary;
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
static void sum_close(void* ctx) {
  (void)ctx;
  logf_("summary closed");
  if (party_active()) party_resume();
}
static void sum_swap(void* ctx, int slot, int a, int b) {
  (void)ctx;
  logf_("swap slot=%d moves %d<->%d", slot, a, b);
  menu_move t = g_party[slot].moves[a];
  g_party[slot].moves[a] = g_party[slot].moves[b];
  g_party[slot].moves[b] = t;
}
static void sum_learn(void* ctx, int cursor) {
  (void)ctx;
  logf_("learn move cursor=%d", cursor);
  summary_close();
}

static void open_summary(int slot, summary_mode mode) {
  summary_args a;
  memset(&a, 0, sizeof a);
  a.mode = mode;
  a.page = SUMMARY_PROFILE;
  a.party = g_party;
  a.n = g_n;
  a.index = slot;
  a.from_party = mode == SUMMARY_DEFAULT;
  a.new_move = 436; /* Lava Plume */
  a.new_move_type = -1;
  a.trainer_name = "Lorenzo";
  a.trainer_id = "54321";
  summary_events ev = {sum_close, sum_swap, sum_learn, NULL};
  summary_open(&a, &ev);
}

static void on_summary(void* ctx, int slot) {
  (void)ctx;
  logf_("summary slot=%d", slot);
  open_summary(slot, SUMMARY_DEFAULT);
}

/* ---- a stand-in message box and yes/no box for the learn-move dialog (the
 * game's are the battle screen's) ---- */
static mui_msg g_msg;
static bool g_msg_on;
static mui_confirm g_confirm;
static char g_pages[640];
static char* g_page;
static bool g_final_prompt;
static menu_cb g_done;
static void* g_done_ctx;

static void next_page(void* ctx);
static void show_page(void) {
  char* end = strchr(g_page, '$');
  if (end) *end = 0;
  char* next = end ? end + 1 : NULL;
  const char* cur = g_page;
  g_page = next;
  g_msg_on = true;
  mui_msg_show(&g_msg, cur, 20, next_page, NULL, next ? true : g_final_prompt);
}
static void next_page(void* ctx) {
  (void)ctx;
  if (g_page) {
    show_page();
    return;
  }
  if (g_done) g_done(g_done_ctx);
}
static void host_text(void* host, const char* text, bool prompt, menu_cb done, void* done_ctx) {
  (void)host;
  /* drop upstream's @d{} @s{} @c{} @f{} tags (this stand-in has no timing or sound for them) */
  size_t o = 0;
  for (const char* p = text; *p && o + 1 < sizeof g_pages; p++) {
    if (p[0] == '@' && p[1] && strchr("cdsf", p[1]) && p[2] == '{') {
      const char* e = strchr(p, '}');
      if (e) {
        p = e;
        continue;
      }
    }
    g_pages[o++] = *p;
  }
  g_pages[o] = 0;
  g_page = g_pages;
  g_final_prompt = prompt;
  g_done = done;
  g_done_ctx = done_ctx;
  logf_("text: %.60s", text);
  show_page();
}
static void host_confirm(void* host, menu_cb yes, menu_cb no, void* ctx) {
  (void)host;
  logf_("confirm");
  mui_confirm_show(&g_confirm, yes, no, ctx);
}
static void host_result(void* host, int slot) {
  (void)host;
  logf_("learn result slot=%d", slot);
  if (slot >= 0) {
    menu_mon* m = &g_party[4];
    m->moves[slot] = (menu_move){436, 0, 15};
    if (slot >= m->nmoves) m->nmoves = slot + 1;
  }
}
static void host_end(void* host) {
  (void)host;
  logf_("learn move end");
  g_msg_on = false;
}
static void start_learn_move(void) {
  menu_mon* m = &g_party[4];
  m->moves[0] = (menu_move){337, 0, 15}; /* Dragon Claw */
  m->moves[1] = (menu_move){245, 1, 5};  /* Extreme Speed */
  m->moves[2] = (menu_move){7, 0, 15};   /* Fire Punch */
  m->moves[3] = (menu_move){9, 2, 15};   /* Thunder Punch */
  m->nmoves = 4;
  learn_move_args a = {g_party, g_n, 4, 436, m->name, true, "Lorenzo", "54321", false};
  learn_move_host h = {host_text, host_confirm, host_result, host_end, NULL};
  learn_move_start(&a, &h);
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

  static void on_biome_select(void* ctx, int option) {
    (void)ctx;
    logf_("biome select option=%d", option);
    biome_map_close();
  }

  /* optional start mode: sdmc:/3ds/pokerogue/menutest.txt holds "<mode> <double>" */
  FILE* cfg = fopen("sdmc:/3ds/pokerogue/menutest.txt", "r");
  if (cfg) {
    int m = 0, d = 0;
    if (fscanf(cfg, "%d %d", &m, &d) >= 1 && m >= 0 && m <= PARTY_DISCARD) g_mode = (party_mode)m;
    if (m >= 14 && m <= 17) g_start_summary = m;
    g_double = d != 0;
    fclose(cfg);
  }
  party_reset();
  open_party();
  /* 14: summary, 15: summary learn move, 16: learn move dialog, 17: biome map UI */
  if (g_start_summary == 17) {
    static const uint8_t test_biomes[3] = {2, 5, 13}; /* GRASS, FOREST, CAVE */
    biome_map_args ba = {test_biomes, 3};
    biome_map_events be = {on_biome_select, NULL};
    biome_map_open(&ba, &be);
  } else if (g_start_summary == 16) {
    start_learn_move();
  } else if (g_start_summary) {
    open_summary(g_start_summary == 15 ? 5 : 0, g_start_summary == 15 ? SUMMARY_LEARN_MOVE : SUMMARY_DEFAULT);
  }

  mui_set_origin(RR_TOP, 40, 30);
  mui_set_origin(RR_BOTTOM, 0, 0);
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
      if (k & map[i].key) {
        if (g_confirm.active)
          mui_confirm_input(&g_confirm, map[i].b);
        else if (summary_active())
          summary_input(map[i].b);
        else if (g_msg_on)
          mui_msg_input(&g_msg, map[i].b);
        else if (biome_map_active())
          biome_map_input(map[i].b);
        else
          party_input(map[i].b);
      }

    u64 now = osGetTime();
    float dt = (float)(now - last);
    if (dt > 100) dt = 100;
    last = now;
    /* fixed step keeps emulator runs deterministic */
    dt = 1000.0f / 60.0f;
    mui_tick(dt);
    party_update(dt);
    summary_update(dt);
    if (g_msg_on) mui_msg_update(&g_msg, dt);
    biome_map_update(dt);

    rr_frame_begin();
    rr_target(RR_TOP, RR_RGBA(0, 0, 0, 255));
    mui_set_origin(RR_TOP, 40, 30);
    if (summary_active())
      summary_draw();
    else
      party_draw();
    if (g_msg_on && !summary_active()) {
      mui_window(0, 132, 320, 48);
      mui_msg_draw(&g_msg, TS_MESSAGE, 12, 138);
    }
    if (biome_map_active()) biome_map_draw();
    if (g_confirm.active && !summary_active()) mui_confirm_draw(&g_confirm);
    rr_target(RR_BOTTOM, RR_RGBA(0x18, 0x18, 0x20, 255));
    mui_set_origin(RR_BOTTOM, 0, 0);
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
