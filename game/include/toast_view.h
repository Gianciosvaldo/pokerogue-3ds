/*
 * toast_view: upstream's AchvBar (src/ui/containers/achv-bar.ts) drawn on the top screen: the bar that slides in from the right edge of
 * the field for an achievement or a voucher, with its icon, title, score and description.
 *
 * The art is upstream's achv_bar (92 x 40, a 9-slice with 41 / 6 / 16 / 4 px borders; tier 0..4 is achv_bar, achv_bar_2 .. achv_bar_5), at
 * its own size: upstream draws the bar at half size on a canvas six times larger than the 3DS screen, which would be a 6 px font here.
 * The text is the pixel font the other menus use for small print (menus/mui.h: TSX_PIXEL_8). Width and height are upstream's rules (at
 * least 200 x 40; wider for a long title, taller for a long description).
 */
#ifndef GM_TOAST_VIEW_H
#define GM_TOAST_VIEW_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  const char* title;
  const char* desc;
  const char* score;  /* "+250pt"; "" or NULL = none (a voucher) */
  const char* icon;   /* frame of the "items" atlas */
  int tier;           /* 0..4 */
} tv_info;

/* Draws the bar at the top right of the field (320 x 180 canvas; call it with the top screen's field space set). `slide` is how far in it
 * is: 0 = past the right edge, 1 = in place. Nothing is drawn at 0. */
void tv_draw(const tv_info* t, float slide);

#ifdef __cplusplus
}
#endif
#endif
