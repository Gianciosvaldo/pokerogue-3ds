/* See fusion_palette.h. A port of the pinned upstream's Pokemon.updateFusionPalette (src/field/pokemon.ts), src/utils/color-utils.ts, and
 * @material/material-color-utilities' QuantizerCelebi (quantizer_wu.js, quantizer_wsmeans.js, quantizer_map.js, lab_point_provider.js,
 * utils/color_utils.js), in IEEE doubles like the JS, with the places where JS semantics decide the result kept on purpose:
 *   - Map iteration order is insertion order, and a key that is deleted and set again goes last;
 *   - Array.prototype.sort with the comparator `a < b ? 1 : -1`, which is not a consistent comparison for equal counts: V8's sort of an
 *     array under 32 elements (a run found by CountAndMakeRun, then BinaryInsertionSort) is what decides ties, so that is what runs here;
 *   - `distanceToIndexMatrix[i].sort()` with no comparator sorts objects by "[object Object]": a stable no-op;
 *   - the merged colours' keys are argbFromRgba's signed 32-bit numbers while the quantizer's are unsigned, so equal colours with different
 *     keys stay two palette entries. */
#include "fusion_palette.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "pr/rng.h"

/* ------------------------------------------------------------------ color_utils.js */

static double js_round(double x) { return floor(x + 0.5); }
static int clamp_int(int lo, int hi, int v) { return v < lo ? lo : v > hi ? hi : v; }

static const double SRGB_TO_XYZ[3][3] = {
    {0.41233895, 0.35762064, 0.18051042}, {0.2126, 0.7152, 0.0722}, {0.01932141, 0.11916382, 0.95034478}};
static const double XYZ_TO_SRGB[3][3] = {{3.2413774792388685, -1.5376652402851851, -0.49885366846268053},
                                         {-0.9691452513005321, 1.8758853451067872, 0.04156585616912061},
                                         {0.05562093689691305, -0.20395524564742123, 1.0571799111220335}};
static const double WHITE_POINT_D65[3] = {95.047, 100.0, 108.883};

static double linearized(int c) {
  const double normalized = c / 255.0;
  if (normalized <= 0.040449936) return normalized / 12.92 * 100.0;
  return pow((normalized + 0.055) / 1.055, 2.4) * 100.0;
}

static int delinearized(double c) {
  const double normalized = c / 100.0;
  double d;
  if (normalized <= 0.0031308) d = normalized * 12.92;
  else d = 1.055 * pow(normalized, 1.0 / 2.4) - 0.055;
  return clamp_int(0, 255, (int)js_round(d * 255.0));
}

static double lab_f(double t) {
  const double e = 216.0 / 24389.0, kappa = 24389.0 / 27.0;
  if (t > e) return pow(t, 1.0 / 3.0);
  return (kappa * t + 16) / 116;
}

static double lab_invf(double ft) {
  const double e = 216.0 / 24389.0, kappa = 24389.0 / 27.0;
  const double ft3 = ft * ft * ft;
  if (ft3 > e) return ft3;
  return (116 * ft - 16) / kappa;
}

/* ARGB as the JS numbers the code passes around: int64 so that argbFromRgba's signed result and argbFromLab's unsigned one stay different */
static int red_of(int64_t argb) { return (int)((uint32_t)argb >> 16 & 255); }
static int green_of(int64_t argb) { return (int)((uint32_t)argb >> 8 & 255); }
static int blue_of(int64_t argb) { return (int)((uint32_t)argb & 255); }
static int alpha_of(int64_t argb) { return (int)((uint32_t)argb >> 24 & 255); }

static void lab_from_argb(int64_t argb, double lab[3]) {
  const double lr = linearized(red_of(argb)), lg = linearized(green_of(argb)), lb = linearized(blue_of(argb));
  const double x = SRGB_TO_XYZ[0][0] * lr + SRGB_TO_XYZ[0][1] * lg + SRGB_TO_XYZ[0][2] * lb;
  const double y = SRGB_TO_XYZ[1][0] * lr + SRGB_TO_XYZ[1][1] * lg + SRGB_TO_XYZ[1][2] * lb;
  const double z = SRGB_TO_XYZ[2][0] * lr + SRGB_TO_XYZ[2][1] * lg + SRGB_TO_XYZ[2][2] * lb;
  const double fx = lab_f(x / WHITE_POINT_D65[0]), fy = lab_f(y / WHITE_POINT_D65[1]), fz = lab_f(z / WHITE_POINT_D65[2]);
  lab[0] = 116.0 * fy - 16;
  lab[1] = 500.0 * (fx - fy);
  lab[2] = 200.0 * (fy - fz);
}

/* argbFromRgb(...) >>> 0 */
static int64_t argb_from_rgb(int r, int g, int b) {
  return (int64_t)(uint32_t)((255u << 24) | ((uint32_t)(r & 255) << 16) | ((uint32_t)(g & 255) << 8) | (uint32_t)(b & 255));
}

