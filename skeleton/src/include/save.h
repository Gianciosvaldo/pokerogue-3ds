/*
 * Local saves on the SD card, replacing PokeRogue's server-side save API.
 *
 * Upstream stores system data and each session slot as JSON strings; this
 * module stores opaque byte blobs (that JSON, later) under sdmc:/3ds/pokerogue/save/.
 *
 * Writes are crash-safe: data goes to <name>.tmp with a CRC32 header, then the
 * previous file is kept as <name>.bak and the temp file is renamed into place.
 * Reads fall back to .bak when the main file is missing or fails its CRC.
 */
#ifndef SAVE_H
#define SAVE_H

#include <stdbool.h>
#include <stddef.h>

#define SAVE_DIR "sdmc:/3ds/pokerogue/save"

typedef enum {
  SAVE_OK = 0,
  SAVE_NOT_FOUND,
  SAVE_CORRUPT,  /* both main and .bak failed the CRC check */
  SAVE_IO_ERROR,
} save_result;

/* Creates SAVE_DIR if needed. */
bool save_init(void);

/* name: a plain file name such as "system" or "session_0". */
save_result save_write(const char* name, const void* data, size_t len);

/* On SAVE_OK, *out is malloc'd (caller frees) and NUL-terminated for convenience.
 * *used_backup is set when the main file was bad and .bak was used. */
save_result save_read(const char* name, char** out, size_t* len, bool* used_backup);

const char* save_result_str(save_result r);

#endif
