/* The starter select screen (see starter.h). Positions are upstream's, on its 320 x 180 canvas:
 *   starter-select-ui-handler.ts setup / setupPartyColumn / updateScroll, starter-summary.ts, scroll-bar.ts,
 *   base-option-select-ui-handler.ts (the options menu) and confirm-ui-handler.ts (the Yes / No box). */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "menus.h"
#include "pr_data.h"

extern const char* const menu_type_frames[];
extern const char* const menu_nature_keys[];
const pr_species* menu_species(uint16_t id);

/* the grid is a container at (speciesContainerX + 6, 9); cell i sits 18 px apart in x, 17 in y, from y 13 */
#define BOX_X 115
#define BOX_Y 9
#define TEAM_X 285 /* teamWindowX */
#define SPRITE_DELAY_MS 120.0f

enum { ASK_NONE, ASK_START, ASK_EXIT };

static struct {
  bool active;
  starter_args a;
  starter_events ev;
  sm_model m;

  starter_info info;      /* the panel: valid while info_species != 0 */
  uint16_t info_species;
  menu_sprite sprite;
  bool sprite_tried;
  float sprite_wait;      /* ms until the sprite is loaded: the cursor has to rest first (the SD card is slow) */
  float now;              /* ms since open */
  float sprite_t0;

  mui_msg msg;
  bool msg_on;
  mui_confirm conf;
  int ask;                /* ASK_* : the question in the box is waiting for the Yes / No box */

  struct {
    bool open;
    int cursor, n;
    int act[2];           /* 0 add, 1 remove, per row (the last row is Cancel) */
    uint16_t species;
  } menu;
  bool done;              /* the team was confirmed: the screen takes no more buttons until the caller replaces it */
} S;

static menu_mon g_icon_mon; /* scratch for the grid icons */

/* ------------------------------------------------------------ helpers */

/* mui_t answers the key itself when a string is missing, so the key must outlive the call: a static buffer (each result is drawn
 * before the next lookup) */
static const char* ability_name(uint16_t id) {
  static char key[96];
  if (id >= PR_ABILITY_COUNT) return "";
  snprintf(key, sizeof key, "ability:%s.name", pr_abilities[id].key);
  return mui_t(key);
}

static const char* nature_name(int nature) {
  static char key[48];
  snprintf(key, sizeof key, "nature:%s", nature >= 0 && nature < PR_NATURE_COUNT ? menu_nature_keys[nature] : "hardy");
  return mui_t(key);
}

static const char* type_frame(int t) { return t >= 0 && t < PR_TYPE_COUNT + 1 ? menu_type_frames[t] : NULL; }

static rr_color with_alpha(rr_color c, int a) { return (c & ~0xFFu) | (uint32_t)a; }

static void entry_mon(const sm_entry* e, menu_mon* m) {
  m->species = e->species;
  m->form = e->form;
  m->gender = e->female ? MG_FEMALE : MG_MALE;
  m->shiny = e->shiny;
  m->variant = e->variant;
}

static void hide_message(void) {
  S.msg_on = false;
  S.msg.text[0] = 0;
  S.msg.shown = 0;
}

static void drop_sprite(void) {
  menu_sprite_free(&S.sprite);
  S.sprite_tried = false;
  S.sprite_wait = SPRITE_DELAY_MS;
}

/* the panel follows the cursor (setStarter / setPartyStarter / setNoStarter) */
static void follow_cursor(void) {
  const uint16_t sp = sm_species(&S.m);
  if (sp == S.info_species) return;
  S.info_species = sp;
  drop_sprite();
  memset(&S.info, 0, sizeof S.info);
  if (sp && S.ev.describe) S.ev.describe(S.ev.ctx, sp, &S.info);
  if (!S.info.mon.species) S.info.mon.species = sp;
}

/* ------------------------------------------------------------ lifecycle */

static int default_rnd(void* ctx, int n) {
  (void)ctx;
  static uint32_t x = 0x2545F491u;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  return n > 0 ? (int)(x % (uint32_t)n) : 0;
}

