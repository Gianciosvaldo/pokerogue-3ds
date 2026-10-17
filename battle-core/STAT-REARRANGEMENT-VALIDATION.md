# Stat-changing moves and wide effective-stat ordering

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

## Wide effective stats and ordering

The new owning turns exposed another narrowing: `getEffectiveStat` returned an
int32 and the speed sorter stored int32 keys. A 2^40 Speed override became a
negative key. Preserve the source's floored double return and double speed keys.
The native signed shift modifiers still require ECMAScript ToInt32 before
shifting; C's out-of-range cast is not that modulo-2^32 conversion. A private
helper now performs truncation/modulo and retains the source's shift ordering
for Slow Start, paralysis and the swamp pledge, including Tailwind before them.
All production effective-stat callers were audited for the return-type change.
The highest-stat tag chooser's separate int32 locals remain an explicit next
wide-stat boundary, not a claimed part of this proof.

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
- The original thirteen compiled callback faults are detected: permanent/base-stat reads/writes,
  reading after a write, narrowing, nonpositive writes, wrong inversion target,
  missing evasion and removal of each parent gate. Thirteen malformed captures
  are rejected, including truncated/footer/version, invalid roles/counts,
  overflowing IDs/levels, nonfinite values and invalid queue counts.
- Retained complete doubles covered IDs rise 1,969→1,974, 1,958→1,965 and
  1,970→1,976. Mixed covered IDs rise 1,978→1,982 and 1,973→1,979; summon/switch
  remain 1,993/1,996. Every previously covered ID is retained with exact state,
  phase/text and RNG comparisons. These are sample counts, not completion rates.

All 16,384 owning doubles turns (8,192 per seed) match, with zero source
exceptions or unsupported cases. The producer retains the established driver
and spread/ally/faint controls, observes successful native attribute calls with
passthrough spies, and compares every requested turn using the unchanged doubles
comparison plus its strict complete-case wrapper. Seed 777 observes
8,875/8,875/2,731 successful Speed Swap/Power Shift/Topsy-Turvy callbacks.
Before the effective-stat repair that same unmodified capture exposed 6,656
mismatches; after it all 8,192 are exact. No floor is weakened.

Final protocol v2 retains the original structural matrix and appends 128 explicit
signed/positive 2^32, 2^40 and MAX_SAFE_INTEGER controls. Each before/after frame
adds actual native effective-stat reads for all five stats on four actors under
ordinary and varied Slow Start/paralysis/Tailwind/swamp contexts. Temporary probe
contexts are restored and both RNG streams must remain unchanged. The 12,416
cases per seed compare 1,986,560 getter results overall, alongside the actual
callbacks, full frames and queues. The historical v1 captures remain unchanged
and replay exactly. Native startup assigns fresh actor IDs to new captures;
those actual IDs are compared without rewriting them to historical values.

The expanded source producer exceeded Node's default 2 GB heap on initial
attempts; those partial/error captures are not evidence. Reproduction gives the
same pinned Node 22 a 4 GB heap and retains all probes/cases. A preliminary v2
capture before the 128 appended controls is retained separately; final v2
transport requires all 12,416 cases. Five additional compiled faults exercise
wide returns, both speed key storage sites, the actual owning phase dispatcher
and incorrect C-cast shift conversion. The final guard totals and exact-head
normal validation are recorded with the PR.

No Pokemon/save fields or generated tables change. Normal game-core,
app/custom/save sanitizer tests and the exact-head 3DS build remain merge gates;
only the dedicated custom-only Task63 job is skipped. HP splitting, pre-damage
stat theft, ability-changing moves, other enabled callbacks and broader
Endless/interaction inventory remain separate work.
