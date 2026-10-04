# Remaining parity tasks (supplement to HANDOFF)

Audit baseline: port main `2a740ab66bce22dd94aa34c6cbf2c8e665ad6a79`; upstream target `pagefaultgames/pokerogue` at `8555c08`. Public beta inspected separately at `4f0969875845fb412c5b0af8ed4bc006484a8f2a`; it is a moving target and is not the port target. This document is a source-backed planning inventory, **not an exhaustive per-move audit, a measured completion percentage, or evidence that the checks below ran**. See [PARITY-LEDGER.md](PARITY-LEDGER.md) for the active subsystem parity matrix, public beta drift analysis, landed PR index, and untested class inventory.

Read [HANDOFF.md](HANDOFF.md), the root README and the owning folder README before each task. HANDOFF remains unchanged and controls every task: one task/branch/PR, never push main or merge; named-folder scope; generated data only via redump; no copyrighted assets, upstream source/dumps or binaries; preserve existing producers/comparators, tolerances, seeds, case counts and coverage. A test defect requires stopping the affected task and reporting it. New harnesses and modes below are additive, separately reviewed work, never permission to loosen or rewrite established checks. Exact commands and printed counts belong in each PR.

Existing Tasks 50–56 retain their assigned numbers; Task 50 fixed Eternatus/Paradox generation is merged in PR #92. Do not duplicate their implementations. Supplementary cards start at 57 to avoid collisions; inspect current integration and recorded validation before starting. Tasks 57 and 58 are merged on `main` (PR #97 / commit `58030d6` and commit `836d10c`).

