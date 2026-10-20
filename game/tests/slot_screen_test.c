/* slot_screen_test: the save slot list (source/slot_screen.c) on the host, over a fake UI stack and drawing layer (no pixels: the fakes record
 * the option menus pushed and the texts drawn).
 *
 *   make slot_screen_test && build/slot_screen_test
 *
 * Checks, against slot_screen.h and upstream's save-slot-select-ui-handler.ts: in the load list a run's menu is Load Game / Rename Run / Delete
 * Run / Cancel (Rename Run only when the caller takes it), a slot that can't be loaded offers Delete Run and Cancel, an empty slot gives the error
 * sound; each answer reaches the callback it names; in the new-run list an empty slot is picked at once and a run asks to be overwritten; a run
 * with a name draws it as the window's first line and drops the biome line, a run with no name draws the lines it always did. */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gm_enum_names.h"
#include "i18n.h"
#include "party_icon.h"
#include "run_summary.h"
#include "run_text.h"
#include "screen_clip.h"
#include "slot_screen.h"
#include "ui.h"
#include "ui_text.h"
#include "ui_widgets.h"

static int checks, failed;
#define CHECK(cond, ...)                                  \
  do {                                                    \
    checks++;                                             \
    if (!(cond)) {                                        \
      failed++;                                           \
      if (failed < 40) {                                  \
        printf("FAIL line %d: %s: ", __LINE__, #cond);    \
        printf(__VA_ARGS__);                              \
        printf("\n");                                     \
      }                                                   \
    }                                                     \
  } while (0)

/* ------------------------------------------------------------------ fakes */

static ui_screen* g_screen;
static int n_set, n_select, n_error;
void ui_set(ui_screen* s) {
  g_screen = s;
  n_set++;
}
void ui_play_select(void) { n_select++; }
void ui_play_error(void) { n_error++; }

static int g_which; /* 0 top, 1 bottom */
bool ui_space(ui_space_id sp) {
  const bool top = sp == UI_FIELD || sp == UI_FIELD_UI || sp == UI_TOP_CANVAS || sp == UI_TOP_CANVAS_UI || sp == UI_PHYS_TOP;
  return top == (g_which == 0);
}
void ui_window(float x, float y, float w, float h, win_variant v) { (void)x, (void)y, (void)w, (void)h, (void)v; }
void rr_rect(float x, float y, float w, float h, rr_color c) { (void)x, (void)y, (void)w, (void)h, (void)c; }
void clip_to_screen(void) {}
void clip_off(void) {}
bool img_has(const char* key) {
  (void)key;
  return false;
}
void img_draw(const char* key, const char* frame, float x, float y, float ox, float oy) { (void)key, (void)frame, (void)x, (void)y, (void)ox, (void)oy; }
const char* gm_enum_lower(gm_enum e, int value, char* out, int n) {
  (void)e, (void)value;
  snprintf(out, (size_t)n, "town");
  return out;
}
static int n_icons, n_summaries;
void pi_draw(const rsv_mon* m, float x, float y, float scale) { (void)m, (void)x, (void)y, (void)scale, n_icons++; }
void rsum_draw(const rsv_info* in, float x, float y) { (void)in, (void)x, (void)y, n_summaries++; }
const char* rt_mode_name(int mode, char* out, int n) {
  (void)mode;
  snprintf(out, (size_t)n, "Classic");
  return out;
}
const char* rt_biome_name(int biome, char* out, int n) {
  (void)biome;
  snprintf(out, (size_t)n, "Town");
  return out;
}
const char* rt_number(long v, char* out, int n) {
  snprintf(out, (size_t)n, "%ld", v);
  return out;
}
const char* rt_money(double v, char* out, int n) {
  snprintf(out, (size_t)n, "P%.0f", v);
  return out;
}

typedef struct {
  float x, y;
  char text[200];
} drawn_t;
static drawn_t g_drawn[128];
static int n_drawn;
text_opts text_defaults(void) {
  text_opts o;
  memset(&o, 0, sizeof o);
  o.chars = TEXT_ALL;
  return o;
}
float text_draw(text_style st, float x, float y, const char* s, const text_opts* o) {
  (void)st, (void)o;
  if (n_drawn < 128) {
    drawn_t* d = &g_drawn[n_drawn++];
    d->x = x;
    d->y = y;
    snprintf(d->text, sizeof d->text, "%s", s);
  }
  return 6.0f * (float)strlen(s);
}
int text_lines(text_style st, const char* s, const text_opts* o) {
  (void)st, (void)s, (void)o;
  return 1;
}
void text_wrap(text_style st, const char* s, const text_opts* o, char* out, int n) {
  (void)st, (void)o;
  snprintf(out, (size_t)n, "%s", s);
}

const char* i18n_t(char* out, int n, const char* key, ...) {
  const char* t = key;
  if (!strcmp(key, "menu:loadGame")) t = "Load Game";
  if (!strcmp(key, "saveSlotSelectUiHandler:renameRun")) t = "Rename Run";
  if (!strcmp(key, "saveSlotSelectUiHandler:deleteRun")) t = "Delete Run";
  if (!strcmp(key, "menu:cancel")) t = "Cancel";
  if (!strcmp(key, "saveSlotSelectUiHandler:empty")) t = "Empty";
  if (!strcmp(key, "saveSlotSelectUiHandler:wave")) t = "Wave";
  if (!strcmp(key, "saveSlotSelectUiHandler:lv")) t = "Lv";
  if (!strcmp(key, "menu:score")) t = "Score";
  if (!strcmp(key, "saveSlotSelectUiHandler:deleteData")) t = "Delete the data of this run?";
  if (!strcmp(key, "saveSlotSelectUiHandler:overwriteData")) t = "Overwrite the data in the selected slot?";
  snprintf(out, (size_t)n, "%s", t);
  return out;
}

/* a message box that has typed everything at once */
void msgbox_init(msgbox* m, text_style style, float wrap_w, int max_lines) {
  (void)style, (void)max_lines;
  memset(m, 0, sizeof *m);
  m->opts.wrap_w = wrap_w;
}
static char last_msgbox[512];
void msgbox_show(msgbox* m, const char* text, float delay, bool prompt) {
  (void)delay;
  m->prompt = prompt;
  snprintf(m->text, sizeof m->text, "%s", text);
  snprintf(last_msgbox, sizeof last_msgbox, "%s", text);
  m->shown = m->total = (int)strlen(text);
  m->done = !prompt;
  m->waiting = prompt;
}
void msgbox_clear(msgbox* m) { memset(m->text, 0, sizeof m->text), m->waiting = false, m->done = true; }
void msgbox_update(msgbox* m, float dt) { (void)m, (void)dt; }
bool msgbox_input(msgbox* m, ui_button b) {
  if (!m->waiting || (b != BTN_ACTION && b != BTN_CANCEL)) return false;
  m->waiting = false;
  m->done = true;
  return true;
}
void msgbox_draw(const msgbox* m, float x, float y) { (void)m, (void)x, (void)y; }
bool msgbox_done(const msgbox* m) { return m->done; }
bool msgbox_typing(const msgbox* m) { return !m->done && !m->waiting; }

/* the option menu and the Yes / No box: recorded, answered by the test */
static const char* g_labels[8];
static int g_n_labels, n_options;
static bool g_can_cancel;
static void (*g_on_options)(int idx, void* arg);
static void* g_options_arg;
void ui_push_options(const char* const* labels, int n, bool can_cancel, void (*on_done)(int idx, void* arg), void* arg) {
  n_options++;
  g_n_labels = n;
  for (int i = 0; i < n && i < 8; i++) g_labels[i] = labels[i];
  g_can_cancel = can_cancel;
  g_on_options = on_done;
  g_options_arg = arg;
}
static int n_confirm;
static void (*g_on_confirm)(int yes, void* arg);
static void* g_confirm_arg;
void ui_push_confirm(void (*on_done)(int yes, void* arg), void* arg) {
  n_confirm++;
  g_on_confirm = on_done;
  g_confirm_arg = arg;
}

/* ------------------------------------------------------------------ the owner */

static int n_pick, n_delete, n_rename, n_back;
static int picked, deleted, renamed;
static void on_pick(int slot, void* arg) { (void)arg, n_pick++, picked = slot; }
static void on_delete(int slot, void* arg) { (void)arg, n_delete++, deleted = slot; }
static void on_rename(int slot, void* arg) { (void)arg, n_rename++, renamed = slot; }
static void on_back(void* arg) { (void)arg, n_back++; }

static rsv_info info[RSV_SLOTS];

static void make_info(void) {
  memset(info, 0, sizeof info);
  for (int i = 0; i < RSV_SLOTS; i++) info[i].status = RSV_EMPTY;
  info[0].status = RSV_OK;
  info[0].wave = 12;
  info[0].score = 300;
  info[0].money = 1000;
  info[0].n_party = 1;
  info[0].party[0].level = 17;
  info[2].status = RSV_INCOMPATIBLE;
  snprintf(info[2].why, sizeof info[2].why, "The save is damaged.");
  info[3] = info[0];
  info[3].wave = 4;
  snprintf(info[3].name, sizeof info[3].name, "Jojo Run");
}

static void open_list(slots_mode mode, bool with_rename, int cursor) {
  n_pick = n_delete = n_rename = n_back = n_options = n_confirm = 0;
  const slots_args a = {mode, info, cursor, on_pick, on_delete, with_rename ? on_rename : NULL, on_back, NULL};
  slot_screen_open(&a);
}
static bool press(ui_button b) { return g_screen->input(g_screen, b); }
static void draw_bottom(void) {
  n_drawn = 0;
  g_which = 1;
  g_screen->draw(g_screen, RR_BOTTOM);
  g_which = 0;
}
static const drawn_t* find(const char* s) {
  for (int i = 0; i < n_drawn; i++)
    if (!strcmp(g_drawn[i].text, s)) return &g_drawn[i];
  return NULL;
}
static bool labels_are(int n, ...) {
  if (g_n_labels != n) return false;
  va_list ap;
  va_start(ap, n);
  bool same = true;
  for (int i = 0; i < n; i++) {
    const char* want = va_arg(ap, const char*);
    if (strcmp(g_labels[i], want)) same = false;
  }
  va_end(ap);
  return same;
}

/* ------------------------------------------------------------------ the tests */

int main(void) {
  make_info();

  /* ---- the load list: a run's menu */
  open_list(SLOTS_LOAD, true, 0);
  CHECK(g_screen && n_set == 1 && !strcmp(g_screen->name, "save-slots"), "the list is the screen");
  press(BTN_ACTION);
  CHECK(n_options == 1 && g_can_cancel && labels_are(4, "Load Game", "Rename Run", "Delete Run", "Cancel"), "a run's menu has 4 entries (%d: %s, %s, %s, %s)", g_n_labels,
        g_n_labels > 0 ? g_labels[0] : "", g_n_labels > 1 ? g_labels[1] : "", g_n_labels > 2 ? g_labels[2] : "", g_n_labels > 3 ? g_labels[3] : "");
  /* Rename Run reaches the caller, and the list takes no more input: the caller has the screens */
  g_on_options(1, g_options_arg);
  CHECK(n_rename == 1 && renamed == 0 && n_pick == 0 && n_delete == 0, "Rename Run calls on_rename for slot 0");
  const int options_before = n_options;
  press(BTN_ACTION);
  press(BTN_DOWN);
  press(BTN_CANCEL);
  CHECK(n_options == options_before && n_back == 0, "nothing more is taken once Rename Run has the screens");

  /* Load Game */
  open_list(SLOTS_LOAD, true, 0);
  press(BTN_ACTION);
  g_on_options(0, g_options_arg);
  CHECK(n_pick == 1 && picked == 0 && n_rename == 0, "Load Game picks the slot");

  /* Delete Run asks first, and Yes deletes */
  open_list(SLOTS_LOAD, true, 0);
  press(BTN_ACTION);
  g_on_options(2, g_options_arg);
  CHECK(n_confirm == 1 && n_delete == 0 && strstr(last_msgbox, "Delete"), "Delete Run asks \"%s\"", last_msgbox);
  g_on_confirm(1, g_confirm_arg);
  CHECK(n_delete == 1 && deleted == 0, "Yes deletes slot 0");
  open_list(SLOTS_LOAD, true, 0);
  press(BTN_ACTION);
  g_on_options(2, g_options_arg);
  g_on_confirm(0, g_confirm_arg);
  CHECK(n_delete == 0, "No does not delete");

  /* Cancel does nothing */
  open_list(SLOTS_LOAD, true, 0);
  press(BTN_ACTION);
  g_on_options(3, g_options_arg);
  CHECK(n_pick == 0 && n_delete == 0 && n_rename == 0 && n_back == 0, "Cancel does nothing");
  g_on_options(-2, g_options_arg);
  CHECK(n_pick == 0 && n_delete == 0 && n_rename == 0, "backing out of the menu does nothing");

  /* without a caller for Rename Run the entry is not offered */
  open_list(SLOTS_LOAD, false, 0);
  press(BTN_ACTION);
  CHECK(labels_are(3, "Load Game", "Delete Run", "Cancel"), "no Rename Run without on_rename (%d entries)", g_n_labels);
  g_on_options(1, g_options_arg);
  CHECK(n_confirm == 1 && n_rename == 0, "the second entry is then Delete Run");

  /* a slot that can't be loaded: Delete Run only, and a named one is the same */
  open_list(SLOTS_LOAD, true, 2);
  press(BTN_ACTION);
  CHECK(labels_are(2, "Delete Run", "Cancel"), "an incompatible slot offers Delete Run and Cancel (%d entries)", g_n_labels);
  g_on_options(0, g_options_arg);
  CHECK(n_confirm == 1 && n_rename == 0, "its first entry is Delete Run");

  /* an empty slot has no menu */
  open_list(SLOTS_LOAD, true, 1);
  const int err = n_error;
  press(BTN_ACTION);
  CHECK(n_options == 0 && n_error == err + 1, "an empty slot gives the error sound and no menu");

  /* a named run offers the same menu */
  open_list(SLOTS_LOAD, true, 3);
  press(BTN_ACTION);
  CHECK(labels_are(4, "Load Game", "Rename Run", "Delete Run", "Cancel"), "a named run has the same 4 entries");
  g_on_options(1, g_options_arg);
  CHECK(n_rename == 1 && renamed == 3, "Rename Run on slot 3");

  /* B goes back */
  open_list(SLOTS_LOAD, true, 0);
  press(BTN_CANCEL);
  CHECK(n_back == 1, "B goes back");

  /* ---- the new-run list: no menu, no Rename Run */
  open_list(SLOTS_NEW, true, 1);
  press(BTN_ACTION);
  CHECK(n_options == 0 && n_pick == 1 && picked == 1, "an empty slot is picked at once");
  open_list(SLOTS_NEW, true, 0);
  press(BTN_ACTION);
  CHECK(n_options == 0 && n_confirm == 1 && n_pick == 0 && strstr(last_msgbox, "Overwrite"), "a run in the way asks to be overwritten");
  g_on_confirm(1, g_confirm_arg);
  CHECK(n_pick == 1 && picked == 0, "Yes picks the slot");

  /* ---- the windows */
  open_list(SLOTS_LOAD, true, 0);
  draw_bottom();
  const drawn_t *mode0 = find("Classic - Wave 12"), *name3 = find("Jojo Run"), *mode3 = find("Classic - Wave 4");
  CHECK(mode0 && name3 && mode3, "slot 0's mode line, slot 3's name and mode line are drawn");
  if (mode0 && name3 && mode3) {
    /* slot 0 is selected: its window is at x 2 (the others at 8), 76 px a slot from y 8 */
    CHECK(mode0->x == 2 + 8 && mode0->y == 8 + 5, "an unnamed run's first line is mode and wave (%.0f, %.0f)", mode0->x, mode0->y);
    CHECK(name3->x == 8 + 8 && name3->y == 8 + 3 * 76 + 5, "a named run's first line is its name (%.0f, %.0f)", name3->x, name3->y);
    CHECK(mode3->y == name3->y + 14, "and mode and wave follow it, 14 px lower (%.0f after %.0f)", mode3->y, name3->y);
  }
  /* the biome line: slot 0 (no name) has it, slot 3 (named) has not: two windows, one biome line (slot 1 and 2 are empty / unreadable) */
  int biome_lines = 0;
  for (int i = 0; i < n_drawn; i++)
    if (!strcmp(g_drawn[i].text, "Town")) biome_lines++;
  CHECK(biome_lines == 1, "only the unnamed run draws a biome line (%d)", biome_lines);
  /* ---- touch: a tap on another slot moves the cursor there, a second tap on it is A; the gaps and the margins take nothing */
  open_list(SLOTS_LOAD, true, 1);
  CHECK(g_screen->touch != NULL, "the list takes taps");
  const int selects = n_select;
  CHECK(!g_screen->touch(g_screen, 150, 8 + 70 + 3) && !g_screen->touch(g_screen, 150, 4) && !g_screen->touch(g_screen, 316, 40),
        "the gap between two slots, the top margin and the right margin take nothing");
  CHECK(g_screen->touch(g_screen, 150, 40) && n_options == 0 && n_select == selects + 1, "a tap on slot 0 moves the cursor only (%d menus)", n_options);
  CHECK(g_screen->touch(g_screen, 150, 40) && n_options == 1, "a second tap on it is A: its menu");
  g_on_options(0, g_options_arg);
  CHECK(n_pick == 1 && picked == 0, "Load Game picks the slot the cursor was moved to by touch (%d picks, slot %d)", n_pick, picked);
  open_list(SLOTS_LOAD, true, 0);
  CHECK(g_screen->touch(g_screen, 150, 8 + 76 + 5) && g_screen->touch(g_screen, 150, 8 + 76 + 5) && n_options == 0,
        "an empty slot is selected by a tap but its second tap does not open a run's menu (%d menus)", n_options);

  /* score and money keep their lines in both */
  int scores = 0, monies = 0;
  for (int i = 0; i < n_drawn; i++) {
    scores += !strcmp(g_drawn[i].text, "Score 300");
    monies += !strcmp(g_drawn[i].text, "P1000");
  }
  CHECK(scores == 2 && monies == 2, "score and money are drawn for both runs (%d, %d)", scores, monies);

  printf("%d checks, %d failed\n", checks, failed);
  printf(failed ? "FAILED\n" : "OK\n");
  return failed ? 1 : 0;
}
