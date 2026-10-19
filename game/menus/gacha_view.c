/* The capsule machine's menu drawn (see gacha_view.h). Upstream's EggGachaUiHandler geometry on the top screen; the bottom screen is
 * the 3DS's own: the options in detail, in the grey windows and the 8 px pixel font. */
#include "gacha_view.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "menus.h"
#include "mui.h"

#define FONT TSX_PIXEL_8
#define W 320.0f
#define H 180.0f

static const char* const MACHINE_ART[GV_MACHINES] = {"egg/gacha_move", "egg/gacha_legendary", "egg/gacha_shiny"};
static const char* const UNDERLAY[GV_MACHINES] = {"egg/gacha_underlay_move", "egg/gacha_underlay_legendary", "egg/gacha_underlay_shiny"};
/* upstream's getVoucherTypeIcon */
static const char* const VOUCHER_ICON[GV_VOUCHERS] = {"coupon", "pair_of_tickets", "mystic_ticket", "golden_mystic_ticket"};

static rr_color dim(rr_color c, int pct) {
  const int r = (int)((c >> 24) & 0xff) * pct / 100, g = (int)((c >> 16) & 0xff) * pct / 100, b = (int)((c >> 8) & 0xff) * pct / 100;
  return RR_RGBA(r, g, b, c & 0xff);
}

static int text8(float x, float y, const char* t, rr_color c) { return mui_text_c(FONT, x, y, t, c, dim(c, 35)); }
static int text8_style(float x, float y, const char* t, mui_style st) {
  return mui_text_c(FONT, x, y, t, mui_style_color(st, false), mui_style_color(st, true));
}
static void text8_centred(float cx, float y, const char* t, mui_style st) {
  text8_style(cx - (float)mui_text_width(FONT, t) / 2.0f, y, t, st);
}
static void text8_right(float right, float y, const char* t, rr_color c) { text8(right - (float)mui_text_width(FONT, t), y, t, c); }

static void alpha_image(const char* key, const char* frame, float x, float y, float ox, float oy, int alpha) {
  rr_draw_opts o = RR_DEFAULT_OPTS;
  o.origin_x = ox;
  o.origin_y = oy;
  o.tint = RR_RGBA(255, 255, 255, alpha);
  mui_image_ex(key, frame, x, y, &o);
}

/* ------------------------------------------------------------------ top screen */

static void draw_background(void) {
  /* upstream's nine-slice of default_bg: its first 16 rows as they are, the rest stretched down to the bottom of the canvas */
  const rr_frame* f = mui_frame("arenas/default_bg", NULL);
  if (!f) {
    rr_rect(0, 0, W, H, RR_RGBA(0xd8, 0xf0, 0xc8, 255));
    return;
  }
  rr_draw_opts o = RR_DEFAULT_OPTS;
  o.scale_y = (H - 16.0f) / (float)(f->src_h - 16);
  rr_draw(f, 0, 16.0f - 16.0f * o.scale_y, &o);
  rr_draw_cropped(f, 0, 0, 320, 16, &RR_DEFAULT_OPTS); /* the fixed rows over the stretched copy's first rows */
}

/* the machine's art at (cx, cy): the hatch's frame (1 closed .. 4 open, upstream's "open" / "close" animations) and the underlay that
 * shows through it (upstream switches it to "open_hatch" on the hatch's fourth frame) */
static void draw_machine_art(int i, float cx, float cy, int hatch) {
  char frame[16];
  if (hatch < 1) hatch = 1;
  if (hatch > 4) hatch = 4;
  snprintf(frame, sizeof frame, "%d.png", hatch);
  mui_image("egg/gacha_eggs", NULL, cx, cy, 0, 0);
  mui_image(UNDERLAY[i], hatch == 4 ? "open_hatch" : "default", cx + 115, cy + 80, 0, 0);
  mui_image(MACHINE_ART[i], NULL, cx, cy, 0, 0);
  alpha_image("egg/gacha_glass", NULL, cx, cy, 0, 0, 128);
  mui_image("egg/gacha_knob", NULL, cx + 191, cy + 89, 0.5f, 0.5f);
  alpha_image("egg/gacha_hatch", frame, cx + 115, cy + 73, 0, 0, 230);
}

