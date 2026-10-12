/* custom-core: the version-1 codec of the persisted custom metadata (see cc/codec.h and section 2.6 of the contracts). */
#include <string.h>

#include "cc/codec.h"

/* ---- writer: little-endian, one field at a time ---- */

typedef struct {
    uint8_t *p;
    size_t cap, pos;
} wr;

static void w8(wr *w, uint8_t v)
{
    if (w->pos < w->cap) w->p[w->pos] = v;
    w->pos++;
}

static void w16(wr *w, uint16_t v)
{
    w8(w, (uint8_t)v);
    w8(w, (uint8_t)(v >> 8));
}

static void w32(wr *w, uint32_t v)
{
    w16(w, (uint16_t)v);
    w16(w, (uint16_t)(v >> 16));
}

static void w64(wr *w, uint64_t v)
{
    w32(w, (uint32_t)v);
    w32(w, (uint32_t)(v >> 32));
}

static void wi8(wr *w, int8_t v) { w8(w, (uint8_t)v); }
static void wi16(wr *w, int16_t v) { w16(w, (uint16_t)v); }
static void wi32(wr *w, int32_t v) { w32(w, (uint32_t)v); }

static void w_actor(wr *w, const cc_actor *a)
{
    w32(w, a->id);
    w16(w, a->species);
    w8(w, a->form);
    w8(w, a->side);
    w8(w, a->level);
}

static void w_event(wr *w, const cc_event *e)
{
    w32(w, e->seq);
    w8(w, e->kind);
    w8(w, e->target_side);
    w8(w, e->source_side);
    w8(w, e->target_form);
    w8(w, e->source_form);
    w8(w, e->result);
    w16(w, e->known);
    w16(w, e->target_species);
    w16(w, e->source_species);
    w16(w, e->move_id);
    w32(w, e->target_id);
    w32(w, e->source_id);
    wi32(w, e->shown);
    wi32(w, e->hp_delta);
    wi32(w, e->hp_after);
}

static void w_turn(wr *w, const cc_turn *t)
{
    w32(w, t->battle_seq);
    w16(w, t->wave);
    w16(w, t->turn);
    w16(w, t->dropped);
    w8(w, t->flags);
    w8(w, t->n_events);
    for (unsigned i = 0; i < t->n_events; i++) w_event(w, &t->events[i]);
}

static void w_terminal(wr *w, const cc_terminal *t)
{
    w64(w, t->run_uid);
    w64(w, t->txn_id);
    w8(w, t->outcome);
    w8(w, t->known);
    w8(w, t->biome);
    w16(w, t->wave);
    w16(w, t->finish_move);
    w_actor(w, &t->finish_actor);
    w_actor(w, &t->fallen_actor);
}

static void w_nemesis(wr *w, const cc_nemesis *n)
{
    w32(w, n->record_id);
    w8(w, n->life);
    w8(w, n->traits);
    w16(w, n->known);
    w64(w, n->source_run_uid);
    w32(w, n->source_pokemon_id);
    w16(w, n->source_wave);
    w8(w, n->source_biome);
    w16(w, n->species);
    w8(w, n->form);
    wi8(w, n->gender);
    w8(w, n->shiny);
    w8(w, n->orig_level);
    w8(w, n->nature);
    w16(w, n->ability);
    w16(w, n->passive);
    for (int i = 0; i < 6; i++) w8(w, n->ivs[i]);
    w8(w, n->n_moves);
    for (int i = 0; i < CC_MAX_MOVES; i++) w16(w, n->moves[i]);
    w8(w, n->n_items);
    for (int i = 0; i < CC_MAX_LOADOUT_ITEMS; i++) {
        for (int k = 0; k < CC_ITEM_KEY_BYTES; k++) w8(w, (uint8_t)n->items[i].key[k]);
        wi16(w, n->items[i].arg);
        w16(w, n->items[i].stack);
    }
    w16(w, n->finish_move);
    w16(w, n->fallen_species);
    w8(w, n->fallen_form);
    w64(w, n->sched_run_uid);
    w16(w, n->challenge_wave);
    w64(w, n->reward_txn);
}

