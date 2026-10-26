# Current pinned held-item dispatch inventory

Reconciled on 2026-10-26. Source: `8555c08c823b856cbec4eb99ca84ea52a955836d`,
`src/modifier/modifier.ts`. The inventory has **33 transitive subclasses**:
**31 concrete classes and two abstract bases**. All 31 concrete names have C
registry mappings. This is a class/owner-reference inventory, **not a semantic
completion count**. It does not establish every input shape or interaction.

The [previous Card 104 snapshot](../docs/history/HELD-ITEM-INVENTORY-2026-10-26.md)
is historical. Its missing-class/helper-only rows have been superseded by the
proofs below. In particular, Baton, Grip Claw, Mini Black Hole, held EXP and
friendship classes are implemented. Form-item acquisition/party actions and the
Macho Brace stat/Victory/capture owners also have their own merged evidence.

The [source capture](inventory/held-classes.source.json) records class headers,
parents, abstract status and the pinned file hash. The [reviewed evidence
ledger](inventory/held-evidence.json) links actual owner references and bounded
proofs. The audit checks C registry names, inherited held-class relationships,
owner symbols, reference files and this rendered table. File/symbol existence
is not a substitute for the native comparisons described in each proof.

| Pinned class | C mapping and owner reference | Bounded evidence and remaining scope |
| --- | --- | --- |
| `LapsingPokemonHeldItemModifier` | Abstract; no standalone C class | Abstract; no concrete descendants at this pin. BattleEnd lapsing-held dispatch has no enabled instance. |
| `BaseStatModifier` | `PR_MOD_BASE_STAT`; [pr_apply_base_stat_after_fusion](src/pokemon.c) | [STAT-ORDER](STAT-ORDER-VALIDATION.md); [VITAMIN](../game-core/VITAMIN-VALIDATION.md). IV-capped canonical vitamins; generic constructor values and all mixed histories are not implied. |
| `EvoTrackerModifier` | `PR_MOD_EVO_TRACKER`; [result.tracker_stacks](../game-core/src/gc_reward.c) | [EVO-TRACKER](../game-core/EVO-TRACKER-VALIDATION.md). Canonical Gimmighoul acquisition and primary evolution; general fusion evolution remains open. |
| `PokemonBaseStatTotalModifier` | `PR_MOD_BASE_STAT_TOTAL`; [pr_apply_base_stat_before_fusion](src/pokemon.c) | [STAT-ORDER](STAT-ORDER-VALIDATION.md). Shuckle Juice 10/-15 or Old Gateau, before fusion/halving; mystery event selection is a separate owner. |
| `PokemonBaseStatFlatModifier` | `PR_MOD_BASE_STAT_FLAT`; [pr_apply_base_stat_before_fusion](src/pokemon.c) | [STAT-ORDER](STAT-ORDER-VALIDATION.md). Shuckle Juice 10/-15 or Old Gateau, before fusion/halving; mystery event selection is a separate owner. |
| `PokemonIncrementingStatModifier` | `PR_MOD_INCREMENTING_STAT`; [pr_apply_incrementing_stat_modifier](src/pokemon.c); [MYSTERY_ENCOUNTER_MACHO_BRACE](../game-core/src/gc_phases.c) | [MACHO-BRACE](../game-core/MACHO-BRACE-VALIDATION.md). Canonical Brace stat, Victory/capture and save owners; Winstrate reward chain and arbitrary cleanup histories are separate. |
| `StatBoosterModifier` | `PR_MOD_STAT_BOOSTER`; [pr_apply_stat_booster_modifiers](src/pokemon.c) | [STAT-ITEM](STAT-ITEM-VALIDATION.md); [STAT-LOOT](STAT-LOOT-VALIDATION.md); [FUSED-HELD](FUSED-HELD-VALIDATION.md). Original Eviolite and six species boosters, transferred base-class clones and fused eligibility; generic constructor parameters retain guards. |
| `EvolutionStatBoosterModifier` | `PR_MOD_EVOLUTION_STAT_BOOSTER`; [pr_apply_stat_booster_modifiers](src/pokemon.c) | [STAT-ITEM](STAT-ITEM-VALIDATION.md); [STAT-LOOT](STAT-LOOT-VALIDATION.md); [FUSED-HELD](FUSED-HELD-VALIDATION.md). Original Eviolite and six species boosters, transferred base-class clones and fused eligibility; generic constructor parameters retain guards. |
| `SpeciesStatBoosterModifier` | `PR_MOD_SPECIES_STAT_BOOSTER`; [pr_apply_stat_booster_modifiers](src/pokemon.c) | [STAT-ITEM](STAT-ITEM-VALIDATION.md); [STAT-LOOT](STAT-LOOT-VALIDATION.md); [FUSED-HELD](FUSED-HELD-VALIDATION.md). Original Eviolite and six species boosters, transferred base-class clones and fused eligibility; generic constructor parameters retain guards. |
| `CritBoosterModifier` | `PR_MOD_CRIT_BOOSTER`; [pr_project_crit_booster_modifiers](src/damage.c) | [CRITICAL](CRITICAL-VALIDATION.md); [FUSED-HELD](FUSED-HELD-VALIDATION.md). Scope Lens and original Leek including fused species; arbitrary raw stat/species identities are outside those proofs. |
| `SpeciesCritBoosterModifier` | `PR_MOD_SPECIES_CRIT_BOOSTER`; [pr_project_crit_booster_modifiers](src/damage.c) | [CRITICAL](CRITICAL-VALIDATION.md); [FUSED-HELD](FUSED-HELD-VALIDATION.md). Scope Lens and original Leek including fused species; arbitrary raw stat/species identities are outside those proofs. |
| `AttackTypeBoosterModifier` | `PR_MOD_ATTACK_TYPE_BOOSTER`; [pr_project_attack_type_booster_modifiers](src/damage.c) | [attack-type.producer.test](tests/attack-type.producer.test.ts); [PICKUP](PICKUP-VALIDATION.md). Eighteen canonical types, 20% and cap99; source base-move type rule, not arbitrary multipliers. |
| `SurviveDamageModifier` | `PR_MOD_SURVIVE_DAMAGE`; [pr_apply_survive_damage_modifiers](src/phases.c) | [turntest.producer.test](tests/turntest.producer.test.ts); [ITEM-TRANSFER](ITEM-TRANSFER-VALIDATION.md); [SCENE-TRANSFER](SCENE-TRANSFER-VALIDATION.md). Focus Band fatal-hit chance; bounded singles item-turn mode and named stock/transfer proofs, not every interaction. |
| `BypassSpeedChanceModifier` | `PR_MOD_BYPASS_SPEED_CHANCE`; [pr_apply_bypass_speed_chance_modifiers](src/phases.c) | [turntest.producer.test](tests/turntest.producer.test.ts); [ITEM-TRANSFER](ITEM-TRANSFER-VALIDATION.md); [SCENE-TRANSFER](SCENE-TRANSFER-VALIDATION.md). Quick Claw turn-start chance; bounded singles item-turn mode and named stock/transfer proofs, not every interaction. |
| `FlinchChanceModifier` | `PR_MOD_FLINCH_CHANCE`; [pr_apply_flinch_chance_modifiers](src/phases.c) | [turntest.producer.test](tests/turntest.producer.test.ts); [ITEM-TRANSFER](ITEM-TRANSFER-VALIDATION.md); [SCENE-TRANSFER](SCENE-TRANSFER-VALIDATION.md). King's Rock post-hit chance; bounded singles item-turn mode and named stock/transfer proofs, not every interaction. |
| `TurnHealModifier` | `PR_MOD_TURN_HEAL`; [pr_apply_turn_heal_modifiers](src/phases.c) | [turntest.producer.test](tests/turntest.producer.test.ts); [ITEM-TRANSFER](ITEM-TRANSFER-VALIDATION.md); [SCENE-TRANSFER](SCENE-TRANSFER-VALIDATION.md). Leftovers turn-end healing; bounded singles item-turn mode and named stock/transfer proofs, not every interaction. |
| `TurnStatusEffectModifier` | `PR_MOD_TURN_STATUS_EFFECT`; [pr_apply_turn_status_modifiers](src/phases.c) | [status-orb.producer.test](tests/status-orb.producer.test.ts); [ORB](../game-core/ORB-VALIDATION.md); [ORB-ACQUISITION](../game-core/ORB-ACQUISITION-VALIDATION.md). Flame/Toxic Orb order, self-status and canonical acquisition; broader mixed inventories remain separate. |
| `HitHealModifier` | `PR_MOD_HIT_HEAL`; [pr_apply_hit_heal_modifiers](src/phases.c) | [turntest.producer.test](tests/turntest.producer.test.ts); [ITEM-TRANSFER](ITEM-TRANSFER-VALIDATION.md); [SCENE-TRANSFER](SCENE-TRANSFER-VALIDATION.md). Shell Bell move-end healing; bounded singles item-turn mode and named stock/transfer proofs, not every interaction. |
| `BerryModifier` | `PR_MOD_BERRY`; [pr_apply_berry_ex](src/phases.c) | [BERRY-LIFECYCLE](BERRY-LIFECYCLE-VALIDATION.md); [BERRY-MOVES](BERRY-MOVES-VALIDATION.md); [RECEIVED-HISTORY](RECEIVED-HISTORY-VALIDATION.md). All eleven pinned berry identities; Harvest/Cud Chew histories, berry moves and Enigma received-hit owner have separate proofs. |
| `PokemonInstantReviveModifier` | `PR_MOD_INSTANT_REVIVE`; [PR_MOD_INSTANT_REVIVE](src/phases.c) | [reviver-seed.producer.test](tests/reviver-seed.producer.test.ts); [reviverseedtest](tests/reviverseedtest.c). Reviver Seed FaintPhase/preventInstantRevive/consumption; general doubles and Commander combinations are not implied. |
| `ResetNegativeStatStageModifier` | `PR_MOD_RESET_NEGATIVE_STAT_STAGE`; [pr_apply_reset_negative_stat_stage_modifier](src/phases.c) | [turntest.producer.test](tests/turntest.producer.test.ts); [ITEM-TRANSFER](ITEM-TRANSFER-VALIDATION.md); [SCENE-TRANSFER](SCENE-TRANSFER-VALIDATION.md). White Herb stat-change owner; bounded singles item-turn mode and named stock/transfer proofs, not every interaction. |
| `FieldEffectModifier` | `PR_MOD_FIELD_EFFECT`; [pr_apply_field_effect_modifier](src/phases.c) | [FIELD-EFFECT](FIELD-EFFECT-VALIDATION.md). Mystical Rock weather/terrain duration and bounded cleanup; arbitrary scene histories remain separate. |
| `PokemonExpBoosterModifier` | `PR_MOD_EXP_BOOSTER`; [gc_apply_exp_boosters](../game-core/src/gc_phases.c) | [WIDE-EXP-PHASE](WIDE-EXP-PHASE-VALIDATION.md); [WIDE-PROGRESSION](WIDE-PROGRESSION-VALIDATION.md). Lucky/Golden Egg award/projection and wide EXP phases; whole Endless progression acceptance remains open. |
| `PokemonFriendshipBoosterModifier` | `PR_MOD_FRIENDSHIP_BOOSTER`; [gc_soothe_battle_gain](../game-core/src/gc_phases.c) | [SOOTHE-BATTLE](../game-core/SOOTHE-BATTLE-VALIDATION.md); [SOOTHE-ACQUISITION](../game-core/SOOTHE-ACQUISITION-VALIDATION.md). Soothe Bell positive battle friendship, cap/candy and acquisition owners within recorded bounds. |
| `PokemonNatureWeightModifier` | `PR_MOD_NATURE_WEIGHT`; [pr_project_nature_weight_modifier](src/pokemon.c) | [soul-dew.producer.test](tests/soul-dew.producer.test.ts); [MINT](../game-core/MINT-VALIDATION.md). Soul Dew canonical stacks and nature rounding; unrestricted nature/stat/inventory histories are not implied. |
| `PokemonMoveAccuracyBoosterModifier` | `PR_MOD_MOVE_ACCURACY_BOOSTER`; [pr_apply_accuracy_booster_modifiers](src/phases.c) | [wide-lens.producer.test](tests/wide-lens.producer.test.ts); [LENS](../game-core/LENS-VALIDATION.md). Wide Lens amount5 with represented stacks; arbitrary accuracy booster identities remain guarded. |
| `PokemonMultiHitModifier` | `PR_MOD_MULTI_HIT`; [pr_apply_multi_hit_count_modifiers](src/phases.c); [pr_apply_multi_hit_damage_modifiers](src/damage.c) | [multi-lens.producer.test](tests/multi-lens.producer.test.ts); [MULTI](../game-core/MULTI-VALIDATION.md); [MULTI-ACQUISITION](../game-core/MULTI-ACQUISITION-VALIDATION.md). Multi Lens hit count/damage/owning inventory within recorded caps; all multi-hit/doubles ability interactions are separate. |
| `PokemonFormChangeItemModifier` | `PR_MOD_FORM_CHANGE_ITEM`; [pr_apply_form_change_item](src/form_items.c) | [HELD-FORM](HELD-FORM-VALIDATION.md); [HELD-FORM-STATE](../game-core/HELD-FORM-STATE-VALIDATION.md); [FORM-ITEM-OWNER](../game-core/FORM-ITEM-OWNER-VALIDATION.md); [FORM-ITEM-MENU-2026-10-24](../FORM-ITEM-MENU-2026-10-24.md). Canonical item trigger, state/save, acquisition and party action owners are implemented; arbitrary Transform/fusion/mixed cleanup is not closed. |
| `DamageMoneyRewardModifier` | `PR_MOD_DAMAGE_MONEY_REWARD`; [pr_apply_damage_money_modifiers](src/phases.c) | [HONEY-MONEY](HONEY-MONEY-VALIDATION.md); [GOLDEN](../game-core/GOLDEN-VALIDATION.md); [MONEY](../game-core/MONEY-VALIDATION.md). Golden Punch final-damage wallet owner, native Number arithmetic and cap; unrelated achievements/UI are not implied. |
| `SwitchEffectTransferModifier` | `PR_MOD_SWITCH_EFFECT_TRANSFER`; [pr_transfer_baton_on_switch](src/switching.c) | [BATON](BATON-VALIDATION.md). Held Baton transfer on switch, distinct from Baton Pass; canonical stock and recorded branch bounds. |
| `HeldItemTransferModifier` | Abstract; no standalone C class; [pr_try_transfer_held_item](src/item_transfer.c) | [ITEM-TRANSFER](ITEM-TRANSFER-VALIDATION.md); [SCENE-TRANSFER](SCENE-TRANSFER-VALIDATION.md). Abstract shared theft semantics are flattened into its two concrete C classes; no standalone source factory. |
| `TurnHeldItemTransferModifier` | `PR_MOD_TURN_HELD_TRANSFER`; [pr_apply_turn_held_item_transfers](src/phases.c) | [ITEM-TRANSFER](ITEM-TRANSFER-VALIDATION.md). Mini Black Hole turn-end theft; canonical stock and source order within the recorded bounds. |
| `ContactHeldItemTransferChanceModifier` | `PR_MOD_CONTACT_HELD_TRANSFER`; [pr_apply_contact_held_item_transfers](src/phases.c) | [GRIP-CLAW](GRIP-CLAW-VALIDATION.md). Grip Claw post-hit theft; 10% canonical chance and bounded effective stacks/stock. |