void starter_open(const starter_args* args, const starter_events* ev) {
  menu_sprite_free(&S.sprite);
  memset(&S, 0, sizeof S);
  S.a = *args;
  S.ev = *ev;
  S.active = true;
  sm_init(&S.m, args->entries, args->n, args->budget, ev->rnd ? ev->rnd : default_rnd, ev->ctx);
  S.sprite_wait = SPRITE_DELAY_MS;
  follow_cursor();
}

void starter_close(void) {
  if (!S.active) return;
  menu_sprite_free(&S.sprite);
  S.active = false;
}

bool starter_active(void) { return S.active; }

/* ------------------------------------------------------------ input */

static void play(sm_result r) {
  if (r == SM_OK) mui_sfx("ui/select");
  else if (r == SM_ERROR) mui_sfx("ui/error");
}

static void answer_start(void* ctx) {
  (void)ctx;
  S.ask = ASK_NONE;
  hide_message();
  uint16_t team[SM_TEAM_MAX];
  memcpy(team, S.m.party, sizeof team);
  /* startRun: the caller takes the team (and may refuse it, with a message the box shows) */
  S.done = !S.ev.on_start || S.ev.on_start(S.ev.ctx, team, S.m.n_party);
}

static void answer_exit(void* ctx) {
  (void)ctx;
  S.ask = ASK_NONE;
  hide_message();
  if (S.ev.on_exit) S.ev.on_exit(S.ev.ctx);
}

static void answer_no(void* ctx) {
  (void)ctx;
  S.ask = ASK_NONE;
  hide_message(); /* cancelStartRun / cancelExit: clearText */
}

/* ui.showText(question, null, () => ui.setModeWithoutClear(CONFIRM)) */
static void question_typed(void* ctx) {
  (void)ctx;
  mui_confirm_show(&S.conf, S.ask == ASK_START ? answer_start : answer_exit, answer_no, NULL);
  S.conf.y = MUI_H - 29; /* ConfirmModeConfig yOffset 29 */
}

static void ask(int what, const char* key) {
  S.ask = what;
  S.msg_on = true;
  mui_msg_show(&S.msg, mui_t(key), 20, question_typed, NULL, false);
}

/* The caller's text, wrapped to the box (the message box does not wrap by itself) */
void starter_message(const char* text) {
  char wrapped[sizeof S.msg.text];
  mui_wrap(TS_WINDOW, text, MUI_W - 2 - 2 * 8, wrapped, sizeof wrapped);
  S.msg_on = true;
  mui_msg_show(&S.msg, wrapped, 20, NULL, NULL, true);
}

static void open_menu(uint16_t species) {
  S.menu.open = true;
  S.menu.cursor = 0;
  S.menu.species = species;
  S.menu.n = 0;
  if (sm_can_add(&S.m, species))
    S.menu.act[S.menu.n++] = 0;
  else if (sm_in_party(&S.m, species, NULL))
    S.menu.act[S.menu.n++] = 1;
  S.menu.act[S.menu.n++] = -1; /* Cancel */
}

static const char* menu_label(int row) {
  switch (S.menu.act[row]) {
    case 0: return mui_t("starterSelectUiHandler:addToParty");
    case 1: return mui_t("starterSelectUiHandler:removeFromParty");
    default: return mui_t("menu:cancel");
  }
}

static void menu_choose(int row) {
  const uint16_t sp = S.menu.species;
  S.menu.open = false;
  if (S.menu.act[row] == 0) {
    if (sm_add(&S.m, sp)) {
      char cry[48];
      menu_cry_key(&S.info.mon, cry, sizeof cry);
      mui_sfx(cry); /* addToParty: the cry */
      mui_sfx("ui/select");
    } else {
      mui_sfx("ui/error");
    }
  } else if (S.menu.act[row] == 1) {
    int idx;
    if (sm_in_party(&S.m, sp, &idx)) {
      sm_remove(&S.m, idx);
      mui_sfx("ui/select");
    }
  }
  follow_cursor();
}

