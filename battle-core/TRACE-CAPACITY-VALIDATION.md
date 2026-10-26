# Long turns and diagnostic trace capacity

Reference: upstream `8555c08c823b856cbec4eb99ca84ea52a955836d`, locales
`270ed0a2938b2c3e2e34e7de940e84aecadf548b`, assets
`87426a79611f9d212c4dc8af58e2834a05b93725`.

The phase trace is diagnostic storage. Filling its 512 entries previously set
`engine.overflow` and stopped the battle before the next phase. A real source
turn with Population Bomb, three Instruct commands, Weak Armor and an enabled
Stamina passive runs 607 phases from TurnStart through Berry. The port stopped
at 512, never reached Berry and omitted the end of the turn's presentation.

The engine now retains the same first 512 entries and reports later omitted
entries through `trace_overflow`. It continues running the actual phases.
Victory continuation also reports an omitted diagnostic entry. Genuine phase
pool/queue/target-storage errors keep their existing failure behavior. There
is no capacity increase, allocation, save-schema change or enum renumbering.
The new flag is transient engine state and resets with a fresh engine call.

## Native evidence

`trace-capacity.producer.test.ts` captures the native BattleScene battle RNG
before constructing GameManager and restores it after battle startup. It runs
every real phase using the unchanged framework per-phase driver. The framework's
20-second whole-turn helper is too short for this legitimate long sequence.
No gameplay method, phase, RNG implementation or framework source is replaced.
UI and damage wrappers observe actual output and call through unchanged.

Controlled initial inputs are two level1000 Blissey with Natural Cure and two
level1000 Regirock with Weak Armor and enabled Stamina. IV15, nature0, natural
HP/stats, explicit Sandstorm, user ATK-6/ACC+6 and speed stages0/-1/-3/-2 are
retained. The four initial commands execute in natural speed order, including
ability-driven speed changes. This tests enabled ability/passive interaction;
it does not claim generated Regirock normally has this combination or that
ordinary NPC AI chooses those Instruct commands. No histories, HP, PP or
phase queues are repaired after the turn starts.

The official capture records 612 phase names (five command/target setup phases
precede TurnStart), all 144 actual UI texts, forty damage observations and
received records, before/after actors, passive IDs, PP, move histories,
weather/terrain and both RNG streams. The source fixture and C include reproduce
byte for byte from a fresh capture. Locale dictionaries are the eight actual
pinned English resources passed to the production C formatter.

The C test observes every real `pr_phase_run` call through a linker wrapper,
including phases beyond the stored prefix. It compares all 607 source phase
names through Berry, all texts and hits in order, critical flags, actor IDs,
HP/stages/PP/passives, retained histories/received records and RNG. The complete
555-event core sequence arrives in three streamed chunks. C then runs the two
later end-turn phases and reaches TurnInit with no engine/unsupported/event
failure; only diagnostic trace truncation is reported.

## Retained checks and limits

- The 2,034-check native comparison and 1,708 raw continuation checks run in
  normal CI. Raw guards
  cover prefix preservation, the exact boundary, omitted Victory-only entries
  and genuine phase-pool exhaustion; they are distinct from native evidence.
- Five compiled production faults are detected. Fifteen malformed captures are
  rejected, including passive and phase-type corruption. Three well-formed
  forged observations (late phase, last message and battle RNG) fail comparison.
  Compilation failures do not count as caught production faults.
- Scoped ASan/UBSan/LeakSanitizer runs instrument the changed engine owner and
  comparison/guard callers, with ordinary optimized supporting objects. This
  is not full-program sanitizer coverage.
- The earlier 361-event native proof and Tera guards remain passing. Normal
  core/app/custom/save/football and actual ARM build/stack checks remain required;
  only the dedicated Task63 check is skipped.

```sh
# Node22, clean pinned source/locales and installed dependencies:
bash battle-core/tools/reproduce-trace-capacity.sh /path/to/upstream /path/to/output
python3 battle-core/tools/check-trace-capacity-proof.py
```

The fixed trace prefix is explicitly incomplete on long turns. A consumer
requiring a full trace must observe all phases rather than treating the prefix
as complete. This proof collects core chunks. The subsequent
[controller storage proof](../game-core/CONTROLLER-EVENT-STORAGE-VALIDATION.md)
passes all 555 events through the app's optional growable storage and converter;
default controller clients still have an explicit 384-event limit. Other
capacities remain open in the [completion checklist](COMPLETION-CHECKLIST.md).
No builder is released.