## Cross-cutting owners and boundaries

- [Scene transfer](SCENE-TRANSFER-VALIDATION.md), [stat-item cloning](STAT-LOOT-VALIDATION.md),
  [Pickup](PICKUP-VALIDATION.md), [fused eligibility](FUSED-HELD-VALIDATION.md),
  [constructor locks](../game-core/HELD-LOCK-VALIDATION.md) and
  [inventory capacity](INVENTORY-CAPACITY-VALIDATION.md) supersede the corresponding
  older exclusions. Their own raw/transient/identity bounds remain explicit.
- The source has no concrete descendant of `LapsingPokemonHeldItemModifier` at
  this pin. The abstract transfer base's shared behavior is represented by the
  common C transfer implementation and its two concrete classes. Neither absent
  abstract registry name is counted as a missing enabled concrete mechanic.
- The ordinary item-turn producer's basic held-item evidence uses `TURN_ITEMS=1`.
  Named stock/transfer evidence does not automatically prove every battle effect,
  and a raw helper alone does not establish its owning phase or acquisition path.
- Source constructors permit inputs beyond canonical factory caps. The C
  admission checks and explicit unknown/unsupported paths remain necessary;
  registry membership does not waive those checks.
- Fusion evolution selection, arbitrary mixed cleanup/Transform histories,
  mystery event choices and whole-run/hardware acceptance are separate caller
  obligations. Mystery encounters/rivals have another owner. They are not closed
  by this index or by a large bounded turn sample.

Enemy tokens and side-wide persistent modifiers are not held subclasses and
remain outside this table. Move/ability attributes, constructor closures,
battler/arena tags, phases and their capacity paths still need their own source
inventory reconciliation in the [completion checklist](COMPLETION-CHECKLIST.md).

## Reproduction

```sh
# Check the retained source inventory against current C and evidence references:
python3 battle-core/tools/audit-held-dispatch.py
# Also reproduce source facts from a clean checkout of the exact pin:
python3 battle-core/tools/audit-held-dispatch.py --upstream /path/to/upstream
# After reviewing a ledger change, regenerate this table:
python3 battle-core/tools/audit-held-dispatch.py --write
```

This audit changes no battle dispatch, admission rule, capture, expected answer,
covered ID, save schema or test exclusion. Normal app/custom/save/football and
3DS/stack checks remain merge requirements; only the dedicated Task63 check is
skipped. It is not a battle-core completion claim.
