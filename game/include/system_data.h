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

#include <stddef.h>
#include <stdint.h>

#include "gc/system.h"
#include "save.h"

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

/* ---- for the profile transaction owner (profile_tx.c, task C08) ---- */

/* The canonical bytes of the system data as it is now (gc_system_save): what a save would write. 0: it could not be serialized (the
 * buffer is too small or the profile is not representable); nothing is changed. */
size_t sysd_serialize(uint8_t* buf, size_t cap);
/* A hash of canonical bytes (the same one the "unchanged" check of sysd_save uses). */
uint64_t sysd_digest(const uint8_t* buf, size_t len);
/* The canonical digest of what the SD card holds (the file as sysd_get read it, saved again; no file = the new-install defaults), or
 * false when the file is unreadable. Only true to the card while nothing has changed memory since the first sysd_get: ask at boot. */
bool sysd_card_digest(uint64_t* out);
/* Replaces the system data in memory with canonical bytes (gc_system_load). False when they do not load: the system data is then left
 * as the loader made it, so the caller must have a way back (the bytes it serialized before). */
bool sysd_restore(const uint8_t* buf, size_t len);
/* Writes these canonical bytes to the card as the system data, whatever memory holds now (one save_write). False when the card
 * refused, or when the file on the card was unreadable (never written over). Memory is not changed. */
bool sysd_write_bytes(const uint8_t* buf, size_t len);
/* sysd_save asks this first when it is set; false means "do not write now" and sysd_save returns false. */
void sysd_set_gate(bool (*gate)(void));

typedef struct {
  save_result (*read)(const char* name, char** out, size_t* len, bool* used_backup);
  save_result (*write)(const char* name, const void* data, size_t len);
} sysd_io;
void sysd_set_io(const sysd_io* io);   /* NULL: save_read / save_write. The next sysd_get reads again only after sysd_reset */

/* Forget what was read, so the next sysd_get reads the folder again. For tests, and for Save and Quit (app_flow.c): what a battle changed
 * in memory and never wrote goes with the battle. */
void sysd_reset(void);

#ifdef __cplusplus
}
#endif
#endif
