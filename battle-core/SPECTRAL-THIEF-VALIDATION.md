# Spectral Thief pre-damage callback

Pinned upstream `8555c08c823b856cbec4eb99ca84ea52a955836d`, locales
`270ed0a2938b2c3e2e34e7de940e84aecadf548b`, Node 22.23.3.

`SpectralThiefAttr.apply` overrides the base attribute without HP, condition or
chance gates. It visits ATK through EVA in native order. For each positive target
stage it requests `min(targetStage, 6 - userStage)` for the user, with the user as
source, and immediately subtracts that amount from the target. Zero amounts still
queue StatStageChangePhase; its constructor removes the zero change, leaving an
empty phase. The theft message is unconditional.

The owning MoveEffectPhase invokes this after the critical roll and before
getAttackDamage. The user's gain remains queued while that hit is calculated;
the target's reduction is immediate. Repeated hits can run after the queued gain.
The move's native registration ignores Substitute. The port now dispatches the
actual pre-damage attribute list at this point, retaining explicit unsupported
reporting for other unported members of the base class. No save fields change.

Reproduce `tools/spectral-thief-diff.sh UPSTREAM 777`, then 2034. Both native
producers capture BattleScene's battle RNG before constructing GameManager and
restore it after natural double-battle startup. The raw producer invokes the
registered attribute; the turn producer runs native phases and uses a passthrough
spy to count real attribute calls. UI refresh and automatic advancement are the
only intercepted presentation operations in the raw proof.

- Raw API captures: 8,192 cases per seed, all 16,384 exact. All four field roles,
  self/ally/opponent aliases, zero/alive HP, Simple/Contrary, levels 1..1,000,000,
  all seven stage axes, caps and empty thefts. Each seed observes 1,060 empty
  thefts, 3,714 zero-change phases and 25,353 queued stat phases.
- Compares all four actors' 125-field frames, complete message/marker queues,
  queue levels/order/text, dynamic stat phase recipient/source/self/change/options
  and both RNG states. Native return true is asserted by the producer; the C
  dispatcher is void, so no return-value parity claim is made.
- The original 8,192 native owning turns (seed 777) are retained unchanged and
  replay exactly, including full states, queues, text, trace and both RNGs.

The matrices are bounded evidence, not a complete interaction inventory or proof
of natural high-wave progression. Normal game-core/app/custom/save tests and
exact-head 3DS CI remain required; only dedicated custom-only Task63 is skipped.
HP Split, ability-changing callbacks and the final completion checklist remain
open.
