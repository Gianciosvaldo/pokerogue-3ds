# PokeRogue for the Old 3DS

A native C port of [PokeRogue](https://github.com/pagefaultgames/pokerogue) for the original (Old) 3DS, plus custom content. Offline, homebrew (.3dsx), saves to the SD card.
Upstream pins: game `8555c08`, with its submodule pins assets `87426a7` and locales `270ed0a`.

**This repo holds code only.** Pokémon and PokeRogue art, music, fonts, logos and generated `.pak` files are built locally from upstream onto your own SD card and must never be committed or shared. `.gitignore` enforces this, along with built binaries (`.3dsx`, `.elf`) and the prebuilt emulator core.

## Parity target and verified scope

The replication target is upstream game `8555c08`, not an unspecified current release. The public upstream beta was inspected separately at `4f0969875845fb412c5b0af8ed4bc006484a8f2a` and changes independently; changes after this pin require a separate source audit and deliberately reviewed target update. Generated data and assets must stay aligned with the documented pins.

See [the remaining parity tasks](docs/PARITY-TASKS.md) for the original 33 task cards (57–89), coordinator follow-ups 90–91, dependencies, folder ownership and validation gates. [HANDOFF](docs/HANDOFF.md) remains the contribution contract. Run-core belongs to its existing outside-session owner, and full run-driven presentation remains deferred until an explicitly authorized console/emulator task. Host soaks establish internal consistency; only recorded upstream comparisons establish parity for the compared scope. None of the planned checks in that roadmap should be read as completed results.

The source audit still finds a last-four-level-moves enemy fallback and a fixed reward option pool in `game-core/src/gc_stubs.c`. Upstream reward options and portions of battle/run generation being implemented in their own cores do not automatically remove these run integration gaps. Fixed-team generation also needs separate checks of its actual run lifecycle. Doubles, form/fusion/reward lifecycle, progression/eggs/modes, presentation and hardware acceptance require further bounded work; the roadmap does not claim an exhaustive audit of every move and ability.

The existing `game-core/tests/mystery_diff_test.c` reads TYPE/OPTION while ignoring RESULT_MONEY, and its producer catches UI errors. Its mystery encounter traces do not establish action or money parity. A correction to an established check must be reported under HANDOFF; an additive, separately reviewed harness is proposed in Task 82. No unavailable upstream differential is counted as passing.

## Coordinator validation status (2026-10-04)

Checked main `ee476524016b0dc0ce6edc93db5baf4f76521325` after PR #106. Task numbers differ from PR numbers. The original 33 cards and new coordinator prerequisites are in [docs/PARITY-TASKS.md](docs/PARITY-TASKS.md); assignment remains **Hard/coordinator** or **Lower compute**.

| Task | PR | State and evidence |
| --- | --- | --- |
| 87: save/resume | [#101](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/101) | Merged. Four production resume bugs fixed, including seed 849's retained switch-out flag. 36,471 checks; all 1000 normal + 1000 saved strict runs reach terminal screens within budget |
| 90: battle regressions | [#106](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/106) | Merged. Seed 777 / N=2000: plain 1940, rich 1826, switch 1885, summon 1856 covered and identical; 0 mismatches, coverage retained. Host and 3DS CI #102 pass |
| 67: Illusion helpers | [#103](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/103), [#108](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/108) | #108 merged directly into main at `cae69338`; bounded loaded/manual helpers are present. CI #104/#105 pass. 8000/8000 manual identity/type helper comparisons; natural activation remains disabled, 9 TODO / 0 executed |
| 88: AI matchup | [#102](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/102) | Merged diagnosis. Node 22: 19,762 covered/identical, 0 mismatches, 238 uncovered. Node 24 reference migration remains unresolved |
| 89: evolution application | [#104](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/104) | Draft. Bounded evolution application/queue slice 8000/8000; existing application regression 3865 pass / 135 fail. Fusion status uses an unrecorded Math.random draw; no full fusion parity claim |
| 91: reward caller inputs | [#107](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/107) | Draft. Separate TM/relearner inputs: 8000/8000 comparisons and 24,000 TM/RNG results match. Host/3DS CI #106 pass. Established Memory producer throws on a stale ValueHolder import before any cases; readiness remains blocked |
| 59: reward integration | Audit only | Fixed option pool remains. Integration needs Task 91's API and its repaired established check; no adapter parity or zero-fallback claim |

Task 87's merged head recorded 1000-run paired digests `0cba4e49`/`0cba4e49`. With Task 90 and the tested Task 67 helper head, fresh 1000-run checks print **1000 ended, 0 cleared/stuck/crashed/timed out, 57,502 turns** on each path and `fea61f28`/`fea61f28`; the 200-run pair is `e59cb7ec`/`e59cb7ec`. The additive strict terminal-screen check independently observes completion and preserves the original chooser/check. These are host consistency results, not upstream run parity.

Task 67's proposed scope covers manually loaded/dormant identity/type helpers, not natural ability selection or a switch-disguise differential. Task 91's new suite does not replace the failed established Memory check. Fresh unchanged seed-99 / N=4000 reward regressions before/after Task 91 are byte-identical: options 3950 pass / 0 fail / 50 unported, victory 4000 pass / 0 fail, application 3865 pass / 135 fail. No missing or failed comparison is credited as passing.

**Jules scheduling:** Task 68's loaded-state AI scope is delegated to Jules after #108 merged into main; Task 60 needs completed 59; Task 71 needs 69 then 70. No new batch of three independent lower-compute tasks is currently unlocked. Merge-triggered notifications are configured to report actual newly available tasks. Task 63 remains run-core owner work; Task 85 remains console gated. [HANDOFF rule 2](docs/HANDOFF.md#rules-read-these-before-touching-anything) requires reporting defective established checks before proceeding; #104 and #107 stay drafts.

## Layout
| Folder | What it is | Status |
|---|---|---|
| [`docs/feasibility.md`](docs/feasibility.md) | Feasibility audit of the port | Done |
| [`docs/beta-plan.md`](docs/beta-plan.md) | Plan to the 10 October 2026 beta: milestones, one workstream per folder, what battle-core does next, the one-click SD card builder | Written 2026-09-28 |
| [`benchmark/`](benchmark/) | Upstream game logic run headless in QuickJS, on PC and 3DS | Done. Old 3DS median turn 2.98 s, so the JS route was dropped for a native port ([RESULTS](benchmark/RESULTS.md)) |
| [`skeleton/`](skeleton/) | First app: `rr` renderer over citro2d, SD saves, battle demo scene | Done, tested in Azahar |
| [`assets-pipeline/`](assets-pipeline/) | Builds the 5 SD packs (gfx, snd, bgm, font, data) from upstream, and `app-src/`: the skeleton extended with pack, sound and font loaders | Done, tested in Azahar |
| [`battle-core/`](battle-core/) | Native battle logic | Source includes singles damage/turns, capture/flee rolls, trainer switch support, held-item slices and selected form triggers. Doubles, remaining forms/item effects and scripted phases remain incomplete; historical differential scopes are in its README |
| [`run-core/`](run-core/) | Waves, encounters, trainers and enemy generation | Source includes fixed rivals and Gym/Elite Four/Champion closures as well as wild/generic trainer and Eternatus/Paradox generation (Task 50). Their producers bypass moveset generation; enemy movesets, scripted boss lifecycle and remaining configurations still need parity work. Owner lane reserved |
| [`reward-core/`](reward-core/) | Reward selection and application | Options/victory APIs plus healing/revive/PP and other application slices exist. Form/evolution/fusion lifecycle needs full-state verification; game-core still uses a placeholder pool |
| [`sd-builder/`](sd-builder/) | One-click Windows builder: downloads PokeRogue's public files at the pins and writes the packs to the SD card, with nothing to install | Built and tested (see its README) |
| [`bench-native/`](bench-native/) | Console speed test of the C battle engine: replays upstream-recorded turns through battle-core on the 3DS, times each one and checks it still matches upstream | Real Old 3DS: 1.51 ms median per singles turn (4.03 ms worst), about 800x faster than the QuickJS route; all covered turns identical on the console ([README](bench-native/README.md)) |
| [`bench-gfx/`](bench-gfx/) | Console test of drawing, texture memory, SD card loading and audio: battle scenes laid out like upstream's (singles, doubles, move animations, the largest sprites), load times from the packs, memory limits, and where the screen edges land | Run on a real Old 3DS without the packs: a battle takes 8.8 ms of the 16.7 ms frame; loading from the packs and audio still to measure on the console ([README](bench-gfx/README.md)) |
| [`ai-core/`](ai-core/) | Enemy command/scoring logic | Task 88 records 19,762 covered/identical cases out of 20,000 under Node 22, zero mismatches; 238 uncovered. Node 24's 2/9/9/12 failures and the runtime-contract conflict remain. Task 58 is merged; Illusion integration remains blocked |
| [`game-core/`](game-core/) | Headless run orchestration, progression and save/resume | Host save/soak tests; enemy movesets and reward options still use stand-ins. See its README and the parity roadmap for lifecycle integration gaps |
| [`game/`](game/) | The game app: screen stack, input, text with upstream's fonts and metrics, strings from the locale files, windows, message box, menus, and the battle screen | In progress: the framework, and the battle screen showing real battle-core turns (texts, hits, HP bars, faints) in a demo battle in Azahar; driving it from game-core's run next ([README](game/README.md)) |
| [`tools/azahar-libretro/`](tools/azahar-libretro/) | Headless emulator test rig (binaries not in git) | See its README |

`game/` is the game app. It takes the renderer, packs, sound and saves from `assets-pipeline/app-src/` (whose own `main.c` is the older demo); `skeleton/src/` is kept as the earlier baseline.

## Building
- Game: `docker run --rm -v "$PWD":/src -w /src/game devkitpro/devkitarm make` from the repo root (the whole repo is mounted because the game links battle-core and run-core).
- Older demo app: `docker run --rm -v "$PWD/assets-pipeline/app-src":/src -w /src devkitpro/devkitarm make` (details in [`skeleton/README.md`](skeleton/README.md)).
- SD card packs: on Windows, the one-click [`sd-builder`](sd-builder/); elsewhere `python3 assets-pipeline/build_sd.py ...` (details in [`assets-pipeline/README.md`](assets-pipeline/README.md)).

The READMEs mention screenshots and pixel-test images. Those contain Pokémon art, so they are not in this repo.

## Audit and change record

- Audit baseline: port main `2a740ab66bce22dd94aa34c6cbf2c8e665ad6a79`, pinned upstream `8555c08`.
- Documentation: added `docs/PARITY-TASKS.md`, identified separate core/run/presentation validation, and listed bounded delegation and hardware gates. Folder README coverage tables are historical measured scopes; read their tested commits and current source before extending a claim.
- [Task 57 merged #97](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/97): seven held-item keys mapped and consumed berry/White Herb stacks reconciled on both sides; recorded 648 upstream adapter/item-loss comparisons and 39 lifecycle checks pass. Task 87 is now merged and its strict completion/equality gate passes; current evidence is above.
- [Task 58 merged #98](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/98): actual BerryModifier recognition for Stuff Cheeks; recorded 268 upstream comparisons and 404 host checks pass. [Task 88 merged #102](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/102) diagnoses the 2/9/9/12 failures as a reference-runtime difference: the unchanged four-seed AI suite passes under HANDOFF's Node 22, with no reduced coverage or relaxed comparisons. Upstream's declared Node 24 environment remains a gap.
- Public-beta drift: 42 commits after the pin (merge commits included), changing moveset weighting/fixed moves, passive Trace, trapping/DoT, No Retreat, Revival Blessing, Uncommon Breed, summary move deletion and UI/assets. Latest-beta parity remains a separately scoped migration.
- PR #86 audit: run #63 failed save equality (`71f2f276`/`e95ed218`) while its 3DS build passed. Temporary `R48W` output is absent from the merged final patch and current main; final-head run #80 passed host/3DS. Four rival comparisons were later actually logged in run #75 (12,000 fights, zero mismatches), on an earlier head; see the [audit details](docs/PARITY-TASKS.md#pr-86-ci-and-diagnostic-audit) for exact commits and the remaining final-head/lifecycle gap.
- Remaining verification: final changed heads require their own recorded checks. Previously tested commits and host builds cannot establish differential results for a later head.

## License
PokeRogue's code is AGPL-3.0, and this port derives from it. Pokémon is © Nintendo, Game Freak and The Pokémon Company; nothing of theirs is included here.


Task 69 implements weather-driven quiet form dispatch for Forecast, Flower Gift and Ice Face, including suppression, loaded/natural passives, summon, replacement and leaving-field resets. See [battle-core validation and reproduction](battle-core/README.md#task-69-weather-driven-quiet-form-changes-2026-10-04). Task 68 is delegated to Jules in lane F; Task 70 awaits Task 69 merging, and Task 71 remains behind 70.
