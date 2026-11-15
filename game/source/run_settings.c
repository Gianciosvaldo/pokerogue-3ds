/* The run settings kept on the SD card (see run_settings.h); the screen is run_settings_screen.c. */
#include "run_settings.h"

#include <stdlib.h>

#include "save.h"

#define SETTINGS_FILE "settings"

/* ------------------------------------------------------------------ the file: speed, Bizarre flag and selected game mode
 * Byte 1 was the "Custom" switch before the Bizarre mode existed. It is still written (1 for Bizarre) and read: an older file with it set,
 * and the Classic mode in byte 2, is a Bizarre choice now (the old Custom runs have the JoJo trainers). */

rset_values rset_load(void) {
  rset_values v = {.speed=RSET_SPEED_NORMAL,.mode=GAME_MODES_CLASSIC};
  char* buf = NULL;
  size_t len = 0;
  if (save_read(SETTINGS_FILE, &buf, &len, NULL) == SAVE_OK) {
    if ((len == 2 || len == 3) && buf[0] >= RSET_SPEED_SLOW && buf[0] <= RSET_SPEED_TURBO && (buf[1] == 0 || buf[1] == 1)) {
      v.speed = buf[0];
      if (len == 3 && (unsigned char)buf[2] <= RSET_MODE_BIZARRE && (unsigned char)buf[2] != GAME_MODES_DAILY && (unsigned char)buf[2] != GAME_MODES_CHALLENGE)
        v.mode = (unsigned char)buf[2];
      if (buf[1] == 1 && v.mode == GAME_MODES_CLASSIC) v.mode = RSET_MODE_BIZARRE;
    }
    free(buf);
  }
  return v;
}

bool rset_save(const rset_values* v) {
  if (!v || v->mode < GAME_MODES_CLASSIC || v->mode > RSET_MODE_BIZARRE || v->mode == GAME_MODES_DAILY || v->mode == GAME_MODES_CHALLENGE) return false;
  const char b[3] = {(char)v->speed, (char)(v->mode == RSET_MODE_BIZARRE ? 1 : 0), (char)v->mode};
  return save_write(SETTINGS_FILE, b, sizeof b) == SAVE_OK;
}

float rset_time_scale(int speed) {
  if (speed < RSET_SPEED_SLOW || speed > RSET_SPEED_TURBO) speed = RSET_SPEED_NORMAL;
  return (float)speed;
}
