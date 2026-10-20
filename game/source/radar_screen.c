/* The Nemesis Radar (see radar_screen.h). */
#include "radar_screen.h"

#include <stdio.h>
#include <string.h>

#include "gm_enum_names.h"
#include "i18n.h"
#include "radar_assets.h"
#include "run_text.h"
#include "ui.h"
#include "ui_text.h"
#include "ui_widgets.h"

#define ROW_H 28
#define VISIBLE 5            /* records on the bottom screen */
#define LIST_Y 40
#define LINE_LEN 112
#define DETAIL_MAX 40        /* wrapped lines on the Scout page */
#define DETAIL_VISIBLE 12
#define DETAIL_Y 34
#define DETAIL_H 14
#define TEXT_W 296           /* the width small text is cut or wrapped to */
#define MORE_X 200           /* the "N more above" / "N more below" notes (drawn at x 232) and the y bands a tap on them takes */
#define MORE_UP_Y0 24
#define MORE_UP_Y1 42
#define MORE_DOWN_Y0 186
#define MORE_DOWN_Y1 208
#define SPRITE_DELAY_MS 140.0f   /* the cursor has to rest this long before a sprite is read from the SD card */

/* The hints on the bottom line, one text each so a tap on one is the button it names (the first of the list's, "A: scout" and "B: back", and
 * the Scout page's "B: back to the list"; the direction hints are only said). */
static const char* const LIST_HINTS[4] = {"A: scout", "B: back", "^/v: choose", "</>: page"};
static const char* const BACK_HINTS[1] = {"B: back"};
static const char* const DETAIL_HINTS[2] = {"B: back to the list", "^/v: scroll"};
#define HINT_X 10
#define HINT_Y 224

typedef enum { MODE_LIST, MODE_DETAIL } rd_mode;
enum { TONE_NORMAL, TONE_HEAD, TONE_NOTE, TONE_DIM };

static struct {
  void (*on_exit)(void*);            /* the only things kept of radar_args: the owner's data is copied below, never pointed at */
  void* exit_arg;
  bool usable;                       /* the custom file could be read */
  char why[RADAR_WHY_LEN];
  cc_roster roster;                  /* a copy: the owner's data can go away while this screen is up */
  cc_schedule sched;                 /* a copy, valid iff has_sched */
  bool has_sched;
  bool left;                         /* on_exit was called: the screen takes no more input */
  rd_mode mode;
  int sel;                           /* index into the list, 0 = newest */
  int scroll;                        /* first visible list row */
  int dscroll;                       /* first visible Scout line */
  int n_detail;
  char detail[DETAIL_MAX][LINE_LEN];
  uint8_t dtone[DETAIL_MAX];
  uint32_t sprite_id;                /* record the loaded sprite (or the failed load) belongs to */
  bool sprite_ok;
  float sprite_wait;
  float sprite_t0;
} R;

/* ------------------------------------------------------------------ the words */

static const char* life_tag(uint8_t life) {
  switch (life) {
    case CC_LIFE_STORED: return "Remembered";
    case CC_LIFE_SCHEDULED: return "Held";
    case CC_LIFE_CHALLENGED: return "Challenged";
    case CC_LIFE_DEFEATED: return "Beaten";
    case CC_LIFE_REWARD_CLAIMED: return "Claimed";
    default: return "?";
  }
}

static const char* skip_word(uint8_t reason) {
  switch (reason) {
    case CC_SKIP_TRAINER: return "a trainer battle";
    case CC_SKIP_FIXED_OR_SCRIPTED: return "a fixed encounter";
    case CC_SKIP_MYSTERY: return "a mystery encounter";
    case CC_SKIP_BOSS: return "a boss";
    case CC_SKIP_FINAL: return "the final wave";
    case CC_SKIP_UNSUPPORTED: return "an unsupported battle";
    default: return "an unsuitable battle";
  }
}

/* The window `i` of a schedule as a phrase ("waiting", "skipped: a boss"). */
static void window_phrase(const cc_window* w, char* out, size_t n) {
  switch (w->state) {
    case CC_WIN_PENDING: snprintf(out, n, "waiting"); break;
    case CC_WIN_SKIPPED: snprintf(out, n, "skipped: %s", skip_word(w->reason)); break;
    case CC_WIN_OFFERED: snprintf(out, n, "offered"); break;
    case CC_WIN_DECLINED: snprintf(out, n, "declined"); break;
    case CC_WIN_ACCEPTED: snprintf(out, n, "accepted"); break;
    default: snprintf(out, n, "?"); break;
  }
}

