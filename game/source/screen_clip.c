/* The GPU scissor for the game framework's screens (see screen_clip.h). It is its own file because it uses menus' layer. */
#include "screen_clip.h"

#include "menus.h"
#include "ui.h"

void clip_to_screen(void) {
  const rr_screen s = ui_drawing();
  mui_set_origin(s, 0, 0);
  mui_clip(0, 0, s == RR_TOP ? 400 : 320, 240);
}

void clip_off(void) { mui_clip_off(); }
