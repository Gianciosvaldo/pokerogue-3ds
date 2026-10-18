# Meloetta's Relic Song form changes

Source `8555c08c823b856cbec4eb99ca84ea52a955836d`, locales
`270ed0a2938b2c3e2e34e7de940e84aecadf548b`, Node 22.23.3.

The registered `MeloettaFormChangePostMoveTrigger` rejects Single Type challenges
and an active Sheer Force, including the passive. Otherwise its inherited
predicate reads the newest history entry and checks for Relic Song. It does not
require a successful result, a particular use mode, a current Relic Song command
or positive HP. The scene matches the raw aria/pirouette form key and queues a
quiet transition in the requested immediate/delayed position. Existing quiet
phase execution handles the later stats/type/name/message changes.

Add the positive branch to the existing scene-trigger helper. Give battle state
an active challenge bitmask and pass the run's known Challenge-mode flags through
the real game setup caller. A new host test plays Relic Song through the normal
command interface and verifies that Single Type prevents the change. This does
not close the pre-existing global challenge inventory or challenge-save input
migration. Unknown/unsupported challenge sets retain the run layer's existing
unsupported reporting; this slice's game proof supplies known live flags.

Reproduce `tools/meloetta-forms-diff.sh UPSTREAM 777`, then 2034. The producer
captures native battle RNG before constructing GameManager and restores it after
startup. Challenge setup uses actual GameMode.setChallengeValue; scene and phase
methods execute unchanged. Asset/UI/phase-advance stubs delimit presentation.
The initial producer used the nonexistent `setChallenge` method and failed
before writing any capture; that diagnostic is excluded.

All 16,384 direct cases match exactly across both seeds: 125-field input,
post-queue and post-execution snapshots, complete queue levels/order/form targets,
form ability sets, text and both RNG states. Cases include both sides, forms,
HP zero/alive, removed field and switch-out states, levels 1/50/256/1,000,000,
primary/passive/suppressed Sheer Force, current-command decoys, empty histories,
Relic Song/Tackle with Pending/Success/Fail/Miss and normal/indirect/follow-up use
modes, immediate/delayed queues and real Single Type challenge flags. Each seed
queues 138/124 transitions to forms 0/1 immediately, and 138/124 delayed, with
704 mono-type and 180 active-Sheer-Force refusals.

The real game host test passes 12 checks for challenge flag propagation and
owning command behavior. The first host test treated setup_field's boolean as
success; it actually means a new lead was sent out. The corrected test checks the
resulting field, flags and form behavior, with no production change for that
harness mistake. Normal game-core tests pass, with saved/unsaved digests both
`28fe3f71`. Retained regressions gain one covered ID and lose none versus the
rebased Disguise/Ice Face parent: doubles 1,993/1,994/1,994; mixed
1,997/2,000/1,996/1,998.

All 16,384 owning turns match exactly with zero source exceptions or unsupported
cases. The native traces execute 10,331 and 10,368 QuietFormChange phases, in
4,712 and 4,732 distinct turns. Source scene-trigger successes agree with those
execution counts. Raw damage above INT32 occurs 5,218 and 5,211 times, up to
604,701,924,891 and 577,524,310,289. Existing ally/spread/faint/switch floors remain
intact, and the guard independently requires at least 100 native turns with an
executed form phase.

All 16 compiled faults and 21 malformed captures are caught. Faults exercise the
history predicate, challenge bit, primary/passive/active Sheer Force, faint gates,
form keys, delayed queue position, eager mutation, owning caller and quiet-phase
execution. All 28 cross-slice captures pass, including Disguise/Ice Face raw and
owning traces plus Secret Power, type moves, Pledge, HP costs and Pain Split.
The local 3DS build passes with the same devkitARM image/command as CI.
Fresh normal app/custom/save sanitizer checks remain required before merge.

These bounded default-mode canonical turns and specific live challenge
inputs do not establish complete battle-core, arbitrary fusion/Transform or
challenge/save parity. Preserve the separate documented enemy-Palafin deviation;
source differential fixtures leave its flag clear. Normal app/custom/save tests
and exact-head 3DS validation remain required. Only dedicated Task63 is skipped.
