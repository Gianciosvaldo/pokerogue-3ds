/* The Run History (see history_screen.h). */
#include "history_screen.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "gm_enum_names.h"
#include "i18n.h"
#include "menus.h"
#include "party_icon.h"
#include "pr_data.h"
#include "run_summary.h"
#include "run_text.h"
#include "screen_clip.h"
#include "ui.h"
#include "ui_text.h"
#include "ui_widgets.h"

#define VISIBLE 4       /* runs on the bottom screen's 240 rows (upstream shows 3 on its 180) */
#define PITCH 56.0f     /* a run's window plus the gap (RunEntryContainer: slotId * 56) */
#define ROW_W 304
#define ROW_H 52

#define YELLOW RR_RGBA(0xf8, 0xe8, 0x58, 255)
#define WHITE RR_RGBA(0xf8, 0xf8, 0xf8, 255)
#define GREY_SHADOW RR_RGBA(0x42, 0x42, 0x42, 255)

extern const char* const menu_nature_keys[];
extern const char* const menu_type_frames[];

static struct {
  hist_args a;
  int n;                 /* runs in the history */
  int cursor, top;       /* the highlighted run, the first run in view */
  float scroll;          /* px the list is scrolled by now (it eases to top * PITCH) */
  bool info;             /* on a run's info page */
  int mon;               /* the highlighted Pokemon on it */
  menu_mon sum[RH_PARTY]; /* the Summary screen reads these while it is up */
} H;

static ui_screen g_sum_screen;

static const rh_entry* entry(int i) { return rhist_get(i); }

/* ------------------------------------------------------------------ words */

static rsv_mon rsv_of(const rh_mon* m) {
  return (rsv_mon){m->species, m->form, m->gender, m->shiny, m->variant, (int32_t)m->level, m->fainted};
}

/* "2026-10-19 11:12" (upstream: new Date(timestamp).toLocaleString()). The 3DS clock is local time, so no zone is applied. */
static const char* when_str(int64_t t, char* out, size_t n) {
  out[0] = 0;
  if (t <= 0) return out;
  const time_t tt = (time_t)t;
  struct tm tmv;
  if (gmtime_r(&tt, &tmv)) strftime(out, n, "%Y-%m-%d %H:%M", &tmv);
  return out;
}

static const char* trainer_text(const rh_entry* e, char* out, size_t n) {
  if (e->defeat == RH_DEFEAT_RIVAL) return i18n_t(out, (int)n, e->trainer_female ? "trainerNames:rivalFemale" : "trainerNames:rival", NULL);
  return i18n_resolve(out, (int)n, e->trainer);
}

/* "Victory!", "Defeated by Rival Finn", "Defeated by Ace Trainer Samantha", "Defeated by " (the Pokemon follow), "Defeated" */
static const char* result_text(const rh_entry* e, char* out, size_t n) {
  char who[160];
  if (e->victory) return i18n_t(out, (int)n, "runHistory:victory", NULL);
  switch (e->defeat) {
    case RH_DEFEAT_WILD:
      return i18n_t(out, (int)n, "runHistory:defeatedWild", NULL);
    case RH_DEFEAT_RIVAL: {
      char label[64];
      i18n_t(label, sizeof label, "runHistory:defeatedRival", NULL);
      snprintf(out, n, "%s %s", label, trainer_text(e, who, sizeof who));
      return out;
    }
    case RH_DEFEAT_TRAINER: {
      char label[64];
      i18n_t(label, sizeof label, "runHistory:defeatedTrainer", NULL);
      snprintf(out, n, "%s%s", label, trainer_text(e, who, sizeof who));
      return out;
    }
    default:
      return i18n_t(out, (int)n, "runHistory:defeated", NULL);
  }
}

/* ------------------------------------------------------------------ drawing: pieces */

