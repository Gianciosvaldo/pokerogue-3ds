/*
 * lr-run: minimal headless libretro frontend used to test the .3dsx in
 * Azahar's libretro core (software renderer, Old 3DS mode) and grab screenshots.
 *
 *   lr-run CORE.so GAME.3dsx DATA_DIR [frames=N] [press=FRAME:KEY[:HOLD]]... [shot=FRAME:out.png]...
 *          [wav=out.wav] [until=PATH] [shotlast=out.png]
 *
 * wav=      records the core's audio output (stereo s16) to a WAV file.
 * until=    stops early (checked every 30 frames) once PATH exists, e.g. a
 *           done-marker the app writes to the emulated SD card.
 * shotlast= screenshot of the last frame run.
 * KEY: a b x y up down left right start select l r
 * DATA_DIR is used as the system + save directory; the emulated SD card is
 * DATA_DIR/Azahar/sdmc/.
 */
#include <dlfcn.h>
#include <png.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "libretro.h"

static const char* g_dir;
static unsigned g_w, g_h;
static unsigned char* g_frame; /* XRGB8888 */
static size_t g_pitch;
static long g_cur;

typedef struct { long at; unsigned id; long hold; } press;
static press g_press[256];
static int g_npress;

static struct { const char* k; const char* v; } g_opts[] = {
    {"citra_graphics_api", "Software"},
    {"citra_is_new_3ds", "Old 3DS"},
    {"citra_use_cpu_jit", "enabled"},
    {"citra_cpu_clock_percentage", "100"},
    {"citra_layout_option", "default"},
    {"citra_use_virtual_sd", "enabled"},
    {"citra_use_libretro_save_path", "LibRetro Default"},
    {"citra_audio_emulation", "hle"},
    {"citra_region_value", "Auto"},
    {"citra_language", "English"},
};

static void log_cb(enum retro_log_level lvl, const char* fmt, ...) {
  if (lvl < RETRO_LOG_WARN && !getenv("LR_VERBOSE")) return;
  va_list ap;
  va_start(ap, fmt);
  fprintf(stderr, "[core] ");
  vfprintf(stderr, fmt, ap);
  va_end(ap);
}

static bool env_cb(unsigned cmd, void* data) {
  switch (cmd) {
    case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
    case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:
      *(const char**)data = g_dir;
      return true;
    case RETRO_ENVIRONMENT_GET_LOG_INTERFACE:
      ((struct retro_log_callback*)data)->log = log_cb;
      return true;
    case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:
      return *(enum retro_pixel_format*)data == RETRO_PIXEL_FORMAT_XRGB8888;
    case RETRO_ENVIRONMENT_GET_VARIABLE: {
      struct retro_variable* v = data;
      for (size_t i = 0; i < sizeof g_opts / sizeof *g_opts; i++)
        if (!strcmp(v->key, g_opts[i].k)) {
          v->value = g_opts[i].v;
          return true;
        }
      v->value = NULL;
      return false;
    }
    case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE:
      *(bool*)data = false;
      return true;
    case RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION:
      *(unsigned*)data = 0; /* force legacy GET_VARIABLE path */
      return true;
    case RETRO_ENVIRONMENT_SET_VARIABLES:
    case RETRO_ENVIRONMENT_SET_CORE_OPTIONS:
    case RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME:
    case RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS:
    case RETRO_ENVIRONMENT_SET_CONTROLLER_INFO:
    case RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO:
    case RETRO_ENVIRONMENT_SET_GEOMETRY:
      return true;
    case RETRO_ENVIRONMENT_GET_CAN_DUPE:
      *(bool*)data = true;
      return true;
    default:
      return false;
  }
}

static void video_cb(const void* data, unsigned w, unsigned h, size_t pitch) {
  if (!data) return;
  if (w * 4 * h > g_w * 4 * g_h || !g_frame) g_frame = realloc(g_frame, (size_t)w * 4 * h);
  g_w = w;
  g_h = h;
  g_pitch = w * 4;
  for (unsigned y = 0; y < h; y++) memcpy(g_frame + y * g_pitch, (const char*)data + y * pitch, w * 4);
}