static void draw_machine(const gv_state* s, int i) {
  const float cx = 180.0f * ((float)i - s->slide), cy = 18.0f;
  if (cx > W || cx + 320.0f < 0) return;
  draw_machine_art(i, cx, cy, 1);
  /* the plate */
  const float px = cx + 160, py = cy + 46;
  if (i == 1) {
    text8_style(px + (s->featured ? 2 : -8), py - 1, s->up_label[i], TS_WINDOW_ALT);
    if (s->countdown[0]) {
      text8_centred(px, cy + 77, s->countdown, TS_WINDOW_ALT);
    }
  } else {
    text8_centred(px, py, s->up_label[i], TS_WINDOW_ALT);
  }
}

void gv_draw_top(const gv_state* s) {
  mui_set_origin(RR_TOP, 40, 30); /* the field canvas centred on the 400 x 240 screen */
  draw_background();
  mui_clip(0, 0, W, H);
  for (int i = 0; i < GV_MACHINES; i++) draw_machine(s, i);
  mui_clip_off();
  /* the species of the day on the legendary machine's plate (after the clip: it would otherwise not show) */
  {
    const float cx = 180.0f * (1.0f - s->slide);
    if (s->featured && cx > -100.0f && cx < 160.0f) {
      menu_mon m;
      memset(&m, 0, sizeof m);
      m.species = s->featured;
      menu_draw_icon_scaled(&m, cx + 146, 18 + 46 + 11, 0.5f, 0.5f, 1.0f);
    }
  }

  /* the voucher counters, top right: each a 56 x 22 window, regular nearest the corner */
  for (int k = 0; k < GV_VOUCHERS; k++) {
    const float wx = W - 56.0f * (float)(k + 1);
    mui_window(wx, 0, 56, 22);
    text8_style(wx + 8, 7, s->voucher_count[k], TS_WINDOW);
    rr_draw_opts ic = RR_DEFAULT_OPTS;
    ic.scale_x = ic.scale_y = 0.5f; /* upstream: 32 px icons at 0.5 */
    mui_image_ex("items", VOUCHER_ICON[k], wx + 37, 2, &ic);
  }

  /* the options, right: a window above the message box with a row per option */
  const float row = 16.0f, oh = 16.0f + row * GV_OPTIONS - 4.0f, ox = W - 96.0f, oy = 148.0f - oh;
  mui_window(ox, oy, 96, oh);
  for (int i = 0; i < GV_OPTIONS; i++) {
    const rr_color c = s->opt[i].can_buy ? mui_style_color(TS_WINDOW, false) : MUI_RGB(0x908e88);
    const float ry = oy + 8 + row * (float)i;
    text8(ox + 28, ry + 2, s->opt[i].short_label, c);
    if (s->opt[i].voucher < GV_VOUCHERS) {
      rr_draw_opts o = RR_DEFAULT_OPTS;
      o.scale_x = o.scale_y = 0.5f;
      o.tint = s->opt[i].can_buy ? RR_RGBA(255, 255, 255, 255) : RR_RGBA(255, 255, 255, 120);
      mui_image_ex("items", VOUCHER_ICON[s->opt[i].voucher], ox + 11, ry, &o);
    }
  }
  if (s->mode == GV_MENU && s->sel >= 0 && s->sel < GV_OPTIONS) mui_image("ui/cursor", NULL, ox + 6, oy + 8 + row * (float)s->sel + 8, 0.5f, 0.5f);

  /* the message box */
  mui_window(0, 148, W, 32);
  text8_style(8, 154, s->top_message, TS_WINDOW);
}

/* ------------------------------------------------------------------ bottom screen */

static void draw_focus(float x, float y, float w, float h) {
  const rr_color c = RR_RGBA(0xf8, 0xf8, 0xf8, 255);
  rr_rect(x, y, w, 1, c);
  rr_rect(x, y + h - 1, w, 1, c);
  rr_rect(x, y, 1, h, c);
  rr_rect(x + w - 1, y, 1, h, c);
  rr_rect(x + 1, y + 1, w - 2, h - 2, RR_RGBA(0xf8, 0xf8, 0xf8, 36));
}

#define ROW_H 24.0f
#define LIST_Y 30.0f

