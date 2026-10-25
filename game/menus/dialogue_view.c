/* The dialogue's name box (see dialogue_view.h). */
#include "dialogue_view.h"

#include <stdio.h>

#include "mui.h"

#define BOX_H 16.0f

void dv_namebox_at(rr_screen screen, int origin_x, int origin_y, float x, float y, const char* name, int window_style) {
  if (!name || !*name) return;
  mui_set_origin(screen, origin_x, origin_y);
  const float w = (float)mui_text_width(TS_MESSAGE, name) + 16.0f;
  char frame[8];
  snprintf(frame, sizeof frame, "%d", window_style < 1 || window_style > 5 ? 1 : window_style);
  const rr_frame* f = mui_frame("ui/namebox", frame);
  if (f)
    mui_nineslice(f, 20, 20, x, y, w, BOX_H, 8, 8, 5, 5);
  else
    mui_window(x, y, w, BOX_H);
  mui_text(TS_MESSAGE, x + 8.0f, y, name);
}

void dv_namebox(float x, float y, const char* name, int window_style) {
  dv_namebox_at(RR_BOTTOM, 0, 0, x, y, name, window_style);
}
