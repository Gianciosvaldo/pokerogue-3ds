/* The menu START opens during a run (see run_menu.h). */
#include "run_menu.h"

#include <stdio.h>
#include <string.h>

#include "i18n.h"
#include "ui.h"
#include "ui_text.h"
#include "ui_widgets.h"

#define OPT_ROW 16.0f
#define WINDOW_PADDING 23.0f   /* base-option-select-ui-handler.ts */
#define CANVAS_W 320.0f
#define CANVAS_H 180.0f
#define MSG_H 48.0f            /* menuMessageBox: addWindow(0, 0, width - w - 2, 48) */

typedef enum { MS_MAIN, MS_WARNING, MS_CONFIRM } menu_state;

static struct {
  bool open;
  runmenu_args a;
  menu_state state;
  char label[64];            /* menuUiHandler:saveAndQuit */
  char yes[32], no[32];
  const char* yn_labels[2];
  optlist yn;                /* the Yes / No box */
  msgbox msg;
  float w;                   /* the option window's width */
  int cursor;
} M;

static ui_screen g_screen;

static void close_menu(bool quit) {
  void (*fn)(void*) = quit ? M.a.on_quit : M.a.on_close;
  void* arg = M.a.arg;
  M.open = false;
  ui_pop();
  if (fn) fn(arg);
}

/* Save and Quit: menu-ui-handler.ts MenuOptions.SAVE_AND_QUIT. Past the first turn the player is asked first; before it nothing is
 * lost, so it goes straight to the title. */
static void start_save_quit(void) {
  if (M.a.turn > 1 || M.a.unsaved) {
    char text[256], wrapped[300];
    if (M.a.unsaved)
      snprintf(text, sizeof text, "The last save did not reach the SD card, so the game goes back to the last one that did. Proceed?");
    else
      i18n_t(text, sizeof text, "menuUiHandler:losingProgressionWarning", NULL);
    text_opts o = text_defaults();
    o.wrap_w = 204; /* the message text's word wrap, 1224 / 6 */
    text_wrap(TS_WINDOW, text, &o, wrapped, sizeof wrapped);
    msgbox_init(&M.msg, TS_WINDOW, 204, 2);
    msgbox_show(&M.msg, wrapped, -1, false);
    M.state = MS_WARNING;
    return;
  }
  close_menu(true);
}

static void show_confirm(void) {
  i18n_t(M.yes, sizeof M.yes, "menu:yes", NULL);
  i18n_t(M.no, sizeof M.no, "menu:no", NULL);
  M.yn_labels[0] = M.yes;
  M.yn_labels[1] = M.no;
  optlist_init(&M.yn, M.yn_labels, 2);
  M.yn.x = 319 - M.w; /* ConfirmModeConfig xOffset: the width of the option window */
  M.yn.y = -48;       /* ConfirmUiHandler yOffset 48 */
  M.state = MS_CONFIRM;
}

static bool rm_input(ui_screen* s, ui_button b) {
  (void)s;
  if (!M.open) return false;
  if (b == BTN_MENU || b == BTN_CANCEL) {
    if (M.state == MS_CONFIRM && b == BTN_CANCEL) {
      /* the Yes / No box's own B is its No: the box goes, the text goes, the menu stays */
      ui_play_select();
      M.state = MS_MAIN;
      msgbox_clear(&M.msg);
      return true;
    }
    ui_play_select();
    close_menu(false);
    return true;
  }
  if (M.state == MS_WARNING) return false;
  if (M.state == MS_CONFIRM) {
    if (!optlist_input(&M.yn, b)) return false;
    if (M.yn.chosen != -1) {
      const int pick = M.yn.chosen;
      if (pick == 0) {
        close_menu(true);
      } else {
        M.state = MS_MAIN;
        msgbox_clear(&M.msg);
      }
    }
    return true;
  }
  if (b == BTN_ACTION) {
    ui_play_select();
    start_save_quit();
    return true;
  }
  return false; /* one entry: the cursor has nowhere to go */
}

static void rm_update(ui_screen* s, float dt) {
  (void)s;
  if (!M.open) return;
  if (M.state == MS_WARNING) {
    msgbox_update(&M.msg, dt);
    if (msgbox_done(&M.msg)) show_confirm();
  }
}

static void rm_draw(ui_screen* s, rr_screen which) {
  (void)s, (void)which;
  if (!M.open || !ui_space(UI_BOTTOM_UI)) return;
  /* optionSelectContainer at (width - 1, -1), origin (1, 1): the window is the canvas height less 2, so it fills the screen */
  const float h = CANVAS_H - 2, left = 319 - M.w, top = -1 - h;
  ui_window(left, top, M.w, h, WIN_NORMAL);
  text_opts o = text_defaults();
  o.line_spacing = 12;
  text_draw(TS_WINDOW, left + 15, top + 7, M.label, &o);
  if (M.state == MS_MAIN) ui_cursor(left + 10, top + 15 + (float)M.cursor * OPT_ROW);
  if (M.state == MS_MAIN) return;
  /* the message box: addWindow(0, 0, width - w - 2, 48) at the bottom left, its text at (8, 8) */
  ui_window(2, -MSG_H, CANVAS_W - M.w - 2, MSG_H, WIN_NORMAL);
  msgbox_draw(&M.msg, 2 + 8, -MSG_H + 8);
  if (M.state == MS_CONFIRM) optlist_draw(&M.yn);
}

static ui_screen g_screen = {"run-menu", UI_OVERLAY, NULL, NULL, NULL, rm_input, rm_update, rm_draw, NULL};

bool run_menu_is_open(void) { return M.open; }

bool run_menu_open(const runmenu_args* a) {
  if (M.open) return false;
  memset(&M, 0, sizeof M);
  M.a = *a;
  M.open = true;
  M.state = MS_MAIN;
  i18n_t(M.label, sizeof M.label, "menuUiHandler:saveAndQuit", NULL);
  M.w = text_width(TS_WINDOW, M.label, NULL) + WINDOW_PADDING;
  msgbox_init(&M.msg, TS_WINDOW, 204, 2);
  ui_play_sound("ui/menu_open");
  ui_push(&g_screen);
  return true;
}