static bool menu_input(mui_button b) {
  switch (b) {
    case MB_UP:
    case MB_DOWN:
      S.menu.cursor = (S.menu.cursor + (b == MB_DOWN ? 1 : S.menu.n - 1)) % S.menu.n;
      mui_sfx("ui/select");
      return true;
    case MB_ACTION:
      menu_choose(S.menu.cursor);
      return true;
    case MB_CANCEL: /* "cancelling moves the cursor to the last option, as if it was selected" */
      menu_choose(S.menu.n - 1);
      return true;
    default:
      return true;
  }
}

bool starter_input(mui_button b) {
  if (!S.active || S.done) return S.active;
  if (S.conf.active) return mui_confirm_input(&S.conf, b);
  if (S.msg_on) {
    if (S.ask) return true; /* the question is still being typed */
    if (mui_msg_input(&S.msg, b)) {
      /* A or B closed the message that was waiting for it */
      if (!mui_msg_busy(&S.msg)) hide_message();
    }
    return true;
  }
  if (S.menu.open) return menu_input(b);

  const sm_result r = sm_input(&S.m, b);
  switch (r) {
    case SM_MENU:
      mui_sfx("ui/select");
      open_menu(sm_species(&S.m));
      break;
    case SM_ASK_START:
      mui_sfx("ui/select");
      ask(ASK_START, "starterSelectUiHandler:confirmStartTeam");
      break;
    case SM_ASK_EXIT:
      mui_sfx("ui/select");
      ask(ASK_EXIT, "starterSelectUiHandler:confirmExit");
      break;
    case SM_OK:
      if (b == MB_ACTION && S.m.focus == SM_RANDOM) {
        char cry[48];
        follow_cursor();
        menu_cry_key(&S.info.mon, cry, sizeof cry);
        mui_sfx(cry);
      }
      play(r);
      break;
    default:
      play(r);
      break;
  }
  follow_cursor();
  return r != SM_IGNORED;
}

void starter_update(float dt) {
  if (!S.active) return;
  S.now += dt;
  mui_msg_update(&S.msg, dt);
  if (S.info_species && !S.sprite.atlas && !S.sprite_tried) {
    S.sprite_wait -= dt;
    if (S.sprite_wait <= 0) {
      S.sprite_tried = true;
      menu_sprite_load(&S.sprite, &S.info.mon, false);
      S.sprite_t0 = S.now;
    }
  }
}

/* ------------------------------------------------------------ drawing */

static void draw_nine(const char* key, float x, float y, float w, float h, int border) {
  const rr_frame* f = mui_frame(key, NULL);
  if (f) rr_nineslice(f, floorf(x), floorf(y), floorf(w), floorf(h), border);
}

/* addWindow(x, y, w, h, mergeMaskTop = true): the mask hides the window's top 2 px where it meets the window above */
static void window_merged_top(float x, float y, float w, float h) {
  mui_clip(x, y + 2, w, h - 2);
  mui_window(x, y, w, h);
  mui_clip_off();
}

static void draw_icon(const sm_entry* e, float x, float y, rr_color tint) {
  entry_mon(e, &g_icon_mon);
  menu_draw_icon_tinted(&g_icon_mon, x, y, 0, 0, 0.5f, tint);
}

