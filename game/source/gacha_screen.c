/* The capsule machine's menu (see gacha_screen.h). */
#include "gacha_screen.h"

#include <stdio.h>
#include <string.h>

#include "capsule_screen.h"
#include "gacha_host.h"
#include "gacha_view.h"
#include "ui.h"
#include "ui_text.h"
#include "ui_widgets.h"

#define SLIDE_MS 500.0f /* upstream: the machines move over 500 ms, Cubic.easeInOut */

typedef enum {
  GS_MENU,     /* choosing */
  GS_CONFIRM,  /* "Pull ...?" Yes / No, defaulting to No */
  GS_MESSAGE,  /* a refusal or an error, shown until a button */
  GS_RETRY,    /* a purchase is unfinished and the card refused: A tries it again */
} gs_mode;

static struct {
  gachas_args a;           /* kept across the capsule screen: closing it comes back here */
  uint32_t reveal_txn;     /* the pull the capsule screen is showing */
} K;

static struct {
  bool open;               /* between enter and leave */
  bool left;               /* on_exit was called: no more input */
  gs_mode mode;
  int machine, sel;
  float slide, slide_from, slide_t; /* the machine strip: where it is, where the move began, ms since */
  int yes;                 /* GS_CONFIRM: 1 = Yes; the question opens on No */
  gh_view view;
  char msg[PTX_WHY_LEN + 64];
} G;

static void refresh(void) { gh_view_of(&G.view); }

/* ------------------------------------------------------------------ the capsule screen */

static cap_fixture g_fixture; /* the capsule screen copies it when it opens */

static void on_cap_settled(void* arg) {
  (void)arg;
  /* whether or not the card takes the mark, the eggs are not shown again in this session; a refused mark can only repeat the same
   * eggs after a restart, never pull them again (gacha_host.h) */
  gh_reveal_done(K.reveal_txn);
}

static void on_cap_exit(caps_exit how, void* arg) {
  (void)how;
  (void)arg;
  gacha_screen_open(&K.a); /* which first gives a half-applied purchase another chance to reach the card */
}

/* Shows a committed pull. False when the screen could not (the result is not a pull that can be drawn). */
static bool reveal(const ptx_result* r) {
  if (!gh_fixture(r, &g_fixture)) return false;
  K.reveal_txn = r->txn;
  caps_args c;
  memset(&c, 0, sizeof c);
  c.fixture = &g_fixture;
  c.on_settled = on_cap_settled;
  c.on_exit = on_cap_exit;
  c.live = true;
  c.autoplay = true;
  return capsule_screen_open(&c);
}

/* ------------------------------------------------------------------ the menu */

static void to_menu(void) {
  G.mode = GS_MENU;
  G.yes = 0;
  G.msg[0] = 0;
  refresh();
}

static void tell(gs_mode mode, const char* text) {
  G.mode = mode;
  snprintf(G.msg, sizeof G.msg, "%s", text);
}

/* The outcome of a purchase (or of finishing one) as the screen's next step. */
static void after(const gh_outcome* o) {
  char text[PTX_WHY_LEN + 64];
  switch (o->status) {
    case GH_BOUGHT:
      if (reveal(&o->result)) return;
      /* cannot happen with a result the file validated; if it does, the eggs are stored, and not showing them must not trap the player */
      gh_reveal_done(o->result.txn);
      refresh();
      tell(GS_MESSAGE, "Your eggs are stored, but the machine could not show them.");
      return;
    case GH_RETRY_IO:
      refresh();
      gh_message(o, text, sizeof text);
      /* a purchase already begun is finished with A (the same transaction); one that could not even be begun is simply tried again */
      tell(ptx_pending() ? GS_RETRY : GS_MESSAGE, text);
      return;
    default:
      refresh();
      gh_message(o, text, sizeof text);
      tell(GS_MESSAGE, text[0] ? text : "Nothing was bought.");
      return;
  }
}

static void buy_now(void) {
  gh_outcome o;
  gh_buy(G.machine, G.sel, &o);
  after(&o);
}

static void leave_screen(void) {
  if (G.left) return;
  G.left = true;
  if (K.a.on_exit) K.a.on_exit(K.a.arg);
}

