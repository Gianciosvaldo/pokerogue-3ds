/*
 * Summary screen: a port of upstream's summary-ui-handler.ts (8555c08).
 * Positions, images, text styles and strings are upstream's; the numbers in
 * comments are its container offsets. The summary container sits at the
 * canvas bottom, so its y values are written Y(upstream y).
 *
 * Not ported: the hover tooltips (no pointer), the tera type icon (needs the
 * type_tera image tinted per type), the rainbow tint on a maxed candy count,
 * the held items' stack counts (the item-count bitmap font isn't in the packs
 * yet) and the fusion palette swap on the big sprite.
 */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "menus.h"
#include "pr_data.h"

#define Y(v) (MUI_H + (v))

extern const char* const menu_move_keys[];
extern const int menu_move_key_count;
extern const char* const menu_biome_keys[];
extern const int menu_biome_key_count;
extern const char* const menu_nature_keys[];
extern const char* const menu_type_frames[];

#define PAGE_X 106
#define SLIDE 214
#define DESC_PITCH 14.83f /* upstream's line pitch for scrolling descriptions */

typedef struct {
  float from, to, t, dur; /* t < 0: idle */
  int ease;               /* 0 linear, 1 Sine.easeOut, 2 Sine.easeIn */
} tween;

static float tween_value(const tween* w) {
  if (w->t < 0 || w->dur <= 0) return w->to;
  float u = w->t / w->dur;
  if (u > 1) u = 1;
  if (w->ease == 1) u = sinf(u * (float)M_PI / 2);
  if (w->ease == 2) u = 1 - cosf(u * (float)M_PI / 2);
  return w->from + (w->to - w->from) * u;
}

static void tween_to(tween* w, float to, float dur, int ease) {
  w->from = tween_value(w);
  w->to = to;
  w->t = dur > 0 ? 0 : -1;
  w->dur = dur;
  w->ease = ease;
}

static void tween_step(tween* w, float dt) {
  if (w->t < 0) return;
  w->t += dt;
  if (w->t >= w->dur) w->t = -1;
}

/* The looping description scroll (delay 2 s, scroll 2 s a line, hold 2 s):
 * how far the text has moved up `ms` after it was shown. */
static float scroll_offset(float ms, int lines, int visible) {
  if (lines <= visible || ms < 2000) return 0;
  float d = (lines - visible) * 2000.0f, dist = DESC_PITCH * (lines - visible);
  float u = fmodf(ms - 2000, d + 2000);
  return u < d ? dist * u / d : dist;
}

/* Status and move effect boxes keep their state between openings, like the
 * handler's fields. */
static bool g_status_visible;
static tween g_status_x = {-106, -106, -1, 0, 0};
static bool g_effects_visible;
static tween g_effect_x = {106, 106, -1, 0, 0};

static struct {
  bool active;
  summary_args a;
  summary_events ev;
  menu_mon mon; /* the member shown (moves swap here first) */
  int index;
  menu_sprite sprite;
  float now; /* ms since opened */
  float sprite_t0;

  int cursor; /* page, -1 before the first */
  int main_page; /* what the page container shows */
  bool page_visible;

  bool transitioning;
  bool trans_forward;
  int trans_page;
  tween trans_x; /* the transition container's x offset from PAGE_X */

  /* PROFILE */
  bool show_passive;
  char ot[160], memo[512];
  char abil_desc[2][512];
  int abil_lines[2];
  float abil_t0;
  /* STATS */
  bool show_ivs;
  /* MOVES */
  bool move_select;
  int move_cursor, selected_move;
  bool extra_row;
  bool cursor_obj;
  float blink_t0;
  char move_desc[512];
  int move_desc_lines;
  float move_desc_y, move_desc_t0;
  char power[8], accuracy[8];
  int category;
} S;

/* ------------------------------------------------------------ data helpers */

static const char* type_frame(int t) { return t >= 0 && t < PR_TYPE_COUNT + 1 ? menu_type_frames[t] : NULL; }

static int move_type(const menu_move* m) {
  if (m->has_type) return m->type;
  return m->id < PR_MOVE_COUNT ? pr_moves[m->id].type : 0;
}

static const char* move_effect(uint16_t id) {
  if (id >= menu_move_key_count || !menu_move_keys[id]) return "";
  char key[96];
  snprintf(key, sizeof key, "move:%s.effect", menu_move_keys[id]);
  return mui_t(key);
}

static const char* ability_text(uint16_t id, const char* what) {
  if (id >= PR_ABILITY_COUNT || !pr_abilities[id].key) return "";
  char key[96];
  snprintf(key, sizeof key, "ability:%s.%s", pr_abilities[id].key, what);
  return mui_t(key);
}

/* utils formatLargeNumber / formatStat */
static void format_stat(char* out, size_t n, uint32_t v, bool hp) {
  uint32_t threshold = hp ? 100000 : 1000000;
  if (v < threshold) {
    snprintf(out, n, "%lu", (unsigned long)v);
    return;
  }
  char s[16];
  snprintf(s, sizeof s, "%lu", (unsigned long)v);
  int len = (int)strlen(s);
  static const char* const abr[] = {"common:abrThousand", "common:abrMillion", "common:abrBillion"};
  int k = (len + 2) / 3 - 1;
  if (k < 1 || k > 3) {
    snprintf(out, n, "?");
    return;
  }
  int digits = ((len + 2) % 3) + 1;
  char dec[3] = {0};
  memcpy(dec, s + digits, 2);
  while (*dec && dec[strlen(dec) - 1] == '0') dec[strlen(dec) - 1] = 0;
  snprintf(out, n, "%.*s%s%s%s", digits, s, *dec ? "." : "", dec, mui_t(abr[k - 1]));
}

