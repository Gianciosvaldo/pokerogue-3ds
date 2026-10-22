/*
 * ui: the game's screen stack, input, timing and drawing spaces.
 *
 * A port of upstream's UI manager (src/ui/ui.ts) and UiHandler (src/ui/handlers/ui-handler.ts)
 * shaped for C: each game screen is a ui_screen with callbacks instead of a class. Upstream's
 * modes map onto the stack:
 *   ui.setMode(m)          -> ui_set(s)      replace the top screen (the old one gets leave())
 *   ui.setOverlayMode(m)   -> ui_push(s)     put a screen on top, keeping the one below
 *   ui.revertMode()        -> ui_pop()       drop the top screen, back to the one below
 * Transitions fade like upstream's (250 ms out, 100 ms black, 250 ms in) when the screen asks
 * for one (UI_FADE), as upstream's transitionModes do.
 *
 * Every screen draws on both 3DS screens. Coordinates are upstream's own: draw inside a
 * "space" (ui_space) and pass the numbers upstream uses. See ui_space below.
 *
 * Time is in milliseconds of game time, advanced once per frame (60 fps = 16.67 ms).
 */
#ifndef GM_UI_H
#define GM_UI_H

#include <stdbool.h>
#include <stdint.h>

#include "rr.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- buttons: upstream's Button enum (src/enums/buttons.ts) ---- */
typedef enum {
  BTN_UP,
  BTN_DOWN,
  BTN_LEFT,
  BTN_RIGHT,
  BTN_SUBMIT,
  BTN_ACTION,
  BTN_CANCEL,
  BTN_MENU,
  BTN_STATS,
  BTN_CYCLE_SHINY,
  BTN_CYCLE_FORM,
  BTN_CYCLE_GENDER,
  BTN_CYCLE_ABILITY,
  BTN_CYCLE_NATURE,
  BTN_CYCLE_TERA,
  BTN_SPEED_UP,
  BTN_SLOW_DOWN,
  BTN_COUNT_
} ui_button;

/* Default 3DS mapping (upstream's gamepad defaults, by position on a Nintendo pad):
 *   D-pad / Circle Pad -> UP DOWN LEFT RIGHT     A -> ACTION        B -> CANCEL
 *   START -> MENU      SELECT -> STATS           Y -> CYCLE_NATURE   X -> CYCLE_TERA
 *   L -> CYCLE_FORM    R -> CYCLE_SHINY          ZL -> CYCLE_GENDER  ZR -> CYCLE_ABILITY (New 3DS only)
 * Held direction and action buttons repeat every 250 ms, as upstream's inputs-controller does;
 * MENU and STATS never repeat. */
const char* ui_button_name(ui_button b);

/* ---- screens ---- */
typedef struct ui_screen ui_screen;

enum {
  UI_FADE = 1,        /* fade through black when this screen is set or popped (upstream transitionModes) */
  UI_OVERLAY = 2,     /* draw the screens below it too (option menus, confirm boxes, the fight menu...) */
};

struct ui_screen {
  const char* name;
  unsigned flags;
  /* All optional. enter: the screen became the top one (upstream show()). leave: it was removed
   * (upstream clear()). resume: a screen pushed over it was popped. input: a button press or
   * repeat for the top screen; return true when it did something (upstream processInput).
   * update: once per frame for every screen on the stack, bottom first, dt in ms. draw: once per
   * frame per physical screen, bottom first; the top screen draws last. */
  void (*enter)(ui_screen* s);
  void (*leave)(ui_screen* s);
  void (*resume)(ui_screen* s);
  bool (*input)(ui_screen* s, ui_button b);
  void (*update)(ui_screen* s, float dt);
  void (*draw)(ui_screen* s, rr_screen which);
  void* data; /* the screen's own state */
  /* Optional: a tap on the bottom screen (a touch that began now), for the top screen. (x, y) are physical pixels of the bottom screen,
   * 0..320 by 0..240; the screen turns them into its own space (ui_bottom_ui_y: UI_BOTTOM_UI's y). Return true when it did something. */
  bool (*touch)(ui_screen* s, float x, float y);
  void (*release)(ui_screen* s, ui_button b); /* button release, also through overlays */
};

void ui_set(ui_screen* s);   /* replace the top screen */
void ui_push(ui_screen* s);  /* overlay */
void ui_pop(void);           /* back to the screen below (no-op when it's the last one) */
void ui_pop_to(ui_screen* s);/* pop until s is on top */
ui_screen* ui_top(void);
int ui_depth(void);
bool ui_busy(void);          /* a fade is running: input is blocked (upstream overlayActive) */

