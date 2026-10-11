/* The system data on the SD card (see system_data.h). */
#include "system_data.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "save.h"

#define FILE_NAME "system"
/* gc_system_save writes a sparse form: 3 KB for a new save, 88 KB with 1100 species fully unlocked */
#define BUF_MAX (1024 * 1024)

/* The system data is 724 KB: a static, not the stack (the 3DS stack is 32 KB). */
static gc_system g_sys;
static struct {
  bool loaded;
  sysd_status status;
  char why[SYSD_WHY_LEN];
  uint64_t written; /* hash of the bytes the SD card holds (or that were read), valid when have_written */
  bool have_written;
} S;

static uint64_t fnv1a(const uint8_t* p, size_t n) {
  uint64_t h = 1469598103934665603ull;
  for (size_t i = 0; i < n; i++) h = (h ^ p[i]) * 1099511628211ull;
  return h;
}

static void use_defaults(void) { gc_system_init_defaults(&g_sys); }

static void load(void) {
  S.loaded = true;
  S.why[0] = 0;
  S.have_written = false;
  char* buf = NULL;
  size_t len = 0;
  const save_result r = save_read(FILE_NAME, &buf, &len, NULL);
  if (r == SAVE_NOT_FOUND) {
    use_defaults();
    S.status = SYSD_NEW;
    return;
  }
  if (r == SAVE_OK) {
    const char* gc_why = NULL;
    if (gc_system_load(&g_sys, buf, len, &gc_why)) {
      free(buf);
      S.status = SYSD_LOADED;
      /* what the file holds, as the next save would write it: a save is skipped while the data is still this */
      uint8_t* out = malloc(BUF_MAX);
      if (out) {
        const size_t n = gc_system_save(&g_sys, out, BUF_MAX);
        if (n) {
          S.written = fnv1a(out, n);
          S.have_written = true;
        }
        free(out);
      }
      return;
    }
    free(buf);
    snprintf(S.why, sizeof S.why, "The system data can't be read: %.60s.", gc_why ? gc_why : "unknown reason");
  } else if (r == SAVE_CORRUPT) {
    snprintf(S.why, sizeof S.why, "The system data is damaged (it failed its checksum).");
  } else {
    snprintf(S.why, sizeof S.why, "The system data could not be read from the SD card.");
  }
  use_defaults(); /* gc_system_load may have left it half filled */
  S.status = SYSD_UNREADABLE;
}

gc_system* sysd_get(void) {
  if (!S.loaded) load();
  return &g_sys;
}

sysd_status sysd_state(char why[SYSD_WHY_LEN]) {
  if (!S.loaded) load();
  if (why) snprintf(why, SYSD_WHY_LEN, "%s", S.why);
  return S.status;
}

bool sysd_save(void) {
  if (!S.loaded) return true; /* nothing was read, so nothing changed */
  /* never over a file that could not be read. Not a failure to report: the player was told when the starter select opened, and a run
   * save that says "could not be saved" at every wave would only be wrong (the run itself is written) */
  if (S.status == SYSD_UNREADABLE) return true;
  uint8_t* buf = malloc(BUF_MAX);
  if (!buf) return false;
  bool ok = false;
  const size_t n = gc_system_save(&g_sys, buf, BUF_MAX);
  if (n) {
    const uint64_t h = fnv1a(buf, n);
    if (S.have_written && h == S.written) {
      ok = true;
    } else if (save_write(FILE_NAME, buf, n) == SAVE_OK) {
      S.written = h;
      S.have_written = true;
      S.status = SYSD_LOADED;
      ok = true;
    }
  }
  free(buf);
  return ok;
}

void sysd_reset(void) { memset(&S, 0, sizeof S); }