void gv_draw_bottom(const gv_state* s) {
  mui_set_origin(RR_BOTTOM, 0, 30);
  mui_window(0, 0, W, H);
  const rr_color body = mui_style_color(TS_WINDOW, false);
  const rr_color quiet = MUI_RGB(0xa8a8a0);

  /* the header: the machine and what it does, the eggs held against the capacity */
  text8_style(8, 5, s->machine_name, TS_WINDOW);
  text8(8, 15, s->machine_note, quiet);
  text8_right(W - 8, 5, s->egg_count, body);

  /* the options */
  mui_window(0, LIST_Y - 2, W, ROW_H * GV_OPTIONS + 12);
  for (int i = 0; i < GV_OPTIONS; i++) {
    const gv_option* o = &s->opt[i];
    const float y = LIST_Y + 4 + ROW_H * (float)i;
    const rr_color c = o->can_buy ? body : MUI_RGB(0x908e88);
    if (i == s->sel && s->mode == GV_MENU) draw_focus(8, y - 1, W - 16, ROW_H - 1);
    if (o->voucher < GV_VOUCHERS) {
      rr_draw_opts t = RR_DEFAULT_OPTS;
      t.tint = o->can_buy ? RR_RGBA(255, 255, 255, 255) : RR_RGBA(255, 255, 255, 120);
      mui_image_ex("items", VOUCHER_ICON[o->voucher], 12, y - 5, &t);
    }
    text8(50, y - 1, o->eggs, c);
    text8(50, y + 8, o->price, o->can_buy ? quiet : MUI_RGB(0x78766f));
    text8_right(W - 14, y + 3, o->have, c);
  }

  /* the buttons, or why the chosen option cannot be bought */
  const float by = LIST_Y + ROW_H * GV_OPTIONS + 12;
  mui_window(0, by, W, H - by);
  if (s->mode == GV_MENU && s->refusal[0]) text8_style(8, by + 4, s->refusal, TS_PARTY_RED);
  else text8(8, by + 4, s->hint, quiet);

  if (s->mode == GV_MENU && s->n_lines == 0) return;
  /* a question, a message, or a locked machine's reason, over the list */
  const float dh = 20.0f + 11.0f * (float)s->n_lines + (s->mode == GV_CONFIRM ? 40.0f : 0.0f);
  const float dy = 24.0f;
  mui_window(12, dy, W - 24, dh);
  for (int i = 0; i < s->n_lines; i++) text8_style(24, dy + 10 + 11.0f * (float)i, s->lines[i], TS_WINDOW);
  if (s->mode == GV_CONFIRM) {
    const float yy = dy + 14 + 11.0f * (float)s->n_lines;
    text8_style(40, yy + 4, "Yes", TS_WINDOW);
    text8_style(40, yy + 20, "No", TS_WINDOW);
    mui_image("ui/cursor", NULL, 30, yy + (s->yes ? 9 : 25), 0.5f, 0.5f);
  }
}

/* ------------------------------------------------------------------ the reveal */

static const uint8_t TIER_RGB[GV_TIERS][3] = {{0xd8, 0xd8, 0xd0}, {0x58, 0x90, 0xf0}, {0xb0, 0x58, 0xe8}, {0xf0, 0xc0, 0x30}};

static rr_color tier_rgba(int tier, int alpha) {
  if (tier < 0 || tier >= GV_TIERS) tier = 0;
  return RR_RGBA(TIER_RGB[tier][0], TIER_RGB[tier][1], TIER_RGB[tier][2], alpha);
}

static void draw_egg(const gv_egg* e) {
  char frame[16];
  snprintf(frame, sizeof frame, "egg_%d", e->tier < GV_TIERS ? e->tier : 0);
  if (!mui_image("egg/egg", frame, e->x, e->y - 4.0f * e->open, 0.5f, 0.5f)) rr_rect(e->x - 7, e->y - 9, 14, 18, tier_rgba(e->tier, 255));
  if (e->picked) { /* corner marks, so the egg itself stays clear (dark: the field is pale) */
    const rr_color w = RR_RGBA(0x18, 0x28, 0x48, 255);
    for (int sx = -1; sx <= 1; sx += 2)
      for (int sy = -1; sy <= 1; sy += 2) {
        const float cx = e->x + (float)sx * 18.0f, cy = e->y + (float)sy * 18.0f;
        rr_rect(sx < 0 ? cx : cx - 5, cy, 6, 1, w);
        rr_rect(cx, sy < 0 ? cy : cy - 5, 1, 6, w);
      }
  }
}

