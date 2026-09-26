/* snd: ndsp music streaming + sound effects. See include/snd.h and
 * assets-pipeline/tools/dspadpcm.c for the RSND format. */
#include "snd.h"

#include <3ds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pak.h"

#define BGM_BUFS 4
#define BGM_CH0 0 /* ndsp channels 0,1: music L/R; 2..: sound effects */
#define SFX_CH0 2
#define SFX_VOICES 14
#define SFX_CACHE 48
#define SFX_CACHE_BYTES (1024 * 1024)

typedef struct {
  char magic[4];
  u8 channels, encoding;
  u16 pad;
  u32 rate, samples, loop_start, block_samples, block_bytes, data_offset, blocks;
} __attribute__((packed)) rsnd_header;

typedef struct {
  s16 coefs[16];
  u16 ps0, pad;
} rsnd_chan;

static bool g_ready;
static float g_bgm_vol = 1.0f, g_sfx_vol = 1.0f;

/* ------------------------------------------------------------------ */
/* ADPCM helper: decoder history at an arbitrary frame (for loop points) */

static void adpcm_skip(const u8* frames, int nframes, const s16* coefs, s16* h1, s16* h2) {
  s32 a = *h1, b = *h2;
  for (int f = 0; f < nframes; f++) {
    const u8* fr = frames + f * 8;
    int ci = fr[0] >> 4, sc = fr[0] & 15;
    s32 c1 = coefs[ci * 2], c2 = coefs[ci * 2 + 1];
    for (int i = 0; i < 14; i++) {
      int nib = (fr[1 + i / 2] >> (i & 1 ? 0 : 4)) & 15;
      if (nib >= 8) nib -= 16;
      s32 v = (((nib << sc) << 11) + 1024 + c1 * a + c2 * b) >> 11;
      if (v < -32768) v = -32768;
      if (v > 32767) v = 32767;
      b = a;
      a = v;
    }
  }
  *h1 = (s16)a;
  *h2 = (s16)b;
}

/* ------------------------------------------------------------------ */
/* music stream                                                         */

typedef struct {
  bool active, loop, ended;
  FILE* f;
  u32 base; /* entry offset in the pak */
  rsnd_header h;
  rsnd_chan ch[2];
  s16* seek; /* blocks * channels * 2 */
  u32 nframes, fpb, loop_frame;
  u32 cursor; /* next frame to queue */
  ndspWaveBuf wb[2][BGM_BUFS];
  ndspAdpcmData ctx[2][BGM_BUFS];
  u8* mem; /* linear: [ch][buf][block_bytes] */
  int next; /* next buffer slot to fill */
} bgm_stream;

static bgm_stream g_bgm;
static LightLock g_bgm_lock;
static LightEvent g_bgm_event;
static Thread g_bgm_thread;
static volatile bool g_bgm_quit;

static u32 block_frames(const bgm_stream* s, u32 blk) {
  u32 f0 = blk * s->fpb;
  return s->nframes - f0 < s->fpb ? s->nframes - f0 : s->fpb;
}

/* Byte offset (from entry start) of channel c's data for frame f. */
static u32 frame_offset(const bgm_stream* s, u32 f, int c) {
  u32 blk = f / s->fpb;
  u32 full = s->h.block_bytes * s->h.channels; /* bytes per full block, all channels */
  return s->h.data_offset + blk * full + c * block_frames(s, blk) * 8 + (f - blk * s->fpb) * 8;
}