static void draw_level(float right, float y, uint32_t level, bool fainted, int font_px) {
  char t[64], lv[48];
  i18n_t(lv, sizeof lv, "saveSlotSelectUiHandler:lv", NULL);
  snprintf(t, sizeof t, "%s%u", lv, (unsigned)level);
  text_opts o = text_defaults();
  o.origin_x = 1;
  o.font_px = font_px;
  o.color = fainted ? RR_RGBA(0xf8, 0x58, 0x58, 255) : WHITE;
  o.shadow = GREY_SHADOW;
  text_draw(TS_PARTY, right, y, t, &o);
}

static void outline(float x, float y, float w, float h) {
  rr_rect(x - 2, y - 2, w + 4, 2, YELLOW);
  rr_rect(x - 2, y + h, w + 4, 2, YELLOW);
  rr_rect(x - 2, y, 2, h, YELLOW);
  rr_rect(x + w, y, 2, h, YELLOW);
}

/* the wild Pokemon that beat the run: an icon with its level, 25 px apart */
static void draw_enemies(const rh_entry* e, float x, float y) {
  for (int i = 0; i < e->n_enemy; i++) {
    const rsv_mon m = rsv_of(&e->enemy[i]);
    const float ix = x + 25 * i;
    pi_draw(&m, ix + 12, y, 0.75f);
    draw_level(ix + 24, y + 15, e->enemy[i].level, false, 44);
  }
}

/* ... in a list row, where the next line is 14 px down: half-size icons on the result's line, without their levels (the page shows those) */
static void draw_enemies_row(const rh_entry* e, float x, float y) {
  for (int i = 0; i < e->n_enemy; i++) {
    const rsv_mon m = rsv_of(&e->enemy[i]);
    pi_draw(&m, x + 8 + 18 * i, y, 0.5f);
  }
}

/* ------------------------------------------------------------------ drawing: the list */

/* one run's window, with its own top left corner at (x, y): RunEntryContainer.setup */
static void draw_row(float x, float y, const rh_entry* e, bool selected) {
  ui_window(x, y, ROW_W, ROW_H, WIN_NORMAL);
  if (selected) outline(x, y, ROW_W, ROW_H);
  char buf[200], a[96], lvl[48];
  /* result at (8, 5); a wild defeat's Pokemon follow it, a trainer's name is cut where the party starts */
  result_text(e, buf, sizeof buf);
  text_opts r = text_defaults();
  float tw;
  if (e->victory || e->defeat == RH_DEFEAT_WILD) {
    tw = text_draw(TS_WINDOW, x + 8, y + 5, buf, &r);
  } else {
    r.wrap_w = 126;
    r.max_lines = 1;
    tw = text_draw(TS_WINDOW, x + 8, y + 5, buf, &r);
  }
  if (!e->victory && e->defeat == RH_DEFEAT_WILD) draw_enemies_row(e, x + 8 + tw + 4, y + 2);
  /* "Classic - Wave 195" at (8, 19), the time at (8, 33) */
  rt_mode_name(e->mode, a, sizeof a);
  i18n_t(lvl, sizeof lvl, "saveSlotSelectUiHandler:wave", NULL);
  snprintf(buf, sizeof buf, "%s - %s %d", a, lvl, e->wave);
  text_draw(TS_WINDOW, x + 8, y + 19, buf, NULL);
  text_draw(TS_WINDOW, x + 8, y + 33, when_str(e->time, a, sizeof a), NULL);
  /* the party: pokemonIconsContainer at (140, 17), icons at 0.75, 26 px apart, levels under them */
  for (int i = 0; i < e->n_party; i++) {
    const rsv_mon m = rsv_of(&e->party[i]);
    const float ix = x + 140 + 26 * i;
    pi_draw(&m, ix + 12, y + 17, 0.75f);
    draw_level(ix + 24, y + 32, e->party[i].level, e->party[i].fainted, 54);
  }
}

