# D07 Cheek Pouch caller evidence

Before production edits: clean local D06 branchf47757c, PR204 awaitingCI, merged mainf425053; README/taskpack/currentrawBerryproof reviewed. Upstream8555c08c823b856cbec4eb99ca84ea52a955836d Node22.23.3.

|Symbol|Observed contract/fixture|
|---|---|
|HealFromBerryUseAbAttr4721–4750|simulated early return, clamped generated percent1/3, primary/passive abilityName, minimum1 heal phase|
|BerryPhase.eatBerries33–83|opponent Unnerve gate, consume all eligible records, updates and queued stats, callback once after round|
|PokemonHealPhase/BerryModifier|HPcap/HealBlock/consumption/hasEatenBerry/remainingorderedstock, normalSitrus/Lum/Leppa, mixedrecords|
|C ab_attrs1383/phases2977|existing leaf but actual caller uses pr_hook_ab, which records unsupported and never dispatches; existing loopbreak and holder-side Unnerve gate require actual source discrepancy proof|
|C hooks.c pr_hook_ab|source32smoke:26 not covered by HealFromBerryUseAbAttr,6covered pass, no valid complete caller proof existed|

Before-edit H D06host4a43b96f;1000normal/save50401turns paired1a8cced4,14700saves,zero clears/crash/stuck/timeout.60 broad/focused source regressions retained; knowntrainer4242/ITEMStext failures stay failed. No raw suite/old comparator edits.

Final2000 per seed777/9001/4242/31337:8000 fully covered state/queue/history/RNG/rendered text matches,0skipped/thrown/parse/stats errors. Eachseed actual consumed1500 turns,blocked126,no-new374,primary1248/passive126 callback calls;1374 total once-per-round source calls,626notreached;16matrixstates126×8/124×8, both sides1000. Sitrus/Lum/Leppa, multi-record rounds, stage berry ordering, historical eaten flag without usable berry, Unnerve, suppressed and primary/passive/dedup Cheek Pouch, full/near-full/tinyShedinja1/odd HP, HealBlock all represented. Remaining ordered stacks and hasEatenBerry explicitly compared. New source observer delegates actual HealFromBerryUseAbAttr; no replacement formula expectation.

Baseline32 source smoke:26notcovered because old real phase called the unported hook instead of existing leaf. Dispatch-only correction exposed7/32 full state/text failures: first-record-only consumption vs actual multi-record rounds, and Unnerve checked on holder vs opposing active Pokemon. Corrected exactly these paths; source32 then32fullpasses. Common leaf source has no persistent hasEatenBerry gate: direct healthy never-eaten helper queues a heal, so remove that extra gate; actual BerryPhase still requires newly eligible consumption. Explicit simulated early return and generated same1/3 payload replace fixeddivision; no new percentage factory or claimed numerical regression.

Nine separately constructed actual BerryPhase/common ability controls compare unchanged pre-drain HP, consumed stock/eaten flag, heal count and summed queued amounts: simulated, fainted ability holder, fainted BerryPhase holder, full historical eaten/no usable berry, suppression, ordinary Sitrus+Cheek once, opposingUnnerve and passive. Actual whole turns cover healing caps, abilityName text and queue/display/stat-change order. Helpers are not counted as legal whole turns or as8000 cases.

49 strict malformed main transports (including MOD range/identity fields) and9 malformed helper transports exit2 after intact positive controls. Actual rebuilt remove-caller and duplicate-caller mutations each fail1374/2000state/textcases; intact passes2000, source unchanged. Final complete corpus additionally prints nonzero consumed/blocked/no-new/primary/passive counts; previous fully passing source corpus retained. Negative/failed experiments and nonexistent local raw Berry filenames/wrapper invocation retained and replaced with actual pinned original producer commands below.

60 unchanged broad/focused replays retain general trainer4242case1051 optional SwitchSummonPhase mismatch exit1 and ITEMS missing1/1/2/1 text gaps; no unported texts claimed passed.56000 statitem/trainerswitch/Growth/typeboost/field/PsychUp/AverageStats focused cases pass. Final rebuilt D04/D05/D06 drivers replay their unchanged24000 source turns: all full state/text pass. Original raw Berry producer/comparator freshly runs all11identities2000×fourseeds=8000pass; no changes to that suite. Prior legacydamage1999/2000 and older trainer diagnostics remain recorded.

Hhost normal/save paired8711f1c7 passes (baseline4a43b96f);1000normal/save each1000ended0clear/stuck/crash/timeout,50458turns pairedf98028d9,14710saves. BaselineD061a8cced4/50401/14700 legitimately changes after corrected berry caller; finalsame-buildnormal/savepairs identical. No wholeClassicclear/hardware parity claim; CudChew/theft/transfer/natural doubles outside scope.

Reproduce tools/cheek-pouch-diff.sh pinnedupstream2000 fourseeds; CHEEK_POUCH_HELPERS=1 samewrapper20 '777'; TURNTEST_LOCALES=<upstream>/locales/en python tools/cheek-pouch-malformed.py executable maintrace; python tools/cheek-pouch-helper-malformed.py helper-executable helpertrace; same locale env tools/cheek-pouch-mutations.py maintrace. Raw existing suite: cp tests/berry.producer.test.ts <upstream>/test/tests/zz-dump/; for each seed use Node22 BERRY_N=2000 BERRY_SEED=seed BERRY_OUT=trace vitest run that file --maxWorkers=1; TURNTEST_LOCALES=<upstream>/locales/en build/berrytest trace. H make -j2 -C game-core test; gcplay --auto1000 --jobs8 with/without --save-test. No established producer/comparator changed; new anchored build-time Growth overlay observes all temporary stats. Logs d07-* /workspace/work/batch-2026-10-11; final corpus596hAB/helperhl3sR8 and raw Berry traces retained. Node22.23.3,GCC14.2.0; exacthead host/3DS CI pending.

|Trace|SHA256|
|---|---|
|596hAB/cheek-pouch-2000-31337.txt|`85072f69bc2ddecda080480b0659f565a2d52eea838a83a32b8c07e78cffbe53`|
|596hAB/cheek-pouch-2000-4242.txt|`2c66e417ffd425c2eb7981708b5f8721de445ad5e37f110037f92141647771c5`|
|596hAB/cheek-pouch-2000-777.txt|`8706f7e01b6080e66f174130227a6e949d83bfaf8d67c3022b699e81bc3619fb`|
|596hAB/cheek-pouch-2000-9001.txt|`dfcd68a4b8ad9d64d5452632f75a667acd7ff5a010ca9a9384e81660bd11ce9b`|
|hl3sR8/cheek-pouch-20-777.txt|`2f1d4772c5b1c2a2de577819408ec1e33278ad8becca3dafe27d942a229a484f`|
|unchanged raw/berry-777.txt|`045b129cfd8759fdfccd453d1d2567e5b6c30208327d177add629024087b3ff6`|
|unchanged raw/berry-9001.txt|`131cdcb31d0e055712ba319dfed6c1d0ea259d439efecf51b19168e65c2dedff`|
|unchanged raw/berry-4242.txt|`8dc99fe482569dca1ad915b31d21d38c4cb1edc0dd6b1ab0409c2b6ea310a37a`|
|unchanged raw/berry-31337.txt|`872b79e736d7faaa4c6720389d4b31cd1c451ba491a580df94df72f8b1601150`|