static int window_in_state(const cc_schedule* s, uint8_t state) {
  for (int i = 0; i < CC_NUM_WINDOWS; i++)
    if (s->windows[i].state == state) return i;
  return -1;
}

void radar_rematch_text(const cc_nemesis* n, const cc_schedule* s, char* out, size_t len) {
  if (len == 0) return;
  out[0] = 0;
  if (!n) return;
  if (n->life == CC_LIFE_REWARD_CLAIMED) {
    snprintf(out, len, "Rematch won. The reward was claimed.");
    return;
  }
  if (n->life == CC_LIFE_DEFEATED) {
    snprintf(out, len, "Rematch won. The reward is not claimed yet.");
    return;
  }
  const bool named = s && s->chosen_id != 0 && s->chosen_id == n->record_id;
  if (named) {
    const bool held = n->life == CC_LIFE_SCHEDULED || n->life == CC_LIFE_CHALLENGED;
    const bool needs_hold = s->state == CC_SCHED_ACTIVE || s->state == CC_SCHED_OFFERED || s->state == CC_SCHED_ACCEPTED;
    if (needs_hold && !held) {   /* the plan and the record contradict each other: show neither as true */
      snprintf(out, len, "The saved plan and this record disagree. No rematch is shown as scheduled.");
      return;
    }
    int w;
    switch (s->state) {
      case CC_SCHED_ACTIVE:
        w = window_in_state(s, CC_WIN_PENDING);
        if (w >= 0)
          snprintf(out, len, "Planned for this run. The next window is wave %u.", (unsigned)s->windows[w].wave);
        else
          snprintf(out, len, "Planned for this run.");
        return;
      case CC_SCHED_OFFERED:
        w = window_in_state(s, CC_WIN_OFFERED);
        snprintf(out, len, "Offered at wave %u.", (unsigned)(w >= 0 ? s->windows[w].wave : 0));
        return;
      case CC_SCHED_ACCEPTED:
        w = window_in_state(s, CC_WIN_ACCEPTED);
        snprintf(out, len, "Accepted at wave %u.", (unsigned)(w >= 0 ? s->windows[w].wave : 0));
        return;
      case CC_SCHED_DECLINED: snprintf(out, len, "Declined. Nothing more is offered this run."); return;
      case CC_SCHED_EXHAUSTED: snprintf(out, len, "Not offered this run: every window was skipped."); return;
      default: break;
    }
  }
  if (n->life == CC_LIFE_SCHEDULED || n->life == CC_LIFE_CHALLENGED)
    snprintf(out, len, "Held by a run whose plan is not loaded here.");
  else
    snprintf(out, len, "No rematch is scheduled.");
}

/* Cuts s so that it fits `w` px in `style`, with ".." at the end. */
static void fit(char* s, size_t cap, text_style style, float w) {
  if (text_width(style, s, NULL) <= w) return;
  size_t len = strlen(s);
  if (len > cap - 3) len = cap - 3;
  while (len > 3) {
    len--;
    while (len > 0 && ((unsigned char)s[len] & 0xC0) == 0x80) len--; /* not in the middle of a UTF-8 character */
    char t[LINE_LEN];
    snprintf(t, sizeof t, "%.*s..", (int)len, s);
    if (text_width(style, t, NULL) <= w) {
      memcpy(s, t, strlen(t) + 1);
      return;
    }
  }
}

static const cc_nemesis* rec_at(int list_index) {
  if (list_index < 0 || list_index >= R.roster.count) return NULL;
  return &R.roster.rec[R.roster.count - 1 - list_index];
}
static const cc_nemesis* sel_rec(void) { return rec_at(R.sel); }

static const char* biome_text(int biome, char* out, int n) {
  rt_biome_name(biome, out, n);
  if (!out[0]) snprintf(out, (size_t)n, "an unknown place");
  return out;
}

/* "Held items" text: one item as "Leftovers x2" (the game's name when it has one, else the key made readable). */
static void item_text(const cc_item* it, char* out, size_t n) {
  char key[96], name[64];
  snprintf(key, sizeof key, "modifierType:ModifierType.%s.name", it->key);
  if (i18n_exists(key)) {
    i18n_t(name, sizeof name, key, NULL);
  } else {
    size_t k = 0;
    for (const char* p = it->key; *p && k + 1 < sizeof name; p++, k++) {
      char c = *p == '_' ? ' ' : *p;
      name[k] = (k > 0 && c >= 'A' && c <= 'Z' && name[k - 1] != ' ') ? (char)(c + 32) : c;
    }
    name[k] = 0;
  }
  if (it->stack > 1)
    snprintf(out, n, "%s x%u", name, (unsigned)it->stack);
  else
    snprintf(out, n, "%s", name);
}

