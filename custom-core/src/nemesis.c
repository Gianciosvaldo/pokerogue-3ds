/* custom-core: Nemesis records, the roster, the insertion gate and the record lifecycle. */
#include <string.h>

#include "cc/model.h"

static bool is_window_wave(uint16_t wave)
{
    for (int i = 0; i < CC_NUM_WINDOWS; i++)
        if (cc_window_waves[i] == wave) return true;
    return false;
}

static bool item_is_zero(const cc_item *it)
{
    for (int i = 0; i < CC_ITEM_KEY_BYTES; i++)
        if (it->key[i]) return false;
    return !it->arg && !it->stack;
}

static cc_status item_validate(const cc_item *it)
{
    int len = 0;
    while (len < CC_ITEM_KEY_BYTES && it->key[len]) len++;
    if (len == 0 || len >= CC_ITEM_KEY_BYTES) return CC_E_RANGE;       /* empty, or no terminator */
    for (int i = 0; i < len; i++) {
        const char c = it->key[i];
        if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_')) return CC_E_RANGE;
    }
    for (int i = len; i < CC_ITEM_KEY_BYTES; i++)
        if (it->key[i]) return CC_E_NONCANONICAL;                      /* bytes after the terminator are zero */
    if (it->arg < -1) return CC_E_RANGE;
    if (it->stack < 1) return CC_E_RANGE;
    return CC_OK;
}

static cc_status lifecycle_fields(const cc_nemesis *n)
{
    switch (n->life) {
    case CC_LIFE_STORED:
        if (n->sched_run_uid || n->challenge_wave || n->reward_txn) return CC_E_CONSISTENCY;
        break;
    case CC_LIFE_SCHEDULED:
        if (!n->sched_run_uid || n->challenge_wave || n->reward_txn) return CC_E_CONSISTENCY;
        break;
    case CC_LIFE_CHALLENGED:
    case CC_LIFE_DEFEATED:
        if (!n->sched_run_uid || !is_window_wave(n->challenge_wave) || n->reward_txn) return CC_E_CONSISTENCY;
        break;
    case CC_LIFE_REWARD_CLAIMED:
        if (!n->sched_run_uid || !is_window_wave(n->challenge_wave) || !n->reward_txn) return CC_E_CONSISTENCY;
        break;
    default:
        return CC_E_RANGE;
    }
    return CC_OK;
}

