/*
 * dspadpcm: encodes 16-bit PCM into the 3DS DSP's ADPCM format and writes an
 * RSND sound entry (the format the app's snd.c streams or plays).
 *
 *   dspadpcm IN.raw OUT.rsnd CHANNELS RATE [LOOP_START|-1] [BLOCK_SAMPLES]
 *
 * IN.raw: interleaved s16le PCM. LOOP_START: default sample index the loop
 * restarts from (rounded to a 14-sample frame), -1 for none; the app can loop
 * from any other point too, using the seek table. BLOCK_SAMPLES (default 14336,
 * multiple of 14; 0 = one block holding everything): the streaming block size;
 * channels are interleaved per block so one read gets all of them.
 * Prints the signal-to-noise ratio of the encode to stdout.
 *
 * Format (DSP-ADPCM, the codec commercial 3DS games stream music in):
 * a frame is 8 bytes = 1 header byte (predictor index << 4 | scale) + 14
 * 4-bit samples; sample = clamp16((((n << scale) << 11) + 1024 + c1*h1 + c2*h2) >> 11).
 * Each channel has 8 predictor pairs. Here they are chosen by Lloyd iteration
 * on the real objective (per-frame prediction error), not by the reference
 * encoder's algorithm; each frame then tries every predictor and nearby
 * scales and keeps the lowest error.
 *
 * RSND layout (little endian):
 *   "RSN1" u8 channels u8 encoding(2 = DSP-ADPCM) u16 0
 *   u32 rate, u32 samples (per channel), u32 loop_start (0xFFFFFFFF none)
 *   u32 block_samples, u32 block_bytes (per channel, full block), u32 data_offset, u32 blocks
 *   per channel (36 bytes): s16 coefs[16], u16 ps0, u16 0
 *   seek table: blocks x channels x {s16 hist1, s16 hist2} (decoder history at block start)
 *   data at data_offset: for each block, channel 0's bytes then channel 1's...
 *   The last block is short (ceil(samples_left / 14) * 8 bytes per channel).
 */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NCOEF 8

static inline int16_t clamp16(int32_t v) { return v < -32768 ? -32768 : v > 32767 ? 32767 : (int16_t)v; }

/* ---- predictor selection ---- */

typedef struct {
  double r00, r01, r02, r11, r12, r22; /* autocorrelation of (x[n], x[n-1], x[n-2]) over one frame */
} frame_stats;

static double frame_err(const frame_stats* s, double a1, double a2) {
  /* sum (x - a1 x1 - a2 x2)^2 */
  return s->r00 - 2 * a1 * s->r01 - 2 * a2 * s->r02 + a1 * a1 * s->r11 + 2 * a1 * a2 * s->r12 + a2 * a2 * s->r22;
}

static void choose_coefs(const int16_t* x, size_t n, int16_t out[16]) {
  size_t nf = (n + 13) / 14;
  frame_stats* fs = calloc(nf, sizeof *fs);
  for (size_t f = 0; f < nf; f++) {
    for (size_t k = 0; k < 14; k++) {
      size_t i = f * 14 + k;
      if (i >= n) break;
      double x0 = x[i], x1 = i >= 1 ? x[i - 1] : 0, x2 = i >= 2 ? x[i - 2] : 0;
      fs[f].r00 += x0 * x0;
      fs[f].r01 += x0 * x1;
      fs[f].r02 += x0 * x2;
      fs[f].r11 += x1 * x1;
      fs[f].r12 += x1 * x2;
      fs[f].r22 += x2 * x2;
    }
  }
  double a[NCOEF][2] = {{0, 0}, {1, 0}, {2, -1}, {1.5, -0.75}, {1.8, -0.9}, {0.5, 0}, {1.2, -0.3}, {1.95, -0.97}};
  int* assign = malloc(nf * sizeof(int));
  for (int it = 0; it < 12; it++) {
    double S[NCOEF][6] = {{0}};
    int cnt[NCOEF] = {0};
    for (size_t f = 0; f < nf; f++) {
      if (fs[f].r00 == 0) {
        assign[f] = -1;
        continue;
      }
      int best = 0;
      double be = 1e300;
      for (int c = 0; c < NCOEF; c++) {
        double e = frame_err(&fs[f], a[c][0], a[c][1]);
        if (e < be) {
          be = e;
          best = c;
        }
      }
      assign[f] = best;
      cnt[best]++;
      S[best][0] += fs[f].r11;
      S[best][1] += fs[f].r12;
      S[best][2] += fs[f].r22;
      S[best][3] += fs[f].r01;
      S[best][4] += fs[f].r02;
    }
    for (int c = 0; c < NCOEF; c++) {
      if (!cnt[c]) continue;
      /* [r11 r12; r12 r22] a = [r01; r02] */
      double r11 = S[c][0] * (1 + 1e-9) + 1e-6, r12 = S[c][1], r22 = S[c][2] * (1 + 1e-9) + 1e-6;
      double det = r11 * r22 - r12 * r12;
      if (fabs(det) < 1e-12) continue;
      double a1 = (S[c][3] * r22 - S[c][4] * r12) / det;
      double a2 = (S[c][4] * r11 - S[c][3] * r12) / det;
      if (a1 > 15.9) a1 = 15.9;
      if (a1 < -16) a1 = -16;
      if (a2 > 15.9) a2 = 15.9;
      if (a2 < -16) a2 = -16;
      a[c][0] = a1;
      a[c][1] = a2;
    }
  }
  for (int c = 0; c < NCOEF; c++) {
    out[c * 2] = clamp16((int32_t)lrint(a[c][0] * 2048));
    out[c * 2 + 1] = clamp16((int32_t)lrint(a[c][1] * 2048));
  }
  free(assign);
  free(fs);
}

