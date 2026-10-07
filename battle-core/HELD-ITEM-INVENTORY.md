# Pinned held-item dispatch inventory (Card 104)

Source target: `8555c08c823b856cbec4eb99ca84ea52a955836d`,
`src/modifier/modifier.ts`, inspected 2026-10-07. This lists every transitive
subclass of `PokemonHeldItemModifier`: 33 classes, comprising 31 concrete
classes and two abstract bases. Enemy tokens and persistent side-wide modifiers
are not held-item subclasses and are outside this inventory.

The C class registry in `src/modifiers.c` is distinct from the conservative
`pr_mod_is_ported_item` turn-path whitelist. An implemented stat/damage helper
alone does not establish full-turn parity; absence from that whitelist alone
does not establish missing behavior. Rows marked "helper" remain conservatively
unported in turns and need their own bounded turn fixtures before promotion.
This task adds only the two-orb family; it does not complete Card 104.

| Pinned class | C mapping / actual dispatch | Remaining scope |
| --- | --- | --- |
| `LapsingPokemonHeldItemModifier` (abstract) | No C class | Lapsing held-item lifecycle; no concrete subclass at this pin |
| `BaseStatModifier` | `PR_MOD_BASE_STAT`; `pr_apply_base_stat_modifiers` helper | Full-turn fixtures/whitelist; vitamin stats themselves already implemented |
| `EvoTrackerModifier` | No C class | Evolution tracker/persistent treasure lifecycle; upstream apply itself is a no-op returning true |
| `PokemonBaseStatTotalModifier` | `PR_MOD_BASE_STAT_TOTAL`; base-stat helper | Full-turn fixtures/whitelist |
| `PokemonBaseStatFlatModifier` | `PR_MOD_BASE_STAT_FLAT`; base-stat helper | Full-turn fixtures/whitelist |
| `PokemonIncrementingStatModifier` | `PR_MOD_INCREMENTING_STAT`; `pr_apply_incrementing_stat_modifier` helper | Full-turn fixtures/whitelist |
| `StatBoosterModifier` | `PR_MOD_STAT_BOOSTER`; `pr_apply_stat_booster_modifiers` helper | Full-turn fixtures/whitelist |
| `EvolutionStatBoosterModifier` | `PR_MOD_EVOLUTION_STAT_BOOSTER`; stat helper includes evolution check | Full-turn fixtures/whitelist |
| `SpeciesStatBoosterModifier` | `PR_MOD_SPECIES_STAT_BOOSTER`; stat helper includes species check | Full-turn fixtures/whitelist |
| `CritBoosterModifier` | `PR_MOD_CRIT_BOOSTER`; `pr_apply_crit_booster_modifiers` helper | Full-turn fixtures/whitelist |
| `SpeciesCritBoosterModifier` | `PR_MOD_SPECIES_CRIT_BOOSTER`; crit helper includes species check | Full-turn fixtures/whitelist |
| `AttackTypeBoosterModifier` | `PR_MOD_ATTACK_TYPE_BOOSTER`; damage helper | Full-turn fixtures/whitelist |
| `SurviveDamageModifier` | `PR_MOD_SURVIVE_DAMAGE`; fatal-hit chance dispatch | Existing Focus Band scope; broader item lifecycle/identities not implied |
| `BypassSpeedChanceModifier` | `PR_MOD_BYPASS_SPEED_CHANCE`; turn-order chance dispatch | Existing Quick Claw scope |
| `FlinchChanceModifier` | `PR_MOD_FLINCH_CHANCE`; post-hit chance dispatch | Existing King's Rock scope |
| `TurnHealModifier` | `PR_MOD_TURN_HEAL`; TurnEnd heal dispatch | Existing Leftovers scope |
| `TurnStatusEffectModifier` | **Added:** `PR_MOD_TURN_STATUS_EFFECT`; TurnEnd self-status dispatch | Toxic/Flame Orb only; reward acquisition and run inventory bridge excluded |
| `HitHealModifier` | `PR_MOD_HIT_HEAL`; damage-dependent recovery dispatch | Existing Shell Bell scope |
| `BerryModifier` | `PR_MOD_BERRY`; trigger/consume dispatch for all eleven pinned berries | Existing bounded berry suites; lifecycle outside their fixtures not implied |
| `PokemonInstantReviveModifier` | `PR_MOD_INSTANT_REVIVE`; FaintPhase early return, queued revival and item consumption | Added bounded singles Reviver Seed; Commander/doubles and acquisition/save integration remain outside this slice |
| `ResetNegativeStatStageModifier` | `PR_MOD_RESET_NEGATIVE_STAT_STAGE`; adverse-stage reset dispatch | Existing White Herb scope |
| `FieldEffectModifier` | `PR_MOD_FIELD_EFFECT`; field-duration helper used by weather/terrain changes | Full-turn fixtures/whitelist |
| `PokemonExpBoosterModifier` | No C class in battle-core | Held Lucky/Golden Egg experience dispatch; side-wide Exp. Charm helper is a different class |
| `PokemonFriendshipBoosterModifier` | No C class in battle-core | Held Soothe Bell friendship dispatch |
| `PokemonNatureWeightModifier` | `PR_MOD_NATURE_WEIGHT`; nature-stat helper | Full-turn fixtures/whitelist |
| `PokemonMoveAccuracyBoosterModifier` | `PR_MOD_MOVE_ACCURACY_BOOSTER`; battle-accuracy dispatch | Added bounded Wide Lens amount=5, stacks 1–3 including virtual stacks; other identities remain unported |
| `PokemonMultiHitModifier` | `PR_MOD_MULTI_HIT`; damage scaling/held-stack helpers exist | Missing turn hit-count dispatch; not just a whitelist omission |
| `PokemonFormChangeItemModifier` | No C class | Held form-change activation/lifecycle; supported species form changes are separate |
| `DamageMoneyRewardModifier` | No C class; post-damage path reports turn modifiers | Missing Golden Punch money reward dispatch |
| `SwitchEffectTransferModifier` | No C class; switching explicitly leaves held Baton transfer unported | Held Baton ownership transfer; Baton Pass move/tag transfer is already implemented separately |
| `HeldItemTransferModifier` (abstract) | No C class | General stealing base; bounded Knock Off/Incinerate removal is a separate move attribute |
| `TurnHeldItemTransferModifier` | No C class; TurnEnd reports turn modifiers | Missing Mini Black Hole end-turn theft |
| `ContactHeldItemTransferChanceModifier` | No C class; post-hit path reports turn modifiers | Missing Grip Claw on-hit theft |

