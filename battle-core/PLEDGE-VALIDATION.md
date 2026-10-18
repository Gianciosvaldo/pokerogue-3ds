# Combined Pledge waiting callback

Pinned upstream `8555c08c823b856cbec4eb99ca84ea52a955836d`, locales
`270ed0a2938b2c3e2e34e7de940e84aecadf548b`, Node 22.23.3.

Fire Pledge, Water Pledge and Grass Pledge are enabled at the pin. Their type,
power, STAB and field-effect callbacks were already implemented. The missing
`AwaitCombinedPledgeAttr.apply` behavior is now called by MoveEffectPhase:

- A user with a nonzero combining flag queues the combined-move message and
  returns false without replacing that flag.
- Otherwise, inspect the first queued MovePhase whose Pokémon is on the user's
  side. Only a different Pledge qualifies. Do not search past a same move or a
  non-Pledge to find another eligible phase.
- Set both Pokémon's combining flags to the waiting user's original move ID,
  queue the waiting message with both names, force the actual ally's first
  queued move next, and override the waiting move's remaining effects.

This override does not invoke parent HP/condition/chance gates. The selected
queued owner can be the user itself in an artificial duplicate-owner queue;
forceMoveNext still searches for the actual ally. Existing queue selection,
timing, type/power/STAB and arena-effect implementations are reused. No fields,
save formats or generated data change.

Reproduce `tools/pledge-diff.sh UPSTREAM 777`, then 2034. Producers use the actual
registered source attributes and natural double-field actors. Native battle RNG
is captured before GameManager construction and restored after startup.

All 25,088 raw callback cases match exactly across both seeds, covering 49,992
calls. Each seed has 920 true returns, 24,076 false returns, 21,838 already-set
flags, 496 successful raw calls by fainted users and 128 explicitly separate
partner controls. Compare native boolean/overridden-holder results, all actors'
HP/acted/combining flags, every phase level/marker/message, queued move owners,
IDs and timing modifiers, and both RNG streams. Include zero through eight
queued moves, duplicate owners, swapped party positions, all three timing
modifiers, repeated calls, same-move and leading non-Pledge refusals. The C raw
runner includes the actual private production helper, not a duplicate model.

The initial 12,288-case matrix allowed successful raw callbacks only with
self-aliasing queued owners, so omitting one setter was masked by the other.
The compiled-fault check caught this evidence weakness. Protocol 2 retains those
cases and adds 128 separate-partner controls plus 128 leading-non-Pledge controls.
It requires successful separate-owner cases; earlier captures remain accepted
and must replay unchanged. Source observations and comparison floors are not
weakened to accept an implementation.

All 16,384 full owning turns match exactly, with zero source exceptions or
unsupported cases. The move cycle covers all six ordered Pledge pairs. Native
successful wait counts for Fire/Water/Grass are 3,854/5,311/3,715 for seed 777 and
3,855/5,315/3,731 for seed 2034. All six registered field-effect callbacks fire
at least 1,191 times per seed. The full comparator retains actor snapshots,
state, histories, arena tags, phase order, text and RNG, plus existing
ally/spread/multiple-faint/switch and wide-damage floors.

All 20 compiled faults are caught: missing/wrong participant flags, extra HP
gating, existing flags, side/first-candidate selection, identical/non-Pledge
refusals, messages, actual-ally versus queued-owner selection, timing, RNG,
override/owning dispatch, combined power/type/STAB and field effects, and
first versus all duplicate queue owners. Twenty-two malformed captures are
rejected. Faults mutate the production helper included by the raw runner or
actual linked implementations; they do not mutate a substitute model.

Rebased on verified HP-cost merge #307. All retained covered IDs remain exact:
doubles 1,986/1,986/1,988 and mixed 1,994/1,993/1,994/1,998, with no lost IDs.
Original Pledge protocol-1 cases and the HP-cost/Pain Split callback and owning
captures replay exactly after integration. Normal game-core tests pass;
saved/unsaved digests both equal `361c1d0e`. Normal app/custom/save tests and
the 3DS build run on the exact proposed PR head in CI.
These bounded fixtures do not establish all Pledge interactions with fusion,
Transform, passive abilities, inventory, redirection or following turns, and do
not close the full battle-core inventory or natural wide-level progression.
Normal app/custom/save tests and exact-head 3DS CI remain required; only the
dedicated custom-only Task63 workflow is skipped.
