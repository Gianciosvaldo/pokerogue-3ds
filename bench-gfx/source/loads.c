/* SD card speed, loading from the packs, audio CPU cost and memory limits. */
#include <lz4.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "bench.h"
#include "preload_keys.h"
#include "save.h"

static u32 g_rng = 12345;
static u32 rnd(void) { return g_rng = g_rng * 1664525u + 1013904223u; }

static long file_size(const char* path) {
  FILE* f = fopen(path, "rb");
  if (!f) return -1;
  fseek(f, 0, SEEK_END);
  long n = ftell(f);
  fclose(f);
  return n;
}

/* ------------------------------------------------------------------ SD card */

#define TMP_PATH "sdmc:/3ds/pokerogue-gfx-bench.tmp"
#define MB_ (1024 * 1024)

void sd_tests(void) {
  res("\n== SD card ==");
  u8* buf = malloc(MB_);
  if (!buf) {
    res("no memory for the SD test");
    return;
  }
  memset(buf, 0x5A, MB_);

  /* write speed: a 4 MB file in 256 KB writes, closed (the close is part of it) */
  status("SD card: write speed");
  long need = 24L * MB_;
  static const char* PAKS[] = {PAK_DIR "bgm.pak", PAK_DIR "gfx.pak", PAK_DIR "snd.pak"};
  const char* path = NULL;
  long size = 0;
  for (int i = 0; i < 3 && !path; i++)
    if ((size = file_size(PAKS[i])) >= need) path = PAKS[i];
  long wsize = path ? 4L * MB_ : need;
  u64 t0 = now_ticks();
  FILE* w = fopen(TMP_PATH, "wb");
  bool wok = w != NULL;
  for (long done = 0; wok && done < wsize; done += 256 * 1024) wok = fwrite(buf, 1, 256 * 1024, w) == 256 * 1024;
  if (w) wok = fclose(w) == 0 && wok;
  double wms = ms_since(t0);
  if (wok)
    res("write %ld MB in 256 KB writes: %.1f MB/s", wsize / MB_, mb(wsize) / (wms / 1000));
  else
    res("write test FAILED (SD card full or read-only?)");
  if (!path) {
    path = wok ? TMP_PATH : NULL;
    size = wsize;
  }
  if (!path) {
    free(buf);
    return;
  }
  const char* base = strrchr(path, '/') + 1;
  bool tmp = !strcmp(path, TMP_PATH);
  res("reads from %s (%.0f MB)%s", base, mb(size), tmp ? ", just written: the packs are not on the SD card" : "");

  FILE* f = fopen(path, "rb");
  if (!f) {
    res("cannot open %s", path);
    free(buf);
    return;
  }
  /* sequential reads, 4 MB each from a different part of the file */
  status("SD card: sequential reads");
  static const int CHUNK[] = {16 * 1024, 64 * 1024, 256 * 1024, MB_};
  char line[160];
  int len = snprintf(line, sizeof line, "sequential read, 4 MB each:");
  double first64 = 0;
  long region0 = 0;
  for (int c = 0; c < 4; c++) {
    long off = ((long)(c + 1) * 5 * MB_) % (size - 4L * MB_);
    if (c == 1) region0 = off;
    fseek(f, off, SEEK_SET);
    u64 s = now_ticks();
    for (long done = 0; done < 4L * MB_; done += CHUNK[c]) fread(buf, 1, CHUNK[c], f);
    double mbs = 4 / (ms_since(s) / 1000);
    if (c == 1) first64 = mbs;
    len += snprintf(line + len, sizeof line - len, " %d KB chunks %.1f MB/s%s", CHUNK[c] / 1024, mbs, c < 3 ? "," : "");
  }
  res("%s", line);
  fseek(f, region0, SEEK_SET);
  u64 s = now_ticks();
  for (long done = 0; done < 4L * MB_; done += 64 * 1024) fread(buf, 1, 64 * 1024, f);
  res("the same 4 MB again (64 KB chunks): %.1f MB/s (first time %.1f MB/s; much faster = something caches)",
      4 / (ms_since(s) / 1000), first64);

  /* random reads the size of typical pack entries */
  status("SD card: random reads");
  static const int RSZ[] = {4 * 1024, 32 * 1024, 256 * 1024};
  for (int k = 0; k < 3; k++) {
    float t[100];
    int n = k == 2 ? 40 : 100;
    for (int i = 0; i < n; i++) {
      long off = (long)(rnd() % (u32)(size - RSZ[k])) & ~3L;
      u64 a = now_ticks();
      fseek(f, off, SEEK_SET);
      fread(buf, 1, RSZ[k], f);
      t[i] = (float)ms_since(a);
    }
    dist d = dist_of(t, n);
    res("random %3d KB reads (seek + read), %3d of them: median %.2f ms, p95 %.2f, max %.2f", RSZ[k] / 1024, n,
        d.med, d.p95, d.max);
  }
  fclose(f);
  remove(TMP_PATH);

  /* a save the way the game will write one: temp file with CRC, .bak rotation, rename */
  status("SD card: save writes");
  if (save_init()) {
    float t[5];
    bool ok = true;
    for (int i = 0; i < 5; i++) {
      buf[0] = (u8)i;
      u64 a = now_ticks();
      ok = save_write("gfx-bench-test", buf, 64 * 1024) == SAVE_OK && ok;
      t[i] = (float)ms_since(a);
    }
    dist d = dist_of(t, 5);
    res("64 KB save (save_write: temp file + CRC + .bak + rename), 5 times: median %.1f ms, max %.1f ms%s", d.med,
        d.max, ok ? "" : " (some FAILED)");
    remove(SAVE_DIR "/gfx-bench-test.sav");
    remove(SAVE_DIR "/gfx-bench-test.bak");
    remove(SAVE_DIR "/gfx-bench-test.tmp");
  }
  free(buf);
  res_save();
}

