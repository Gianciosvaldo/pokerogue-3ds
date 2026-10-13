# Battle Bond callbacks and activation state

Pinned source `8555c08c823b856cbec4eb99ca84ea52a955836d`, locales
`270ed0a2938b2c3e2e34e7de940e84aecadf548b`, Node 22.

The pin treats Battle Bond Greninja as a separate species. Its non-fused form
changes to Ash after a victory and requests the base form after fainting or
PostBattleInit. Ordinary users instead queue +1 Attack, Special Attack and
Speed when neither permanent base nor fusion species is Battle Bond Greninja
and summonData.abilitiesApplied does not already contain Battle Bond.
Temporary Transform identity does not affect hasSpecies. This is a per-summon
predicate in the pin: do not replace it with an invented once-per-battle flag.

The port now maps all four pinned ability closures, dispatches the victory and
faint form attributes through their real formFunc, and queues the registered
quiet form change. Existing stat-stage execution and source flags are retained.
The shared quiet form implementation is documented in FORM-LIFECYCLE-VALIDATION.md.

The actual-source boundary matrix has 8,000 complete cases (4,000 each at seeds
777 and 2034), 32 scenarios and six required operation kinds: normal/simulated
victory dispatch, natural opponent FaintPhase with a fatal-source record,
PostBattleInit dispatch, PostFaint dispatch and natural own FaintPhase. Four
natural source actors are used. The comparison checks the 125-field actor
vectors, world state, both RNG streams, phase levels/order, rendered messages,
stat payloads and the exact Pokemon/target form of quiet phases, plus summon
reset. Cases cover base/Ash, fusion in both directions, prior activation,
primary/passive duplicates, suppression, dead/off-field users, capped stages,
Simple/Contrary and temporary form identities. Natural own-faint cases use
non-fusion actors; fused faint naming is a separate context. This matrix is
phase/API proof, not complete doubles battles.

A second additive matrix executes 8,000 complete source turns (2,000 each at
seeds 777, 9001, 4242 and 31337). It uses the unchanged Fell Stinger/knockout
turn scaffold and comparator, replacing only its input ability matrix and an
already-applied control. Original source methods execute unchanged; callback
instrumentation delegates to the original implementations. All 22 scenarios,
both sides, ordinary/passive activation, Simple/Contrary, stage caps,
suppression, indirect/no-knockout and repeated-use controls are required.
All turns are covered, with exact text, phases, state and both RNG streams.
The inherited 16 queue/simulation checks still test the legacy knockout
abilities and are labeled accordingly.

Twelve compiled mutations are caught: raw/fusion/temporary identity,
already-applied refusal, wrong victory/reset forms, disconnected faint and
form callers, simulation mutation, doubled stat payload and wrong queued form
target. Fifteen malformed phase traces and five malformed turn traces are
rejected; quiet targets and queue indices are range-checked before narrowing.
Complete-case guards reject omissions and unsupported cases.

Nineteen host integration checks perform an actual KO and wave transition,
verify the three stage boosts, retain the activation bit at the next legal
pre-turn save point, load it and refuse another activation, and clear it on
summon reset. Mid-turn saves remain unavailable. A separate Ash fixture
checks the reset API and queued quiet phase. Ordinary same-arena continuation
keeps Ash, as in the source.

Reproduce:

```
battle-core/tools/battle-bond-diff.sh /path/to/upstream 4000 777 phase
battle-core/tools/battle-bond-diff.sh /path/to/upstream 4000 2034 phase
battle-core/tools/battle-bond-diff.sh /path/to/upstream 2000 777 turn
make -C game-core battle-bond-host-test
```

The source Scene.newBattle resetArenaState boundary invokes PostBattleInit
before ReturnPhase clears summon data. That game-core reset controller is not
yet wired to these callbacks and needs its own source/order proof; the API
matrix does not establish that integration. Fusion faint naming, broader
forms/native gain-loss, held-modifier scene refresh and complete doubles remain
separate work. No full Battle Bond integration, battle-core, emulator or
hardware parity is claimed by these matrices.
