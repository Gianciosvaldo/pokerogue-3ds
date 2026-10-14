# Stance Change registered quiet transitions

Source pin: `8555c08c823b856cbec4eb99ca84ea52a955836d`, locales:
`270ed0a2938b2c3e2e34e7de940e84aecadf548b`; Node 22.

## Behavior

Aegislash now queues the registered QuietFormChangePhase before changing form
and stats. The pre-move trigger reads the selected turn command, including when
the currently executing phase belongs to a called move. King's Shield changes
Blade to Shield; damaging commands change Shield to Blade when Stance Change
can apply. Leaving the field checks activity with field presence and reverts
Blade without requiring Stance Change to remain available.

FaintPhase and Pokemon.leaveField explicitly request delayed queue placement;
ReturnPhase.onEnd retains its ordinary unshifted trigger. The quiet phase owns
the mutation, ability callbacks, form text and duplicate-form guard. This fixes
inline mutation/message ordering rather than suppressing the source's form text.

## Validation

- **16,384** actual scene-trigger and held Quiet-phase cases (8192 each, seeds
  777/2034): complete actor vectors, before-mutation state, exact queue levels,
  indices, current level/deferred flag, primary/passive and missing-ability cases,
  inactive/fainted actors, quiet results, callback sets, text and both RNGs.
  The selected command deliberately differs from the actual current MovePhase.
- **8,000** actual complete ordinary singles turns (2000 each, seeds
  777/9001/4242/31337), all covered and exact with the unchanged comparator and
  tolerances. Includes damaging/status commands, King's Shield, actual sleeping
  Status objects and Sleep Talk calls, suppression, primary/passive conditions,
  Water Gun knockouts, FaintPhase, switches and victory. Each seed reaches 666
  quiet phases; carrier knockout cases reach at least 750 per seed.
- **7 compiled faults** caught and **13 malformed fixtures** rejected. Tests must
  catch current-phase versus command confusion, incorrect leave-field predicates,
  omitted queueing, wrong delayed placement, King's Shield routing and immediate
  faint reverts. Queue hierarchy is compared, not just its flattened phase list.
- **54 real game-core command/save/recall checks**, including form-dependent
  commands, ordinary wild continuation with Blade preserved, actual legal save
  points and roundtrips, biome recall, and reversion after losing the ability.
  The recall snapshot remains Blade; the queued quiet mutation produces Shield
  before the next encounter's save point. In-flight saving is not enabled.
- Full game-core host suite passes; paired save/no-save digest remains
  `f44fb2f0`. Full ordinary/custom/save/resume app suite passes, including its
  10,000-run soak and evolution coverage (63 completed, 56 cancelled, 13 later
  moves), with zero stalls/model errors. Local tracing requires
  `ASAN_OPTIONS=detect_leaks=0`; CI keeps its normal sanitizer configuration.

Retained 24,000 ordinary turns have no covered loss or mismatch. Rich seed777
increases from 1,955 to 1,963, plain from 1,965 to 1,971 and summon from 1,914 to
1,919. The prior 72,000 dedicated ability turns and 16,000 Ditto phase/turn cases
remain unchanged. Exact-head host/app/3DS CI is recorded in the PR before merge;
no emulator or hardware validation is claimed.

## Reproduction and limits

`tools/stance-diff.sh UPSTREAM [api|turn] [N=8192] [SEED=777]` checks pins and
tracked upstream cleanliness. After producing fixtures, run
`tools/check-stance-proof.py API TURN` with `TURNTEST_LOCALES=UPSTREAM/locales/en`.
Host integration: `make -C game-core stance-change-host-test` and the ordinary
`make -C game-core test` / game app suite.

The API executes captured quiet objects separately while a presentation anchor
remains queued; it does not claim a complete owning turn. The separate complete
turn matrix provides that proof. Asset, audio/UI and timing transport follows
existing test scaffolding; upstream gameplay methods/formulas are untouched.

The first API draft compared flattened queues: a delay fault could survive, so
v2 fixtures capture actual queue levels and now reject it. An initial C anchor
used unshift while the source used push; corrected transport matches the source
capture unchanged. Initial Sleep Talk setup used a plain status object, then a
redundant Status import; those failed source drafts are excluded. Final producer
uses the actual existing Status class. Knockout fixtures use Water Gun because
Aegislash is immune to Tackle. Host save checks proceed through real rewards to
legal save points instead of forcing the save flag.

Native fused/Transformed form transitions, general weather-form scheduling,
remaining item/form registrations, Challenge eligibility, Pending Heal and
complete doubles/Commander/redirects are separate unfinished boundaries. No
run-core source is edited. Custom-core/Task63 CI is excluded per standing user
instruction; ordinary custom-run/save tests stay required.