cc_status cc_nemesis_validate(const cc_nemesis *n)
{
    if (!n) return CC_E_NULL;
    if (n->record_id == 0) return CC_E_RANGE;
    if (n->life == CC_LIFE_NONE || n->life >= CC_LIFE_COUNT_) return CC_E_RANGE;
    if (n->known & ~(uint16_t)CC_NK_ALL) return CC_E_UNKNOWN_BITS;
    const uint16_t k = n->known;
    if (n->source_run_uid == 0 || n->source_pokemon_id == 0 || n->source_wave < 1) return CC_E_RANGE;
    if (n->source_biome > CC_BIOME_ID_MAX) return CC_E_RANGE;
    if (n->species < 1 || n->species > CC_SPECIES_ID_MAX) return CC_E_RANGE;
    if (n->gender < -1 || n->gender > 1) return CC_E_RANGE;
    if (n->shiny > 1) return CC_E_RANGE;
    if (n->orig_level < 1) return CC_E_RANGE;

    if (k & CC_NK_NATURE) {
        if (n->nature > CC_NATURE_MAX) return CC_E_RANGE;
    } else if (n->nature) {
        return CC_E_NONCANONICAL;
    }
    if (k & CC_NK_ABILITY) {
        if (n->ability < 1 || n->ability > CC_ABILITY_ID_MAX) return CC_E_RANGE;
    } else if (n->ability) {
        return CC_E_NONCANONICAL;
    }
    if (k & CC_NK_PASSIVE) {
        if (n->passive > CC_ABILITY_ID_MAX) return CC_E_RANGE;
    } else if (n->passive) {
        return CC_E_NONCANONICAL;
    }
    for (int i = 0; i < 6; i++) {
        if (k & CC_NK_IVS) {
            if (n->ivs[i] > 31) return CC_E_RANGE;
        } else if (n->ivs[i]) {
            return CC_E_NONCANONICAL;
        }
    }
    if (k & CC_NK_MOVES) {
        if (n->n_moves > CC_MAX_MOVES) return CC_E_COUNT;
        if (n->n_moves < 1) return CC_E_RANGE;
        for (int i = 0; i < CC_MAX_MOVES; i++) {
            if (i < n->n_moves) {
                if (n->moves[i] < 1 || n->moves[i] > CC_MOVE_ID_MAX) return CC_E_RANGE;
                for (int j = 0; j < i; j++)
                    if (n->moves[j] == n->moves[i]) return CC_E_DUPLICATE;
            } else if (n->moves[i]) {
                return CC_E_NONCANONICAL;
            }
        }
    } else if (n->n_moves) {
        return CC_E_NONCANONICAL;
    } else {
        for (int i = 0; i < CC_MAX_MOVES; i++)
            if (n->moves[i]) return CC_E_NONCANONICAL;
    }
    if (k & CC_NK_LOADOUT) {
        if (n->n_items > CC_MAX_LOADOUT_ITEMS) return CC_E_COUNT;
        for (int i = 0; i < CC_MAX_LOADOUT_ITEMS; i++) {
            if (i < n->n_items) {
                cc_status s = item_validate(&n->items[i]);
                if (s != CC_OK) return s;
            } else if (!item_is_zero(&n->items[i])) {
                return CC_E_NONCANONICAL;
            }
        }
    } else {
        if (n->n_items) return CC_E_NONCANONICAL;
        for (int i = 0; i < CC_MAX_LOADOUT_ITEMS; i++)
            if (!item_is_zero(&n->items[i])) return CC_E_NONCANONICAL;
    }
    /* truncation means a full loadout was kept; an unknown loadout has n_items 0, so this also refuses it */
    if ((k & CC_NK_LOADOUT_TRUNCATED) && n->n_items != CC_MAX_LOADOUT_ITEMS) return CC_E_CONSISTENCY;
    if (k & CC_NK_FINISH_MOVE) {
        if (n->finish_move < 1 || n->finish_move > CC_MOVE_ID_MAX) return CC_E_RANGE;
    } else if (n->finish_move) {
        return CC_E_NONCANONICAL;
    }
    if (k & CC_NK_FALLEN) {
        if (n->fallen_species < 1 || n->fallen_species > CC_SPECIES_ID_MAX) return CC_E_RANGE;
    } else if (n->fallen_species || n->fallen_form) {
        return CC_E_NONCANONICAL;
    }
    return lifecycle_fields(n);
}

int cc_nemesis_support(const cc_nemesis *n)
{
    if (!n) return CC_REFUSE_NOT_ATTRIBUTED;
    if (n->traits & CC_TR_BOSS) return CC_REFUSE_BOSS;
    if (n->traits & CC_TR_FUSED) return CC_REFUSE_FUSED;
    if (n->traits & CC_TR_TERA) return CC_REFUSE_TERA;
    if (n->traits & CC_TR_DOUBLES) return CC_REFUSE_DOUBLES;
    if (n->traits & CC_TR_TRAINER_OWNED) return CC_REFUSE_TRAINER_OWNED;
    if (n->traits & CC_TR_MYSTERY) return CC_REFUSE_MYSTERY;
    if (n->traits & CC_TR_FIXED_SCRIPTED) return CC_REFUSE_FIXED_SCRIPTED;
    if (n->traits & CC_TR_REMATCH) return CC_REFUSE_REMATCH;
    if (!(n->known & CC_NK_FINISHER_VERIFIED)) return CC_REFUSE_NOT_ATTRIBUTED;
    if (!(n->known & CC_NK_ABILITY)) return CC_REFUSE_ABILITY_UNKNOWN;
    if (!(n->known & CC_NK_MOVES)) return CC_REFUSE_MOVES_UNKNOWN;
    return CC_SUPPORT_OK;
}

int cc_insertion_check(const cc_terminal *term, const cc_nemesis *cand)
{
    if (!term || !cand) return CC_REFUSE_NOT_ATTRIBUTED;
    if (term->outcome != CC_OUT_DEFEAT) return CC_REFUSE_NOT_DEFEAT;
    if (!(term->known & CC_TK_FINISH_ACTOR)) return CC_REFUSE_NOT_ATTRIBUTED;
    const cc_actor *f = &term->finish_actor;
    if (f->side != CC_SIDE_ENEMY || f->id != cand->source_pokemon_id || f->species != cand->species ||
        term->run_uid != cand->source_run_uid || term->wave != cand->source_wave || term->biome != cand->source_biome)
        return CC_REFUSE_NOT_ATTRIBUTED;
    return cc_nemesis_support(cand);
}

