/* The party screen's background in the bands of the bottom screen (lorenzo, 2026-11-17: "the 'pokemon' bottom screen does not fit the entire
 * bottom screen as there are black bands above and below"). Upstream's 320x180 canvas is centred on the 320x240 bottom screen, so 30 rows
 * above and below stayed black. party_draw now draws the background there too. The real party_draw is run with a recording stand-in for the
 * drawing calls (party_fake.c's hook):
 *   1  one clip over the rows above the canvas (y -30, 30 rows and the canvas' first row, whose art is empty), one over the 30 rows below
 *      it (y 180), both the screen's width, each followed by the background image drawn into it, and the clip turned off after
 *   2  every copy is moved by whole grid periods (32 px): x 0, 128, 256, the rows by -64 (top) and +96 (bottom), so the grid lines carry on
 *   3  the baked empty-slot bars (x >= 143 in the art) never reach a band: for every x of the band, the copy drawn last over it takes its
 *      pixels from art x < 143
 *   4  the rows a band copies are inside the art and clear of its empty first rows
 *   5  the bands are drawn before the canvas' own background (it goes over their edge), with the same image (singles, doubles, item
 *      management), and only while the screen is open
 * The grid's 32 px period, the bars' x of 143 and the first row being empty are read off the app's own screenshots of this screen and the
 * code (slot_rect), not off an upstream run; the art itself is not in the repo, so its pixels are not checked here. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "menus.h"
#include "party_fake.h"

typedef struct {
  char kind;
  char key[48];
  float x, y, w, h;
} call;
static call g_log[64];
static int g_n;
static void hook(char kind, const char* key, float x, float y, float w, float h) {
  if (g_n >= 64) return;
  g_log[g_n++] = (call){kind, "", x, y, w, h};
  snprintf(g_log[g_n - 1].key, sizeof g_log[g_n - 1].key, "%s", key);
}

static int checks;
#define CHECK(x)                                                                        \
  do {                                                                                  \
    if (!(x)) {                                                                         \
      fprintf(stderr, "party bands FAIL line %d: %s\n", __LINE__, #x);                  \
      exit(1);                                                                          \
    }                                                                                   \
    checks++;                                                                           \
  } while (0)

#define CANVAS_H 180
#define BAND 30
#define BAR_X 143 /* the benched slots' x (menus/party.c slot_rect): the empty-slot bars baked into the art start there */
#define ART_H 180 /* the background's height: its first row is empty, so the art can be 179 rows as well; 180 is the worse case below */

static void open_screen(party_mode mode, bool dbl) {
  static menu_mon party[6];
  memset(party, 0, sizeof party);
  for (int i = 0; i < 3; i++) {
    party[i].species = (uint16_t)(1 + i);
    snprintf(party[i].name, sizeof party[i].name, "Mon%d", i);
    party[i].level = 5;
    party[i].hp = party[i].max_hp = 20;
    party[i].allowed_in_battle = true;
  }
  party_args a;
  memset(&a, 0, sizeof a);
  a.mode = mode;
  a.battler_count = dbl ? 2 : 1;
  a.double_battle = dbl;
  static party_events ev;
  memset(&ev, 0, sizeof ev);
  party_open(&a, party, 3, &ev);
}

/* what party_draw drew */
static void draw(void) {
  g_n = 0;
  party_draw();
}

/* the images between the clip at log index `c` and the next clip or clip-off, as indices; returns how many */
static int images_in(int c, int out[8]) {
  int k = 0;
  for (int i = c + 1; i < g_n && g_log[i].kind == 'i' && k < 8; i++) out[k++] = i;
  return k;
}