/* ---- frame encoder ---- */

/* Encodes one frame of up to 14 samples. h1/h2: decoder history in/out. */
static void encode_frame(const int16_t* x, int count, const int16_t* coefs, int16_t* h1, int16_t* h2,
                         uint8_t out[8], double* err_out) {
  double best_err = 1e300;
  uint8_t best[8];
  int16_t bh1 = *h1, bh2 = *h2;
  for (int c = 0; c < NCOEF; c++) {
    int32_t c1 = coefs[c * 2], c2 = coefs[c * 2 + 1];
    /* estimate the scale from the open-loop residual */
    int32_t maxr = 0;
    {
      int16_t p1 = *h1, p2 = *h2;
      for (int i = 0; i < count; i++) {
        int32_t pred = (c1 * p1 + c2 * p2 + 1024) >> 11;
        int32_t r = x[i] - pred;
        if (r < 0) r = -r;
        if (r > maxr) maxr = r;
        p2 = p1;
        p1 = x[i];
      }
    }
    int s0 = 0;
    while (s0 < 12 && (7 << s0) < maxr) s0++;
    for (int s = s0 - 1; s <= s0 + 1; s++) {
      if (s < 0 || s > 12) continue;
      int16_t p1 = *h1, p2 = *h2;
      uint8_t buf[8] = {(uint8_t)((c << 4) | s)};
      double e = 0;
      for (int i = 0; i < 14; i++) {
        int32_t xi = i < count ? x[i] : 0;
        int32_t base = c1 * p1 + c2 * p2 + 1024;
        /* nibble minimising |decoded - x| */
        double want = ((double)xi * 2048 - base) / (double)(2048 << s);
        int32_t nib = (int32_t)lrint(want);
        if (nib > 7) nib = 7;
        if (nib < -8) nib = -8;
        int32_t dec = clamp16((((nib << s) << 11) + base) >> 11);
        /* the clamp or rounding can make a neighbour better */
        for (int d = -1; d <= 1; d += 2) {
          int32_t n2 = nib + d;
          if (n2 < -8 || n2 > 7) continue;
          int32_t dec2 = clamp16((((n2 << s) << 11) + base) >> 11);
          if (abs(dec2 - xi) < abs(dec - xi)) {
            nib = n2;
            dec = dec2;
          }
        }
        if (i < count) e += (double)(dec - xi) * (dec - xi);
        buf[1 + i / 2] |= (uint8_t)((nib & 15) << (i & 1 ? 0 : 4));
        p2 = p1;
        p1 = (int16_t)dec;
        if (e >= best_err) break;
      }
      if (e < best_err) {
        best_err = e;
        memcpy(best, buf, 8);
        bh1 = p1;
        bh2 = p2;
      }
    }
  }
  memcpy(out, best, 8);
  *h1 = bh1;
  *h2 = bh2;
  *err_out = best_err;
}