static void draw_cell(int i, const sm_entry* e) {
  const float x = BOX_X + (i % SM_COLS) * 18, y = BOX_Y + 13 + (i / SM_COLS) * 17;
  if (e->passive) {
    rr_draw_opts o = RR_DEFAULT_OPTS;
    o.scale_x = o.scale_y = 0.75f;
    mui_image_ex("ui/passive_bg", NULL, x + 2, y + 5, &o);
  }
  draw_icon(e, x - 2, y + 2, sm_lit(&S.m, e) ? RR_RGBA(255, 255, 255, 255) : RR_RGBA(255, 255, 255, 96)); /* alpha 1 / 0.375 */
  char t[16];
  sm_cost_text(t, sizeof t, e->cost);
  static const mui_style styles[3] = {TS_WINDOW, TS_SUMMARY_BLUE, TS_SUMMARY_GOLD};
  const mui_style st = styles[sm_cost_style(e)];
  mui_text_c(TSX_METRICS_32, x + 1, y + 2, t, mui_style_color(st, false), mui_style_color(st, true));
  if (e->hidden_ability) {
    rr_draw_opts o = RR_DEFAULT_OPTS;
    o.scale_x = o.scale_y = 0.5f;
    mui_image_ex("ui/ha_capsule", NULL, x + 12, y + 7, &o);
  }
  if (e->won) {
    rr_draw_opts o = RR_DEFAULT_OPTS;
    o.scale_x = o.scale_y = 0.5f;
    mui_image_ex("ui/champion_ribbon", NULL, x, y + 12, &o);
  }
}

static void draw_scroll_bar(void) {
  const int rows = (S.m.n + SM_COLS - 1) / SM_COLS;
  if (rows <= SM_ROWS) return;
  const float x = BOX_X + 161, y = BOX_Y + 12, w = 5, h = 155;
  draw_nine("ui/scroll_bar", x, y, w, h, 2);
  const float bottom_h = 2;
  const float body_h = (h - 1 - bottom_h) * SM_ROWS / rows;
  const float body_y = 1 + (h - 1 - bottom_h) / rows * S.m.scroll;
  rr_rect(x + 1, y + body_y, w - 2, body_h, RR_RGBA(0xaa, 0xaa, 0xaa, 255));
  draw_nine("ui/scroll_bar_handle", x + 1, y + body_y + body_h, w - 2, bottom_h, 2);
}

static void draw_team_column(void) {
  window_merged_top(TEAM_X, 38 - 20, 34, 20); /* the Randomize label's window */
  mui_window(TEAM_X, 38, 34, 107);
  window_merged_top(TEAM_X, 38 + 107, 34, 34); /* value limit and Start */

  /* tryUpdateValue: the team's value over the budget. The options menu offers Add to Party only when the cost fits, so the
   * pink flash of a refused add (SUMMARY_PINK for half a second) is never reached here and is left out. */
  char t[32];
  sm_value_text(t, sizeof t, sm_value(&S.m), S.m.budget);
  mui_text_o(TS_STARTER_VALUE_LIMIT, TEAM_X + 17, 150, 0.5f, 0, t);
  mui_text_o(TS_TOOLTIP_CONTENT, TEAM_X + 17, 162, 0.5f, 0, mui_t("common:start"));
  mui_text_o(TS_TOOLTIP_CONTENT, TEAM_X + 17, 23, 0.5f, 0, mui_t("starterSelectUiHandler:randomize"));

  if (S.m.focus == SM_START) draw_nine("ui/select_cursor", TEAM_X + 4, 160, 26, 15, 6);
  if (S.m.focus == SM_RANDOM) draw_nine("ui/select_cursor", TEAM_X + 4, 21, 26, 15, 6);

  for (int i = 0; i < SM_TEAM_MAX; i++) {
    const float y = (float)sm_party_icon_y(i);
    const sm_entry* e = i < S.m.n_party ? sm_find(&S.m, S.m.party[i]) : NULL;
    if (e) {
      draw_icon(e, TEAM_X + 7, y, RR_RGBA(255, 255, 255, 255));
    } else {
      rr_draw_opts o = RR_DEFAULT_OPTS;
      o.scale_x = o.scale_y = 0.5f;
      mui_image_ex("pokemon_icons_0", "unknown", TEAM_X + 7, y, &o);
    }
  }
  if (S.m.focus == SM_PARTY && S.m.party_cursor < S.m.n_party)
    mui_image("ui/select_gen_cursor", NULL, 289, (float)sm_party_icon_y(S.m.party_cursor) + 1, 0, 0); /* STARTER_ICONS_CURSOR offsets (-3, 1) */
}

