/*
 * title_save_ui_test.c: Host integration test and screenshot generator for
 * Card 113 (Title UI, Save Slots UI, Refusal Dialogs, Game Over, Clear Screen).
 */

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <zlib.h>

#include "clear_ui.h"
#include "game_over_ui.h"
#include "gc/run.h"
#include "gc/system.h"
#include "menus.h"
#include "mui.h"
#include "run_screen.h"
#include "save.h"
#include "save_slot_ui.h"
#include "title_ui.h"

void rs_on_end_screen_dismissed(void) {}

/* ------------------------------------------------------------------ Software Canvas & PNG Writer */

#define CANVAS_W 400
#define CANVAS_H 480

static uint32_t canvas[CANVAS_H][CANVAS_W];
static rr_screen current_target = RR_TOP;

static void write_be32(uint8_t* p, uint32_t val) {
  p[0] = (val >> 24) & 0xFF;
  p[1] = (val >> 16) & 0xFF;
  p[2] = (val >> 8) & 0xFF;
  p[3] = val & 0xFF;
}

static void write_chunk(FILE* f, const char* type, const uint8_t* data, size_t len) {
  uint32_t len_be = (uint32_t)len;
  uint8_t len_buf[4];
  write_be32(len_buf, len_be);
  fwrite(len_buf, 1, 4, f);
  fwrite(type, 1, 4, f);
  if (len > 0) fwrite(data, 1, len, f);

  uint32_t crc = crc32(0L, Z_NULL, 0);
  crc = crc32(crc, (const Bytef*)type, 4);
  if (len > 0) crc = crc32(crc, (const Bytef*)data, (uInt)len);

  uint8_t crc_buf[4];
  write_be32(crc_buf, crc);
  fwrite(crc_buf, 1, 4, f);
}

static void save_png(const char* filepath) {
  mkdir("/home/jules/verification", 0777);

  FILE* f = fopen(filepath, "wb");
  if (!f) {
    printf("Failed to open PNG output file: %s\n", filepath);
    return;
  }

  const uint8_t sig[8] = {137, 80, 78, 71, 13, 10, 26, 10};
  fwrite(sig, 1, 8, f);

  uint8_t ihdr[13];
  write_be32(&ihdr[0], CANVAS_W);
  write_be32(&ihdr[4], CANVAS_H);
  ihdr[8] = 8;
  ihdr[9] = 6;
  ihdr[10] = 0;
  ihdr[11] = 0;
  ihdr[12] = 0;
  write_chunk(f, "IHDR", ihdr, 13);

  size_t raw_size = CANVAS_H * (1 + CANVAS_W * 4);
  uint8_t* raw = malloc(raw_size);
  size_t pos = 0;

  for (int y = 0; y < CANVAS_H; y++) {
    raw[pos++] = 0;
    for (int x = 0; x < CANVAS_W; x++) {
      uint32_t c = canvas[y][x];
      raw[pos++] = (c >> 24) & 0xFF;
      raw[pos++] = (c >> 16) & 0xFF;
      raw[pos++] = (c >> 8) & 0xFF;
      raw[pos++] = c & 0xFF;
    }
  }

  uLongf comp_size = compressBound((uLong)raw_size);
  uint8_t* comp = malloc(comp_size);
  compress(comp, &comp_size, raw, (uLong)raw_size);

  write_chunk(f, "IDAT", comp, comp_size);
  write_chunk(f, "IEND", NULL, 0);

  fclose(f);
  free(raw);
  free(comp);
  printf("Saved dual-screen PNG screenshot: %s\n", filepath);
}

/* ------------------------------------------------------------------ Stubs for mui */

void mui_sfx(const char* key) { (void)key; }
const char* mui_t(const char* key) { return key; }
int mui_text_width(mui_style s, const char* utf8) { (void)s; return (int)strlen(utf8) * 8; }
bool mui_image(const char* key, const char* frame, float x, float y, float ox, float oy) { (void)key; (void)frame; (void)x; (void)y; (void)ox; (void)oy; return false; }