/* Fade the whole display to black and back, as upstream's ui.fadeOut / fadeIn, with a callback
 * at full black (NULL for none). ms = each half. */
void ui_fade(float out_ms, float hold_ms, float in_ms, void (*at_black)(void* arg), void* arg);

/* ---- time ---- */
float ui_now(void);     /* ms since boot, game time */
/* How much faster than real time the screens' clock runs (dt, ui_now, timers, fades): upstream's gameSpeed (run_settings.h). Input
 * repeat and the music keep real time. */
void ui_set_game_speed(float scale);
float ui_game_speed(void);
uint32_t ui_frame(void);/* frames since boot */

/* One-shot timers (upstream globalScene.time.delayedCall). They fire during ui_tick, before the
 * screens update. Returns an id for ui_timer_cancel (0 when the table is full). */
int ui_after(float ms, void (*fn)(void* arg), void* arg);
void ui_timer_cancel(int id);

/* ---- sounds (no-ops without snd.pak / the DSP firmware) ---- */
void ui_play_select(void); /* upstream ui.playSelect: "ui/select" */
void ui_play_error(void);  /* "ui/error" */
void ui_play_sound(const char* key); /* upstream playSound keys, e.g. "se/hit", "ui/menu_open"; NULL = none */

/* ---- drawing spaces ----
 * Upstream draws on one 320x180 canvas. The 3DS has a 400x240 top and a 320x240 bottom screen.
 * A space fixes where upstream's coordinates land; set it in draw() before drawing.
 *   UI_FIELD        top screen, the battle field container: (0,0)..(320,180), centred.
 *   UI_FIELD_UI     top screen, the fieldUI container (info boxes...): y is relative to the
 *                   canvas bottom, so y runs -180..0 (upstream positions like (140,-141)).
 *   UI_BOTTOM_UI    bottom screen, the ui container: y relative to the canvas bottom
 *                   (-180..0), bottom-aligned, so upstream's message strip (y -48..0) is the
 *                   last 48 rows of the bottom screen. Rows above y -240 are off screen.
 *   UI_BOTTOM_CANVAS bottom screen, a whole 320x180 canvas (0,0)..(320,180), centred
 *                   vertically: for full-screen upstream screens (party, summary...).
 *   UI_BOTTOM_CANVAS_UI the same canvas in ui-container coordinates (y -180..0).
 *   UI_TOP_CANVAS / UI_TOP_CANVAS_UI  the same on the top screen (identical to FIELD/FIELD_UI).
 *   UI_PHYS_TOP / UI_PHYS_BOTTOM  raw physical pixels (400x240 / 320x240).
 * The draw callback is told which physical screen it's drawing; drawing to the other one's
 * space is a no-op. */
typedef enum {
  UI_FIELD,
  UI_FIELD_UI,
  UI_TOP_CANVAS,
  UI_TOP_CANVAS_UI,
  UI_BOTTOM_UI,
  UI_BOTTOM_CANVAS,
  UI_BOTTOM_CANVAS_UI,
  UI_PHYS_TOP,
  UI_PHYS_BOTTOM,
} ui_space_id;
/* Sets the space for the following draws. Returns false when the space belongs to the other
 * physical screen than the one being drawn (skip the drawing then). */
bool ui_space(ui_space_id sp);
rr_screen ui_drawing(void); /* the physical screen being drawn */

/* ---- main loop (main.c) ---- */
void ui_init(void);
/* Reads the buttons, runs timers, updates and draws every screen. Returns false to quit. */
bool ui_tick(void);
void ui_request_quit(void);
/* True while an input callback is being given the auto-repeat of a held button (not the first press). For screens where a held
 * button must not do an action twice (a pull, a skip). Only meaningful inside the input callback. */
bool ui_input_repeating(void);
/* Feed a button as if pressed (demo scripts, tests). */
void ui_inject(ui_button b);
/* Feed a tap as if the touch screen was pressed at physical (x, y) (tests); and the debug file's "touch=<frame>:<x>,<y>" lines: a tap
 * delivered at that frame, up to 64 (for emulator runs, which have no touch input). */
void ui_inject_touch(float x, float y);
void ui_debug_touch(uint32_t frame, float x, float y);
/* UI_BOTTOM_UI's y (the command strip's, -48..0) of a physical bottom-screen y */
static inline float ui_bottom_ui_y(float py) { return py - 240.0f; }

#ifdef __cplusplus
}
#endif
#endif
