/*
 * summary_ui_test.c: PC test harness for Summary UI, Shiny Tier parsing, and Passive Ability slot.
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <stdarg.h>

#include "menus.h"

/* What the screen draws, recorded: the layout checks below read it. */
typedef struct {
  char kind;     /* 'i' image, 't' text, 'o' text with an origin, 'b' bbtext, 'c' clip */
  char what[640];
  float x, y, w, h, ox, oy;
} drawn;
static drawn g_dr[256];
static int g_ndr;
static char g_idno[32];
static void rec(char kind, const char* what, float x, float y, float w, float h, float ox, float oy) {
  if (g_ndr >= 256) return;
  drawn* d = &g_dr[g_ndr++];
  d->kind = kind;
  snprintf(d->what, sizeof d->what, "%s", what ? what : "");
  d->x = x, d->y = y, d->w = w, d->h = h, d->ox = ox, d->oy = oy;
}
static const drawn* find(char kind, const char* what_part, int nth) {
  for (int i = 0; i < g_ndr; i++)
    if (g_dr[i].kind == kind && strstr(g_dr[i].what, what_part) && nth-- == 0) return &g_dr[i];
  return NULL;
}

/* the clip of the given width (the screen's own full clip is the first one drawn) */
static const drawn* find_clip(float w) {
  for (int i = 0; i < g_ndr; i++)
    if (g_dr[i].kind == 'c' && g_dr[i].w == w) return &g_dr[i];
  return NULL;
}

const rr_draw_opts RR_DEFAULT_OPTS = {0, 0, 1.0f, 1.0f, 0, 0xffffffff};

void rr_draw(const rr_frame* frame, float x, float y, const rr_draw_opts* opts) {
  (void)frame; (void)x; (void)y; (void)opts;
}

void rr_draw_cropped(const rr_frame* frame, float x, float y, int crop_w, int crop_h, const rr_draw_opts* opts) {
  (void)frame; (void)x; (void)y; (void)crop_w; (void)crop_h; (void)opts;
}

/* Stub functions required by mui when building on PC without 3DS libraries */
void mui_sfx(const char* key) {
  (void)key;
}

/* the key itself, in a buffer that outlives the caller's (the real mui_t returns a string of its own) */
const char* mui_t(const char* key) {
  /* Actual pinned English locale labels, rather than generated Move.name debug text. */
  if (!strcmp(key, "move:return.name")) return "Return";
  if (!strcmp(key, "move:pursuit.name")) return "Pursuit (P)";
  if (!strcmp(key, "move:uproar.name")) return "Uproar (P)";
  static char buf[16][160];
  static int next;
  char* b = buf[next++ & 15];
  snprintf(b, sizeof buf[0], "%s", key);
  return b;
}

int mui_text_width(mui_style s, const char* utf8) {
  (void)s;
  return (int)strlen(utf8) * 8;
}

float mui_text_height(mui_style s, const char* utf8) {
  (void)s; (void)utf8;
  return 12.0f;
}

void mui_window(float x, float y, float w, float h) {
  rec('w', "window", x, y, w, h, 0, 0);
}

int mui_line_height(mui_style s) {
  (void)s;
  return 12;
}

void rr_rect(float x, float y, float w, float h, rr_color c) {
  (void)c;
  rec('r', "rect", x, y, w, h, 0, 0);
}

int mui_text(mui_style s, float x, float y, const char* utf8) {
  (void)s;
  rec('t', utf8, x, y, 0, 0, 0, 0);
  return 0;
}

int mui_text_o(mui_style s, float x, float y, float ox, float oy, const char* utf8) {
  (void)s;
  rec('o', utf8, x, y, 0, 0, ox, oy);
  return 0;
}

int mui_text_c(mui_style s, float x, float y, const char* utf8, rr_color c, rr_color sh) {
  (void)s; (void)x; (void)y; (void)utf8; (void)c; (void)sh;
  return 0;
}

