# Speed Swap, Power Shift and Topsy-Turvy

Pinned upstream `8555c08c823b856cbec4eb99ca84ea52a955836d`, locales
`270ed0a2938b2c3e2e34e7de940e84aecadf548b`, Node 22.23.3.

The registered `SwapStatAttr`, `ShiftStatAttr` and `InvertStatsAttr` implementations
in `src/data/moves/move.ts` call only the parent `MoveEffectAttr.apply` gate.
Their raw apply methods do not run the move's conditions or chance roll. All
three constructors use the default false selfTarget, including Power Shift;
its move caller normally supplies the user as target. Consequently the raw
boundary gate tests the supplied target's HP, even when it differs from the user.

Speed Swap reads both effective stat overrides before writing summon data.
Power Shift snapshots both user stats before writing either override. Native
`Pokemon.getStat(..., false)` uses the summon override when truthy, and
`setStat(..., false)` ignores nonpositive values. Neither move rewrites permanent
stats or stat stages. Topsy-Turvy directly negates all seven target stages and
queues its target-named message, without stat-change phases or ability modifiers.
The port now follows those paths and uses the existing localized message system.

## Native evidence

Reproduce with `tools/stat-rearrangement-diff.sh UPSTREAM 777`, then `2034`.
Native BattleScene battle RNG is captured before GameManager construction and
restored after starting the battle. No gameplay method is replaced. Only UI info
refresh and automatic phase advancement are intercepted for the owning API
capture; registered attribute objects execute their actual native methods.

- Each seed records 12,288 complete API cases, all exact. The structural matrix
  is deterministic; the seeds change the initialized RNG streams, not the case
  layout. Every case compares all four actors' 125-field frames before/after,
  queue level/order/text and both RNG states. There are no unsupported cases or
  source exceptions. Cases cover all user/target positions, self/ally/opponent,
  living/fainted targets and users, one through three consecutive calls, levels
  1 through 1,000,000, every stage from -6 through +6, existing/absent overrides
  and values around int32 and 2^40. Negative and fractional overrides are explicit
  raw API boundaries, not evidence of naturally generated encounter stats.
- The C replay invokes the actual move-effect dispatcher and checks positive/
  negative parent gates, target relationships and repeated-call coverage floors.
  It does not supply an independently copied formula as the expected state.
- Thirteen compiled faults are detected: permanent/base-stat reads/writes,
  reading after a write, narrowing, nonpositive writes, wrong inversion target,
  missing evasion and removal of each parent gate. Thirteen malformed captures
  are rejected, including truncated/footer/version, invalid roles/counts,
  overflowing IDs/levels, nonfinite values and invalid queue counts.
- Retained complete doubles covered IDs rise 1,969→1,974, 1,958→1,965 and
  1,970→1,976. Mixed covered IDs rise 1,978→1,982 and 1,973→1,979; summon/switch
  remain 1,993/1,996. Every previously covered ID is retained with exact state,
  phase/text and RNG comparisons. These are sample counts, not completion rates.

Owning full-turn results and final normal validation are recorded with this
slice's PR. The owning producer retains the established doubles driver and its
spread/ally/faint controls, observes successful native attribute calls with
passthrough spies, and compares every requested turn with the unchanged doubles
comparison plus its strict complete-case wrapper. No floor is weakened.

No Pokemon/save fields or generated tables change. Normal game-core,
app/custom/save sanitizer tests and the exact-head 3DS build remain merge gates;
only the dedicated custom-only Task63 job is skipped. HP splitting, pre-damage
stat theft, ability-changing moves, other enabled callbacks and broader
Endless/interaction inventory remain separate work.