static void draw_grid(void) {
  for (int i = 0; i < SM_PAGE; i++) {
    const sm_entry* e = sm_cell(&S.m, i);
    if (!e) break;
    draw_cell(i, e);
  }
  /* the cursor-highlight of each species in the team */
  for (int i = 0; i < SM_PAGE; i++) {
    const sm_entry* e = sm_cell(&S.m, i);
    if (!e) break;
    if (sm_in_party(&S.m, e->species, NULL))
      mui_image("ui/select_cursor_highlight", NULL, BOX_X + (i % SM_COLS) * 18 - 1, BOX_Y + 13 + (i / SM_COLS) * 17 + 1, 0, 0);
  }
  if (S.m.focus == SM_GRID)
    mui_image("ui/select_cursor", NULL, BOX_X + (S.m.cursor % SM_COLS) * 18 - 1, BOX_Y + 13 + (S.m.cursor / SM_COLS) * 17 + 1, 0, 0);
  draw_scroll_bar();
}

/* the text of the panel at 56px (starterInfoTextSize) in a style's colours */
static void info_text(float x, float y, const char* t, mui_style colours, int alpha, int* width) {
  rr_color c = mui_style_color(colours, false), s = mui_style_color(colours, true);
  if (alpha < 255) {
    c = with_alpha(c, alpha);
    s = with_alpha(s, alpha);
  }
  const int w = mui_text_c(TS_MOVE_INFO_CONTENT, x, y, t, c, s);
  if (width) *width = w;
}