void gv_draw_reveal_top(const gv_reveal* s) {
  mui_set_origin(RR_TOP, 40, 30);
  draw_background();
  /* the machine the eggs came out of, its hatch opening while they leave (behind the eggs, inside the field) */
  mui_clip(0, 0, W, H);
  if (s->machine >= 0 && s->machine < GV_MACHINES) draw_machine_art(s->machine, 0.0f, 18.0f, s->hatch);
  mui_clip_off();
  if (s->top_title[0]) {
    mui_window(60, 0, 200, s->top_sub[0] ? 30 : 20);
    text8_centred(160, 4, s->top_title, TS_WINDOW);
    if (s->top_sub[0]) text8(160 - (float)mui_text_width(FONT, s->top_sub) / 2.0f, 15, s->top_sub, MUI_RGB(0xa8a8a0));
  }
  /* the chute is above the field's top edge: the eggs come in through it, so nothing is drawn outside the field */
  mui_clip(0, 0, W, H);
  for (int i = 0; i < s->n && i < GV_CAP_MAX; i++)
    if (s->egg[i].phase != 0) draw_egg(&s->egg[i]);
  mui_clip_off();
}

void gv_draw_reveal_bottom(const gv_reveal* s) {
  mui_set_origin(RR_BOTTOM, 0, 30);
  mui_window(0, 0, W, H);
  const rr_color body = mui_style_color(TS_WINDOW, false);
  const rr_color quiet = MUI_RGB(0xa8a8a0);
  mui_window(0, 0, W, 24);
  text8_style(8, 7, s->title, TS_WINDOW);
  if (s->stage == GV_REVEAL_SETTLED) {
    const float row_h = 18.0f;
    mui_window(0, 26, W, row_h * GV_LIST_ROWS + 8);
    for (int i = 0; i < s->n_rows && i < GV_LIST_ROWS; i++) {
      const float y = 30.0f + row_h * (float)i;
      if (s->row[i].selected) draw_focus(8, y - 1, W - 16, row_h - 1);
      char frame[8];
      snprintf(frame, sizeof frame, "%d", s->row[i].tier < GV_TIERS ? s->row[i].tier : 0);
      /* the frames of egg_icons are padded: the egg sits about 21 px right of and 23 px below the frame's corner */
      mui_image("egg/egg_icons", frame, -1, y - 15, 0, 0);
      text8(34, y + 1, s->row[i].name, body);
      text8_right(W - 12, y + 1, s->row[i].tier_name, tier_rgba(s->row[i].tier, 255));
    }
    if (s->more_above[0]) text8(10, 26.0f + row_h * GV_LIST_ROWS + 8 + 2, s->more_above, quiet);
    if (s->more_below[0]) text8_right(W - 10, 26.0f + row_h * GV_LIST_ROWS + 8 + 2, s->more_below, quiet);
    const float ny = 26.0f + row_h * GV_LIST_ROWS + 22;
    for (int i = 0; i < 2; i++)
      if (s->note[i][0]) text8(8, ny + 9.0f * (float)i, s->note[i], quiet);
  } else {
    /* the knob: upstream's gacha_knob at 3x, turning while the machine runs */
    rr_draw_opts o = RR_DEFAULT_OPTS;
    o.origin_x = o.origin_y = 0.5f;
    o.scale_x = o.scale_y = 3.0f;
    o.angle = s->knob * 6.2831853f;
    if (!mui_image_ex("egg/gacha_knob", NULL, 160, 112, &o)) rr_rect(144, 96, 32, 32, RR_RGBA(0xe0, 0xe0, 0xe8, 255));
    text8_centred(160, 40, s->big, TS_WINDOW);
    if (s->small[0]) text8(160 - (float)mui_text_width(FONT, s->small) / 2.0f, 54, s->small, quiet);
  }
  mui_window(0, H - 20, W, 20);
  text8(8, H - 18, s->hint, quiet);
}
