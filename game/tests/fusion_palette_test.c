/* game/source/fusion_palette.c against the pinned upstream's real Pokemon.updateFusionPalette: replays the trace of
 * fusion-palette.producer.test.ts (random sprite frames, shiny colour maps, the sprite shader's blend) and compares, per case, the list of
 * sprite colours, the fusion colour of each, and the colour the shader draws for each of the first 32.
 * Usage: fusion_palette_test <trace>; exit 0 only with 0 mismatches. */
#include "fusion_palette.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static FILE* f;
static int case_no, mismatches, shown, compared, recoloured, none;
#define CHECK(cond, ...) do { if (!(cond)) { bad = 1; if (shown < 30) { shown++; fprintf(stderr, "case %d: ", case_no); fprintf(stderr, __VA_ARGS__); fputc('\n', stderr); } } } while (0)

static int unhex(int c) { return c >= 'a' ? c - 'a' + 10 : c >= 'A' ? c - 'A' + 10 : c - '0'; }
static void need(int ok) { if (!ok) { fputs("malformed trace\n", stderr); exit(2); } }

int main(int argc, char** argv) {
  if (argc < 2) { fputs("usage: fusion_palette_test <trace>\n", stderr); return 2; }
  const size_t plen = strlen(argv[1]);
  if (plen > 3 && !strcmp(argv[1] + plen - 3, ".gz")) {   /* the committed fixture is gzipped */
    char cmd[1024];
    snprintf(cmd, sizeof cmd, "gzip -dc '%s'", argv[1]);
    f = popen(cmd, "r");
  } else {
    f = fopen(argv[1], "r");
  }
  if (!f) { perror(argv[1]); return 2; }
  char tag[8];
  int version, n_total;
  need(fscanf(f, "%7s %d %d", tag, &version, &n_total) == 3 && !strcmp(tag, "V") && version == 1);
  static uint8_t pixels[4][64 * 64 * 4];
  static char hex[65536];
  for (;;) {
    if (fscanf(f, "%7s", tag) != 1 || tag[0] == '#') break;
    need(!strcmp(tag, "C"));
    need(fscanf(f, "%d", &case_no) == 1);
    fp_image img[4];
    for (int i = 0; i < 4; i++) {
      int w, h;
      need(fscanf(f, "%7s %d %d %65000s", tag, &w, &h, hex) == 4 && !strcmp(tag, "P"));
      need(w * h * 4 <= (int)sizeof pixels[i] && strlen(hex) == (size_t)w * h * 8);
      for (int k = 0; k < w * h * 4; k++) pixels[i][k] = (uint8_t)(unhex(hex[k * 2]) * 16 + unhex(hex[k * 2 + 1]));
      img[i] = (fp_image){pixels[i], w, h};
    }
    static uint32_t pairs[4][64];
    fp_map maps[4];
    for (int i = 0; i < 4; i++) {
      int n;
      need(fscanf(f, "%7s %d", tag, &n) == 2 && !strcmp(tag, "M") && n <= 32);
      for (int k = 0; k < n; k++) {
        char a[16], b[16];
        need(fscanf(f, "%15s %15s", a, b) == 2);
        pairs[i][k * 2] = (uint32_t)strtoul(a, NULL, 16) << 8 | 255;
        pairs[i][k * 2 + 1] = (uint32_t)strtoul(b, NULL, 16) << 8 | 255;
      }
      maps[i] = (fp_map){pairs[i], n};
    }
    int n_sprite;
    need(fscanf(f, "%7s %d", tag, &n_sprite) == 2 && !strcmp(tag, "S"));
    static int sc[600][4], fc[600][4], to[40][3];
    int n_fusion = 0, n_to = 0;
    if (n_sprite >= 0) {
      need(n_sprite <= 600);
      for (int i = 0; i < n_sprite; i++) need(fscanf(f, "%d %d %d %d", &sc[i][0], &sc[i][1], &sc[i][2], &sc[i][3]) == 4);
      need(fscanf(f, "%7s %d", tag, &n_fusion) == 2 && !strcmp(tag, "F") && n_fusion == n_sprite);
      for (int i = 0; i < n_fusion; i++) need(fscanf(f, "%d %d %d %d", &fc[i][0], &fc[i][1], &fc[i][2], &fc[i][3]) == 4);
      need(fscanf(f, "%7s %d", tag, &n_to) == 2 && !strcmp(tag, "T"));
      for (int i = 0; i < n_to; i++) need(fscanf(f, "%d %d %d", &to[i][0], &to[i][1], &to[i][2]) == 3);
    }
    need(fscanf(f, "%7s", tag) == 1 && !strcmp(tag, "E"));

    int bad = 0;
    static fp_result r;
    const int ok = fp_compute(img, maps, &r);
    if (n_sprite < 0) {
      CHECK(!ok, "upstream recoloured nothing, the port made %d pairs", r.n);
      none++;
    } else {
      CHECK(ok, "upstream recoloured, the port did not");
      if (ok) {
        recoloured++;
        CHECK(r.n_sprite_colors == n_sprite, "%d sprite colours, upstream %d", r.n_sprite_colors, n_sprite);
        CHECK(r.n == n_to, "%d pairs, upstream %d", r.n, n_to);
        for (int i = 0; i < r.n && i < n_to; i++) {
          CHECK(r.from[i][0] == sc[i][0] && r.from[i][1] == sc[i][1] && r.from[i][2] == sc[i][2], "sprite colour %d is %d,%d,%d, upstream %d,%d,%d", i,
                r.from[i][0], r.from[i][1], r.from[i][2], sc[i][0], sc[i][1], sc[i][2]);
          CHECK(r.fusion[i][0] == fc[i][0] && r.fusion[i][1] == fc[i][1] && r.fusion[i][2] == fc[i][2], "fusion colour %d is %d,%d,%d, upstream %d,%d,%d", i,
                r.fusion[i][0], r.fusion[i][1], r.fusion[i][2], fc[i][0], fc[i][1], fc[i][2]);
          CHECK(r.to[i][0] == to[i][0] && r.to[i][1] == to[i][1] && r.to[i][2] == to[i][2], "drawn colour %d is %d,%d,%d, upstream %d,%d,%d", i, r.to[i][0],
                r.to[i][1], r.to[i][2], to[i][0], to[i][1], to[i][2]);
          compared++;
        }
      }
    }
    if (bad) mismatches++;
    case_no++;
  }
  printf("fusion palette: %d recoloured, %d not recoloured, %d colour triples compared. TOTAL mismatches %d\n", recoloured, none, compared, mismatches);
  return mismatches ? 1 : 0;
}