/* Helper to parse active unit's Shiny Tier (0 = None, 1 = Yellow, 2 = Blue, 3 = Red) */
int summary_mon_shiny_tier(const menu_mon* m, int* fusion_tier) {
  if (fusion_tier) *fusion_tier = 0;
  if (!m) return 0;
  bool base_shiny = m->shiny;
  bool fus_shiny = m->fusion && m->fusion_shiny;
  if (!base_shiny && !fus_shiny) return 0;

  int base_t = base_shiny ? (m->variant < 3 ? m->variant + 1 : 1) : 0;
  int fus_t = fus_shiny ? (m->fusion_variant < 3 ? m->fusion_variant + 1 : 1) : 0;

  if (fusion_tier) *fusion_tier = fus_t;

  if (base_shiny) return base_t;
  return fus_t;
}

const char* summary_shiny_tier_name(int tier) {
  switch (tier) {
    case 1: return "Yellow";
    case 2: return "Blue";
    case 3: return "Red";
    default: return "None";
  }
}

/* utils padInt */
static void pad_int(char* out, size_t n, int v, int len, const char* with) {
  char s[16], t[48];
  snprintf(s, sizeof s, "%d", v);
  while ((int)strlen(s) < len) {
    snprintf(t, sizeof t, "%s%s", with, s);
    snprintf(s, sizeof s, "%s", t);
  }
  snprintf(out, n, "%s", s);
}

static rr_color tint_mul(rr_color c, uint32_t rgb) {
  uint32_t r = ((c >> 24) & 0xff) * ((rgb >> 16) & 0xff) / 255;
  uint32_t g = ((c >> 16) & 0xff) * ((rgb >> 8) & 0xff) / 255;
  uint32_t b = ((c >> 8) & 0xff) * (rgb & 0xff) / 255;
  return RR_RGBA(r, g, b, c & 0xff);
}

/* sprites/variant.ts getVariantTint */
static uint32_t variant_tint(int v) { return v == 1 ? 0x20f8f0 : v == 2 ? 0xe81048 : 0xf8c020; }

static void title_case(char* s) {
  bool start = true;
  for (; *s; s++) {
    if (*s == ' ' || *s == '-' || *s == '_') {
      start = true;
      continue;
    }
    if (*s >= 'a' && *s <= 'z' && start) *s -= 32;
    else if (*s >= 'A' && *s <= 'Z' && !start) *s += 32;
    start = false;
  }
}

static const char* page_key(int page) {
  return page == SUMMARY_PROFILE ? "ui/summary_profile" : page == SUMMARY_STATS ? "ui/summary_stats" : "ui/summary_moves";
}

static int frame_h(const char* key) {
  const rr_frame* f = mui_frame(key, NULL);
  return f ? f->src_h : 0;
}

/* ------------------------------------------------------------ status and move effect */

static void show_status(bool instant) {
  if (g_status_visible) return;
  g_status_visible = true;
  tween_to(&g_status_x, 0, instant ? 0 : 250, 1);
}

static void hide_status(bool instant) {
  if (!g_status_visible) return;
  g_status_visible = false;
  tween_to(&g_status_x, -106, instant ? 0 : 250, 2);
}

static void show_move_effect(bool instant) {
  if (g_effects_visible) return;
  g_effects_visible = true;
  tween_to(&g_effect_x, 6, instant ? 0 : 250, 1);
}

static void hide_move_effect(bool instant) {
  if (!g_effects_visible) return;
  g_effects_visible = false;
  tween_to(&g_effect_x, 106, instant ? 0 : 250, 2);
}

/* ------------------------------------------------------------ pages */