/* Queues the next chunk into slot s->next. Returns false at the end of a non-looping track. */
static bool bgm_fill(bgm_stream* s) {
  if (s->cursor >= s->nframes) {
    if (!s->loop) return false;
    s->cursor = s->loop_frame;
    s->ended = false;
  }
  u32 blk = s->cursor / s->fpb;
  u32 end = blk * s->fpb + block_frames(s, blk);
  u32 nf = end - s->cursor;
  int slot = s->next;
  bool need_ctx = s->cursor == s->loop_frame || s->cursor == 0;
  for (int c = 0; c < s->h.channels; c++) {
    u8* dst = s->mem + ((size_t)c * BGM_BUFS + slot) * s->h.block_bytes;
    if (fseek(s->f, s->base + frame_offset(s, s->cursor, c), SEEK_SET) || fread(dst, 8, nf, s->f) != nf)
      return false;
    DSP_FlushDataCache(dst, nf * 8);
    ndspWaveBuf* w = &s->wb[c][slot];
    memset(w, 0, sizeof *w);
    w->data_adpcm = dst;
    u32 samples = nf * 14;
    if (end == s->nframes && s->h.samples % 14) samples -= 14 - s->h.samples % 14;
    w->nsamples = samples;
    if (need_ctx) {
      /* decoder history at this frame: seek table at the block start, then decode forward */
      s16 h1 = s->seek[(blk * s->h.channels + c) * 2], h2 = s->seek[(blk * s->h.channels + c) * 2 + 1];
      u32 skip = s->cursor - blk * s->fpb;
      if (skip) {
        u8* tmp = malloc(skip * 8);
        if (tmp && !fseek(s->f, s->base + frame_offset(s, blk * s->fpb, c), SEEK_SET) &&
            fread(tmp, 8, skip, s->f) == skip)
          adpcm_skip(tmp, skip, s->ch[c].coefs, &h1, &h2);
        free(tmp);
      }
      s->ctx[c][slot].index = dst[0];
      s->ctx[c][slot].history0 = h1;
      s->ctx[c][slot].history1 = h2;
      w->adpcm_data = &s->ctx[c][slot];
    }
  }
  /* queue both channels back to back so they stay in step */
  for (int c = 0; c < s->h.channels; c++) ndspChnWaveBufAdd(BGM_CH0 + c, &s->wb[c][slot]);
  s->cursor = end;
  if (s->cursor >= s->nframes) s->ended = true;
  s->next = (slot + 1) % BGM_BUFS;
  return true;
}

static void bgm_refill(bgm_stream* s) {
  if (!s->active) return;
  for (int i = 0; i < BGM_BUFS; i++) {
    ndspWaveBuf* w = &s->wb[0][s->next];
    if (w->status != NDSP_WBUF_DONE && w->status != NDSP_WBUF_FREE) break;
    if (!bgm_fill(s)) break;
  }
}

static void bgm_thread(void* arg) {
  (void)arg;
  while (!g_bgm_quit) {
    LightEvent_Wait(&g_bgm_event);
    LightLock_Lock(&g_bgm_lock);
    bgm_refill(&g_bgm);
    LightLock_Unlock(&g_bgm_lock);
  }
}

static void ndsp_cb(void* arg) {
  (void)arg;
  LightEvent_Signal(&g_bgm_event);
}

static void bgm_release(bgm_stream* s) {
  for (int c = 0; c < 2; c++) ndspChnWaveBufClear(BGM_CH0 + c);
  if (s->f) fclose(s->f);
  free(s->seek);
  if (s->mem) linearFree(s->mem);
  memset(s, 0, sizeof *s);
}

static void bgm_mix(void) {
  float l[12] = {0}, r[12] = {0};
  if (g_bgm.h.channels == 1) {
    l[0] = l[1] = g_bgm_vol;
  } else {
    l[0] = g_bgm_vol;
    r[1] = g_bgm_vol;
  }
  ndspChnSetMix(BGM_CH0, l);
  ndspChnSetMix(BGM_CH0 + 1, r);
}

bool snd_bgm_play_from(const char* key, bool loop, float loop_seconds) {
  if (!g_ready) return false;
  pak_entry e;
  if (!pak_find(NULL, key, &e) || e.codec != 0 || e.type != PAK_SND) return false;
  LightLock_Lock(&g_bgm_lock);
  bgm_release(&g_bgm);
  bgm_stream* s = &g_bgm;
  bool ok = false;
  s->f = pak_fopen(e.pak);
  s->base = e.offset;
  if (s->f && !fseek(s->f, e.offset, SEEK_SET) && fread(&s->h, sizeof s->h, 1, s->f) == 1 &&
      !memcmp(s->h.magic, "RSN1", 4) && s->h.encoding == 2 && s->h.channels >= 1 && s->h.channels <= 2 &&
      fread(s->ch, sizeof(rsnd_chan), s->h.channels, s->f) == s->h.channels) {
    size_t seek_n = (size_t)s->h.blocks * s->h.channels * 2;
    s->seek = malloc(seek_n * 2);
    s->mem = linearAlloc((size_t)s->h.block_bytes * BGM_BUFS * 2);
    if (s->seek && s->mem && fread(s->seek, 2, seek_n, s->f) == seek_n) ok = true;
  }
  if (!ok) {
    bgm_release(s);
    LightLock_Unlock(&g_bgm_lock);
    return false;
  }
  s->fpb = s->h.block_samples / 14;
  s->nframes = (s->h.samples + 13) / 14;
  u32 loop_sample = loop_seconds >= 0 ? (u32)(loop_seconds * s->h.rate + 0.5f)
                                      : (s->h.loop_start == 0xFFFFFFFFu ? 0 : s->h.loop_start);
  s->loop_frame = (loop_sample + 7) / 14;
  if (s->loop_frame >= s->nframes) s->loop_frame = 0;
  s->loop = loop;
  for (int c = 0; c < s->h.channels; c++) {
    int ch = BGM_CH0 + c;
    ndspChnReset(ch);
    ndspChnSetInterp(ch, NDSP_INTERP_POLYPHASE);
    ndspChnSetRate(ch, (float)s->h.rate);
    ndspChnSetFormat(ch, NDSP_FORMAT_MONO_ADPCM);
    ndspChnSetAdpcmCoefs(ch, (u16*)s->ch[c].coefs);
    ndspChnSetPaused(ch, true);
  }
  bgm_mix();
  s->active = true;
  s->next = 0;
  for (int i = 0; i < BGM_BUFS; i++)
    if (!bgm_fill(s)) break;
  for (int c = 0; c < s->h.channels; c++) ndspChnSetPaused(BGM_CH0 + c, false);
  LightLock_Unlock(&g_bgm_lock);
  return true;
}