/* ------------------------------------------------------------------ the Scout page */

static void add_line(const char* text, uint8_t tone) {
  if (R.n_detail >= DETAIL_MAX) return;
  snprintf(R.detail[R.n_detail], LINE_LEN, "%s", text);
  R.dtone[R.n_detail++] = tone;
}

/* A paragraph, wrapped to the page width; `lead` is on its first line. */
static void add_para(const char* text, uint8_t tone) {
  text_opts o = text_defaults();
  o.wrap_w = TEXT_W;
  char wrapped[512];
  text_wrap(TS_INSTRUCTIONS_TEXT, text, &o, wrapped, sizeof wrapped);
  for (char* line = wrapped; line && *line;) {
    char* nl = strchr(line, '\n');
    char one[LINE_LEN];
    snprintf(one, sizeof one, "%.*s", nl ? (int)(nl - line) : (int)strlen(line), line);
    add_line(one, tone);
    line = nl ? nl + 1 : NULL;
  }
}

static void build_detail(void) {
  R.n_detail = 0;
  R.dscroll = 0;
  const cc_nemesis* n = sel_rec();
  if (!n) return;
  char t[320], a[96], b[96];
  const bool female = n->gender == 1;
  snprintf(t, sizeof t, "%s, level %u%s%s", i18n_species_name(n->species), (unsigned)n->orig_level, n->shiny ? ", shiny" : "",
           n->gender < 0 ? "" : (female ? ", female" : ", male"));
  add_para(t, TONE_HEAD);
  snprintf(t, sizeof t, "Ended a run at wave %u in %s.", (unsigned)n->source_wave, biome_text(n->source_biome, a, sizeof a));
  add_para(t, TONE_NORMAL);
  if (n->known & CC_NK_FALLEN)
    snprintf(t, sizeof t, "Last of yours to fall: %s.", i18n_species_name(n->fallen_species));
  else
    snprintf(t, sizeof t, "Last of yours to fall: not recorded.");
  add_para(t, TONE_NORMAL);
  if (n->known & CC_NK_FINISH_MOVE)
    snprintf(t, sizeof t, "Final move: %s.", ra_move_name(n->finish_move));
  else
    snprintf(t, sizeof t, "Final move: not recorded.");
  add_para(t, TONE_NORMAL);

  if (n->known & CC_NK_ABILITY)
    snprintf(t, sizeof t, "Ability: %s.", i18n_ability_name(n->ability));
  else
    snprintf(t, sizeof t, "Ability: not recorded.");
  add_para(t, TONE_NORMAL);
  if (n->known & CC_NK_PASSIVE) {
    if (n->passive)
      snprintf(t, sizeof t, "Passive: %s.", i18n_ability_name(n->passive));
    else
      snprintf(t, sizeof t, "Passive: none.");
    add_para(t, TONE_NORMAL);
  }
  if (n->known & CC_NK_NATURE)
    snprintf(t, sizeof t, "Nature: %s.", ra_nature_name(n->nature, a, sizeof a));
  else
    snprintf(t, sizeof t, "Nature: not recorded.");
  add_para(t, TONE_NORMAL);

  if (n->known & CC_NK_MOVES) {
    snprintf(t, sizeof t, "Moves:");
    for (int i = 0; i < n->n_moves; i++) {
      const size_t k = strlen(t);
      snprintf(t + k, sizeof t - k, "%s %s", i ? "," : "", ra_move_name(n->moves[i]));
    }
    strncat(t, ".", sizeof t - strlen(t) - 1);
  } else {
    snprintf(t, sizeof t, "Moves: not recorded.");
  }
  add_para(t, TONE_NORMAL);

  if (n->known & CC_NK_IVS) {
    snprintf(t, sizeof t, "IVs: HP %u, Atk %u, Def %u, SpA %u, SpD %u, Spe %u.", (unsigned)n->ivs[0], (unsigned)n->ivs[1],
             (unsigned)n->ivs[2], (unsigned)n->ivs[3], (unsigned)n->ivs[4], (unsigned)n->ivs[5]);
  } else {
    snprintf(t, sizeof t, "IVs: not recorded.");
  }
  add_para(t, TONE_NORMAL);

  if (n->known & CC_NK_LOADOUT) {
    if (n->n_items == 0) {
      snprintf(t, sizeof t, "Held items: none.");
    } else {
      snprintf(t, sizeof t, "Held items:");
      for (int i = 0; i < n->n_items; i++) {
        item_text(&n->items[i], b, sizeof b);
        const size_t k = strlen(t);
        snprintf(t + k, sizeof t - k, "%s %s", i ? "," : "", b);
      }
      strncat(t, (n->known & CC_NK_LOADOUT_TRUNCATED) ? ", and more." : ".", sizeof t - strlen(t) - 1);
    }
  } else {
    snprintf(t, sizeof t, "Held items: not recorded.");
  }
  add_para(t, TONE_NORMAL);

  radar_rematch_text(n, R.has_sched ? &R.sched : NULL, t, sizeof t);
  add_para(t, TONE_NOTE);
  if (R.has_sched && R.sched.chosen_id == n->record_id && R.sched.chosen_id != 0) {
    add_line("This run's windows:", TONE_HEAD);
    for (int i = 0; i < CC_NUM_WINDOWS; i++) {
      window_phrase(&R.sched.windows[i], a, sizeof a);
      snprintf(t, sizeof t, "Wave %u: %s", (unsigned)R.sched.windows[i].wave, a);
      add_line(t, TONE_NORMAL);
    }
  }
}