static int64_t argb_from_lab(double l, double a, double b) {
  const double fy = (l + 16.0) / 116.0, fx = a / 500.0 + fy, fz = fy - b / 200.0;
  const double x = lab_invf(fx) * WHITE_POINT_D65[0], y = lab_invf(fy) * WHITE_POINT_D65[1], z = lab_invf(fz) * WHITE_POINT_D65[2];
  const double lr = XYZ_TO_SRGB[0][0] * x + XYZ_TO_SRGB[0][1] * y + XYZ_TO_SRGB[0][2] * z;
  const double lg = XYZ_TO_SRGB[1][0] * x + XYZ_TO_SRGB[1][1] * y + XYZ_TO_SRGB[1][2] * z;
  const double lb = XYZ_TO_SRGB[2][0] * x + XYZ_TO_SRGB[2][1] * y + XYZ_TO_SRGB[2][2] * z;
  return argb_from_rgb(delinearized(lr), delinearized(lg), delinearized(lb));
}

/* color-utils.ts argbFromRgba: Phaser.Math.Clamp each, then (a << 24) | (r << 16) | (g << 8) | b as a signed int32 */
static int64_t argb_from_rgba(double r, double g, double b, double a) {
  const double lo = 0, hi = 255;
  r = r < lo ? lo : r > hi ? hi : r;
  g = g < lo ? lo : g > hi ? hi : g;
  b = b < lo ? lo : b > hi ? hi : b;
  a = a < lo ? lo : a > hi ? hi : a;
  return (int64_t)(int32_t)(((uint32_t)(int)a << 24) | ((uint32_t)(int)r << 16) | ((uint32_t)(int)g << 8) | (uint32_t)(int)b);
}

/* ------------------------------------------------------------------ QuantizerWu */

#define WU_INDEX_BITS 5
#define WU_SIDE 33
#define WU_TOTAL 35937

typedef struct {
  int r0, r1, g0, g1, b0, b1, vol;
} wu_box;

typedef struct {
  double weights[WU_TOTAL], mr[WU_TOTAL], mg[WU_TOTAL], mb[WU_TOTAL], moments[WU_TOTAL];
} wu_state;

static int wu_index(int r, int g, int b) { return (r << (WU_INDEX_BITS * 2)) + (r << (WU_INDEX_BITS + 1)) + r + (g << WU_INDEX_BITS) + g + b; }

static double wu_volume(const wu_box* c, const double* m) {
  return m[wu_index(c->r1, c->g1, c->b1)] - m[wu_index(c->r1, c->g1, c->b0)] - m[wu_index(c->r1, c->g0, c->b1)] +
         m[wu_index(c->r1, c->g0, c->b0)] - m[wu_index(c->r0, c->g1, c->b1)] + m[wu_index(c->r0, c->g1, c->b0)] +
         m[wu_index(c->r0, c->g0, c->b1)] - m[wu_index(c->r0, c->g0, c->b0)];
}

enum { DIR_RED, DIR_GREEN, DIR_BLUE };

static double wu_bottom(const wu_box* c, int dir, const double* m) {
  switch (dir) {
    case DIR_RED:
      return -m[wu_index(c->r0, c->g1, c->b1)] + m[wu_index(c->r0, c->g1, c->b0)] + m[wu_index(c->r0, c->g0, c->b1)] -
             m[wu_index(c->r0, c->g0, c->b0)];
    case DIR_GREEN:
      return -m[wu_index(c->r1, c->g0, c->b1)] + m[wu_index(c->r1, c->g0, c->b0)] + m[wu_index(c->r0, c->g0, c->b1)] -
             m[wu_index(c->r0, c->g0, c->b0)];
    default:
      return -m[wu_index(c->r1, c->g1, c->b0)] + m[wu_index(c->r1, c->g0, c->b0)] + m[wu_index(c->r0, c->g1, c->b0)] -
             m[wu_index(c->r0, c->g0, c->b0)];
  }
}

static double wu_top(const wu_box* c, int dir, int pos, const double* m) {
  switch (dir) {
    case DIR_RED:
      return m[wu_index(pos, c->g1, c->b1)] - m[wu_index(pos, c->g1, c->b0)] - m[wu_index(pos, c->g0, c->b1)] + m[wu_index(pos, c->g0, c->b0)];
    case DIR_GREEN:
      return m[wu_index(c->r1, pos, c->b1)] - m[wu_index(c->r1, pos, c->b0)] - m[wu_index(c->r0, pos, c->b1)] + m[wu_index(c->r0, pos, c->b0)];
    default:
      return m[wu_index(c->r1, c->g1, pos)] - m[wu_index(c->r1, c->g0, pos)] - m[wu_index(c->r0, c->g1, pos)] + m[wu_index(c->r0, c->g0, pos)];
  }
}

static double wu_variance(const wu_state* w, const wu_box* c) {
  const double dr = wu_volume(c, w->mr), dg = wu_volume(c, w->mg), db = wu_volume(c, w->mb);
  const double xx = w->moments[wu_index(c->r1, c->g1, c->b1)] - w->moments[wu_index(c->r1, c->g1, c->b0)] -
                    w->moments[wu_index(c->r1, c->g0, c->b1)] + w->moments[wu_index(c->r1, c->g0, c->b0)] -
                    w->moments[wu_index(c->r0, c->g1, c->b1)] + w->moments[wu_index(c->r0, c->g1, c->b0)] +
                    w->moments[wu_index(c->r0, c->g0, c->b1)] - w->moments[wu_index(c->r0, c->g0, c->b0)];
  const double hypotenuse = dr * dr + dg * dg + db * db;
  return xx - hypotenuse / wu_volume(c, w->weights);
}

typedef struct {
  int cut;
  double maximum;
} wu_max;

