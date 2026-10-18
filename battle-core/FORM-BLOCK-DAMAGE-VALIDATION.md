# Disguise and Ice Face damage/form callbacks

Source `8555c08c823b856cbec4eb99ca84ea52a955836d`, locales
`270ed0a2938b2c3e2e34e7de940e84aecadf548b`, Node 22.23.3.

The owning dispatcher is `ReceivedMoveDamageMultiplierAbAttr`.
`PreDefendModifyDamageAbAttrParams` is a parameter interface, not an attribute
class. Keep the source's positive damage, raw form, move condition and Substitute
checks. Simulation does not change damage, HP, applied-ability sets or queues.
Real application zeros incoming damage. Disguise then calls damageAndUpdate with
one eighth of permanent maximum HP, rounded down with a minimum of one;
`result=INDIRECT`, `ignoreSegments=true`, `ignoreFaintPhase=true`, no source.
This queues DamageAnim before damage and calls PostDamage abilities. The scene
then unshifts the registered quiet form change. Ice Face has no recoil.

Reuse the existing source-backed Mimikyu/Eiscue form registry helper and quiet
phase implementation. Form changes are queued without eagerly mutating forms.
There is no extra HP/species gate in the attribute; foreign species with these
abilities still pass the callback's own predicates, while the scene's registry
chooses whether to queue a form change. Both abilities bypass faint and are
unsuppressable at this pin, but remain ignorable. The no-engine numeric caller
continues to report deferred side effects explicitly.

Add an explicit ignoreSegments variant to the shared damageAndUpdate operation.
Existing callers retain their default false option. No save format changes;
received-attack history belongs to turnData and is excluded from serialized
Pokemon fields.

Reproduce `tools/form-block-damage-diff.sh UPSTREAM 777`, then 2034. Capture native
BattleScene battle RNG before GameManager construction and restore after battle
startup. Spies on real methods pass through; presentation update/phase advance
stubs only delimit the observation boundary.

Raw protocol 2 retains the original 8,576 cases and appends 128 positive passive
Wimp Out cases. All 17,408 cases across both seeds match exactly: all actors'
125-field state, boss shields, queue levels/order, messages, cached ability
name/ID/passive/on-field inputs, quiet forms, DamageAnim amounts/results/critical,
stat-change dynamic payloads and both RNG states. Each seed observes 2,273 damage
blocks, 1,664 simulations, 1,947 recoil calls, 1,371 form phases, 256 boss controls
and 128 successful Wimp Out callbacks. Wimp Out has no eligible player bench in
these raw fixtures; its applied-ability sets establish the positive callback,
without claiming positive switch selection there.

A preliminary producer called the parameter-interface name as if it were a
class. Its zero-positive capture failed its own floor and is excluded. The first
appended Wimp Out transport stopped on ShowAbilityPhase; that incomplete capture
is excluded. Corrected complete captures observe both ability displays and
retain the original source outcomes. Original complete protocol 1 captures also
remain accepted and exact.

Full-turn validation initially exposed a separate storage prerequisite: native
seed 777 cases 2025, 3333 and 5988 record nine, ten and nine received attacks.
The old core retained eight and explicitly rejected those turns. Expand actual
storage to 32, retaining true lengths and the existing unsupported reporting on
reads past capacity. Keep every source input and complete-state comparison.
This bounded capacity increase does not establish parity for arbitrary longer
called-move histories. The genuine old binary and failed replay are retained as
counterexample evidence.

Both complete owning captures match all 16,384 turns with zero source exceptions,
unsupported cases or mismatches. Source successful callback calls are Disguise
1,925/Ice Face 1,717 (777), and 1,899/1,711 (2034). QuietFormChangePhase runs
2,430 and 2,437 times. Source raw damage above INT32 occurs 4,193 and 4,179 times;
all existing ally, spread, faint and switch floors remain intact. Seed 2034 also
retains four native nine/ten-record histories; no cases are removed.

Normal game-core tests pass after the history change; saved and unsaved digests
both equal `28fe3f71`. Retained doubles are 1,993/1,992/1,993; mixed turns are
1,997/2,000/1,994/1,998. Six additional covered IDs and no lost IDs, including after
rebuilding for the changed history layout. The first normal app/custom/save
sanitizer run passed; a forced fresh host rebuild is required for the header
layout change because that Makefile does not track every header dependency.

All 21 compiled runtime faults are caught, including missing post-damage dispatch,
wrong recoil/form ordering, boss clamping, immediate faint, simulation, predicates,
recipients and the owning callback. All 21 malformed captures are rejected.
All 24 retained cross-slice captures match after the layout rebuild: Secret Power
(original/observed/owning), type moves (three raw protocols/owning), Pledge,
HP-cost boosts and Pain Split. Fresh normal app/custom/save and 3DS CI on the
exact proposed head remain required before merging.

This proof does not close fusion/Transform, every passive
or held-item interaction, arbitrary history lengths, hardware acceptance,
natural wide-level progression or the complete source inventory. Normal app,
custom, save and 3DS exact-head CI remain required. Skip only the dedicated
custom-only Task63 workflow.
