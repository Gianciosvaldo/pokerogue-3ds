# D06 Bad Dreams evidence

Before edits: current merged main c22eb74; D05 local caller already verified pending publication; generated class has no leaf, TurnEnd already calls PostTurnAbAttr. README/task pack/dispatch reviewed. Node22.23.3 pin8555c08c823b856cbec4eb99ca84ea52a955836d.

|Source symbol|Contract/fixture|
|---|---|
|PostTurnHurtIfSleepingAbAttr4416–4453|canApply sleeping/Comatose active opponent without indirect blocker; simulated early return; apply blocker to HOLDER, not victim; min1 floormaxHP/8; localized message|
|getOpponents3357/getOpponentsGenerator3366|active ordinary singles, switched-out excluded; source generator speed order|
|applyAbAttrs/common gates|primary/passive/suppression/condition/display before leaf; retain simulation behavior|
|TurnEndPhase/C3152|actual post-turn caller, queue drain, indirect DamageAnim/FaintPhase controls|
|damageAndUpdate/pr_damage_and_update|indirect bookkeeping, no direct damage roll; existing blocker/queue APIs available|
|BadDreams generatedA1182|showAbilitytrue, no new closures or fake percentage factories|

Fixture planned2000 per fourseeds, balanced sides, sleep/awake/Comatose, victim/holder MagicGuard, suppression, HP1/tinyShedinjaKO, waking, passive. Direct boundaries separate from legal whole turns. Baseline unchanged broad/focused D05 and H retained; no natural doubles/fusion claim.

Final complete corpus:2000 per777/9001/4242/31337,8000 fully covered state/history/queue/RNG/rendered text identical;0skipped/thrown/parse/stat errors. Eachseed1332 actual delegated source apply calls,668 not reached by canApply/suppression/waking/awake controls; twelve matrix states168/168/168/168/166×8, both sides1000. Actual source holderMagicGuard cancels166 calls; victimMagicGuard gate refuses; suppressed victimMagicGuard allows damage. Comatose, waking before endturn, primary/passive and odd maxHP all represented. Tiny Shedinja minimum1 uses intrinsic WonderGuard while summon ability is BallFetch; global testing ability override cleared after start so source getAbility(ignoreOverride) reads real baseability. Initial8000 attempt had Shedinja hp55 through global BallFetch override, so it did not establish tinyHP; final8000 rerun corrects it. No source behavior patched.

Nine separately labeled actual common ability callback probes match HP/message counts, including simulated, victimout, holderout, victim/holderMagicGuard, tiny1,Comatose andHP1KO. Hand-constructed holderout invocation still damages at this pin; real TurnEnd excludes inactive holders. No silent extra holder on-field gate added to leaf. Actual whole-turn state compares indirect bookkeeping, Faint/replacement, ability displays/order and texts; no direct damage roll invented.

45 malformed main transports with required locale/intact control and9 malformed helper transports all exit2. Actual rebuilt leaf mutants wrong-blocker-holder166 state/text differences, rounding-up734 state differences; intact2000pass. Source expectation untouched. Wrapper initially tried to execute helper while assigning EXE (own new script), corrected and rerun; failed logs retained.

60 unchanged broad/focused replays: retained general trainer4242case1051 optional SwitchSummonPhase mismatch remains exit1; ITEMS missing1/1/2/1 rendered texts remain failed.56000 statitem/trainerswitch/Growth/typeboost/field/PsychUp/AverageStats focused passes; prior older trainer/legacydamage failures remain recorded. Hhost4a43b96f passed;1000normal/save1000endedeach0clear/stuck/crash/timeout,50401turns paired1a8cced4,14700saves. BaselineD05e60559d1 same50401turns; new ability bookkeeping legitimately changes digest, finalsame-buildpairidentical. No natural doubles/fusion/wholeClassic/hardware claim.

Reproduce tools/bad-dreams-diff.sh pinnedupstream2000 fourseeds; BAD_DREAMS_HELPERS=1 samewrapper20 '777'; TURNTEST_LOCALES=<upstream>/locales/en python tools/bad-dreams-malformed.py executable maintrace; python tools/bad-dreams-helper-malformed.py helper-executable helpertrace; same locale env tools/bad-dreams-mutations.py maintrace; make -j2 -C game-core test; gcplay --auto1000 --jobs8 with/without --save-test. Existing producer/comparator files untouched; new single anchored build-time Growth extension observes all temporary stats. Logs d06-* /workspace/work/batch-2026-10-11; final sourceJSBGhG, helperoQYGs2. Node22.23.3,GCC14.2.0; exacthead host/3DS CI pending.

|Trace|SHA256|
|---|---|
|JSBGhG/bad-dreams-2000-31337.txt|`e843028fda97f888801dd14294891d17383a799981f9cae32f5400747e1f6918`|
|JSBGhG/bad-dreams-2000-4242.txt|`409a85dfcb3f042e980a15ef5b33c8fb4d0c5eadb42115d373325643fb772cca`|
|JSBGhG/bad-dreams-2000-777.txt|`3f1fecfd484c09c519d2dfe22a73f431a3b31dbd424e3703a41187bc3a35ae06`|
|JSBGhG/bad-dreams-2000-9001.txt|`346d9d1e0c54edb696e8e63d5806b5237549172ac1e475efcd3eb4b7471b4dab`|
|oQYGs2/bad-dreams-20-777.txt|`d9a2eda6418721e05ae37c885f779cfa2d4e9157ddde4ccb1c602b50c1aa0f78`|