static bool gs_input(ui_screen* s, ui_button b) {
  (void)s;
  if (!G.open || G.left) return true;
  const bool nav = b == BTN_UP || b == BTN_DOWN || b == BTN_LEFT || b == BTN_RIGHT;
  if (ui_input_repeating() && !(nav && G.mode == GS_MENU)) return true; /* a held button does its action once */
  switch (G.mode) {
    case GS_MENU:
      if (b == BTN_LEFT || b == BTN_RIGHT) {
        G.machine = (G.machine + (b == BTN_RIGHT ? 1 : GH_MACHINES - 1)) % GH_MACHINES;
        G.slide_from = G.slide;
        G.slide_t = 0;
        ui_play_select();
        return true;
      }
      if (b == BTN_UP || b == BTN_DOWN) {
        const int to = G.sel + (b == BTN_DOWN ? 1 : -1);
        if (to >= 0 && to < GH_OPTIONS) {
          G.sel = to;
          ui_play_select();
        }
        return true;
      }
      if (b == BTN_ACTION || b == BTN_SUBMIT) {
        if (G.view.opt[G.sel].can_buy) {
          G.mode = GS_CONFIRM;
          G.yes = 0;
          ui_play_select();
        } else {
          ui_play_error();
        }
        return true;
      }
      if (b == BTN_CANCEL) {
        ui_play_select();
        leave_screen();
        return true;
      }
      return false;
    case GS_CONFIRM:
      if (b == BTN_UP || b == BTN_DOWN || b == BTN_LEFT || b == BTN_RIGHT) {
        G.yes = !G.yes;
        ui_play_select();
        return true;
      }
      if (b == BTN_ACTION || b == BTN_SUBMIT) {
        if (G.yes) {
          buy_now();
        } else {
          ui_play_select();
          to_menu();
        }
        return true;
      }
      if (b == BTN_CANCEL) {
        ui_play_select();
        to_menu();
        return true;
      }
      return false;
    case GS_MESSAGE:
      if (b == BTN_ACTION || b == BTN_SUBMIT || b == BTN_CANCEL) {
        ui_play_select();
        to_menu();
        return true;
      }
      return false;
    case GS_RETRY:
      if (b == BTN_ACTION || b == BTN_SUBMIT) {
        gh_outcome o;
        gh_resume(&o);
        after(&o);
        return true;
      }
      if (b == BTN_CANCEL) {
        ui_play_select();
        leave_screen();
        return true;
      }
      return false;
  }
  return false;
}

static void gs_enter(ui_screen* s) {
  (void)s;
  G.open = true; /* the screen is the top one now (ui_set may run leave() of an earlier opening first) */
}

static void gs_leave(ui_screen* s) {
  (void)s;
  G.open = false;
}

/* ------------------------------------------------------------------ drawing */

static void gs_update(ui_screen* s, float dt) {
  (void)s;
  if (!G.open || G.slide == (float)G.machine) return;
  G.slide_t += dt;
  float k = G.slide_t / SLIDE_MS;
  if (k >= 1.0f) {
    G.slide = (float)G.machine;
    return;
  }
  k = k < 0.5f ? 4.0f * k * k * k : 1.0f - (-2.0f * k + 2.0f) * (-2.0f * k + 2.0f) * (-2.0f * k + 2.0f) / 2.0f;
  G.slide = G.slide_from + ((float)G.machine - G.slide_from) * k;
}

/* text of any length in a window: broken at spaces into lines of at most `cols` characters, at most GV_MSG_LINES of them (the rest is cut) */
static void wrap_lines(gv_state* v, const char* text, int cols) {
  const char* p = text;
  v->n_lines = 0;
  while (*p && v->n_lines < GV_MSG_LINES) {
    while (*p == ' ') p++;
    if (!*p) break;
    int len = (int)strlen(p);
    if (len > cols) {
      len = cols;
      while (len > 0 && p[len] != ' ') len--;
      if (len == 0) len = cols;   /* a word longer than the line */
    }
    char* ln = v->lines[v->n_lines];
    const int n = len < (int)sizeof v->lines[0] - 4 ? len : (int)sizeof v->lines[0] - 4;
    memcpy(ln, p, (size_t)n);
    ln[n] = 0;
    p += len;
    while (*p == ' ') p++;
    if (v->n_lines == GV_MSG_LINES - 1 && *p) snprintf(ln + (n > 3 ? n - 3 : 0), 4, "...");
    v->n_lines++;
  }
}

static void countdown_text(char out[12]) { /* hh:mm: the plate's window is too narrow for the seconds at a readable size */
  const uint64_t day = 86400000u;
  const uint64_t left = day - gh_clock_ms() % day;
  snprintf(out, 12, "%02u:%02u", (unsigned)(left / 3600000u), (unsigned)(left % 3600000u / 60000u));
}