/* ------------------------------------------------------------------ input */

static void select_to(int i) {
  if (i < 0) i = 0;
  if (i > R.roster.count - 1) i = R.roster.count - 1;
  if (i < 0 || i == R.sel) return;
  R.sel = i;
  if (R.sel < R.scroll) R.scroll = R.sel;
  if (R.sel >= R.scroll + VISIBLE) R.scroll = R.sel - VISIBLE + 1;
  R.sprite_wait = 0;
  ui_play_select();
}

static bool rd_input(ui_screen* s, ui_button b) {
  (void)s;
  if (R.left) return true;
  if (R.mode == MODE_DETAIL) {
    switch (b) {
      case BTN_UP:
        if (R.dscroll > 0) R.dscroll--;
        return true;
      case BTN_DOWN:
        if (R.dscroll < R.n_detail - DETAIL_VISIBLE) R.dscroll++;
        return true;
      case BTN_CANCEL:
        ui_play_select();
        R.mode = MODE_LIST;
        return true;
      default:
        return false;
    }
  }
  switch (b) {
    case BTN_UP: select_to(R.sel - 1); return true;
    case BTN_DOWN: select_to(R.sel + 1); return true;
    case BTN_LEFT: select_to(R.sel - VISIBLE); return true;
    case BTN_RIGHT: select_to(R.sel + VISIBLE); return true;
    case BTN_ACTION:
      if (!R.usable || R.roster.count == 0) {
        ui_play_error();
        return true;
      }
      ui_play_select();
      build_detail();
      R.mode = MODE_DETAIL;
      return true;
    case BTN_CANCEL:
      ui_play_select();
      R.left = true;
      if (R.on_exit) R.on_exit(R.exit_arg);
      return true;
    default:
      return false;
  }
}

/* A tap on the bottom screen (physical px). The hints on the bottom line that name a button press it. The list: a tap on a record points at it
 * and a second tap on the pointed one scouts it, like the save slots; the "more above" and "more below" notes page the list as </> do. The Scout
 * page: the upper half of the text scrolls it up by half a page, the lower half down. */
