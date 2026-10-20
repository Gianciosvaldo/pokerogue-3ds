/* The screen stack, input, timers, fades and drawing spaces (see ui.h). */
#include "ui.h"

#include <3ds.h>
#include <stdio.h>
#include <string.h>

#include "bgm.h"
#include "snd.h"
#include "tween.h"

#define STACK_MAX 16
#define TIMER_MAX 48
#define REPEAT_MS 250.0f /* upstream inputs-controller repeatInputDelayMillis */
#define FADE_MS 250.0f
#define FADE_HOLD_MS 100.0f

static ui_screen* g_stack[STACK_MAX];
static int g_depth;
static bool g_quit;
static float g_now;
static float g_real;          /* ms since boot at 1x: input repeat, music */
static float g_speed = 3;     /* the screens' clock over real time (run_settings.h: upstream's gameSpeed, Normal 3) */
static uint32_t g_frame;
static u64 g_last_ms;
static rr_screen g_drawing;

/* ---- timers ---- */
typedef struct {
  int id;
  float at;
  void (*fn)(void*);
  void* arg;
} timer;
static timer g_timers[TIMER_MAX];
static int g_next_timer = 1;

int ui_after(float ms, void (*fn)(void* arg), void* arg) {
  for (int i = 0; i < TIMER_MAX; i++) {
    if (!g_timers[i].id) {
      g_timers[i] = (timer){g_next_timer++, g_now + ms, fn, arg};
      if (g_next_timer <= 0) g_next_timer = 1;
      return g_timers[i].id;
    }
  }
  return 0;
}

void ui_timer_cancel(int id) {
  if (!id) return;
  for (int i = 0; i < TIMER_MAX; i++)
    if (g_timers[i].id == id) g_timers[i].id = 0;
}

static void run_timers(void) {
  for (int i = 0; i < TIMER_MAX; i++) {
    if (g_timers[i].id && g_timers[i].at <= g_now) {
      timer t = g_timers[i];
      g_timers[i].id = 0;
      t.fn(t.arg);
    }
  }
}

float ui_now(void) { return g_now; }

void ui_set_game_speed(float scale) { g_speed = scale > 0 ? scale : 1; }
float ui_game_speed(void) { return g_speed; }
uint32_t ui_frame(void) { return g_frame; }

/* ---- fades ---- */
typedef enum { FADE_NONE, FADE_OUT, FADE_HOLD, FADE_IN } fade_phase;
static struct {
  fade_phase phase;
  float t, out_ms, hold_ms, in_ms;
  float alpha;
  void (*at_black)(void*);
  void* arg;
} g_fade;

void ui_fade(float out_ms, float hold_ms, float in_ms, void (*at_black)(void* arg), void* arg) {
  if (g_fade.phase != FADE_NONE) {
    /* already fading: run the callback at this fade's black point instead of stacking fades */
    if (at_black) at_black(arg);
    return;
  }
  g_fade.phase = FADE_OUT;
  g_fade.t = 0;
  g_fade.out_ms = out_ms;
  g_fade.hold_ms = hold_ms;
  g_fade.in_ms = in_ms;
  g_fade.at_black = at_black;
  g_fade.arg = arg;
  g_fade.alpha = 0;
}

bool ui_busy(void) { return g_fade.phase != FADE_NONE; }

static void fade_update(float dt) {
  switch (g_fade.phase) {
    case FADE_NONE:
      return;
    case FADE_OUT:
      g_fade.t += dt;
      g_fade.alpha = g_fade.out_ms > 0 ? ease(EASE_SINE_OUT, g_fade.t / g_fade.out_ms) : 1;
      if (g_fade.t >= g_fade.out_ms) {
        g_fade.alpha = 1;
        g_fade.phase = FADE_HOLD;
        g_fade.t = 0;
        if (g_fade.at_black) {
          void (*fn)(void*) = g_fade.at_black;
          g_fade.at_black = NULL;
          fn(g_fade.arg);
        }
      }
      return;
    case FADE_HOLD:
      g_fade.t += dt;
      if (g_fade.t >= g_fade.hold_ms) {
        g_fade.phase = FADE_IN;
        g_fade.t = 0;
      }
      return;
    case FADE_IN:
      g_fade.t += dt;
      g_fade.alpha = g_fade.in_ms > 0 ? 1 - ease(EASE_SINE_IN, g_fade.t / g_fade.in_ms) : 0;
      if (g_fade.t >= g_fade.in_ms) {
        g_fade.alpha = 0;
        g_fade.phase = FADE_NONE;
      }
      return;
  }
}

/* ---- the stack ---- */
static void do_set(void* arg) {
  ui_screen* s = arg;
  if (g_depth > 0) {
    ui_screen* old = g_stack[g_depth - 1];
    g_depth--;
    if (old->leave) old->leave(old);
  }
  if (g_depth < STACK_MAX) {
    g_stack[g_depth++] = s;
    if (s->enter) s->enter(s);
  }
}

static void do_push(void* arg) {
  ui_screen* s = arg;
  if (g_depth >= STACK_MAX) return;
  g_stack[g_depth++] = s;
  if (s->enter) s->enter(s);
}

