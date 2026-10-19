/* The achievement / voucher bar drawn (see toast_view.h). */
#include "toast_view.h"

#include <stdio.h>
#include <string.h>

#include "mui.h"

#define FONT TSX_PIXEL_8
#define FIELD_W 320.0f

/* the art: 92 x 40, borders left 41, right 6, top 16, bottom 4 (NineSlice(0, 0, "achv_bar", undefined, w, h, 41, 6, 16, 4)) */
#define ART_W 92.0f
#define ART_H 40.0f
#define B_LEFT 41.0f
#define B_RIGHT 6.0f
#define B_TOP 16.0f
#define B_BOTTOM 4.0f

#define BAR_MIN_W 200.0f
#define BAR_MIN_H 40.0f

/* draws source rectangle (sx, sy, sw, sh) of the art into (dx, dy, dw, dh): the art scaled and clipped to the destination */
static void slice(const rr_frame* f, float dx, float dy, float dw, float dh, float sx, float sy, float sw, float sh) {
  if (dw <= 0 || dh <= 0) return;
  mui_clip(dx, dy, dw, dh);
  rr_draw_opts o = RR_DEFAULT_OPTS;
  o.scale_x = dw / sw;
  o.scale_y = dh / sh;
  rr_draw(f, dx - sx * o.scale_x, dy - sy * o.scale_y, &o);
  mui_clip_off();
}

static void nine_slice(const rr_frame* f, float x, float y, float w, float h) {
  const float mw = w - B_LEFT - B_RIGHT, mh = h - B_TOP - B_BOTTOM;
  const float cw = ART_W - B_LEFT - B_RIGHT, ch = ART_H - B_TOP - B_BOTTOM;
  const float xs[3] = {x, x + B_LEFT, x + w - B_RIGHT}, ws[3] = {B_LEFT, mw, B_RIGHT};
  const float sxs[3] = {0, B_LEFT, ART_W - B_RIGHT}, sws[3] = {B_LEFT, cw, B_RIGHT};
  const float ys[3] = {y, y + B_TOP, y + h - B_BOTTOM}, hs[3] = {B_TOP, mh, B_BOTTOM};
  const float sys_[3] = {0, B_TOP, ART_H - B_BOTTOM}, shs[3] = {B_TOP, ch, B_BOTTOM};
  for (int j = 0; j < 3; j++)
    for (int i = 0; i < 3; i++) slice(f, xs[i], ys[j], ws[i], hs[j], sxs[i], sys_[j], sws[i], shs[j]);
}

void tv_draw(const tv_info* t, float slide) {
  if (slide <= 0.0f) return;
  if (slide > 1.0f) slide = 1.0f;
  mui_set_origin(RR_TOP, 40, 30); /* the field canvas centred on the 400 x 240 screen */
  const char* score = t->score ? t->score : "";
  /* AchvBar.showAchv: the width is the default or the icon (32), the title, the score and 16 if that is more; the height the default or the
   * title and the description and 8 */
  const float title_w = (float)mui_text_width(FONT, t->title), score_w = (float)mui_text_width(FONT, score);
  float w = 40.0f + title_w + 8.0f + score_w + 6.0f;
  if (w < BAR_MIN_W) w = BAR_MIN_W;
  char wrapped[256];
  mui_wrap(FONT, t->desc, w - 32.0f - 16.0f - 3.0f, wrapped, sizeof wrapped);
  float h = 18.0f + mui_text_height(FONT, wrapped) + 4.0f;
  if (h < BAR_MIN_H) h = BAR_MIN_H;
  const float x = FIELD_W - w * slide;

  char key[24];
  snprintf(key, sizeof key, t->tier > 0 ? "ui/achv_bar_%d" : "ui/achv_bar", t->tier + 1);
  const rr_frame* f = mui_frame(key, NULL);
  if (f)
    nine_slice(f, x, 0, w, h);
  else
    mui_window(x, 0, w, h);
  rr_draw_opts io = RR_DEFAULT_OPTS;
  mui_image_ex("items", t->icon, x + 4.0f, h / 2.0f - 16.0f, &io);
  const rr_color white = mui_style_color(TS_MESSAGE, false), grey = mui_style_color(TS_MESSAGE, true);
  mui_text_c(FONT, x + 40.0f, 4.0f, t->title, white, grey);
  if (score[0]) mui_text_c(FONT, x + w - 2.0f - score_w, 4.0f, score, white, grey);
  mui_text_c(FONT, x + 43.0f, 18.0f, wrapped, mui_style_color(TS_WINDOW_ALT, false), mui_style_color(TS_WINDOW_ALT, true));
}
