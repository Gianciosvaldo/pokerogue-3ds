# Authentic interlude sequencing

Base: merged #279, `a23a3a5ee37544e468218323f3bb77b6f6da6259`.
Source: `8555c08c823b856cbec4eb99ca84ea52a955836d`; locales:
`270ed0a2938b2c3e2e34e7de940e84aecadf548b`; Node 22.

The old CheckInterludePhase reported unsupported at X0 waves after the enemy
party fainted. It also consulted field occupants independently of the party.
The implemented condition follows Source's complete enemy-party `every` check,
including its empty-party result. Missing party records remain the owning
caller's responsibility; field presence is not substituted for party state.

Source PhaseManager.onInterlude removes every WeatherEffectPhase, BerryPhase
and CheckStatusEffectPhase from every tree level. It preserves positional tags
and any already queued PostTurnStatusEffectPhase. Only the first TurnEndPhase
found in the highest tree level is marked upcomingInterlude. That phase still
increments the battle turn, clears commands, resets battle RNG ownership and
hides the ability bar. It skips Pokemon tag/item/ability callbacks, temporary
turn counters and arena-tag lapse. Weather and terrain durations still lapse,
including weather form reversion. The transient phase flag needs no save field.

Actual-source evidence:

- The unchanged initial 2,048-case queue capture produced 625 baseline failures.
  The same capture matches exactly after the fix, including 614 changed queues.
- Expanded queue captures match 16,384 cases (8192 each, seeds 777/2034), with
  one to four nested levels, duplicate phases/TurnEnds, existing flags, empty/
  active/fainted/reserve enemy parties, X0/non-X0 controls and both RNGs.
- Actual CheckInterlude -> selected TurnEnd captures match another 16,384 cases
  at those seeds. Sunny/Grassy durations and Mist tags cover expiration and
  preservation, alive/fainted enemy controls, temporary counters, queued heals/
  messages and both RNGs. These are bounded owning-tail API cases, not a
  whole-game source comparison or a legal Classic-clear demonstration.
- Seven compiled faults are caught: retained BerryPhase, removing only one
  match, wrong TurnEnd level, ignored reserves, retained Pokemon callbacks,
  skipped weather lapse and skipped terrain lapse. Ten malformed queue
  captures are rejected.
- Native game-core fight/tail/save integration passes 68 checks in Classic
  and Endless. The non-interlude control emits hail damage; the interlude
  case emits none. Public save points are reached through reward/biome
  continuation and round-trip state. The fixture supplies wave, HP and weather
  inputs; it does not prove authentic run generation or a legal Classic clear.

The initial turn-end capture codec failed to encode Source's null battle RNG
state after incrementTurn. An early C loader omitted the no-used-ball sentinel
and actual wave seed, creating unrelated Fetch Ball/wave-data unsupported flags.
Those codecs were repaired to carry actual inputs; runtime behavior and Source
expectations were not weakened to accommodate them.

Reproduce with `tools/interlude-diff.sh PINNED_UPSTREAM [api|end] [N] [SEED]`.
Pins, Node 22 and tracked Source cleanliness are required. Gameplay methods and
formulas are unchanged; only UI/assets/timing transport is stubbed. The mutation
runner `tools/interlude-guards.py` accepts battle-core, capture directory and
pinned English locales. Complete-capture checks retain all declared cases.

The complete game-core host suite passes, including paired 200-run uninterrupted/
save-resume digest `d62eb254`. All retained 24,000 mixed, 72,000 dedicated ability
and 16,000 Ditto inputs replay without covered-case mismatches or coverage loss.
The mixed switch capture gains 94 covered cases (1895 -> 1989); the summon
capture gains 68 (1919 -> 1987). Other mixed coverage counts are unchanged.
The ordinary app's 10,000-run sample reports zero stuck/model violations and
digest `1ab9bd5d`; its zero clears do not establish a Classic completion result.
The complete app host script also passes, including normal/custom save-resume
and rematch flows. Exact-head CI results are recorded with the PR. No custom-content workflow or run-core file is changed;
no emulator/hardware readiness, doubles, final-boss or full Endless claim follows
from this interlude slice. Final-boss/doubles and wide levels remain next.
