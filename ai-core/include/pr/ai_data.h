/* What the enemy AI reads from upstream's move objects beyond battle-core's tables, dumped by
 * gen/dump-ai.test.ts into gen/out/ai_data.c. */
#ifndef PR_AI_DATA_H
#define PR_AI_DATA_H
#include <stdbool.h>
#include <stdint.h>

/* The class whose getUserBenefitScore / getTargetBenefitScore an attribute runs (its own, or the
 * nearest ancestor's), in the dumper's OWNERS order */
enum {
    AI_OWN_MoveAttr, AI_OWN_HighCritAttr, AI_OWN_CritOnlyAttr, AI_OWN_TargetHalfHpDamageAttr,
    AI_OWN_SurviveDamageAttr, AI_OWN_RecoilAttr, AI_OWN_SacrificialAttr, AI_OWN_SacrificialAttrOnHit,
    AI_OWN_HalfSacrificialAttr, AI_OWN_AddSubstituteAttr, AI_OWN_HealAttr, AI_OWN_FlameBurstAttr,
    AI_OWN_SacrificialFullRestoreAttr, AI_OWN_HitHealAttr, AI_OWN_MultiHitAttr, AI_OWN_StatusEffectAttr,
    AI_OWN_MultiStatusEffectAttr, AI_OWN_PsychoShiftEffectAttr, AI_OWN_StealHeldItemChanceAttr,
    AI_OWN_RemoveHeldItemAttr, AI_OWN_HealStatusEffectAttr, AI_OWN_BypassSleepAttr, AI_OWN_TerrainChangeAttr,
    AI_OWN_StatStageChangeAttr, AI_OWN_PartingShotAttr, AI_OWN_AddBattlerTagAttr, AI_OWN_GulpMissileTagAttr,
    AI_OWN_HitsTagAttr, AI_OWN_RevivalBlessingAttr, AI_OWN_ForceSwitchOutAttr, AI_OWN_RepeatMoveAttr,
    AI_OWN_ReducePpMoveAttr, AI_OWN_ShiftStatAttr,
    AI_OWN_COUNT_,
    AI_OWN_UNKNOWN = 255
};
/* StatStageChangeAttr.getLevels owner */
enum { AI_LEVELS_BASE, AI_LEVELS_GROWTH };
/* ai_move_info.restrictions: Move.checkRestrictions entries */
enum {
    AI_RESTR_GRAVITY = 1,        /* gravityUseRestriction */
    AI_RESTR_CONSECUTIVE = 2,    /* consecutiveUseRestriction (Gigaton Hammer, Blood Moon) */
    AI_RESTR_BELCH = 4,          /* no berry eaten this battle */
    AI_RESTR_NO_BERRY = 8,       /* Stuff Cheeks: the side holds no berry */
    AI_RESTR_UNKNOWN = 128
};

typedef struct {
    const char *cls;             /* the attribute's class (checked against battle-core's pr_attrs) */
    uint8_t ubs, tbs;            /* AI_OWN_* */
    uint8_t levels;              /* AI_LEVELS_* (StatStageChangeAttr and subclasses) */
    bool self_target;            /* MoveAttr.selfTarget */
    int8_t stages;               /* StatStageChangeAttr.stages */
    int8_t stats[8];             /* StatStageChangeAttr.stats */
    uint8_t n_stats;
    int8_t effects[4];           /* StatusEffectAttr.effect, or MultiStatusEffectAttr.effects */
    uint8_t n_effects;
    int8_t tag_score;            /* AddBattlerTagAttr.getTagTargetBenefitScore() */
    int16_t tag_type;            /* HitsTagAttr / AddBattlerTagAttr.tagType, -1 = none */
    bool double_damage;          /* HitsTagAttr.doubleDamage */
    bool self_switch;            /* ForceSwitchOutAttr.selfSwitch (PartingShotAttr: its switchAttr's) */
    uint8_t switch_type;         /* ForceSwitchOutAttr.switchType (SwitchType) */
    double heal_ratio;           /* HealAttr.healRatio as constructed (VariableHealAttr: 1 until used) */
    int8_t heal_stat;            /* HitHealAttr.healStat, -1 = null */
    int8_t stat_to_switch, stat_to_switch_with;   /* ShiftStatAttr */
} ai_attr_info;

typedef struct {
    double eff_power;            /* Move.calculateEffectivePower() (no Pokemon) */
    uint8_t restrictions;        /* AI_RESTR_* bits */
    uint8_t first_move_conds;    /* FirstMoveCondition entries in move.conditions; 128 = an unknown owner */
    uint16_t attr0;              /* slice of ai_attrs, in the order of the move's attrs */
    uint8_t nattr;
} ai_move_info;

extern const ai_attr_info ai_attrs[];
extern const ai_move_info ai_moves[];
extern const int ai_move_count;
extern const char *const ai_unknown_notes;   /* what the dumper didn't recognise ("" when nothing) */

#endif