int mui_text_ls(mui_style s, float x, float y, float pitch, const char* utf8, rr_color c, rr_color sh) {
  (void)s; (void)x; (void)y; (void)pitch; (void)utf8; (void)c; (void)sh;
  return 0;
}

int mui_bbtext(mui_style s, float x, float y, const char* bbtext) {
  (void)s;
  rec('b', bbtext, x, y, 0, 0, 0, 0);
  return 0;
}

/* The clips and images of a draw, with the scale of a stretched one, for the band checks below (on while g_bl_on) */
typedef struct {
  char kind; /* 'c' clip (x, y, w, h), 'i' image (x, y, origin ox, oy), 'e' image_ex (x, y, scale sx, sy, origin ox, oy), 'o' clip off */
  char key[40];
  float x, y, w, h, sx, sy, ox, oy;
} bcall;
static bcall g_bl[64];
static int g_nbl;
static bool g_bl_on;
static void brec(char kind, const char* key, float x, float y, float w, float h, float sx, float sy, float ox, float oy) {
  if (!g_bl_on || g_nbl >= 64) return;
  g_bl[g_nbl++] = (bcall){kind, "", x, y, w, h, sx, sy, ox, oy};
  snprintf(g_bl[g_nbl - 1].key, sizeof g_bl[g_nbl - 1].key, "%s", key);
}

void mui_clip(float x, float y, float w, float h) {
  rec('c', "", x, y, w, h, 0, 0);
  brec('c', "", x, y, w, h, 0, 0, 0, 0);
}

void mui_clip_off(void) { brec('o', "", 0, 0, 0, 0, 0, 0, 0, 0); }

bool mui_image(const char* atlas, const char* frame, float x, float y, float ox, float oy) {
  (void)frame;
  rec('i', atlas, x, y, 0, 0, ox, oy);
  brec('i', atlas, x, y, 0, 0, 0, 0, ox, oy);
  return true;
}

bool mui_image_ex(const char* atlas, const char* frame, float x, float y, const rr_draw_opts* opts) {
  (void)frame;
  brec('e', atlas, x, y, 0, 0, opts->scale_x, opts->scale_y, opts->origin_x, opts->origin_y);
  return true;
}

const rr_frame* mui_frame(const char* key, const char* frame) {
  (void)frame;
  static rr_frame dummy = {0, 0, 16, 16};
  /* a page's art as the band checks need it: 214 wide (PAGE_X 106 to the screen's edge), 158 high; elsewhere the old 16 x 16 */
  static rr_frame page = {0, 0, 0, 0, 0, 0, 0, 214, 158};
  if (g_bl_on && key && (!strcmp(key, "ui/summary_profile") || !strcmp(key, "ui/summary_stats") || !strcmp(key, "ui/summary_moves")))
    return &page;
  return &dummy;
}

int mui_wrap(mui_style s, const char* text, float max_w, char* out, size_t out_len) {
  (void)s; (void)max_w;
  if (text && out) {
    snprintf(out, out_len, "%s", text);
  }
  return 1;
}

/* "key{name=value;...}": the arguments stay readable */
char* mui_tf(char* out, size_t len, const char* key, ...) {
  char tmp[512];
  int n = snprintf(tmp, sizeof tmp, "%s{", key);
  va_list ap;
  va_start(ap, key);
  for (const char* name; (name = va_arg(ap, const char*)) != NULL;) {
    const char* val = va_arg(ap, const char*);
    n += snprintf(tmp + n, sizeof tmp - (size_t)n, "%s=%s;", name, val ? val : "");
  }
  va_end(ap);
  snprintf(tmp + n, sizeof tmp - (size_t)n, "}");
  if (!strcmp(key, "pokemonSummary:idNo")) snprintf(g_idno, sizeof g_idno, "%s", tmp);
  if (out) snprintf(out, len, "%s", tmp);
  return out;
}

char* mui_bbfrag(char* out, size_t len, const char* text, mui_style style, bool close) {
  if (out) snprintf(out, len, "%s", text ? text : "");
  return out;
}

