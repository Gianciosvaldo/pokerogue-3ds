/*
 * profile_tx: the app's owner of profile (system data) transactions (task C08; docs/CUSTOM-CONTENT-CONTRACTS.md, section 2.14).
 *
 * Two things change the player's profile outside a run: a voucher-to-egg pull (game-core's gc_system_gacha_pull) and the once-per-record
 * Nemesis reward (one regular voucher, claimed together with the record's REWARD_CLAIMED). The profile (system.sav), the custom file
 * (custom.sav) and the gacha scene RNG are three things the save library cannot change together (it is atomic per file only), so every
 * such change goes through ONE write-ahead envelope, the file "ptx":
 *
 *   begin_*   PREPARED   the request (machine, option, clock, entropy seed; or the record) is saved first and gets a transaction id
 *   execute   COMMITTED  the pull runs ONCE on the profile; the candidate profile bytes, the resulting scene RNG, the transaction id and
 *                        the immutable result are written in ONE envelope write: that write is the commit point
 *   settle    cleared    the committed changes are rolled forward to their files (custom claim, then system.sav), then the pending record
 *                        is cleared. Every step is idempotent, so a crash anywhere, or a failed write, is finished by the next boot
 *                        (ptx_recover) or the next profile save (the gate on sysd_save) with the SAME result: no second pull, no second grant
 *
 * Nothing is granted, debited or revealed before the commit point; if that write fails the live profile is put back from its own
 * serialization (the bytes a restart would read) and the scene RNG is not advanced. A committed result is durable: if only the roll-
 * forward failed, the result is still returned (PTX_X_DONE_APPLY_PENDING) and the files catch up.
 *
 * The profile is never copied as a struct: it is serialized (gc_system_save) and loaded (gc_system_load), exactly as it is saved.
 * A ptx file that is damaged or from a newer version, or a profile that cannot be read, is reported and left as it is.
 * The session slots, the system.sav format and save.c's CRC / backup are unchanged. The gacha scene RNG is its own stream in this file,
 * never a run's battle or generation RNG. No network, no account, no hatching.
 */
#ifndef GM_PROFILE_TX_H
#define GM_PROFILE_TX_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "cc/types.h"
#include "gc/system.h"
#include "save.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PTX_FILE_NAME "ptx"
#define PTX_SEED_MAX 32      /* bytes of entropy seed text a request carries */
#define PTX_RESULTS 8        /* committed results kept (oldest dropped) */
#define PTX_MAX_EGGS 25
#define PTX_KEY_LEN 40
#define PTX_WHY_LEN 160

typedef enum { PTX_NEW, PTX_LOADED, PTX_UNUSABLE } ptx_state;

typedef enum {
  PTX_OK = 0,
  PTX_E_UNUSABLE,     /* the ptx file or the profile cannot be used (ptx_why says why); nothing was written */
  PTX_E_NO_SEED,      /* the first transaction needs ptx_set_scene_seed first */
  PTX_E_BAD_REQUEST,  /* the request does not fit the contract */
  PTX_E_BUSY,         /* another transaction is not finished (ptx_pending) */
  PTX_E_IO,           /* the SD card refused the write, or the profile could not be serialized: nothing changed, a retry is safe */
  PTX_E_NOT_OWED,     /* the record's reward is not owed (not won, already claimed, unknown) */
  PTX_E_NO_MEMORY,
} ptx_status;

typedef struct {
  uint8_t machine;                 /* 0..2 */
  uint8_t option;                  /* 0..4 (gc_gacha_ctx.cursor) */
  uint64_t timestamp_ms;           /* the clock the pull uses, supplied once: 0..8640000000000000 */
  uint8_t seed_len;                /* 1..PTX_SEED_MAX: the entropy stream's seed text, supplied once */
  uint8_t seed[PTX_SEED_MAX];
} ptx_pull_request;

enum { PTX_KIND_PULL = 1, PTX_KIND_CLAIM = 2 };
enum { PTX_R_DONE = 1, PTX_R_REFUSED = 2 };

typedef struct {
  uint32_t txn;
  uint8_t kind;                    /* PTX_KIND_* */
  uint8_t outcome;                 /* PTX_R_DONE, or PTX_R_REFUSED (nothing changed; key says why) */
  char key[PTX_KEY_LEN];           /* the refusal's source key, e.g. "egg:notEnoughVouchers" */
  /* a pull: the eggs it appended, in storage order, and the order they are shown in (indexes into eggs) */
  uint8_t n;
  uint8_t order[PTX_MAX_EGGS];
  gc_egg eggs[PTX_MAX_EGGS];
  /* a claim */
  uint32_t record;
  uint8_t vouchers;
} ptx_result;

