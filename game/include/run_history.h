/*
 * run_history: the runs the player has finished, kept on the SD card (upstream's runHistoryData, game-data.ts saveRunHistory /
 * getRunHistoryData, RUN_HISTORY_LIMIT = 25 in run-history-ui-handler.ts).
 *
 * Upstream keeps, per finished run, the session as it ended (SessionSaveData) with whether it was a win, under the time it ended. Here an
 * entry keeps what the two history screens show and nothing of the run's state: result, mode, wave, biome, score, money, when it ended,
 * the name the player gave it (Rename Run), the party as it ended (with what the Summary page needs: moves, ability, nature, stats...), and
 * for a defeat what beat the player (the wild Pokemon, or the trainer's name). The file is `run_history` (save.h: CRC, .bak, atomic
 * replace), a little-endian byte format with a magic and a version; nothing is read or written until a function here is called.
 *
 * Rules, as for the run saves (run_saves.h):
 *   - a file that does not decode is UNREADABLE: the game never overwrites it, so a run that ends meanwhile is not added (the caller says
 *     so) and the old file stays for someone to look at. Only a missing file is an empty history.
 *   - at most RH_LIMIT entries; adding one to a full history drops the oldest (upstream: while (timestamps.length >= RUN_HISTORY_LIMIT)
 *     delete the oldest). An entry never changes after it is written.
 *   - no UI, no clock: the caller gives the time.
 */
#ifndef GM_RUN_HISTORY_H
#define GM_RUN_HISTORY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "run_saves.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RH_LIMIT 25            /* RUN_HISTORY_LIMIT */
#define RH_PARTY 6
#define RH_ENEMIES 2           /* a wild double is the most that beats a run */
#define RH_TRAINER_LEN 128     /* bytes of the trainer's name as game-core hands it out (nesting text, see i18n_resolve), with the NUL */

/* One Pokemon of an entry: the slot list's fields (rsv_mon) and what the Summary page shows. */
typedef struct {
  uint16_t species;
  uint8_t form;
  int8_t gender;          /* -1 genderless, 0 male, 1 female */
  bool shiny;
  uint8_t variant;
  uint32_t level;
  uint32_t hp;            /* 0 = fainted */
  bool fainted;
  uint8_t nature;
  uint16_t ability, passive;
  bool has_passive;
  uint8_t n_moves;
  uint16_t moves[4];
  uint8_t pp_used[4], pp_max[4];
  int8_t types[2];        /* types[1] = -1 when single-typed */
  uint32_t stats[6];      /* hp (max), atk, def, spatk, spdef, spd */
  uint8_t ivs[6];
  uint8_t friendship;
  int8_t tera_type;
  uint8_t luck;
} rh_mon;

typedef enum {
  RH_DEFEAT_NONE,         /* a win, or the run ended with nothing to name */
  RH_DEFEAT_WILD,         /* by wild Pokemon: `enemy` */
  RH_DEFEAT_TRAINER,      /* by a trainer: `trainer` is Trainer.getName(0, true) as game-core hands it out: text with $t(key) nesting */
  RH_DEFEAT_RIVAL,        /* by the rival ("Defeated by Rival"); `trainer_female` picks the female rival's name */
} rh_defeat;

typedef struct {
  int64_t time;           /* seconds since 1970 when the run ended, 0 = not known */
  bool victory;
  int mode;               /* GameModes */
  int wave, biome, score;
  int64_t money;
  uint8_t n_party;
  rh_mon party[RH_PARTY];
  uint8_t defeat;         /* rh_defeat */
  uint8_t n_enemy;
  rh_mon enemy[RH_ENEMIES];
  bool trainer_double;    /* TRAINER / RIVAL: a pair of trainers (TrainerVariant.DOUBLE) */
  bool trainer_female;    /* ... and the female variant (the rival's name) */
  char trainer[RH_TRAINER_LEN];
  char name[RSV_NAME_LEN];  /* the run's name (Rename Run), "" = none */
} rh_entry;

/* Bytes of the longest file the encoder writes (the decoder refuses a longer one). */
#define RH_ENTRY_MAX 1024
#define RH_FILE_MAX (8 + RH_LIMIT * RH_ENTRY_MAX)

typedef enum {
  RH_OK,                  /* read (an empty history if there was no file) */
  RH_UNREADABLE,          /* there is a file and it does not decode (damaged, or made by a newer version) */
} rh_status;

/* Reads the history from the SD card into the module's copy. The copy is what rhist_count / rhist_get return; it is replaced on every
 * call that returns RH_OK and emptied on RH_UNREADABLE. */
rh_status rhist_load(void);
int rhist_count(void);
/* i = 0 is the newest. NULL outside 0..count-1. Valid until the next rhist_load / rhist_add. */
const rh_entry* rhist_get(int i);
/* Adds the entry as the newest and writes the file; the history is first read from the card. False: nothing was written and the copy is
 * as it was (the file is unreadable, or the SD card refused the write); `status` (may be NULL) says which. */
bool rhist_add(const rh_entry* e, rh_status* status);

/* The byte format, for the tests: encodes `n` entries (oldest first) into `out` (RH_FILE_MAX bytes) and returns the length, or 0 when an
 * entry does not fit the format; decodes `len` bytes into `out` (up to RH_LIMIT) and returns the count, or -1 when they do not decode. */
size_t rhist_encode(const rh_entry* e, int n, uint8_t* out, size_t cap);
int rhist_decode(const uint8_t* buf, size_t len, rh_entry* out, int cap);

/* ---- building an entry from the run that ended (history_record.c) ---- */
struct gc_run;
/* Fills `out` from the run as it ended: `victory`, `now` (seconds, the caller's clock), `name` (the run's name, may be NULL). The enemy
 * that beat the player comes from the battle the run ended in. Never fails. */
void rhist_record(const struct gc_run* run, bool victory, int64_t now, const char* name, rh_entry* out);

/* ---- what the screens show of an entry ---- */
/* The slot list's view of it (run_summary.h draws that): mode, wave, biome, score, money, party. */
void rhist_as_info(const rh_entry* e, rsv_info* out);

#ifdef __cplusplus
}
#endif
#endif