rr_color mui_style_color(mui_style s, bool shadow) {
  (void)s; (void)shadow;
  return 0xffffffff;
}

/* Menu sprite stubs */
bool menu_sprite_load(menu_sprite* s, const menu_mon* m, bool back) {
  (void)m; (void)back;
  if (s) s->n = 1;
  return true;
}

void menu_sprite_free(menu_sprite* s) {
  if (s) s->n = 0;
}

const rr_frame* menu_sprite_frame(const menu_sprite* s, uint32_t ms) {
  (void)s; (void)ms;
  static rr_frame dummy = {0, 0, 32, 32};
  return &dummy;
}

typedef struct {
  bool closed;
  int swap_slot, swap_a, swap_b;
} test_events;

static char g_info_name[80], g_info_desc[160];
static bool g_info_on;
static void on_test_info(void* ctx, const char* name, const char* desc) {
  (void)ctx;
  g_info_on = name != NULL;
  snprintf(g_info_name, sizeof g_info_name, "%s", name ? name : "");
  snprintf(g_info_desc, sizeof g_info_desc, "%s", desc ? desc : "");
}

static void on_test_close(void* ctx) {
  test_events* e = (test_events*)ctx;
  e->closed = true;
}

static void on_test_swap(void* ctx, int slot, int a, int b) {
  test_events* e = (test_events*)ctx;
  e->swap_slot = slot;
  e->swap_a = a;
  e->swap_b = b;
}