static wu_max wu_maximize(const wu_state* w, const wu_box* c, int dir, int first, int last, double wr, double wg, double wb, double ww) {
  const double bottom_r = wu_bottom(c, dir, w->mr), bottom_g = wu_bottom(c, dir, w->mg), bottom_b = wu_bottom(c, dir, w->mb),
               bottom_w = wu_bottom(c, dir, w->weights);
  double max = 0.0;
  int cut = -1;
  for (int i = first; i < last; i++) {
    double hr = bottom_r + wu_top(c, dir, i, w->mr), hg = bottom_g + wu_top(c, dir, i, w->mg), hb = bottom_b + wu_top(c, dir, i, w->mb),
           hw = bottom_w + wu_top(c, dir, i, w->weights);
    if (hw == 0) continue;
    double temp = (hr * hr + hg * hg + hb * hb) * 1.0 / (hw * 1.0);
    hr = wr - hr;
    hg = wg - hg;
    hb = wb - hb;
    hw = ww - hw;
    if (hw == 0) continue;
    temp += (hr * hr + hg * hg + hb * hb) * 1.0 / (hw * 1.0);
    if (temp > max) {
      max = temp;
      cut = i;
    }
  }
  return (wu_max){cut, max};
}

static int wu_cut(const wu_state* w, wu_box* one, wu_box* two) {
  const double wr = wu_volume(one, w->mr), wg = wu_volume(one, w->mg), wb = wu_volume(one, w->mb), ww = wu_volume(one, w->weights);
  const wu_max mr = wu_maximize(w, one, DIR_RED, one->r0 + 1, one->r1, wr, wg, wb, ww);
  const wu_max mg = wu_maximize(w, one, DIR_GREEN, one->g0 + 1, one->g1, wr, wg, wb, ww);
  const wu_max mb = wu_maximize(w, one, DIR_BLUE, one->b0 + 1, one->b1, wr, wg, wb, ww);
  int dir;
  if (mr.maximum >= mg.maximum && mr.maximum >= mb.maximum) {
    if (mr.cut < 0) return 0;
    dir = DIR_RED;
  } else if (mg.maximum >= mr.maximum && mg.maximum >= mb.maximum) {
    dir = DIR_GREEN;
  } else {
    dir = DIR_BLUE;
  }
  two->r1 = one->r1;
  two->g1 = one->g1;
  two->b1 = one->b1;
  switch (dir) {
    case DIR_RED:
      one->r1 = mr.cut;
      two->r0 = one->r1;
      two->g0 = one->g0;
      two->b0 = one->b0;
      break;
    case DIR_GREEN:
      one->g1 = mg.cut;
      two->r0 = one->r0;
      two->g0 = one->g1;
      two->b0 = one->b0;
      break;
    default:
      one->b1 = mb.cut;
      two->r0 = one->r0;
      two->g0 = one->g0;
      two->b0 = one->b1;
      break;
  }
  one->vol = (one->r1 - one->r0) * (one->g1 - one->g0) * (one->b1 - one->b0);
  two->vol = (two->r1 - two->r0) * (two->g1 - two->g0) * (two->b1 - two->b0);
  return 1;
}

typedef struct {
  int64_t argb;
  int count;
} pixel_count;

/* QuantizerWu.quantize(pixels, maxColors): `colors` are the distinct pixels with their counts (QuantizerMap keeps the opaque ones) */
static int wu_quantize(wu_state* w, const pixel_count* colors, int n_colors, int max_colors, int64_t* out) {
  memset(w, 0, sizeof *w);
  for (int i = 0; i < n_colors; i++) {
    if (alpha_of(colors[i].argb) < 255) continue;
    const int red = red_of(colors[i].argb), green = green_of(colors[i].argb), blue = blue_of(colors[i].argb);
    const int bits = 8 - WU_INDEX_BITS;
    const int index = wu_index((red >> bits) + 1, (green >> bits) + 1, (blue >> bits) + 1);
    const double count = colors[i].count;
    w->weights[index] += count;
    w->mr[index] += count * red;
    w->mg[index] += count * green;
    w->mb[index] += count * blue;
    w->moments[index] += count * (red * red + green * green + blue * blue);
  }
  for (int r = 1; r < WU_SIDE; r++) {
    double area[WU_SIDE] = {0}, area_r[WU_SIDE] = {0}, area_g[WU_SIDE] = {0}, area_b[WU_SIDE] = {0}, area2[WU_SIDE] = {0};
    for (int g = 1; g < WU_SIDE; g++) {
      double line = 0, line_r = 0, line_g = 0, line_b = 0, line2 = 0;
      for (int b = 1; b < WU_SIDE; b++) {
        const int index = wu_index(r, g, b);
        line += w->weights[index];
        line_r += w->mr[index];
        line_g += w->mg[index];
        line_b += w->mb[index];
        line2 += w->moments[index];
        area[b] += line;
        area_r[b] += line_r;
        area_g[b] += line_g;
        area_b[b] += line_b;
        area2[b] += line2;
        const int prev = wu_index(r - 1, g, b);
        w->weights[index] = w->weights[prev] + area[b];
        w->mr[index] = w->mr[prev] + area_r[b];
        w->mg[index] = w->mg[prev] + area_g[b];
        w->mb[index] = w->mb[prev] + area_b[b];
        w->moments[index] = w->moments[prev] + area2[b];
      }
    }
  }
  wu_box cubes[8];
  double volume_variance[8] = {0};
  if (max_colors > 8) max_colors = 8;
  memset(cubes, 0, sizeof cubes);
  cubes[0].r1 = cubes[0].g1 = cubes[0].b1 = WU_SIDE - 1;
  int generated = max_colors, next = 0;
  for (int i = 1; i < max_colors; i++) {
    if (wu_cut(w, &cubes[next], &cubes[i])) {
      volume_variance[next] = cubes[next].vol > 1 ? wu_variance(w, &cubes[next]) : 0.0;
      volume_variance[i] = cubes[i].vol > 1 ? wu_variance(w, &cubes[i]) : 0.0;
    } else {
      volume_variance[next] = 0.0;
      i--;
    }
    next = 0;
    double temp = volume_variance[0];
    for (int j = 1; j <= i; j++)
      if (volume_variance[j] > temp) {
        temp = volume_variance[j];
        next = j;
      }
    if (temp <= 0.0) {
      generated = i + 1;
      break;
    }
  }
  int n = 0;
  for (int i = 0; i < generated; i++) {
    const double weight = wu_volume(&cubes[i], w->weights);
    if (weight > 0) {
      const int r = (int)js_round(wu_volume(&cubes[i], w->mr) / weight);
      const int g = (int)js_round(wu_volume(&cubes[i], w->mg) / weight);
      const int b = (int)js_round(wu_volume(&cubes[i], w->mb) / weight);
      /* (255 << 24) | ((r & 0xff) << 16) | ...: a negative int32 in JS */
      out[n++] = (int64_t)(int32_t)((255u << 24) | ((uint32_t)(r & 255) << 16) | ((uint32_t)(g & 255) << 8) | (uint32_t)(b & 255));
    }
  }
  return n;
}