/* How the file was found (reads it then); why (may be NULL) is a sentence for the player when PTX_UNUSABLE. */
ptx_state ptx_get_state(char why[PTX_WHY_LEN]);

/* The text the gacha scene RNG is first seeded from, once, when the file is new. Call before the first begin; the app supplies it from
 * its own clock. Not used after the stream exists (it lives in the file). */
void ptx_set_scene_seed(const char* ascii);

/* Boot: installs the gate on sysd_save and finishes a committed transaction a crash left half applied. Call once, before the profile is
 * first used. Returns PTX_OK or PTX_E_UNUSABLE. Cheap when there is no ptx file. */
ptx_status ptx_boot(void);
/* The part of ptx_boot that settles; also callable later. */
ptx_status ptx_recover(void);

/* Persists the request and returns its transaction id (never 0). Refuses when the profile is unreadable (a default profile is not the
 * player's wallet) or another transaction is pending. */
ptx_status ptx_begin_pull(const ptx_pull_request* req, uint32_t* txn);
ptx_status ptx_begin_claim(cc_record_id record, uint32_t* txn);

typedef enum {
  PTX_X_DONE,                /* committed and rolled forward; *out is the immutable result */
  PTX_X_DONE_APPLY_PENDING,  /* committed (the result is durable) but a file is not caught up yet: it will be, at the next boot or save */
  PTX_X_REFUSED,             /* a cost or capacity refusal: nothing changed; out->key says why */
  PTX_X_IO,                  /* the SD card refused before the commit point: nothing changed (live profile and RNG as they were), retry */
  PTX_X_UNKNOWN,             /* no such transaction pending or kept */
  PTX_X_UNUSABLE,            /* the file or the profile cannot be used */
  PTX_X_FATAL,               /* the live profile could not be put back after a failed commit: no more transactions this session */
} ptx_exec;

/* Runs the transaction: the pull happens once. Asking again for the same id (a retry, a restart, a repeated tap) returns the committed
 * result and never pulls again. */
ptx_exec ptx_execute(uint32_t txn, ptx_result* out);
/* A committed (or refused) result still kept. */
bool ptx_result_of(uint32_t txn, ptx_result* out);
/* The transaction begun but not finished (prepared, or committed and not rolled forward); 0 when none. */
uint32_t ptx_pending(void);
/* A committed transaction waits for its roll-forward. */
bool ptx_apply_pending(void);
/* Retries the roll-forward now. True when nothing is left to apply. */
bool ptx_settle(void);

/* Whether a committed pull has been SHOWN to the player (task C09). A watermark in this file: every pull with an id up to it has been
 * shown. Showing is not part of the transaction (the result is durable and immutable either way); this only keeps a restart from
 * showing the same eggs twice, or never. */
/* The oldest committed pull that was not shown yet (its id, and the result in *out when out is not NULL); 0 when none. */
uint32_t ptx_unrevealed(ptx_result* out);
/* Records that the pull txn (a kept, committed pull) was shown. One write of this file; the pending transaction, if any, is carried
 * unchanged. True when it is recorded (or already was); false when the card refused (nothing changed: the eggs would be shown again
 * after a restart, never pulled again), or when txn is not a kept committed pull. */
bool ptx_mark_revealed(uint32_t txn);

/* ---- for tests ---- */
typedef struct {
  save_result (*read)(const char* name, char** out, size_t* len, bool* used_backup);
  save_result (*write)(const char* name, const void* data, size_t len);
} ptx_io;
void ptx_set_io(const ptx_io* io);   /* NULL: save_read / save_write */
void ptx_set_profile_cap(size_t cap); /* a smaller bound for serializing the profile (0 = the real one): forces "cannot be serialized" */
void ptx_reset(void);                /* forget everything read (and any scene seed), so the next use reads the folder again */
size_t ptx_static_bytes(void);       /* the static storage this module holds (the profile buffers are allocated per transaction) */
uint64_t ptx_scene_digest(void);     /* a hash of the gacha scene RNG's state, 0 when it has none yet */

#ifdef __cplusplus
}
#endif
#endif
