# D09 Soothe Bell acquisition evidence

Completed locally. Base d8f16d8bdcbd14d72d193e62c8b83e633210de53; clean at start;
current main/symbol/PR check found raw #163 helper but no Scene caller.
Upstream8555c08c823b856cbec4eb99ca84ea52a955836d, Node22.23.3.

Evidence before production edits:

|Source/API symbol|Lines / intended behavior|Fixture|
|---|---|---|
|PokemonFriendshipBoosterModifier.matchType/getMaxHeldItemCount|modifier.ts2596–2621: shared class, exact holder matching, cap3|absent/1/2/3/first duplicate|
|modifier-type.ts SOOTHE_BELL|2020: real holder constructor|arg-1/0 shadow and original args|
|PersistentModifier.add/PokemonHeldItemModifier.match|first same-holder; wildcard does not match acquisition|wildcard/duplicate/other holder|
|Scene.addModifier/updateModifiers|2395–2429/2748–2780: full tier fallback, false persistent return, orphan cleanup/recalc|direct/public/free/paid/capacity|
|SelectModifierPhase public filter/apply|271 onward: full refusal before add, paid false errors without charge|real public selection and purchase|
|pr_reward_add_soothe_bell|soothe_bell.c20–34: canonical cap3, unique ID/owner validity, other stock opaque|storage→party_order→ID|

Unchanged #163 soothebelldiff baseline2000×4 seeds passes,25 original guards;
raw gain remains its existing separate proof. Other Orb/Wide/Multi and H baseline
are freshly retained from D08/D03, with combined d8 H pairedb8b28c8b,50406 turns,
1000 ended each,zero clears/stuck/crashes/timeouts,14701 saves;host4a43b96f.
No friendship multiplier, reward-core changes or established test changes.

Final actual source Scene.addModifier and SelectModifierPhase caller2000 per seed777/9001/4242/31337:8000 exact,0 failed/skipped/thrown; everyseed1200 changes,400 direct full replacements,400 public full refusals,660 wildcard,800 duplicate,1000direct/1000public,1332paid;140 capacity95/96 and20 empty inventory, party1counts315/359/340/338 plus all sizes1–6. Constructor arg0/-1 each1000 (fixture groups, one canonical item class), each split500direct/500public. Full before/after modeled Pokemon54 fields, friendship unchanged, ordered stock/args/IDs, profile/wallet/RNG, update flags and localized events compared. Inventory-record holder visits are compared; no friendship-gain/battle-effect implementation claim.

Unchanged #163 raw source2000×4 and25 original guards pass independently.374 new host checks pass, including atomic unsupported contexts, first-match cap3, duplicate preservation, wildcard cleanup only successfulSceneadd, owner mapping, full fallback, free completion/paid refusal with stock mutation and no charge, save/load. Established Orb/Wide/Multi 8000 each pass, no edits. New Orb acquisition source2000×4 retained passes unchanged. Current old Wide/Multi wrappers overwrite caller-$seed paths; Wide replay uses retained pinned historical trace explicitly.

38 malformed owning transports exit2. Real gc_soothe.c rebuilt mutants raised cap and storage-index owner exit1 with800 and1022 source caller differences; intact exit0. No reward-core edits. Source comparison includes opaque positive stock, but stat-changing/fusion/orphan-other-held inventories, virtual/detached/invalid identity/challenge contexts stay guarded. Legacy Soothe fallback stays generic held storage with GC_STUB_REWARD_APPLY0; this is old fallback behavior, not newly proved battle-effect parity. Initial borrowed Wide host assertion expected stub1 and was corrected to the unchanged actual Soothe fallback0. Missing local trace filename invocation was corrected; no source failures omitted.

H final host pass4a43b96f; normal/save1000 ended each,0clear/stuck/crash/timeout,50406turns pairedb8b28c8b,14701 saves, identical baseline. Battle-core readonly, no multiplier D10 edits or broader whole-run/hardware claims.

Reproduce tools/soothe-acquisition-diff.sh pinned-upstream2000 fourseeds; python tools/soothe-acquisition-malformed.py executable caller-trace; python tools/soothe-acquisition-mutations.py caller-trace; make -j2 -C game-core test; gcplay --auto1000 --jobs8 with/without --save-test. Logs /workspace/work/batch-2026-10-11/d09-*; ignored build source traces. Node22.23.3,GCC14.2.0. Exact-head host/3DS CI pending.

|Trace|SHA256|
|---|---|
|caller-2000-31337.txt|`29185828323ac26133382d673fff762952db71eb45d27ed00ca8a25a24761c5f`|
|caller-2000-4242.txt|`64f022d4209cf403f96f1f6d5f151cfec588e01a10585e9ee247a63967d68bd7`|
|caller-2000-777.txt|`02f2d4712b13dc9395d944d7bbfa362edfd9f2c086b313e34330b83ac1e7bf33`|
|caller-2000-9001.txt|`56a4aa9a147940dc117d5eace27cca14f82b10c020896a46ad1594035d561b6f`|
|raw-2000-31337.txt|`3bfca22ae1e406ff81e8bfcbff3e1efb3c6292bbc12806c6e325cc7774f66c73`|
|raw-2000-4242.txt|`b504c9020ccff27ee8f1a74fcb216607686d7edf565089008d7556d7bf8a3094`|
|raw-2000-777.txt|`cfc317858675c0bb3f47f976d0c9b28b6a6f38ad9b178f4edb52d273bdc4bc79`|
|raw-2000-9001.txt|`293a31d36d680b981874ac72a9310fc7ca4d9f81b61e6546d10eebcecfee7507`|