/* ------------------------------------------------------------------ QuantizerWsmeans */

#define WS_MAX_POINTS 1024
#define WS_MAX_CLUSTERS 8
#define WS_MAX_ITERATIONS 10
#define WS_MIN_MOVEMENT 3.0

typedef struct {
  double distance;
  int index;
} dist_index;

static double lab_distance(const double* a, const double* b) {
  const double dl = a[0] - b[0], da = a[1] - b[1], db = a[2] - b[2];
  return dl * dl + da * da + db * db;
}

/* the result is Map<argb, population> in insertion order */
typedef struct {
  int64_t key[WS_MAX_CLUSTERS];
  double value[WS_MAX_CLUSTERS];
  int n;
} pop_map;

static int wsmeans_quantize(const pixel_count* colors, int n_colors, const int64_t* starting, int n_starting, int max_colors, pr_rng* rng,
                            pop_map* out) {
  static double points[WS_MAX_POINTS][3];
  static int counts[WS_MAX_POINTS], cluster_indices[WS_MAX_POINTS];
  out->n = 0;
  if (n_colors > WS_MAX_POINTS) return 0;
  for (int i = 0; i < n_colors; i++) {
    lab_from_argb(colors[i].argb, points[i]);
    counts[i] = colors[i].count;
  }
  int cluster_count = max_colors < n_colors ? max_colors : n_colors;
  if (n_starting > 0 && n_starting < cluster_count) cluster_count = n_starting;
  if (cluster_count > WS_MAX_CLUSTERS) return 0;
  double clusters[WS_MAX_CLUSTERS][3];
  int n_clusters = 0;
  for (int i = 0; i < n_starting && i < WS_MAX_CLUSTERS; i++) lab_from_argb(starting[i], clusters[n_clusters++]);
  const int additional = cluster_count - n_clusters;
  if (n_starting == 0 && additional > 0) {
    for (int i = 0; i < additional; i++) {
      const double l = pr_rng_seed_float(rng) * 100.0;
      const double a = pr_rng_seed_float(rng) * (100.0 - (-100.0) + 1) + -100;
      const double b = pr_rng_seed_float(rng) * (100.0 - (-100.0) + 1) + -100;
      clusters[n_clusters][0] = l;
      clusters[n_clusters][1] = a;
      clusters[n_clusters][2] = b;
      n_clusters++;
    }
  }
  for (int i = 0; i < n_colors; i++) cluster_indices[i] = (int)floor(pr_rng_seed_float(rng) * cluster_count);
  dist_index dmatrix[WS_MAX_CLUSTERS][WS_MAX_CLUSTERS];
  for (int i = 0; i < cluster_count; i++)
    for (int j = 0; j < cluster_count; j++) dmatrix[i][j] = (dist_index){-1, -1};
  double count_sums[WS_MAX_CLUSTERS] = {0};
  for (int iteration = 0; iteration < WS_MAX_ITERATIONS; iteration++) {
    for (int i = 0; i < cluster_count; i++) {
      for (int j = i + 1; j < cluster_count; j++) {
        const double d = lab_distance(clusters[i], clusters[j]);
        dmatrix[j][i].distance = d;
        dmatrix[j][i].index = i;
        dmatrix[i][j].distance = d;
        dmatrix[i][j].index = j;
      }
      /* distanceToIndexMatrix[i].sort() orders objects by their string "[object Object]": every pair is equal, the sort is stable */
    }
    int points_moved = 0;
    for (int i = 0; i < n_colors; i++) {
      const double* point = points[i];
      const int previous_index = cluster_indices[i];
      const double previous_distance = lab_distance(point, clusters[previous_index]);
      double minimum = previous_distance;
      int new_index = -1;
      for (int j = 0; j < cluster_count; j++) {
        if (dmatrix[previous_index][j].distance >= 4 * previous_distance) continue;
        const double d = lab_distance(point, clusters[j]);
        if (d < minimum) {
          minimum = d;
          new_index = j;
        }
      }
      if (new_index != -1) {
        const double change = fabs(sqrt(minimum) - sqrt(previous_distance));
        if (change > WS_MIN_MOVEMENT) {
          points_moved++;
          cluster_indices[i] = new_index;
        }
      }
    }
    if (points_moved == 0 && iteration != 0) break;
    double sums[WS_MAX_CLUSTERS][3] = {{0}};
    for (int i = 0; i < cluster_count; i++) count_sums[i] = 0;
    for (int i = 0; i < n_colors; i++) {
      const int ci = cluster_indices[i];
      const double count = counts[i];
      count_sums[ci] += count;
      sums[ci][0] += points[i][0] * count;
      sums[ci][1] += points[i][1] * count;
      sums[ci][2] += points[i][2] * count;
    }
    for (int i = 0; i < cluster_count; i++) {
      if (count_sums[i] == 0) {
        clusters[i][0] = clusters[i][1] = clusters[i][2] = 0.0;
        continue;
      }
      clusters[i][0] = sums[i][0] / count_sums[i];
      clusters[i][1] = sums[i][1] / count_sums[i];
      clusters[i][2] = sums[i][2] / count_sums[i];
    }
  }
  for (int i = 0; i < cluster_count; i++) {
    if (count_sums[i] == 0) continue;
    const int64_t color = argb_from_lab(clusters[i][0], clusters[i][1], clusters[i][2]);
    int seen = 0;
    for (int k = 0; k < out->n; k++) seen |= out->key[k] == color;
    if (seen) continue;
    out->key[out->n] = color;
    out->value[out->n] = count_sums[i];
    out->n++;
  }
  return 1;
}

