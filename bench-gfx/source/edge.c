/* The 1 px top-screen offset seen in Azahar screenshots: 1 px lines on every screen edge,
 * then (a) read back from the framebuffer the console actually displays, and (b) a question
 * about what the screen shows, since only a person can see the LCD. */
#include <stdlib.h>
#include <string.h>

#include "bench.h"

static const rr_color RED = RR_RGBA(255, 0, 0, 255), GREEN = RR_RGBA(0, 255, 0, 255),
                      BLUE = RR_RGBA(0, 0, 255, 255), YELLOW = RR_RGBA(255, 255, 0, 255),
                      BACK = RR_RGBA(48, 48, 48, 255);

static void edges(int w) {
  rr_rect(0, 0, w, 1, RED);
  rr_rect(0, 239, w, 1, GREEN);
  rr_rect(0, 1, 1, 238, BLUE);
  rr_rect(w - 1, 1, 1, 238, YELLOW);
}

static void draw_edges(const char* bottom_text) {
  rr_set_origin(RR_TOP, 0, 0);
  rr_set_origin(RR_BOTTOM, 0, 0);
  rr_frame_begin();
  rr_target(RR_TOP, BACK);
  edges(400);
  rr_target(RR_BOTTOM, BACK);
  edges(320);
  if (bottom_text) rr_text_sys(10, 8, 0.45f, bottom_text, RR_RGBA(240, 240, 240, 255));
  rr_frame_end();
}

/* 0 other, 1 red, 2 green, 3 blue, 4 yellow */
static int classify(const u8* p, int bpp) {
  int r, g, b;
  if (bpp == 3) { /* BGR8: B, G, R in memory */
    b = p[0], g = p[1], r = p[2];
  } else if (bpp == 4) { /* RGBA8: A, B, G, R in memory */
    b = p[1], g = p[2], r = p[3];
  } else if (bpp == 2) { /* RGB565 */
    u16 v = p[0] | p[1] << 8;
    r = (v >> 11) << 3, g = ((v >> 5) & 63) << 2, b = (v & 31) << 3;
  } else {
    return 0;
  }
#define NEAR(v, t) ((v) > (t) - 40 && (v) < (t) + 40)
  if (NEAR(r, 255) && NEAR(g, 0) && NEAR(b, 0)) return 1;
  if (NEAR(r, 0) && NEAR(g, 255) && NEAR(b, 0)) return 2;
  if (NEAR(r, 0) && NEAR(g, 0) && NEAR(b, 255)) return 3;
  if (NEAR(r, 255) && NEAR(g, 255) && NEAR(b, 0)) return 4;
#undef NEAR
  return 0;
}

static void list_hits(char* out, size_t n, const int* at, int k) {
  size_t len = 0;
  out[0] = 0;
  if (!k) snprintf(out, n, "nowhere");
  for (int i = 0; i < k && len < n; i++) len += snprintf(out + len, n - len, "%s%d", i ? "," : "", at[i]);
}

/* The framebuffer is the screen rotated: 240 pixels per column, columns left to right, and
 * each column stored bottom to top. Screen pixel (x, y) is at (x * 240 + 239 - y). */
static bool scan(gfxScreen_t scr, int W, const char* name) {
  u16 fw, fh;
  u8* fb = gfxGetFramebuffer(scr, GFX_LEFT, &fw, &fh);
  int bpp = (int)gspGetBytesPerPixel(gfxGetScreenFormat(scr));
  if (!fb || fw != 240 || fh != W) {
    res("%s screen: framebuffer is %dx%d, expected 240x%d; not checked", name, fw, fh, W);
    return false;
  }
  GSPGPU_InvalidateDataCache(fb, (u32)fw * fh * bpp);
  int rows[2][8], nr[2] = {0}, cols[2][8], nc[2] = {0};
  for (int y = 0; y < 240; y++) { /* down the middle column */
    int c = classify(fb + ((W / 2) * 240 + 239 - y) * bpp, bpp);
    if ((c == 1 || c == 2) && nr[c - 1] < 8) rows[c - 1][nr[c - 1]++] = y;
  }
  for (int x = 0; x < W; x++) { /* along the middle row */
    int c = classify(fb + (x * 240 + 239 - 120) * bpp, bpp);
    if ((c == 3 || c == 4) && nc[c - 3] < 8) cols[c - 3][nc[c - 3]++] = x;
  }
  char r0[48], r1[48], c0[48], c1[48];
  list_hits(r0, sizeof r0, rows[0], nr[0]);
  list_hits(r1, sizeof r1, rows[1], nr[1]);
  list_hits(c0, sizeof c0, cols[0], nc[0]);
  list_hits(c1, sizeof c1, cols[1], nc[1]);
  bool exact = nr[0] == 1 && rows[0][0] == 0 && nr[1] == 1 && rows[1][0] == 239 && nc[0] == 1 && cols[0][0] == 0 &&
               nc[1] == 1 && cols[1][0] == W - 1;
  res("%s screen (%dx240): red line drawn on row 0 is on rows %s; green on row 239 is on rows %s;", name, W, r0,
      r1);
  res("  blue on column 0 is on columns %s; yellow on column %d is on columns %s => %s", c0, W - 1, c1,
      exact ? "exact, no offset in the framebuffer" : "NOT where it was drawn");
  return exact;
}

void edge_readback(void) {
  res("\n== screen edges (the 1 px offset seen in Azahar) ==");
  for (int i = 0; i < 6; i++) draw_edges(NULL); /* both framebuffers of each screen get the pattern */
  scan(GFX_TOP, 400, "top");
  scan(GFX_BOTTOM, 320, "bottom");
  res_save();
}

void edge_ask(void) {
  static const char* Q =
      "Look at the edges of the TOP screen.\n"
      "Each edge should show a thin 1-pixel line:\n"
      "red along the top, green along the bottom,\n"
      "blue on the left, yellow on the right.\n\n"
      "A      I can see all four lines\n"
      "UP     the red (top) line is missing\n"
      "DOWN   the green (bottom) line is missing\n"
      "LEFT   the blue (left) line is missing\n"
      "RIGHT  the yellow (right) line is missing\n"
      "B      not sure / skip";
  const char* answer = NULL;
  if (g_headless) { /* emulator: show the pattern, tell the runner, keep it up for its screenshot */
    for (int i = 0; i < 60; i++) draw_edges(NULL);
    res("what the top screen shows: not asked (headless emulator run)");
    res_save();
    FILE* f = fopen(DONE_PATH, "w");
    if (f) fclose(f);
    for (int i = 0; i < 240 && aptMainLoop(); i++) draw_edges(NULL);
    return;
  }
  while (!answer) {
    if (!aptMainLoop()) {
      answer = "no answer (app closed)";
      break;
    }
    hidScanInput();
    u32 k = hidKeysDown();
    if (k & KEY_A) answer = "all four edge lines visible";
    else if (k & KEY_UP) answer = "the red TOP line is missing";
    else if (k & KEY_DOWN) answer = "the green BOTTOM line is missing";
    else if (k & KEY_LEFT) answer = "the blue LEFT line is missing";
    else if (k & KEY_RIGHT) answer = "the yellow RIGHT line is missing";
    else if (k & (KEY_B | KEY_START)) answer = "not sure / skipped";
    draw_edges(Q);
  }
  res("what the top screen shows (your answer): %s", answer);
  res_save();
}