The chosen family was genuinely absent before this change: neither a C modifier
class nor a TurnEnd status application existed. Upstream applies the orb after
PostTurn ability dispatch, outside its `switchOutStatus` guard, using the holder
as source, the orb name as `sourceText`, and `trySetStatus`'s default `quiet=true`.
The new C dispatch uses the existing status/ability/queue implementation and
preserves that position. It consumes no item, changes no stack or owner, and
makes no item RNG draw. Unknown subclasses and unsupported status identities
continue to report unported.

`PR_MOD_TURN_STATUS_EFFECT` is appended after all existing modifier IDs. The
`pr_modifier.status_effect` field carries TOXIC/BURN identity explicitly. Struct
layout changes require dependent C consumers to rebuild; no existing enum ID,
phase ID, generated table, application save schema or bridge is changed.

The later Wide Lens slice adds the actual missing battle-accuracy dispatch. It
adds five accuracy points per effective stack before Fog and Gravity flooring,
while preserving OHKO and accuracy=-1 rejection. The new ID is appended after
the orb ID; the existing `num` field holds the bounded identity amount of five.
Neither promotion implies reward acquisition, save bridging, removal or theft
parity for those identities.

The Reviver Seed slice appends `PR_MOD_INSTANT_REVIVE` after Wide Lens without
changing existing IDs or modifier fields. Its FaintPhase path follows pinned
upstream's `preventInstantRevive` gate, immediate status reset, first matching
item consumption and early return. Direct-hit knockouts can revive; indirect
self-KOs cannot. The scope is supported singles fixtures and stat-neutral
adjacent Leftovers; promotion does not establish acquisition, inventory/save,
Commander or general doubles parity.
