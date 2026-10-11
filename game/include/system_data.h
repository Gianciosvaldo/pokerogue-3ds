/*
 * system_data: the system data (game-core's gc_system: the dex, the starter unlocks, candy, eggs, vouchers, win records) that
 * every run shares, kept on the SD card in the file "system" next to the run slots (save.h: CRC, backup, atomic replace).
 *
 * Upstream keeps it in the account's system save; it is what makes a species a starter and what the starter select screen
 * lists. A run reads and updates it while it plays (gc_run.sys); the app writes it at each of the run's save points and
 * when a run ends, and only when it changed.
 *
 * A file that is there and cannot be read (damaged, made by a newer version) is never repaired, overwritten or deleted: the game
 * goes on with the default system data (the starters a new save has) and does not write it, and sysd_state says why so the
 * player can be told. A file that is not there is a new install: the defaults, written at the first save point.
 */
#ifndef GM_SYSTEM_DATA_H
#define GM_SYSTEM_DATA_H

#include <stdbool.h>

#include "gc/system.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  SYSD_LOADED,     /* read from the SD card */
  SYSD_NEW,        /* no file: the defaults of a new save */
  SYSD_UNREADABLE, /* a file that could not be read: the defaults, never written over it */
} sysd_status;

#define SYSD_WHY_LEN 96

/* The system data, read from the SD card on first use. Never NULL. */
gc_system* sysd_get(void);

/* How the file was found at the first use of sysd_get (reads it if that has not happened). `why` (may be NULL) gets the reason
 * for SYSD_UNREADABLE, in words for the player. */
sysd_status sysd_state(char why[SYSD_WHY_LEN]);

/* Writes the system data if it changed since it was read or last written. True when there is nothing more to write: it is on the
 * SD card as it is now (including "unchanged"), or the file on the card could not be read (SYSD_UNREADABLE), which is never written
 * over and is not reported as a failure at every save. False when the SD card refused the write; the next call retries it. */
bool sysd_save(void);

/* For tests: forget what was read, so the next sysd_get reads the folder again. */
void sysd_reset(void);

#ifdef __cplusplus
}
#endif
#endif
