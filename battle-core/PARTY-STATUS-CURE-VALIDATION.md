# D04 Party status cure evidence

Completed locally; base d8f16d8bdcbd14d72d193e62c8b83e633210de53, clean at start,
current main/open PR symbols checked. Generated PartyStatusCureAttr exists but
has no C leaf. Upstream8555c08c823b856cbec4eb99ca84ea52a955836d, Node22.23.3.

Evidence before production edits:

|Source symbol|Lines / intended C behavior|Fixture|
|---|---|---|
|PartyStatusCureAttr.canApply/apply/cureStatus|2613–2672: custom protection check without target HP; whole acting party; user/bench resetStatus(false), active partner ability display excluded|3 factories, both sides, sizes1–6, protection and KO|
|Pokemon.resetStatus/clearStatus|5191–5230: false leaves FAINT; queue ResetStatusPhase, confusionfalse; sleep cure lapses Nightmare|fainted bench, toxic/sleep counters, retained confusion|
|Move factories|10069–10073 HealBell Soundproof/bellChimed/PARTY;10440–10442 Aroma SapSipper/soothingAroma/PARTY;11878–11879 SparklySwirl null/NONE|all identities/hit/miss|
|Generated A229/A346/A778|English message payloads A bell chimed!/A soothing aroma.../NULL; map those exact payloads to their existing locale keys, no move-ID dispatch|rendered full text|
|pr_reset_status_phase(false)|phases.c723–740: Pokemon pointer supports bench, false does not revive; ResetStatusPhase3428 clears status|loaded player/enemy bench|

Baseline before edits:60 D03 broad/focused unchanged replays retained (known
trainer4242/ITEMS text failures unchanged); H freshly rerun after both D03/D08:
host pass,1000 normal/save ended each,clear0/stuck0/crash0/timeout0,50406 turns,
pairedb8b28c8b,14701 saves,host200-run4a43b96f. No existing tests change.

Final dedicated 2000 cases per seed777/9001/4242/31337:8000 covered,8000 state/queue/RNG and rendered texts identical,0 skipped/thrown/parse/stats failures. Actual source PartyStatusCureAttr.apply delegated, all3 real factories on both sides. Applied counts444/437/404,438/439/400,443/440/404,437/440/401; other selected turns correctly never reached the attr (sleep/paralysis/Taunt/miss/protection). Each seed has all sizes1–6, six status/miss/protection/KO/immunity states, normal statuses and FAINT bench, retained bench confusion, Sleep/Nightmare cure, toxic counters, Sparkly KO target. No synthetic damage helper or hypothetical doubles parity.

Unchanged upstream Heal Bell/Aromatherapy/Sparkly Swirl tests:8 pass, including actual double-field Soundproof refusal vs user/bench cure. New C boundary executes actual dispatcher for all3 factories:9 atomic unsupported refusals (double battle, other active partner, empty party). Singles corpus cures user/bench regardless of immunity. Full partner display remains explicitly unsupported, never silently partial.

45 malformed owning transports exit2. Actual rebuilt C mutants user-only and wrong-bench Soundproof gate exit1; intact exit0;1098 and208 state failures respectively (see retained logs for exact totals). Existing source producers/comparators untouched. Replay60 unchanged broad/focused files: only retained general trainer4242 case1051 optional SwitchSummonPhase mismatch exit1; ITEMS missing1/1/2/1 texts remain failed, not counted text passes. Stat-item/trainerswitch/Growth/typeboost/field/PsychUp/AverageStats 56000 focused cases all pass.

H final host pass4a43b96f;1000 normal/save ended each,clear0/stuck0/crash0/timeout0,50412 turns each,paired dca55868,14703 saves. Baseline50406/b8b28c8b changed because newly ported party cures affect play; same final build pair identical. No Classic clear/hardware claim.

Reproduce using tools/party-status-cure-diff.sh pinned-upstream2000 fourseeds; tools/party-status-cure-malformed.py executable trace; TURNTEST_LOCALES=<upstream>/locales/en python tools/party-status-cure-mutations.py trace; make -C battle-core party-status-cure-boundary; make -j2 -C game-core test; gcplay --auto1000 --jobs8 with/without --save-test. Earlier generator await syntax and KO after-command transport failures were corrected in owning new files, not skipped. Build-time single anchored overlay of unchanged Growth comparator observes zero temporary overrides. Logs under /workspace/work/batch-2026-10-11/d04-*, traces ignored build directory. Node22.23.3,GCC14.2.0. Exact-head host/3DS CI pending.

|Trace|SHA256|
|---|---|
|party-status-cure-2000-31337.txt|`8aacc7a290c89873c24cc10498aa7bf1b2272485f50f67d2aa8af3322686d09c`|
|party-status-cure-2000-4242.txt|`b2feddddac9b8e3f49208ccffbc69194e6d3222531b9f8ed717405836d1dd100`|
|party-status-cure-2000-777.txt|`8969c1e2c723b8879e565037889449369f867b2dace180d6087cb8e6b52867f9`|
|party-status-cure-2000-9001.txt|`66e18693d58d408baf656c3a47bd4f03a66be939fa1b1f38899b8a59595c38a8`|
