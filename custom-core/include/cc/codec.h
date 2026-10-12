/* custom-core: the version-1 codec of the persisted custom metadata (docs/CUSTOM-CONTENT-CONTRACTS.md, section 2.6) and the
 * terminal transaction that changes it (section 2.10).
 *
 * Pure like the rest of the module: no globals, no I/O, no allocation, no clock. Every field is written and read one by one with
 * its stated width, little-endian; no struct is ever copied to or from the buffer. The caller owns the file: the envelope here is
 * the blob's own (magic, schema, body length); the save transport wraps it in its CRC. */
#ifndef CC_CODEC_H
#define CC_CODEC_H

#include "cc/model.h"

/* Writes `m` (which must pass cc_meta_validate) as one blob of exactly cc_wire_meta_size(m) bytes, at most CC_META_BUDGET_BYTES.
 * `cap` is the room in `buf`; a blob that does not fit returns CC_E_COUNT and writes nothing. *len is set only on success. */
cc_status cc_meta_encode(const cc_meta *m, uint8_t *buf, size_t cap, size_t *len);

/* Reads a blob. Strict: the magic, schema (CC_E_VERSION for any other than 1, newer or older), reserved field, body length (it
 * must be exactly the bytes present, and the body must use all of them), every count, every enum and "known" bit, and then every
 * cc_meta_validate rule (ordering, duplicates, canonical unknowns, ids below the counters, lifecycle bookkeeping). A blob that
 * decodes always re-encodes to the same bytes. On any error `*out` is set to all zero bytes (an invalid state, never a half-read
 * one) and the status says why: CC_E_FORMAT for bytes that are not a blob, CC_E_VERSION for a schema this build does not read,
 * otherwise the validator's status. The caller keeps the file it read, untouched. */
cc_status cc_meta_decode(const uint8_t *buf, size_t len, cc_meta *out);

/* ---- the terminal transaction ---- */

typedef struct {
    bool replayed;               /* this run's terminal transaction was already committed: nothing was changed */
    uint8_t outcome;             /* CC_TXO_INSERTED or CC_TXO_REPORT_ONLY (from the ledger on a replay) */
    int refusal;                 /* CC_SUPPORT_OK when inserted, else the CC_REFUSE_* that kept the record out; -1 on a replay
                                  * of a report-only commit (the ledger does not keep the reason) */
    cc_txn_id txn_id;            /* the transaction that stored the report */
    cc_record_id record_id;      /* the inserted record, else 0 */
    cc_record_id overwritten;    /* the record a full roster gave up for it, else 0 (never set on a replay) */
} cc_commit_result;

/* One run's end, as one transaction on `m` (which must pass cc_meta_validate; a stored state is not trusted).
 *   term   the run's terminal summary as a candidate: txn_id 0, run_uid one of m's issued UIDs, and
 *          cc_terminal_validate(term, false) OK. Any outcome is stored as the most recent report (an ended run's post-mortem);
 *          only a defeat can add a record.
 *   hist   the post-mortem's last turns (n_turns may be 0).
 *   cand   the defeating enemy as a roster record, or NULL when it is not reliably known (then the report is stored with an
 *          unknown cause and nothing else happens). cand->record_id must be 0 (the id is allocated here) and everything else
 *          must pass cc_nemesis_validate with life STORED. cc_insertion_check decides: when it does not return CC_SUPPORT_OK the
 *          report is still stored and `res->refusal` says why. At most one record is ever added.
 * Idempotent by run: a run UID already in the ledger returns CC_OK with res->replayed set and changes nothing, so a retried or
 * reopened end-of-run cannot add a second record or a second ledger entry. A new run stores the report, bumps the counters, and
 * appends the ledger entry in the same call. A full roster gives up its oldest record that cc_life_overwritable allows; none of
 * them does: the report is stored, CC_REFUSE_ROSTER_FULL, no record.
 * Atomic in memory: every check runs before the first write, so any error return leaves `m` exactly as it was. The caller makes
 * it atomic on disk by encoding `m` afterwards and keeping the previous bytes if that write fails. */
cc_status cc_commit_terminal(cc_meta *m, const cc_terminal *term, const cc_history *hist, const cc_nemesis *cand,
                             cc_commit_result *res);

#endif
