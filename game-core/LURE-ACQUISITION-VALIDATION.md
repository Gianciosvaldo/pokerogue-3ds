# D11 canonical Lure acquisition

Base290243097c6324e7950b03c67be694c56f7d6034, clean branch start after #206;
D09 #201 merged; reward #153 canonical API present. Current caller has no Lure
adapter, ctx.n_lures remains0. Source8555c08c823b856cbec4eb99ca84ea52a955836d,
Node22.23.3. No open duplicate PRs at batch start; refresh before publication.

Evidence recorded before production edits:

|Source symbol|Intended C behavior|Fixture|
|---|---|---|
|modifier-type.ts LURE/SUPER_LURE/MAX_LURE factories|durations10/15/30, no holder, arg0|max/partial/last tick per kind|
|DoubleBattleChanceBoosterModifier.match modifier.ts461|same class AND maxBattles, first record|mixed kinds/duplicate order|
|LapsingPersistentModifier.add modifier.ts400–454|partial refresh leaves stack unchanged; full returnsfalse|stack1/2, expiry1, full duration|
|Scene.addModifier battle-scene.ts2395–2429/updateModifiers|full tier default replacement, persistent false result, actual update|mixed held/temp stock, full/capacity|
|SelectModifierPhase.applyChosenModifier/applyModifier select-modifier-phase.ts170–186/271–298|Lure no party filter, free ends; paid false retains money, fallback consumable charges|direct and selected, free/paid|
|pr_reward_add_lure reward-core/src/lure.c32–47|validated read-only shadow plus canonical add/refresh status|opaque old timer refusal, no invented lifetime|
|gc_item.battle_count save transport and gc_temp adapter|preserve named timers/original stock flags/args; independent temp acquisition|legal save/load mixed inventory|

Planned acquisition only; no lapse or encounter changes. Unknown count0 stock,
held/detached/virtual Lures, unrepresentable context retain counted fallback.
Final proof below replaces the initial pending status.

## Final executed source proof

`source /workspace/shared/activate-parity.sh` then
`game-core/tools/lure-acquisition-diff.sh UP 2000 '777 9001 4242 31337'`.
Actual Scene.addModifier, SelectModifierPhase.applyChosenModifier/applyModifier,
and regenerateModifierPoolThresholds execute. The source pool excludes Lures
at remaining>=60% of duration, Classic wave199, or the existing persistent
stack limit. It is distinct from direct Scene full-duration refusal/replacement.
C uses real gc_reward_ctx/unchanged reward thresholds to compare that eligibility;
filter observers use separate RNG snapshots (restored before acquisition), not
an additional acquisition roll. The source queue observer delegates the original
queueMessage; all queued kinds are asserted MessagePhase and complete rendered
texts compare to C message events in order via real pinned locales.

Every seed:2000 completed/covered/identical,0 skipped/thrown/differences;
kinds667/667/666; paths1008direct/992public;1198 stock changes,470 refreshes,
252 direct full tier replacements,550 publicly filtered choices,1328paid.
Mixed classes, duplicates and first-match order, last/partial/full timers,
threshold -1/exact/above, stack1/2, capacity95/96 and party1–6 are deterministic.
All54 Pokemon fields, whole ordered stock/args/timers, wallet/balls/profile,
global and battle RNG and queue/message transport match. UI completion/audio/
modifier-bar rendering are stubs, not console proof; restore sound callback
count is captured, no audio playback claimed. Fused/stat-changing/orphan-held,
virtual/detached/opaque count0 contexts remain outside canonical Scene update.
No lapse, encounter or natural doubles implementation claim.

1246 host checks pass. All canonical timers1..10/15/30 and stacks1/2 save/load
at the existing legal encounter boundary, refresh the first duplicate without
changing other timers/order/stack/args, then save/load again. Source independently
reconstructs real ModifierData for each Lure output. Old count0 saves roundtrip
without invented expiry and canonical addition refuses them atomically. Direct
capacity96 new-record refusal is explicit; source-comparison fixtures use95
when a new record is required,96 when a same-kind match can refresh/refuse.

52 malformed transports exit2 after intact0; actual rebuilt C refresh-stack
mutation exits1 with470 differences and wrong-kind mutation exits1 with1450;
intact exits0. Final delegated-message source traces are byte-identical to the
traces used for negative/mutation proof, all four SHA256s checked.
Unchanged reward lurediff.sh source2000×4 and26 guards pass. Existing game
acquisition/candy/apply/ball/Orb/Wide/Multi and raw Bell checks retained from D10
replay unchanged; temporary source-caller2000×4 also passes113059/111641/111718/113224 assertions respectively (per-seed exact counts in retained logs).
No existing comparator, producer, generated data or other core changed.

Before: paired host8711f1c7,1000 ended normal/save each,50458turns/f98028d9,
14710roundtrips. Final rebuilt H: make -j2 -C game-core test build/gcplay passes,
paired hostdcc5fea5;1000 ended each,0 clears/stuck/crashes/timeouts,50862turns,
matching5eb7b696,14824roundtrips,largest11898 bytes. Canonical timer stock and
full-tier fallback alter reward/capture trajectories; expected new digest is
not forced to the old digest. Expiry and chance are still separate cards.

Retained failed experiments: initial compile referenced nonexistent GC_EV_SCREEN
and was corrected from the actual event enum. Host byte-comparison of loaded
key pointers was replaced by semantic key/field comparison (pointers are not
save identity). Capacity guard detected possible append above the projection
limit and was added before mutation. Initial mode6 mixed-kind capacity fixtures
would create record97; strict replay refused2 on all four seeds. Input fixture
capacity was corrected to95 for new-record paths, full8000 regenerated and
passed, with the unrepresented96->97 refusal separately tested. No failing case
was dropped or converted into a pass. Message observer was strengthened to
original delegation and8000 rerun, with byte-identical transports.

Reproduce strict/mutations with GC_LOCALES=UP/locales/en:
`python game-core/tools/lure-acquisition-malformed.py EXE TRACE` and
`python game-core/tools/lure-acquisition-mutations.py TRACE`.
`make -j2 -C game-core test build/gcplay`; gcplay --auto1000 --jobs8 with/without
--save-test. Node22.23.3/GCC14.2.0. Evidence ignored build directory
`lure-acquisition-evidence.gsATEv`, raw reward traces
`/workspace/work/remaining-2026-10-11/raw-lure`, and d11-* logs in that workspace.
Ordinary exact-head host/3DS CI must pass before merge; conditional source skips
are not passes. Existing broad trainer4242case1051 and ITEMS text1/1/2/1 remain
FAILED; legacy damage31337coverage1999, Classic clears0 and hardware unrun remain.

|Evidence|SHA256|
|---|---|
|lure-acquisition.producer.test.ts|`9a1fc677530250c881fa903fd1b1ced96546665de5820b82d879c390d8d9192c`|
|caller-2000-31337.txt|`4fc429fc38a98a5f4b769911ba506da40b057b41c7d5acf1ae85c48b0f9218b8`|
|caller-2000-4242.txt|`9cf92c5476ff229f6f1fb6592d22c7cd8006ff8ba0257d2f1f1479a4e9380fa4`|
|caller-2000-777.txt|`3a0fa0684a169c8e639dd3946b87a4e18af21e4337b7482858098063bf9cdd25`|
|caller-2000-9001.txt|`cc4dc1d4012c7cb1082f0fed10539090864e860ef447fa81b922cc5e5ea951b0`|