Implementation PRs: [Task 57 #97](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/97) and Task 58 (commit `836d10c`) are merged on `main`. No next integration task starts on an unmerged prerequisite.

## Scheduling

| Lane | Folder | Sequential tasks | Assignment |
|---|---|---|---|
| A | battle-core/ | 67 → 69 → 70 → 71 → 72 → 73 | Hard coordinator work except bounded 71 |
| B | reward-core/ | 89; existing reward API tasks first | Hard; reward state validation |
| C | game-core/ | 87 → 57 → 59 → 60 → 61 (first slice) → 92 → 93 → 94 → 77 → 95 → 96 → 64 → 65 → 66 → 78 → 79 → 61 (further slices) → 62 → 75 → 76 → 80 → 81 → 82 → 83 → 84 | One active task; lower-compute cards 60/64/76/77/78/84/96 |
| D | run-core/ | Existing 50 → 63; 100 | Lorenzo's run-core owner only; HANDOFF ownership unchanged; producer edits allowed for card 100 only |
| E | game/ | 110 (authorized); 85 console half | Card 110 is a project thread, Azahar first; console acceptance stays a human gate |
| F | ai-core/ | 88 → 58 → 68 → 74 | Lower-compute 58/68, hard 74 |
| G | docs/ | 86 | Lower-compute independent audit documentation |

A prerequisite API is a real gate: verify it exists and has recorded checks before beginning. If absent, report the missing bounded prerequisite instead of changing its owner's folder. Different folders may proceed in parallel; each lane starts its next task only after the previous PR merges. Lower-compute describes bounded scope, not lower correctness requirements.

## Validation command contracts

These aliases specify exact baseline commands; substitute the absolute upstream checkout for `<upstream>`, kept outside this repo. Capture baseline and final output.

- **H:** `make -C game-core test`; `game-core/build/gcplay --auto 1000 --jobs 8`; `game-core/build/gcplay --auto 1000 --jobs 8 --save-test`. Paste save checks, completion/hang/crash counts, stub counts and both digests. Equality with/without save is required; a digest change versus baseline needs an explanation.
- **A:** `make -C ai-core`; `ai-core/tools/difftest.sh <upstream> 5000 <seed>` for each seed `777 9001 4242 31337`. Paste cases, covered, identical, mismatches and unsupported reasons before/after.
- **R:** `make -C run-core`; for each seed `777 9001 4242 31337`, run `run-core/tools/difftest.sh <upstream> 20000 <seed> <suite>` for each of `wave shop biome encounter newbattle exp partyexp upkeep trainerspecies luck daily`; use `10000` for `battlegen`, `8000` for `pokemongen`, `3000` for `trainerparty rival gym_e4 wildenemy enemyitems`, and `2000` for `enemybuffs`. Also retain `DIFF_FOCUS=1` seed-31337 runs where the owning README records a focused baseline. If a later merged task adds a stronger suite or larger baseline, retain it too. A blanket “R passed” is insufficient.
- **B:** `make -C battle-core`. In `<upstream>`, run `TURN_OUT=/tmp/parity-plain.txt TURN_N=2000 TURN_SEED=777 npx vitest run test/tests/zz-dump/turntest.producer.test.ts`; then in this repo run `battle-core/build/turntest /tmp/parity-plain.txt`. Repeat the same producer command with `TURN_RICH=1`, `TURN_SWITCH=1` and `TURN_SUMMON=1`, separate `/tmp/parity-rich.txt`, `/tmp/parity-switch.txt`, `/tmp/parity-summon.txt` files, and replay each. Existing item/weather/condition modes relevant to a change also run at their documented seeds/counts; these four baseline runs do not replace stronger existing checks. Paste each complete command and coverage summary.
- A **new approved suite** must name its producer path, environment flags, trace path, C executable and complete commands in its own PR before its numbers are evidence. The task's minimum seeds/counts never override a stricter existing baseline. No fictional command name is provided for a harness that does not exist yet.

Missing upstream dependencies/network are a validation gap. Host fixtures, build success and code inspection do not substitute for unrun upstream differential checks. In particular, existing mystery traces are not validated money/party/modifier parity unless the comparator actually checks those outputs.

## Coordinator status after save and battle repairs (2026-10-03)

Main checked: `ee476524016b0dc0ce6edc93db5baf4f76521325`. Actual code on main controls prerequisites; a side-branch merge does not unlock integration.

| Task | State | Gate |
| --- | --- | --- |
| 87 | #101 merged | 36,471 resume checks and 1000 normal + 1000 saved strict terminal completions; four production fixes including seed 849. Host consistency, not upstream run parity |
| 90 | #106 merged | Plain 1940, rich 1826, switch 1885, summon 1856 covered/identical; zero failures, no coverage loss; CI #102 host/3DS pass |
| 67 | #108 merged directly into main | 8000/8000 dormant/manual helper comparisons; original natural ability gate remains deferred. #108 CI #105 host/3DS pass; bounded helpers on main `cae69338` |
| 88 | #102 merged diagnosis | Node 22: 19762 identical/covered, 0 mismatch, 238 uncovered. Node 24 contract remains unresolved |
| 89 | #104 validated repair | Evolution queue8000/8000; existing application12000/12000 and Memory4000/4000; full fusion lifecycle remains unvalidated, updated-head CI required |
| 91 | #107 validated repair | Dual inputs8000/8000 plus24,000 TM/RNG comparisons; legacy Memory4000/4000 after import repair; unchanged135 application failures addressed in #104, updated-head CI required |
| 59 | Audit blocked on 91 | Existing fixed reward pool remains; no integration or zero-placeholder claim |

Task 68 is delegated to Jules: #108 landed its bounded loaded-state contract directly on main. Lane F is reserved; natural-ability exclusions remain. Task 69's weather-form work is separately validated for review; Task 70 waits for its merge. No new three-task Jules wave is available. Task 60 needs 59 and Task 71 needs 67/69/70. Established checks are untouched; unavailable natural Illusion/Memory and failed fusion comparisons do not count as passing. #108 landed the explicitly revised helper scope in the Task 67/68 cards below. Exact commands and results are in the linked PRs and owning READMEs.

## Task cards

### Task 57: Held-item mapping and consumed-stack persistence (game-core/, lane C)

**Branch:** `outside/held-item-bridge`. **Assignment:** Hard; coordinator. **Prerequisites:** None.

**Problem.** The run item list and battle modifier list are separate representations; gc_items.c currently maps only five classes. A consumed battle item must update the run before the next encounter and before saving.

**Do.** Restrict this slice to LEFTOVERS, SHELL_BELL, FOCUS_BAND, QUICK_CLAW, KINGS_ROCK, WHITE_HERB and BERRY. Map their classes/arguments/stacks into battle state and reconcile consumed stacks on both player and enemy sides. Preserve save/rebuild ownership. Add an additive constructor/item-loss producer using actual upstream. Do not silently map an unknown item to a known class.

**Pass.** Host checks H; focused scenarios consume Sitrus, Lum, Leppa and White Herb, leave an unused item intact, switch holders, and save/reload at the next legal save point. Compare owner/class/argument/stacks before and after; ordinary and saved run digests equal.

### Task 58: Stuff Cheeks requires a real berry (ai-core/, lane F)

**Branch:** `outside/ai-berry-restriction`. **Assignment:** Lower compute. **Prerequisites:** None.

**Problem.** Stuff Cheeks must distinguish BerryModifier from unrelated held items; generic held-item presence is insufficient.

**Do.** Use the existing battle modifier class information to check berry possession. Check positive berries, non-berries, zero stacks and no items against pinned move conditions. Keep battle-core read-only.

**Pass.** Host build make -C ai-core; focused condition regression checks each of those four states. Existing AI differential command A remains at zero mismatches and coverage does not decrease.

### Task 59: Replace the placeholder reward option pool (game-core/, lane C)

**Branch:** `outside/reward-options-integration`. **Assignment:** Hard. **Prerequisites:** 57 and 87 merged; 91 merged after its established Memory check reruns; reward-core option API available.

**Problem.** gc_port_reward_options in src/gc_stubs.c still chooses a small fixed pool instead of reward-core options.

**Do.** Construct pr_reward_ctx from actual party, items, unlocks, balls, learnsets and arena. Call reward-core options for first roll, reroll and copied screen. Preserve generator arguments, tiers, luck upgrades and RNG state.

**Pass.** H; focused first-roll/reroll/copy fixtures compare the adapter input and output to reward-core, and the placeholder reward-option counter becomes zero. Do not call this upstream end-to-end parity.

### Task 60: Use reward application for healing and PP (game-core/, lane C)

**Branch:** `outside/reward-consumables-integration`. **Assignment:** Lower compute. **Prerequisites:** 59 merged; reward-core apply API for these classes available.

**Problem.** Run reward application still contains its own potion, revive and PP behavior.

**Do.** Delegate potions, Full Restore, revives and PP restorers to the ported application API, preserving target validation, messages and actual consumption. Leave unsupported classes explicit.

**Pass.** H; table-driven cases cover full HP, fainted targets, statuses, empty PP, PP Ups, invalid targets and no-effect rejection. Record application counts by item.

### Task 61: Reward lifecycle for evolution and form items (game-core/, lane C)

**Branch:** `outside/reward-evolution-lifecycle`. **Assignment:** Hard. **Prerequisites:** 60 merged; prerequisite reward and battle APIs merged.

**Problem.** Item application, evolution conditions, paused evolution and form transitions must use one consistent party state.

**Do.** Wire the existing reward APIs to evolution/form-choice screens and cancellation. Update stats, moves, dex and persisted modifier ownership in upstream phase order. Limit the first PR to one documented item-evolution family and one form-item family.

**Pass.** H; compare accepted/rejected/cancelled choices, stats, moves, dex and item stacks in focused pinned fixtures. Further families require subsequent bounded tasks; no blanket form parity claim.

### Task 62: Fusion reward lifecycle (game-core/, lane C)

**Branch:** `outside/reward-fusion-lifecycle`. **Assignment:** Hard. **Prerequisites:** 61 merged; reward-core fusion API available.

**Problem.** Offering DNA Splicers is distinct from applying and persisting a fusion correctly.

**Do.** Wire fusion selection/cancellation and unfusion to existing APIs. Preserve donor ownership, ability choice, move learning, stats and dex updates. Do not edit the other cores.

**Pass.** H; fixtures cover valid fusion, invalid donor, cancellation, full moveset and save/resume, comparing both source party members and resulting fusion attributes.

### Task 63: Enemy movesets generated by upstream rules (run-core/, lane D)

**Branch:** `outside/enemy-movesets`. **Assignment:** Hard; run-core owner only. **Prerequisites:** Task50 merged; lane owner schedules.

**Problem.** Enemy species and pre/post-moveset rolls do not establish moveset parity; game-core chooses the last four level moves.

**Do.** Port the pinned enemy moveset selection in a bounded species/form fixture set first, then expand. Preserve rejection rules, order, PP and RNG draws. Add a separate approved producer mode rather than modify an existing producer.

**Pass.** New approved differential suite: 2,000 cases on each of 777, 9001, 4242, 31337, zero mismatches for move IDs/order/PP/RNG; each supported path exercised. Existing run suites R retain coverage.

### Task 64: Wire enemy movesets into the run (game-core/, lane C)

**Branch:** `outside/enemy-movesets-integration`. **Assignment:** Lower compute. **Prerequisites:** 63 merged. 62 (the player's fusion reward lifecycle) is no longer required; lorenzo approved this reorder on 2026-10-04. If an enemy fusion case needs player-side fusion state, stop and report it.

**Problem.** gc_port_enemy_moveset remains a last-four-level-moves fallback even after generation is ported.

**Do.** Call run-core moveset generation and retain the generated RNG sequence and species/form inputs. Delete only the superseded fallback and its counter.

**Pass.** H; enemy-moveset fallback count becomes zero for the supported encounters; adapter fixtures match run-core output exactly, including RNG state.

### Task 65: Fixed rival and gym teams in the run (game-core/, lane C)

**Branch:** `outside/fixed-trainer-integration`. **Assignment:** Hard. **Prerequisites:** 64 merged; Tasks48 and 49 merged with recorded final-head validation.

**Problem.** Ported fixed-team generation is separate from running its trainer flags, rewards, AI and Tera behavior in game-core.

**Do.** Connect supported fixed rival/gym/Elite Four teams to the run. Preserve trainer configs, boss flags, party ordering, custom rewards and title messages. Leave any unsupported fixed battle explicit.

**Pass.** H; focused wave fixtures cover rival, gym and Elite Four send-outs, faint replacement and reward selection. Fixed-battle fallback count becomes zero only for the supported classes.

### Task 66: Final boss transition and shield lifecycle (game-core/, lane C)

**Branch:** `outside/final-boss-lifecycle`. **Assignment:** Hard. **Prerequisites:** 65 and Task50 merged; battle shield/transition API available.

**Problem.** Task50 covers fixed generation; scripted transformations and subsequent battle phases are a separate integration gap.

**Do.** Integrate the pinned Classic final-boss phase transition, shield state, forced commands and termination using existing core APIs. Do not reimplement Task50 generation.

**Pass.** H; focused first-form/transition/final-form victory fixtures compare phase sequence, shield counters, stats and final clear state to pinned source; report unavailable differential validation.

### Task 67: Dormant Illusion identity/type representation (battle-core/, lane A)

**Branch:** `outside/illusion-state`. **Assignment:** Hard. **Prerequisites:** Task 90 baseline repair merged; prerequisite form APIs available. PR #103 proposes this bounded scope for review.

**Problem.** Pinned Illusion is disabled: natural selection/break/switch activation cannot provide a nonzero differential. Dormant helpers can still store and read loaded/manual apparent identities; C lacks that representation.

**Do.** Model the identity subset used by actual pinned set/break/reset helpers, separate real stats/ability from apparent species/form/fusion/gender and type reads, and snapshot ordinary species/form messages. Add a separate helper producer; do not register commented-out abilities or change existing checks. Keep nickname/fused display names, fusion gender, visual assets and natural dispatch explicitly deferred.

**Pass.** New helper mode: 2,000 cases per seeds 777, 9001, 4242 and 31337, zero mismatches, nonzero manual Pokemon/species setters, breaks and reset cleanup; all 32 type-flag combinations; source snapshot independence, unchanged real state and RNG; planted mutation and malformed trace rejected. B unchanged with coverage retained after Task 90; host/3DS CI pass. The original natural disguise/break/switch gate remains deferred and unrun; this card does not claim it passed.

### Task 68: Loaded Illusion AI integration (ai-core/, lane F)

**Branch:** `outside/ai-illusion`. **Assignment:** Lower compute. **Prerequisites:** 58, 90 and the reviewed bounded 67 scope merged.

**Problem.** AI cannot read a manually loaded apparent identity in the pinned helper contract. Natural Illusion remains disabled upstream.

**Do.** Use the exported disguise-aware reads where the pinned AI reads apparent identity/types. Add a separate producer with explicit manual/loaded snapshots invoking actual upstream helpers and AI scores. Preserve all established producers, comparators and natural-ability coverage exclusions. Keep automatic disguise dispatch and presentation outside this task.

**Pass.** A: zero mismatches and coverage retained. Additive loaded-state AI comparisons: 2,000 per each of the four seeds, zero mismatches, at least one disguise changes a matchup score, and a planted identity mutation fails. Report before/after covered and Illusion-excluded counts unchanged for the established suite; natural activation is unavailable, not passing evidence.

### Task 69: Weather-driven form changes (battle-core/, lane A)

**Branch:** `outside/weather-form-triggers`. **Assignment:** Hard. **Prerequisites:** 67 merged.

**Problem.** Forecast, Flower Gift and Ice Face require form triggers rather than weather damage alone.

**Do.** Implement their existing upstream trigger dispatch, including ability suppression, passive abilities, summon and weather replacement. Keep standard form ability selection distinct from fusion selection.

**Pass.** Approved focused differential mode: 2,000 cases per four seeds, zero mismatches, each named ability triggers; B and weather regressions retain coverage.

### Task 70: Held-item theft and removal (battle-core/, lane A)

**Branch:** `outside/item-theft-removal`. **Assignment:** Hard. **Prerequisites:** 69 merged.

**Problem.** Thief/Knock Off and related modifier removal must update both holders and stack ownership.

**Do.** Port one bounded move-attribute family first, including Sticky Hold, success/failure, stack limits and messages. Export ownership changes for the run adapter; do not edit game-core.

**Pass.** Approved focused mode: 2,000 cases per four seeds, zero mismatches for source/target stacks, messages and RNG; B unchanged. Other theft families stay explicitly outstanding.

### Task 71: Enigma, Lansat and Starf berry effects (battle-core/, lane A)

**Branch:** `outside/special-berries`. **Assignment:** Lower compute. **Prerequisites:** 70 merged.

**Problem.** These berries need effect triggers and consumption, beyond the first HP/status/PP slice.

**Do.** Check pinned BerryModifier classes and port only missing named effects with modifiers, stat caps, random stat choices and consumption order. Already-ported pinch berries receive regression coverage, not duplicate implementations.

**Pass.** Approved focused mode: 2,000 cases per four seeds, zero mismatches, each named berry consumed at least once and rejected at least once; B unchanged.

### Task 72: Dancer and Tera dispatch order (battle-core/, lane A)

**Branch:** `outside/dancer-tera-dispatch`. **Assignment:** Hard. **Prerequisites:** 71 merged.

**Problem.** Dispatch order, copied moves and Tera precommands need focused coverage; broad singles results do not prove every branch.

**Do.** Audit pinned Dancer copied-move queueing and Tera preturn dispatch; fix one identified phase-order gap per PR. Stop if the existing producer cannot expose the expected state.

**Pass.** Approved focused fixtures compare queue/order/targets/RNG/messages; randomized 2,000 cases per four seeds, zero mismatches. B unchanged; report unreachable branches.

### Task 73: Doubles turn engine, first slice (battle-core/, lane A)

**Branch:** `outside/doubles-turn-core`. **Assignment:** Hard. **Prerequisites:** 72 merged.

**Problem.** Doubles paths and spread damage remain outside established singles coverage.

**Do.** Implement two active battlers per side, target selection, spread damage and one-turn command order. Exclude full run/UI integration from this slice. Add a separately reviewed doubles producer.

**Pass.** New doubles suite: 2,000 cases per four seeds, zero mismatches, ally and both-opponent targeting and simultaneous faint states exercised; B unchanged.

### Task 74: Doubles AI command selection (ai-core/, lane F)

**Branch:** `outside/ai-doubles`. **Assignment:** Hard. **Prerequisites:** 68 and 73 merged.

**Problem.** Singles AI checks do not validate ally scores, target cutoffs or Commander cases.

**Do.** Exercise existing doubles AI branches against actual upstream commands and score vectors; fix C code only. Keep battle-core read-only.

**Pass.** New approved doubles AI suite: 2,000 cases per four seeds, zero mismatches with ally/dual-target cases nonzero; A unchanged.

### Task 75: Doubles run lifecycle (game-core/, lane C)

**Branch:** `outside/doubles-run`. **Assignment:** Hard. **Prerequisites:** 66, 73 and 74 merged.

**Problem.** The headless run reduces a doubles encounter to singles.

**Do.** Add two command selections, active-party mapping, ally faint replacement and victory continuation through existing battle/AI APIs. Keep presentation outside scope.

**Pass.** H; targeted doubles runs and save/resume preserve both active slots and command order; singles fallback count becomes zero for supported doubles encounters.

### Task 76: Biome weather integration (game-core/, lane C)

**Branch:** `outside/biome-weather`. **Assignment:** Lower compute. **Prerequisites:** 75 merged; run weather-generation API available.

**Problem.** A weather-capable battle engine does not establish the new-biome weather roll or its persistence.

**Do.** Call the pinned random-weather generator at the correct biome transition, retaining RNG order, summon effects and reload state.

**Pass.** H; focused biome/time/weather fixtures compare generator output and RNG; existing-weather persistence and new-biome replacement pass. Record before/after weather fallback count.

### Task 77: Flee through the run loop (game-core/, lane C)

**Branch:** `outside/flee-run`. **Assignment:** Lower compute. **Prerequisites:** 94 merged; battle flee-roll API available (`attempt_run_phase`). Moved ahead of 75/76 by the 2026-10-04 reorder; it needs neither doubles nor weather.

**Problem.** Battle escape eligibility/roll must reach game-core commands and wave progression.

**Do.** Wire allowed, blocked, failed and successful attempts, the attempt counter, message events, rewards suppression and subsequent encounter transition.

**Pass.** H; fixtures cover trainer prohibition, trapped wild, failed attempts followed by success, and save/resume at the next legal point. Escape RNG matches battle-core fixtures.

### Task 78: Starter unlocks and cost enforcement (game-core/, lane C)

**Branch:** `outside/starter-unlock-gates`. **Assignment:** Lower compute. **Prerequisites:** 77 merged; system-data API available.

**Problem.** Default starter data alone does not enforce unlocked abilities/forms/passives, reductions or challenge admission.

**Do.** Validate supplied starter selections against persisted system data, budget and existing challenge APIs. Reject unavailable variants without mutating RNG or system state.

**Pass.** H; fixtures cover locked species/ability/form/passive, legal reduction, exact budget, over-budget and challenge restrictions; invalid choices leave state unchanged.

### Task 79: Classic win progression (game-core/, lane C)

**Branch:** `outside/classic-win-progression`. **Assignment:** Hard. **Prerequisites:** 78 merged.

**Problem.** A clear screen alone is not classicWinCount, ribbons, unlocks, vouchers and terminal save parity.

**Do.** Port the bounded pinned Classic victory bookkeeping into the system state; make repeat processing idempotent and distinguish ordinary defeat.

**Pass.** H; fixtures compare first clear, repeated clear, fused team members and defeat to source-derived expected system updates; persisted reload does not award twice.

### Task 80: Egg generation and voucher gacha (game-core/, lane C)

**Branch:** `outside/egg-generation`. **Assignment:** Hard. **Prerequisites:** 79 merged.

**Problem.** Egg generation, machine type, voucher costs and pity/session state are distinct from reward voucher offers.

**Do.** Port the pure egg-data/gacha state slice from src/data/egg.ts and src/system/egg-data.ts. Add an approved producer calling real upstream functions; exclude rendering and timed live-service events.

**Pass.** New approved suite: 2,000 cases per four seeds, zero mismatches for egg fields, balances, pity and RNG; all pinned machine types exercised; H unchanged.

### Task 81: Egg lapse and hatching progression (game-core/, lane C)

**Branch:** `outside/egg-hatch-progression`. **Assignment:** Hard. **Prerequisites:** 80 merged.

**Problem.** Hatching must update dex/starter attributes, IVs and egg moves as well as create a species.

**Do.** Port lapse counters and the headless hatch result/bookkeeping from egg-lapse/egg-hatch phases, with exact queue ordering and interrupted-save recovery.

**Pass.** Approved suite: 2,000 cases per four seeds, zero mismatches for hatch result/system delta/RNG; H unchanged; fixtures cover multiple eggs and interrupted processing.

### Task 82: Mystery encounter comparison harness (game-core/, lane C)

**Branch:** `outside/mystery-validation-harness`. **Assignment:** Hard. **Prerequisites:** 81 merged; existing check defect documented in a separate review.

**Problem.** game-core/tests/mystery_diff_test.c reads TYPE and OPTION but ignores RESULT_MONEY; gen/dump-mystery-actions.test.ts also catches UI errors. Its reported “differential” result does not establish action/money parity. A generated trace is not a passing differential test.

**Do.** First document every produced record the comparator ignores and the swallowed UI exceptions. Stop the affected implementation task and report this existing check defect. In this separate reviewed task, add a new producer/comparator without modifying the established producer/check; compare money, choices, party/modifier changes and RNG for a single encounter family. Keep all edits within game-core; preserve the old producer and check unchanged.

**Pass.** Demonstrate a planted money mutation makes the new suite fail, then unmodified C replay has zero mismatches over 2,000 cases per four seeds. Record all compared fields; do not credit old mystery traces as parity evidence.

### Task 83: Daily, Endless and Challenge run entry (game-core/, lane C)

**Branch:** `outside/run-mode-entry`. **Assignment:** Hard. **Prerequisites:** 81 merged; run-core mode APIs available.

**Problem.** Ported mode helpers and admission rules are separate from complete playable-mode integration.

**Do.** Start with plain Daily initial starters, starting money and biome, then document separate follow-up cards for Endless and each Challenge. Do not claim custom event seeds or server dailies supported.

**Pass.** H; plain Daily entry fixtures match existing run-core outputs and mode limits. Report Endless/custom Daily/each Challenge status separately; no blanket mode parity claim.

### Task 84: Settings, history and save compatibility (game-core/, lane C)

**Branch:** `outside/system-save-metadata`. **Assignment:** Lower compute. **Prerequisites:** 83 merged.

**Problem.** Persistent settings and run-history state need a versioned host contract before UI integration.

**Do.** Inventory pinned offline settings/history fields; implement one bounded serializable metadata slice, add migration/rejection cases and document incompatible upstream-browser import. Do not equate offline history with server account features.

**Pass.** H; roundtrip, older-field defaults, newer-version rejection, CRC/truncation failure and history append fixtures pass; report source-derived fields and exclusions.

### Task 85: Run-driven 3DS presentation acceptance (game/, lane E)

**Branch:** `outside/run-driver-acceptance`. **Assignment:** Human/console gated; deferred. **Prerequisites:** Headless contracts stable; separate authorization after HANDOFF7 gate.

**Problem.** The displayed battle is still a demo; a headless clear does not prove menus, rendering, input, audio or saving work on hardware.

**Do.** Planning card only under current HANDOFF. Stop before implementation requiring console/emulator judgment; report the gate. After an explicitly authorized successor task, integrate one screen flow at a time and capture reproducible emulator/Old 3DS acceptance results.

**Pass.** Not executable as an outside task under HANDOFF7. Future acceptance must cover starter-to-wave, party/reward/capture/doubles/final-clear flows, SD save/reload and measured memory/frame/audio behavior; host builds alone cannot pass it.

### Task 86: Pinned parity ledger and upstream drift (docs/, lane G)

**Branch:** `outside/parity-ledger`. **Assignment:** Lower compute. **Prerequisites:** All landed tasks summarized; independent docs lane.

**Problem.** The pinned 8555c08 target differs from current public beta. A one-to-one claim requires a named target and a coverage ledger, not a percentage guessed from source size.

**Do.** Maintain a matrix of subsystem/function/supported scope/source path/commit/test command/count/exclusions; record public beta separately by full SHA. Inventory untested move/ability classes without claiming an exhaustive audit. Link each merged PR and each unavailable check.

**Pass.** git diff --check; every claimed result links to recorded output and tested SHA. No stale implementation status presented as current evidence; no public-beta mechanics silently mixed into pinned parity.

### Task 87: Expanded save/no-save divergence (game-core/, lane C)

**Branch:** `outside/save-soak-divergence`. **Assignment:** Hard. **Prerequisites:** Lane C exclusively scheduled; pause Task57 before changing the same lane.

**Problem.** At main `2a740ab`, 1,000 soaks give no-save `5c084cbb` versus saved `f12eb771`, despite the 200-run CI gate passing. Task57 also differs (`47b699cc` versus `ec350294`). Both sets finish without hangs/crashes/timeouts. A baseline failure does not prove that every changed-branch difference has the same cause.

**Do.** Run before changes: `game-core/build/gcplay --auto 1000 --jobs 8` and with `--save-test`. Locate the first seed/phase/state divergence without editing the existing digest or soak chooser. Fix production save/load or phase state in game-core. Add focused replay coverage, then rebase Task57 and verify its own before/after state. Do not lower count or remove counters from digest.

**Pass.** H has identical save/no-save digests for baseline and rebased item adapter, zero crash/hang/timeout, and legal saves resume the same queue/state. Include exact first-failing seed and diagnosis. If a check is defective, stop and report under HANDOFF2.

### Task 88: Existing AI matchup differential failures (ai-core/, lane F)

**Branch:** `outside/ai-matchup-differential`. **Assignment:** Hard. **Prerequisites:** Lane F exclusively scheduled; Task58 kept draft until the gate is addressed.

**Problem.** The untouched 5,000-case AI producer/comparator has 2, 9, 9 and 12 mismatches on seeds 777, 9001, 4242 and 31337 in this environment. Original and Task58 C replay the same files with byte-identical failures; printed differences concern trainer MATCHUP floating-point tails.

**Do.** Compare exact upstream operations, JavaScript rounding/order and C matchup math. Fix C fidelity if wrong. Preserve equality checks and tolerances. If producer/comparison is wrong, stop and report the evidence instead of relaxing it. Keep battle-core read-only.

**Pass.** A gives zero mismatches on all four seeds with coverage no lower than 4,938 / 4,946 / 4,942 / 4,936; focused Task58 berry producer remains 268/268 and host checks404/404. Record compiler/runtime versions and before/after raw score values.

### Task 89: Reward full-state validation (reward-core/, lane B)

**Branch:** `outside/reward-full-state-validation`. **Assignment:** Hard. **Prerequisites:** Exclusive reward lane; existing apply APIs available.

**Problem.** Existing application producer output covers HP/status/PP; it does not establish evolution, form, fusion metadata, held inventory, stats or queued lifecycle parity. Source handlers that validate an evolution item and return success do not prove evolution happened.

**Do.** Add a separate producer/comparator around real upstream item application for a bounded evolution/form/fusion family. Compare complete changed party members, modifier ownership, stats, moves, ability/fusion attributes and queued lifecycle events. Fix reward-core code only; report missing game-core lifecycle APIs separately. The user's 2026-10-04 authorization permits established input/import repairs that preserve assertions and coverage.

**Pass.** A planted species or ability mutation fails the new suite; then at least2,000 cases per each of four seeds show zero mismatches with nonzero successful/rejected applications of each family. Existing reward option/victory/apply regressions retain coverage. State precisely which lifecycle remains in game-core.

### Task 90: Repair current battle regression failures (battle-core/, lane A)

**Branch:** `outside/battle-baseline-regressions`. **Assignment:** Hard/coordinator. **Prerequisites:** Lane A free; upstream pin unchanged. Task 67 stacks on this repair.

**Problem.** The B regression suite fails on main before Task 67: plain 1, rich 7, switch 2 and summon 30 mismatches at 2,000 cases / seed 777.

**Do.** Fix C behavior against the pinned upstream source: form-based Max immunities, form callback applicability and enabled Daily passives whose ability is `NONE`. Keep established producers, comparators and exclusions unchanged; document before/after counts.

**Pass.** B: plain, rich, switch and summon have zero mismatches and covered counts at least 1940, 1826, 1885 and 1855 respectively. Host tests and 3DS CI pass. This does not enable natural Illusion or unlock Task 68 by itself.

### Task 91: Separate reward learnset caller contracts (reward-core/, lane B)

**Branch:** `outside/reward-learnset-inputs`. **Assignment:** Hard/coordinator. **Prerequisites:** Lane B free; upstream pin unchanged. Task 59 integration awaits this API.

**Problem.** One `pr_reward_mon.level_moves` list cannot represent both actual TM exclusions and Memory Mushroom pre-evolution/relearner rows. The established Memory producer also has a stale ValueHolder import and emits no cases at the pin.

**Do.** Add an explicit separate relearner list, preserve legacy callers, use base moves for Memory exclusions under summon overrides, and preserve duplicate TM history as actual upstream does. Add a separate simultaneous getter/Memory/TM generator producer. Do not edit established checks; report the stale import and stop readiness pending its separately reviewed repair.

**Pass.** New dual-input comparisons: 2,000 per seeds 777, 9001, 4242 and 31337, zero Memory/TM/RNG mismatches, nonzero distinct-list/fusion/summon-override cases; mutation detected. Explicit-empty/bounds host checks pass. Existing reward option/victory/application coverage and mismatch counts do not worsen. Established Memory producer must execute and replay cleanly after reviewed test-contract repair; no unexecuted cases credited. Host/3DS CI pass. No game lifecycle parity claim.

## Cards added after the parity ledger (2026-10-04)

Numbered from 92 in the post-parity plan (cards 92 to 151 by milestone; the full plan lives in lorenzo's project notes, outside this repo). Lorenzo approved cards 92 to 96, the lane C order in the table above, card 100's producer edits and card 110 on 2026-10-04 (see HANDOFF, "Settled project decisions"). Every card inherits HANDOFF and the validation aliases above. Sizes are not time estimates. Claims about current behaviour were read from code at main `6f944f7`; re-check each against `main` before starting.

### Task 92: Finish the turn: `pr_continue` and the enemy's switch-out (game-core/, lane C)

**Branch:** `outside/turn-tail-and-enemy-switch`. **Assignment:** Hard (exact-port model). **Prerequisites:** None.

**Problem.** game-core never calls battle-core's `pr_continue` (`battle-core/include/pr/engine.h:201`), so after a victory the rest of the turn is dropped (`gc_phases.c:20,34,41`, stub `GC_STUB_TURN_TAIL`), and an AI switch-out loses the enemy's turn (`gc_battle.c:427`, `GC_STUB_ENEMY_SWITCH`). HANDOFF Task 12's leftovers; some README rows may be stale, so check each against code first.

**Do.** Carry on past the victory stop with `pr_continue`, wire the enemy side's switch-out, and the "Will you switch?" prompt at a trainer's next Pokemon if it is still missing. Delete the matching stub counters and README rows.

**Pass.** H. "rest of the turn after a victory lost" and "trainer's switch-out" read 0 in `gcplay --auto 1000`; one focused fixture per path (victory with a pending end-of-turn effect, trainer switch-out, forced switch); any digest change explained.

### Task 93: Boss segments in battles (game-core/, lane C)

**Branch:** `outside/boss-segments-run`. **Assignment:** Lower compute. **Prerequisites:** None (battle-core's boss shield code is merged).

**Problem.** `gc_mon.c:112-113` sets `is_boss` and `gc_mon.boss_segments`, but never battle-core's `boss_segments` / `boss_segment_index` on the battle Pokemon, so shields and their stat boosts never run in a game-core battle.

**Do.** Set both from run-core's segment count when the encounter is created (wild, trainer boss, mystery spawn), keep them through save/resume (`gc_save.c` field list), and expose the segment count to the app. Leave the classic final boss's first-segment rule to Task 66.

**Pass.** H; the soak report's "(boss segments)" count is 0; fixtures for a legendary wave, a trainer boss and a save/load in the middle of a boss; the battle-core boss-segment differential runs unchanged. State plainly that this is host consistency, not a new upstream comparison.

### Task 94: Catching in the run: verify against upstream and finish (game-core/, lane C)

**Branch:** `outside/catching-audit`. **Assignment:** Hard. **Prerequisites:** None. Task 93 first if the caught-boss case is in scope.

**Problem.** Catching is already wired (PR #55): `GC_CH_BALL` reaches battle-core's capture phase (`gc_battle.c:311-325`), `victory()` adds the Pokemon, a full party opens the swap prompt and the dex is updated (`gc_phases.c`). Nothing checks this against upstream's `VictoryPhase` catch branch and `AttemptCapturePhase`; `GC_STUB_CATCH` is declared and never incremented; the game-core README still says "No catching or fleeing". Candy on a catch, a caught boss and the catch-related charms are not shown to be handled (unknown).

**Do.** Audit the catch branch against the pinned upstream, add focused fixtures (each ball type, full party, caught boss, candy and egg-move bookkeeping), delete the dead stub and fix the README row. Fleeing is Task 77, next in lane C.

**Pass.** H; fixtures compare result and RNG with battle-core's capture fixtures and the dex/candy deltas with the existing dex differential; save/resume with the new party member.

### Task 95: Reward application, part 1: TMs and Memory Mushroom (game-core/, lane C)

**Branch:** `outside/reward-apply-tm-memory`. **Assignment:** Hard. **Prerequisites:** 60 and 91 merged (both are).

**Problem.** The soak reports thousands of rewards "taken without their own effect" and the tally cannot say which classes. Nothing applies a TM or Memory Mushroom, so the move-copy screen is unreachable.

**Do.** First add a per-class tally to `gcplay` (additive; do not touch the digest definition). Then apply TM and Memory Mushroom through reward-core, including the copy screen, `usedTMs` and the learn-move flow.

**Pass.** H; the tally lists every class; TM and Memory fixtures plus Task 91's dual-input comparisons pass through the adapter; reward-core regressions unchanged.

### Task 96: Reward application, part 2: stat and level items, balls, Full Heal, lapsing items (game-core/, lane C)

**Branch:** `outside/reward-apply-consumables-2`. **Assignment:** Lower compute. **Prerequisites:** 95 merged.

**Problem.** Mints, vitamins, Rare Candy (Candy Jar), Full Heal, Poke Balls as items, Lures and Charms, and the post-battle lapsing items (`GC_STUB_POST_BATTLE`: Pickup, Honey Gather, Pay Day) are still stand-ins.

**Do.** Apply each through reward-core's API (Rare Candy has `pr_reward_apply_candy`). Evolution, form and fusion items stay with Tasks 61 and 62.

**Pass.** H; the stub counters for these classes read 0; per-item application tallies; table cases as in Task 60.

### Task 100: Run-core producer repairs: `gym_e4` crash and `rival` memory (run-core/, lane D)

**Branch:** `outside/run-core-producer-repairs`. **Assignment:** Hard; run-core owner only. **Prerequisites:** None. Lorenzo allowed these producer edits on 2026-10-04 (HANDOFF rule 2 otherwise forbids them).

**Problem.** The upstream `gym_e4` producer throws a `TypeError` (`p.moveset` undefined in the trainer closures). The `rival` producer needs 3 to 5 GB and over an hour per seed.

**Do.** Repair each producer minimally and state the case counts before and after. Do not loosen any comparison, tolerance, seed or count.

**Pass.** `gym_e4` and `rival` at 3000 cases on seeds 777, 9001, 4242 and 31337 run to completion on Node 22 at zero mismatches, or the PR says exactly what still fails. A planted mutation is caught. The rest of R unchanged.

### Task 110: App: drive the run from game-core (game/, lane E)

**Branch:** `outside/app-run-driver`. **Assignment:** Hard; project thread, not an outside-agent task. **Prerequisites:** Authorized by lorenzo on 2026-10-04. Cards 92 and 93 preferably first. Plan: `game/WIP-run-driver.md`.

**Problem.** The `game/` app boots a demo battle fed by a stand-in; nothing drives game-core's run (Task 85's software half).

**Do.** A `run_screen.c` that owns the run and drains its events, splitting at each wave; start each battle from run state, answer COMMAND with the battle command screen, SWITCH with the party screen and LEARN_MOVE with the learn dialog; use stand-ins for starter choice, rewards and biome choice (via the existing options UI); evolution automatic; game over starts a new run. Delete `battle_setup.c` and move the event and switch tests to a host test over whole runs. One screen flow at a time.

**Pass.** A host `runtest` plays N seeded runs through the converter without assertion failures; emulator screenshots of one run flow (wave 1, a reward, wave 2). No hardware claim; console acceptance stays Task 85 / card 118.

## Beyond these bounded slices

This backlog is deliberately not a promise that 33 PRs produce full parity. Later tasks must separately bound remaining move/ability dispatch, Transform/Sketch and boss cases, complete held-item families, reward lifecycle classes, mystery encounter families, custom Daily event configurations, Endless/Challenge mechanics, egg-event differences, achievements and statistics, translations/tutorials, full menus and presentation, audio/animation acceptance, and Old 3DS resource limits. Online account/server features require an explicit offline replacement policy. Custom content follows pinned upstream parity; it is not parity work. Each further task needs the same Problem/Do/Pass contract and an owning lane.

## Public upstream drift inspected separately

Public beta `4f0969875845fb412c5b0af8ed4bc006484a8f2a` has42 commits since pinned `8555c08` (including merge commits). Its commit history/source diff includes priority-move weighting and forcing two damage moves, fixed trainer moveset functions, passive Trace behavior, trapping/damage-over-time tags, No Retreat once per switch-in, Revival Blessing held-item handling, Uncommon Breed balance, summary move deletion, tutorial speaker boxes and Pokedex/UI fixes; assets/locales pins also changed. These differences are **outside the named pin**, not evidence that a feature already ported at the pin is missing. Latest-beta parity requires a separately reviewed target/pin migration and new traces. See upstream paths `src/ai/ai-moveset-gen.ts`, `src/data/balance/moves/moveset-generation.ts`, `src/data/trainers/trainer-config.ts`, `src/data/battler-tags.ts`, `src/data/moves/move.ts`, `src/field/pokemon.ts`, and `src/ui/handlers/`.


## PR #86 CI and diagnostic audit

Run [#63](https://github.com/Gianciosvaldo/pokerogue-3ds/actions/runs/37113138258), head `4f9c7ff9081d841acba31552dfb339956d7e269a`: `build-3ds` PASS; host `test` FAIL. All 14 printed save checks pass, but `make -C game-core test` rejects no-save digest `71f2f276` versus saved `e95ed218` (make error 1; Actions step exit 2). Its logs contain unconditional `R48W` output, interleaved across soak workers. That diagnostic should never ship unconditionally.

PR #86 is now merged. Its final head `e7f54b36c422f53e5f269f22047737ed9612a769` contains only four run-core files and no `R48W`; current main also contains no `R48W`. [Run #80](https://github.com/Gianciosvaldo/pokerogue-3ds/actions/runs/37118573215) passed both host and 3DS jobs. No extra removal patch is needed.

The original four unavailable rival checks were subsequently run in [#75](https://github.com/Gianciosvaldo/pokerogue-3ds/actions/runs/37114586055), at head `3966f42930a8e0a399b808a47a786f328419e7f3`. Job logs explicitly show `run-core/tools/difftest.sh /tmp/pokerogue-upstream-8555c08 500 <seed> rival` for seeds 777, 9001, 4242 and 31337: each prints **3000 fights, 0 mismatches**; total 12,000. Save digests there are `b7c0a91b`/`b7c0a91b`. These are historical, actual recorded comparisons, not results run by this audit on final head or current main. Final rebased-head upstream comparison remains a distinct gap; fixed-team traces also stop at the deliberate moveset boundary and do not establish full game-core rival lifecycle parity.

## Authorized draft-blocker repairs (2026-10-04)

Results describe the respective unmerged PR branches, not code already on main. Task70/#111: status-token effect is now recorded and required; 8000 focused comparisons and7722 covered item turns pass, B/condition coverage retained; CI#110 host/3DS pass. Task91/#107: correct ValueHolder import restores4000 legacy Memory cases with zero mismatches;8000 dual-input cases and24,000 TM/RNG results pass. Its unchanged135 existing application failures are fixed separately in Task89/#104. Task89: real unseeded status draw is recorded, old-max fusion HP and actual form/IV stat calculation repaired, form selection/hardcore gates corrected, duplicate TM history preserved;12000 established applications and8000 bounded evolution-state comparisons pass. Established options3950pass/0fail/50unported and victory4000pass/0fail. Missing inputs/footer are rejected and planted mutations fail. Latest-head CI is required before marking reward PRs ready. Complete fusion/form/evolution execution, broader held-stat inventories, whole-run upstream and hardware parity remain unvalidated. No unavailable comparison is called a pass.