static void do_pop(void* arg) {
  (void)arg;
  if (g_depth <= 1) return;
  ui_screen* old = g_stack[--g_depth];
  if (old->leave) old->leave(old);
  ui_screen* top = g_stack[g_depth - 1];
  if (top->resume) top->resume(top);
}

static bool wants_fade(const ui_screen* a, const ui_screen* b) {
  return (a && (a->flags & UI_FADE)) || (b && (b->flags & UI_FADE));
}

void ui_set(ui_screen* s) {
  if (wants_fade(ui_top(), s))
    ui_fade(FADE_MS, FADE_HOLD_MS, FADE_MS, do_set, s);
  else
    do_set(s);
}

void ui_push(ui_screen* s) {
  if (s->flags & UI_FADE)
    ui_fade(FADE_MS, FADE_HOLD_MS, FADE_MS, do_push, s);
  else
    do_push(s);
}

void ui_pop(void) {
  ui_screen* top = ui_top();
  if (top && (top->flags & UI_FADE))
    ui_fade(FADE_MS, FADE_HOLD_MS, FADE_MS, do_pop, NULL);
  else
    do_pop(NULL);
}

void ui_pop_to(ui_screen* s) {
  while (g_depth > 1 && g_stack[g_depth - 1] != s) do_pop(NULL);
}

ui_screen* ui_top(void) { return g_depth ? g_stack[g_depth - 1] : NULL; }
int ui_depth(void) { return g_depth; }

/* ---- input ---- */
static const struct {
  u32 key;
  ui_button b;
} KEYMAP[] = {
    {KEY_UP, BTN_UP},          {KEY_DOWN, BTN_DOWN},     {KEY_LEFT, BTN_LEFT},
    {KEY_RIGHT, BTN_RIGHT},    {KEY_A, BTN_ACTION},      {KEY_B, BTN_CANCEL},
    {KEY_START, BTN_MENU},     {KEY_SELECT, BTN_STATS},  {KEY_Y, BTN_CYCLE_NATURE},
    {KEY_X, BTN_CYCLE_TERA},   {KEY_L, BTN_CYCLE_FORM},  {KEY_R, BTN_CYCLE_SHINY},
    {KEY_ZL, BTN_CYCLE_GENDER}, {KEY_ZR, BTN_CYCLE_ABILITY},
};
#define NKEYS ((int)(sizeof KEYMAP / sizeof KEYMAP[0]))
static float g_held_since[NKEYS];
static float g_next_repeat[NKEYS];

static const char* const BTN_NAMES[BTN_COUNT_] = {
    "UP",          "DOWN",       "LEFT",         "RIGHT",         "SUBMIT",       "ACTION",
    "CANCEL",      "MENU",       "STATS",        "CYCLE_SHINY",   "CYCLE_FORM",   "CYCLE_GENDER",
    "CYCLE_ABILITY", "CYCLE_NATURE", "CYCLE_TERA", "SPEED_UP",    "SLOW_DOWN",
};
const char* ui_button_name(ui_button b) { return (unsigned)b < BTN_COUNT_ ? BTN_NAMES[b] : "?"; }

#define INJECT_MAX 16
static ui_button g_inject[INJECT_MAX];
static int g_n_inject;
void ui_inject(ui_button b) {
  if (g_n_inject < INJECT_MAX) g_inject[g_n_inject++] = b;
}

static bool g_repeating; /* the button being delivered is an auto-repeat of a held one */
bool ui_input_repeating(void) { return g_repeating; }

static void deliver(ui_button b) {
  if (ui_busy()) return;
  ui_screen* s = ui_top();
  if (s && s->input) s->input(s, b);
}

#define TOUCH_MAX 8
static struct {
  uint32_t frame;
  float x, y;
} g_dbg_touch[TOUCH_MAX];
static int g_n_dbg_touch;
void ui_debug_touch(uint32_t frame, float x, float y) {
  if (g_n_dbg_touch >= TOUCH_MAX) return;
  g_dbg_touch[g_n_dbg_touch].frame = frame;
  g_dbg_touch[g_n_dbg_touch].x = x;
  g_dbg_touch[g_n_dbg_touch++].y = y;
}
static float g_inj_touch[INJECT_MAX][2];
static int g_n_inj_touch;
void ui_inject_touch(float x, float y) {
  if (g_n_inj_touch < INJECT_MAX) {
    g_inj_touch[g_n_inj_touch][0] = x;
    g_inj_touch[g_n_inj_touch++][1] = y;
  }
}

static void deliver_touch(float x, float y) {
  if (ui_busy()) return;
  ui_screen* s = ui_top();
  if (s && s->touch) s->touch(s, x, y);
}

