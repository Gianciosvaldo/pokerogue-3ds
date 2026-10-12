/* custom-core: status and refusal text, and the rematch window waves. */
#include "cc/model.h"

const uint16_t cc_window_waves[CC_NUM_WINDOWS] = {27, 77, 127, 177};

const char *cc_status_str(cc_status s)
{
    switch (s) {
    case CC_OK: return "ok";
    case CC_E_NULL: return "null argument";
    case CC_E_RANGE: return "value out of range";
    case CC_E_COUNT: return "count over its limit";
    case CC_E_UNKNOWN_BITS: return "undefined flag bits";
    case CC_E_NONCANONICAL: return "value stored for an unknown field";
    case CC_E_ORDER: return "out of order";
    case CC_E_DUPLICATE: return "duplicate";
    case CC_E_CONSISTENCY: return "inconsistent fields";
    case CC_E_LIFECYCLE: return "forbidden lifecycle transition";
    case CC_E_OVERFLOW: return "overflow or exhausted identifier space";
    case CC_E_VERSION: return "unsupported schema version";
    case CC_E_FORMAT: return "not a custom metadata blob";
    }
    return "unknown status";
}

const char *cc_refusal_str(int r)
{
    switch (r) {
    case CC_SUPPORT_OK: return "supported";
    case CC_REFUSE_NOT_DEFEAT: return "the run did not end in a defeat";
    case CC_REFUSE_NOT_ATTRIBUTED: return "the finishing source is not reliably this enemy";
    case CC_REFUSE_BOSS: return "boss enemy";
    case CC_REFUSE_FUSED: return "fused enemy";
    case CC_REFUSE_TERA: return "Terastallized enemy";
    case CC_REFUSE_DOUBLES: return "double battle";
    case CC_REFUSE_TRAINER_OWNED: return "trainer-owned enemy";
    case CC_REFUSE_MYSTERY: return "mystery encounter enemy";
    case CC_REFUSE_FIXED_SCRIPTED: return "fixed or scripted encounter";
    case CC_REFUSE_REMATCH: return "itself a rematch enemy";
    case CC_REFUSE_ABILITY_UNKNOWN: return "ability not known";
    case CC_REFUSE_MOVES_UNKNOWN: return "moves not known";
    case CC_REFUSE_ROSTER_FULL: return "the roster is full and none of its records may be overwritten";
    }
    return "unknown refusal";
}
