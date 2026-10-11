# D05 Fell Stinger evidence

Before production edits: clean D04 local branch2115a58 (PR200 awaiting CI), main d8f16d8; reviewed README, parity/task pack and current dispatch. Pin8555c08c823b856cbec4eb99ca84ea52a955836d,Node22.23.3.

|Source|Intended API/cases|
|---|---|
|FaintPhase.doFaint126–154|received[0], on-field defeat source, victory abilities before move; both source as user/target; actual KO and separate callback boundary probes|
|PostVictoryStatStageChangeAttr4245–4274|generated stats/stages, ignored selfTarget; showMessage passed as fresh StatStageChangeAttr selfTarget; null condition at sole factory|
|Fell Stinger11289|ATK+3 after KO, no hardcoded move-ID stage write|
|StatStageChangeAttr/common stat phase|HP/condition/chance gates, queued drain, Simple/Contrary/floor/cap|
|C phases2680,move_effects85|replace only named unported callback through shared stat-stage helper; retain unrelated faint paths|
|Generated A646|condition null bit0x8, stats list/stages/showMessage; future nonnull condition unsupported rather than guessed closure|

Baseline60 unchanged source regressions D04 retain trainer4242 and ITEMS text gaps; H final D04 paired dca55868,50412turns,1000ended0clear/crash/stuck/timeout,14703saves,host4a43b96f. Retained before-edit proof.

Final8000 dedicated actual turns (2000×777/9001/4242/31337) fully covered,0state/queue/history/RNG/text differences,0skipped/thrown/parse/stat errors. Actual source callback applied1355/1350/1344/1362; remainder645/650/656/638 correctly never reached after noKO/miss/protection. Bothsides1000; eight states250 each perseed; allATKstages-6..6 plus explicitcap6/floor-6/Simple/Contrary. Full actual damage/KO/Faint/StatStageChangePhase drain, ordinary replacement/victory.

Separate6 hand-constructed source FaintPhase.start callback probes execute real classes: firstFell/lastTackleboost, firstTackle/lastFellnoboost, nohistory, switched-outsource, noindirectsource, repeatedpositive. Delegated source applyAbAttrs observation confirms PostVictoryAbAttr precedes move callback without adding an unported victory ability. Cactual PH_FAINT matches6 queue counts. These probes do not claim legal multi-attack whole turns. Simultaneous victory-ability implementation stays unchanged.

45 owning malformed turn transports exit2, with required locale and intact positive control. First malformed experiment exposed omitted type/added/terrain/biome/END guards, now explicit. Two actual-runtime mutants: last-historyfails2/6focusedcallbacks; skipqueuedstatphasefails1355/2000state/text; intact8000passes. Initial helper observed only phase tree levels (stat changes are dynamic), then observed delegated unshiftNew; initial off-field renderer mutation replaced with actual switchOutStatus; corrected source outputs and previous failed logs retained. No source case skipped.

60 unchanged broad/focused replays retain known general trainer4242case1051 optional SwitchSummon mismatch exit1; ITEMS missing1/1/2/1 texts remain failed.56000 focused statitem/trainerswitch/Growth/typeboost/field/PsychUp/AverageStats state/text cases pass. Prior failed damage1999/2000and START+TRAINER diagnostics retained, not cleared by this leaf.

Hhost200normal/savepair4a43b96f passed; final1000normal/save each1000ended0clear/stuck/crash/timeout,50401turns pairede60559d1,14700saves. BaselineD04dca55868/50412/14703; newKOcallback affects play, finalsame-buildpairidentical. No Classicclear/hardware claim.

Reproduce tools/fell-stinger-diff.sh pinnedupstream2000 fourseeds; FELL_STINGER_CALLBACKS=1 samewrapper20 '777'; TURNTEST_LOCALES=<upstream>/locales/en python tools/fell-stinger-malformed.py executable maintrace; sameenv tools/fell-stinger-mutations.py maintrace helpertrace; make -j2 -C game-core test; gcplay --auto1000 --jobs8 with/without --save-test. One anchored build-time overlay of unchanged Growth comparator observes all temporary stats. No established producer/comparator modified. Source/helper provenance differs only additive helper-mode observation and imports, main fixtures unchanged. Logs d05-* under /workspace/work/batch-2026-10-11; finalignored main n8tOhc/helperddknl6 traces. Node22.23.3,GCC14.2.0. Exacthead host/3DS CI pending.

|Trace|SHA256|
|---|---|
|n8tOhc/fell-stinger-2000-31337.txt|`da273920a7ffb4ccfc5cc5cdd12c31b3b159e16eca9e0e129cf7b28f3c57e906`|
|n8tOhc/fell-stinger-2000-4242.txt|`25db73d04a9b9fdddb62060ebdce72bf30cac1567e653b179cc01902b8dff3c0`|
|n8tOhc/fell-stinger-2000-777.txt|`5ac2979638b5093b84c4b84c1bc0d88a948c7de506d7770e6574fb1a2ff0cd84`|
|n8tOhc/fell-stinger-2000-9001.txt|`4da1e65a8fe9a496ab69d50e4363522bb62f27d1294a965652abc48d2076cb71`|
|ddknl6/fell-stinger-20-777.txt|`66aa9005da63c4fe7d70321b03e203b5b2577c470c706d8fd1867eecee5457b1`|