static bool rd_touch(ui_screen* s, float x, float y) {
  if (R.left) return true;
  if (y >= HINT_Y - 8) {
    if (R.mode == MODE_DETAIL) return hint_at(TS_INSTRUCTIONS_TEXT, HINT_X, DETAIL_HINTS, 2, x) == 0 ? rd_input(s, BTN_CANCEL) : false;
    if (!R.usable || R.roster.count == 0) return hint_at(TS_INSTRUCTIONS_TEXT, HINT_X, BACK_HINTS, 1, x) == 0 ? rd_input(s, BTN_CANCEL) : false;
    const int h = hint_at(TS_INSTRUCTIONS_TEXT, HINT_X, LIST_HINTS, 4, x);
    return h == 0 ? rd_input(s, BTN_ACTION) : (h == 1 ? rd_input(s, BTN_CANCEL) : false); /* the direction hints are only said */
  }
  if (R.mode == MODE_DETAIL) {
    const float top = DETAIL_Y - 4, bottom = DETAIL_Y - 4 + DETAIL_VISIBLE * DETAIL_H + 8;
    if (x < 1 || x >= 319 || y < top || y >= bottom) return false;
    const int max = R.n_detail > DETAIL_VISIBLE ? R.n_detail - DETAIL_VISIBLE : 0;
    R.dscroll += y < (top + bottom) / 2 ? -DETAIL_VISIBLE / 2 : DETAIL_VISIBLE / 2;
    if (R.dscroll > max) R.dscroll = max;
    if (R.dscroll < 0) R.dscroll = 0;
    return true;
  }
  for (int i = 0; i < VISIBLE && R.scroll + i < R.roster.count; i++) {
    const float top = LIST_Y + 6 + i * ROW_H - 2;
    if (x < 4 || x >= 316 || y < top || y >= top + ROW_H) continue;
    if (R.scroll + i == R.sel) return rd_input(s, BTN_ACTION);
    select_to(R.scroll + i);
    return true;
  }
  if (x >= MORE_X) {
    if (R.scroll > 0 && y >= MORE_UP_Y0 && y < MORE_UP_Y1) return rd_input(s, BTN_LEFT);
    if (R.scroll + VISIBLE < R.roster.count && y >= MORE_DOWN_Y0 && y < MORE_DOWN_Y1) return rd_input(s, BTN_RIGHT);
  }
  return false;
}

static void rd_update(ui_screen* s, float dt) {
  (void)s;
  const cc_nemesis* n = sel_rec();
  if (R.left || !R.usable || !n) return;
  if (R.sprite_id == n->record_id) return;   /* loaded, or tried and missing: not read again for this record */
  /* the sprite is read from the SD card once the cursor has rested: a held direction button does not load one per record */
  R.sprite_wait += dt;
  if (R.sprite_wait < SPRITE_DELAY_MS) return;
  R.sprite_ok = ra_sprite_load(n->species, n->form, n->gender, n->shiny);
  R.sprite_id = n->record_id;
  R.sprite_t0 = ui_now();
  R.sprite_wait = 0;
}

static void rd_leave(ui_screen* s) {
  (void)s;
  ra_sprite_free();
  R.sprite_ok = false;
}

/* ------------------------------------------------------------------ drawing */

static void line(float x, float y, const char* s, text_style st, rr_color color) {
  text_opts o = text_defaults();
  if (color) o.color = color;
  text_draw(st, x, y, s, &o);
}

static void centred(float x, float y, const char* s, text_style st, rr_color color) {
  text_opts o = text_defaults();
  o.origin_x = 0.5f;
  if (color) o.color = color;
  text_draw(st, x, y, s, &o);
}

/* PokemonType 0..17 -> a colour for the faint tint of the top screen. Colour only: no rule reads it. */
static rr_color type_tint(int type) {
  static const uint8_t rgb[18][3] = {
      {0xa8, 0xa8, 0x78}, {0xc0, 0x30, 0x28}, {0xa8, 0x90, 0xf0}, {0xa0, 0x40, 0xa0}, {0xe0, 0xc0, 0x68}, {0xb8, 0xa0, 0x38},
      {0xa8, 0xb8, 0x20}, {0x70, 0x58, 0x98}, {0xb8, 0xb8, 0xd0}, {0xf0, 0x80, 0x30}, {0x68, 0x90, 0xf0}, {0x78, 0xc8, 0x50},
      {0xf8, 0xd0, 0x30}, {0xf8, 0x58, 0x88}, {0x98, 0xd8, 0xd8}, {0x70, 0x38, 0xf8}, {0x70, 0x58, 0x48}, {0xee, 0x99, 0xac}};
  if (type < 0 || type > 17) return 0;
  return RR_RGBA(rgb[type][0], rgb[type][1], rgb[type][2], 70);
}

static void draw_top_background(int biome) {
  if (ui_space(UI_PHYS_TOP)) rr_rect(0, 0, 400, 240, RR_RGBA(0, 0, 0, 255));
  if (!ui_space(UI_FIELD)) return;
  rr_rect(0, 0, 320, 180, RR_RGBA(0x10, 0x10, 0x18, 255));
  char name[48], key[96];
  gm_enum_lower(GM_E_BIOME_ID, biome, name, sizeof name);
  if (name[0]) {
    snprintf(key, sizeof key, "images/arenas/%s_bg", name);
    if (img_has(key)) img_draw(key, NULL, 0, 0, 0, 0);
  }
}