static FILE* g_wav;
static unsigned long g_wav_frames;
static double g_sample_rate = 32728.0;
static void audio_cb(int16_t l, int16_t r) {
  if (g_wav) {
    int16_t s[2] = {l, r};
    fwrite(s, 2, 2, g_wav);
    g_wav_frames++;
  }
}
static size_t audio_batch_cb(const int16_t* d, size_t n) {
  if (g_wav) {
    fwrite(d, 4, n, g_wav);
    g_wav_frames += n;
  }
  return n;
}
static void wav_header(FILE* f, unsigned long frames, unsigned rate) {
  unsigned long data = frames * 4;
  unsigned char h[44] = "RIFF____WAVEfmt ";
  #define P32(o, v) (h[o] = (v) & 255, h[o + 1] = ((v) >> 8) & 255, h[o + 2] = ((v) >> 16) & 255, h[o + 3] = ((v) >> 24) & 255)
  P32(4, 36 + data);
  P32(16, 16);
  h[20] = 1; h[21] = 0; h[22] = 2; h[23] = 0;
  P32(24, rate);
  P32(28, rate * 4);
  h[32] = 4; h[33] = 0; h[34] = 16; h[35] = 0;
  memcpy(h + 36, "data", 4);
  P32(40, data);
  #undef P32
  fseek(f, 0, SEEK_SET);
  fwrite(h, 1, 44, f);
}
static void poll_cb(void) {}
static int16_t input_cb(unsigned port, unsigned dev, unsigned idx, unsigned id) {
  if (port || dev != RETRO_DEVICE_JOYPAD) return 0;
  (void)idx;
  if (id == RETRO_DEVICE_ID_JOYPAD_MASK) {
    int16_t m = 0;
    for (int i = 0; i < g_npress; i++)
      if (g_cur >= g_press[i].at && g_cur < g_press[i].at + g_press[i].hold) m |= 1 << g_press[i].id;
    return m;
  }
  for (int i = 0; i < g_npress; i++)
    if (g_press[i].id == id && g_cur >= g_press[i].at && g_cur < g_press[i].at + g_press[i].hold) return 1;
  return 0;
}

static unsigned key_id(const char* k) {
  static const struct { const char* n; unsigned id; } m[] = {
      {"a", RETRO_DEVICE_ID_JOYPAD_A},         {"b", RETRO_DEVICE_ID_JOYPAD_B},
      {"x", RETRO_DEVICE_ID_JOYPAD_X},         {"y", RETRO_DEVICE_ID_JOYPAD_Y},
      {"up", RETRO_DEVICE_ID_JOYPAD_UP},       {"down", RETRO_DEVICE_ID_JOYPAD_DOWN},
      {"left", RETRO_DEVICE_ID_JOYPAD_LEFT},   {"right", RETRO_DEVICE_ID_JOYPAD_RIGHT},
      {"start", RETRO_DEVICE_ID_JOYPAD_START}, {"select", RETRO_DEVICE_ID_JOYPAD_SELECT},
      {"l", RETRO_DEVICE_ID_JOYPAD_L},         {"r", RETRO_DEVICE_ID_JOYPAD_R},
  };
  for (size_t i = 0; i < sizeof m / sizeof *m; i++)
    if (!strcmp(k, m[i].n)) return m[i].id;
  fprintf(stderr, "unknown key %s\n", k);
  exit(2);
}

static void write_png(const char* path) {
  if (!g_frame) {
    fprintf(stderr, "no frame yet for %s\n", path);
    return;
  }
  png_image img;
  memset(&img, 0, sizeof img);
  img.version = PNG_IMAGE_VERSION;
  img.width = g_w;
  img.height = g_h;
  img.format = PNG_FORMAT_BGRA; /* XRGB8888 little-endian = B,G,R,X in memory */
  for (size_t i = 0; i < (size_t)g_w * g_h; i++) g_frame[i * 4 + 3] = 255;
  if (!png_image_write_to_file(&img, path, 0, g_frame, (png_int_32)g_pitch, NULL))
    fprintf(stderr, "png write failed: %s\n", img.message);
  else
    printf("frame %ld -> %s (%ux%u)\n", g_cur, path, g_w, g_h);
}

