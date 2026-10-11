/* custom-core: identifier counters and the transaction ledger. */
#include <stdint.h>

#include "cc/model.h"

cc_status cc_counters_validate(const cc_counters *c)
{
    if (!c) return CC_E_NULL;
    if (c->next_record_id < 1 || c->next_run_seq < 1 || c->next_txn_id < 1) return CC_E_RANGE;
    return CC_OK;
}

cc_status cc_alloc_record_id(cc_counters *c, cc_record_id *out)
{
    if (!c || !out) return CC_E_NULL;
    cc_status s = cc_counters_validate(c);
    if (s != CC_OK) return s;
    if (c->next_record_id == UINT32_MAX) return CC_E_OVERFLOW;         /* the last value stays unissued so next never wraps */
    *out = c->next_record_id++;
    return CC_OK;
}

cc_status cc_alloc_txn_id(cc_counters *c, cc_txn_id *out)
{
    if (!c || !out) return CC_E_NULL;
    cc_status s = cc_counters_validate(c);
    if (s != CC_OK) return s;
    if (c->next_txn_id == UINT64_MAX) return CC_E_OVERFLOW;
    *out = c->next_txn_id++;
    return CC_OK;
}

cc_status cc_alloc_run_uid(cc_counters *c, cc_run_uid *out)
{
    if (!c || !out) return CC_E_NULL;
    cc_status s = cc_counters_validate(c);
    if (s != CC_OK) return s;
    if (c->next_run_seq == UINT32_MAX) return CC_E_OVERFLOW;
    *out = ((cc_run_uid)c->install_nonce << 32) | c->next_run_seq;
    c->next_run_seq++;
    return CC_OK;
}

static cc_status entry_validate(const cc_ledger_entry *e)
{
    if (e->txn_id == 0 || e->run_uid == 0) return CC_E_RANGE;
    switch (e->kind) {
    case CC_TXN_TERMINAL:
        if (e->outcome == CC_TXO_INSERTED) return e->record_id ? CC_OK : CC_E_RANGE;
        if (e->outcome == CC_TXO_REPORT_ONLY) return e->record_id ? CC_E_NONCANONICAL : CC_OK;
        return CC_E_RANGE;
    case CC_TXN_REWARD_CLAIM:
        if (e->outcome != CC_TXO_CLAIMED) return CC_E_RANGE;
        return e->record_id ? CC_OK : CC_E_RANGE;
    default:
        return CC_E_RANGE;
    }
}

/* The idempotency key: a terminal transaction by run, a reward claim by record. */
static bool same_key(const cc_ledger_entry *a, const cc_ledger_entry *b)
{
    if (a->kind != b->kind) return false;
    return a->kind == CC_TXN_TERMINAL ? a->run_uid == b->run_uid : a->record_id == b->record_id;
}

cc_status cc_ledger_validate(const cc_ledger *l)
{
    if (!l) return CC_E_NULL;
    if (l->n > CC_LEDGER_ENTRIES) return CC_E_COUNT;
    for (unsigned i = 0; i < l->n; i++) {
        cc_status s = entry_validate(&l->e[i]);
        if (s != CC_OK) return s;
        if (i > 0) {
            if (l->e[i].txn_id == l->e[i - 1].txn_id) return CC_E_DUPLICATE;
            if (l->e[i].txn_id < l->e[i - 1].txn_id) return CC_E_ORDER;
        }
        for (unsigned j = 0; j < i; j++)
            if (same_key(&l->e[j], &l->e[i])) return CC_E_DUPLICATE;
    }
    return CC_OK;
}

bool cc_ledger_find(const cc_ledger *l, uint8_t kind, cc_run_uid run_uid, cc_record_id record_id, cc_ledger_entry *out)
{
    if (!l) return false;
    for (unsigned i = 0; i < l->n && i < CC_LEDGER_ENTRIES; i++) {
        const cc_ledger_entry *e = &l->e[i];
        if (e->kind != kind) continue;
        if (kind == CC_TXN_TERMINAL ? e->run_uid == run_uid : e->record_id == record_id) {
            if (out) *out = *e;
            return true;
        }
    }
    return false;
}

cc_status cc_ledger_append(cc_ledger *l, const cc_ledger_entry *e)
{
    if (!l || !e) return CC_E_NULL;
    cc_status s = cc_ledger_validate(l);
    if (s != CC_OK) return s;
    s = entry_validate(e);
    if (s != CC_OK) return s;
    if (l->n > 0 && e->txn_id <= l->e[l->n - 1].txn_id) return CC_E_ORDER;
    for (unsigned i = 0; i < l->n; i++)
        if (same_key(&l->e[i], e)) return CC_E_DUPLICATE;
    if (l->n == CC_LEDGER_ENTRIES) {
        for (unsigned i = 1; i < CC_LEDGER_ENTRIES; i++) l->e[i - 1] = l->e[i];
        l->n--;
    }
    l->e[l->n++] = *e;
    return CC_OK;
}