static void build_profile_text(void) {
  const menu_mon* m = &S.mon;
  char a[256], b[256];
  /* OT line */
  const char* name = S.a.trainer_name && *S.a.trainer_name ? S.a.trainer_name : mui_t("pokemonSummary:unknownTrainer");
  snprintf(b, sizeof b, "%s/", mui_t("pokemonSummary:ot"));
  mui_bbfrag(a, sizeof a, b, TS_SUMMARY_ALT, false);
  mui_bbfrag(b, sizeof b, name, S.a.player_female ? TS_SUMMARY_PINK : TS_SUMMARY_BLUE, false);
  snprintf(S.ot, sizeof S.ot, "%s%s", a, b);

  /* ability and passive descriptions, wrapped at 1224 / 6 */
  for (int i = 0; i < 2; i++) {
    uint16_t id = i ? m->passive : m->ability;
    S.abil_lines[i] = mui_wrap(TS_WINDOW_ALT, ability_text(id, "description"), 1224 / 6.0f, S.abil_desc[i],
                               sizeof S.abil_desc[i]);
  }
  S.abil_t0 = S.now;

  /* trainer memo */
  char close[64], nature[320], nat_name[48], biome[320], level[320], wave[320], met[1024], nat_frag[400], tmp[96];
  mui_bbfrag(close, sizeof close, "", TS_WINDOW_ALT, false);
  const char* nkey = m->nature < PR_NATURE_COUNT ? menu_nature_keys[m->nature] : "hardy";
  snprintf(tmp, sizeof tmp, "nature:%s", nkey);
  snprintf(nat_name, sizeof nat_name, "%s", mui_t(tmp));
  title_case(nat_name);
  mui_bbfrag(a, sizeof a, nat_name, TS_SUMMARY_RED, false);
  snprintf(nature, sizeof nature, "%s%s", a, close);

  const char* bname;
  if (m->met_biome < 0 || m->met_biome >= menu_biome_key_count || !menu_biome_keys[m->met_biome]) {
    bname = mui_t("biome:unknownLocation");
  } else {
    snprintf(tmp, sizeof tmp, "biome:%s", menu_biome_keys[m->met_biome]);
    bname = mui_t(tmp);
  }
  mui_bbfrag(a, sizeof a, bname, TS_SUMMARY_RED, false);
  snprintf(biome, sizeof biome, "%s%s", a, close);
  snprintf(tmp, sizeof tmp, "%u", m->met_level);
  mui_bbfrag(a, sizeof a, tmp, TS_SUMMARY_RED, false);
  snprintf(level, sizeof level, "%s%s", a, close);
  if (m->met_wave)
    snprintf(tmp, sizeof tmp, "%u", m->met_wave);
  else
    snprintf(tmp, sizeof tmp, "%s", mui_t("pokemonSummary:unknownTrainer"));
  mui_bbfrag(a, sizeof a, tmp, TS_SUMMARY_RED, false);
  snprintf(wave, sizeof wave, "%s%s", a, close);
  mui_tf(met, sizeof met, m->met_biome == -1 ? "pokemonSummary:metFragment.apparently" : "pokemonSummary:metFragment.normal",
         "biome", biome, "level", level, "wave", wave, NULL);
  snprintf(tmp, sizeof tmp, "pokemonSummary:natureFragment.%s", nkey);
  mui_tf(nat_frag, sizeof nat_frag, tmp, "nature", nature, NULL);
  mui_tf(S.memo, sizeof S.memo, "pokemonSummary:memoString", "metFragment", met, "natureFragment", nat_frag, NULL);
}

/* populatePageContainer for the page container (the transition container
 * shows a fresh copy of its page, drawn from the same data). */
static void populate_main(int page) {
  S.main_page = page;
  S.page_visible = true;
  switch (page) {
    case SUMMARY_PROFILE:
      S.show_passive = false;
      build_profile_text();
      break;
    case SUMMARY_STATS:
      S.show_ivs = false;
      break;
    case SUMMARY_MOVES:
      S.extra_row = S.a.mode == SUMMARY_LEARN_MOVE;
      S.move_desc[0] = 0;
      S.move_desc_lines = 0;
      S.move_desc_y = 84;
      break;
  }
}

static void draw_profile(float px, float py, bool main) {
  const menu_mon* m = &S.mon;
  mui_image("ui/text_images/en/summary/summary_profile_profile_title", NULL, px + 7, py + 4, 0, 0.5f);
  if (!main) build_profile_text(); /* cheap enough for the 250 ms slide */
  mui_bbtext(TS_SUMMARY_ALT, px + 7, py + 10, S.ot);
  char buf[96];
  mui_tf(buf, sizeof buf, "pokemonSummary:idNo", "idNo", S.a.trainer_id ? S.a.trainer_id : "", NULL);
  mui_text(TS_SUMMARY_ALT, px + 141, py + 10, buf);

  snprintf(buf, sizeof buf, "%s/", mui_t("pokemonSummary:type"));
  int tw = mui_text(TS_WINDOW_ALT, px + 7, py + 27, buf);
  for (int i = 0; i < 2; i++) {
    const char* tf = type_frame(m->types[i]);
    if (m->types[i] >= 0 && tf) mui_image("types", tf, px + tw + 9 + 34 * i, py + 41, 0, 1);
  }
  if (m->luck) {
    int lw = mui_text(TS_WINDOW_ALT, px + 141, py + 27, mui_t("common:luckIndicator"));
    snprintf(buf, sizeof buf, "%u", m->luck);
    uint32_t t = variant_tint(m->luck - 1 > 2 ? 2 : m->luck - 1);
    mui_text_c(TS_LUCK_VALUE, px + 141 + lw + 2, py + 27, buf, tint_mul(mui_style_color(TS_LUCK_VALUE, false), t),
               tint_mul(mui_style_color(TS_LUCK_VALUE, true), t));
  }
  if (m->show_tera && type_frame(m->tera_type))
    mui_image("ui/button_tera", type_frame(m->tera_type), px + 128, py + 26, 0.5f, 0.5f);

  if (m->has_passive) mui_image("ui/summary_profile_prompt_a", NULL, px + 8, py + 43, 0, 0);

  /* One ability block at a time (upstream toggles the ability and the passive in place with A): the label at (17, 47), the name's
   * bottom edge at 68, the description from 71, behind a mask (110, 90, 206, 31 in the scene) that its scrolling slides under */
  const bool passive = m->has_passive && S.show_passive;
  mui_image(passive ? "ui/text_images/en/summary/summary_profile_passive" : "ui/text_images/en/summary/summary_profile_ability", NULL,
            px + 17, py + 47, 0, 0.5f);
  mui_text_o(TS_SUMMARY_ALT, px + 7, py + 68, 0, 1, ability_text(passive ? m->passive : m->ability, "name"));
  const int lines = S.abil_lines[passive ? 1 : 0];
  float dy = lines > 2 ? 69 : 71;
  dy -= scroll_offset(S.now - S.abil_t0, lines, 2);
  mui_clip(px, py + 71, 206, 31);
  mui_text(TS_WINDOW_ALT, px + 7, py + dy, S.abil_desc[passive ? 1 : 0]);
  mui_clip_off();

  mui_image("ui/text_images/en/summary/summary_profile_memo_title", NULL, px + 7, py + 107, 0, 0.5f);
  mui_bbtext(TS_WINDOW_ALT, px + 7, py + 113, S.memo);
}