static void draw_top(void) {
  const cc_nemesis* n = R.usable ? sel_rec() : NULL;
  if (!n) {
    draw_top_background(-1);
    if (!ui_space(UI_FIELD)) return;
    rr_rect(0, 0, 320, 180, RR_RGBA(0, 0, 0, 190));
    centred(160, 14, "Nemesis Radar", TS_SUMMARY, 0);
    if (!R.usable) {
      centred(160, 60, "The custom content file cannot be used.", TS_WINDOW, 0);
      text_opts o = text_defaults();
      o.wrap_w = 280;
      o.origin_x = 0.5f;
      char wrapped[320];
      text_wrap(TS_INSTRUCTIONS_TEXT, R.why[0] ? R.why : "It could not be read.", &o, wrapped, sizeof wrapped);
      text_draw(TS_INSTRUCTIONS_TEXT, 160, 84, wrapped, &o);
      centred(160, 150, "It is left as it is.", TS_INSTRUCTIONS_TEXT, RR_RGBA(0xa0, 0xa0, 0xa0, 255));
    } else {
      centred(160, 66, "No enemy is remembered yet.", TS_WINDOW, 0);
      centred(160, 96, "Only a Custom Run adds enemies,", TS_INSTRUCTIONS_TEXT, RR_RGBA(0xa0, 0xa0, 0xa0, 255));
      centred(160, 108, "and only from defeats it can explain.", TS_INSTRUCTIONS_TEXT, RR_RGBA(0xa0, 0xa0, 0xa0, 255));
    }
    return;
  }
  draw_top_background(n->source_biome);
  if (!ui_space(UI_FIELD)) return;
  int t1 = -1, t2 = -1;
  bool typed = ra_species_types(n->species, n->form, &t1, &t2);
  if (typed && (t1 < 0 || t1 > 17)) typed = false; /* a type outside the eighteen is not named or tinted */
  if (t2 > 17) t2 = -1;
  rr_rect(0, 0, 320, 180, RR_RGBA(0, 0, 0, 150));
  if (typed && type_tint(t1)) rr_rect(0, 0, 320, 180, type_tint(t1));    /* modest, and only for a known type */

  char t[160], a[64];
  snprintf(t, sizeof t, "%s  Lv. %u", i18n_species_name(n->species), (unsigned)n->orig_level);
  fit(t, sizeof t, TS_SUMMARY, 300);
  centred(160, 8, t, TS_SUMMARY, 0);

  if (R.sprite_ok && R.sprite_id == n->record_id && ra_sprite_draw(160, 84, (uint32_t)(ui_now() - R.sprite_t0), 96)) {
    /* drawn */
  } else {
    ra_icon(n->species, n->form, n->gender, n->shiny, 160, 84, 2.0f, 0);
  }

  snprintf(t, sizeof t, "Ended a run at wave %u", (unsigned)n->source_wave);
  centred(160, 136, t, TS_WINDOW, 0);
  snprintf(t, sizeof t, "%s", biome_text(n->source_biome, a, sizeof a));
  if (typed && t1 >= 0) {
    char tn[64];
    snprintf(tn, sizeof tn, "%s", i18n_type_name(t1));
    const size_t k = strlen(t);
    snprintf(t + k, sizeof t - k, "  -  %s%s%s", tn, t2 >= 0 ? "/" : "", t2 >= 0 ? i18n_type_name(t2) : "");
  }
  fit(t, sizeof t, TS_INSTRUCTIONS_TEXT, 300);
  centred(160, 156, t, TS_INSTRUCTIONS_TEXT, RR_RGBA(0xc8, 0xc8, 0xc8, 255));
  if (n->known & CC_NK_FALLEN) {
    snprintf(t, sizeof t, "Last of yours to fall: %s", i18n_species_name(n->fallen_species));
    fit(t, sizeof t, TS_INSTRUCTIONS_TEXT, 300);
    centred(160, 168, t, TS_INSTRUCTIONS_TEXT, RR_RGBA(0xa0, 0xa0, 0xa0, 255));
  }
}