void rr_target(rr_screen s, rr_color clear) {
  current_target = s;
  int start_y = (s == RR_TOP) ? 0 : 240;
  int end_y = (s == RR_TOP) ? 240 : 480;
  int start_x = (s == RR_TOP) ? 0 : 40;
  int end_x = (s == RR_TOP) ? 400 : 360;

  for (int y = start_y; y < end_y; y++) {
    for (int x = start_x; x < end_x; x++) {
      canvas[y][x] = clear;
    }
  }
}

static void draw_pixel(int x, int y, uint32_t col) {
  int screen_offset_y = (current_target == RR_TOP) ? 0 : 240;
  int screen_offset_x = (current_target == RR_TOP) ? 0 : 40;

  int px = screen_offset_x + x;
  int py = screen_offset_y + y;

  if (px >= 0 && px < CANVAS_W && py >= 0 && py < CANVAS_H) {
    canvas[py][px] = col;
  }
}

void mui_window(float x, float y, float w, float h) {
  int ix = (int)x, iy = (int)y, iw = (int)w, ih = (int)h;
  uint32_t bg = 0x28283CFF;
  uint32_t border = 0x9090C0FF;

  for (int j = iy; j < iy + ih; j++) {
    for (int i = ix; i < ix + iw; i++) {
      if (i == ix || i == ix + iw - 1 || j == iy || j == iy + ih - 1) {
        draw_pixel(i, j, border);
      } else {
        draw_pixel(i, j, bg);
      }
    }
  }
}

static const uint8_t font5x7[128][7] = {
  ['A'] = {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11},
  ['B'] = {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E},
  ['C'] = {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E},
  ['D'] = {0x1C, 0x12, 0x11, 0x11, 0x11, 0x12, 0x1C},
  ['E'] = {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F},
  ['F'] = {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10},
  ['G'] = {0x0E, 0x11, 0x10, 0x13, 0x11, 0x11, 0x0F},
  ['H'] = {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11},
  ['I'] = {0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E},
  ['J'] = {0x07, 0x02, 0x02, 0x02, 0x02, 0x12, 0x0C},
  ['K'] = {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11},
  ['L'] = {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F},
  ['M'] = {0x11, 0x1B, 0x15, 0x11, 0x11, 0x11, 0x11},
  ['N'] = {0x11, 0x11, 0x19, 0x15, 0x13, 0x11, 0x11},
  ['O'] = {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E},
  ['P'] = {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10},
  ['Q'] = {0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D},
  ['R'] = {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11},
  ['S'] = {0x0E, 0x11, 0x10, 0x0E, 0x01, 0x11, 0x0E},
  ['T'] = {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04},
  ['U'] = {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E},
  ['V'] = {0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04},
  ['W'] = {0x11, 0x11, 0x11, 0x11, 0x15, 0x1B, 0x11},
  ['X'] = {0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11},
  ['Y'] = {0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04},
  ['Z'] = {0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F},
  ['0'] = {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E},
  ['1'] = {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E},
  ['2'] = {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F},
  ['3'] = {0x1F, 0x02, 0x04, 0x02, 0x01, 0x11, 0x0E},
  ['4'] = {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02},
  ['5'] = {0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E},
  ['6'] = {0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E},
  ['7'] = {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08},
  ['8'] = {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E},
  ['9'] = {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C},
  [':'] = {0x00, 0x0C, 0x0C, 0x00, 0x0C, 0x0C, 0x00},
  ['-'] = {0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00},
  ['/'] = {0x01, 0x02, 0x04, 0x08, 0x10, 0x00, 0x00},
  ['.'] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C},
  ['('] = {0x02, 0x04, 0x08, 0x08, 0x08, 0x04, 0x02},
  [')'] = {0x08, 0x04, 0x02, 0x02, 0x02, 0x04, 0x08},
  ['!'] = {0x04, 0x04, 0x04, 0x04, 0x04, 0x00, 0x04},
  ['?'] = {0x0E, 0x11, 0x01, 0x02, 0x04, 0x00, 0x04},
  ['#'] = {0x0A, 0x0A, 0x1F, 0x0A, 0x1F, 0x0A, 0x0A},
  ['%'] = {0x19, 0x1A, 0x04, 0x08, 0x13, 0x13, 0x00},
  [']'] = {0x0E, 0x08, 0x08, 0x08, 0x08, 0x08, 0x0E},
  ['['] = {0x0E, 0x02, 0x02, 0x02, 0x02, 0x02, 0x0E},
};