static void wr16(FILE* f, uint16_t v) { fwrite(&v, 2, 1, f); }
static void wr32(FILE* f, uint32_t v) { fwrite(&v, 4, 1, f); }

int main(int argc, char** argv) {
  if (argc < 5) {
    fprintf(stderr, "usage: %s IN.raw OUT.rsnd CHANNELS RATE [LOOP_START|-1] [BLOCK_SAMPLES]\n", argv[0]);
    return 2;
  }
  int ch = atoi(argv[3]);
  uint32_t rate = (uint32_t)atol(argv[4]);
  long loop = argc > 5 ? atol(argv[5]) : -1;
  uint32_t block = argc > 6 ? (uint32_t)atol(argv[6]) : 14336;
  if (ch < 1 || ch > 2 || block % 14) return 2;
  if (loop >= 0) loop = (loop + 7) / 14 * 14;
  FILE* in = fopen(argv[1], "rb");
  if (!in) return 1;
  fseek(in, 0, SEEK_END);
  size_t bytes = (size_t)ftell(in);
  fseek(in, 0, SEEK_SET);
  size_t n = bytes / 2 / ch;
  int16_t* pcm = malloc(bytes + 4);
  if (fread(pcm, 1, bytes, in) != bytes) return 1;
  fclose(in);
  if (loop >= (long)n) loop = -1;
  if (block == 0) block = (uint32_t)((n + 13) / 14 * 14);
  if (block == 0) block = 14;

  int16_t* chan[2];
  int16_t coefs[2][16];
  for (int c = 0; c < ch; c++) {
    chan[c] = malloc(n * 2 + 28);
    for (size_t i = 0; i < n; i++) chan[c][i] = pcm[i * ch + c];
    choose_coefs(chan[c], n, coefs[c]);
  }
  free(pcm);

  size_t nframes = (n + 13) / 14;
  uint8_t* enc[2];
  uint16_t ps0[2] = {0};
  size_t fpb = block / 14;
  size_t nblocks = (nframes + fpb - 1) / fpb;
  int16_t* seek = calloc(nblocks * ch * 2 + 2, sizeof(int16_t));
  double sig = 0, noise = 0;
  for (int c = 0; c < ch; c++) {
    enc[c] = malloc(nframes * 8);
    int16_t h1 = 0, h2 = 0;
    for (size_t f = 0; f < nframes; f++) {
      if (f % fpb == 0) {
        seek[((f / fpb) * ch + c) * 2] = h1;
        seek[((f / fpb) * ch + c) * 2 + 1] = h2;
      }
      int count = (int)((n - f * 14) < 14 ? n - f * 14 : 14);
      double e;
      encode_frame(chan[c] + f * 14, count, coefs[c], &h1, &h2, enc[c] + f * 8, &e);
      noise += e;
      for (int i = 0; i < count; i++) sig += (double)chan[c][f * 14 + i] * chan[c][f * 14 + i];
    }
    ps0[c] = nframes ? enc[c][0] : 0;
  }

  FILE* out = fopen(argv[2], "wb");
  if (!out) return 1;
  uint32_t block_bytes = block / 14 * 8;
  uint32_t data_off = (uint32_t)(36 + 36 * ch + nblocks * ch * 4);
  fwrite("RSN1", 1, 4, out);
  fputc(ch, out);
  fputc(2, out);
  wr16(out, 0);
  wr32(out, rate);
  wr32(out, (uint32_t)n);
  wr32(out, loop >= 0 ? (uint32_t)loop : 0xFFFFFFFFu);
  wr32(out, block);
  wr32(out, block_bytes);
  wr32(out, data_off);
  wr32(out, (uint32_t)nblocks);
  for (int c = 0; c < ch; c++) {
    fwrite(coefs[c], 2, 16, out);
    wr16(out, ps0[c]);
    wr16(out, 0);
  }
  fwrite(seek, 2, nblocks * ch * 2, out);
  for (size_t f = 0; f < nframes; f += fpb) {
    size_t cnt = nframes - f < fpb ? nframes - f : fpb;
    for (int c = 0; c < ch; c++) fwrite(enc[c] + f * 8, 8, cnt, out);
  }
  fclose(out);
  double snr = noise > 0 ? 10 * log10(sig / noise) : 999;
  printf("%.2f\n", snr);
  return 0;
}
