/* The capsule machine's menu (see gacha_screen.h). */
#include "gacha_screen.h"

#include <stdio.h>
#include <string.h>

#include "capsule_screen.h"
#include "gacha_host.h"
#include "ui.h"
#include "ui_text.h"
#include "ui_widgets.h"

#define ROW_H 28
#define LIST_Y 66

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

/* text of any length in a window: broken at spaces into lines of at most `cols` characters, at most `max_lines` of them (the rest is cut) */
static void wrapped(float x, float y, const char* text, int cols, int max_lines) {
  char ln[96];
  const char* p = text;
  for (int i = 0; i < max_lines && *p; i++) {
    while (*p == ' ') p++;
    int len = (int)strlen(p);
    if (len > cols) {
      len = cols;
      while (len > 0 && p[len] != ' ') len--;
      if (len == 0) len = cols;   /* a word longer than the line */
    }
    int n = len < (int)sizeof ln - 4 ? len : (int)sizeof ln - 4;
    memcpy(ln, p, (size_t)n);
    ln[n] = 0;
    p += len;
    while (*p == ' ') p++;
    if (i == max_lines - 1 && *p) snprintf(ln + (n > 3 ? n - 3 : 0), 4, "...");
    line(x, y + (float)i * 13.0f, ln, TS_WINDOW, 0);
  }
}

static const uint8_t BODY_RGB[GH_MACHINES][3] = {{0x58, 0x90, 0xf0}, {0xf0, 0xc0, 0x30}, {0xb0, 0x58, 0xe8}};

static void draw_top(void) {
  if (ui_space(UI_PHYS_TOP)) rr_rect(0, 0, 400, 240, RR_RGBA(0, 0, 0, 255));
  if (!ui_space(UI_FIELD)) return;
  rr_rect(0, 0, 320, 180, RR_RGBA(0x20, 0x24, 0x40, 255));
  rr_rect(0, 164, 320, 16, RR_RGBA(0x38, 0x3c, 0x60, 255));
  const uint8_t* c = BODY_RGB[G.machine];
  const rr_color body = RR_RGBA(c[0], c[1], c[2], 255);
  rr_rect(100, 28, 120, 130, body);
  rr_rect(108, 36, 104, 70, RR_RGBA(0x18, 0x1c, 0x30, 255));            /* the glass */
  rr_rect(116, 100, 88, 4, RR_RGBA(c[0] / 2, c[1] / 2, c[2] / 2, 255));
  rr_rect(146, 116, 28, 28, RR_RGBA(0xe0, 0xe0, 0xe8, 255));            /* the knob */
  rr_rect(150, 128, 20, 4, RR_RGBA(0x40, 0x40, 0x50, 255));
  centred(160, 6, gh_machine_name(G.machine), TS_WINDOW, 0);
  line(14, 80, "<", TS_SUMMARY, 0);
  line(298, 80, ">", TS_SUMMARY, 0);
  centred(160, 164, gh_machine_note(G.machine), TS_INSTRUCTIONS_TEXT, RR_RGBA(0xc8, 0xc8, 0xc8, 255));
}

