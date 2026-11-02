# Registered move attribute source inventory

Source `8555c08c823b856cbec4eb99ca84ea52a955836d`. All **921** move registrations,
**86** source-disabled flags and ordered normal/charge attribute instances match
the C tables, including complete class inheritance bitmaps. The **835 enabled**
entries contain **982 instances across 196 most-derived classes** (NONE is included
in the registry count). This proves registration transport, not semantic parity.

Nine reviewed family switches account for every registered enabled subclass
admitted by their filters:

| C owner | Source family | Enabled concrete classes |
| --- | --- | --- |
| `pr_apply_variable_power` | `VariablePowerAttr` | 28 |
| `pr_apply_variable_move_type` | `VariableMoveTypeAttr` | 12 |
| `pr_apply_type_chart_override` | `MoveTypeChartOverrideAttr` | 6 |
| `pr_apply_variable_move_category` | `VariableMoveCategoryAttr` | 3 |
| `pr_apply_variable_atk` | `VariableAtkAttr` | 2 |
| `pr_apply_variable_def` | `VariableDefAttr` | 1 |
| `pr_apply_fixed_damage` | `FixedDamageAttr` | 7 |
| `pr_apply_modified_damage` | `ModifiedDamageAttr` | 1 |
| `move_header_phase` | `MoveHeaderAttr` | 3 |

The switch check proves class exhaustiveness only. It does not establish arithmetic,
conditions/chance, target lookup, queues, text, RNG, owning callers or interactions.
Those still require their native proofs and the unsupported-path ledger.

`HitsTagForDoubleDamageAttr` uses inherited `HitsTagAttr` dispatch in
`pr_hits_tag_multiplier`; its missing most-derived token is not an absent mechanic.
Other enabled names have source references below. These are candidate owner
references, not semantic sign-offs, and may include AI or constructor callers.

