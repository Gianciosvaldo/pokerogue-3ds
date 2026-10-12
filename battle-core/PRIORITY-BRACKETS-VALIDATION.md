# Within-bracket move ordering

Base main `fa1cb29bae82397863f1a08bb40affeeac9e4506`. Source pin
`8555c08c823b856cbec4eb99ca84ea52a955836d`, locales
`270ed0a2938b2c3e2e34e7de940e84aecadf548b`, Node 22.23.3 / GCC 14.2.

Move.getPriorityModifier gives an existing bypass-speed tag precedence, then
dispatches ChangeMovePriorityInBracketAbAttr with source-default simulation.
Stall always assigns LAST; Mycelium Might assigns LAST for status moves. These
source closures are registered by owner/hash, without changing generated data.

TurnStartPhase dispatches Quick Draw in speed order before Quick Claw. Source
Quick Draw draws from battle RNG before checking damage category or tag
eligibility, even when those later checks refuse it. Simulation does not draw.
Actual BypassSpeedTag.canAdd dispatches Mycelium Might's refusal using the
selected command; the earlier Pokemon.canAddTag eligibility stub does not.
The shared Mycelium predicate also fills its existing move-ability-bypass leaf;
this does not close all ability-bypass behavior.

The new source builder uses the unchanged Speed Boost scaffold. It observes and
delegates all three actual attribute classes and drains actual upstream phases.
Three ability families each have 16 loaded states on both sides: faster/slower,
damage/status/priority moves, suppression, passive slots, combined abilities,
existing bypass tag, canonical Quick Claw, Mycelium refusal, opponent abilities,
speed ties and speed stages. Every state and both sides occur on each seed;
every observed class applies and refuses. Framework RNG routing and only asset/UI
boundaries use the established scaffold; source handlers are not replaced.

Two actual getPriorityModifier probes per case compare source numerical values
with the public version of the C queue's existing calculation. Actual simulated
Quick Draw calls assert unchanged source state/RNG; the C projection checks RNG
and its full before-state comparison. No established comparator is edited and
ordinary state/effective-stat/RNG/phase/text comparisons remain unchanged.

```sh
source /workspace/shared/activate-parity.sh
for seed in 777 9001 4242 31337; do
  battle-core/tools/priority-brackets-diff.sh /path/to/pinned/upstream 2000 "$seed"
done
TURNTEST_LOCALES=/path/to/pinned/upstream/locales/en \
  python3 battle-core/tools/priority-brackets-mutations.py \
    battle-core/build/priority-brackets-777.txt
TURNTEST_LOCALES=/path/to/pinned/upstream/locales/en \
  python3 battle-core/tools/priority-brackets-malformed.py \
    battle-core/build/priority-brackets-test battle-core/build/priority-brackets-777.txt
make -C battle-core all test
make -C game-core test
game-core/build/gcplay --auto 1000 --jobs 8
game-core/build/gcplay --auto 1000 --jobs 8 --save-test
```

Results:

- Baseline C rejects every seed's new fixture: 920 numerical bracket differences
  (reported by the before-snapshot reader as parse mismatches), 1,038 unsupported
  turns, only 42 covered exact turns. For the numeric oracle only, a scratch copy
  exposes the baseline private priority function; its body and queue calls are
  unchanged. The strict wrapper never credits the baseline subset as a pass.
- Four seeds × 2,000 complete turns and 16,000 bracket probes all match, with
  zero parse/stat/state/RNG/phase/text differences, unsupported/skipped cases or
  missing text. Simulated Quick Draw does not advance RNG.
- Twelve compiled production mutations fail after intact exit 0: wrong bracket,
  disabled chance, skipping status RNG, allowing status activation, simulated
  draws, bypass refusal disabled, missing chance/tag/bracket dispatches, wrong
  existing-tag precedence, inverted Mycelium condition and Claw-before-Draw order.
- Twelve malformed traces return 2 after intact 0: missing footer, count/version,
  fractional count, case order, trailing records, duplicate/missing RUN, missing
  END, missing/duplicate bracket probe and nonfinite bracket value.
- Retained 24,000 cases compare baseline and changed C: no covered mismatch or
  coverage loss. Rich seed777 rises 1,872→1,896; summon 1,868→1,869. Plain 1,958,
  switch 1,891, ITEMS 1,939/1,945/1,940/1,956 and TRAINER
  1,946/1,943/1,945/1,953 stay unchanged. Retained Moody, knockout-stat,
  leave-weather and Wonder Skin suites: 32,000 complete turns pass unchanged.
- Battle builds/boundary checks and game-core host tests pass. Paired 200-run
  digest `031085fd`; paired 1,000-run normal/save digest `037d9cb9`, 49,864 turns,
  1,000 ended each, zero stuck/crashed/timed out and 14,633 save round trips.

Host/3DS CI is recorded on this task's PR. Other agents' concurrent game/game-core
merges are preserved; local source/save numbers above identify this branch's
base. Custom-core checks are excluded by the user's instruction. This proves
the loaded ordinary singles path, not all doubles, scripted final-boss phases,
whole-run upstream parity, emulator behavior or console acceptance.
