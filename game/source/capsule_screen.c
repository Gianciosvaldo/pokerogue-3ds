/* The capsule-machine presentation prototype (see capsule_screen.h). */
#include "capsule_screen.h"

#include <stdio.h>
#include <string.h>

#include "gacha_view.h"
#include "ui.h"
#include "ui_widgets.h"

#define DWELL_MS 300.0f       /* after the results are shown, how long before a button closes the screen */
#define LIST_ROWS 5
#define HATCH_FRAME_MS (1000.0f / 12.0f)  /* upstream's "open" and "close" animations of gacha_hatch: 4 frames at 12 a second */
#define HATCH_HOLD_MS 100.0f              /* kept open this long after the last capsule leaves the chute */

static const char* const TIER_NAME[CAP_TIERS] = {"Common", "Rare", "Epic", "Legendary"};
static struct {
  cap_model m;
  caps_args a;
  bool open;                 /* between open and leave */
  bool exited;               /* on_exit was called: no more input */
  bool live;                 /* a committed purchase (words only) */
  bool settled_fired;
  float since_settled;
  int sel;                   /* the result the list and the top screen point at */
  int scroll;
} C;

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

int capsule_hatch_frame(const cap_model* m) {
  if (!m || (m->stage != CAP_PULL && m->stage != CAP_DROP) || m->t < CAP_PULL_MS) return 1;
  float last = CAP_PULL_MS;
  for (int i = 0; i < m->fx.n; i++)
    if (m->body[i].spawn_ms > last) last = m->body[i].spawn_ms;
  const float full = CAP_PULL_MS + 3.0f * HATCH_FRAME_MS;   /* the fourth frame */
  const float close_at = last + HATCH_HOLD_MS > full ? last + HATCH_HOLD_MS : full;
  if (m->t < close_at) {
    const int f = 1 + (int)((m->t - CAP_PULL_MS) / HATCH_FRAME_MS);
    return f > 4 ? 4 : f;
  }
  const int f = 4 - (int)((m->t - close_at) / HATCH_FRAME_MS);
  return f < 1 ? 1 : f;
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

static void build_state(gv_reveal* v) {
  memset(v, 0, sizeof *v);
  v->stage = C.m.stage == CAP_SETTLED ? GV_REVEAL_SETTLED : (C.m.stage == CAP_IDLE ? GV_REVEAL_IDLE : GV_REVEAL_RUNNING);
  v->n = C.m.fx.n;
  for (int i = 0; i < C.m.fx.n; i++) {
    const cap_body* b = &C.m.body[i];
    v->egg[i] = (gv_egg){b->x, b->y, b->open, b->phase, C.m.fx.item[i].tier, C.m.stage == CAP_SETTLED && i == C.sel};
  }
  v->knob = C.m.knob;
  v->machine = C.a.machine;
  v->hatch = capsule_hatch_frame(&C.m);
  snprintf(v->title, sizeof v->title, "%s", C.live ? "Capsule machine" : "Capsule machine (preview)");
  const int n = C.m.fx.n;
  if (C.m.stage == CAP_SETTLED) {
    snprintf(v->top_title, sizeof v->top_title, C.live ? "%d egg%s" : "%d result%s", n, n == 1 ? "" : "s");
    for (int i = 0; i < LIST_ROWS && C.scroll + i < n; i++) {
      const int k = C.scroll + i;
      snprintf(v->row[v->n_rows].name, sizeof v->row[0].name, "%d. %s", k + 1, C.m.fx.item[k].label);
      snprintf(v->row[v->n_rows].tier_name, sizeof v->row[0].tier_name, "%s", TIER_NAME[C.m.fx.item[k].tier]);
      v->row[v->n_rows].tier = C.m.fx.item[k].tier;
      v->row[v->n_rows].selected = k == C.sel;
      v->n_rows++;
    }
    if (C.scroll > 0) snprintf(v->more_above, sizeof v->more_above, "more above");
    if (C.scroll + LIST_ROWS < n) snprintf(v->more_below, sizeof v->more_below, "%d more below", n - C.scroll - LIST_ROWS);
    if (C.live) {
      snprintf(v->note[0], sizeof v->note[0], "These eggs were decided when you pulled.");
      snprintf(v->note[1], sizeof v->note[1], "They are stored with your other eggs.");
    } else {
      snprintf(v->note[0], sizeof v->note[0], "These results were fixed before it ran.");
    }
    snprintf(v->hint, sizeof v->hint, "UP/DOWN: look   A: close");
  } else if (C.m.stage == CAP_IDLE) {
    snprintf(v->top_title, sizeof v->top_title, "Capsule machine");
    snprintf(v->top_sub, sizeof v->top_sub, "%s", C.live ? "Your eggs are ready." : "A preview of the animation. Nothing is bought.");
    snprintf(v->big, sizeof v->big, "%s", C.live ? "Pull the knob to see your eggs." : "Pull the knob to run the machine.");
    snprintf(v->small, sizeof v->small, "%s", C.live ? "They are paid for: nothing more is spent." : "A preview with fixed results.");
    snprintf(v->hint, sizeof v->hint, "A: pull   B: back");
  } else {
    snprintf(v->big, sizeof v->big, "Here they come...");
    snprintf(v->hint, sizeof v->hint, "B or START: skip to the results");
  }
}

static void cs_draw(ui_screen* s, rr_screen which) {
  (void)s;
  gv_reveal v;
  build_state(&v);
  if (which == RR_TOP) {
    if (ui_space(UI_PHYS_TOP)) rr_rect(0, 0, 400, 240, RR_RGBA(0, 0, 0, 255));
    gv_draw_reveal_top(&v);
  } else {
    if (ui_space(UI_PHYS_BOTTOM)) rr_rect(0, 0, 320, 240, RR_RGBA(0, 0, 0, 255));
    gv_draw_reveal_bottom(&v);
  }
}

static ui_screen g_screen = {"capsule-preview", UI_FADE, cs_enter, cs_leave, NULL, cs_input, cs_update, cs_draw, NULL};

bool capsule_screen_open(const caps_args* a) {
  if (!a || !cap_fixture_valid(a->fixture)) return false;
  memset(&C, 0, sizeof C);
  cap_init(&C.m, a->fixture);
  C.a = *a;
  C.a.fixture = NULL; /* the model holds the copy */
  C.live = a->live;
  if (a->autoplay) cap_pull(&C.m); /* the player already accepted: nothing more to decide */
  ui_set(&g_screen);
  return true;
}