/* ------------------------------------------------------------------ loading from the packs */

typedef struct {
  double read, lz4, build, total;
  u32 stored;
  size_t tex;
} lt;

#define MAX_PF 8
static struct {
  rr_pak* pak;
  FILE* f;
} g_pf[MAX_PF];

static FILE* file_for(rr_pak* p) {
  for (int i = 0; i < MAX_PF; i++) {
    if (g_pf[i].pak == p) return g_pf[i].f;
    if (!g_pf[i].pak) {
      g_pf[i].pak = p;
      g_pf[i].f = pak_fopen(p);
      return g_pf[i].f;
    }
  }
  return NULL;
}

static void close_files(void) {
  for (int i = 0; i < MAX_PF; i++)
    if (g_pf[i].f) fclose(g_pf[i].f);
  memset(g_pf, 0, sizeof g_pf);
}

static size_t atlas_bytes(const rr_atlas* a) {
  size_t n = 0;
  for (int i = 0; i < rr_atlas_pages(a); i++) n += rr_tex_bytes(rr_atlas_page(a, i));
  return n;
}

/* pak_read_entry + rr_atlas_load_rrt (what rr_atlas_load_key does), each step timed. */
static rr_atlas* load_split(const char* key, lt* r) {
  memset(r, 0, sizeof *r);
  pak_entry e;
  if (!pak_find(NULL, key, &e)) return NULL;
  FILE* f = file_for(e.pak);
  if (!f) return NULL;
  u64 t0 = now_ticks();
  char* raw = malloc(e.size + 1);
  char* st = e.codec ? malloc(e.stored) : raw;
  bool ok = raw && st && !fseek(f, e.offset, SEEK_SET) && fread(st, 1, e.stored, f) == e.stored;
  u64 t1 = now_ticks();
  if (ok && e.codec) ok = LZ4_decompress_safe(st, raw, (int)e.stored, (int)e.size) == (int)e.size;
  u64 t2 = now_ticks();
  rr_atlas* a = ok ? rr_atlas_load_rrt(raw, e.size, NULL, 0) : NULL;
  u64 t3 = now_ticks();
  if (st != raw) free(st);
  free(raw);
  r->read = ticks_ms(t1 - t0);
  r->lz4 = ticks_ms(t2 - t1);
  r->build = ticks_ms(t3 - t2);
  r->total = ticks_ms(t3 - t0);
  r->stored = e.stored;
  r->tex = a ? atlas_bytes(a) : 0;
  return a;
}

typedef struct {
  const char* key;
  const char* what;
} item;

