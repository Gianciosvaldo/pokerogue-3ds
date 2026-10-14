# Copied native abilities disabled during Transform

Source: `8555c08c823b856cbec4eb99ca84ea52a955836d`; locales:
`270ed0a2938b2c3e2e34e7de940e84aecadf548b`; Node 22.

The pin's actual PokemonTransformPhase copies the target ability, including
abilities otherwise marked uncopiable. Pokemon.canApplyAbility then disables
abilities carrying NoTransformAbilityAbAttr while the actor is transformed.
The core checked NoFusionAbilityAbAttr but omitted this separate gate. A copied
Hunger Switch, Disguise, Shields Down or Ice Face could consequently activate
callbacks or damage protection that upstream rejects. The gate now precedes
ordinary ability suppression, without preventing the copy itself.

## Source evidence

The unchanged 512-case source smoke produced **224 baseline failures**: wrong
canApply predicates and wrong per-summon/wave callback sets. The same capture
matches all 512 cases after the gate is added. Expected output is not modified.

The expanded API matrix matches **16,384** complete cases (8192 each, seeds
777/2034). It executes actual PokemonTransformPhase.start and subsequent actual
PostTurnAbAttr dispatch, with full actor vectors before, after Transform and
after dispatch, both primary/passive predicates, normal/simulated controls,
suppression, HP boundaries, callback flags, both RNGs and hierarchical queues/text.
Eight target species cover restricted abilities and positive controls that remain
usable after Transform: Morpeko, Mimikyu, Minior, Eiscue, Darmanitan, Wishiwashi,
Zygarde and Aegislash.

The independent owning-turn matrix matches **8,000** ordinary singles turns
(2000 each, seeds 777/9001/4242/31337). It first performs an actual Transform turn,
asserts the native target ability was copied, then proves the following complete
damage/status turn. Source and core state, trace, RNG and text comparisons retain
the existing comparator/tolerances. Water Gun and Growl exercise damage and
stat-change ordering; low-HP states reach real faint, switch and victory phases.
The first Transform turn is setup for this projection; the dedicated API matrix
and retained Transform suite separately compare that phase.

Presentation assets/UI timing are transport stubs; no upstream gameplay method
or formula is replaced. Source producers and complete-fixture runners are
committed. All **6 compiled faults** are caught and **13 malformed fixtures** rejected,
including omission of the gate in a complete damage turn. **64 real game-core
command/legal-save checks** preserve copied restricted abilities while rejecting
their effects before and after load at actual save points. The full game-core
host suite and paired save/no-save digest (`f44fb2f0`) pass. Retained 24,000 turns
have unchanged coverage and no mismatch; the prior 72,000 dedicated ability
turns and 16,000 Ditto phase/turn cases also remain unchanged and pass.
The full app suite passes, including its 10,000-run soak and ordinary/custom
save/resume flows. Exact-head host/app/3DS CI is recorded in the PR once complete. No hardware/emulator claim is made.

## Reproduction and remaining boundaries

`tools/form-interaction-diff.sh UPSTREAM [api|turn] [N=8192] [SEED=777]` checks
pins and refuses tracked source changes. With generated traces, run
`tools/check-form-interaction-proof.py API TURN` using
`TURNTEST_LOCALES=UPSTREAM/locales/en`. Run
`make -C game-core transform-restriction-host-test` for legal command/save checks.

This closes the copied-ability NoTransform gate. It does not establish all native
fused/Transformed form registrations, generic weather-form timing, item forms,
Challenge rules or complete doubles. Upstream's moveset-generation shortcut is
outside this ordinary battle projection. Existing NoFusion behavior is retained.
No run-core source changes or custom-core/Task63 checks are included; ordinary
custom-run/save app tests stay required. Local sanitizer tracing needs
`ASAN_OPTIONS=detect_leaks=0`, while CI uses its normal settings.