bool snd_bgm_play(const char* key, bool loop) { return snd_bgm_play_from(key, loop, -1.0f); }

void snd_bgm_stop(void) {
  if (!g_ready) return;
  LightLock_Lock(&g_bgm_lock);
  bgm_release(&g_bgm);
  LightLock_Unlock(&g_bgm_lock);
}

bool snd_bgm_playing(void) { return g_ready && g_bgm.active && ndspChnIsPlaying(BGM_CH0); }

void snd_bgm_volume(float v) {
  g_bgm_vol = v;
  if (!g_ready) return;
  LightLock_Lock(&g_bgm_lock);
  bgm_mix();
  LightLock_Unlock(&g_bgm_lock);
}

/* ------------------------------------------------------------------ */
/* sound effects                                                        */

typedef struct {
  u64 hash;
  u8* mem; /* linear: channel data, channel 0 then 1 */
  u32 bytes, samples, rate, per_chan;
  u8 channels;
  rsnd_chan ch[2];
  u32 last_used;
  int playing; /* voices using it */
} sfx_entry;

typedef struct {
  ndspWaveBuf wb;
  ndspAdpcmData ctx;
  sfx_entry* src;
} voice;

static sfx_entry g_cache[SFX_CACHE];
static voice g_voices[SFX_VOICES];
static u32 g_tick, g_cache_bytes;

static void voices_reap(void) {
  for (int v = 0; v < SFX_VOICES; v++) {
    voice* vo = &g_voices[v];
    if (vo->src && !ndspChnIsPlaying(SFX_CH0 + v) &&
        (vo->wb.status == NDSP_WBUF_DONE || vo->wb.status == NDSP_WBUF_FREE)) {
      vo->src->playing--;
      vo->src = NULL;
    }
  }
}

static sfx_entry* sfx_get(const char* key) {
  u64 h = pak_hash(key);
  for (int i = 0; i < SFX_CACHE; i++)
    if (g_cache[i].mem && g_cache[i].hash == h) {
      g_cache[i].last_used = ++g_tick;
      return &g_cache[i];
    }
  size_t len;
  u8* d = pak_read(key, &len);
  if (!d) return NULL;
  rsnd_header hd;
  sfx_entry* e = NULL;
  if (len >= sizeof hd) memcpy(&hd, d, sizeof hd);
  if (len < sizeof hd || memcmp(hd.magic, "RSN1", 4) || hd.encoding != 2 || hd.channels < 1 || hd.channels > 2 ||
      hd.blocks != 1 || hd.data_offset > len) {
    free(d);
    return NULL;
  }
  u32 bytes = (u32)len - hd.data_offset;
  voices_reap();
  /* evict LRU entries not playing until there is a free slot and room */
  for (;;) {
    int free_slot = -1, lru = -1;
    for (int i = 0; i < SFX_CACHE; i++) {
      if (!g_cache[i].mem) {
        if (free_slot < 0) free_slot = i;
      } else if (!g_cache[i].playing && (lru < 0 || g_cache[i].last_used < g_cache[lru].last_used)) {
        lru = i;
      }
    }
    if (free_slot >= 0 && g_cache_bytes + bytes <= SFX_CACHE_BYTES) {
      e = &g_cache[free_slot];
      break;
    }
    if (lru < 0) {
      if (free_slot >= 0) e = &g_cache[free_slot]; /* over budget but nothing evictable */
      break;
    }
    g_cache_bytes -= g_cache[lru].bytes;
    linearFree(g_cache[lru].mem);
    memset(&g_cache[lru], 0, sizeof g_cache[lru]);
  }
  if (!e || !(e->mem = linearAlloc(bytes))) {
    free(d);
    return NULL;
  }
  memcpy(e->mem, d + hd.data_offset, bytes);
  DSP_FlushDataCache(e->mem, bytes);
  memcpy(e->ch, d + sizeof hd, sizeof(rsnd_chan) * hd.channels);
  free(d);
  e->hash = h;
  e->bytes = bytes;
  e->samples = hd.samples;
  e->rate = hd.rate;
  e->channels = hd.channels;
  e->per_chan = (hd.samples + 13) / 14 * 8;
  e->last_used = ++g_tick;
  g_cache_bytes += bytes;
  return e;
}