static void draw_empty(float x, float y, float w, float h) {
  char buf[96];
  ui_window(x, y, w, h, WIN_NORMAL);
  text_opts mid = text_defaults();
  mid.origin_x = mid.origin_y = 0.5f;
  if (H.a.status == RH_UNREADABLE)
    snprintf(buf, sizeof buf, "The run history can't be read.");
  else
    i18n_t(buf, sizeof buf, "saveSlotSelectUiHandler:empty", NULL);
  text_draw(TS_WINDOW, x + w / 2, y + h / 2, buf, &mid);
}

static void draw_arena(int biome) {
  char a[48], key[128];
  gm_enum_lower(GM_E_BIOME_ID, biome, a, sizeof a);
  if (!a[0]) return;
  snprintf(key, sizeof key, "images/arenas/%s_bg", a);
  if (img_has(key)) img_draw(key, NULL, 0, 0, 0, 0);
}

/* the highlighted run in full, on the top screen */
static void draw_preview(const rh_entry* e) {
  if (ui_space(UI_PHYS_TOP)) rr_rect(0, 0, 400, 240, RR_RGBA(0, 0, 0, 255));
  if (!ui_space(UI_FIELD)) return;
  if (!e) {
    draw_empty(12, 10, 296, 160);
    return;
  }
  draw_arena(e->biome);
  ui_window(12, 6, 296, 168, WIN_NORMAL);
  char buf[200];
  text_opts o = text_defaults();
  o.wrap_w = 268;
  o.max_lines = 1;
  const bool wild = !e->victory && e->defeat == RH_DEFEAT_WILD;
  const float tw = text_draw(TS_WINDOW, 26, 18, result_text(e, buf, sizeof buf), &o);
  if (wild) draw_enemies(e, 26 + tw, 10);
  rsv_info in;
  rhist_as_info(e, &in);
  rsum_draw(&in, 26, 38);
}

static void list_draw(rr_screen which) {
  if (which == RR_TOP) {
    draw_preview(H.n > 0 ? entry(H.cursor) : NULL);
    return;
  }
  if (!ui_space(UI_PHYS_BOTTOM)) return;
  clip_to_screen(); /* the run below the last one in view starts under the screen's edge */
  rr_rect(0, 0, 320, 240, RR_RGBA(0x00, 0x68, 0x60, 255));
  if (H.n == 0) {
    draw_empty(8, 8, 304, 165);
  } else {
    for (int i = 0; i < H.n; i++) {
      const float y = 8 + i * PITCH - H.scroll;
      if (y < -ROW_H - 4 || y > 244) continue;
      draw_row(i == H.cursor ? 4 : 8, y, entry(i), i == H.cursor);
    }
  }
  clip_off();
}

/* ------------------------------------------------------------------ drawing: the info page */

static int luck_of(const rh_entry* e) {
  int v = 0;
  for (int i = 0; i < e->n_party; i++) v += e->party[i].luck;
  return v > 14 ? 14 : v; /* Phaser.Math.Clamp(sum of getLuck(), 0, 14) */
}

static const char* const LUCK[] = {"D", "C", "C+", "B-", "B", "B+", "A-", "A", "A+", "A++", "S", "S+", "SS", "SS+", "SSS"};

/* The info page's top screen is upstream's layout (the header, the result window and the info window in a column on the left, the six party
 * rows on the right) laid out again on the whole 400 x 240 in physical px, not on the 320 x 180 field: upstream's 9 to 11 px words smear on
 * the 3DS (emerald is crisp at 16 px and from about 12 px up), so the words are 16 px, the fine print is the 8 px pixel font and the icons
 * are 1.0 (a 0.75 icon drops every fourth pixel row). Nothing was dropped to make room. */
#define IT_HEAD_H 24    /* the header window, the full width */
#define IT_SIDE_W 148   /* the column of the result window and the info window */
#define IT_RESULT_H 124 /* the result window (the info window has what is left of the 240) */
#define IT_TEXT_X 10    /* text inside those windows, 8 px frame plus 2 */
#define IT_TEXT_W 128   /* ... and its wrap width (the window's width less both frames) */
#define IT_ROW_X 152    /* the party rows: the right edge of the screen is 400, the highlight is 2 px outside the row */
#define IT_ROW_W 244
#define IT_ROW_Y 27
#define IT_ROW_PITCH 35
#define IT_ROW_H 33
#define IT_ROW_TEXT 44  /* the row's words start this far from its left edge, after a 32 px icon */

