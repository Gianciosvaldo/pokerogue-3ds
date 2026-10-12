/* The starter select screen on the UI stack (see starter_host.h). */
#include "starter_host.h"

#include <stdio.h>
#include <string.h>

#include "bgm.h"
#include "i18n.h"
#include "starter_data.h"
#include "ui.h"

static struct {
  const gc_system* sys;
  sh_events ev;
  sm_entry entries[SS_MAX_ENTRIES];
  int n;
  uint32_t rng;
} H;

/* ---- what the screen asks the game ---- */

static void describe(void* ctx, uint16_t species, starter_info* out) {
  (void)ctx;
  ss_describe(H.sys, species, out);
}

/* Math.floor(Math.random() * n) for Randomize */
static int rnd(void* ctx, int n) {
  (void)ctx;
  H.rng ^= H.rng << 13;
  H.rng ^= H.rng >> 17;
  H.rng ^= H.rng << 5;
  return n > 0 ? (int)(H.rng % (uint32_t)n) : 0;
}

/* The team was confirmed: game-core's rules decide whether it is a team. The screen only offers what the dex holds, so a refusal
 * means the dex and game-core disagree about a default (the message says what game-core said). */
static bool on_start(void* ctx, const uint16_t* species, int n) {
  (void)ctx;
  gc_starter team[6];
  const char* why = NULL;
  if (!ss_team(H.sys, species, n, team, &why)) {
    char text[200], base[96];
    i18n_t(base, sizeof base, "starterSelectUiHandler:invalidParty", NULL);
    snprintf(text, sizeof text, "%s (%s)", base, why ? why : "");
    starter_message(text);
    return false;
  }
  if (H.ev.on_start) H.ev.on_start(team, n, H.ev.arg);
  return true;
}

static void on_exit(void* ctx) {
  (void)ctx;
  if (H.ev.on_exit) H.ev.on_exit(H.ev.arg);
}

/* ---- the screen ---- */

static bool sh_input(ui_screen* s, ui_button b) {
  (void)s;
  return starter_input((mui_button)b); /* both are upstream's Button enum, in order */
}

static void sh_update(ui_screen* s, float dt) {
  (void)s;
  mui_tick(dt);
  starter_update(dt);
}

static void sh_draw(ui_screen* s, rr_screen which) {
  (void)s;
  if (which == RR_BOTTOM) {
    if (ui_space(UI_PHYS_BOTTOM)) rr_rect(0, 0, 320, 240, RR_RGBA(0, 0, 0, 255));
    mui_set_origin(RR_BOTTOM, 0, 30); /* UI_BOTTOM_CANVAS */
    starter_draw();
  } else {
    if (ui_space(UI_PHYS_TOP)) rr_rect(0, 0, 400, 240, RR_RGBA(0, 0, 0, 255));
    mui_set_origin(RR_TOP, 40, 30); /* UI_TOP_CANVAS */
    starter_draw_top();
  }
}

static void sh_leave(ui_screen* s) {
  (void)s;
  starter_close();
}

/* SelectStarterPhase.start: the menu music */
static void sh_enter(ui_screen* s) {
  (void)s;
  bgm_on_starter_select();
}

static ui_screen g_screen = {"starter-select", UI_FADE, sh_enter, sh_leave, NULL, sh_input, sh_update, sh_draw, NULL};

bool sh_open(const gc_system* sys, const char* notice, const sh_events* ev) {
  H.sys = sys;
  H.ev = *ev;
  H.n = ss_list(sys, H.entries, SS_MAX_ENTRIES, NULL);
  if (H.n == 0) return false;
  H.rng = 0x9E3779B9u ^ (uint32_t)(ui_now() * 1000.0f) ^ (ui_frame() * 2654435761u);
  if (!H.rng) H.rng = 1;
  const starter_args args = {H.entries, H.n, gc_starter_budget(GAME_MODES_CLASSIC)};
  const starter_events events = {describe, on_start, on_exit, rnd, NULL};
  starter_open(&args, &events);
  ui_set(&g_screen);
  if (notice && *notice) starter_message(notice);
  return true;
}