#define LOAD(sym) \
  typeof(&sym) p_##sym = (typeof(&sym))dlsym(core, #sym); \
  if (!p_##sym) { fprintf(stderr, "missing %s\n", #sym); return 1; }

int main(int argc, char** argv) {
  if (argc < 4) {
    fprintf(stderr, "usage: %s CORE GAME DIR [frames=N] [press=F:KEY[:HOLD]] [shot=F:file.png]\n", argv[0]);
    return 2;
  }
  g_dir = argv[3];
  long frames = 600;
  struct { long at; const char* path; } shots[64];
  int nshots = 0;
  const char* until = NULL;
  const char* shotlast = NULL;
  for (int i = 4; i < argc; i++) {
    if (!strncmp(argv[i], "frames=", 7)) {
      frames = atol(argv[i] + 7);
    } else if (!strncmp(argv[i], "press=", 6)) {
      char k[16] = {0};
      long at, hold = 6;
      if (sscanf(argv[i] + 6, "%ld:%15[a-z]:%ld", &at, k, &hold) < 2) return 2;
      g_press[g_npress++] = (press){at, key_id(k), hold};
    } else if (!strncmp(argv[i], "shot=", 5)) {
      char* c = strchr(argv[i] + 5, ':');
      shots[nshots].at = atol(argv[i] + 5);
      shots[nshots++].path = c + 1;
    } else if (!strncmp(argv[i], "wav=", 4)) {
      g_wav = fopen(argv[i] + 4, "wb+");
      if (g_wav) fseek(g_wav, 44, SEEK_SET);
    } else if (!strncmp(argv[i], "until=", 6)) {
      until = argv[i] + 6;
    } else if (!strncmp(argv[i], "shotlast=", 9)) {
      shotlast = argv[i] + 9;
    }
  }

  void* core = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
  if (!core) {
    fprintf(stderr, "dlopen: %s\n", dlerror());
    return 1;
  }
  LOAD(retro_set_environment);
  LOAD(retro_set_video_refresh);
  LOAD(retro_set_audio_sample);
  LOAD(retro_set_audio_sample_batch);
  LOAD(retro_set_input_poll);
  LOAD(retro_set_input_state);
  LOAD(retro_init);
  LOAD(retro_load_game);
  LOAD(retro_run);
  LOAD(retro_unload_game);
  LOAD(retro_deinit);

  p_retro_set_environment(env_cb);
  p_retro_init();
  p_retro_set_video_refresh(video_cb);
  p_retro_set_audio_sample(audio_cb);
  p_retro_set_audio_sample_batch(audio_batch_cb);
  p_retro_set_input_poll(poll_cb);
  p_retro_set_input_state(input_cb);

  struct retro_game_info gi = {argv[2], NULL, 0, NULL};
  if (!p_retro_load_game(&gi)) {
    fprintf(stderr, "retro_load_game failed\n");
    return 1;
  }
  struct retro_system_av_info av;
  typeof(&retro_get_system_av_info) p_av = (typeof(&retro_get_system_av_info))dlsym(core, "retro_get_system_av_info");
  if (p_av) {
    p_av(&av);
    g_sample_rate = av.timing.sample_rate;
  }
  struct timespec t0, t1;
  clock_gettime(CLOCK_MONOTONIC, &t0);
  for (g_cur = 0; g_cur < frames; g_cur++) {
    p_retro_run();
    for (int s = 0; s < nshots; s++)
      if (shots[s].at == g_cur) write_png(shots[s].path);
    if (until && g_cur % 30 == 0) {
      FILE* u = fopen(until, "rb");
      if (u) {
        fclose(u);
        printf("stop: %s exists at frame %ld\n", until, g_cur);
        g_cur++;
        break;
      }
    }
  }
  if (shotlast) write_png(shotlast);
  frames = g_cur;
  if (g_wav) {
    wav_header(g_wav, g_wav_frames, (unsigned)(g_sample_rate + 0.5));
    fclose(g_wav);
    printf("audio: %lu frames at %.1f Hz\n", g_wav_frames, g_sample_rate);
  }
  clock_gettime(CLOCK_MONOTONIC, &t1);
  printf("ran %ld frames in %.1f s\n", frames, (t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) / 1e9);
  p_retro_unload_game();
  p_retro_deinit();
  return 0;
}