static void draw_panel(void) {
  const menu_mon* m = &S.info.mon;
  /* No (the label) and the dex number */
  mui_image("ui/text_images/en/summary/summary_dexnb_label", NULL, 6, 14, 0, 1);
  char num[8];
  if (S.info_species) snprintf(num, sizeof num, "%04u", S.info_species);
  else snprintf(num, sizeof num, "0000");
  {
    const bool shiny = m->shiny;
    const mui_style ns = shiny ? TS_SUMMARY_DEX_NUM_GOLD : TS_SUMMARY_DEX_NUM;
    const int w = mui_text_width(TS_SUMMARY_DEX_NUM, num);
    mui_text_c(TS_SUMMARY_DEX_NUM, 41 - w, 1, num, mui_style_color(ns, false), mui_style_color(ns, true));
    if (shiny) mui_image("ui/text_images/en/summary/summary_dexnb_label_overlay_shiny", NULL, 6, 111, 0, 1);
  }
  if (!S.info_species) return;

  /* the sprite, once the cursor has rested on the species */
  const rr_frame* sf = menu_sprite_frame(&S.sprite, (uint32_t)(S.now - S.sprite_t0));
  if (sf) {
    rr_draw_opts o = RR_DEFAULT_OPTS;
    o.origin_x = o.origin_y = 0.5f;
    rr_draw(sf, 53, 63, &o);
  }

  /* types (8, 98) and (26, 98) at 0.5 */
  for (int i = 0; i < 2; i++) {
    const char* tf = m->types[i] >= 0 ? type_frame(m->types[i]) : NULL;
    if (!tf) continue;
    rr_draw_opts o = RR_DEFAULT_OPTS;
    o.scale_x = o.scale_y = 0.5f;
    mui_image_ex("types", tf, 8 + 18 * i, 98, &o);
  }
  /* growth rate (36px) */
  {
    static const char* const keys[6] = {"erratic", "fast", "mediumFast", "mediumSlow", "slow", "fluctuating"};
    static const uint32_t fg[6] = {0xf85888, 0xf8d030, 0x78c850, 0x6890f0, 0xf08030, 0xa040a0};
    static const uint32_t sh[6] = {0x906060, 0xb8a038, 0x588040, 0x807870, 0xc03028, 0x483850};
    const pr_species* sp = menu_species(S.info_species);
    if (sp && sp->growth_rate < 6) {
      char key[48];
      snprintf(key, sizeof key, "growth:%s", keys[sp->growth_rate]);
      mui_text_c(TSX_METRICS_36, 8, 106, mui_t("starterSelectUiHandler:growthRate"), mui_style_color(TS_WINDOW_ALT, false),
                 mui_style_color(TS_WINDOW_ALT, true));
      mui_text_c(TSX_METRICS_36, 34, 106, mui_t(key), MUI_RGB(fg[sp->growth_rate]), MUI_RGB(sh[sp->growth_rate]));
    }
  }
  /* name and gender */
  {
    char name[48];
    snprintf(name, sizeof name, "%.40s", m->name);
    if (strlen(name) > 15) { /* truncateString(name, 15) */
      name[12] = 0;
      strcat(name, "...");
    }
    mui_text(TS_SUMMARY, 6, 112, name);
    const pr_species* sp = menu_species(S.info_species);
    if (sp && sp->male_permille >= 0) {
      const bool female = m->gender == MG_FEMALE;
      mui_text_c(TS_SUMMARY_ALT, 96, 112, female ? "\xe2\x99\x80" : "\xe2\x99\x82", MUI_RGB(female ? 0xf89890 : 0x40c8f8),
                 MUI_RGB(female ? 0x984038 : 0x006090));
    }
  }
  /* ability, passive, nature */
  {
    int w;
    info_text(6, 127, mui_t("starterSelectUiHandler:ability"), TS_SUMMARY_ALT, 255, NULL);
    info_text(31, 127, ability_name(m->ability), S.info.ability_hidden ? TS_SUMMARY_GOLD : TS_SUMMARY_ALT, 255, NULL);
    if (m->has_passive) {
      const bool lit = S.info.passive_unlocked && S.info.passive_enabled;
      info_text(6, 136, mui_t("starterSelectUiHandler:passive"), TS_SUMMARY_ALT, 255, NULL);
      info_text(31, 136, ability_name(m->passive), lit ? TS_SUMMARY_ALT : TS_SUMMARY_GRAY, lit ? 255 : 128, &w);
      if (!S.info.passive_unlocked || !S.info.passive_enabled) {
        rr_draw_opts o = RR_DEFAULT_OPTS;
        o.origin_y = 0.5f;
        o.scale_x = S.info.passive_unlocked ? 0.35f : 0.42f;
        o.scale_y = S.info.passive_unlocked ? 0.35f : 0.38f;
        mui_image_ex(S.info.passive_unlocked ? "ui/icon_stop" : "ui/icon_lock", NULL, 31 + w + 1, 137, &o);
      }
    }
    info_text(6, 145, mui_t("starterSelectUiHandler:nature"), TS_SUMMARY_ALT, 255, NULL);
    int nw;
    info_text(31, 145, nature_name(m->nature), TS_SUMMARY_ALT, 255, &nw);
    /* getNatureName(nature, true, true): " (+Atk/-SpAtk)" with the stats in the pink and blue of the summary */
    int up = -1, down = -1;
    for (int s = 1; s < 6 && m->nature < PR_NATURE_COUNT; s++) {
      if (pr_nature_mult[m->nature][s] > 1.0) up = s;
      else if (pr_nature_mult[m->nature][s] < 1.0) down = s;
    }
    static const char* const short_keys[6] = {"pokemonInfo:stat.hpShortened", "pokemonInfo:stat.atkShortened",
                                              "pokemonInfo:stat.defShortened", "pokemonInfo:stat.spatkShortened",
                                              "pokemonInfo:stat.spdefShortened", "pokemonInfo:stat.spdShortened"};
    float x = 31 + nw;
    int w2;
    if (up >= 0 && down >= 0) {
      char t[48];
      info_text(x, 145, " (", TS_SUMMARY_ALT, 255, &w2);
      x += w2;
      snprintf(t, sizeof t, "+%s", mui_t(short_keys[up]));
      info_text(x, 145, t, TS_SUMMARY_PINK, 255, &w2);
      x += w2;
      info_text(x, 145, "/", TS_SUMMARY_ALT, 255, &w2);
      x += w2;
      snprintf(t, sizeof t, "-%s", mui_t(short_keys[down]));
      info_text(x, 145, t, TS_SUMMARY_BLUE, 255, &w2);
      x += w2;
      info_text(x, 145, ")", TS_SUMMARY_ALT, 255, NULL);
    } else {
      info_text(x, 145, " (-)", TS_SUMMARY_ALT, 255, NULL);
    }
  }
}

