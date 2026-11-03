#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "run_settings.h"
#include "save.h"

static int checks, failures;
#define CHECK(c, ...) do { checks++; if (!(c)) { failures++; printf("FAIL: "); printf(__VA_ARGS__); printf("\n"); } } while (0)

static char saved[8];
static size_t saved_len;
static const char* input;
static size_t input_len;
static save_result read_result = SAVE_OK, write_result = SAVE_OK;

save_result save_read(const char* name, char** out, size_t* len, bool* backup) {
  CHECK(!strcmp(name, "settings"), "settings filename");
  if (backup) *backup = false;
  if (read_result != SAVE_OK) return read_result;
  *out = malloc(input_len + 1);
  if (!*out) return SAVE_IO_ERROR;
  memcpy(*out, input, input_len);
  (*out)[input_len] = 0;
  *len = input_len;
  return SAVE_OK;
}

save_result save_write(const char* name, const void* data, size_t len) {
  CHECK(!strcmp(name, "settings"), "settings filename");
  if (write_result != SAVE_OK) return write_result;
  if (len > sizeof saved) return SAVE_IO_ERROR;
  memcpy(saved, data, len);
  saved_len = len;
  return SAVE_OK;
}

static void load_case(const char* bytes, size_t len, rset_values expected, const char* why) {
  input = bytes;
  input_len = len;
  read_result = SAVE_OK;
  rset_values got = rset_load();
  CHECK(got.speed == expected.speed && got.custom == expected.custom && got.mode == expected.mode,
        "%s (speed %d custom %d mode %d)", why, got.speed, got.custom, got.mode);
}

int main(void) {
  const rset_values defaults = {.speed=RSET_SPEED_NORMAL,.custom=false,.mode=GAME_MODES_CLASSIC};
  load_case("", 0, defaults, "empty file defaults to Classic");
  const char legacy[] = {RSET_SPEED_FAST, 1};
  load_case(legacy, sizeof legacy, (rset_values){.speed=RSET_SPEED_FAST,.custom=true,.mode=GAME_MODES_CLASSIC},
            "two-byte settings remain compatible and default to Classic");
  const char endless[] = {RSET_SPEED_SLOW, 0, GAME_MODES_ENDLESS};
  load_case(endless, sizeof endless, (rset_values){.speed=RSET_SPEED_SLOW,.custom=false,.mode=GAME_MODES_ENDLESS},
            "Endless mode persists");
  const char spliced[] = {RSET_SPEED_TURBO, 0, GAME_MODES_SPLICED_ENDLESS};
  load_case(spliced, sizeof spliced, (rset_values){.speed=RSET_SPEED_TURBO,.custom=false,.mode=GAME_MODES_SPLICED_ENDLESS},
            "Spliced Endless mode persists");
  const char bad_mode[] = {RSET_SPEED_NORMAL, 0, 99};
  load_case(bad_mode, sizeof bad_mode, defaults, "invalid stored modes fall back to Classic");
  const rset_values write = {.speed=RSET_SPEED_FAST,.custom=true,.mode=GAME_MODES_SPLICED_ENDLESS};
  CHECK(rset_save(&write) && saved_len == 3 && saved[0] == RSET_SPEED_FAST && saved[1] == 1 && saved[2] == GAME_MODES_SPLICED_ENDLESS,
        "saving writes speed, custom and mode");
  CHECK(!rset_save(&(rset_values){.speed=RSET_SPEED_NORMAL,.mode=99}), "invalid modes are not saved");
  read_result = SAVE_NOT_FOUND;
  CHECK(rset_load().mode == GAME_MODES_CLASSIC, "missing settings default to Classic");
  printf("run_settings_test: %d checks, %d failed\n", checks, failures);
  return failures ? 1 : 0;
}