static void draw_info_top(const rh_entry* e) {
  if (!ui_space(UI_PHYS_TOP)) return;
  rr_rect(0, 0, 400, 240, RR_RGBA(0x00, 0x68, 0x60, 255)); /* gameStatsBg 0x006860 */
  char buf[200], a[96], lvl[48];
  text_opts o = text_defaults();
  const float step = text_line_pitch(TS_WINDOW, NULL) + 2; /* one line of the words to the next */
  /* the header: "Run Info" and the run's name */
  ui_window(0, 0, 400, IT_HEAD_H, WIN_NORMAL);
  i18n_t(buf, sizeof buf, "runHistory:runInfo", NULL);
  const float hw = text_draw(TS_HEADER_LABEL, IT_TEXT_X, 4, buf, &o);
  if (e->name[0]) {
    o.wrap_w = 400 - IT_TEXT_X - hw - 10 - 14;
    o.max_lines = 1;
    text_draw(TS_WINDOW, IT_TEXT_X + hw + 10, 4, e->name, &o);
  }
  /* the result window: the result, the wave, then who won */
  ui_window(0, IT_HEAD_H, IT_SIDE_W, IT_RESULT_H, WIN_NORMAL);
  float y = IT_HEAD_H + 8;
  o = text_defaults();
  if (e->victory) {
    result_text(e, buf, sizeof buf);
    o.color = RR_RGBA(0x78, 0xc8, 0xf0, 255);
  } else {
    i18n_t(buf, sizeof buf, "runHistory:defeated", NULL);
    o.color = RR_RGBA(0xf8, 0x58, 0x58, 255);
  }
  text_draw(TS_WINDOW, IT_TEXT_X, y, buf, &o);
  y += step;
  i18n_t(lvl, sizeof lvl, "saveSlotSelectUiHandler:wave", NULL);
  snprintf(a, sizeof a, "%s %d", lvl, e->wave);
  text_draw(TS_WINDOW, IT_TEXT_X, y, a, NULL);
  y += step;
  if (!e->victory && e->defeat == RH_DEFEAT_WILD) {
    /* the wild Pokemon that won: 2.0 icons (a crisp doubling) side by side, filling the window's width, their levels centred under them */
    for (int i = 0; i < e->n_enemy; i++) {
      const rsv_mon m = rsv_of(&e->enemy[i]);
      const float cx = IT_TEXT_X + 32 + 64 * i;
      pi_draw(&m, cx, y, 2.0f);
      i18n_t(a, sizeof a, "saveSlotSelectUiHandler:lv", NULL);
      snprintf(buf, sizeof buf, "%s%u", a, (unsigned)e->enemy[i].level);
      o = text_defaults();
      o.origin_x = 0.5f;
      text_draw(TS_WINDOW, cx, y + 58, buf, &o);
    }
  } else if (!e->victory && (e->defeat == RH_DEFEAT_TRAINER || e->defeat == RH_DEFEAT_RIVAL)) {
    /* upstream shows "<name> would like to battle!" here (battle:trainerAppeared), beside the trainer's sprite; this page has no sprite, so
     * the sentence alone reads oddly under "Defeated" (lorenzo, 2026-11-16): just the name, trainerAppeared's {{trainerName}} */
    char who[160];
    trainer_text(e, who, sizeof who);
    o = text_defaults();
    o.wrap_w = IT_TEXT_W;
    /* four lines of the words fit; a longer name drops to the pixel font rather than be cut */
    if (text_lines(TS_WINDOW, who, &o) <= 4) {
      o.max_lines = 4;
      text_draw(TS_WINDOW, IT_TEXT_X, y, who, &o);
    } else {
      o.max_lines = 8;
      text_draw(TS_INSTRUCTIONS_TEXT, IT_TEXT_X, y, who, &o);
    }
  }
  /* the info window: mode (a long name wraps to a second line), money, luck */
  const float iy = IT_HEAD_H + IT_RESULT_H;
  ui_window(0, iy, IT_SIDE_W, 240 - iy, WIN_NORMAL);
  y = iy + 8;
  o = text_defaults();
  o.wrap_w = IT_TEXT_W;
  char mode[96], md[48];
  i18n_t(md, sizeof md, "runHistory:mode", NULL);
  rt_mode_name(e->mode, mode, sizeof mode);
  snprintf(buf, sizeof buf, "%s: %s", md, mode);
  text_draw(TS_WINDOW, IT_TEXT_X, y, buf, &o);
  y += text_lines(TS_WINDOW, buf, &o) * text_line_pitch(TS_WINDOW, NULL) + 2;
  o = text_defaults();
  o.font_px = 96; /* TS_MONEY's colours at the same size as the words around it */
  text_draw(TS_MONEY, IT_TEXT_X, y, rt_money((double)e->money, a, sizeof a), &o);
  y += step;
  i18n_t(md, sizeof md, "runHistory:luck", NULL);
  snprintf(buf, sizeof buf, "%s: %s", md, LUCK[luck_of(e)]);
  text_draw(TS_WINDOW, IT_TEXT_X, y, buf, NULL);
  /* the party: six rows, the highlighted one outlined */
  i18n_t(lvl, sizeof lvl, "saveSlotSelectUiHandler:lv", NULL);
  for (int i = 0; i < e->n_party; i++) {
    const rh_mon* m = &e->party[i];
    const float ry = IT_ROW_Y + i * IT_ROW_PITCH;
    rr_rect(IT_ROW_X, ry, IT_ROW_W, IT_ROW_H, RR_RGBA(0x10, 0x30, 0x38, 255));
    if (i == H.mon) outline(IT_ROW_X, ry, IT_ROW_W, IT_ROW_H);
    const rsv_mon rm = rsv_of(m);
    pi_draw(&rm, IT_ROW_X + 22, ry + 1, 1.0f);
    o = text_defaults();
    o.wrap_w = IT_ROW_W - IT_ROW_TEXT - 6;
    o.max_lines = 1;
    char nm[96], nat[64], ab[96];
    snprintf(nm, sizeof nm, "%s", i18n_species_name(m->species));
    snprintf(buf, sizeof buf, "%s  %s%u", nm, lvl, (unsigned)m->level);
    text_draw(TS_SUMMARY, IT_ROW_X + IT_ROW_TEXT, ry + 1, buf, &o);
    snprintf(a, sizeof a, "nature:%s", m->nature < PR_NATURE_COUNT ? menu_nature_keys[m->nature] : "hardy");
    i18n_t(nat, sizeof nat, a, NULL);
    snprintf(ab, sizeof ab, "%s", i18n_ability_name(m->ability));
    snprintf(buf, sizeof buf, "%s - %s", nat, ab);
    text_draw(TS_INSTRUCTIONS_TEXT, IT_ROW_X + IT_ROW_TEXT, ry + 21, buf, &o);
  }
}