static void draw_bottom(void) {
  if (!ui_space(UI_PHYS_BOTTOM)) return;
  rr_rect(0, 0, 320, 240, RR_RGBA(0x10, 0x10, 0x18, 255));
  char t[160];
  ui_window(1, 1, 318, 58, WIN_NORMAL);
  line(10, 5, "Vouchers", TS_WINDOW, 0);
  snprintf(t, sizeof t, "Regular %u   Plus %u", (unsigned)G.view.vouchers[0], (unsigned)G.view.vouchers[1]);
  line(10, 19, t, TS_INSTRUCTIONS_TEXT, 0);
  snprintf(t, sizeof t, "Premium %u   Golden %u", (unsigned)G.view.vouchers[2], (unsigned)G.view.vouchers[3]);
  line(10, 31, t, TS_INSTRUCTIONS_TEXT, 0);
  snprintf(t, sizeof t, "Eggs %d / %d", G.view.n_eggs, G.view.capacity);
  line(220, 5, t, TS_WINDOW, 0);

  ui_window(1, LIST_Y - 6, 318, GH_OPTIONS * ROW_H + 12, WIN_NORMAL);
  for (int i = 0; i < GH_OPTIONS; i++) {
    const gh_option* o = &G.view.opt[i];
    const float y = (float)(LIST_Y + i * ROW_H);
    if (i == G.sel) rr_rect(4, y - 2, 312, ROW_H - 2, RR_RGBA(255, 255, 255, 30));
    const rr_color col = o->can_buy ? 0 : RR_RGBA(0x80, 0x80, 0x80, 255);
    snprintf(t, sizeof t, "%d egg%s", o->rule.pulls, o->rule.pulls == 1 ? "" : "s");
    line(14, y, t, TS_WINDOW, col);
    snprintf(t, sizeof t, "%d %s voucher%s", o->rule.cost, gh_voucher_name(o->rule.voucher), o->rule.cost == 1 ? "" : "s");
    line(14, y + 12, t, TS_INSTRUCTIONS_TEXT, col ? col : RR_RGBA(0xc8, 0xc8, 0xc8, 255));
    snprintf(t, sizeof t, "have %u", (unsigned)o->have);
    line(250, y + 4, t, TS_INSTRUCTIONS_TEXT, col ? col : RR_RGBA(0xc8, 0xc8, 0xc8, 255));
  }

  if (G.mode == GS_MENU && !G.view.locked) {
    const gh_option* o = &G.view.opt[G.sel];
    if (o->refusal_key) line(10, 214, gh_refusal_text(o->refusal_key), TS_INSTRUCTIONS_TEXT, RR_RGBA(0xf0, 0x90, 0x90, 255));
    line(10, 226, "L/R: machine  U/D: option  A: pull  B: back", TS_INSTRUCTIONS_TEXT, 0);
    return;
  }
  /* a question or a message over the list (a locked machine says why, over the list too) */
  ui_window(16, 78, 288, 104, WIN_NORMAL);
  if (G.mode == GS_CONFIRM) {
    const gh_option* o = &G.view.opt[G.sel];
    snprintf(t, sizeof t, "Pull %d egg%s from the %s", o->rule.pulls, o->rule.pulls == 1 ? "" : "s", gh_machine_name(G.machine));
    line(28, 88, t, TS_WINDOW, 0);
    snprintf(t, sizeof t, "for %d %s voucher%s?", o->rule.cost, gh_voucher_name(o->rule.voucher), o->rule.cost == 1 ? "" : "s");
    line(28, 102, t, TS_WINDOW, 0);
    line(40, 134, "Yes", TS_WINDOW, 0);
    line(40, 152, "No", TS_WINDOW, 0);
    ui_cursor(30, G.yes ? 140 : 158);
    line(10, 226, "A: choose   B: back", TS_INSTRUCTIONS_TEXT, 0);
  } else if (G.mode == GS_MENU) {
    wrapped(28, 88, G.view.lock_text, 42, 6);
    line(10, 226, "L/R: machine   B: back", TS_INSTRUCTIONS_TEXT, 0);
  } else {
    wrapped(28, 88, G.msg, 42, 6);
    line(10, 226, G.mode == GS_RETRY ? "A: try again   B: leave" : "A: back", TS_INSTRUCTIONS_TEXT, 0);
  }
}

static void gs_draw(ui_screen* s, rr_screen which) {
  (void)s;
  if (which == RR_TOP)
    draw_top();
  else
    draw_bottom();
}

static ui_screen g_screen = {"capsule-machine", UI_FADE, gs_enter, gs_leave, NULL, gs_input, NULL, gs_draw, NULL};

bool gacha_screen_open(const gachas_args* a) {
  if (!a) return false;
  K.a = *a;
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
