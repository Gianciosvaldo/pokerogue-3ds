# Weather forms and ally lookup

Target: upstream `8555c08c823b856cbec4eb99ca84ea52a955836d`, locales
`270ed0a2938b2c3e2e34e7de940e84aecadf548b`, Node 22.23.3, GCC 14.2.

Castform and Cherrim now use the scene's requested queue mode. Ordinary active
triggers unshift; explicit delayed callers push. Faint/leave delays from the
Stance Change repair remain explicit. RevertWeatherFormTrigger also preserves
the source's distinction between unset summonData.ability and explicit NONE:
undefined differs from both the native ability and NONE, so it permits reversion.
This is the pinned behavior, including its unintuitive unset-ability case.

The complete-turn proof also found a shared getter error. Pokemon.getAlly uses
indexOf in the side's first one/two party slots; a reserve has index -1 (truthy),
so it returns the first party member even in singles. The port now follows that
lookup. This matters when querying a reserve's effective stats with Flower Gift
on the field. It does not enable a second battler in singles.

Validation:

- Registered scene and held QuietFormChangePhase capture: 16,384 cases each at
  seeds777/2034. Checks raw/effective identity, stats, HP, abilities, suppression,
  Tera, primary/passive/lost abilities, all weather values, both sides, activity,
  default/explicit delay, full queue hierarchy, messages and both RNG streams.
  Final capture adds Cloud Nine controls and explicit NONE/native overrides.
  Both final captures match32,768/32,768 exactly.
- Complete singles turns: 2,000 each at seeds777/9001/4242/31337. Actual Sunny Day,
  Rain Dance and Hail, weather lapse, native/passive/lost/suppressed abilities,
  Cloud Nine, faint, switch, re-entry and victory. Established whole-turn
  comparison and tolerances are unchanged; all8,000 turns are covered and exact.
- Actual getAlly: 8,192 cases match across both sides, singles/doubles, empty and
  one-to-six-member parties, field/reserve actors, fainted and switching actors.
- Host weather commands, legal save/load and biome recall:112 checks passed.
  No can-save flag is forced. Weather/form/stat state survives legal saves;
  recall snapshots precede reversion, including after the native ability is lost.
- Eight compiled behavioral faults are caught; seventeen malformed captures are
  rejected, including incomplete API/turn/getter captures.
- Complete game-core suite passes; paired ordinary save/no-save run digests are
  both`f44fb2f0`.
- Retained24,000 mixed turns have unchanged coverage and no mismatch; retained
  72,000 dedicated ability turns and16,000 Ditto cases also pass.
- Full app host suite passes, including10,000 ordinary runs (120,360 waves),
  ordinary saves, custom runs and custom slot resumption. No screen/run mismatch,
  stuck command, model violation or failed save is reported.
- Host and3DS CI are separate publication gates; see the PR check results.

The first 120 complete turns had eight input mismatches in reserve effective
stats. The shared getter fix makes the same unchanged capture120/120 exact.
The initial suppression mutation survived the scene-only matrix because that
capture did not contain a suppressing actor. The final matrix adds actual Cloud
Nine controls; do not count the initial mutation run as passing validation.

Reproduce:

```sh
source /path/to/activate-parity.sh
battle-core/tools/weather-form-diff.sh /path/to/pinned-upstream api 16384 777
battle-core/tools/weather-form-diff.sh /path/to/pinned-upstream turn 2000 777
# Repeat API seed2034 and turn seeds9001/4242/31337.
# Copy only the additive ally producer into upstream test/tests/zz-dump:
ALLY_OUT=/abs/ally.txt node node_modules/vitest/vitest.mjs run \
  test/tests/zz-dump/ally-lookup.producer.test.ts --maxWorkers=1
battle-core/build/ally-lookup-test /abs/ally.txt
TURNTEST_LOCALES=/path/to/pinned-upstream/locales/en \
  python3 battle-core/tools/check-weather-proof.py /abs/weather-api.txt /abs/weather-turn.txt
# check-weather-proof expects ally.txt alongside the API capture.
make -C game-core test
ASAN_OPTIONS=detect_leaks=0 JOBS=2 game/tests/run_host_tests.sh
```

The local app run disables LeakSanitizer because the execution environment uses
ptrace; normal CI sanitizer settings remain unchanged. No source gameplay method,
formula, registration, expectation or retained exclusion is replaced. Asset/UI
and timing transport stubs are inherited from the established phase harness.

This slice does not close native fusion/Transform form registrations, item/form
eligibility, full doubles turns, Pending Heal, wide levels or final-boss/run
prerequisites. No emulator or hardware validation is claimed.