static void draw_stats(float px, float py, bool main) {
  const menu_mon* m = &S.mon;
  bool ivs = main && S.show_ivs;
  mui_image("ui/text_images/en/summary/summary_stats_item_title", NULL, px + 7, py + 4, 0, 0.5f);
  mui_image("ui/text_images/en/summary/summary_stats_stats_title", NULL, px + 16, py + 59, 0, 0.5f);
  mui_image("ui/text_images/en/summary/summary_stats_exp_title", NULL, px + 7, py + 115, 0, 0.5f);
  mui_image("ui/text_images/en/summary/summary_stats_expbar_title", NULL, px + 126, py + 152, 0, 0);

  static const char* const stat_keys[6] = {"pokemonInfo:stat.hp", "pokemonInfo:stat.atk", "pokemonInfo:stat.def",
                                           "pokemonInfo:stat.spatk", "pokemonInfo:stat.spdef", "pokemonInfo:stat.spd"};
  float gx = px + 27, gy = py + 64;
  for (int s = 0; s < 6; s++) {
    int row = s % 3, col = s / 3;
    mui_style st;
    if (ivs) {
      st = m->ivs[s] == 31 ? TS_SUMMARY_STATS_GOLD : TS_SUMMARY_STATS;
    } else {
      double mult = m->nature < PR_NATURE_COUNT ? pr_nature_mult[m->nature][s] : 1.0;
      st = mult == 1.0 ? TS_SUMMARY_STATS : mult > 1.0 ? TS_SUMMARY_STATS_PINK : TS_SUMMARY_STATS_BLUE;
    }
    mui_text_o(st, gx + 116 * col + (col == 1 ? 5 : 0), gy + 16 * row, 0.5f, 0, mui_t(stat_keys[s]));
    char v[40], a[16], b[16];
    if (ivs) {
      snprintf(v, sizeof v, "%u/31", m->ivs[s]);
    } else if (s == 0) {
      format_stat(a, sizeof a, m->hp, true);
      format_stat(b, sizeof b, m->max_hp, true);
      snprintf(v, sizeof v, "%s/%s", a, b);
    } else {
      format_stat(v, sizeof v, m->stats[s], false);
    }
    mui_text_o(TS_WINDOW_ALT, gx + 93 + 93 * col, gy + 16 * row, 1, 0, v);
  }

  /* held items: getIcon(true), a container at scale 0.5 with the item at (0, 12) */
  for (int i = 0; i < m->nitems; i++) {
    if (!m->items[i].icon) continue;
    rr_draw_opts o = RR_DEFAULT_OPTS;
    o.origin_y = 0.5f;
    o.scale_x = o.scale_y = 0.5f;
    mui_image_ex("items", m->items[i].icon, px + (i % 17) * 12 + 3, py + 14 * (i / 17) + 11 + 6, &o);
  }

  mui_text(TS_SUMMARY, px + 6, py + 120, mui_t("pokemonSummary:expPoints"));
  mui_text(TS_SUMMARY, px + 6, py + 136, mui_t("pokemonSummary:nextLv"));
  char v[16];
  snprintf(v, sizeof v, "%lu", (unsigned long)m->exp);
  mui_text_o(TS_WINDOW_ALT, px + 213, py + 120, 1, 0, v);
  snprintf(v, sizeof v, "%lu", (unsigned long)m->next_lv_exp);
  mui_text_o(TS_WINDOW_ALT, px + 213, py + 136, 1, 0, v);
  const rr_frame* ov = mui_frame("ui/summary_stats_overlay_exp", NULL);
  int w = (int)floorf(m->exp_ratio * 64);
  if (ov && w > 0) rr_draw_cropped(ov, px + 140, py + 153, w, ov->src_h, &RR_DEFAULT_OPTS);
  mui_image("ui/summary_profile_prompt_a", NULL, px + 8, py + 55, 0, 0);
}

static void pp_text(char* out, size_t n, int pp, int max) {
  char a[16], b[16];
  pad_int(a, sizeof a, pp, 2, "  ");
  pad_int(b, sizeof b, max, 2, "  ");
  snprintf(out, n, "%s/%s", a, b);
}

