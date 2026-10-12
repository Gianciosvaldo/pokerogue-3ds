/* The capsule-machine presentation prototype (see capsule_screen.h). */
#include "capsule_screen.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "ui.h"
#include "ui_text.h"
#include "ui_widgets.h"

#define DWELL_MS 300.0f       /* after the results are shown, how long before a button closes the screen */
#define LIST_ROWS 5
#define ROW_H 24
#define LIST_Y 44
#define KNOB_X 160.0f
#define KNOB_Y 120.0f

static const char* const TIER_NAME[CAP_TIERS] = {"Common", "Rare", "Epic", "Legendary"};
static const uint8_t TIER_RGB[CAP_TIERS][3] = {{0xd8, 0xd8, 0xd0}, {0x58, 0x90, 0xf0}, {0xb0, 0x58, 0xe8}, {0xf0, 0xc0, 0x30}};

static struct {
  cap_model m;
  caps_args a;
  bool open;                 /* between open and leave */
  bool exited;               /* on_exit was called: no more input */
  bool settled_fired;
  float since_settled;
  int sel;                   /* the result the list and the top screen point at */
  int scroll;
} C;

static rr_color tier_color(int tier, int alpha) {
  if (tier < 0 || tier >= CAP_TIERS) tier = 0;
  return RR_RGBA(TIER_RGB[tier][0], TIER_RGB[tier][1], TIER_RGB[tier][2], alpha);
}

bool capsule_debug_fixture(int n, cap_fixture* out) {
  if (!out || n < 1 || n > CAP_MAX) return false;
  memset(out, 0, sizeof *out);
  out->n = (uint8_t)n;
  for (int i = 0; i < n; i++) {
    out->item[i].tier = (uint8_t)(i % 7 == 6 ? 3 : (i % 5 == 4 ? 2 : (i % 3 == 2 ? 1 : 0)));
    snprintf(out->item[i].label, CAP_LABEL, "Fixture result %d", i + 1);
  }
  return true;
}

/* ------------------------------------------------------------------ the callbacks */

static void fire_settled(void) {
  if (C.settled_fired) return;
  C.settled_fired = true;
  C.since_settled = 0;
  if (C.a.on_settled) C.a.on_settled(C.a.arg);
}

static void leave_screen(caps_exit how) {
  if (C.exited) return;
  C.exited = true;
  if (C.a.on_exit) C.a.on_exit(how, C.a.arg);
}

/* ------------------------------------------------------------------ input and time */

static bool cs_input(ui_screen* s, ui_button b) {
  (void)s;
  if (!C.open || C.exited) return true;
  if (ui_input_repeating()) return true; /* a held button does its action once */
  switch (C.m.stage) {
    case CAP_IDLE:
      if (b == BTN_ACTION || b == BTN_SUBMIT) {
        if (cap_pull(&C.m)) ui_play_select();
        return true;
      }
      if (b == BTN_CANCEL) {
        ui_play_select();
        leave_screen(CAPS_EXIT_BEFORE_REVEAL);
        return true;
      }
      return false;
    case CAP_PULL:
    case CAP_DROP:
      if (b == BTN_CANCEL || b == BTN_MENU) {
        if (cap_skip(&C.m)) {
          if (cap_take_settled(&C.m)) fire_settled();
        }
        return true;
      }
      return false;
    case CAP_SETTLED:
      if (b == BTN_UP || b == BTN_DOWN) {
        const int n = C.m.fx.n;
        const int to = C.sel + (b == BTN_DOWN ? 1 : -1);
        if (to >= 0 && to < n) {
          C.sel = to;
          if (C.sel < C.scroll) C.scroll = C.sel;
          if (C.sel >= C.scroll + LIST_ROWS) C.scroll = C.sel - LIST_ROWS + 1;
          ui_play_select();
        }
        return true;
      }
      if (b == BTN_ACTION || b == BTN_CANCEL || b == BTN_MENU) {
        if (C.since_settled >= DWELL_MS) {
          ui_play_select();
          leave_screen(CAPS_EXIT_AFTER_REVEAL);
        }
        return true;
      }
      return false;
  }
  return false;
}

static void cs_update(ui_screen* s, float dt) {
  (void)s;
  if (!C.open || C.exited) return;
  cap_step(&C.m, dt);
  if (cap_take_settled(&C.m)) fire_settled();
  if (C.m.stage == CAP_SETTLED && dt > 0) C.since_settled += dt;
}

