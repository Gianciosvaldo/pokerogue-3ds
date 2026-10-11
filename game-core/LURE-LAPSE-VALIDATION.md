# D12 Lure expiry at BattleEnd

Baseff728e20e7443b4c41116c0c5e02dd10f4bcc5aa. D11 #208 merged;
concurrent docs-only #207 imported intact. Clean branch; source
8555c08c823b856cbec4eb99ca84ea52a955836d, Node22.23.3.

Evidence before production edits:

|Source symbol|Intended C behavior|Fixture|
|---|---|---|
|BattleEndPhase.start battle-end-phase.ts19–93|cull duplicate boundaries, score/abilities, enemy cleanup, then lapse all records before rewards|victory/flee/nonvictory and duplicate queue|
|LapsingPersistentModifier.lapse modifier.ts390–395|one decrement per record; remove<=0, independent of stack|all kinds1/2/max, stack1/2, duplicates|
|Scene.removeModifier/updateModifiers|preserve survivor order, refresh after all expiry|interleaved X/Dire/held stock and multiple expirations|
|pr_reward_lapse_lures reward-core/src/lure.c44–62|bounded canonical timers; opaque other fields preserved|all three durations, atomic count0 guards|
|gc_phases.c battle_end/gc_temp_lapse/gc_battle_end_cleanup|enemy cleanup before timers; keep temporary clock; no per-turn/KO tick|actual C phase invocation, trainer member KO control|
|gc_run_can_save/gc_run_save/load named battle_count|save only at legal encounter boundary; load then execute same boundary once|uninterrupted/loaded copies, post-boundary save refusal|

Final executed proof follows; exact-head CI is required before merge.
PostBattle abilities/Pay Day/natural doubles remain outside this card.

## Executed boundary proof

Node22.23.3/GCC14.2.0; `game-core/tools/lure-lapse-diff.sh UP 2000
'777 9001 4242 31337'`. All8000 requested source cases complete without skips/
throws and replay exactly; source real BattleEnd executes both uninterrupted
and restored real ModifierData copies (16000 phase calls), source resume output
matches the uninterrupted stock/queue/callback output. The C replay invokes the
actual RP_BATTLE_END handler on uninterrupted and legally saved/loaded copies.
Final replay files d12-legal-save-replay-* each2000/2000,0 differences.

Each seed:1672 Lure and675 independent Dire expirations;795 effective victories,
603 flee and602 nonvictory boundaries;1333 queued-duplicate cases,0/1/2 queued
BattleEnds and mixed victory flags;2000 actual trainer member Victory no-tick
controls;2000 legal C resume comparisons. Fixture kind groups667/667/666, all
actual canonical kinds/stack1/2/timers1/2/max and duplicate/interleaved stock.
67 cases per seed have no Lure and prove the new clock is a no-op while Dire
still lapses. These are supplied phase boundary fixtures, not legal whole-turn
trainer/doubles or generated enemy-party proofs.

Observed source order: enemy held cleanup (including its own enemy-side refresh),
then one lapse per matching modifier RECORD in stock order, corresponding
removals, final player-side refresh, rewards. Actual observers delegate originals.
The two C family adapters commute for this bounded independent-counter domain;
stock survivor order/identity/flags/args/timers and expiry agree, temporary
counters remain correct. C performs no extra turn/KO/new-wave ticks. Source/C
message and boundary event queues are empty here; reward selection phase count
is compared after duplicate culling. Pokemon54 fields, wallet and global RNG
are identical. Source lazy battle RNG snapshot and C prepared battle RNG each
remain unchanged (no battle draws); this does not claim constructor-RNG parity.
Source gameStats/achievement bookkeeping, post-battle abilities/Pay Day,
presentation/graphical destroy/phase completion and hardware are excluded.