static void draw_moves(float px, float py, bool main) {
  const menu_mon* m = &S.mon;
  float mx = px + 5, my = py + 26;
  bool learn = S.a.mode == SUMMARY_LEARN_MOVE;
  mui_image("ui/text_images/en/summary/summary_moves_moves_title", NULL, mx + 2, my - 22, 0, 0.5f);

  if (main ? S.extra_row : learn) {
    float ey = my + 64;
    mui_image("ui/summary_moves_overlay_row", NULL, mx - 2, ey + 1, 0, 1);
    if (learn) {
      menu_move nm = {S.a.new_move, 0, 0, S.a.new_move_type >= 0, S.a.new_move_type};
      const char* tf = type_frame(move_type(&nm));
      if (tf) mui_image("types", tf, mx, ey, 0, 1);
      mui_image("ui/text_images/en/summary/summary_moves_overlay_pp", NULL, mx + 177, ey - 5, 1, 0.5f);
      int pp = S.a.new_move < PR_MOVE_COUNT ? pr_moves[S.a.new_move].pp : 0;
      char t[40];
      pp_text(t, sizeof t, pp, pp);
      mui_text_o(TS_WINDOW, mx + 178, ey + 1, 0, 1, t);
    }
    mui_text_o(learn ? TS_SUMMARY_PINK : TS_SUMMARY, mx + 35, ey, 0, 1,
               learn ? menu_move_name(S.a.new_move) : mui_t("pokemonSummary:cancel"));
  }
  mui_image("ui/text_images/en/summary/summary_moves_descriptions_title", NULL, mx + 2, my + 69, 0, 0.5f);

  for (int i = 0; i < 4; i++) {
    float ry = my + 16 * i;
    const menu_move* mv = i < m->nmoves ? &m->moves[i] : NULL;
    if (mv) {
      const char* tf = type_frame(move_type(mv));
      if (tf) mui_image("types", tf, mx, ry, 0, 1);
    }
    mui_text_o(TS_SUMMARY, mx + 35, ry, 0, 1, mv ? menu_move_name(mv->id) : "-");
    mui_image("ui/text_images/en/summary/summary_moves_overlay_pp", NULL, mx + 177, ry - 5, 1, 0.5f);
    char t[40] = "--/--";
    if (mv) pp_text(t, sizeof t, mv->pp_max - mv->pp_used, mv->pp_max);
    mui_text_o(TS_WINDOW, mx + 178, ry + 1, 0, 1, t);
  }

  if (!main) return;
  if (S.move_desc[0]) {
    float dy = S.move_desc_y - scroll_offset(S.now - S.move_desc_t0, S.move_desc_lines, 4);
    mui_clip(112, 121, 205, 59);
    mui_text(TS_WINDOW_ALT, mx + 1, my + dy, S.move_desc);
    mui_clip(0, 0, MUI_W, MUI_H);
  }
  if (S.cursor_obj) {
    if (S.selected_move > -1)
      mui_image("ui/summary_moves_cursor", "select", mx - 2, my + 16 * S.selected_move + 1, 0, 1);
    /* blinks: hidden for 100 ms every 600 ms */
    float bt = fmodf(S.now - S.blink_t0, 600);
    bool hidden = S.now - S.blink_t0 >= 600 && bt < 100;
    if (!hidden) mui_image("ui/summary_moves_cursor", "highlight", mx - 2, my + 16 * S.move_cursor + 1, 0, 1);
  }
}

static void draw_page(int page, float px, bool main) {
  const char* key = page_key(page);
  mui_image(key, NULL, px, MUI_H, 0, 1);
  float py = MUI_H - frame_h(key);
  switch (page) {
    case SUMMARY_PROFILE: draw_profile(px, py, main); break;
    case SUMMARY_STATS: draw_stats(px, py, main); break;
    case SUMMARY_MOVES: draw_moves(px, py, main); break;
  }
}

/* ------------------------------------------------------------ cursor */

static const menu_move* selected_move(menu_move* scratch) {
  if (S.cursor != SUMMARY_MOVES) return NULL;
  if (S.move_cursor < 4 && S.move_cursor < S.mon.nmoves) return &S.mon.moves[S.move_cursor];
  if (S.a.mode == SUMMARY_LEARN_MOVE && S.move_cursor == 4) {
    *scratch = (menu_move){S.a.new_move, 0, 0, S.a.new_move_type >= 0, S.a.new_move_type};
    return scratch;
  }
  return NULL;
}

static void show_move_select(void);

static bool set_cursor(int c) {
  bool changed = S.move_cursor != c;
  if (S.move_select) {
    S.move_cursor = c;
    menu_move scratch;
    const menu_move* sel = selected_move(&scratch);
    if (sel) {
      const pr_move* pm = sel->id < PR_MOVE_COUNT ? &pr_moves[sel->id] : &pr_moves[0];
      S.move_desc_y = 74;
      if (pm->power >= 0)
        snprintf(S.power, sizeof S.power, "%d", pm->power);
      else
        snprintf(S.power, sizeof S.power, "---");
      if (pm->accuracy >= 0)
        snprintf(S.accuracy, sizeof S.accuracy, "%d", pm->accuracy);
      else
        snprintf(S.accuracy, sizeof S.accuracy, "---");
      S.category = pm->category;
      show_move_effect(false);
    } else {
      hide_move_effect(false);
    }
    S.move_desc_lines =
        mui_wrap(TS_WINDOW_ALT, sel ? move_effect(sel->id) : "", 1252 / 6.0f, S.move_desc, sizeof S.move_desc);
    S.move_desc_t0 = S.now;
    S.cursor_obj = true;
    S.blink_t0 = S.now;
    return changed;
  }
  changed = S.cursor != c;
  if (changed) {
    bool forward = S.cursor < c;
    S.cursor = c;
    if (S.page_visible) {
      S.transitioning = true;
      S.trans_forward = forward;
      S.trans_page = forward ? c : c + 1;
      if (forward) {
        S.trans_x = (tween){SLIDE, SLIDE, -1, 0, 0};
        tween_to(&S.trans_x, 0, 250, 0);
      } else {
        populate_main(c);
        S.trans_x = (tween){0, 0, -1, 0, 0};
        tween_to(&S.trans_x, SLIDE, 250, 0);
      }
    } else {
      populate_main(c);
    }
  }
  return changed;
}