static void draw_row(int list_index, float y) {
  const cc_nemesis* n = rec_at(list_index);
  if (!n) return;
  const bool sel = list_index == R.sel;
  if (sel) rr_rect(4, y - 2, 312, ROW_H - 2, RR_RGBA(255, 255, 255, 30));
  ra_icon(n->species, n->form, n->gender, n->shiny, 28, y + 12, 0.75f, 0);
  if (sel) ui_cursor(11, y + 12);
  char t[LINE_LEN], a[64];
  snprintf(t, sizeof t, "%s  Lv. %u", i18n_species_name(n->species), (unsigned)n->orig_level);
  fit(t, sizeof t, TS_WINDOW, 170);
  line(46, y - 1, t, TS_WINDOW, 0);
  snprintf(t, sizeof t, "Wave %u, %s", (unsigned)n->source_wave, biome_text(n->source_biome, a, sizeof a));
  fit(t, sizeof t, TS_INSTRUCTIONS_TEXT, 170);
  line(46, y + 15, t, TS_INSTRUCTIONS_TEXT, RR_RGBA(0xa8, 0xa8, 0xa8, 255));
  text_opts o = text_defaults();
  o.origin_x = 1.0f;
  o.color = n->life == CC_LIFE_STORED ? RR_RGBA(0xc8, 0xc8, 0xc8, 255) : RR_RGBA(0xe8, 0xe8, 0xa8, 255);
  text_draw(TS_INSTRUCTIONS_TEXT, 308, y + 7, life_tag(n->life), &o);
}

static void draw_hints(const char* const* texts, int n) {
  for (int i = 0; i < n; i++) line(hint_x(TS_INSTRUCTIONS_TEXT, HINT_X, texts, i), HINT_Y, texts[i], TS_INSTRUCTIONS_TEXT, 0);
}

static void draw_bottom_list(void) {
  char t[LINE_LEN];
  ui_window(1, 1, 318, 24, WIN_NORMAL);
  line(10, 5, "Nemesis Radar", TS_WINDOW, 0);
  if (R.usable) {
    snprintf(t, sizeof t, "%d/%d", (int)R.roster.count, CC_MAX_NEMESES);
    text_opts o = text_defaults();
    o.origin_x = 1.0f;
    text_draw(TS_WINDOW, 310, 5, t, &o);
  }
  line(10, 28, "Rematches are offered during a Custom Run.", TS_INSTRUCTIONS_TEXT, RR_RGBA(0xe8, 0xe8, 0xa8, 255));

  ui_window(1, LIST_Y + 2, 318, VISIBLE * ROW_H + 10, WIN_NORMAL);
  if (!R.usable) {
    char wrapped[320];
    text_opts o = text_defaults();
    o.wrap_w = TEXT_W;
    text_wrap(TS_INSTRUCTIONS_TEXT, R.why[0] ? R.why : "The custom content file cannot be used.", &o, wrapped, sizeof wrapped);
    text_draw(TS_INSTRUCTIONS_TEXT, 12, LIST_Y + 12, wrapped, &o);
    line(12, LIST_Y + 60, "The file is left as it is.", TS_INSTRUCTIONS_TEXT, RR_RGBA(0xa0, 0xa0, 0xa0, 255));
  } else if (R.roster.count == 0) {
    line(12, LIST_Y + 14, "No enemy is remembered.", TS_WINDOW, 0);
    line(12, LIST_Y + 38, "Only a Custom Run adds enemies to this list.", TS_INSTRUCTIONS_TEXT, RR_RGBA(0xa0, 0xa0, 0xa0, 255));
  } else {
    for (int i = 0; i < VISIBLE && R.scroll + i < R.roster.count; i++) draw_row(R.scroll + i, (float)(LIST_Y + 6 + i * ROW_H));
    if (R.scroll > 0) {
      snprintf(t, sizeof t, "%d more above", R.scroll);
      line(232, 28, t, TS_INSTRUCTIONS_TEXT, RR_RGBA(0xa0, 0xa0, 0xa0, 255));
    }
    if (R.scroll + VISIBLE < R.roster.count) {
      snprintf(t, sizeof t, "%d more below", R.roster.count - R.scroll - VISIBLE);
      line(232, LIST_Y + VISIBLE * ROW_H + 14, t, TS_INSTRUCTIONS_TEXT, RR_RGBA(0xa0, 0xa0, 0xa0, 255));
    }
  }

  /* is a rematch scheduled for the highlighted record? */
  const cc_nemesis* n = R.usable ? sel_rec() : NULL;
  ui_window(1, 192, 318, 28, WIN_NORMAL);
  if (n) {
    char wrapped[256];
    radar_rematch_text(n, R.has_sched ? &R.sched : NULL, t, sizeof t);
    text_opts o = text_defaults();
    o.wrap_w = TEXT_W;
    text_wrap(TS_INSTRUCTIONS_TEXT, t, &o, wrapped, sizeof wrapped);
    text_draw(TS_INSTRUCTIONS_TEXT, 12, 197, wrapped, &o);
  } else if (R.usable && R.has_sched) {
    line(12, 197, R.sched.state == CC_SCHED_NO_CANDIDATE ? "This run has no rematch planned." : "This run's plan names no record here.",
         TS_INSTRUCTIONS_TEXT, 0);
  }
  if (R.usable && R.roster.count)
    draw_hints(LIST_HINTS, 4);
  else
    draw_hints(BACK_HINTS, 1);
}

