# Task 103 continuation: primal-weather leave-field cleanup

Base main `efd64988c4a1ab3755f999b29c8eac0c48ad8d49`; source
`8555c08c823b856cbec4eb99ca84ea52a955836d`, English locales
`270ed0a2938b2c3e2e34e7de940e84aecadf548b`, Node 22.23.3 / GCC 14.2.

Ordinary trainer routing is already merged (#185). This slice connects the
existing Pokemon.leaveField callback to the ability dispatcher and implements
PreLeaveFieldClearWeatherAbAttr (source ab-attrs.ts:3023). Primordial Sea,
Desolate Land and Delta Stream clear their matching weather unless another
eligible holder has the same weather-clearing attribute in either ability slot.
Other holders are queried as source getField(true)/getAbilityAttrs does: their
eligibility is checked, rather than excluding them just for not being on the
drawn field. Raw arena weather is used, including when Cloud Nine suppresses its
effects. Existing reset/leave ordering, weather texts, weather-change callbacks,
and ability faint/suppression rules are preserved. Simulation does not clear;
real application without an engine is explicitly unsupported.

The additive producer is built from the unchanged trainer-switch scaffold.
It delegates original ability predicates/applications, Trainer.getNextSummonIndex
and real phases, records full before/after state, effective stats, callbacks,
phase order, RNG and texts, and rejects source exceptions or incomplete coverage.
The test framework's ordinary override API disables initialization-only ability
overrides; legal controlled primary/passive abilities then survive real reset
operations. No source handler, outcome or expected snapshot is replaced.

Twelve states cover matching/no/different weather, another matching/different
holder, passive holders, peer suppression/fainting, Cloud Nine, holder suppression
and an actual holder knockout. The last exercises primal abilities' bypassFaint
behavior. Both acting sides and all four trainer-switch families are loaded;
the knockout control substitutes real Tackle/Splash commands. Observations
include 160/160/160/161 actual FaintPhase executions at the four seeds.

```sh
source /workspace/shared/activate-parity.sh
for seed in 777 9001 4242 31337; do
  battle-core/tools/leave-weather-diff.sh /path/to/pinned/upstream 2000 "$seed"
done
TURNTEST_LOCALES=/path/to/pinned/upstream/locales/en \
  python3 battle-core/tools/leave-weather-mutations.py battle-core/build/leave-weather-777.txt
TURNTEST_LOCALES=/path/to/pinned/upstream/locales/en \
  python3 battle-core/tools/leave-weather-malformed.py \
    battle-core/build/leave-weather-test battle-core/build/leave-weather-777.txt
```

Results:

- 8,000/8,000 complete source turns pass, with zero skipped/unsupported cases,
  parse/stat/state/RNG/phase/text mismatches or missing texts. Before: 126/2,000
  covered at each seed; after: 2,000. The same fixtures add 7,496 covered turns.
  Before uses the unchanged trainer replay projection with pre-change production
  objects; the owning new driver additionally requires full coverage.
- Ten compiled production mutations fail after an intact return-0 control:
  no clearing, wrong-weather clearing, ignoring another holder, wrong peer weather,
  ignoring peer passive/suppression/faint eligibility, using effective weather,
  simulation requiring an engine, and removing the actual leave-field caller.
  Simulation mutation failure is a C API boundary result, not a simulated
  source whole-turn comparison. Six queue/simulation boundaries pass.
- Nine malformed transports return 2 after intact0: missing completion, wrong
  count/version, fractional count, case order, trailing records, duplicate/missing
  RUN and missing END. Existing missing trainer callback still rejects explicitly.
- Retained plain/rich/switch/summon seed777 and ITEMS/TRAINER four seeds:
  24,000 cases before/after, unchanged coverage and zero covered mismatches.
  Retained Moody and knockout-stat suites: 16,000/16,000 complete turns pass.
  Established producers, comparisons, seeds, counts and tolerances are unchanged.
- Battle builds/unit tests and game-core host tests pass. Paired 200-run digest
  `76dab423`; paired 1,000-run normal/save digest `1e2b9205`, 49,864 turns,
  1,000 ended each, zero stuck/crashed/timed out, 14,633 round trips.

This is ordinary loaded singles parity. Challenge legality policy, natural
partner doubles and mystery encounters remain separate. Neutralizing Gas's
source-leave lifecycle is still unsupported, not silently implemented here.
See REMAINING-BATTLE-CORE.md for subsequent source-backed slices. Host/3DS CI
results belong to the task PR; custom-core CI is excluded by user instruction.
No full battle-core, Classic-clear, emulator or console result is claimed.
