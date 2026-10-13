/*
 * custom_store: the custom content metadata on the SD card (task C04; docs/CUSTOM-CONTENT-CONTRACTS.md, sections 2.10 and 5.3).
 *
 * One file, "custom" next to the run slots and the system data (save.h: CRC, backup, atomic replace), holding custom-core's cc_meta:
 * the id counters, the transaction ledger, the Nemesis roster (empty until a defeating enemy can be identified: see below) and the
 * most recent run report with its last turns. The run slots, the system data and every existing schema are untouched.
 *
 * What the app does with it:
 *   - cs_begin_run (installed as the observer's uid source) gives each observed run a durable uid: the counter is written to the SD
 *     card BEFORE the uid is used, so a restart can never hand the same uid out twice.
 *   - cs_commit stores a finished run's report as one transaction (cc_commit_terminal): report, history and ledger entry in one
 *     write, idempotent by run uid, so a retried callback, a reopened result screen or a restart cannot store a run twice. The
 *     in-memory state changes only after the write succeeded; a failed write leaves the previous durable state and memory as they were.
 *
 * What it does NOT do: it never inserts a Nemesis. A record needs the enemy that finished the run, reliably identified, with its ability,
 * moves and held items; the app has none of that (the battle events carry no attacker, no move, no stable Pokemon id: contracts section
 * 1.8, items 1 and 2), so every commit passes no candidate and stores the report with an unknown cause. The tested store and
 * cc_commit_terminal do insert; the live insertion is a boundary C10 has to cross.
 *
 * A file that is there and cannot be used (damaged, made by another version, or one the SD card would not read) is never repaired,
 * overwritten or deleted by this module: the game goes on, nothing is stored, and cs_get_state says why. A file that is not there is a
 * new install: the empty state, written at the first run. A damaged main file with a good backup (save_read's fallback) is used from the
 * backup, and the next write replaces the damaged file, as save_write always does.
 *
 * Buffers are static (about 61 KB): the 3DS stack is 32 KB.
 */
#ifndef GM_CUSTOM_STORE_H
#define GM_CUSTOM_STORE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "cc/codec.h"
#include "cc/scheduler.h"
#include "combat_observer.h"
#include "save.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CS_FILE_NAME "custom"
#define CS_WHY_LEN 160

typedef enum {
  CS_NEW,         /* no file: the empty state, written when the first run starts */
  CS_LOADED,      /* read from the SD card (or written by this session) */
  CS_UNREADABLE,  /* a file that cannot be used: left exactly as it is, nothing is written */
} cs_state;

/* How the file was found at first use (reads it then). `why` (may be NULL) gets the reason for CS_UNREADABLE, in words for the player. */
cs_state cs_get_state(char why[CS_WHY_LEN]);
/* The state was read from the backup because the main file was damaged. */
bool cs_from_backup(void);
/* What is on the SD card as of the last successful read or write; NULL for CS_UNREADABLE. Never changes behind the caller's back. */
const cc_meta* cs_meta(void);

/* The high half of every run uid, chosen once for a new file (an existing file keeps its own). Call before the first use; never 0. */
void cs_set_nonce(uint32_t nonce);

/* A durable run uid, written to the SD card first. 0: none (the file is unreadable, or the write failed: nothing changed). */
cc_run_uid cs_begin_run(void);

typedef enum {
  CS_STORED,            /* the report is on the SD card */
  CS_REPLAYED,          /* this run was already stored: nothing was written */
  CS_NO_DURABLE_RUN,    /* the run had no durable uid (the file was unusable, or its write failed at the start): nothing stored */
  CS_FILE_UNUSABLE,     /* the custom file cannot be used: nothing stored, the file untouched */
  CS_BAD_REPORT,        /* the report does not fit the contract (a wave or biome out of range...): nothing stored */
  CS_WRITE_FAILED,      /* the SD card refused the write: the previous file and the memory state are as they were; a retry is safe */
} cs_outcome;

typedef struct {
  cs_outcome outcome;
  cc_status status;            /* CS_BAD_REPORT: what custom-core refused with, else CC_OK */
  cc_commit_result commit;     /* CS_STORED / CS_REPLAYED */
} cs_result;

/* Stores the finished run as one transaction. Safe to call again for the same report. */
cs_result cs_commit(const co_report* report);

/* A line for the player when something was not stored, in the result screen's words; NULL when the report is on the card. */
const char* cs_note(cs_outcome outcome);

/* ---- the once-per-record Nemesis reward (task C08: profile_tx.c is the only caller) ---- */
typedef enum {
  CS_CLAIM_OWED,          /* cs_reward_state: the reward is owed and can be claimed */
  CS_CLAIM_DONE,          /* cs_claim_reward: the claim is on the SD card now */
  CS_CLAIM_REPLAYED,      /* already claimed: nothing was written */
  CS_CLAIM_NOT_OWED,      /* the record was not won (or is not in a state that owes a reward) */
  CS_CLAIM_UNKNOWN,       /* no such record */
  CS_CLAIM_FILE_UNUSABLE, /* the custom file cannot be used: nothing written */
  CS_CLAIM_WRITE_FAILED,  /* the SD card refused the write: the file and memory are as they were; a retry is safe */
} cs_claim_outcome;
/* Reads only: CS_CLAIM_OWED, CS_CLAIM_REPLAYED, CS_CLAIM_NOT_OWED, CS_CLAIM_UNKNOWN or CS_CLAIM_FILE_UNUSABLE. */
cs_claim_outcome cs_reward_state(cc_record_id record_id);
/* Claims the record's reward as one transaction of the custom file (cc_reward_claim): the ledger entry and the record's REWARD_CLAIMED
 * in one write, idempotent by record. It grants nothing: the voucher is the profile transaction's (profile_tx.c). *txn gets the
 * claim's custom transaction id for DONE and REPLAYED (may be NULL). */
cs_claim_outcome cs_claim_reward(cc_record_id record_id, cc_txn_id* txn);

/* ---- for tests ---- */
typedef struct {
  save_result (*read)(const char* name, char** out, size_t* len, bool* used_backup);
  save_result (*write)(const char* name, const void* data, size_t len);
} cs_io;
void cs_set_io(const cs_io* io);   /* NULL: save_read / save_write */
void cs_reset(void);               /* forget what was read, so the next use reads the folder again */
size_t cs_static_bytes(void);      /* the static storage this module holds */

#ifdef __cplusplus
}
#endif
#endif