static void cs_enter(ui_screen* s) {
  (void)s;
  C.open = true; /* the screen is the top one now (ui_set may run leave() of an earlier opening first, which is why this is not set in open) */
}

static void cs_leave(ui_screen* s) {
  (void)s;
  C.open = false; /* no callback from here: whoever left the screen knows */
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

/* A disc of radius 10 in seven strips (3 px tall each): `hw` is the half width at |dy| 0, 3, 6, 9. */
static const int HALF_W[4] = {10, 9, 8, 4};

static void draw_capsule(float x, float y, int tier, float open, bool picked) {
  /* the lid half lifts and thins as the capsule opens; the base stays */
  const float lift = open * 6.0f;
  const int lid_alpha = (int)(255.0f * (1.0f - 0.65f * open));
  for (int k = -3; k <= 3; k++) {
    const int hw = HALF_W[k < 0 ? -k : k];
    const bool lid = k < 0;
    const float yy = y + (float)k * 3.0f - 1.5f - (lid ? lift : 0.0f);
    if (lid)
      rr_rect(x - (float)hw, yy, (float)(2 * hw), 3.0f, tier_color(tier, lid_alpha));
    else
      rr_rect(x - (float)hw, yy, (float)(2 * hw), 3.0f, RR_RGBA(0xf0, 0xf0, 0xf0, 255));
  }
  if (open < 1.0f) rr_rect(x - 6.0f, y - 7.0f, 3.0f, 3.0f, RR_RGBA(255, 255, 255, 140)); /* a shine on the closed lid */
  if (open > 0.3f) {
    /* what came out: a small egg-shaped mark in the result's colour, rising a little */
    const float ey = y - 4.0f - open * 5.0f;
    rr_rect(x - 3.0f, ey - 4.0f, 6.0f, 2.0f, tier_color(tier, 255));
    rr_rect(x - 4.0f, ey - 2.0f, 8.0f, 5.0f, tier_color(tier, 255));
    rr_rect(x - 3.0f, ey + 3.0f, 6.0f, 2.0f, tier_color(tier, 255));
  }
  if (picked) {
    const rr_color w = RR_RGBA(255, 255, 255, 255);
    rr_rect(x - 13.0f, y - 14.0f, 26.0f, 1.0f, w);
    rr_rect(x - 13.0f, y + 13.0f, 26.0f, 1.0f, w);
    rr_rect(x - 13.0f, y - 14.0f, 1.0f, 28.0f, w);
    rr_rect(x + 12.0f, y - 14.0f, 1.0f, 28.0f, w);
  }
}

static void draw_top(void) {
  if (ui_space(UI_PHYS_TOP)) rr_rect(0, 0, 400, 240, RR_RGBA(0, 0, 0, 255));
  if (!ui_space(UI_FIELD)) return;
  rr_rect(0, 0, 320, 180, RR_RGBA(0x20, 0x24, 0x40, 255));
  rr_rect(0, 0, 320, 4, RR_RGBA(0x38, 0x3c, 0x60, 255));
  rr_rect(144, 0, 32, 10, RR_RGBA(0x50, 0x54, 0x78, 255)); /* the chute */
  rr_rect(40, 176, 240, 4, RR_RGBA(0x38, 0x3c, 0x60, 255));
  const int n = C.m.fx.n;
  for (int i = 0; i < n; i++) {
    const cap_body* b = &C.m.body[i];
    if (b->phase == 0) continue;
    draw_capsule(b->x, b->y, C.m.fx.item[i].tier, b->open, C.m.stage == CAP_SETTLED && i == C.sel);
  }
  char t[64];
  if (C.m.stage == CAP_SETTLED) {
    snprintf(t, sizeof t, "%d result%s", n, n == 1 ? "" : "s");
    centred(160, 8, t, TS_WINDOW, 0);
  } else if (C.m.stage == CAP_IDLE) {
    centred(160, 70, "Capsule machine", TS_SUMMARY, 0);
    centred(160, 96, "A preview of the animation. Nothing is bought.", TS_INSTRUCTIONS_TEXT, RR_RGBA(0xc8, 0xc8, 0xc8, 255));
  }
}

static void draw_knob(float angle) {
  rr_draw_opts o = RR_DEFAULT_OPTS;
  o.origin_x = o.origin_y = 0.5f;
  o.scale_x = o.scale_y = 3.0f;
  o.angle = angle;
  if (img_has("images/egg/gacha_knob")) {
    img_draw_opts("images/egg/gacha_knob", NULL, KNOB_X, KNOB_Y, &o);
    return;
  }
  /* no art in the packs: a disc and a slot */
  static const int hw[4] = {20, 18, 16, 8};
  for (int k = -3; k <= 3; k++) rr_rect(KNOB_X - (float)hw[k < 0 ? -k : k], KNOB_Y + (float)k * 6.0f - 3.0f, (float)(2 * hw[k < 0 ? -k : k]), 6.0f, RR_RGBA(0xe0, 0xe0, 0xe8, 255));
  rr_rect(KNOB_X - 16.0f, KNOB_Y - 2.0f, 32.0f, 4.0f, RR_RGBA(0x40, 0x40, 0x50, 255));
}

static void draw_bottom(void) {
  if (!ui_space(UI_PHYS_BOTTOM)) return;
  rr_rect(0, 0, 320, 240, RR_RGBA(0x10, 0x10, 0x18, 255));
  ui_window(1, 1, 318, 24, WIN_NORMAL);
  line(10, 5, "Capsule machine (preview)", TS_WINDOW, 0);
  char t[96];
  if (C.m.stage == CAP_SETTLED) {
    ui_window(1, LIST_Y - 6, 318, LIST_ROWS * ROW_H + 12, WIN_NORMAL);
    for (int i = 0; i < LIST_ROWS && C.scroll + i < C.m.fx.n; i++) {
      const int k = C.scroll + i;
      const float y = (float)(LIST_Y + i * ROW_H);
      if (k == C.sel) rr_rect(4, y - 2, 312, ROW_H - 2, RR_RGBA(255, 255, 255, 30));
      rr_rect(12, y + 3, 10, 10, tier_color(C.m.fx.item[k].tier, 255));
      snprintf(t, sizeof t, "%d. %s", k + 1, C.m.fx.item[k].label);
      line(30, y, t, TS_WINDOW, 0);
      line(30, y + 12, TIER_NAME[C.m.fx.item[k].tier], TS_INSTRUCTIONS_TEXT, RR_RGBA(0xa8, 0xa8, 0xa8, 255));
    }
    if (C.scroll > 0) line(232, 28, "more above", TS_INSTRUCTIONS_TEXT, RR_RGBA(0xa0, 0xa0, 0xa0, 255));
    if (C.scroll + LIST_ROWS < C.m.fx.n) {
      snprintf(t, sizeof t, "%d more below", C.m.fx.n - C.scroll - LIST_ROWS);
      line(232, LIST_Y + LIST_ROWS * ROW_H + 8, t, TS_INSTRUCTIONS_TEXT, RR_RGBA(0xa0, 0xa0, 0xa0, 255));
    }
    line(10, 190, "These results were fixed before the machine ran.", TS_INSTRUCTIONS_TEXT, RR_RGBA(0xc8, 0xc8, 0xc8, 255));
    line(10, 224, "UP/DOWN: look   A: close", TS_INSTRUCTIONS_TEXT, 0);
    return;
  }
  draw_knob(C.m.knob * 6.2831853f);
  if (C.m.stage == CAP_IDLE) {
    centred(160, 52, "Pull the knob to run the machine.", TS_WINDOW, 0);
    centred(160, 76, "A preview with fixed results.", TS_INSTRUCTIONS_TEXT, RR_RGBA(0xc8, 0xc8, 0xc8, 255));
    line(10, 224, "A: pull   B: back", TS_INSTRUCTIONS_TEXT, 0);
  } else {
    centred(160, 52, "Here they come...", TS_WINDOW, 0);
    line(10, 224, "B or START: skip to the results", TS_INSTRUCTIONS_TEXT, 0);
  }
}

static void cs_draw(ui_screen* s, rr_screen which) {
  (void)s;
  if (which == RR_TOP)
    draw_top();
  else
    draw_bottom();
}

static ui_screen g_screen = {"capsule-preview", UI_FADE, cs_enter, cs_leave, NULL, cs_input, cs_update, cs_draw, NULL};

bool capsule_screen_open(const caps_args* a) {
  if (!a || !cap_fixture_valid(a->fixture)) return false;
  memset(&C, 0, sizeof C);
  cap_init(&C.m, a->fixture);
  C.a = *a;
  C.a.fixture = NULL; /* the model holds the copy */
  ui_set(&g_screen);
  return true;
}