static void draw_char(char c, int x, int y, uint32_t col) {
  if (c >= 'a' && c <= 'z') c = (char)(c - 32);
  if (c < 0 || c >= 128) return;

  for (int row = 0; row < 7; row++) {
    uint8_t bits = font5x7[(int)c][row];
    for (int bit = 0; bit < 5; bit++) {
      if (bits & (1 << (4 - bit))) {
        draw_pixel(x + bit, y + row, col);
      }
    }
  }
}

int mui_text(mui_style s, float x, float y, const char* utf8) {
  uint32_t col = 0xFFFFFFFF;
  if (s == TS_SUMMARY_GOLD) col = 0xFFD700FF;
  else if (s == TS_PARTY_RED) col = 0xFF5555FF;
  else if (s == TS_SUMMARY_GRAY) col = 0x808080FF;
  else if (s == TS_SUMMARY_HEADER) col = 0x60A0FFFF;

  int ix = (int)x, iy = (int)y;
  int cx = ix, cy = iy;

  for (size_t i = 0; utf8 && utf8[i]; i++) {
    if (utf8[i] == '\n') {
      cy += 10;
      cx = ix;
      continue;
    }
    draw_char(utf8[i], cx, cy, col);
    cx += 6;
  }
  return 0;
}

int mui_text_o(mui_style s, float x, float y, float ox, float oy, const char* utf8) {
  if (!utf8) return 0;
  size_t max_len = 0, cur_len = 0;
  for (size_t i = 0; utf8[i]; i++) {
    if (utf8[i] == '\n') {
      if (cur_len > max_len) max_len = cur_len;
      cur_len = 0;
    } else {
      cur_len++;
    }
  }
  if (cur_len > max_len) max_len = cur_len;

  int w = (int)max_len * 6;
  float rx = x - ox * w;
  float ry = y - oy * 8;
  return mui_text(s, rx, ry, utf8);
}

/* ------------------------------------------------------------------ Tests */

static void ensure_save_dir(void) {
  mkdir("sdmc:", 0777);
  mkdir("sdmc:/3ds", 0777);
  mkdir("sdmc:/3ds/pokerogue", 0777);
  mkdir(SAVE_DIR, 0777);
  save_init();
}

static void test_1_new_game_save_point_reload(void) {
  printf("Running Test 1: New Game -> Inter-wave Save Point -> Reload Round-trip...\n");
  ensure_save_dir();

  gc_run original;
  gc_run_new(&original, GAME_MODES_CLASSIC, "testseed1234567890123456", 12345);

  gc_starter team[1] = {{.species = 25}};
  const char* why = NULL;
  bool ok = gc_run_set_starters(&original, team, 1, &why);
  assert(ok);

  char save_buf[GC_SAVE_MAX];
  size_t save_len = gc_run_save(&original, save_buf, sizeof save_buf);
  assert(save_len > 0);

  save_result write_res = save_write("session_0", save_buf, save_len);
  assert(write_res == SAVE_OK);

  char* read_buf = NULL;
  size_t read_len = 0;
  bool used_bak = false;
  save_result read_res = save_read("session_0", &read_buf, &read_len, &used_bak);
  assert(read_res == SAVE_OK && read_buf && read_len == save_len);

  gc_run reloaded;
  ok = gc_run_load(&reloaded, read_buf, read_len, &why);
  assert(ok);
  free(read_buf);

  assert(reloaded.wave == original.wave);
  assert(reloaded.score == original.score);
  assert(reloaded.money == original.money);
  assert(reloaded.n_party == original.n_party);
  assert(reloaded.party[0].b.species == original.party[0].b.species);

  printf("Test 1 PASSED.\n");
}