/* ------------------------------------------------------------------ V8's sort of a short array */

/* The comparator of updateFusionPalette: count(a) < count(b) ? 1 : -1 (never 0) */
typedef struct {
  const int64_t* key;
  const double* value;
  int n;
} pop_view;

static double pop_of(const pop_view* v, int64_t key) {
  for (int i = 0; i < v->n; i++)
    if (v->key[i] == key) return v->value[i];
  return 0;
}
static int pop_cmp(const pop_view* v, int64_t a, int64_t b) { return pop_of(v, a) < pop_of(v, b) ? 1 : -1; }

/* ArrayTimSortImpl for fewer than 32 elements: CountAndMakeRun, then BinaryInsertionSort of the rest */
static void v8_sort(const pop_view* v, int64_t* a, int n) {
  if (n < 2) return;
  int run = 2;
  {
    const int order0 = pop_cmp(v, a[1], a[0]);
    const int descending = order0 < 0;
    int64_t previous = a[1];
    for (int idx = 2; idx < n; idx++) {
      const int order = pop_cmp(v, a[idx], previous);
      if (descending) {
        if (order >= 0) break;
      } else if (order < 0) {
        break;
      }
      previous = a[idx];
      run++;
    }
    if (descending)
      for (int i = 0, j = run - 1; i < j; i++, j--) {
        const int64_t t = a[i];
        a[i] = a[j];
        a[j] = t;
      }
  }
  for (int start = run; start < n; start++) {
    int left = 0, right = start;
    const int64_t pivot = a[start];
    while (left < right) {
      const int mid = left + ((right - left) >> 1);
      if (pop_cmp(v, pivot, a[mid]) < 0) right = mid;
      else left = mid + 1;
    }
    for (int p = start; p > left; --p) a[p] = a[p - 1];
    a[left] = pivot;
  }
}

/* ------------------------------------------------------------------ updateFusionPalette */

#define MAX_PAL 8

typedef struct {
  int64_t key[MAX_PAL];
  double count[MAX_PAL];
  int n;   /* insertion order, like a JS Map */
} jsmap;

static int map_find(const jsmap* m, int64_t key) {
  for (int i = 0; i < m->n; i++)
    if (m->key[i] == key) return i;
  return -1;
}
static void map_delete(jsmap* m, int64_t key) {
  const int i = map_find(m, key);
  if (i < 0) return;
  for (int k = i; k + 1 < m->n; k++) {
    m->key[k] = m->key[k + 1];
    m->count[k] = m->count[k + 1];
  }
  m->n--;
}
static void map_set(jsmap* m, int64_t key, double count) {
  const int i = map_find(m, key);
  if (i >= 0) {
    m->count[i] = count;
    return;
  }
  if (m->n < MAX_PAL) {
    m->key[m->n] = key;
    m->count[m->n] = count;
    m->n++;
  }
}

static void rgb_to_hsv(double r, double g, double b, double hsv[3]) {
  const double v = fmax(fmax(r, g), b);
  const double c = v - fmin(fmin(r, g), b);
  double h = 0;
  if (c) h = v == r ? (g - b) / c : v == g ? 2 + (b - r) / c : 4 + (r - g) / c;
  hsv[0] = 60 * (h < 0 ? h + 6 : h);
  hsv[1] = v ? c / v : 0;
  hsv[2] = v;
}

typedef struct {
  int rgba[MAX_PAL][4];
  int n;
} palette_out;

