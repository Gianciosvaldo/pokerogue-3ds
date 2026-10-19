/* The end of a run (see result_screen.h). */
#include "result_screen.h"

#include <stdio.h>
#include <string.h>

#include "gm_enum_names.h"
#include "i18n.h"
#include "postmortem_screen.h"
#include <math.h>

#include "run_summary.h"
#include "toast_view.h"
#include "ui.h"
#include "ui_text.h"
#include "ui_widgets.h"

/* AchvBar: it slides in over 500 ms, is in place until 10 s after it began (showAchv's delayedCall(10000, hide)), slides out over 500 ms */
#define TOAST_IN_MS 500.0f
#define TOAST_OUT_AT_MS 10000.0f
#define TOAST_OUT_MS 500.0f

static struct {
  result_args a;
  msgbox msg;
  bool done;
  char note[192]; /* a.note copied: the caller's text may be gone by the time the screen is drawn */
  wg_gains gains;           /* a.gains copied */
  int msgs[WG_MAX], n_msgs; /* the gains that are messages, in order; page is the one in the box (-1 = the line of the result) */
  int page;
  int toasts[WG_MAX], n_toasts; /* the gains that are bars */
  int toast;                    /* the bar on screen (index into toasts), == n_toasts when none is */
  float toast_ms;               /* how long it has been up */
  wg_toast info;                /* its words, and what tv_draw gets */
  tv_info tv;
} R;

/* the box and its window grow from two lines to three when there are messages to tell: a first ribbon is two long lines */
#define BOX_LINES_PLAIN 2
#define BOX_LINES_TALL 3
#define BOX_H_PLAIN 42.0f
#define BOX_H_TALL 58.0f

static void show_page_text(const char* text) {
  char wrapped[512];
  text_opts o = text_defaults();
  o.wrap_w = 300;
  text_wrap(TS_WINDOW, text, &o, wrapped, sizeof wrapped);
  msgbox_show(&R.msg, wrapped, -1, true);
}

/* the next message of the gains: UnlockPhase / RibbonModifierRewardPhase / GameOverModifierRewardPhase play the level up fanfare with it */
static void show_gain(int page) {
  char text[256];
  R.page = page;
  wg_message_of(&R.gains.item[R.msgs[page]], text, sizeof text);
  ui_play_sound("se/level_up_fanfare");
  show_page_text(text);
}

static void start_toast(int i) {
  R.toast = i;
  R.toast_ms = 0;
  if (i >= R.n_toasts) return;
  wg_toast_of(&R.gains.item[R.toasts[i]], &R.info);
  R.tv = (tv_info){R.info.title, R.info.desc, R.info.score, R.info.icon, R.info.tier};
  ui_play_sound("se/achv");
}

static void leave_result(void) {
  R.done = true;
  if (R.a.on_done) R.a.on_done(R.a.arg);
}

/* the post-mortem's buttons: A leaves for the title, B comes back to this screen with its line waiting again */
static void on_postmortem(pm_exit how, void* arg) {
  (void)arg;
  if (how == PM_CONTINUE) {
    leave_result();
    return;
  }
  result_args again = R.a; /* result_screen_open clears R */
  again.gains = NULL;      /* what the win gave was told before the post-mortem */
  result_screen_open(&again);
}

static bool rs_input(ui_screen* s, ui_button b) {
  (void)s;
  if (R.done) return true;
  if (msgbox_input(&R.msg, b)) {
    if (msgbox_done(&R.msg)) {
      if (R.page + 1 < R.n_msgs) {
        show_gain(R.page + 1);
        return true;
      }
      const pm_args pm = {R.a.report, on_postmortem, NULL};
      if (!postmortem_open(&pm)) leave_result();
    }
    return true;
  }
  return false;
}

static void rs_update(ui_screen* s, float dt) {
  (void)s;
  msgbox_update(&R.msg, dt);
  if (R.toast < R.n_toasts) {
    R.toast_ms += dt;
    if (R.toast_ms >= TOAST_OUT_AT_MS + TOAST_OUT_MS) start_toast(R.toast + 1);
  }
}

/* the bar on the top screen: Sine.easeOut in, Sine.easeIn out */
static void draw_toast(void) {
  if (R.toast >= R.n_toasts) return;
  float slide = 1.0f;
  if (R.toast_ms < TOAST_IN_MS)
    slide = sinf(R.toast_ms / TOAST_IN_MS * 1.5707963f);
  else if (R.toast_ms >= TOAST_OUT_AT_MS)
    slide = cosf((R.toast_ms - TOAST_OUT_AT_MS) / TOAST_OUT_MS * 1.5707963f); /* 1 - Sine.easeIn */
  tv_draw(&R.tv, slide);
}