static const item SHEETS[] = {
    {"images/pokemon/1", "front Bulbasaur"},
    {"images/pokemon/25", "front Pikachu"},
    {"images/pokemon/94", "front Gengar"},
    {"images/pokemon/149", "front Dragonite"},
    {"images/pokemon/255", "front Torchic (median sheet)"},
    {"images/pokemon/445", "front Garchomp"},
    {"images/pokemon/643", "front Reshiram"},
    {"images/pokemon/890", "front Eternatus"},
    {"images/pokemon/646-white", "front White Kyurem"},
    {"images/pokemon/642-therian", "front Thundurus-T"},
    {"images/pokemon/back/1", "back Bulbasaur"},
    {"images/pokemon/back/25", "back Pikachu"},
    {"images/pokemon/back/257", "back Blaziken"},
    {"images/pokemon/back/445", "back Garchomp"},
    {"images/pokemon/back/6", "back Charizard"},
    {"images/pokemon/back/642-therian", "back Thundurus-T (largest)"},
    {"images/arenas/grass_bg", "arena grass bg"},
    {"images/arenas/volcano_b_2", "arena volcano prop"},
    {"images/battle_anims/PRAS- Fire", "anim sheet PRAS- Fire"},
    {"images/battle_anims/GEN8- Fiery", "anim sheet GEN8- Fiery (largest)"},
    {"images/pokemon_icons_1", "Pokemon icon sheet 1"},
    {"images/items", "item icons (all)"},
};
#define N_SHEETS ((int)(sizeof SHEETS / sizeof SHEETS[0]))
#define N_SPECIES 16 /* the first 16 SHEETS are Pokemon */

static const char* CRIES[] = {"audio/cry/1", "audio/cry/25", "audio/cry/257", "audio/cry/445", "audio/cry/149", "audio/cry/6"};

static double load_set(const char* const* keys, int n, size_t* tex) {
  rr_atlas* held[8];
  lt r;
  double ms = 0;
  *tex = 0;
  for (int i = 0; i < n; i++) {
    held[i] = load_split(keys[i], &r);
    ms += r.total;
    *tex += r.tex;
  }
  for (int i = 0; i < n; i++) rr_atlas_free(held[i]);
  return ms;
}