static void test_2_corrupt_refusal_non_destructive(void) {
  printf("Running Test 2: Corrupt / Newer-Version Format Refusal (Non-Destructive)...\n");
  ensure_save_dir();

  const char corrupt_data[] = "PRS1_CORRUPT_HEADER_BAD_CRC_DATA";
  save_write("session_1", corrupt_data, sizeof(corrupt_data));

  save_slot_info slots[SAVE_SLOT_COUNT];
  save_slot_ui_scan_slots(slots);

  assert(slots[1].exists);
  assert(!slots[1].valid);

  char main_p[256];
  snprintf(main_p, sizeof main_p, "%s/session_1.sav", SAVE_DIR);
  FILE* f = fopen(main_p, "rb");
  assert(f != NULL);
  fclose(f);

  save_slot_ui_open(NULL);
  save_slot_ui_input(MB_DOWN);
  save_slot_ui_input(MB_ACTION);
  save_slot_ui_input(MB_ACTION);

  save_slot_ui_draw();
  save_png("/home/jules/verification/save_refusal.png");

  f = fopen(main_p, "rb");
  assert(f != NULL);
  fclose(f);

  save_slot_ui_close();
  printf("Test 2 PASSED.\n");
}

static void test_3_full_5_slots(void) {
  printf("Running Test 3: Full 5 Save Slots Populated Test & Screenshots...\n");
  ensure_save_dir();

  for (int i = 0; i < 5; i++) {
    gc_run run;
    char seed[32];
    snprintf(seed, sizeof seed, "seed_slot_%d_1234567890", i);
    gc_run_new(&run, GAME_MODES_CLASSIC, seed, 1000 + i);

    gc_starter team[1] = {{.species = (uint16_t)(1 + i * 3)}};
    const char* why = NULL;
    gc_run_set_starters(&run, team, 1, &why);
    run.wave = 10 * (i + 1);
    run.score = 500 * (i + 1);
    run.money = 1200.0 * (i + 1);

    char buf[GC_SAVE_MAX];
    size_t len = gc_run_save(&run, buf, sizeof buf);

    char slot_name[32];
    snprintf(slot_name, sizeof slot_name, "session_%d", i);
    save_write(slot_name, buf, len);
  }

  save_slot_info slots[SAVE_SLOT_COUNT];
  save_slot_ui_scan_slots(slots);

  for (int i = 0; i < 5; i++) {
    assert(slots[i].exists);
    assert(slots[i].valid);
    assert(slots[i].wave == 10 * (i + 1));
    assert(slots[i].score == 500 * (i + 1));
  }

  title_ui_events te = {0};
  title_ui_open(true, 0, slots[0].wave, slots[0].biome_name, &te);
  title_ui_draw();
  save_png("/home/jules/verification/title_screen.png");
  title_ui_close();

  save_slot_ui_events se = {0};
  save_slot_ui_open(&se);
  save_slot_ui_draw();
  save_png("/home/jules/verification/save_slots_screen.png");

  save_slot_ui_input(MB_ACTION);
  save_slot_ui_draw();
  save_png("/home/jules/verification/continue_slot_submenu.png");
  save_slot_ui_close();

  printf("Test 3 PASSED.\n");
}

static void test_4_single_slot_game_over_clear_deletion(void) {
  printf("Running Test 4: Game Over & Clear Screen Single-Slot Isolation Deletion...\n");
  ensure_save_dir();

  save_slot_info slots[SAVE_SLOT_COUNT];
  save_slot_ui_scan_slots(slots);
  for (int i = 0; i < 5; i++) assert(slots[i].exists);

  game_over_ui_open(45, 2250, 2);
  game_over_ui_draw();
  save_png("/home/jules/verification/game_over_screen.png");
  game_over_ui_close();

  save_slot_ui_scan_slots(slots);
  assert(slots[0].exists);
  assert(slots[1].exists);
  assert(!slots[2].exists);
  assert(slots[3].exists);
  assert(slots[4].exists);

  clear_ui_open(200, 15000, 85000.0, 3);
  clear_ui_draw();
  save_png("/home/jules/verification/clear_screen.png");
  clear_ui_close();

  save_slot_ui_scan_slots(slots);
  assert(slots[0].exists);
  assert(slots[1].exists);
  assert(!slots[2].exists);
  assert(!slots[3].exists);
  assert(slots[4].exists);

  printf("Test 4 PASSED.\n");
}

int main(void) {
  printf("=== Title & Save UI Integration Test Suite ===\n");

  test_1_new_game_save_point_reload();
  test_2_corrupt_refusal_non_destructive();
  test_3_full_5_slots();
  test_4_single_slot_game_over_clear_deletion();

  printf("\nALL TITLE & SAVE UI TESTS PASSED SUCCESSFULLY!\n");
  return 0;
}