C saves at the untouched real encounter COMMAND boundary obtained through
actual gc_run_set_starters, BEFORE the supplied trainer Victory controls.
No save flags/queue/current screen are restored to enable saving. Loaded
encounter display events WAVE/SUMMON/SAVE_POINT are separately asserted/drained
before the compared boundary. Both copies then receive the same nonparticipating
trainer-member control input (loader normally registers the lead participant;
that is not a claim that an empty participant set survives session rehydration).
After BattleEnd both copies refuse saving, duplicates are absent, clocks have
advanced once, and all stock remains source-exact. Profile state remains
unchanged by the timer adapter. Source ModifierData constructors preserve timer/
stack; account/session persistence and complete constructor parity are separate.

25 host boundary guards pass, including atomic legacy count0/invalid timer/
stack/owner/virtual/detached/challenge/unknown projection refusal, untouched
expired output on refusal, opaque no-Lure no-op, record compaction and independent
Dire clock. Legacy/unrepresented Lure stock remains frozen behind the existing
counted post-battle fallback, without invented remaining lifetime.
44 malformed owning transports exit2 after intact0. Rebuilt real-handler
mutations: double Lure tick1868 differences, missing Lure tick1933 differences,
exit1 each; intact2000/2000 exit0. Full four-seed legal-save replay passes again.
No source expectations or established producer/comparator were edited.

Existing D11/D10 owning8000 each, retained raw Lure8000 and26 guards, temporary
8000, acquisition/candy/apply/ball/Bell/Orb/Wide/Multi regressions pass unchanged.
Evidence explicitly reuses the retained pinned traces; raw Lure proof was freshly
executed in D11 and reward-core is unchanged here. Final H rebuilt before soak:
make -j2 -C game-core test build/gcplay passes, host pair6ddb965a;
1000 normal/save ended each,0 clears/stuck/crashes/timeouts,50236turns,
matching0414bcf0,14675roundtrips,largest11472bytes. Before D11 checkpoint was
50862turns/5eb7b696 and14824roundtrips: real finite expiry changes later rewards,
so trajectories change naturally. No forced old digest or Classic-clear claim.

Retained corrections: source enemy cleanup triggers two updateModifiers calls,
not the guessed one; fixed after reading the actual source method. Initial
resume replay treated expected load display events as unexpected boundary
output and failed120/120; those load events are now explicitly checked before
comparison. Save proof was strengthened from manually rebuilding a COMMAND
queue after a supplied control to the untouched encounter boundary BEFORE
controls. Its first replay detected loader participant reconstruction (+3
friendship); both supplied nonparticipating control inputs are now explicit,
and final8000 pass. Original source cases/expectations were retained unchanged;
no failure was waived or removed. Source callbacks are counted only after
all fixtures completed, not inferred from H.

Reproduce: wrapper above; `python game-core/tools/lure-lapse-malformed.py EXE TRACE`;
`python game-core/tools/lure-lapse-mutations.py TRACE`; host/H commands above.
Evidence: ignored `game-core/build/lure-lapse-evidence.yZ20aQ/` and
`/workspace/work/remaining-2026-10-11/d12-*`. Broader trainer4242case1051 and
ITEMS text1/1/2/1 stay FAILED, legacy damage31337coverage1999,0 Classic clears
and hardware unrun remain visible. Exact-head host/3DS CI must pass before merge;
conditional source skips are not passes.

|Evidence|SHA256|
|---|---|
|lure-lapse.producer.test.ts|`3134734d0b31809e499743bb85aa687993467975ed9b591d7e84d7e0b6687ce1`|
|caller-2000-31337.txt|`a737b2070a7b7da14ce28124e9129db0c6e9adbb7875e2e73a4bffd9a386c567`|
|caller-2000-4242.txt|`4f5d6896a7fd0d59f26b4df68595255977a2d127e4fa9a3964f79ce7a6cd6c37`|
|caller-2000-777.txt|`1ed403503797256bd2468864c1a9973a4c9b4cb72a40c409b836aa2bffb00d40`|
|caller-2000-9001.txt|`f2b07313338558f64c35269ed59dfef56ae29c2e36b1bca122257b23ac3b8961`|