void load_tests(void) {
  res("\n== loading from the packs ==");
  if (!g_pak[P_GFX]) {
    res("skipped: gfx.pak is not on the SD card (sdmc:/3ds/pokerogue/gfx.pak)");
    return;
  }
  res("ms per image: SD read, LZ4 unpack, texture build (palette to RGBA5551, into linear memory);");
  res("1st = first read after launch, 2nd = same again, API = rr_atlas_load_key (the app's call)");
  res("%-34s %6s %5s %6s %7s | %6s %6s | %8s %6s", "image", "read", "LZ4", "build", "1st", "2nd", "API", "texture",
      "on SD");
  float firsts[N_SHEETS];
  int nsp = 0;
  double front445 = -1, back257 = -1;
  for (int i = 0; i < N_SHEETS && !g_stop; i++) {
    if (!status("loading: %s", SHEETS[i].what)) break;
    lt a, b;
    rr_atlas* x = load_split(SHEETS[i].key, &a);
    if (!x) {
      res("%-34s missing from gfx.pak", SHEETS[i].what);
      continue;
    }
    rr_atlas_free(x);
    rr_atlas_free(load_split(SHEETS[i].key, &b));
    u64 t0 = now_ticks();
    rr_atlas_free(rr_atlas_load_key(SHEETS[i].key));
    double api = ms_since(t0);
    res("%-34s %6.1f %5.1f %6.1f %7.1f | %6.1f %6.1f | %5.0f KB %3.0f KB", SHEETS[i].what, a.read, a.lz4, a.build,
        a.total, b.total, api, a.tex / 1024.0, a.stored / 1024.0);
    if (i < N_SPECIES) firsts[nsp++] = (float)a.total;
    if (!strcmp(SHEETS[i].key, "images/pokemon/445")) front445 = a.total;
    if (!strcmp(SHEETS[i].key, "images/pokemon/back/257")) back257 = a.total;
  }
  if (nsp) {
    dist d = dist_of(firsts, nsp);
    res("Pokemon sheets above (first load): median %.1f ms, max %.1f ms", d.med, d.max);
  }
  if (g_stop) return;

  /* shiny: the same sheet recoloured through upstream's variant colour map */
  {
    u64 t0 = now_ticks();
    rr_atlas* v = rr_atlas_load_key_variant("images/pokemon/445", "images/pokemon/variant/445.json", 1);
    double ms = ms_since(t0);
    res("shiny Garchomp (sheet + variant colour map, rr_atlas_load_key_variant): %s %.1f ms", v ? "" : "FAILED", ms);
    rr_atlas_free(v);
  }

  /* a biome change: the arena background, both bases and the props */
  static const char* GRASS[] = {"images/arenas/grass_bg", "images/arenas/grass_a", "images/arenas/grass_b"};
  static const char* VOLCANO[] = {"images/arenas/volcano_bg", "images/arenas/volcano_a", "images/arenas/volcano_b",
                                  "images/arenas/volcano_b_1", "images/arenas/volcano_b_2", "images/arenas/volcano_b_3"};
  size_t tg, tv;
  double mg = load_set(GRASS, 3, &tg), mv = load_set(VOLCANO, 6, &tv);
  res("biome change: grass (bg + 2 bases) %.1f ms, %.2f MB; volcano (bg + 2 bases + 3 props) %.1f ms, %.2f MB", mg,
      mb(tg), mv, mb(tv));

  /* fonts: open, then the first page a string touches */
  if (g_pak[P_FONT]) {
    mem_snap m0 = mem_now();
    u64 t0 = now_ticks();
    rr_font* f = rr_font_load_key("fonts/emerald@16");
    double open = ms_since(t0);
    t0 = now_ticks();
    rr_text_width(f, "Garchomp used Dragon Claw!");
    double latin = ms_since(t0);
    mem_snap m1 = mem_now();
    t0 = now_ticks();
    /* Japanese: kana and kanji live in other pages */
    rr_text_width(f, "\xe3\x82\xac\xe3\x83\x96\xe3\x83\xaa\xe3\x82\xa2\xe3\x82\xb9\xe3\x81\xae\xe3\x83\x89"
                     "\xe3\x83\xa9\xe3\x82\xb4\xe3\x83\xb3\xe3\x82\xaf\xe3\x83\xad\xe3\x83\xbc\xef\xbc\x81"
                     "\xe5\x8a\xb9\xe6\x9e\x9c\xe3\x81\xaf\xe6\x8a\x9c\xe7\xbe\xa4\xe3\x81\xa0");
    double jp = ms_since(t0);
    mem_snap m2 = mem_now();
    res("font emerald 16 px: open %.1f ms; first English line %.1f ms (%d page, %.0f KB); a Japanese line %.1f ms "
        "(%d more pages, %.0f KB)",
        open, latin, m1.tex_count - m0.tex_count, (m1.tex_bytes - m0.tex_bytes) / 1024.0, jp,
        m2.tex_count - m1.tex_count, (m2.tex_bytes - m1.tex_bytes) / 1024.0);
    rr_font_free(f);
  } else {
    res("fonts: skipped (font.pak missing)");
  }

  /* cries: first play loads from snd.pak into linear memory; later plays are cached */
  if (g_pak[P_SND]) {
    float first[6], again[6];
    int n = 0;
    for (int i = 0; i < 6; i++) {
      u64 t0 = now_ticks();
      bool ok;
      if (g_sound) {
        ok = snd_sfx_preload(CRIES[i]);
      } else {
        size_t len;
        void* d = pak_read(CRIES[i], &len);
        ok = d != NULL;
        free(d);
      }
      double a = ms_since(t0);
      t0 = now_ticks();
      if (g_sound) snd_sfx_play(CRIES[i]);
      double b = ms_since(t0);
      if (ok) {
        first[n] = (float)a;
        again[n++] = (float)b;
      }
    }
    dist d1 = dist_of(first, n), d2 = dist_of(again, n);
    if (g_sound)
      res("cries (%d): first play (load) median %.1f ms, max %.1f; cached play median %.2f ms", n, d1.med, d1.max,
          d2.med);
    else
      res("cries (%d), read + unpack only (sound off): median %.1f ms, max %.1f", n, d1.med, d1.max);
    if (front445 >= 0)
      res("=> a new wild Garchomp appears (front sheet + cry): about %.0f ms; Blaziken switches in (back + cry): "
          "about %.0f ms (sums of the first loads above)",
          front445 + d1.med, back257 + d1.med);
  } else {
    res("cries: skipped (snd.pak missing)");
  }

  /* music */
  if (g_sound && g_pak[P_BGM]) {
    status("loading: music start");
    u64 t0 = now_ticks();
    bool ok = snd_bgm_play("audio/bgm/battle_wild", true);
    double queued = ms_since(t0);
    double first = -1;
    while (ok && ms_since(t0) < 2000) {
      if (ndspChnGetSamplePos(0) > 0) {
        first = ms_since(t0);
        break;
      }
      svcSleepThread(500000);
    }
    svcSleepThread(1500000000LL);
    t0 = now_ticks();
    bool ok2 = snd_bgm_play("audio/bgm/grass", true);
    double sw = ms_since(t0);
    res("music: start %s %.1f ms to queue, first sample playing after %.0f ms; switching track %s %.1f ms",
        ok ? "" : "FAILED", queued, first, ok2 ? "" : "FAILED", sw);
    /* does streaming slow down a big load? */
    lt a;
    rr_atlas_free(load_split("images/pokemon/back/642-therian", &a));
    snd_bgm_stop();
    lt b;
    rr_atlas_free(load_split("images/pokemon/back/642-therian", &b));
    res("largest sheet while music streams: %.1f ms, silent: %.1f ms", a.total, b.total);
  } else {
    res("music: skipped (%s)", !g_pak[P_BGM] ? "bgm.pak missing" : "sound off");
  }
  close_files();
  res_save();
}