/* checks 1 to 5 for the screen as it is open, with its background image `bg` */
static void check_screen(const char* bg) {
  draw();
  /* ---- 1: the two clips, each with the background in it, then the clip off */
  CHECK(g_n > 8 && g_log[0].kind == 'c');
  CHECK(g_log[0].x == 0 && g_log[0].y == -BAND && g_log[0].w == 320 && g_log[0].h == BAND + 1);
  int top[8], bot[8];
  const int nt = images_in(0, top);
  CHECK(nt == 3);
  const int c2 = top[nt - 1] + 1;
  CHECK(g_log[c2].kind == 'c');
  CHECK(g_log[c2].x == 0 && g_log[c2].y == CANVAS_H && g_log[c2].w == 320 && g_log[c2].h == BAND);
  const int nb = images_in(c2, bot);
  CHECK(nb == 3);
  CHECK(g_log[bot[nb - 1] + 1].kind == 'o');
  for (int i = 0; i < 3; i++) {
    CHECK(!strcmp(g_log[top[i]].key, bg));
    CHECK(!strcmp(g_log[bot[i]].key, bg));
  }
  /* ---- 2: whole grid periods: x 0, 128, 256; the rows moved by -64 and +96 from where the canvas' own copy is drawn (bottom edge 180) */
  static const float X[3] = {0, 128, 256};
  for (int i = 0; i < 3; i++) {
    CHECK(g_log[top[i]].x == X[i] && g_log[bot[i]].x == X[i]);
    const float dy_top = g_log[top[i]].y - CANVAS_H, dy_bot = g_log[bot[i]].y - CANVAS_H;
    CHECK(dy_top == -64 && dy_bot == 96);
    CHECK(((int)dy_top) % 32 == 0 && ((int)dy_bot) % 32 == 0 && ((int)X[i]) % 32 == 0);
    /* drawn like the canvas' own: origin (0, 1), the image's bottom edge on y */
    CHECK(g_log[top[i]].w == 0 && g_log[top[i]].h == 1 && g_log[bot[i]].w == 0 && g_log[bot[i]].h == 1);
  }
  /* ---- 3: no band pixel comes from the bars' columns: the copy drawn last over each x (copies are 320 wide) takes art x = x - copy x */
  for (int band = 0; band < 2; band++) {
    const int* idx = band ? bot : top;
    for (int x = 0; x < 320; x++) {
      int last = -1;
      for (int i = 0; i < 3; i++)
        if (g_log[idx[i]].x <= x && x < g_log[idx[i]].x + 320) last = i;
      CHECK(last >= 0);
      CHECK(x - (int)g_log[idx[last]].x < BAR_X);
    }
  }
  /* ---- 4: the rows copied: dest row y of the band is art row y - shift (for an art ART_H high); inside the art, and not its first rows */
  for (int band = 0; band < 2; band++) {
    const float dy = g_log[(band ? bot : top)[0]].y - CANVAS_H;
    const int y0 = band ? CANVAS_H : -BAND, y1 = band ? CANVAS_H + BAND - 1 : 0;
    const int r0 = y0 - (int)dy - (CANVAS_H - ART_H), r1 = y1 - (int)dy - (CANVAS_H - ART_H);
    CHECK(r0 >= 3 && r1 < ART_H - 1);
  }
  /* ---- 5: before the canvas' own background, which is the same image at the canvas' bottom */
  const int own = bot[nb - 1] + 2;
  CHECK(g_log[own].kind == 'i' && !strcmp(g_log[own].key, bg) && g_log[own].x == 0 && g_log[own].y == CANVAS_H);
  for (int i = own; i < g_n; i++)
    if (g_log[i].kind == 'c' || g_log[i].kind == 'o') CHECK(0); /* nothing else clips: the clips are off for the rest of the screen */
}

int main(void) {
  pf_draw_hook = hook;

  /* the screen is closed: nothing drawn */
  draw();
  CHECK(g_n == 0);

  open_screen(PARTY_RELEASE, false);
  check_screen("ui/party_bg");
  party_close();
  draw();
  CHECK(g_n == 0);

  open_screen(PARTY_RELEASE, true);
  check_screen("ui/party_bg_double");
  party_close();

  open_screen(PARTY_MODIFIER_TRANSFER, false);
  check_screen("ui/party_bg");
  party_close();

  open_screen(PARTY_MODIFIER_TRANSFER, true);
  check_screen("ui/party_bg_double_manage");
  party_close();

  printf("Party bands: the background fills the 30 rows above and below the canvas, grid lines carried on, bars kept out (%d checks)\n", checks);
  return 0;
}