/* the `[palette, fusionPalette] = [...].map(paletteColors => {...})` body */
static void merge_palette(jsmap* pal, palette_out* out) {
  int64_t keys[MAX_PAL];
  int n_keys = pal->n;
  for (int i = 0; i < n_keys; i++) keys[i] = pal->key[i];
  pop_view view = {pal->key, pal->count, pal->n};
  v8_sort(&view, keys, n_keys);
  for (;;) {
    int rgba[MAX_PAL][4];
    double hsv[MAX_PAL][3];
    for (int i = 0; i < n_keys; i++) {
      rgba[i][0] = red_of(keys[i]);
      rgba[i][1] = green_of(keys[i]);
      rgba[i][2] = blue_of(keys[i]);
      rgba[i][3] = alpha_of(keys[i]);
      rgb_to_hsv(rgba[i][0], rgba[i][1], rgba[i][2], hsv[i]);
    }
    /* mappedColors: key -> the earlier keys it is within 30 degrees of (the first one found), in insertion order */
    int64_t mkey[MAX_PAL];
    int64_t mvals[MAX_PAL][MAX_PAL];
    int n_vals[MAX_PAL], alive[MAX_PAL], n_mapped = 0;
    for (int c = n_keys - 1; c >= 0; c--) {
      for (int c2 = 0; c2 < c; c2++) {
        const double diff = fabs(hsv[c][0] - hsv[c2][0]);
        if (diff < 30 || diff >= 330) {
          int at = -1;
          for (int m = 0; m < n_mapped; m++)
            if (alive[m] && mkey[m] == keys[c]) at = m;
          if (at < 0) {
            at = n_mapped++;
            mkey[at] = keys[c];
            n_vals[at] = 0;
            alive[at] = 1;
          }
          mvals[at][n_vals[at]++] = keys[c2];
          break;
        }
      }
    }
    /* mappedColors.forEach: entries deleted along the way (merged into another) are skipped */
    for (int m = 0; m < n_mapped; m++) {
      if (!alive[m]) continue;
      const int64_t key = mkey[m];
      const int* key_color = NULL;
      for (int i = 0; i < n_keys; i++)
        if (keys[i] == key) key_color = rgba[i];
      if (!key_color) continue;
      double color[4] = {key_color[0], key_color[1], key_color[2], key_color[3]};
      const int ki = map_find(pal, key);
      if (ki < 0) continue;
      const double key_count = pal->count[ki];
      double count = key_count;
      for (int v = 0; v < n_vals[m]; v++) {
        const int vi = map_find(pal, mvals[m][v]);
        const double value_count = vi < 0 ? 0 : pal->count[vi];
        if (!value_count) continue;
        count += value_count;
      }
      for (int c = 0; c < 3; c++) {
        color[c] *= key_count / count;
        for (int v = 0; v < n_vals[m]; v++) {
          const int vi = map_find(pal, mvals[m][v]);
          if (vi < 0) continue;
          const int* value_color = NULL;
          for (int i = 0; i < n_keys; i++)
            if (keys[i] == mvals[m][v]) value_color = rgba[i];
          if (!value_color) continue;
          color[c] += value_color[c] * (pal->count[vi] / count);
        }
        color[c] = js_round(color[c]);
      }
      map_delete(pal, key);
      for (int v = 0; v < n_vals[m]; v++) {
        map_delete(pal, mvals[m][v]);
        for (int k = 0; k < n_mapped; k++)
          if (alive[k] && mkey[k] == mvals[m][v]) alive[k] = 0;
      }
      map_set(pal, argb_from_rgba(color[0], color[1], color[2], color[3]), count);
    }
    n_keys = pal->n;
    for (int i = 0; i < n_keys; i++) keys[i] = pal->key[i];
    view = (pop_view){pal->key, pal->count, pal->n};
    v8_sort(&view, keys, n_keys);
    int any = 0;
    for (int m = 0; m < n_mapped; m++) any |= alive[m];
    if (!any) break;
  }
  out->n = n_keys;
  for (int i = 0; i < n_keys; i++) {
    out->rgba[i][0] = red_of(keys[i]);
    out->rgba[i][1] = green_of(keys[i]);
    out->rgba[i][2] = blue_of(keys[i]);
    out->rgba[i][3] = alpha_of(keys[i]);
  }
}

static int delta_rgb(const int a[3], const int b[3]) {
  const double drp2 = (double)(a[0] - b[0]) * (a[0] - b[0]), dgp2 = (double)(a[1] - b[1]) * (a[1] - b[1]),
               dbp2 = (double)(a[2] - b[2]) * (a[2] - b[2]);
  const double t = (a[0] + b[0]) / 2.0;
  return (int)ceil(sqrt(2 * drp2 + 4 * dgp2 + 3 * dbp2 + (t * (drp2 - dbp2)) / 256));
}

#define MAX_DISTINCT 1024
#define MAX_SPRITE_COLORS 512

typedef struct {
  pixel_count colors[MAX_DISTINCT];
  int n;
} distinct;

/* the Map<number, count> of `pixelColors.push(argb)` / the wsmeans pixelToCount: distinct values in first-seen order */
static int distinct_add(distinct* d, int64_t argb) {
  for (int i = 0; i < d->n; i++)
    if (d->colors[i].argb == argb) {
      d->colors[i].count++;
      return 1;
    }
  if (d->n >= MAX_DISTINCT) return 0;
  d->colors[d->n].argb = argb;
  d->colors[d->n].count = 1;
  d->n++;
  return 1;
}