static void read_input(void) {
  hidScanInput();
  if (hidKeysDown() & KEY_TOUCH) {
    touchPosition t;
    hidTouchRead(&t);
    deliver_touch((float)t.px, (float)t.py);
  }
  for (int i = 0; i < g_n_dbg_touch; i++)
    if (g_dbg_touch[i].frame == ui_frame()) deliver_touch(g_dbg_touch[i].x, g_dbg_touch[i].y);
  for (int i = 0; i < g_n_inj_touch; i++) deliver_touch(g_inj_touch[i][0], g_inj_touch[i][1]);
  g_n_inj_touch = 0;
  u32 down = hidKeysDown(), held = hidKeysHeld();
  /* START + SELECT together quits (the app's own exit; HOME still works as usual) */
  if ((held & (KEY_START | KEY_SELECT)) == (KEY_START | KEY_SELECT) && (down & (KEY_START | KEY_SELECT))) {
    g_quit = true;
    return;
  }
  for (int i = 0; i < NKEYS; i++) {
    u32 k = KEYMAP[i].key;
    bool repeats = KEYMAP[i].b != BTN_MENU && KEYMAP[i].b != BTN_STATS;
    if (down & k) {
      g_held_since[i] = g_real;
      g_next_repeat[i] = g_real + REPEAT_MS;
      deliver(KEYMAP[i].b);
    } else if ((held & k) && repeats && g_next_repeat[i] > 0 && g_real >= g_next_repeat[i]) {
      g_next_repeat[i] += REPEAT_MS;
      g_repeating = true;
      deliver(KEYMAP[i].b);
      g_repeating = false;
    } else if (!(held & k)) {
      g_next_repeat[i] = 0;
    }
  }
  for (int i = 0; i < g_n_inject; i++) deliver(g_inject[i]);
  g_n_inject = 0;
}

/* ---- sounds ---- */
void ui_play_sound(const char* key) {
  if (!key || !snd_ready()) return;
  char k[96];
  snprintf(k, sizeof k, "audio/%s", key);
  snd_sfx_play(k);
}
void ui_play_select(void) { ui_play_sound("ui/select"); }
void ui_play_error(void) { ui_play_sound("ui/error"); }

/* ---- spaces ---- */
bool ui_space(ui_space_id sp) {
  rr_screen want = (sp == UI_BOTTOM_UI || sp == UI_BOTTOM_CANVAS || sp == UI_BOTTOM_CANVAS_UI || sp == UI_PHYS_BOTTOM)
                       ? RR_BOTTOM
                       : RR_TOP;
  if (want != g_drawing) return false;
  int x = 0, y = 0;
  switch (sp) {
    case UI_FIELD:
    case UI_TOP_CANVAS:
      x = 40, y = 30;
      break;
    case UI_FIELD_UI:
    case UI_TOP_CANVAS_UI:
      x = 40, y = 210;
      break;
    case UI_BOTTOM_UI:
      x = 0, y = 240;
      break;
    case UI_BOTTOM_CANVAS:
      x = 0, y = 30;
      break;
    case UI_BOTTOM_CANVAS_UI:
      x = 0, y = 210;
      break;
    case UI_PHYS_TOP:
    case UI_PHYS_BOTTOM:
      break;
  }
  rr_set_origin(g_drawing, x, y);
  return true;
}

rr_screen ui_drawing(void) { return g_drawing; }

/* ---- loop ---- */
void ui_init(void) {
  g_depth = 0;
  g_now = 0;
  g_frame = 0;
  g_last_ms = osGetTime();
  memset(g_timers, 0, sizeof g_timers);
  memset(&g_fade, 0, sizeof g_fade);
}

void ui_request_quit(void) { g_quit = true; }

static int first_drawn(void) {
  int i = g_depth - 1;
  while (i > 0 && (g_stack[i]->flags & UI_OVERLAY)) i--;
  return i < 0 ? 0 : i;
}

bool ui_tick(void) {
  if (g_quit || !aptMainLoop()) return false;
  u64 now = osGetTime();
  float dt = (float)(now - g_last_ms);
  g_last_ms = now;
  if (dt > 50) dt = 50; /* a slow frame (SD load) doesn't skip animations */
  if (dt < 0) dt = 0;
  const float real_dt = dt;
  g_real += real_dt;
  dt *= g_speed; /* upstream's applyGameSpeedMult: every tween, timer and text delay over gameSpeed */
  g_now += dt;
  g_frame++;

  read_input();
  run_timers();
  fade_update(dt);
  int from = first_drawn();
  for (int i = from; i < g_depth; i++)
    if (g_stack[i]->update) g_stack[i]->update(g_stack[i], dt);
  bgm_tick(real_dt); /* music plays at its own pace (upstream scales only its fades) */
  snd_update();

  rr_frame_begin();
  for (int scr = 0; scr < 2; scr++) {
    g_drawing = (rr_screen)scr;
    rr_target(g_drawing, RR_RGBA(0, 0, 0, 255));
    from = first_drawn();
    for (int i = from; i < g_depth; i++)
      if (g_stack[i]->draw) g_stack[i]->draw(g_stack[i], g_drawing);
    if (g_fade.alpha > 0) {
      rr_set_origin(g_drawing, 0, 0);
      int a = (int)(g_fade.alpha * 255 + 0.5f);
      rr_rect(0, 0, 400, 240, RR_RGBA(0, 0, 0, a > 255 ? 255 : a));
    }
  }
  rr_frame_end();
  return !g_quit;
}