static void transition_done(void) {
  if (S.trans_forward) {
    populate_main(S.cursor);
    if (S.cursor == SUMMARY_MOVES) {
      S.cursor_obj = false;
      show_move_select();
      show_move_effect(false);
    }
  }
  S.transitioning = false;
}

static void destroy_blink_cursor(void) {
  S.cursor_obj = false;
  S.selected_move = -1;
}

static void show_move_select(void) {
  S.move_select = true;
  S.extra_row = true;
  S.selected_move = -1;
  set_cursor(S.a.mode == SUMMARY_LEARN_MOVE ? 4 : 0);
  show_move_effect(false);
}

static void hide_move_select(void) {
  if (S.a.mode == SUMMARY_LEARN_MOVE) {
    if (S.ev.on_move_select) S.ev.on_move_select(S.ev.ctx, 4);
    return;
  }
  S.move_select = false;
  S.extra_row = false;
  S.move_desc[0] = 0;
  destroy_blink_cursor();
  hide_move_effect(false);
}

/* ------------------------------------------------------------ open / close */

/* show(): `instant` is upstream's !fromSummary (the status box appears at
 * once when the party screen opens the summary, slides otherwise). */
static void show(int index, summary_page page, bool instant) {
  S.index = index;
  S.mon = S.a.party[index];
  S.now = 0;
  S.cursor = -1;
  S.page_visible = false;
  S.move_select = false;
  S.move_cursor = 0;
  S.selected_move = -1;
  S.cursor_obj = false;
  S.extra_row = false;
  menu_sprite_free(&S.sprite);
  menu_sprite_load(&S.sprite, &S.mon, false);
  S.sprite_t0 = S.now;
  char cry[48];
  menu_cry_key(&S.mon, cry, sizeof cry);
  mui_sfx(cry);

  if (S.a.mode == SUMMARY_DEFAULT) {
    hide_move_effect(true);
    set_cursor(page);
  } else {
    show_move_effect(true);
    set_cursor(SUMMARY_MOVES);
    show_move_select();
  }
  if (S.mon.status || S.mon.pokerus)
    show_status(instant);
  else
    hide_status(instant);
}

void summary_open(const summary_args* args, const summary_events* ev) {
  menu_sprite_free(&S.sprite);
  memset(&S, 0, sizeof S);
  S.a = *args;
  S.ev = *ev;
  S.active = true;
  show(args->index, args->mode == SUMMARY_DEFAULT ? args->page : SUMMARY_MOVES, args->mode == SUMMARY_DEFAULT);
}

void summary_close(void) {
  if (!S.active) return;
  menu_sprite_free(&S.sprite);
  S.active = false;
}

bool summary_active(void) { return S.active; }

/* ------------------------------------------------------------ input */

bool summary_input(mui_button b) {
  if (!S.active || S.transitioning) return false;
  bool success = false, error = false;
  bool learn = S.a.mode == SUMMARY_LEARN_MOVE;

  if (S.move_select) {
    if (b == MB_ACTION) {
      if (S.move_cursor < S.mon.nmoves) {
        if (learn) {
          mui_sfx("ui/select");
          if (S.ev.on_move_select) S.ev.on_move_select(S.ev.ctx, S.move_cursor);
          return true;
        } else if (S.selected_move == -1) {
          S.selected_move = S.move_cursor;
          set_cursor(S.move_cursor);
        } else {
          if (S.selected_move != S.move_cursor) {
            menu_move t = S.mon.moves[S.selected_move];
            S.mon.moves[S.selected_move] = S.mon.moves[S.move_cursor];
            S.mon.moves[S.move_cursor] = t;
            if (S.ev.on_move_swap) S.ev.on_move_swap(S.ev.ctx, S.index, S.selected_move, S.move_cursor);
          }
          S.selected_move = -1;
        }
        success = true;
      } else if (S.move_cursor == 4) {
        return summary_input(MB_CANCEL);
      } else {
        error = true;
      }
    } else if (b == MB_CANCEL) {
      mui_sfx("ui/select");
      hide_move_select(); /* in LEARN_MOVE this reports 4 and may close the screen */
      return true;
    } else {
      switch (b) {
        case MB_UP: success = set_cursor(S.move_cursor ? S.move_cursor - 1 : 4); break;
        case MB_DOWN: success = set_cursor(S.move_cursor < 4 ? S.move_cursor + 1 : 0); break;
        case MB_LEFT:
          S.move_select = false;
          set_cursor(SUMMARY_STATS);
          if (learn) {
            hide_move_effect(false);
            destroy_blink_cursor();
          } else {
            hide_move_select();
          }
          success = true;
          break;
        default: break;
      }
    }
  } else if (b == MB_ACTION) {
    if (S.cursor == SUMMARY_MOVES) {
      show_move_select();
      success = true;
    } else if (S.cursor == SUMMARY_PROFILE && S.mon.has_passive) {
      S.show_passive = !S.show_passive;
      S.abil_t0 = S.now; /* the other description's scroll starts over */
    } else if (S.cursor == SUMMARY_STATS) {
      S.show_ivs = !S.show_ivs;
    }
  } else if (b == MB_CANCEL) {
    mui_sfx("ui/select");
    if (learn) {
      hide_move_select();
    } else {
      summary_close();
      if (S.ev.on_close) S.ev.on_close(S.ev.ctx);
    }
    return true;
  } else {
    switch (b) {
      case MB_UP:
      case MB_DOWN: {
        if (learn || !S.a.from_party) break;
        bool down = b == MB_DOWN;
        if ((down && S.index < S.a.n - 1) || (!down && S.index)) {
          show(S.index + (down ? 1 : -1), S.cursor, false);
          success = true;
        }
        break;
      }
      case MB_LEFT:
        if (S.cursor) success = set_cursor(S.cursor - 1);
        break;
      case MB_RIGHT:
        if (S.cursor < SUMMARY_MOVES) {
          success = set_cursor(S.cursor + 1);
          if (learn && S.cursor == SUMMARY_MOVES) S.move_select = true;
        }
        break;
      default: break;
    }
  }
  if (success)
    mui_sfx("ui/select");
  else if (error)
    mui_sfx("ui/error");
  return success || error;
}

