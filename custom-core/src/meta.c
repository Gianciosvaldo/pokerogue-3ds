/* custom-core: the persisted metadata as a whole, and checked size arithmetic. */
#include <stdint.h>
#include <string.h>

#include "cc/model.h"

void cc_meta_init(cc_meta *m, uint32_t install_nonce)
{
    if (!m) return;
    memset(m, 0, sizeof *m);
    m->schema = CC_SCHEMA_VERSION;
    m->counters.next_record_id = 1;
    m->counters.next_run_seq = 1;
    m->counters.install_nonce = install_nonce;
    m->counters.next_txn_id = 1;
}

/* A run UID in the file must be one this install's counters have issued. */
#define uid_issued cc_run_uid_issued

cc_status cc_meta_validate(const cc_meta *m)
{
    if (!m) return CC_E_NULL;
    if (m->schema != CC_SCHEMA_VERSION) return CC_E_VERSION;
    cc_status s = cc_counters_validate(&m->counters);
    if (s != CC_OK) return s;
    s = cc_ledger_validate(&m->ledger);
    if (s != CC_OK) return s;
    s = cc_roster_validate(&m->roster);
    if (s != CC_OK) return s;

    const cc_counters *c = &m->counters;
    for (unsigned i = 0; i < m->roster.count; i++) {
        const cc_nemesis *n = &m->roster.rec[i];
        if (n->record_id >= c->next_record_id) return CC_E_CONSISTENCY;
        if (!uid_issued(c, n->source_run_uid)) return CC_E_CONSISTENCY;
        if (n->sched_run_uid && !uid_issued(c, n->sched_run_uid)) return CC_E_CONSISTENCY;
        if (n->reward_txn >= c->next_txn_id) return CC_E_CONSISTENCY;
    }
    for (unsigned i = 0; i < m->ledger.n; i++) {
        const cc_ledger_entry *e = &m->ledger.e[i];
        if (e->txn_id >= c->next_txn_id) return CC_E_CONSISTENCY;
        if (e->record_id >= c->next_record_id) return CC_E_CONSISTENCY;
        if (!uid_issued(c, e->run_uid)) return CC_E_CONSISTENCY;
    }

    if (m->has_report > 1) return CC_E_RANGE;
    if (m->has_report) {
        s = cc_terminal_validate(&m->report, true);
        if (s != CC_OK) return s;
        if (m->report.txn_id >= c->next_txn_id || !uid_issued(c, m->report.run_uid)) return CC_E_CONSISTENCY;
        return cc_history_validate(&m->history);
    }
    /* no report: the report and history hold nothing */
    const cc_terminal *t = &m->report;
    if (t->run_uid || t->txn_id || t->outcome || t->known || t->biome || t->wave || t->finish_move ||
        t->finish_actor.id || t->fallen_actor.id || m->history.n_turns || m->history.flags)
        return CC_E_NONCANONICAL;
    return CC_OK;
}

/* ---- sizes ---- */

cc_status cc_size_add(size_t a, size_t b, size_t *out)
{
    if (!out) return CC_E_NULL;
    if (a > SIZE_MAX - b) return CC_E_OVERFLOW;
    *out = a + b;
    return CC_OK;
}

cc_status cc_size_mul(size_t a, size_t b, size_t *out)
{
    if (!out) return CC_E_NULL;
    if (b != 0 && a > SIZE_MAX / b) return CC_E_OVERFLOW;
    *out = a * b;
    return CC_OK;
}

/* total += count * unit, checked */
static cc_status add_scaled(size_t *total, size_t count, size_t unit)
{
    size_t part;
    cc_status s = cc_size_mul(count, unit, &part);
    if (s != CC_OK) return s;
    return cc_size_add(*total, part, total);
}

cc_status cc_wire_meta_size_for(unsigned n_records, unsigned n_ledger, bool has_report, unsigned n_turns,
                                const unsigned *events_per_turn, size_t *out)
{
    if (!out) return CC_E_NULL;
    if (n_records > CC_MAX_NEMESES || n_ledger > CC_LEDGER_ENTRIES || n_turns > CC_HISTORY_TURNS) return CC_E_COUNT;
    if (!has_report && n_turns) return CC_E_CONSISTENCY;
    if (n_turns && !events_per_turn) return CC_E_NULL;
    size_t total = CC_WIRE_ENVELOPE_HEADER;
    cc_status s = add_scaled(&total, 1, CC_WIRE_COUNTERS);
    if (s == CC_OK) s = add_scaled(&total, 1, 1);                       /* n_ledger */
    if (s == CC_OK) s = add_scaled(&total, n_ledger, CC_WIRE_LEDGER_ENTRY);
    if (s == CC_OK) s = add_scaled(&total, 1, 1);                       /* n_records */
    if (s == CC_OK) s = add_scaled(&total, n_records, CC_WIRE_NEMESIS);
    if (s == CC_OK) s = add_scaled(&total, 1, 1);                       /* has_report */
    if (s == CC_OK && has_report) {
        s = add_scaled(&total, 1, CC_WIRE_TERMINAL);
        if (s == CC_OK) s = add_scaled(&total, 1, CC_WIRE_HISTORY_HEADER);
        for (unsigned i = 0; s == CC_OK && i < n_turns; i++) {
            if (events_per_turn[i] > CC_MAX_EVENTS_PER_TURN) return CC_E_COUNT;
            s = add_scaled(&total, 1, CC_WIRE_TURN_HEADER);
            if (s == CC_OK) s = add_scaled(&total, events_per_turn[i], CC_WIRE_EVENT);
        }
    }
    if (s != CC_OK) return s;
    *out = total;
    return CC_OK;
}

cc_status cc_wire_meta_size(const cc_meta *m, size_t *out)
{
    if (!m || !out) return CC_E_NULL;
    cc_status s = cc_meta_validate(m);
    if (s != CC_OK) return s;
    unsigned events[CC_HISTORY_TURNS] = {0};
    const unsigned n_turns = m->has_report ? m->history.n_turns : 0;
    for (unsigned i = 0; i < n_turns; i++) events[i] = m->history.turns[i].n_events;
    return cc_wire_meta_size_for(m->roster.count, m->ledger.n, m->has_report != 0, n_turns, events, out);
}