cc_status cc_meta_encode(const cc_meta *m, uint8_t *buf, size_t cap, size_t *len)
{
    if (!m || !buf || !len) return CC_E_NULL;
    size_t want;
    cc_status s = cc_wire_meta_size(m, &want);          /* validates m */
    if (s != CC_OK) return s;
    if (want > cap || want > CC_META_BUDGET_BYTES) return CC_E_COUNT;

    wr w = {buf, cap, 0};
    w32(&w, CC_META_MAGIC);
    w16(&w, (uint16_t)CC_SCHEMA_VERSION);
    w16(&w, 0);
    w32(&w, (uint32_t)(want - CC_WIRE_ENVELOPE_HEADER));

    w32(&w, m->counters.next_record_id);
    w32(&w, m->counters.next_run_seq);
    w32(&w, m->counters.install_nonce);
    w64(&w, m->counters.next_txn_id);

    w8(&w, m->ledger.n);
    for (unsigned i = 0; i < m->ledger.n; i++) {
        const cc_ledger_entry *e = &m->ledger.e[i];
        w64(&w, e->txn_id);
        w64(&w, e->run_uid);
        w32(&w, e->record_id);
        w8(&w, e->kind);
        w8(&w, e->outcome);
    }

    w8(&w, m->roster.count);
    for (unsigned i = 0; i < m->roster.count; i++) w_nemesis(&w, &m->roster.rec[i]);

    w8(&w, m->has_report);
    if (m->has_report) {
        w_terminal(&w, &m->report);
        w8(&w, m->history.n_turns);
        w8(&w, m->history.flags);
        for (unsigned i = 0; i < m->history.n_turns; i++) w_turn(&w, &m->history.turns[i]);
    }
    if (w.pos != want) return CC_E_CONSISTENCY;         /* the size function and this writer disagree: never hand out the bytes */
    *len = want;
    return CC_OK;
}

/* ---- reader: bounds-checked, one field at a time ---- */

typedef struct {
    const uint8_t *p;
    size_t len, pos;
    bool bad;                    /* a read ran past the end */
} rd;

static uint8_t r8(rd *r)
{
    if (r->bad || r->pos >= r->len) {
        r->bad = true;
        return 0;
    }
    return r->p[r->pos++];
}

static uint16_t r16(rd *r)
{
    const uint16_t lo = r8(r);
    return (uint16_t)(lo | ((uint16_t)r8(r) << 8));
}

static uint32_t r32(rd *r)
{
    const uint32_t lo = r16(r);
    return lo | ((uint32_t)r16(r) << 16);
}

static uint64_t r64(rd *r)
{
    const uint64_t lo = r32(r);
    return lo | ((uint64_t)r32(r) << 32);
}

static int8_t ri8(rd *r) { return (int8_t)r8(r); }
static int16_t ri16(rd *r) { return (int16_t)r16(r); }
static int32_t ri32(rd *r) { return (int32_t)r32(r); }

static void r_actor(rd *r, cc_actor *a)
{
    a->id = r32(r);
    a->species = r16(r);
    a->form = r8(r);
    a->side = r8(r);
    a->level = r8(r);
}

static void r_event(rd *r, cc_event *e)
{
    e->seq = r32(r);
    e->kind = r8(r);
    e->target_side = r8(r);
    e->source_side = r8(r);
    e->target_form = r8(r);
    e->source_form = r8(r);
    e->result = r8(r);
    e->known = r16(r);
    e->target_species = r16(r);
    e->source_species = r16(r);
    e->move_id = r16(r);
    e->target_id = r32(r);
    e->source_id = r32(r);
    e->shown = ri32(r);
    e->hp_delta = ri32(r);
    e->hp_after = ri32(r);
}

static void r_terminal(rd *r, cc_terminal *t)
{
    t->run_uid = r64(r);
    t->txn_id = r64(r);
    t->outcome = r8(r);
    t->known = r8(r);
    t->biome = r8(r);
    t->wave = r16(r);
    t->finish_move = r16(r);
    r_actor(r, &t->finish_actor);
    r_actor(r, &t->fallen_actor);
}

static void r_nemesis(rd *r, cc_nemesis *n)
{
    n->record_id = r32(r);
    n->life = r8(r);
    n->traits = r8(r);
    n->known = r16(r);
    n->source_run_uid = r64(r);
    n->source_pokemon_id = r32(r);
    n->source_wave = r16(r);
    n->source_biome = r8(r);
    n->species = r16(r);
    n->form = r8(r);
    n->gender = ri8(r);
    n->shiny = r8(r);
    n->orig_level = r8(r);
    n->nature = r8(r);
    n->ability = r16(r);
    n->passive = r16(r);
    for (int i = 0; i < 6; i++) n->ivs[i] = r8(r);
    n->n_moves = r8(r);
    for (int i = 0; i < CC_MAX_MOVES; i++) n->moves[i] = r16(r);
    n->n_items = r8(r);
    for (int i = 0; i < CC_MAX_LOADOUT_ITEMS; i++) {
        for (int k = 0; k < CC_ITEM_KEY_BYTES; k++) n->items[i].key[k] = (char)r8(r);
        n->items[i].arg = ri16(r);
        n->items[i].stack = r16(r);
    }
    n->finish_move = r16(r);
    n->fallen_species = r16(r);
    n->fallen_form = r8(r);
    n->sched_run_uid = r64(r);
    n->challenge_wave = r16(r);
    n->reward_txn = r64(r);
}