static void draw_stat(int i, float x, float y, const rh_mon* m) {
  static const char* const KEYS[6] = {"hp", "atk", "def", "spatk", "spdef", "spd"};
  char key[48], label[48], buf[96];
  snprintf(key, sizeof key, "pokemonInfo:stat.%sShortened", KEYS[i]);
  i18n_t(label, sizeof label, key, NULL);
  /* the label is pink for the stat the nature raises and blue for the one it lowers (summary.c does the same) */
  const double mult = i > 0 && m->nature < PR_NATURE_COUNT ? pr_nature_mult[m->nature][i] : 1.0;
  text_draw(mult == 1.0 ? TS_SUMMARY_STATS : mult > 1.0 ? TS_SUMMARY_STATS_PINK : TS_SUMMARY_STATS_BLUE, x, y, label, NULL);
  snprintf(buf, sizeof buf, "%u", (unsigned)m->stats[i]);
  text_opts o = text_defaults();
  o.origin_x = 1;
  text_draw(TS_WINDOW_ALT, x + 100, y, buf, &o);
}

/* The hints on the info page's bottom line: also what a tap on them presses */
static const char* const INFO_HINTS[2] = {"A: Summary", "B: Back"};
#define INFO_HINT_X 16
#define INFO_HINT_Y 222 /* 2 px above the other screens' hints (lorenzo, 2026-11-23) */
/* The Pokemon's block at the top (icon, name, nature, types) is 76 px tall: a tap on it goes on to the next Pokemon of the run */
#define INFO_NAME_H 76
#define IB_ICON_X 48 /* the icon is drawn at 2.0, a crisp doubling of the 32 px icon: centre x and top y */
#define IB_ICON_Y 8
#define IB_TEXT_X 92 /* name, nature and types beside it */