static void draw_bottom_detail(void) {
  const cc_nemesis* n = sel_rec();
  ui_window(1, 1, 318, 24, WIN_NORMAL);
  char t[LINE_LEN];
  snprintf(t, sizeof t, "Scout: %s", n ? i18n_species_name(n->species) : "?");
  fit(t, sizeof t, TS_WINDOW, 220);
  line(10, 5, t, TS_WINDOW, 0);
  if (n) {
    text_opts o = text_defaults();
    o.origin_x = 1.0f;
    o.color = RR_RGBA(0xc8, 0xc8, 0xc8, 255);
    text_draw(TS_INSTRUCTIONS_TEXT, 310, 9, life_tag(n->life), &o);
  }
  ui_window(1, DETAIL_Y - 4, 318, DETAIL_VISIBLE * DETAIL_H + 8, WIN_NORMAL);
  for (int i = 0; i < DETAIL_VISIBLE && R.dscroll + i < R.n_detail; i++) {
    const int k = R.dscroll + i;
    rr_color c = 0;
    if (R.dtone[k] == TONE_HEAD) c = RR_RGBA(0xf8, 0xf8, 0xf8, 255);
    if (R.dtone[k] == TONE_NOTE) c = RR_RGBA(0xe8, 0xe8, 0xa8, 255);
    line(12, DETAIL_Y + i * DETAIL_H, R.detail[k], TS_INSTRUCTIONS_TEXT, c);
  }
  if (R.dscroll > 0) {
    snprintf(t, sizeof t, "%d more above", R.dscroll);
    line(232, 28, t, TS_INSTRUCTIONS_TEXT, RR_RGBA(0xa0, 0xa0, 0xa0, 255));
  }
  if (R.dscroll + DETAIL_VISIBLE < R.n_detail) {
    snprintf(t, sizeof t, "%d more below", R.n_detail - R.dscroll - DETAIL_VISIBLE);
    line(232, DETAIL_Y + DETAIL_VISIBLE * DETAIL_H + 8, t, TS_INSTRUCTIONS_TEXT, RR_RGBA(0xa0, 0xa0, 0xa0, 255));
  }
  draw_hints(DETAIL_HINTS, 2);
}

static void rd_draw(ui_screen* s, rr_screen which) {
  (void)s;
  if (which == RR_TOP) {
    draw_top();
    return;
  }
  if (!ui_space(UI_PHYS_BOTTOM)) return;
  rr_rect(0, 0, 320, 240, RR_RGBA(0x10, 0x10, 0x18, 255));
  if (R.mode == MODE_DETAIL)
    draw_bottom_detail();
  else
    draw_bottom_list();
}

static ui_screen g_screen = {"radar", UI_FADE, NULL, rd_leave, NULL, rd_input, rd_update, rd_draw, NULL, rd_touch};

bool radar_open(const radar_args* a) {
  if (!a) return false;
  if (a->meta && cc_roster_validate(&a->meta->roster) != CC_OK) return false;
  if (a->schedule && cc_schedule_validate(a->schedule) != CC_OK) return false;
  memset(&R, 0, sizeof R);
  R.on_exit = a->on_exit;
  R.exit_arg = a->arg;
  R.usable = a->meta != NULL;
  if (a->why) snprintf(R.why, sizeof R.why, "%s", a->why);
  if (R.usable) R.roster = a->meta->roster;
  if (a->schedule) {
    R.sched = *a->schedule;
    R.has_sched = true;
  }
  ui_set(&g_screen);
  return true;
}