/* ------------------------------------------------------------ update / draw */

void summary_update(float dt_ms) {
  tween_step(&g_status_x, dt_ms);
  tween_step(&g_effect_x, dt_ms);
  if (!S.active) return;
  S.now += dt_ms;
  if (S.transitioning) {
    tween_step(&S.trans_x, dt_ms);
    if (S.trans_x.t < 0) transition_done();
  }
}

static const char* status_frame(int s) {
  static const char* const names[] = {NULL, "poison", "toxic", "paralysis", "sleep", "freeze", "burn", "faint"};
  return s > 0 && s <= MS_FAINT ? names[s] : NULL;
}

void summary_draw(void) {
  if (!S.active) return;
  const menu_mon* m = &S.mon;
  mui_clip(0, 0, MUI_W, MUI_H);

  mui_image("ui/summary_bg", NULL, 0, MUI_H, 0, 1);
  int bg_h = frame_h("ui/summary_bg");
  char tab[64];
  snprintf(tab, sizeof tab, "ui/text_images/en/summary/summary_tabs_%d", (S.cursor < 0 ? 0 : S.cursor) + 1);
  mui_image(tab, NULL, 81, Y(-bg_h + 16), 0, 1);
  mui_text_o(TS_SUMMARY_HEADER, 4, Y(-165), 0, 1, mui_t("pokemonSummary:pokemonInfo"));
  mui_image("ui/text_images/en/summary/summary_dexnb_label", NULL, 6, Y(-151), 0, 1);

  bool shiny = m->shiny || (m->fusion && m->fusion_shiny);
  if (shiny) mui_image("ui/text_images/en/summary/summary_dexnb_label_overlay_shiny", NULL, 6, Y(-54), 0, 1);
  char num[8];
  pad_int(num, sizeof num, m->species % 2000, 4, "0");
  mui_style ns = shiny ? TS_SUMMARY_GOLD : TS_SUMMARY;
  {
    int w = mui_text_width(TS_SUMMARY_DEX_NUM, num);
    float h = mui_text_height(TS_SUMMARY_DEX_NUM, num);
    mui_text_c(TS_SUMMARY_DEX_NUM, 41 - w, Y(-149) - h, num, mui_style_color(ns, false), mui_style_color(ns, true));
  }

  const rr_frame* sf = menu_sprite_frame(&S.sprite, (uint32_t)(S.now - S.sprite_t0));
  if (sf) {
    rr_draw_opts o = RR_DEFAULT_OPTS;
    o.origin_x = o.origin_y = 0.5f;
    rr_draw(sf, 56, Y(-106), &o);
  }

  int name_w = mui_text(TS_SUMMARY, 6, Y(-53), m->name);
  float icon_x = 6 + name_w + 1;
  if (m->fusion) {
    const rr_frame* f = mui_frame("ui/icon_spliced", NULL);
    rr_draw_opts o = RR_DEFAULT_OPTS;
    o.scale_x = o.scale_y = 0.75f;
    if (f) {
      rr_draw(f, 6 + name_w + 2, Y(-50), &o);
      icon_x += f->src_w * 0.75f + 1;
    }
  }
  bool double_shiny = m->fusion && m->shiny && m->fusion_shiny;
  if (shiny) {
    int fus_tier = 0;
    int tier = summary_mon_shiny_tier(m, &fus_tier);
    int big = double_shiny ? (m->variant < 3 ? m->variant : 0)
                           : (tier > 0 ? tier - 1 : 0);
    rr_draw_opts o = RR_DEFAULT_OPTS;
    o.scale_x = o.scale_y = 0.75f;
    o.tint = MUI_RGB(variant_tint(big));
    mui_image_ex(double_shiny ? "ui/shiny_1" : "ui/shiny", NULL, icon_x, Y(-50), &o);
    if (double_shiny) {
      o.tint = MUI_RGB(variant_tint(fus_tier > 0 ? fus_tier - 1 : 0));
      mui_image_ex("ui/shiny_2", NULL, icon_x, Y(-50), &o);
    }
  }

  static const char* const balls[] = {"pb", "gb", "ub", "rb", "mb", "lb"};
  mui_image("pb", balls[m->pokeball < 6 ? m->pokeball : 0], 6, Y(-19), 0, 1);

  /* candy (starter friendship fills it) and friendship */
  {
    rr_draw_opts o = RR_DEFAULT_OPTS;
    o.origin_x = o.origin_y = 0.5f;
    o.scale_x = o.scale_y = 0.8f;
    o.tint = MUI_RGB(m->candy_colors[0]);
    mui_image_ex("ui/candy", NULL, 13, Y(-140), &o);
    o.tint = MUI_RGB(m->candy_colors[1]);
    mui_image_ex("ui/candy_overlay", NULL, 13, Y(-140), &o);
    o.tint = RR_RGBA(0, 0, 0, 128);
    const rr_frame* c = mui_frame("ui/candy", NULL);
    float cap = m->friendship_cap ? m->friendship_cap : 1;
    int crop = (int)(16 - 16 * (m->starter_friendship / cap));
    if (c && crop > 0) rr_draw_cropped(c, 13, Y(-140), 16, crop, &o);
    char t[16];
    snprintf(t, sizeof t, "\xc3\x97%u", m->candy_count);
    mui_style cs = m->candy_count >= 9999 ? TS_SUMMARY_GOLD : TSX_WINDOW_ALT_76;
    mui_text_c(TSX_WINDOW_ALT_76, 20, Y(-146), t, mui_style_color(cs, false), mui_style_color(cs, true));

    o.tint = RR_RGBA(255, 255, 255, 255);
    mui_image_ex("ui/friendship", NULL, 13, Y(-60), &o);
    mui_image_ex("ui/friendship_overlay", NULL, 13, Y(-60), &o);
    o.tint = RR_RGBA(0, 0, 0, 128);
    const rr_frame* f = mui_frame("ui/friendship", NULL);
    crop = (int)(16 - 16 * (m->friendship / 255.0f));
    if (f && crop > 0) rr_draw_cropped(f, 13, Y(-60), 16, crop, &o);
    snprintf(t, sizeof t, " %u/255", m->friendship);
    mui_text(TSX_WINDOW_ALT_76, 20, Y(-66), t);
  }
  if (m->champion) {
    rr_draw_opts o = RR_DEFAULT_OPTS;
    o.scale_x = o.scale_y = 1.25f;
    mui_image_ex("ui/champion_ribbon", NULL, 88, Y(-146), &o);
  }

  char lv[24];
  snprintf(lv, sizeof lv, "%s%u", mui_t("pokemonSummary:lv"), m->level);
  mui_text_o(TS_SUMMARY_ALT, 24, Y(-17), 0, 1, lv);
  if (m->gender != MG_GENDERLESS) {
    const char* sym = m->gender == MG_FEMALE ? "\xe2\x99\x80" : "\xe2\x99\x82";
    rr_color c = m->gender == MG_FEMALE ? MUI_RGB(0xf89890) : MUI_RGB(0x40c8f8);
    rr_color sh = m->gender == MG_FEMALE ? MUI_RGB(0x984038) : MUI_RGB(0x006090);
    mui_text_c(TS_SUMMARY, 96, Y(-17) - mui_text_height(TS_SUMMARY, sym), sym, c, sh);
  }

  /* status box (slides out from under the left edge) */
  float sx = tween_value(&g_status_x);
  if (sx > -106) {
    mui_image("ui/summary_status", NULL, sx, Y(-16), 0, 0);
    mui_text(TS_SUMMARY, sx + 3, Y(-16), mui_t("pokemonSummary:status"));
    const char* st = m->status ? status_frame(m->status) : m->pokerus ? "pokerus" : NULL;
    if (st) mui_image("statuses", st, sx + 91, Y(-16) + 4, 0.5f, 0);
  }

  /* move effect box (slides out from under the page) */
  float ex = tween_value(&g_effect_x);
  if (ex < 106) {
    float ey = Y(-62);
    mui_image("ui/summary_moves_effect", NULL, ex, ey, 0, 0);
    mui_image("ui/text_images/en/summary/summary_moves_effect_title", NULL, ex + 7, ey + 7, 0, 0.5f);
    const char* labels = mui_t("pokemonSummary:powerAccuracyCategory");
    mui_text_ls(TS_SUMMARY, ex + 8, ey + 13, 9, labels, mui_style_color(TS_SUMMARY, false),
                mui_style_color(TS_SUMMARY, true));
    mui_text_o(TS_WINDOW_ALT, ex + 99, ey + 27, 1, 1, S.power);
    mui_text_o(TS_WINDOW_ALT, ex + 99, ey + 43, 1, 1, S.accuracy);
    static const char* const cats[] = {"physical", "special", "status"};
    mui_image("categories", cats[S.category >= 0 && S.category < 3 ? S.category : 0], ex + 99, ey + 57, 1, 1);
  }

  if (S.page_visible) draw_page(S.main_page, PAGE_X, true);
  if (S.transitioning) draw_page(S.trans_page, PAGE_X + tween_value(&S.trans_x), false);
  mui_clip_off();
}