/* the highlighted Pokemon's stats and moves, on the bottom screen */
static void draw_info_bottom(const rh_entry* e) {
  if (!ui_space(UI_PHYS_BOTTOM)) return;
  rr_rect(0, 0, 320, 240, RR_RGBA(0x00, 0x68, 0x60, 255));
  ui_window(4, 4, 312, 232, WIN_NORMAL);
  if (H.mon >= e->n_party) return;
  const rh_mon* m = &e->party[H.mon];
  char buf[160], nat[64], a[96], lvl[48], ab[96];
  const rsv_mon rm = rsv_of(m);
  pi_draw(&rm, IB_ICON_X, IB_ICON_Y, 2.0f);
  i18n_t(lvl, sizeof lvl, "saveSlotSelectUiHandler:lv", NULL);
  snprintf(buf, sizeof buf, "%s  %s%u", i18n_species_name(m->species), lvl, (unsigned)m->level);
  text_draw(TS_WINDOW, IB_TEXT_X, 14, buf, NULL);
  snprintf(a, sizeof a, "nature:%s", m->nature < PR_NATURE_COUNT ? menu_nature_keys[m->nature] : "hardy");
  i18n_t(nat, sizeof nat, a, NULL);
  text_draw(TS_WINDOW, IB_TEXT_X, 34, nat, NULL);
  /* types (beside the icon, bottom edge at y 70), ability and passive */
  for (int i = 0; i < 2; i++) {
    if (m->types[i] < 0 || m->types[i] >= PR_TYPE_COUNT + 1 || !menu_type_frames[m->types[i]]) continue;
    img_draw("images/types", menu_type_frames[m->types[i]], IB_TEXT_X + 34 * i, 70, 0, 1);
  }
  i18n_t(lvl, sizeof lvl, "starterSelectUiHandler:ability", NULL);
  snprintf(ab, sizeof ab, "%s", i18n_ability_name(m->ability));
  snprintf(buf, sizeof buf, "%s %s", lvl, ab);
  text_draw(TS_WINDOW, 16, 78, buf, NULL);
  if (m->has_passive) {
    i18n_t(lvl, sizeof lvl, "starterSelectUiHandler:passive", NULL);
    snprintf(ab, sizeof ab, "%s", i18n_ability_name(m->passive));
    snprintf(buf, sizeof buf, "%s %s", lvl, ab);
    text_draw(TS_WINDOW, 16, 96, buf, NULL);
  }
  /* the six stats in two columns */
  for (int i = 0; i < 6; i++) draw_stat(i, 16 + (i / 3) * 150, 118 + (i % 3) * 16, m);
  /* the moves, two columns of two */
  for (int i = 0; i < 4; i++) {
    const float x = 16 + (i % 2) * 150, y = 172 + (i / 2) * 26;
    if (i >= m->n_moves || !m->moves[i]) {
      text_draw(TS_WINDOW, x, y, "-", NULL);
      continue;
    }
    text_opts o = text_defaults();
    o.wrap_w = 138;
    o.max_lines = 1;
    text_draw(TS_WINDOW, x, y, i18n_move_name(m->moves[i]), &o);
  }
  for (int i = 0; i < 2; i++) text_draw(TS_INSTRUCTIONS_TEXT, hint_x(TS_INSTRUCTIONS_TEXT, INFO_HINT_X, INFO_HINTS, i), INFO_HINT_Y, INFO_HINTS[i], NULL);
}