cc_status cc_nemesis_validate_stored(const cc_nemesis *n)
{
    cc_status s = cc_nemesis_validate(n);
    if (s != CC_OK) return s;
    return cc_nemesis_support(n) == CC_SUPPORT_OK ? CC_OK : CC_E_CONSISTENCY;
}

cc_status cc_roster_validate(const cc_roster *r)
{
    if (!r) return CC_E_NULL;
    if (r->count > CC_MAX_NEMESES) return CC_E_COUNT;
    int active = 0;
    for (unsigned i = 0; i < r->count; i++) {
        const cc_nemesis *n = &r->rec[i];
        cc_status s = cc_nemesis_validate_stored(n);
        if (s != CC_OK) return s;
        if (i > 0) {
            if (n->record_id == r->rec[i - 1].record_id) return CC_E_DUPLICATE;
            if (n->record_id < r->rec[i - 1].record_id) return CC_E_ORDER;
        }
        for (unsigned j = 0; j < i; j++)
            if (r->rec[j].source_run_uid == n->source_run_uid && r->rec[j].source_pokemon_id == n->source_pokemon_id)
                return CC_E_DUPLICATE;
        if (n->life == CC_LIFE_SCHEDULED || n->life == CC_LIFE_CHALLENGED) active++;
    }
    return active <= 1 ? CC_OK : CC_E_CONSISTENCY;
}

/* ---- lifecycle ---- */

bool cc_life_can_transition(uint8_t from, uint8_t to)
{
    static const bool ok[CC_LIFE_COUNT_][CC_LIFE_COUNT_] = {
        [CC_LIFE_STORED] = {[CC_LIFE_SCHEDULED] = true},
        [CC_LIFE_SCHEDULED] = {[CC_LIFE_STORED] = true, [CC_LIFE_CHALLENGED] = true},
        [CC_LIFE_CHALLENGED] = {[CC_LIFE_STORED] = true, [CC_LIFE_DEFEATED] = true},
        [CC_LIFE_DEFEATED] = {[CC_LIFE_REWARD_CLAIMED] = true},
    };
    if (from >= CC_LIFE_COUNT_ || to >= CC_LIFE_COUNT_) return false;
    return ok[from][to];
}

bool cc_life_overwritable(uint8_t life)
{
    return life == CC_LIFE_STORED || life == CC_LIFE_REWARD_CLAIMED;
}

cc_status cc_nemesis_transition(cc_nemesis *n, uint8_t to, cc_run_uid run_uid, uint16_t wave, cc_txn_id reward_txn)
{
    if (!n) return CC_E_NULL;
    cc_status s = cc_nemesis_validate(n);
    if (s != CC_OK) return s;
    if (!cc_life_can_transition(n->life, to)) return CC_E_LIFECYCLE;
    cc_nemesis c = *n;                         /* the change is built aside and checked, so an error changes nothing */
    switch (to) {
    case CC_LIFE_SCHEDULED:
        if (run_uid == 0) return CC_E_RANGE;
        c.sched_run_uid = run_uid;
        break;
    case CC_LIFE_CHALLENGED:
        if (!is_window_wave(wave)) return CC_E_RANGE;
        if (run_uid != n->sched_run_uid) return CC_E_CONSISTENCY;      /* only the run that scheduled it may accept it */
        c.challenge_wave = wave;
        break;
    case CC_LIFE_STORED:
        c.sched_run_uid = 0;
        c.challenge_wave = 0;
        break;
    case CC_LIFE_DEFEATED:
        break;
    case CC_LIFE_REWARD_CLAIMED:
        if (reward_txn == 0) return CC_E_RANGE;
        c.reward_txn = reward_txn;
        break;
    default:
        return CC_E_LIFECYCLE;
    }
    c.life = to;
    s = cc_nemesis_validate(&c);
    if (s != CC_OK) return s;
    *n = c;
    return CC_OK;
}