/* ------------------------------------------------------------------ audio CPU */

static double spin_rate(int ms) {
  volatile u32 acc = 1;
  u32 n = 0;
  u64 end = now_ticks() + (u64)ms * (SYSCLOCK_ARM11 / 1000);
  while (now_ticks() < end) {
    for (int i = 0; i < 2000; i++) acc = acc * 1664525u + 1013904223u;
    n++;
  }
  return n / (double)ms;
}

static double spin_med(int ms) {
  float r[3];
  for (int i = 0; i < 3; i++) r[i] = (float)spin_rate(ms);
  return dist_of(r, 3).med;
}

void audio_cpu_test(void) {
  res("\n== audio CPU cost ==");
  if (!g_sound || !g_pak[P_BGM] || !g_pak[P_SND]) {
    res("skipped (%s)", !g_sound ? "sound off" : "bgm.pak or snd.pak missing");
    return;
  }
  status("audio: CPU left while music and sound effects play");
  double base = spin_med(600);
  snd_bgm_play("audio/bgm/battle_wild", true);
  svcSleepThread(300000000LL);
  double music = spin_med(600);
  static const char* SFX[] = {"audio/cry/445", "audio/cry/257", "audio/cry/25", "audio/se/hit",
                              "audio/battle_anims/PRSFX- Flamethrower", "audio/cry/1", "audio/cry/149", "audio/cry/6"};
  float r[3];
  for (int k = 0; k < 3; k++) {
    for (int i = 0; i < 8; i++) snd_sfx_play(SFX[i]);
    r[k] = (float)spin_rate(600);
    snd_update();
  }
  double both = dist_of(r, 3).med;
  snd_bgm_stop();
  res("CPU left for the game (a busy loop's speed vs silence): music streaming %.1f%%, music + 8 sound effects "
      "%.1f%%",
      100 * music / base, 100 * both / base);
  res_save();
}

/* ------------------------------------------------------------------ memory */

#define MAX_HELD 3000

static bool is_front_key(const char* k) {
  const char* p = "images/pokemon/";
  size_t n = strlen(p);
  return !strncmp(k, p, n) && !strchr(k + n, '/') && k[n] >= '0' && k[n] <= '9';
}