/* ------------------------------------------------------------------ the Summary screen over the info page */

static void sum_close(void* ctx) {
  (void)ctx;
  if (ui_top() == &g_sum_screen) ui_pop();
}

static void sum_swap(void* ctx, int slot, int a, int b) { (void)ctx, (void)slot, (void)a, (void)b; /* shown until it closes; a past run is not changed */ }

static void to_menu_mon(const rh_mon* m, menu_mon* o) {
  memset(o, 0, sizeof *o);
  o->species = m->species;
  o->form = m->form;
  o->gender = m->gender;
  o->shiny = m->shiny;
  o->variant = m->variant;
  snprintf(o->name, sizeof o->name, "%s", i18n_species_name(m->species));
  o->level = (uint16_t)m->level;
  o->hp = m->hp;
  o->max_hp = m->stats[0];
  for (int i = 0; i < m->n_moves && i < 4; i++) o->moves[i] = (menu_move){m->moves[i], m->pp_used[i], m->pp_max[i], false, 0};
  o->nmoves = m->n_moves;
  o->allowed_in_battle = true;
  o->types[0] = m->types[0];
  o->types[1] = m->types[1];
  o->ability = m->ability;
  o->passive = m->passive;
  o->has_passive = m->has_passive;
  o->nature = m->nature;
  for (int i = 0; i < 6; i++) {
    o->stats[i] = m->stats[i];
    o->ivs[i] = m->ivs[i];
  }
  o->friendship = m->friendship;
  o->tera_type = m->tera_type;
  o->luck = m->luck;
  o->met_biome = -1;
  o->met_level = (uint16_t)m->level;
}

static void open_summary(const rh_entry* e) {
  for (int i = 0; i < e->n_party; i++) to_menu_mon(&e->party[i], &H.sum[i]);
  summary_args a;
  memset(&a, 0, sizeof a);
  a.mode = SUMMARY_DEFAULT;
  a.page = SUMMARY_PROFILE;
  a.party = H.sum;
  a.n = e->n_party;
  a.index = H.mon;
  a.from_party = true;
  a.new_move_type = -1;
  static const summary_events ev = {sum_close, sum_swap, NULL, NULL};
  summary_open(&a, &ev);
  ui_push(&g_sum_screen);
}

static bool hsum_input(ui_screen* s, ui_button b) {
  (void)s;
  return summary_input((mui_button)b); /* both are upstream's Button enum, in order */
}

static void hsum_update(ui_screen* s, float dt) {
  (void)s;
  mui_tick(dt);
  summary_update(dt);
}

static void hsum_draw(ui_screen* s, rr_screen which) {
  (void)s;
  if (which != RR_BOTTOM) return;
  if (ui_space(UI_PHYS_BOTTOM)) rr_rect(0, 0, 320, 240, RR_RGBA(0, 0, 0, 255));
  mui_set_origin(RR_BOTTOM, 0, 30); /* UI_BOTTOM_CANVAS */
  summary_draw();
}

static bool hsum_touch(ui_screen* s, float x, float y) {
  (void)s;
  return summary_active() && summary_touch(x, y - 30); /* UI_BOTTOM_CANVAS: the canvas starts 30 px down */
}

static ui_screen g_sum_screen = {"history-summary", UI_OVERLAY, NULL, NULL, NULL, hsum_input, hsum_update, hsum_draw, NULL, hsum_touch};

/* ------------------------------------------------------------------ the screen */