| Enabled class | Instances | Candidate source references |
| --- | --- | --- |
| `AbilityChangeAttr` | 2 | `battle-core/src/closures_conditions.c`, `battle-core/src/move_effects.c` |
| `AbilityCopyAttr` | 2 | `battle-core/src/closures_conditions.c`, `battle-core/src/move_effects.c` |
| `AbilityGiveAttr` | 1 | `battle-core/src/move_effects.c` |
| `AcupressureStatStageChangeAttr` | 1 | `battle-core/src/move_effects.c` |
| `AddArenaTagAttr` | 22 | `battle-core/src/closures_conditions.c`, `battle-core/src/move_effects.c` |
| `AddArenaTrapTagAttr` | 4 | `battle-core/src/closures_conditions.c`, `battle-core/src/move_effects.c`, `battle-core/src/phases.c` |
| `AddArenaTrapTagHitAttr` | 2 | `battle-core/src/move_effects.c` |
| `AddBattlerTagAttr` | 54 | `battle-core/src/closures_conditions.c`, `battle-core/src/move_effects.c` |
| `AddBattlerTagHeaderAttr` | 1 | `battle-core/src/phases.c` |
| `AddBattlerTagIfBoostedAttr` | 1 | `battle-core/src/move_effects.c` |
| `AddPledgeEffectAttr` | 6 | `battle-core/src/move_effects.c` |
| `AddSubstituteAttr` | 2 | `battle-core/src/closures_conditions.c`, `battle-core/src/move_effects.c`, `battle-core/src/phases.c` |
| `AddTypeAttr` | 2 | `battle-core/src/closures_conditions.c`, `battle-core/src/move_effects.c` |
| `AfterYouAttr` | 1 | `battle-core/src/move_effects.c` |
| `AlwaysHitMinimizeAttr` | 8 | `battle-core/src/phases.c` |
| `AntiSunlightPowerDecreaseAttr` | 2 | `battle-core/src/move_attrs.c` |
| `AttackReducePpMoveAttr` | 1 | `battle-core/src/move_effects.c` |
| `AuraWheelTypeAttr` | 1 | `battle-core/src/move_attrs.c`, `game-core/src/gc_reward.c` |
| `AverageStatsAttr` | 2 | `battle-core/src/move_effects.c` |
| `AwaitCombinedPledgeAttr` | 3 | `battle-core/src/phases.c` |
| `BeakBlastHeaderAttr` | 1 | `battle-core/src/phases.c` |
| `BeatUpAttr` | 1 | `battle-core/src/move_attrs.c` |
| `BlizzardAccuracyAttr` | 1 | `battle-core/src/phases.c` |
| `BypassBurnDamageReductionAttr` | 1 | `ai-core/src/ai_damage.c`, `battle-core/src/damage.c` |
| `BypassRedirectAttr` | 4 | `battle-core/src/phases.c` |
| `BypassSleepAttr` | 2 | `battle-core/src/phases.c` |
| `ChangeTypeAttr` | 2 | `battle-core/src/closures_conditions.c`, `battle-core/src/move_effects.c` |
| `ChillyReceptionAttr` | 1 | `battle-core/src/closures_conditions.c`, `battle-core/src/move_effects.c`, `battle-core/src/switching.c` |
| `ClearTerrainAttr` | 3 | `battle-core/src/move_effects.c` |
| `ClearWeatherAttr` | 1 | `battle-core/src/move_effects.c` |
| `CombinedPledgePowerAttr` | 3 | `battle-core/src/move_attrs.c` |
| `CombinedPledgeStabBoostAttr` | 3 | `battle-core/src/move_attrs.c` |
| `CombinedPledgeTypeAttr` | 3 | `battle-core/src/move_attrs.c` |
| `CompareWeightPowerAttr` | 2 | `battle-core/src/move_attrs.c` |
| `ConfuseAttr` | 18 | `battle-core/src/move_effects.c` |
| `ConsecutiveUseDoublePowerAttr` | 3 | `ai-core/src/ai.c`, `battle-core/src/closures_moves.c`, `battle-core/src/commands.c`, `battle-core/src/move_attrs.c` |
| `ConsecutiveUseMultiBasePowerAttr` | 1 | `battle-core/src/closures_moves.c`, `battle-core/src/move_attrs.c` |
| `CopyBiomeTypeAttr` | 1 | `battle-core/src/move_effects.c` |
| `CopyMoveAttr` | 2 | `battle-core/src/called_moves.c`, `battle-core/src/closures_conditions.c`, `battle-core/src/phases.c` |
| `CopyStatsAttr` | 1 | `battle-core/src/move_effects.c` |
| `CopyTypeAttr` | 1 | `battle-core/src/move_effects.c` |
| `CounterDamageAttr` | 4 | `ai-core/src/ai.c`, `battle-core/src/ab_attrs.c`, `battle-core/src/move_attrs.c` |
| `CounterRedirectAttr` | 4 | `battle-core/src/phases.c` |
| `CritOnlyAttr` | 6 | `ai-core/src/ai.c`, `battle-core/src/damage.c` |
| `CueNextRoundAttr` | 1 | `battle-core/src/move_effects.c` |
| `CurseAttr` | 1 | `battle-core/src/move_effects.c` |
| `CutHpStatStageBoostAttr` | 3 | `battle-core/src/closures_conditions.c`, `battle-core/src/move_effects.c` |
| `DefAtkAttr` | 1 | `battle-core/src/move_attrs.c` |
| `DefDefAttr` | 3 | `battle-core/src/move_attrs.c` |
| `DelayedAttackAttr` | 2 | `battle-core/src/phases.c` |
| `DestinyBondAttr` | 1 | `battle-core/src/move_effects.c` |
| `DoublePowerChanceAttr` | 1 | `battle-core/src/move_attrs.c` |
| `EatBerryAttr` | 2 | `battle-core/src/move_effects.c` |
| `ElectroBallPowerAttr` | 1 | `battle-core/src/move_attrs.c` |
| `ExposedMoveAttr` | 3 | `battle-core/src/closures_conditions.c`, `battle-core/src/move_effects.c` |
| `FallDownAttr` | 2 | `battle-core/src/move_effects.c` |
| `FirstMoveTypeAttr` | 1 | `battle-core/src/move_effects.c` |
| `FixedDamageAttr` | 2 | `battle-core/src/ab_attrs.c`, `battle-core/src/damage.c`, `battle-core/src/move_attrs.c` |
| `FlameBurstAttr` | 1 | `battle-core/src/move_effects.c` |
| `FlinchAttr` | 33 | `ai-core/src/ai.c`, `battle-core/src/closures_abilities.c`, `battle-core/src/move_effects.c`, `battle-core/src/phases.c` |
| `FlyingTypeMultiplierAttr` | 1 | `battle-core/src/move_attrs.c` |
| `ForceLastAttr` | 1 | `battle-core/src/move_effects.c` |
| `ForceSwitchOutAttr` | 10 | `battle-core/src/closures_conditions.c`, `battle-core/src/move_effects.c`, `battle-core/src/phases.c`, `battle-core/src/switching.c` |
| `FormChangeItemTypeAttr` | 2 | `battle-core/src/move_attrs.c`, `game-core/src/gc_reward.c` |
| `FreezeDryAttr` | 1 | `battle-core/src/move_attrs.c` |
| `FriendshipPowerAttr` | 4 | `battle-core/src/move_attrs.c` |
| `GrowthStatStageChangeAttr` | 1 | `battle-core/src/move_effects.c` |
| `GulpMissileTagAttr` | 2 | `battle-core/src/move_effects.c` |
| `GyroBallPowerAttr` | 1 | `battle-core/src/move_attrs.c` |
| `HalfSacrificialAttr` | 2 | `battle-core/src/move_effects.c` |
| `HealAttr` | 11 | `battle-core/src/closures_conditions.c`, `battle-core/src/move_effects.c`, `battle-core/src/phases.c` |
| `HealOnAllyAttr` | 1 | `battle-core/src/closures_conditions.c`, `battle-core/src/modifiers.c`, `battle-core/src/move_effects.c`, `battle-core/src/phases.c` |
| `HealStatusEffectAttr` | 65 | `battle-core/src/move_effects.c`, `battle-core/src/phases.c` |
| `HiddenPowerTypeAttr` | 1 | `battle-core/src/closures_abilities.c`, `battle-core/src/move_attrs.c`, `game-core/src/gc_reward.c` |
| `HighCritAttr` | 25 | `battle-core/src/damage.c` |
| `HitHealAttr` | 14 | `battle-core/src/ab_attrs.c`, `battle-core/src/move_effects.c` |
| `HitsSameTypeAttr` | 1 | `battle-core/src/move_attrs.c` |
| `HitsTagAttr` | 6 | `battle-core/src/move_attrs.c`, `battle-core/src/phases.c` |
| `HitsTagForDoubleDamageAttr` | 14 | `battle-core/src/move_attrs.c (inherited HitsTagAttr)` |
| `HpPowerAttr` | 3 | `battle-core/src/move_attrs.c` |
| `HpSplitAttr` | 1 | `battle-core/src/move_effects.c` |
| `IceNoEffectTypeAttr` | 1 | `battle-core/src/move_attrs.c` |
| `IgnoreOpponentStatStagesAttr` | 4 | `battle-core/src/phases.c`, `battle-core/src/pokemon.c` |
| `IncrementMovePriorityAttr` | 1 | `battle-core/src/ab_attrs.c` |
| `InvertStatsAttr` | 1 | `battle-core/src/move_effects.c` |
| `IvyCudgelTypeAttr` | 1 | `battle-core/src/move_attrs.c`, `game-core/src/gc_reward.c` |
| `JawLockAttr` | 1 | `battle-core/src/move_effects.c` |
| `LastMoveDoublePowerAttr` | 2 | `battle-core/src/move_attrs.c` |
| `LeechSeedAttr` | 2 | `battle-core/src/move_effects.c` |
| `LessPPMorePowerAttr` | 1 | `battle-core/src/move_attrs.c` |
| `LevelDamageAttr` | 2 | `battle-core/src/move_attrs.c` |
| `LowHpPowerAttr` | 2 | `battle-core/src/move_attrs.c` |
| `MagnitudePowerAttr` | 1 | `battle-core/src/move_attrs.c` |
| `MatchHpAttr` | 1 | `battle-core/src/move_attrs.c` |
| `MatchUserTypeAttr` | 1 | `battle-core/src/move_attrs.c`, `game-core/src/gc_reward.c` |
| `MessageAttr` | 6 | `battle-core/src/move_effects.c` |
| `MessageHeaderAttr` | 1 | `battle-core/src/phases.c` |
| `MissEffectAttr` | 4 | `battle-core/src/phases.c` |
| `MoneyAttr` | 3 | `battle-core/src/move_effects.c` |
| `MovePowerMultiplierAttr` | 32 | `battle-core/src/move_attrs.c` |
| `MovesetCopyMoveAttr` | 1 | `battle-core/src/phases.c` |
| `MultiHitAttr` | 32 | `battle-core/src/damage.c`, `battle-core/src/modifiers.c`, `battle-core/src/phases.c` |
| `MultiHitPowerIncrementAttr` | 2 | `battle-core/src/move_attrs.c` |
| `MultiStatusEffectAttr` | 2 | `battle-core/src/move_effects.c` |
| `NaturePowerAttr` | 1 | `battle-core/src/ab_attrs.c`, `battle-core/src/called_moves.c`, `battle-core/src/phases.c` |
| `NeutralDamageAgainstFlyingTypeAttr` | 1 | `ai-core/src/ai_damage.c`, `battle-core/src/damage.c`, `battle-core/src/move_attrs.c` |
| `NihilLightAttr` | 1 | `battle-core/src/move_attrs.c` |
| `NoEffectAttr` | 4 | `battle-core/src/phases.c` |
| `OneHitKOAccuracyAttr` | 3 | `battle-core/src/phases.c` |
| `OneHitKOAttr` | 4 | `battle-core/src/ab_attrs.c`, `battle-core/src/closures_abilities.c`, `battle-core/src/move_attrs.c`, `battle-core/src/phases.c` |
| `OpponentHighHpPowerAttr` | 3 | `battle-core/src/move_attrs.c` |
| `OrderUpStatBoostAttr` | 1 | `battle-core/src/move_effects.c` |
| `OverrideWeatherMultiplierAttr` | 1 | `ai-core/src/ai_damage.c`, `battle-core/src/damage.c` |
| `PartingShotAttr` | 1 | `battle-core/src/move_effects.c`, `battle-core/src/switching.c` |
| `PartyStatusCureAttr` | 3 | `battle-core/src/move_effects.c` |
| `PhotonGeyserCategoryAttr` | 1 | `battle-core/src/move_attrs.c` |
| `PositiveStatStagePowerAttr` | 2 | `battle-core/src/move_attrs.c` |
| `PostVictoryStatStageChangeAttr` | 1 | `battle-core/src/move_effects.c`, `battle-core/src/phases.c` |
| `PreMoveMessageAttr` | 5 | `battle-core/src/phases.c` |
| `PreUseInterruptAttr` | 1 | `battle-core/src/phases.c` |
| `PresentPowerAttr` | 1 | `battle-core/src/move_attrs.c` |
| `ProtectAttr` | 9 | `battle-core/src/history.c`, `battle-core/src/move_effects.c` |
| `PsychoShiftEffectAttr` | 1 | `battle-core/src/move_effects.c` |
| `PunishmentPowerAttr` | 1 | `battle-core/src/move_attrs.c` |
| `RageAttr` | 1 | `battle-core/src/move_effects.c` |
| `RageFistPowerAttr` | 1 | `battle-core/src/move_attrs.c` |
| `RagingBullTypeAttr` | 1 | `battle-core/src/move_attrs.c`, `game-core/src/gc_reward.c` |
| `RandomLevelDamageAttr` | 1 | `battle-core/src/move_attrs.c` |
| `RandomMoveAttr` | 1 | `battle-core/src/called_moves.c`, `battle-core/src/closures_conditions.c`, `battle-core/src/phases.c` |
| `RandomMovesetMoveAttr` | 2 | `battle-core/src/called_moves.c`, `battle-core/src/closures_conditions.c`, `battle-core/src/phases.c` |
| `RechargeAttr` | 10 | `battle-core/src/move_effects.c` |
| `RecoilAttr` | 14 | `battle-core/src/move_effects.c` |
| `ReducePpMoveAttr` | 1 | `battle-core/src/move_effects.c` |
| `RemoveAllSubstitutesAttr` | 1 | `battle-core/src/move_effects.c` |
| `RemoveArenaTagsAttr` | 2 | `battle-core/src/move_effects.c` |
| `RemoveArenaTrapAttr` | 4 | `battle-core/src/move_effects.c` |
| `RemoveBattlerTagAttr` | 5 | `battle-core/src/move_effects.c` |
| `RemoveHeldItemAttr` | 2 | `battle-core/src/move_effects.c` |
| `RemoveScreensAttr` | 4 | `battle-core/src/move_effects.c` |
| `RemoveTypeAttr` | 2 | `battle-core/src/move_effects.c` |
| `RepeatMoveAttr` | 1 | `battle-core/src/move_effects.c` |
| `ResetStatsAttr` | 3 | `battle-core/src/move_effects.c` |
| `ResistLastMoveTypeAttr` | 1 | `battle-core/src/move_effects.c` |
| `RespectAttackTypeImmunityAttr` | 1 | `ai-core/src/ai_damage.c`, `battle-core/src/damage.c` |
| `RestAttr` | 1 | `battle-core/src/closures_conditions.c`, `battle-core/src/move_effects.c`, `battle-core/src/phases.c` |
| `RevivalBlessingAttr` | 1 | `battle-core/src/move_effects.c` |
| `RoundPowerAttr` | 1 | `battle-core/src/move_attrs.c` |
| `SacrificialAttr` | 3 | `battle-core/src/modifiers.c`, `battle-core/src/move_effects.c` |
| `SacrificialAttrOnHit` | 2 | `battle-core/src/modifiers.c`, `battle-core/src/move_effects.c` |
| `SacrificialFullRestoreAttr` | 2 | `battle-core/src/arena_tags.c`, `battle-core/src/move_effects.c`, `game-core/src/gc_save.c` |
| `SecretPowerAttr` | 1 | `ai-core/src/ai.c`, `battle-core/src/move_effects.c` |
| `SemiInvulnerableAttr` | 7 | `battle-core/src/move_effects.c` |
| `SheerColdAccuracyAttr` | 1 | `battle-core/src/phases.c` |
| `ShellSideArmCategoryAttr` | 1 | `battle-core/src/move_attrs.c` |
| `ShiftStatAttr` | 1 | `battle-core/src/move_effects.c` |
| `SketchAttr` | 1 | `battle-core/src/move_effects.c` |
| `SpectralThiefAttr` | 1 | `battle-core/src/move_effects.c` |
| `SpitUpPowerAttr` | 1 | `battle-core/src/move_attrs.c` |
| `StatStageChangeAttr` | 182 | `battle-core/src/move_effects.c` |
| `StatusEffectAttr` | 84 | `battle-core/src/move_effects.c` |
| `StatusIfBoostedAttr` | 1 | `battle-core/src/move_effects.c` |
| `StealEatBerryAttr` | 2 | `battle-core/src/move_effects.c` |
| `StealHeldItemChanceAttr` | 2 | `battle-core/src/move_effects.c` |
| `StormAccuracyAttr` | 3 | `battle-core/src/phases.c` |
| `SuppressAbilitiesAttr` | 1 | `battle-core/src/move_effects.c` |
| `SuppressAbilitiesIfActedAttr` | 1 | `battle-core/src/move_effects.c` |
| `SurviveDamageAttr` | 2 | `battle-core/src/move_attrs.c` |
| `SwapArenaTagsAttr` | 1 | `battle-core/src/move_effects.c` |
| `SwapStatAttr` | 1 | `battle-core/src/move_effects.c` |
| `SwapStatStagesAttr` | 3 | `battle-core/src/move_effects.c` |
| `SwitchAbilitiesAttr` | 1 | `battle-core/src/move_effects.c` |
| `TargetAtkUserAtkAttr` | 1 | `battle-core/src/move_attrs.c` |
| `TargetHalfHpDamageAttr` | 3 | `battle-core/src/move_attrs.c` |
| `TechnoBlastTypeAttr` | 1 | `battle-core/src/move_attrs.c`, `game-core/src/gc_reward.c` |
| `TeraBlastPowerAttr` | 1 | `battle-core/src/move_attrs.c` |
| `TeraBlastTypeAttr` | 1 | `battle-core/src/move_attrs.c`, `game-core/src/gc_reward.c` |
| `TeraMoveCategoryAttr` | 2 | `battle-core/src/move_attrs.c` |
| `TeraStarstormTypeAttr` | 1 | `battle-core/src/move_attrs.c` |
| `TerrainChangeAttr` | 4 | `battle-core/src/closures_conditions.c`, `battle-core/src/move_effects.c` |
| `TerrainPulseTypeAttr` | 1 | `battle-core/src/move_attrs.c` |
| `ThunderAccuracyAttr` | 2 | `battle-core/src/phases.c` |
| `ToxicAccuracyAttr` | 1 | `battle-core/src/phases.c` |
| `TransformAttr` | 1 | `battle-core/src/move_effects.c` |
| `TrapAttr` | 10 | `battle-core/src/move_effects.c` |
| `TurnDamagedDoublePowerAttr` | 2 | `battle-core/src/move_attrs.c` |
| `TypelessAttr` | 1 | `ai-core/src/ai_damage.c`, `battle-core/src/ab_attrs.c`, `battle-core/src/damage.c` |
| `UserHpDamageAttr` | 1 | `battle-core/src/move_attrs.c` |
| `VariableHealAttr` | 6 | `battle-core/src/closures_conditions.c`, `battle-core/src/move_effects.c`, `battle-core/src/phases.c` |
| `VariableTargetAttr` | 2 | `battle-core/src/battler_tags.c`, `battle-core/src/called_moves.c`, `battle-core/src/modifiers.c` |
| `WaterShurikenMultiHitTypeAttr` | 1 | `battle-core/src/phases.c` |
| `WaterShurikenPowerAttr` | 1 | `battle-core/src/move_attrs.c` |
| `WeatherBallTypeAttr` | 1 | `battle-core/src/move_attrs.c` |
| `WeatherChangeAttr` | 5 | `battle-core/src/closures_conditions.c`, `battle-core/src/move_effects.c` |
| `WeatherInstantChargeAttr` | 3 | `battle-core/src/phases.c` |
| `WeightPowerAttr` | 2 | `battle-core/src/move_attrs.c` |
| `WishAttr` | 1 | `battle-core/src/move_effects.c` |

Five compiled table faults and eight malformed/forged source captures are
rejected; a pristine control passes. Fresh native reproduction is byte-identical.
Run `python3 battle-core/tools/check-move-inventory-guards.py` for those controls.

Reproduce: `python3 battle-core/tools/audit-move-inventory.py` and
`battle-core/tools/reproduce-move-inventory.sh PINNED_UPSTREAM OUTPUT_DIR`.
The native producer observes initialized registrations and retains disabled
entries; it never enables a source-disabled move. Closure identity, held-class and
capacity audits remain separate. This record does not close the broader source
review or certify battle-core completion. Only dedicated Task63 is excluded;
normal core/app/custom/save/football and actual 3DS/ARM checks remain merge gates.
