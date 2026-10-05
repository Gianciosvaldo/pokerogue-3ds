/*
 * run_saves: the run's save slots on the SD card.
 *
 * Upstream keeps five session slots (SESSION_SLOTS_COUNT in save-slot-select-ui-handler.ts) and saves the
 * session at the start of every encounter. Here a slot is a file `session_<n>` under SAVE_DIR (save.h),
 * holding game-core's own run save (gc_run_save / gc_run_load): this module does not know the format, it only
 * moves the bytes and asks game-core whether they load.
 *
 * Rules the rest of the app relies on:
 *   - A slot whose file exists but does not load (damaged, or made by a newer version) is INCOMPATIBLE. The
 *     game never overwrites, repairs or deletes it on its own; only an explicit "Delete Run" does.
 *   - Writing goes through save_write: a temp file with a checksum, renamed into place, the previous save kept as
 *     `.bak`. A failed write leaves the previous save as it was.
 *   - Buffers are not on the stack: a save can be 128 KB (GC_SAVE_MAX) and the 3DS stack is 32 KB.
 *   - No UI, no timing: callers decide when saving is allowed (run_screen.c saves between scenes only).
 */
#ifndef GM_RUN_SAVES_H
#define GM_RUN_SAVES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "gc/run.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RSV_SLOTS 5     /* upstream's SESSION_SLOTS_COUNT */
#define RSV_WHY_LEN 96

typedef enum {
  RSV_EMPTY,          /* no save in the slot */
  RSV_OK,             /* the slot loads; the summary is filled in */
  RSV_INCOMPATIBLE,   /* there is a file but it can't be loaded; `why` says why */
} rsv_status;

/* One party member as the slot list and the result screens show it. */
typedef struct {
  uint16_t species;
  uint8_t form;
  int8_t gender;      /* -1 genderless, 0 male, 1 female */
  bool shiny;
  uint8_t variant;
  uint8_t level;
  bool fainted;
} rsv_mon;

/* What a run (or a slot) looks like from outside. */
typedef struct {
  rsv_status status;
  char why[RSV_WHY_LEN];  /* INCOMPATIBLE: the message the screens show */
  bool from_backup;       /* OK: the main file was damaged and the previous save (.bak) was read */
  int mode;               /* GameModes */
  int wave, biome, score;
  double money;
  int n_party;
  rsv_mon party[6];
} rsv_info;

/* The summary of a live run (party in the player's order). Leaves `status` and `why` alone. */
void rsv_summarize(const gc_run* run, rsv_info* out);

bool rsv_slot_ok(int slot);
/* "session_<slot>" */
const char* rsv_slot_name(int slot, char out[16]);

/* Reads a slot and tries to load it into game-core, to tell what it holds. Never changes a file. */
rsv_status rsv_inspect(int slot, rsv_info* out);
/* Same for every slot. */
void rsv_inspect_all(rsv_info out[RSV_SLOTS]);

/* Loads the slot into `run`, which is replaced. EMPTY: nothing there. INCOMPATIBLE: `why` says why and the file is
 * untouched (`run` is then unusable until gc_run_new or another load). */
rsv_status rsv_load(int slot, gc_run* run, char why[RSV_WHY_LEN]);

typedef enum {
  RSV_SAVED,
  RSV_NOT_ALLOWED,    /* game-core is not at a save point (gc_run_can_save is false) */
  RSV_IO_ERROR,       /* the SD card refused the write; the previous save is as it was */
  RSV_BAD_SLOT,
} rsv_save_result;

/* Saves the run into the slot (gc_run_save, then save_write) and makes the slot the last one used. */
rsv_save_result rsv_save(int slot, const gc_run* run);

/* Removes the slot's files (the save, its backup and a leftover temp file). The run in it is over, or the player
 * asked for it to go. */
bool rsv_clear(int slot);

/* The slot the Continue entry loads (upstream's loggedInUser.lastSessionSlot): set by every save and load. -1 when
 * none is recorded. */
int rsv_last_slot(void);
void rsv_set_last_slot(int slot);

#ifdef __cplusplus
}
#endif
#endif