int main(void) {
  printf("Running summary_ui_test...\n");

  /* 1. Test Shiny Tier Parsing */
  menu_mon mon;
  memset(&mon, 0, sizeof mon);
  mon.species = 25; /* Pikachu */
  mon.level = 50;

  /* Non-shiny */
  int fus_tier = -1;
  int tier = summary_mon_shiny_tier(&mon, &fus_tier);
  assert(tier == 0);
  assert(fus_tier == 0);
  assert(!strcmp(summary_shiny_tier_name(tier), "None"));
  printf("Shiny Tier (Non-shiny -> None) PASSED.\n");

  /* Yellow Shiny (Tier 1) */
  mon.shiny = true;
  mon.variant = 0;
  tier = summary_mon_shiny_tier(&mon, &fus_tier);
  assert(tier == 1);
  assert(!strcmp(summary_shiny_tier_name(tier), "Yellow"));
  printf("Shiny Tier (Variant 0 -> Yellow) PASSED.\n");

  /* Blue Shiny (Tier 2) */
  mon.variant = 1;
  tier = summary_mon_shiny_tier(&mon, &fus_tier);
  assert(tier == 2);
  assert(!strcmp(summary_shiny_tier_name(tier), "Blue"));
  printf("Shiny Tier (Variant 1 -> Blue) PASSED.\n");

  /* Red Shiny (Tier 3) */
  mon.variant = 2;
  tier = summary_mon_shiny_tier(&mon, &fus_tier);
  assert(tier == 3);
  assert(!strcmp(summary_shiny_tier_name(tier), "Red"));
  printf("Shiny Tier (Variant 2 -> Red) PASSED.\n");

  /* Fused Double Shiny (Base Red, Fusion Blue) */
  mon.fusion = true;
  mon.fusion_shiny = true;
  mon.fusion_variant = 1;
  tier = summary_mon_shiny_tier(&mon, &fus_tier);
  assert(tier == 3);
  assert(fus_tier == 2);
  assert(!strcmp(summary_shiny_tier_name(tier), "Red"));
  assert(!strcmp(summary_shiny_tier_name(fus_tier), "Blue"));
  printf("Shiny Tier (Fused Double Shiny Red+Blue) PASSED.\n");

  /* 2. Test Summary Screen Open & Profile Page Rendering */
  menu_mon party[2];
  memset(party, 0, sizeof party);

  /* Mon 0: No passive ability, a starter (never met anywhere) */
  party[0].met_biome = -1;
  party[0].met_level = 5;
  party[0].species = 6; /* Charizard */
  party[0].ability = 66; /* Blaze */
  party[0].passive = 0;
  party[0].has_passive = false;
  snprintf(party[0].name, sizeof party[0].name, "Charizard");

  /* Mon 1: Has passive ability, caught in the wild (biome 4, level 7, wave 12) */
  party[1].met_biome = 4;
  party[1].met_level = 7;
  party[1].met_wave = 12;
  party[1].species = 257; /* Blaziken */
  party[1].ability = 66; /* Blaze */
  party[1].passive = 3;  /* Speed Boost */
  party[1].has_passive = true;
  snprintf(party[1].name, sizeof party[1].name, "Blaziken");

  test_events ev_data;
  memset(&ev_data, 0, sizeof ev_data);

  summary_args args;
  memset(&args, 0, sizeof args);
  args.mode = SUMMARY_DEFAULT;
  args.page = SUMMARY_PROFILE;
  args.party = party;
  args.n = 2;
  args.index = 0;
  args.from_party = true;
  args.trainer_id = "12345";

  summary_events ev;
  ev.on_close = on_test_close;
  ev.on_move_swap = on_test_swap;
  ev.ctx = &ev_data;

  summary_open(&args, &ev);
  assert(summary_active());
  printf("Summary screen opened on Mon 0 (no passive).\n");

  /* Render profile page for Mon 0 */
  summary_update(300.0f);
  g_ndr = 0;
  summary_draw();
  printf("Profile page drawn for Mon 0 (primary ability only).\n");
  {
    /* the page's origin, from the title image drawn at (7, 4) */
    const drawn* title = find('i', "summary_profile_profile_title", 0);
    assert(title);
    const float px = title->x - 7, py = title->y - 4;
    /* the ability block is upstream's: label (17, 47), name's bottom edge at 68, description from 71 behind a 31 px mask */
    const drawn* lab = find('i', "summary_profile_ability", 0);
    assert(lab && lab->x == px + 17 && lab->y == py + 47 && lab->oy == 0.5f);
    assert(!find('i', "summary_profile_passive", 0));
    const drawn* name = find('o', ".name", 0);
    assert(name && name->x == px + 7 && name->y == py + 68 && name->ox == 0 && name->oy == 1);
    assert(!find('o', ".name", 1)); /* one ability name, not both */
    const drawn* clip = find_clip(206);
    assert(clip && clip->x == px && clip->y == py + 71 && clip->w == 206 && clip->h == 31);
    const drawn* desc = find('t', ".description", 0);
    assert(desc && desc->x == px + 7 && desc->y >= clip->y && desc->y < clip->y + clip->h);
    assert(!find('t', ".description", 1));
    /* the memo: title at 107, text at 113 */
    const drawn* mt = find('i', "summary_profile_memo_title", 0);
    assert(mt && mt->x == px + 7 && mt->y == py + 107);
    const drawn* memo = find('b', "memoString", 0);
    assert(memo && memo->x == px + 7 && memo->y == py + 113);
    /* a starter was never met anywhere: upstream's "apparently met" with the unknown place */
    assert(strstr(memo->what, "metFragment.apparently") && strstr(memo->what, "biome:unknownLocation"));
    /* the ID No. is the trainer id the caller gave */
    assert(strstr(g_idno, "idNo=12345"));
  }
  printf("Profile layout (ability block, memo, ID No., starter's place) PASSED.\n");

  /* Navigate D-pad Down to Mon 1 (has passive) */
  bool handled = summary_input(MB_DOWN);
  assert(handled);
  summary_update(300.0f);
  g_ndr = 0;
  summary_draw();
  printf("Profile page drawn for Mon 1 (primary + passive slot rendered).\n");
  char ability_name[128];
  float px1, py1;
  {
    const drawn* title = find('i', "summary_profile_profile_title", 0);
    assert(title);
    px1 = title->x - 7, py1 = title->y - 4;
    const drawn* name = find('o', ".name", 0);
    assert(name && name->y == py1 + 68);
    snprintf(ability_name, sizeof ability_name, "%s", name->what);
    assert(find('i', "summary_profile_ability", 0) && !find('i', "summary_profile_passive", 0));
    assert(find('i', "summary_profile_prompt_a", 0)); /* the passive can be shown with A */
    /* caught in the wild: its biome, level and wave, not the unknown place */
    const drawn* memo = find('b', "memoString", 0);
    assert(memo && strstr(memo->what, "metFragment.normal") && !strstr(memo->what, "unknownLocation"));
    assert(strstr(memo->what, "biome:"));
    assert(strstr(memo->what, "level=7") && strstr(memo->what, "wave=12"));
  }
  /* A swaps the ability for the passive in place (same label, name and description positions), never both at once */
  (void)summary_input(MB_ACTION);
  g_ndr = 0;
  summary_draw();
  {
    const drawn* lab = find('i', "summary_profile_passive", 0);
    assert(lab && lab->x == px1 + 17 && lab->y == py1 + 47);
    assert(!find('i', "summary_profile_ability", 0));
    const drawn* name = find('o', ".name", 0);
    assert(name && name->x == px1 + 7 && name->y == py1 + 68 && strcmp(name->what, ability_name) && !find('o', ".name", 1));
    const drawn* clip = find_clip(206);
    assert(clip && clip->y == py1 + 71 && clip->h == 31);
    const drawn* desc = find('t', ".description", 0);
    assert(desc && desc->y >= clip->y && desc->y < clip->y + clip->h && !find('t', ".description", 1));
  }
  (void)summary_input(MB_ACTION); /* and back to the ability */
  g_ndr = 0;
  summary_draw();
  assert(find('i', "summary_profile_ability", 0) && !find('i', "summary_profile_passive", 0));
  printf("Passive toggle (one block at a time, same positions) PASSED.\n");

  /* Navigate Right to Stats page */
  handled = summary_input(MB_RIGHT);
  assert(handled);
  summary_update(300.0f);
  summary_draw();

  /* Navigate Right to Moves page */
  handled = summary_input(MB_RIGHT);
  assert(handled);
  summary_update(300.0f);
  summary_draw();

  /* Cancel move selection mode */
  handled = summary_input(MB_CANCEL);
  assert(handled);

  /* Cancel summary screen */
  handled = summary_input(MB_CANCEL);
  assert(handled);
  assert(ev_data.closed);
  assert(!summary_active());
  printf("Summary screen closed successfully.\n");

  /* 3. The held items: Start on the Stats page puts the cursor on the first one, A opens its description (on_item_info: the caller
   * shows it), Left/Right move the cursor (an open description follows), B closes the description and then leaves the row */
  memset(party, 0, sizeof party);
  party[0].species = 6;
  party[1].species = 257;
  snprintf(party[0].name, sizeof party[0].name, "Charizard");
  snprintf(party[1].name, sizeof party[1].name, "Blaziken");
  party[0].nitems = 3;
  snprintf(party[0].items[0].name, sizeof party[0].items[0].name, "Leftovers");
  party[0].items[0].count = 2;
  party[0].items[0].desc = "Heals 1/16 of its max HP every turn.";
  snprintf(party[0].items[1].name, sizeof party[0].items[1].name, "Focus Band");
  party[0].items[1].count = 1;
  party[0].items[1].desc = "Sometimes survives a hit with 1 HP.";
  snprintf(party[0].items[2].name, sizeof party[0].items[2].name, "Mystery");
  party[0].items[2].count = 1; /* no description */
  memset(&ev_data, 0, sizeof ev_data);
  ev.on_item_info = on_test_info;
  g_info_on = false;
  args.page = SUMMARY_STATS;
  args.index = 0;
  summary_open(&args, &ev);
  summary_update(300.0f);
  g_ndr = 0;
  summary_draw();
  assert(!g_info_on && !find('r', "rect", 0)); /* nothing is picked on opening */
  assert(summary_input(MB_MENU));
  g_ndr = 0;
  summary_draw();
  assert(find('r', "rect", 3)); /* the cursor frame round the first icon: four sides */
  assert(!g_info_on); /* ... and no description until A */
  assert(summary_input(MB_ACTION));
  assert(g_info_on && !strcmp(g_info_name, "Leftovers x2") && strstr(g_info_desc, "Heals 1/16"));
  assert(summary_input(MB_RIGHT)); /* the cursor moves on and the open description follows */
  g_ndr = 0;
  summary_draw();
  assert(g_info_on && !strcmp(g_info_name, "Focus Band") && strstr(g_info_desc, "Sometimes survives") && find('r', "rect", 3));
  assert(summary_input(MB_RIGHT)); /* the last: no description, the name alone */
  assert(g_info_on && !strcmp(g_info_name, "Mystery") && !g_info_desc[0]);
  assert(summary_input(MB_RIGHT)); /* past the end: refused (an error sound), the cursor stays */
  assert(g_info_on && !strcmp(g_info_name, "Mystery"));
  assert(summary_input(MB_CANCEL)); /* B closes the description, the cursor stays on the item */
  assert(!g_info_on);
  g_ndr = 0;
  summary_draw();
  assert(find('r', "rect", 3));
  assert(summary_input(MB_ACTION) && g_info_on); /* A opens it again */
  assert(summary_input(MB_CANCEL) && !g_info_on);
  assert(summary_input(MB_CANCEL)); /* B again leaves the row (not the summary) */
  assert(summary_active() && !ev_data.closed);
  g_ndr = 0;
  summary_draw();
  assert(!find('r', "rect", 0));
  /* an open description closes with the row (Down) and with the screen */
  assert(summary_input(MB_MENU) && summary_input(MB_ACTION) && g_info_on);
  assert(summary_input(MB_DOWN) && !g_info_on); /* Down only leaves the row */
  assert(summary_input(MB_DOWN)); /* the next Pokemon, which holds nothing */
  assert(summary_input(MB_LEFT));
  summary_update(300.0f);
  assert(!summary_input(MB_MENU)); /* a Pokemon with no items, and the other pages, never pick one */
  assert(summary_input(MB_CANCEL));
  assert(ev_data.closed);
  printf("Item window PASSED.\n");

  /* 4. Touch: a tap on an item puts the cursor there and opens its description; the same item or anywhere else closes it */
  memset(&ev_data, 0, sizeof ev_data);
  summary_open(&args, &ev);
  summary_update(300.0f);
  const float ty = 180 - (float)mui_frame("ui/summary_stats", NULL)->src_h + 31; /* the item row's centre */
  assert(!summary_touch(300, 20)); /* nothing open: a tap off the items does nothing */
  assert(summary_touch(122, ty));
  assert(g_info_on && !strcmp(g_info_name, "Leftovers x2"));
  assert(summary_touch(106 + 26 + 17, ty));
  assert(g_info_on && !strcmp(g_info_name, "Focus Band"));
  assert(summary_touch(106 + 26 + 17, ty)); /* the same item: closes */
  g_ndr = 0;
  summary_draw();
  assert(!g_info_on && !find('r', "rect", 0));
  assert(summary_touch(122, ty) && g_info_on && summary_touch(300, 20)); /* a tap elsewhere closes it */
  assert(!g_info_on);
  assert(!summary_touch(106 + 3 * 26 + 17, ty)); /* past the last item (3 held) */
  assert(summary_touch(122, ty) && g_info_on);
  summary_close(); /* closing the screen takes the description down */
  assert(!g_info_on);
  printf("Item window touch PASSED.\n");

  /* Live stages stay distinct from permanent stats and IVs, including ACC/EVA. */
  party[0].show_battle_stages = true;
  party[0].stat_stages[0] = 2;
  party[0].stat_stages[1] = -6;
  party[0].stat_stages[5] = 3;
  party[0].stat_stages[6] = -2;
  args.page = SUMMARY_STATS;
  args.index = 0;
  summary_open(&args, &ev);
  summary_update(300.0f);
  g_ndr = 0;
  summary_draw();
  assert(find('o', "(+2)", 0) && find('o', "(-6)", 0));
  assert(find('o', "ACC +3   EVA -2", 0));
  summary_close();
  party[0].show_battle_stages = false;
  summary_open(&args, &ev);
  summary_update(300.0f);
  g_ndr = 0;
  summary_draw();
  assert(!find('o', "(+2)", 0) && !find('o', "ACC +3", 0));
  summary_close();

  /* ---- the bands of the bottom screen (lorenzo, 2026-11-17: black bands above and below "pokemon info"): the canvas is 180 rows on a 240-row
   * screen, so the art's own edges are drawn into the 30 rows above and below it. The calls are checked, not the art's pixels (not in the repo);
   * the art rows named come from the app's screenshot of this screen (header dark between teal lines at canvas rows 2 and 14). */
  {
    party[0].show_battle_stages = false;
    args.mode = SUMMARY_DEFAULT;
    args.page = SUMMARY_PROFILE;
    args.index = 0;
    summary_open(&args, &ev);
    summary_update(300.0f);
    g_bl_on = true;
    g_nbl = 0;
    summary_draw();
    g_bl_on = false;
    int own = -1; /* the canvas' own background */
    for (int i = 0; i < g_nbl; i++)
      if (g_bl[i].kind == 'i' && !strcmp(g_bl[i].key, "ui/summary_bg") && g_bl[i].y == 180 && g_bl[i].x == 0) {
        own = i;
        break;
      }
    assert(own == 8);
    /* above: the dark row stretched over the band and the canvas' first row (the art's own is empty), then the teal line copied up */
    assert(g_bl[0].kind == 'c' && g_bl[0].x == 0 && g_bl[0].y == -30 && g_bl[0].w == 320 && g_bl[0].h == 31);
    assert(g_bl[1].kind == 'e' && !strcmp(g_bl[1].key, "ui/summary_bg") && g_bl[1].x == 0 && g_bl[1].sx == 1 && g_bl[1].sy == 31 && g_bl[1].oy == 0);
    assert(g_bl[1].y + 8 * g_bl[1].sy == -30 && g_bl[1].y + 9 * g_bl[1].sy == 1); /* art row 8 covers exactly the band and row 0 */
    assert(g_bl[2].kind == 'c' && g_bl[2].y == -30 && g_bl[2].h == 1 && g_bl[2].w == 320);
    assert(g_bl[3].kind == 'i' && !strcmp(g_bl[3].key, "ui/summary_bg") && g_bl[3].x == 0 && g_bl[3].oy == 1);
    assert(2 + (g_bl[3].y - 180) == -30); /* the canvas' row 2, the teal line, lands on the band's first row */
    /* the mask after them is the canvas and the bands: 320 x 240, from y -30 */
    assert(g_bl[4].kind == 'c' && g_bl[4].x == 0 && g_bl[4].y == -30 && g_bl[4].w == 320 && g_bl[4].h == 240);
    /* below: the background's last row stretched over the 30 rows under the canvas (and the canvas' own last row) */
    assert(g_bl[5].kind == 'c' && g_bl[5].x == 0 && g_bl[5].y == 180 && g_bl[5].w == 320 && g_bl[5].h == 30);
    assert(g_bl[6].kind == 'e' && !strcmp(g_bl[6].key, "ui/summary_bg") && g_bl[6].x == 0 && g_bl[6].y == 210 && g_bl[6].sy == 31 && g_bl[6].oy == 1);
    assert(g_bl[7].kind == 'c' && g_bl[7].y == -30 && g_bl[7].h == 240); /* the mask is back */
    /* the profile page: its image at (106, 180), then its last row over x 106..320 of the band, then the mask again */
    int pg = -1;
    for (int i = own; i < g_nbl; i++)
      if (g_bl[i].kind == 'i' && !strcmp(g_bl[i].key, "ui/summary_profile")) {
        pg = i;
        break;
      }
    assert(pg > 0 && g_bl[pg].x == 106 && g_bl[pg].y == 180 && g_bl[pg].oy == 1);
    assert(g_bl[pg + 1].kind == 'c' && g_bl[pg + 1].x == 106 && g_bl[pg + 1].y == 180 && g_bl[pg + 1].w == 214 && g_bl[pg + 1].h == 30);
    assert(g_bl[pg + 2].kind == 'e' && !strcmp(g_bl[pg + 2].key, "ui/summary_profile") && g_bl[pg + 2].x == 106 && g_bl[pg + 2].y == 210 &&
           g_bl[pg + 2].sy == 31 && g_bl[pg + 2].oy == 1);
    assert(g_bl[pg + 3].kind == 'c' && g_bl[pg + 3].y == -30 && g_bl[pg + 3].h == 240);
    /* a page that slides in is only drawn where the screen is: no band clip leaves it */
    summary_input(MB_RIGHT);
    summary_update(100.0f);
    g_bl_on = true;
    g_nbl = 0;
    summary_draw();
    g_bl_on = false;
    int below = 0;
    for (int i = 0; i < g_nbl; i++)
      if (g_bl[i].kind == 'c' && g_bl[i].y == 180 && g_bl[i].h == 30) {
        assert(g_bl[i].x >= 0 && g_bl[i].w > 0 && g_bl[i].x + g_bl[i].w <= 320);
        below++;
      }
    assert(below == 3); /* the background and the two pages of the slide */
    /* each page's band is cut to where that page is: from its x (or the screen's edge) to its right edge (or the screen's) */
    int pages = 0;
    for (int i = 0; i + 1 < g_nbl; i++)
      if (g_bl[i].kind == 'i' && g_bl[i].y == 180 && !strncmp(g_bl[i].key, "ui/summary_", 11) && strcmp(g_bl[i].key, "ui/summary_bg") &&
          strstr(g_bl[i].key, "prompt") == NULL && g_bl[i].oy == 1 && g_bl[i + 1].kind == 'c') {
        const float px = g_bl[i].x, l = px < 0 ? 0 : px, r = px + 214 > 320 ? 320 : px + 214;
        assert(g_bl[i + 1].x == l && g_bl[i + 1].x + g_bl[i + 1].w == r && g_bl[i + 1].y == 180 && g_bl[i + 1].h == 30);
        pages++;
      }
    assert(pages == 2);
    summary_close();

    /* the moves page: the description's own mask is put back as the widened one, so the bands keep showing */
    party[0].nmoves = 1;
    party[0].moves[0].id = 33;
    args.page = SUMMARY_MOVES;
    summary_open(&args, &ev);
    summary_update(300.0f);
    (void)summary_input(MB_ACTION);
    summary_update(300.0f);
    g_bl_on = true;
    g_nbl = 0;
    summary_draw();
    g_bl_on = false;
    int desc = -1;
    for (int i = 0; i < g_nbl; i++)
      if (g_bl[i].kind == 'c' && g_bl[i].x == 112 && g_bl[i].y == 121 && g_bl[i].w == 205 && g_bl[i].h == 59) desc = i;
    assert(desc > 0 && desc + 1 < g_nbl);
    assert(g_bl[desc + 1].kind == 'c' && g_bl[desc + 1].x == 0 && g_bl[desc + 1].y == -30 && g_bl[desc + 1].w == 320 && g_bl[desc + 1].h == 240);
    summary_close();
    party[0].nmoves = 0;
  }

  assert(!strcmp(menu_move_name(216), "Return"));
  assert(!strcmp(menu_move_name(228), "Pursuit"));
  assert(!strcmp(menu_move_name(253), "Uproar"));

  printf("summary_ui_test PASSED ALL CHECKS!\n");
  return 0;
}