static void keep_cursor_in_view(void) {
  if (H.cursor < H.top) H.top = H.cursor;
  if (H.cursor > H.top + VISIBLE - 1) H.top = H.cursor - (VISIBLE - 1);
  if (H.top > H.n - VISIBLE) H.top = H.n - VISIBLE;
  if (H.top < 0) H.top = 0;
}

static bool hs_input(ui_screen* s, ui_button b) {
  (void)s;
  if (H.info) {
    const rh_entry* e = entry(H.cursor);
    const int n = e ? e->n_party : 0;
    switch (b) {
      case BTN_UP:
      case BTN_DOWN:
        if (n > 1) {
          H.mon = (H.mon + (b == BTN_UP ? n - 1 : 1)) % n;
          ui_play_select();
        }
        return true;
      case BTN_ACTION:
        if (e && n > 0) {
          ui_play_select();
          open_summary(e);
        }
        return true;
      case BTN_CANCEL:
        H.info = false;
        ui_play_select();
        return true;
      default:
        return false;
    }
  }
  switch (b) {
    case BTN_UP:
    case BTN_DOWN:
      if (H.n > 1) {
        H.cursor = (H.cursor + (b == BTN_UP ? H.n - 1 : 1)) % H.n;
        keep_cursor_in_view();
        ui_play_select();
      }
      return true;
    case BTN_ACTION:
      if (H.n > 0) {
        H.info = true;
        H.mon = 0;
        ui_play_select();
      }
      return true;
    case BTN_CANCEL:
      ui_play_select();
      if (H.a.on_exit) H.a.on_exit(H.a.arg);
      return true;
    default:
      return false;
  }
}

/* A tap on the bottom screen (physical px). The list: a tap on a run points at it (the pointed run is drawn at x 4, the others at x 8: either
 * way x 4..312), a second tap on the pointed run is A (its info page), like the save slots. The info page: the hints on the bottom line are A
 * and B, and a tap on the Pokemon's name block above (y < 60) goes on to the next one of the party, which the top screen's rows show. */
static bool hs_touch(ui_screen* s, float x, float y) {
  if (H.info) {
    if (y >= INFO_HINT_Y - 8) {
      const int h = hint_at(TS_INSTRUCTIONS_TEXT, INFO_HINT_X, INFO_HINTS, 2, x);
      return h < 0 ? false : hs_input(s, h == 0 ? BTN_ACTION : BTN_CANCEL);
    }
    return y < INFO_NAME_H ? hs_input(s, BTN_DOWN) : false;
  }
  for (int i = 0; i < H.n; i++) {
    const float top = 8 + i * PITCH - H.scroll;
    if (x < 4 || x >= 8 + ROW_W || y < top || y >= top + ROW_H) continue;
    if (i == H.cursor) return hs_input(s, BTN_ACTION);
    H.cursor = i;
    keep_cursor_in_view();
    ui_play_select();
    return true;
  }
  return false;
}

static void hs_update(ui_screen* s, float dt) {
  (void)s;
  const float target = H.top * PITCH;
  const float k = dt / 90.0f > 1 ? 1 : dt / 90.0f;
  H.scroll += (target - H.scroll) * k;
  if (H.scroll - target < 0.5f && target - H.scroll < 0.5f) H.scroll = target;
}

static void hs_draw(ui_screen* s, rr_screen which) {
  (void)s;
  if (H.info) {
    const rh_entry* e = entry(H.cursor);
    if (!e) return;
    if (which == RR_TOP)
      draw_info_top(e);
    else
      draw_info_bottom(e);
    return;
  }
  list_draw(which);
}

static ui_screen g_screen = {"run-history", UI_FADE, NULL, NULL, NULL, hs_input, hs_update, hs_draw, NULL, hs_touch};

void history_screen_open(const hist_args* a) {
  memset(&H, 0, sizeof H);
  H.a = *a;
  H.n = rhist_count();
  ui_set(&g_screen);
}
