/* The end of a run (see result_screen.h). */
#include "result_screen.h"

#include <stdio.h>
#include <string.h>

#include "gm_enum_names.h"
#include "i18n.h"
#include "postmortem_screen.h"
#include "run_summary.h"
#include "ui.h"
#include "ui_text.h"
#include "ui_widgets.h"

static struct {
  result_args a;
  msgbox msg;
  bool done;
} R;

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
  const result_args again = R.a; /* result_screen_open clears R */
  result_screen_open(&again);
}

static bool rs_input(ui_screen* s, ui_button b) {
  (void)s;
  if (R.done) return true;
  if (msgbox_input(&R.msg, b)) {
    if (msgbox_done(&R.msg)) {
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
  ui_window(1, 239 - 42, 318, 42, WIN_NORMAL);
  msgbox_draw(&R.msg, 8, 239 - 42 + 8);
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
  memset(&R, 0, sizeof R);
  R.a = *a;
  char line[160], wrapped[256];
  if (a->victory)
    snprintf(line, sizeof line, "You cleared the run at wave %d!", a->run.wave);
  else
    snprintf(line, sizeof line, "You were defeated at wave %d.", a->run.wave);
  text_opts o = text_defaults();
  o.wrap_w = 300;
  text_wrap(TS_WINDOW, line, &o, wrapped, sizeof wrapped);
  msgbox_init(&R.msg, TS_WINDOW, 300, 2);
  msgbox_show(&R.msg, wrapped, -1, true);
  ui_set(&g_screen);
}
