/* fp_rrt_first_frame (source/fusion_palette.c) on a sheet written by the asset pipeline's own encoder (assets-pipeline/gfx.py): the frame
 * must come out as the pixels it was cut from, through the pipeline's palette and its tile order. Usage: rrt_first_frame_test <fixture> */
#include "fusion_palette.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int unhex(int c) { return c >= 'a' ? c - 'a' + 10 : c - '0'; }

static uint8_t* read_hex(FILE* f, size_t* n) {
  static char buf[1 << 16];
  if (fscanf(f, "%65000s", buf) != 1) { fputs("malformed fixture\n", stderr); exit(2); }
  *n = strlen(buf) / 2;
  uint8_t* out = malloc(*n + 1);
  for (size_t i = 0; i < *n; i++) out[i] = (uint8_t)(unhex(buf[i * 2]) * 16 + unhex(buf[i * 2 + 1]));
  return out;
}

int main(int argc, char** argv) {
  if (argc < 2) { fputs("usage: rrt_first_frame_test <fixture>\n", stderr); return 2; }
  FILE* f = fopen(argv[1], "r");
  if (!f) { perror(argv[1]); return 2; }
  char line[256], tag[4];
  if (!fgets(line, sizeof line, f)) return 2;   /* the comment */
  size_t rlen, xlen;
  int w, h, bad = 0;
  if (fscanf(f, "%3s", tag) != 1 || strcmp(tag, "R")) return 2;
  uint8_t* rrt = read_hex(f, &rlen);
  if (fscanf(f, "%3s %d %d", tag, &w, &h) != 3 || strcmp(tag, "W")) return 2;
  if (fscanf(f, "%3s", tag) != 1 || strcmp(tag, "X")) return 2;
  uint8_t* want = read_hex(f, &xlen);
  if (xlen != (size_t)w * h * 4) { fputs("malformed fixture\n", stderr); return 2; }

  int gw = 0, gh = 0;
  if (!fp_rrt_first_frame(rrt, rlen, NULL, 0, &gw, &gh) || gw != w || gh != h) {
    fprintf(stderr, "size %dx%d, want %dx%d\n", gw, gh, w, h);
    bad++;
  }
  uint8_t* got = malloc(xlen);
  if (!fp_rrt_first_frame(rrt, rlen, got, xlen, &gw, &gh)) { fputs("the frame was not read\n", stderr); bad++; }
  else if (memcmp(got, want, xlen)) {
    size_t diff = 0;
    for (size_t i = 0; i < xlen; i++) diff += got[i] != want[i];
    fprintf(stderr, "%zu of %zu bytes differ\n", diff, xlen);
    bad++;
  }
  if (fp_rrt_first_frame(rrt, rlen, got, xlen - 1, &gw, &gh)) { fputs("a buffer too small was accepted\n", stderr); bad++; }
  rrt[0] = 'X';
  if (fp_rrt_first_frame(rrt, rlen, got, xlen, &gw, &gh)) { fputs("a sheet with no RRT1 magic was accepted\n", stderr); bad++; }
  rrt[0] = 'R';
  if (fp_rrt_first_frame(rrt, rlen / 2, got, xlen, &gw, &gh)) { fputs("a cut-off sheet was accepted\n", stderr); bad++; }
  free(rrt);
  free(want);
  free(got);
  fclose(f);
  printf("rrt first frame: %d checks failed\n", bad);
  return bad ? 1 : 0;
}
