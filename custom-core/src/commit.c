/* custom-core: the terminal transaction (see cc/codec.h and section 2.10 of the contracts). */
#include <string.h>

#include "cc/codec.h"

/* The slot a full roster gives up: the oldest record (lowest record_id = first slot) that may be overwritten; -1 when none may. */
static int overwrite_slot(const cc_roster *r)
{
    for (unsigned i = 0; i < r->count; i++)
        if (cc_life_overwritable(r->rec[i].life)) return (int)i;
    return -1;
}

static bool roster_has_source(const cc_roster *r, const cc_nemesis *n)
{
    for (unsigned i = 0; i < r->count; i++)
        if (r->rec[i].source_run_uid == n->source_run_uid && r->rec[i].source_pokemon_id == n->source_pokemon_id) return true;
    return false;
}

cc_status cc_commit_terminal(cc_meta *m, const cc_terminal *term, const cc_history *hist, const cc_nemesis *cand,
                             cc_commit_result *res)
{
    if (!m || !term || !hist || !res) return CC_E_NULL;
    memset(res, 0, sizeof *res);

    /* ---- checks: nothing below this block may fail once the first write has happened ---- */
    cc_status s = cc_meta_validate(m);
    if (s != CC_OK) return s;
    s = cc_terminal_validate(term, false);
    if (s != CC_OK) return s;
    s = cc_history_validate(hist);
    if (s != CC_OK) return s;
    if (!cc_run_uid_issued(&m->counters, term->run_uid)) return CC_E_CONSISTENCY;

    cc_ledger_entry prior;
    if (cc_ledger_find(&m->ledger, CC_TXN_TERMINAL, term->run_uid, 0, &prior)) {
        res->replayed = true;
        res->outcome = prior.outcome;
        res->refusal = prior.outcome == CC_TXO_INSERTED ? CC_SUPPORT_OK : -1;
        res->txn_id = prior.txn_id;
        res->record_id = prior.record_id;
        return CC_OK;
    }

    cc_counters counters = m->counters;
    cc_txn_id txn_id;
    s = cc_alloc_txn_id(&counters, &txn_id);
    if (s != CC_OK) return s;

    int refusal = CC_REFUSE_NOT_ATTRIBUTED;                    /* no candidate: the finishing enemy is not known */
    int slot = -1;                                             /* the record given up, if the roster is full */
    cc_nemesis rec;
    memset(&rec, 0, sizeof rec);
    if (cand) {
        if (cand->record_id != 0) return CC_E_RANGE;           /* ids are the store's to give */
        rec = *cand;
        rec.record_id = counters.next_record_id;               /* the id cc_alloc_record_id hands out below, so what is checked is what is stored */
        s = cc_nemesis_validate(&rec);
        if (s != CC_OK) return s;
        if (rec.life != CC_LIFE_STORED) return CC_E_LIFECYCLE; /* a new record starts remembered, not scheduled */
        refusal = cc_insertion_check(term, &rec);
        if (refusal == CC_SUPPORT_OK) {
            if (roster_has_source(&m->roster, &rec)) return CC_E_DUPLICATE;      /* the same enemy of the same run twice */
            if (m->roster.count == CC_MAX_NEMESES) {
                slot = overwrite_slot(&m->roster);
                if (slot < 0) refusal = CC_REFUSE_ROSTER_FULL;
            }
        }
    }
    const bool insert = refusal == CC_SUPPORT_OK;
    cc_record_id record_id = 0;
    if (insert) {
        s = cc_alloc_record_id(&counters, &record_id);
        if (s != CC_OK) return s;
    }

    cc_ledger ledger = m->ledger;
    const cc_ledger_entry entry = {txn_id, term->run_uid, record_id, CC_TXN_TERMINAL,
                                   (uint8_t)(insert ? CC_TXO_INSERTED : CC_TXO_REPORT_ONLY)};
    s = cc_ledger_append(&ledger, &entry);
    if (s != CC_OK) return s;

    /* ---- writes ---- */
    m->counters = counters;
    m->ledger = ledger;
    if (insert) {
        if (m->roster.count == CC_MAX_NEMESES) {
            res->overwritten = m->roster.rec[slot].record_id;
            for (int i = slot + 1; i < CC_MAX_NEMESES; i++) m->roster.rec[i - 1] = m->roster.rec[i];
            m->roster.count--;
        }
        m->roster.rec[m->roster.count++] = rec;
    }
    m->has_report = 1;
    m->report = *term;
    m->report.txn_id = txn_id;
    m->history = *hist;

    res->outcome = entry.outcome;
    res->refusal = refusal;
    res->txn_id = txn_id;
    res->record_id = record_id;
    return CC_OK;
}
