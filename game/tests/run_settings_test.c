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
  CHECK(got.speed == expected.speed && got.mode == expected.mode, "%s (speed %d mode %d)", why, got.speed, got.mode);
}

int main(void) {
  const rset_values defaults = {.speed=RSET_SPEED_NORMAL,.mode=GAME_MODES_CLASSIC};
  load_case("", 0, defaults, "empty file defaults to Classic");
  const char legacy[] = {RSET_SPEED_FAST, 1};
  load_case(legacy, sizeof legacy, (rset_values){.speed=RSET_SPEED_FAST,.mode=RSET_MODE_BIZARRE},
            "two-byte settings with the old Custom switch on remain compatible and are a Bizarre choice now");
  const char legacy_off[] = {RSET_SPEED_FAST, 0};
  load_case(legacy_off, sizeof legacy_off, (rset_values){.speed=RSET_SPEED_FAST,.mode=GAME_MODES_CLASSIC},
            "two-byte settings with Custom off are Classic");
  const char custom3[] = {RSET_SPEED_SLOW, 1, GAME_MODES_CLASSIC};
  load_case(custom3, sizeof custom3, (rset_values){.speed=RSET_SPEED_SLOW,.mode=RSET_MODE_BIZARRE},
            "three-byte settings the Custom row wrote (switch on, Classic) are Bizarre now");
  const char bizarre[] = {RSET_SPEED_NORMAL, 1, RSET_MODE_BIZARRE};
  load_case(bizarre, sizeof bizarre, (rset_values){.speed=RSET_SPEED_NORMAL,.mode=RSET_MODE_BIZARRE}, "Bizarre mode persists");
  const char daily[] = {RSET_SPEED_NORMAL, 0, GAME_MODES_DAILY};
  load_case(daily, sizeof daily, defaults, "a stored Daily Run (never offered) falls back to Classic");
  const char endless[] = {RSET_SPEED_SLOW, 0, GAME_MODES_ENDLESS};
  load_case(endless, sizeof endless, (rset_values){.speed=RSET_SPEED_SLOW,.mode=GAME_MODES_ENDLESS},
            "Endless mode persists");
  const char challenge[] = {RSET_SPEED_NORMAL, 0, GAME_MODES_CHALLENGE};
  load_case(challenge, sizeof challenge, (rset_values){.speed=RSET_SPEED_NORMAL,.mode=GAME_MODES_CHALLENGE}, "Challenge mode persists");
  const char spliced[] = {RSET_SPEED_TURBO, 0, GAME_MODES_SPLICED_ENDLESS};
  load_case(spliced, sizeof spliced, (rset_values){.speed=RSET_SPEED_TURBO,.mode=GAME_MODES_SPLICED_ENDLESS},
            "Spliced Endless mode persists");
  const char bad_mode[] = {RSET_SPEED_NORMAL, 0, 99};
  load_case(bad_mode, sizeof bad_mode, defaults, "invalid stored modes fall back to Classic");
  const rset_values write = {.speed=RSET_SPEED_FAST,.mode=GAME_MODES_SPLICED_ENDLESS};
  CHECK(rset_save(&write) && saved_len == 3 && saved[0] == RSET_SPEED_FAST && saved[1] == 0 && saved[2] == GAME_MODES_SPLICED_ENDLESS,
        "saving writes speed, the Bizarre switch (off) and mode");
  const rset_values write_bizarre = {.speed=RSET_SPEED_TURBO,.mode=RSET_MODE_BIZARRE};
  CHECK(rset_save(&write_bizarre) && saved_len == 3 && saved[0] == RSET_SPEED_TURBO && saved[1] == 1 && saved[2] == RSET_MODE_BIZARRE,
        "saving Bizarre writes the old Custom switch on too");
  const rset_values write_challenge = {.speed=RSET_SPEED_NORMAL,.mode=GAME_MODES_CHALLENGE};
  CHECK(rset_save(&write_challenge) && saved_len == 3 && saved[1] == 0 && saved[2] == GAME_MODES_CHALLENGE, "Challenge mode is saved as the mode, Bizarre off");
  CHECK(!rset_save(&(rset_values){.speed=RSET_SPEED_NORMAL,.mode=99}), "invalid modes are not saved");
  CHECK(!rset_save(&(rset_values){.speed=RSET_SPEED_NORMAL,.mode=GAME_MODES_DAILY}), "a Daily Run is not saved");
  read_result = SAVE_NOT_FOUND;
  CHECK(rset_load().mode == GAME_MODES_CLASSIC, "missing settings default to Classic");
  /* the in-battle Y button: Slow -> Normal -> Fast -> Turbo -> Slow; a speed that is no preset goes to Normal */
  CHECK(rset_next_speed(RSET_SPEED_SLOW) == RSET_SPEED_NORMAL, "Slow goes to Normal");
  CHECK(rset_next_speed(RSET_SPEED_NORMAL) == RSET_SPEED_FAST, "Normal goes to Fast");
  CHECK(rset_next_speed(RSET_SPEED_FAST) == RSET_SPEED_TURBO, "Fast goes to Turbo");
  CHECK(rset_next_speed(RSET_SPEED_TURBO) == RSET_SPEED_SLOW, "Turbo wraps to Slow");
  CHECK(rset_next_speed(1) == RSET_SPEED_NORMAL && rset_next_speed(0) == RSET_SPEED_NORMAL && rset_next_speed(6) == RSET_SPEED_NORMAL &&
        rset_next_speed(-3) == RSET_SPEED_NORMAL, "a speed that is no preset goes to Normal");
  for (int s = RSET_SPEED_SLOW; s <= RSET_SPEED_TURBO; s++)
    CHECK(rset_speed_from_scale(rset_time_scale(s)) == s, "the preset %d is found again from its time scale", s);
  CHECK(rset_speed_from_scale(1.0f) == RSET_SPEED_NORMAL && rset_speed_from_scale(0.0f) == RSET_SPEED_NORMAL &&
        rset_speed_from_scale(9.0f) == RSET_SPEED_NORMAL, "a scale that is no preset reads as Normal");
  CHECK(rset_speed_from_scale(3.4f) == RSET_SPEED_NORMAL && rset_speed_from_scale(3.6f) == RSET_SPEED_FAST, "a scale rounds to the nearest preset");
  CHECK(!strcmp(rset_speed_key(RSET_SPEED_SLOW), "settings:gameSpeedSlow") && !strcmp(rset_speed_key(RSET_SPEED_NORMAL), "settings:gameSpeedNormal") &&
        !strcmp(rset_speed_key(RSET_SPEED_FAST), "settings:gameSpeedFast") && !strcmp(rset_speed_key(RSET_SPEED_TURBO), "settings:gameSpeedTurbo"),
        "each preset has its own name");
  CHECK(!strcmp(rset_speed_key(0), "settings:gameSpeedNormal") && !strcmp(rset_speed_key(9), "settings:gameSpeedNormal"), "a speed that is no preset is named Normal");
  printf("run_settings_test: %d checks, %d failed\n", checks, failures);
  return failures ? 1 : 0;
}