bool snd_sfx_preload(const char* key) { return g_ready && sfx_get(key) != NULL; }

bool snd_sfx_play(const char* key) {
  if (!g_ready) return false;
  sfx_entry* e = sfx_get(key);
  if (!e) return false;
  voices_reap();
  int vs[2], n = 0;
  for (int v = 0; v < SFX_VOICES && n < e->channels; v++)
    if (!g_voices[v].src) vs[n++] = v;
  if (n < e->channels) return false; /* all voices busy */
  for (int c = 0; c < e->channels; c++) {
    int ch = SFX_CH0 + vs[c];
    voice* vo = &g_voices[vs[c]];
    ndspChnReset(ch);
    ndspChnSetInterp(ch, NDSP_INTERP_LINEAR);
    ndspChnSetRate(ch, (float)e->rate);
    ndspChnSetFormat(ch, NDSP_FORMAT_MONO_ADPCM);
    ndspChnSetAdpcmCoefs(ch, (u16*)e->ch[c].coefs);
    float mix[12] = {0};
    if (e->channels == 1)
      mix[0] = mix[1] = g_sfx_vol;
    else
      mix[c] = g_sfx_vol;
    ndspChnSetMix(ch, mix);
    memset(&vo->wb, 0, sizeof vo->wb);
    vo->ctx.index = e->mem[c * e->per_chan];
    vo->ctx.history0 = vo->ctx.history1 = 0;
    vo->wb.data_adpcm = e->mem + c * e->per_chan;
    vo->wb.nsamples = e->samples;
    vo->wb.adpcm_data = &vo->ctx;
    vo->src = e;
    e->playing++;
  }
  for (int c = 0; c < e->channels; c++) ndspChnWaveBufAdd(SFX_CH0 + vs[c], &g_voices[vs[c]].wb);
  return true;
}

void snd_sfx_volume(float v) { g_sfx_vol = v; }

void snd_update(void) {
  if (g_ready) voices_reap();
}

/* ------------------------------------------------------------------ */

bool snd_init(void) {
  if (g_ready) return true;
  if (R_FAILED(ndspInit())) return false;
  ndspSetOutputMode(NDSP_OUTPUT_STEREO);
  LightLock_Init(&g_bgm_lock);
  LightEvent_Init(&g_bgm_event, RESET_ONESHOT);
  g_bgm_quit = false;
  s32 prio = 0x30;
  svcGetThreadPriority(&prio, CUR_THREAD_HANDLE);
  g_bgm_thread = threadCreate(bgm_thread, NULL, 16 * 1024, prio > 0x19 ? prio - 1 : 0x18, -2, false);
  if (!g_bgm_thread) {
    ndspExit();
    return false;
  }
  ndspSetCallback(ndsp_cb, NULL);
  g_ready = true;
  return true;
}

bool snd_ready(void) { return g_ready; }

void snd_exit(void) {
  if (!g_ready) return;
  ndspSetCallback(NULL, NULL);
  g_bgm_quit = true;
  LightEvent_Signal(&g_bgm_event);
  threadJoin(g_bgm_thread, U64_MAX);
  threadFree(g_bgm_thread);
  snd_bgm_stop();
  for (int v = 0; v < SFX_VOICES; v++) ndspChnWaveBufClear(SFX_CH0 + v);
  for (int i = 0; i < SFX_CACHE; i++)
    if (g_cache[i].mem) linearFree(g_cache[i].mem);
  memset(g_cache, 0, sizeof g_cache);
  memset(g_voices, 0, sizeof g_voices);
  g_cache_bytes = 0;
  ndspExit();
  g_ready = false;
}