static int mapped_pixel(const fp_map* m, const uint8_t* px, int out[4]) {
  out[0] = px[0];
  out[1] = px[1];
  out[2] = px[2];
  out[3] = px[3];
  if (!m || px[3] != 255) return 0;
  for (int i = 0; i < m->n; i++) {
    const uint32_t from = m->from_to[i * 2], to = m->from_to[i * 2 + 1];
    if ((int)(from >> 24) == px[0] && (int)(from >> 16 & 255) == px[1] && (int)(from >> 8 & 255) == px[2]) {
      out[0] = (int)(to >> 24);
      out[1] = (int)(to >> 16 & 255);
      out[2] = (int)(to >> 8 & 255);
      out[3] = 255;
      return 1;
    }
  }
  return 0;
}

static int compute(wu_state* wu, const fp_image images[4], const fp_map maps[4], fp_result* out) {
  static distinct pixel_colors, fusion_pixel_colors;
  static int sprite[MAX_SPRITE_COLORS][4];
  memset(out, 0, sizeof *out);
  pixel_colors.n = fusion_pixel_colors.n = 0;
  int n_sprite = 0;

  /* spriteColors: the visible colours of the first species' two frames (after the shiny variant's map), in reading order */
  for (int f = 0; f < 2; f++) {
    const fp_image* im = &images[f];
    for (int i = 0; i < im->w * im->h; i++) {
      const uint8_t* px = im->rgba + (size_t)i * 4;
      if (!px[3]) continue;
      int c[4];
      mapped_pixel(maps ? &maps[f] : NULL, px, c);
      int seen = 0;
      for (int k = 0; k < n_sprite && !seen; k++) seen = sprite[k][0] == c[0] && sprite[k][1] == c[1] && sprite[k][2] == c[2];
      if (!seen && n_sprite < MAX_SPRITE_COLORS) {
        memcpy(sprite[n_sprite], c, sizeof c);
        n_sprite++;
      }
    }
  }
  /* pixelColors: the first species' pixels as they are, but black ones */
  for (int f = 0; f < 2; f++) {
    const fp_image* im = &images[f];
    for (int i = 0; i < im->w * im->h; i++) {
      const uint8_t* px = im->rgba + (size_t)i * 4;
      if (!(px[0] + px[1] + px[2])) continue;
      if (!distinct_add(&pixel_colors, argb_from_rgba(px[0], px[1], px[2], px[3]))) return 0;
    }
  }
  /* fusionPixelColors: the fusion half's, after its shiny variant's map */
  for (int f = 0; f < 2; f++) {
    const fp_image* im = &images[2 + f];
    for (int i = 0; i < im->w * im->h; i++) {
      const uint8_t* px = im->rgba + (size_t)i * 4;
      if (!(px[0] + px[1] + px[2])) continue;
      int c[4];
      mapped_pixel(maps ? &maps[2 + f] : NULL, px, c);
      if (!distinct_add(&fusion_pixel_colors, argb_from_rgba(c[0], c[1], c[2], c[3]))) return 0;
    }
  }
  if (fusion_pixel_colors.n == 0) return 0;   /* "Failed to create fusion palette" */

  /* QuantizerCelebi twice in one seeded stream: executeWithSeedOffset(..., 0, "This result should not vary") */
  pr_rng rng;
  {
    pr_seedstr seed, shifted;
    pr_seed_from_ascii(&seed, "This result should not vary");
    pr_seed_shift(&shifted, &seed, 0);
    pr_rng_sow(&rng, &shifted);
  }
  jsmap palette_map = {0}, fusion_map = {0};
  for (int pass = 0; pass < 2; pass++) {
    const distinct* d = pass ? &fusion_pixel_colors : &pixel_colors;
    jsmap* m = pass ? &fusion_map : &palette_map;
    int64_t starting[8];
    const int n_starting = wu_quantize(wu, d->colors, d->n, 4, starting);
    pop_map pop;
    if (!wsmeans_quantize(d->colors, d->n, starting, n_starting, 4, &rng, &pop)) return 0;
    for (int i = 0; i < pop.n && i < MAX_PAL; i++) {
      m->key[i] = pop.key[i];
      m->count[i] = pop.value[i];
    }
    m->n = pop.n < MAX_PAL ? pop.n : MAX_PAL;
  }
  palette_out palette, fusion_palette;
  merge_palette(&palette_map, &palette);
  merge_palette(&fusion_map, &fusion_palette);

  /* each sprite colour towards the fusion palette colour that sits where it does in the sprite's own palette */
  static int fusion_sprite[MAX_SPRITE_COLORS][4];
  memcpy(fusion_sprite, sprite, sizeof fusion_sprite);
  for (int sc = 0; sc < n_sprite; sc++) {
    int delta = 1 << 30, at = -1;
    for (int p = 0; p < palette.n; p++) {
      const int d = delta_rgb(sprite[sc], palette.rgba[p]);
      if (d < delta) {
        delta = d;
        at = p;
      }
    }
    if (at < 0) continue;   /* Math.min() of nothing is Infinity: not < 255 */
    if (at > fusion_palette.n - 1) at = fusion_palette.n - 1;
    if (at < 0) continue;
    if (delta < 255) {
      const double t = delta / 255.0, ratio = t * t * t;   /* Cubic.easeIn */
      for (int c = 0; c < 3; c++)
        fusion_sprite[sc][c] = (int)js_round(fusion_sprite[sc][c] * ratio + fusion_palette.rgba[at][c] * (1 - ratio));
    }
  }

  /* the shader: the first 32 spriteColors, a visible pixel of exactly that colour becomes the overlay of its grey over the fusion colour */
  out->n_sprite_colors = n_sprite;
  for (int i = 0; i < n_sprite && i < FP_MAX_PAIRS; i++) {
    if (sprite[i][3] == 0) break;   /* the loop stops at the first colour with no alpha (none here: only visible pixels were collected) */
    out->from[out->n][0] = (uint8_t)sprite[i][0];
    out->from[out->n][1] = (uint8_t)sprite[i][1];
    out->from[out->n][2] = (uint8_t)sprite[i][2];
    out->sprite_alpha[out->n] = (uint8_t)sprite[i][3];
    const double gray = (sprite[i][0] + sprite[i][1] + sprite[i][2]) / 3.0 / 255.0;
    for (int c = 0; c < 3; c++) {
      const double fg = fusion_sprite[i][c] / 255.0, bg = gray;
      const double blended = bg <= 0.5 ? 2.0 * bg * fg : 1.0 - 2.0 * (1.0 - bg) * (1.0 - fg);   /* mix(1-2(1-bg)(1-fg), 2 bg fg, step(bg, .5)) */
      out->fusion[out->n][c] = (uint8_t)fusion_sprite[i][c];
      int v = (int)js_round(blended * 255.0);
      out->to[out->n][c] = (uint8_t)clamp_int(0, 255, v);
    }
    out->n++;
  }
  return 1;
}