static void draw_top(void) {
  if (ui_space(UI_PHYS_TOP)) rr_rect(0, 0, 400, 240, RR_RGBA(0, 0, 0, 255));
  if (!ui_space(UI_FIELD)) return;
  text_opts o = text_defaults();
  o.origin_x = o.origin_y = 0.5f;
  o.font_px = 128;
  if (R.a.victory) {
    /* addImage(0, 0, "end_m") with origin 0 and scale 0.5; the text at (width / 2, height - 16) */
    if (img_has("images/cg/end_m")) {
      rr_draw_opts d = RR_DEFAULT_OPTS;
      d.scale_x = d.scale_y = 0.5f;
      img_draw_opts("images/cg/end_m", NULL, 0, 0, &d);
    }
    char t[64];
    i18n_t(t, sizeof t, "battle:congratulations", NULL);
    text_draw(TS_SUMMARY, 160, 164, t, &o);
    draw_toast();
    return;
  }
  char name[48], key[96];
  gm_enum_lower(GM_E_BIOME_ID, R.a.run.biome, name, sizeof name);
  if (name[0]) {
    snprintf(key, sizeof key, "images/arenas/%s_bg", name);
    if (img_has(key)) img_draw(key, NULL, 0, 0, 0, 0);
  }
  rr_rect(0, 0, 320, 180, RR_RGBA(0, 0, 0, 160));
  text_draw(TS_SUMMARY, 160, 90, "Game Over", &o);
}

static void draw_bottom(void) {
  if (!ui_space(UI_PHYS_BOTTOM)) return;
  rr_rect(0, 0, 320, 240, RR_RGBA(0x10, 0x10, 0x18, 255));
  ui_window(8, 8, 304, 160, WIN_NORMAL);
  rsum_draw(&R.a.run, 22, 18);
  /* something that was not stored (the custom file): small print in the gap above the line, the 8 px pixel font that is legible here */
  if (R.note[0]) {
    text_opts o = text_defaults();
    o.wrap_w = 300;
    char wrapped[256];
    text_wrap(TS_INSTRUCTIONS_TEXT, R.note, &o, wrapped, sizeof wrapped);
    text_draw(TS_INSTRUCTIONS_TEXT, 10, 170, wrapped, &o);
  }
  const float box_h = R.n_msgs > 0 ? BOX_H_TALL : BOX_H_PLAIN;
  ui_window(1, 239 - box_h, 318, box_h, WIN_NORMAL);
  msgbox_draw(&R.msg, 8, 239 - box_h + 8);
}

static void rs_draw(ui_screen* s, rr_screen which) {
  (void)s;
  if (which == RR_TOP)
    draw_top();
  else
    draw_bottom();
}

static ui_screen g_screen = {"run-result", UI_FADE, NULL, NULL, NULL, rs_input, rs_update, rs_draw, NULL};

void result_screen_open(const result_args* a) {
  /* a->note may point into R itself (the post-mortem's B comes back here with R.a): take it out before R is cleared */
  char keep[sizeof R.note];
  keep[0] = 0;
  if (a->note) snprintf(keep, sizeof keep, "%s", a->note);
  const result_args args = *a;
  wg_gains gains;
  gains.n = 0;
  if (a->gains) gains = *a->gains;
  memset(&R, 0, sizeof R);
  R.a = args;
  R.a.gains = NULL;
  memcpy(R.note, keep, sizeof R.note);
  R.a.note = R.note[0] ? R.note : NULL;
  /* the bars and the unlocks are a win's: a defeat is given none and tells none */
  if (a->victory && gains.n > 0 && gains.n <= WG_MAX) R.gains = gains;
  for (int i = 0; i < R.gains.n; i++) {
    if (wg_is_toast(&R.gains.item[i]))
      R.toasts[R.n_toasts++] = i;
    else
      R.msgs[R.n_msgs++] = i;
  }
  R.page = -1;
  char line[160], wrapped[256];
  if (a->victory)
    snprintf(line, sizeof line, "You cleared the run at wave %d!", a->run.wave);
  else
    snprintf(line, sizeof line, "You were defeated at wave %d.", a->run.wave);
  text_opts o = text_defaults();
  o.wrap_w = 300;
  text_wrap(TS_WINDOW, line, &o, wrapped, sizeof wrapped);
  msgbox_init(&R.msg, TS_WINDOW, 300, R.n_msgs > 0 ? BOX_LINES_TALL : BOX_LINES_PLAIN);
  msgbox_show(&R.msg, wrapped, -1, true);
  start_toast(0);
  ui_set(&g_screen);
}
