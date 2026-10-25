/* The biome's background widened over the top screen's bands (see field_border.h). */
#include "field_border.h"

#include "rr.h"
#include "screen_clip.h"
#include "ui_widgets.h"

void field_border_draw(const char* key) {
  const rr_frame* f = img_frame(key, NULL);
  if (!f) {
    rr_rect(0, 0, 40, 240, RR_RGBA(0, 0, 0, 255));
    rr_rect(360, 0, 40, 240, RR_RGBA(0, 0, 0, 255));
    rr_rect(0, 0, 400, 30, RR_RGBA(0, 0, 0, 255));
    rr_rect(0, 210, 400, 30, RR_RGBA(0, 0, 0, 255));
    return;
  }
  const float fw = f->src_w, fh = f->src_h;
  /* the bands: left, right, top, bottom (the corners go with the top and bottom ones) */
  static const float BAND[4][4] = {{0, 30, 40, 180}, {360, 30, 40, 180}, {0, 0, 400, 30}, {0, 210, 400, 30}};
  for (int b = 0; b < 4; b++) {
    clip_rect(BAND[b][0], BAND[b][1], BAND[b][2], BAND[b][3]);
    for (int tx = -1; tx <= 1; tx++)
      for (int ty = -1; ty <= 1; ty++) {
        if (!tx && !ty) continue;
        rr_draw_opts o = RR_DEFAULT_OPTS;
        o.scale_x = tx ? -1 : 1;
        o.scale_y = ty ? -1 : 1;
        /* a mirrored copy is drawn with its edge on the field's edge: x 40 for the left one (it runs leftwards), 40 + 2 * 320 for the
         * right one; the same with 30 and 30 + 2 * 180 vertically */
        const float x = tx < 0 ? 40 : tx > 0 ? 40 + 2 * fw : 40, y = ty < 0 ? 30 : ty > 0 ? 30 + 2 * fh : 30;
        rr_draw(f, x, y, &o);
      }
    clip_off();
  }
}