/* the text box at the bottom: a window 28 px high (42 for two lines, 14 more for each one after), the text 8 px in */
static void draw_message(void) {
  if (!S.msg_on) return;
  const int lines = mui_msg_lines(&S.msg);
  const float h = 14.0f * (float)lines + 14;
  mui_window(1, MUI_H - h, 318, h);
  mui_msg_draw(&S.msg, TS_WINDOW, 8, MUI_H - h + (lines > 1 ? 5 : 6));
}

static void draw_options_menu(void) {
  if (!S.menu.open) return;
  int w = 0;
  for (int i = 0; i < S.menu.n; i++) {
    const int tw = mui_text_width(TS_WINDOW, menu_label(i));
    if (tw > w) w = tw;
  }
  const float bw = w + 23, bh = (S.menu.n + 1) * 16 - 2;
  const float left = MUI_W - 1 - bw, top = MUI_H - bh;
  mui_window(left, top, bw, bh);
  for (int i = 0; i < S.menu.n; i++) mui_text(TS_WINDOW, left + 15, top + 7 + 16 * i, menu_label(i));
  mui_image("ui/cursor", NULL, left + 10, top + 15 + S.menu.cursor * 16, 0.5f, 0.5f);
}

void starter_draw(void) {
  if (!S.active) return;
  rr_rect(0, 0, MUI_W, MUI_H, RR_RGBA(0x00, 0x68, 0x60, 255)); /* bgColor 0x006860 */
  mui_image("ui/starter_select_bg", NULL, 0, 0, 0, 0);
  mui_image("ui/starter_container_bg", NULL, 110, 19, 0, 0);
  draw_team_column();
  draw_grid();
  draw_panel();
  draw_message();
  mui_confirm_draw(&S.conf);
  draw_options_menu();
}

/* The top screen: the species large, and its moves (upstream's panel draws them small beside the sprite) */
void starter_draw_top(void) {
  if (!S.active) return;
  rr_rect(0, 0, MUI_W, MUI_H, RR_RGBA(0x00, 0x68, 0x60, 255));
  if (!S.info_species) return;
  const menu_mon* m = &S.info.mon;
  const rr_frame* sf = menu_sprite_frame(&S.sprite, (uint32_t)(S.now - S.sprite_t0));
  if (sf) {
    rr_draw_opts o = RR_DEFAULT_OPTS;
    o.origin_x = o.origin_y = 0.5f;
    o.scale_x = o.scale_y = 1.5f;
    rr_draw(sf, 80, 92, &o);
  }
  char name[48];
  snprintf(name, sizeof name, "%.40s", m->name);
  mui_text(TS_SUMMARY, 160, 16, name);
  for (int i = 0; i < 2; i++) {
    const char* tf = m->types[i] >= 0 ? type_frame(m->types[i]) : NULL;
    if (!tf) continue;
    rr_draw_opts o = RR_DEFAULT_OPTS;
    o.scale_x = o.scale_y = 1.0f;
    mui_image_ex("types", tf, 160 + 36 * i, 38, &o);
  }
  for (int i = 0; i < 4; i++) {
    const uint16_t mv = S.info.moves[i];
    if (!mv) continue;
    const float y = 70 + 18 * i;
    const char* tf = mv < PR_MOVE_COUNT ? type_frame(pr_moves[mv].type) : NULL;
    if (tf) mui_image("types", tf, 160, y, 0, 0);
    mui_text(TS_SUMMARY, 160 + 35, y, menu_move_name(mv));
  }
}