static cc_status decode_body(rd *r, cc_meta *out)
{
    out->counters.next_record_id = r32(r);
    out->counters.next_run_seq = r32(r);
    out->counters.install_nonce = r32(r);
    out->counters.next_txn_id = r64(r);

    const unsigned n_ledger = r8(r);
    if (r->bad) return CC_E_FORMAT;
    if (n_ledger > CC_LEDGER_ENTRIES) return CC_E_COUNT;
    out->ledger.n = (uint8_t)n_ledger;
    for (unsigned i = 0; i < n_ledger; i++) {
        cc_ledger_entry *e = &out->ledger.e[i];
        e->txn_id = r64(r);
        e->run_uid = r64(r);
        e->record_id = r32(r);
        e->kind = r8(r);
        e->outcome = r8(r);
    }

    const unsigned n_records = r8(r);
    if (r->bad) return CC_E_FORMAT;
    if (n_records > CC_MAX_NEMESES) return CC_E_COUNT;
    out->roster.count = (uint8_t)n_records;
    for (unsigned i = 0; i < n_records; i++) r_nemesis(r, &out->roster.rec[i]);

    out->has_report = r8(r);
    if (r->bad) return CC_E_FORMAT;
    if (out->has_report > 1) return CC_E_RANGE;
    if (out->has_report) {
        r_terminal(r, &out->report);
        const unsigned n_turns = r8(r);
        out->history.flags = r8(r);
        if (r->bad) return CC_E_FORMAT;
        if (n_turns > CC_HISTORY_TURNS) return CC_E_COUNT;
        out->history.n_turns = (uint8_t)n_turns;
        for (unsigned i = 0; i < n_turns; i++) {
            cc_turn *t = &out->history.turns[i];
            t->battle_seq = r32(r);
            t->wave = r16(r);
            t->turn = r16(r);
            t->dropped = r16(r);
            t->flags = r8(r);
            const unsigned n_events = r8(r);
            if (r->bad) return CC_E_FORMAT;
            if (n_events > CC_MAX_EVENTS_PER_TURN) return CC_E_COUNT;
            t->n_events = (uint8_t)n_events;
            for (unsigned k = 0; k < n_events; k++) r_event(r, &t->events[k]);
        }
    }
    if (r->bad) return CC_E_FORMAT;                      /* a count promised more bytes than the body has */
    if (r->pos != r->len) return CC_E_FORMAT;            /* bytes left over after the last field */
    return CC_OK;
}

cc_status cc_meta_decode(const uint8_t *buf, size_t len, cc_meta *out)
{
    if (!out) return CC_E_NULL;
    memset(out, 0, sizeof *out);
    if (!buf) return CC_E_NULL;
    if (len < CC_WIRE_ENVELOPE_HEADER) return CC_E_FORMAT;

    rd h = {buf, CC_WIRE_ENVELOPE_HEADER, 0, false};
    const uint32_t magic = r32(&h);
    const uint16_t schema = r16(&h);
    const uint16_t reserved = r16(&h);
    const uint32_t body_len = r32(&h);
    if (magic != CC_META_MAGIC) return CC_E_FORMAT;
    if (schema != CC_SCHEMA_VERSION) return CC_E_VERSION;      /* before anything else is trusted: a later version may lay the rest out differently */
    if (reserved != 0) return CC_E_FORMAT;
    if (body_len != len - CC_WIRE_ENVELOPE_HEADER) return CC_E_FORMAT;
    if (len > CC_WIRE_META_MAX) return CC_E_FORMAT;            /* no valid blob is this long */

    rd r = {buf + CC_WIRE_ENVELOPE_HEADER, len - CC_WIRE_ENVELOPE_HEADER, 0, false};
    out->schema = CC_SCHEMA_VERSION;
    cc_status s = decode_body(&r, out);
    if (s == CC_OK) s = cc_meta_validate(out);
    if (s != CC_OK) memset(out, 0, sizeof *out);
    return s;
}