void memory_tests(void) {
  res("\n== memory ==");
  rr_atlas** held = calloc(MAX_HELD, sizeof *held);
  if (!held) return;
  int nh = 0;
  mem_snap m0 = mem_now();
  res("before: linear heap %.1f MB free, %d textures (%.2f MB) still loaded", mb(m0.linear_free), m0.tex_count,
      mb(m0.tex_bytes));

  /* 1. what upstream keeps loaded from boot */
  if (g_pak[P_GFX]) {
    int ok = 0, missing = 0, failed = 0;
    u64 t0 = now_ticks();
    for (int i = 0; i < N_PRELOAD_KEYS && !g_stop; i++) {
      if (i % 20 == 0 && !status("memory: upstream-style preload %d/%d", i, N_PRELOAD_KEYS)) break;
      pak_entry e;
      if (!pak_find(NULL, PRELOAD_KEYS[i], &e)) {
        missing++;
        continue;
      }
      rr_atlas* a = rr_atlas_load_key(PRELOAD_KEYS[i]);
      if (a && nh < MAX_HELD)
        held[nh++] = a, ok++;
      else
        failed++, rr_atlas_free(a);
    }
    double s = ms_since(t0) / 1000;
    mem_snap m1 = mem_now();
    res("upstream-style preload (every image upstream's loading scene loads at boot, English):");
    res("  %d of %d loaded (%d not in gfx.pak, %d FAILED to load), %.1f MB of textures in %.1f s", ok,
        N_PRELOAD_KEYS, missing, failed, mb(m1.tex_bytes - m0.tex_bytes), s);
    res("  linear heap left: %.1f MB", mb(m1.linear_free));
    for (int i = 0; i < nh; i++) rr_atlas_free(held[i]);
    nh = 0;
  }
  if (g_stop) {
    free(held);
    return;
  }

  /* 2. from an empty heap: how many Pokemon sheets fit (an on-demand cache's ceiling) */
  mem_snap m2 = mem_now();
  u64 t0 = now_ticks();
  size_t bytes = 0;
  bool real = false;
  const char* why = "stopped";
  if (g_pak[P_GFX]) {
    rr_pak* p = pak_open(PAK_DIR "gfx.pak");
    size_t nlen;
    char* names = p ? pak_load_names(p, &nlen) : NULL;
    if (names) {
      real = true;
      const char* k = names;
      why = "every front sheet loaded";
      for (int i = 0; i < pak_count(p) && nh < MAX_HELD; i++, k += strlen(k) + 1) {
        if (!is_front_key(k)) continue;
        if (nh % 25 == 0 && !status("memory: filling the heap, %d sheets", nh)) {
          why = "stopped";
          break;
        }
        rr_atlas* a = rr_atlas_load_key(k);
        if (!a) {
          why = "a load failed";
          break;
        }
        bytes += atlas_bytes(a);
        held[nh++] = a;
      }
      if (nh == MAX_HELD) why = "bench limit";
    }
    free(names);
    pak_close(p);
  }
  if (!real) { /* stand-ins the size of a median Pokemon sheet (64 KB of texture) */
    for (; nh < MAX_HELD; nh++) {
      if (nh % 50 == 0 && !status("memory: filling the heap, %d sheets", nh)) break;
      rr_atlas* a = synth_atlas(128, 256, 40, 40, 32, nh);
      if (!a) {
        why = "a load failed";
        break;
      }
      bytes += atlas_bytes(a);
      held[nh] = a;
    }
  }
  double s = ms_since(t0) / 1000;
  mem_snap m3 = mem_now();
  res("filling the linear heap with %s from empty: %d sheets, %.1f MB of textures in %.1f s (%s)",
      real ? "Pokemon front sheets (pack order)" : "64 KB stand-in sheets", nh, mb(bytes), s, why);
  res("  linear heap left then: %.2f MB (started at %.1f MB)", mb(m3.linear_free), mb(m2.linear_free));
  if (g_sound) {
    bool sfx = snd_sfx_play("audio/ui/select");
    bool bgm = snd_bgm_play("audio/bgm/battle_wild", false);
    snd_bgm_stop();
    res("  with the heap that full: a new sound effect %s, music start %s", sfx ? "plays" : "FAILS",
        bgm ? "works" : "FAILS");
  }
  for (int i = 0; i < nh; i++) rr_atlas_free(held[i]);
  free(held);
  mem_snap m4 = mem_now();
  long leak = (long)m0.linear_free - (long)m4.linear_free;
  res("after freeing everything: linear heap %.2f MB free, %ld KB %s than before these tests (the sound cache",
      mb(m4.linear_free), labs(leak) / 1024, leak > 0 ? "less" : "more");
  res("  keeps up to 1 MB of recent sound effects; more than that missing would be a leak)");
  res_save();
}