static void build_state(gv_state* v) {
  memset(v, 0, sizeof *v);
  v->slide = G.slide;
  v->machine = G.machine;
  v->sel = G.sel;
  v->yes = G.yes;
  v->mode = G.mode == GS_CONFIRM ? GV_CONFIRM : (G.mode == GS_MENU ? GV_MENU : GV_MESSAGE);
  snprintf(v->machine_name, sizeof v->machine_name, "%s", gh_machine_name(G.machine));
  snprintf(v->machine_note, sizeof v->machine_note, "%s", gh_machine_note(G.machine));
  for (int i = 0; i < GV_VOUCHERS && i < GH_VOUCHER_TYPES; i++) snprintf(v->voucher_count[i], sizeof v->voucher_count[i], "%u", (unsigned)G.view.vouchers[i]);
  snprintf(v->egg_count, sizeof v->egg_count, "Eggs %d / %d", G.view.n_eggs, G.view.capacity);
  for (int i = 0; i < GH_OPTIONS; i++) {
    const gh_option* o = &G.view.opt[i];
    gv_option* d = &v->opt[i];
    snprintf(d->eggs, sizeof d->eggs, "%d egg%s", o->rule.pulls, o->rule.pulls == 1 ? "" : "s");
    snprintf(d->price, sizeof d->price, "%d %s voucher%s", o->rule.cost, gh_voucher_name(o->rule.voucher), o->rule.cost == 1 ? "" : "s");
    snprintf(d->have, sizeof d->have, "have %u", (unsigned)o->have);
    snprintf(d->short_label, sizeof d->short_label, "x%d %d %s", o->rule.cost, o->rule.pulls, o->rule.pulls == 1 ? "Pull" : "Pulls");
    d->voucher = o->rule.voucher;
    d->can_buy = o->can_buy;
  }
  snprintf(v->up_label[0], sizeof v->up_label[0], "Move UP!");
  snprintf(v->up_label[1], sizeof v->up_label[1], "UP!");
  snprintf(v->up_label[2], sizeof v->up_label[2], "Shiny UP!");
  v->featured = gh_featured_species();
  countdown_text(v->countdown);

  if (G.mode == GS_MENU && !G.view.locked) {
    const gh_option* o = &G.view.opt[G.sel];
    if (o->refusal_key) snprintf(v->refusal, sizeof v->refusal, "%s", gh_refusal_text(o->refusal_key));
    snprintf(v->hint, sizeof v->hint, "L/R: machine  U/D: option  A: pull  B: back");
    snprintf(v->top_message, sizeof v->top_message, "Select a machine.");
    return;
  }
  snprintf(v->top_message, sizeof v->top_message, "%s", gh_machine_note(G.machine));
  if (G.mode == GS_CONFIRM) {
    const gh_option* o = &G.view.opt[G.sel];
    snprintf(v->lines[0], sizeof v->lines[0], "Pull %d egg%s from the %.20s", o->rule.pulls, o->rule.pulls == 1 ? "" : "s", gh_machine_name(G.machine));
    snprintf(v->lines[1], sizeof v->lines[1], "for %d %.12s voucher%s?", o->rule.cost, gh_voucher_name(o->rule.voucher), o->rule.cost == 1 ? "" : "s");
    v->n_lines = 2;
    snprintf(v->hint, sizeof v->hint, "A: choose   B: back");
  } else if (G.mode == GS_MENU) {
    wrap_lines(v, G.view.lock_text, 42);
    snprintf(v->hint, sizeof v->hint, "L/R: machine   B: back");
  } else {
    wrap_lines(v, G.msg, 42);
    snprintf(v->hint, sizeof v->hint, "%s", G.mode == GS_RETRY ? "A: try again   B: leave" : "A: back");
  }
}

static void gs_draw(ui_screen* s, rr_screen which) {
  (void)s;
  gv_state v;
  build_state(&v);
  if (which == RR_TOP) {
    if (ui_space(UI_PHYS_TOP)) rr_rect(0, 0, 400, 240, RR_RGBA(0, 0, 0, 255));
    gv_draw_top(&v);
  } else {
    if (ui_space(UI_PHYS_BOTTOM)) rr_rect(0, 0, 320, 240, RR_RGBA(0, 0, 0, 255));
    gv_draw_bottom(&v);
  }
}

static ui_screen g_screen = {"capsule-machine", UI_FADE, gs_enter, gs_leave, NULL, gs_input, gs_update, gs_draw, NULL};

bool gacha_screen_open(const gachas_args* a) {
  if (!a) return false;
  K.a = *a;
  gh_set_species_names(a->species_name);
  memset(&G, 0, sizeof G);
  ptx_settle(); /* a committed purchase left half applied (the card refused a file) is finished before anything else */
  /* what a restart left behind comes first: a purchase begun and not finished is finished (the same transaction), then a committed pull
   * that was never shown is shown. A pull that was finished and shown, whose file is still catching up, is not shown again. */
  gh_outcome o;
  gh_resume(&o);
  ptx_result r;
  if (o.status != GH_RETRY_IO && o.status != GH_UNUSABLE && o.status != GH_FATAL && gh_unrevealed(&r)) {
    if (reveal(&r)) return true;
    gh_reveal_done(r.txn);   /* a result that cannot be drawn must not trap the player in a loop */
  }
  refresh();
  if (o.status == GH_RETRY_IO) {
    char text[PTX_WHY_LEN + 64];
    gh_message(&o, text, sizeof text);
    tell(GS_RETRY, text);
  } else if (o.status == GH_UNUSABLE || o.status == GH_FATAL) {
    tell(GS_MESSAGE, o.why);
  }
  ui_set(&g_screen);
  return true;
}
