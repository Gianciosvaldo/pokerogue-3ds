# Player cleanup and Return at an arena reset

Source: `8555c08c823b856cbec4eb99ca84ea52a955836d`; locales:
`270ed0a2938b2c3e2e34e7de940e84aecadf548b`; Node 22.

The player cleanup now dispatches PostBattleInit before Return clears summon
state. Queued quiet forms execute first. Ordinary wild continuation keeps Ash
Greninja and its summon state. Game-core checks the incoming battle type, as
upstream does, and runs cleanup before generating the next encounter and
publishing its save point. Direct setup callers with an outgoing field also
recall it before rebuilding pointers. Loading a save does not rerun cleanup.

Return runs the inherited PreSummon callback and leaveField/onEnd sequence,
without adding the ordinary switch-out callbacks. Natural Cure and Regenerator
therefore do not activate here. Battle-and-wave reset clears wave history,
endurance and wave turn count; Encounter setup also resets wave-only data on
ordinary continuation without clearing summon history. Tera and Stellar boosts
are cleared on an arena reset.

## Actual source proof

The producer calls the unchanged Scene.doPostBattleCleanup, drains its actual
phase tree and dynamic Return queue, and starts the actual QuietFormChangePhase
and ReturnPhase. Asset/audio/UI transport is stubbed; gameplay methods,
conditions and formulas are unchanged. Delayed callbacks run after leaveField,
preserving the source timer order. Encounter, trainer-display and level-cap
phases are projected out because this matrix ends at player cleanup.

- 4,000 complete cases on seed 777 and 4,000 on seed 2034, all exact.
- Four owning contexts: ordinary wild after a trainer, new biome, incoming
  trainer and incoming mystery encounter; 32 actor states and old single/double
  field sizes, including inactive reserves, suppression, passives, temporary
  identity and already-used summon history.
- An additional 256 actual-source Tera cases on seed 4242, all exact.
- Comparisons include four 125-field actor vectors before preparation, after
  preparation and after execution; wave endurance flags; 14 world fields;
  both RNG streams; pending tree/dynamic queues including form targets;
  executed phase order and exact rendered messages. No skipped cases or
  unsupported results are credited.
- Eleven compiled runtime faults are caught and 14 malformed traces are rejected.
  The portable guard runner covers callback timing, recall priority,
  old field size, wave reset, ordinary switch-out effects, Return and Tera reset.
  Malformed complete-case transports are rejected separately.

Reproduce:

```sh
battle-core/tools/reset-arena-diff.sh /path/to/upstream 4000 777
battle-core/tools/reset-arena-diff.sh /path/to/upstream 4000 2034
RESETARENA_TERA=1 battle-core/tools/reset-arena-diff.sh /path/to/upstream 256 4242
TURNTEST_LOCALES=/path/to/upstream/locales/en \
  python3 battle-core/tools/check-reset-arena-proof.py \
  battle-core/build/reset-arena-777.txt --tera battle-core/build/reset-arena-4242.txt
make -C game-core test
ASAN_OPTIONS=detect_leaks=0 JOBS=4 game/tests/run_host_tests.sh
```

The local LeakSanitizer setting is needed because the managed host uses ptrace;
address and undefined-behavior sanitizers remain enabled. CI uses normal settings.

## Integration and limits

The Battle Bond host test has 29 checks covering KO activation, same-arena
continuation, the real wave-11 controller, callback-before-recall form snapshots,
Tera/Stellar clearing, wave reset and legal save/load without duplicate callbacks.
Full game-core tests pass; 1,000 runs finish both with and without saves, and the
paired 200-run digest is `c6cf5f73` on both paths. These are consistency checks,
not upstream run parity.

The first app soak reproduced the pinned Helping Hand stalemate: a trainer's
Ditto and the player's Jirachi had only the ally-only move left, which fails
without spending PP. The retained 80-turn source proof remains exact. The fake
player now chooses to replace nearby-ally moves when learning a usable singles
move. Game behavior, source expectations, seeds, run counts, progress budgets
and coverage floors are unchanged. This is a legal test-player decision,
not a runtime exception for stalled runs. The 10,000-run app check passes:
119,603 waves, 485,432 commands, 57 completed and 61 cancelled evolutions,
16 post-evolution moves checked, and zero stalls or model violations. The
full app suite also includes ordinary saved runs and custom saved/resumed runs.

This closes the non-fused Battle Bond player-cleanup caller. The source matrix
uses an empty new enemy party and a clean arena. Complete newBattle generation,
time-of-day forms, arena-tag removal callbacks/messages, Neutralizing Gas,
Commander, fused naming, native form gain/loss combinations and full doubles
transitions remain separate work. The first fusion probe exposed fused return
names that the existing formatter does not represent; it was kept as a finding,
not credited as passing fusion coverage. No emulator, hardware or whole-core
completion is claimed.