/* the Wu quantizer's histogram is over a megabyte: it is taken for the call, not kept */
int fp_compute(const fp_image images[4], const fp_map maps[4], fp_result* out) {
  wu_state* wu = malloc(sizeof *wu);
  if (!wu) {
    memset(out, 0, sizeof *out);
    return 0;
  }
  const int ok = compute(wu, images, maps, out);
  free(wu);
  return ok;
}

/* ------------------------------------------------------------------ RRT sheets */

#pragma pack(push, 1)
typedef struct {
  char magic[4];
  uint8_t kind, gpu;
  uint16_t pages, frames, npal, img_w, img_h;
  uint32_t names_size;
} rrt_head;
typedef struct {
  uint16_t page, x, y, w, h;
  int16_t off_x, off_y;
  uint16_t src_w, src_h, name_off;
} rrt_fr;
#pragma pack(pop)

int fp_rrt_first_frame(const void* rrt, size_t len, uint8_t* rgba, size_t cap, int* w, int* h) {
  const uint8_t* p = rrt;
  rrt_head hd;
  if (len < sizeof hd) return 0;
  memcpy(&hd, p, sizeof hd);
  if (memcmp(hd.magic, "RRT1", 4) || hd.kind != 0 || hd.frames < 1 || hd.pages < 1 || hd.npal > 256) return 0;
  size_t off = sizeof hd;
  const size_t pal_off = off;
  off += (size_t)hd.npal * 4u;
  const size_t dims_off = off;
  off += (size_t)hd.pages * 4u;
  const size_t fr_off = off;
  off += (size_t)hd.frames * sizeof(rrt_fr) + hd.names_size;
  off = (off + 3) & ~(size_t)3;
  if (off > len) return 0;
  rrt_fr fr;
  memcpy(&fr, p + fr_off, sizeof fr);
  if (fr.page >= hd.pages) return 0;
  /* the pages follow one after the other, one byte per texel, each in the GPU's tile order (gfx.py swizzle: 8x8 tiles row by row, the 64
   * texels of a tile in Morton order) */
  size_t page_off = off;
  for (int i = 0; i < fr.page; i++) {
    uint16_t d[2];
    memcpy(d, p + dims_off + (size_t)i * 4, 4);
    page_off += (size_t)d[0] * d[1];
  }
  uint16_t pw, ph;
  memcpy(&pw, p + dims_off + (size_t)fr.page * 4, 2);
  memcpy(&ph, p + dims_off + (size_t)fr.page * 4 + 2, 2);
  if (pw < 8 || ph < 8 || (pw & 7) || (ph & 7) || page_off + (size_t)pw * ph > len || fr.x + fr.w > pw || fr.y + fr.h > ph) return 0;
  *w = fr.w;
  *h = fr.h;
  if (!rgba) return 1;
  if (cap < (size_t)fr.w * fr.h * 4) return 0;
  for (int y = 0; y < fr.h; y++)
    for (int x = 0; x < fr.w; x++) {
      const unsigned tx = (unsigned)(fr.x + x), ty = (unsigned)(fr.y + y);
      const unsigned morton = (tx & 1) | ((ty & 1) << 1) | ((tx & 2) << 1) | ((ty & 2) << 2) | ((tx & 4) << 2) | ((ty & 4) << 3);
      const uint8_t idx = p[page_off + ((((size_t)(ty >> 3) * (pw >> 3) + (tx >> 3)) << 6) + morton)];
      uint32_t c = 0;
      if (idx < hd.npal) memcpy(&c, p + pal_off + (size_t)idx * 4, 4);
      if (idx == 0) c = 0;
      uint8_t* o = rgba + ((size_t)y * fr.w + x) * 4;
      o[0] = (uint8_t)(c >> 24);
      o[1] = (uint8_t)(c >> 16);
      o[2] = (uint8_t)(c >> 8);
      o[3] = (uint8_t)c;
    }
  return 1;
}
