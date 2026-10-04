# PokeRogue for the Old 3DS

A native C port of [PokeRogue](https://github.com/pagefaultgames/pokerogue) for the original (Old) 3DS, plus custom content. Offline, homebrew (.3dsx), saves to the SD card.
Upstream pins: game `8555c08`, with its submodule pins assets `87426a7` and locales `270ed0a`.

**This repo holds code only.** Pokémon and PokeRogue art, music, fonts, logos and generated `.pak` files are built locally from upstream onto your own SD card and must never be committed or shared. `.gitignore` enforces this, along with built binaries (`.3dsx`, `.elf`) and the prebuilt emulator core.

## Parity target and verified scope

The replication target is upstream game `8555c08`, not an unspecified current release. The public upstream beta was inspected separately at `4f0969875845fb412c5b0af8ed4bc006484a8f2a` and changes independently; changes after this pin require a separate source audit and deliberately reviewed target update. Generated data and assets must stay aligned with the documented pins.

See [the remaining parity tasks](docs/PARITY-TASKS.md) for 33 task cards (57–89), dependencies, folder ownership and validation gates. [HANDOFF](docs/HANDOFF.md) remains the contribution contract. Run-core belongs to its existing outside-session owner, and full run-driven presentation remains deferred until an explicitly authorized console/emulator task. Host soaks establish internal consistency; only recorded upstream comparisons establish parity for the compared scope. None of the planned checks in that roadmap should be read as completed results.

The source audit still finds a last-four-level-moves enemy fallback and a fixed reward option pool in `game-core/src/gc_stubs.c`. Upstream reward options and portions of battle/run generation being implemented in their own cores do not automatically remove these run integration gaps. Fixed-team generation also needs separate checks of its actual run lifecycle. Doubles, form/fusion/reward lifecycle, progression/eggs/modes, presentation and hardware acceptance require further bounded work; the roadmap does not claim an exhaustive audit of every move and ability.

The existing `game-core/tests/mystery_diff_test.c` reads TYPE/OPTION while ignoring RESULT_MONEY, and its producer catches UI errors. Its mystery encounter traces do not establish action or money parity. A correction to an established check must be reported under HANDOFF; an additive, separately reviewed harness is proposed in Task 82. No unavailable upstream differential is counted as passing.

## Coordinator validation status (2026-10-03)

Checked main `cdac97d859cd39ef96200570287efb28b77bc287` after Task 88 / PR #102 merged. Task numbers and PR numbers differ; the 33 task cards remain in [docs/PARITY-TASKS.md](docs/PARITY-TASKS.md).

| Task | PR | State and evidence |
|---|---|---|
| 67: Illusion | [#103](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/103) | Draft diagnostic. The pinned upstream disables the ability and marks all 9 tests TODO; 0 executed. A nonzero disguise/break/switch differential is unavailable at this target |
| 87: save/resume | [#101](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/101) | Draft. Three production resume bugs have focused fixes, but seed 849 exhausts the existing soak budget and is incorrectly reported ended. Expanded equality is still unpassed |
| 88: AI matchup | [#102](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/102) | Merged diagnosis. Node 22: 19,762 covered/identical, zero mismatches, 238 uncovered. Node 24 reference migration remains unresolved |
| 89: evolution application | [#104](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/104) | Draft. New application/queue slice: 8,000/8,000 identical; species/ability mutations detected. Established apply regression: 3,865 pass / 135 fail on both original and changed C. Fusion status uses an unrecorded Math.random draw; no fusion parity claim |

Task 89 now exports evolution requests and preserves full named application-state fields until a caller executes them. This does not execute evolution, form/fusion changes or progression in game-core. Its stronger victory check passes 20,000/20,000; standard 200-run save digests match `030a9394`/`030a9394`. Exact commands, exclusions and outputs are in the PR and owning README. No expanded save, Illusion or complete fusion differential is credited.

Draft PRs stay draft until their validation blockers are resolved. Task 68 still needs completed Task 67; Task 59 still needs completed Task 87. Task 69 and the later battle chain still need Task 67. Task 63 belongs to the run-core owner and Task 85 remains console gated. No new three-task Jules wave is ready under the existing prerequisites. [HANDOFF rule 2](docs/HANDOFF.md#rules-read-these-before-touching-anything) requires reporting defective checks instead of changing them to pass.

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
- [Task 57 merged #97](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/97): seven held-item keys mapped and consumed berry/White Herb stacks reconciled on both sides; recorded 648 upstream adapter/item-loss comparisons and 39 lifecycle checks pass. Expanded save equality remains blocked by Task 87; its fixes are still draft.
- [Task 58 merged #98](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/98): actual BerryModifier recognition for Stuff Cheeks; recorded 268 upstream comparisons and 404 host checks pass. [Task 88 merged #102](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/102) diagnoses the 2/9/9/12 failures as a reference-runtime difference: the unchanged four-seed AI suite passes under HANDOFF's Node 22, with no reduced coverage or relaxed comparisons. Upstream's declared Node 24 environment remains a gap.
- Public-beta drift: 42 commits after the pin (merge commits included), changing moveset weighting/fixed moves, passive Trace, trapping/DoT, No Retreat, Revival Blessing, Uncommon Breed, summary move deletion and UI/assets. Latest-beta parity remains a separately scoped migration.
- PR #86 audit: run #63 failed save equality (`71f2f276`/`e95ed218`) while its 3DS build passed. Temporary `R48W` output is absent from the merged final patch and current main; final-head run #80 passed host/3DS. Four rival comparisons were later actually logged in run #75 (12,000 fights, zero mismatches), on an earlier head; see the [audit details](docs/PARITY-TASKS.md#pr-86-ci-and-diagnostic-audit) for exact commits and the remaining final-head/lifecycle gap.
- Remaining verification: final changed heads require their own recorded checks. Previously tested commits and host builds cannot establish differential results for a later head.

## License
PokeRogue's code is AGPL-3.0, and this port derives from it. Pokémon is © Nintendo, Game Freak and The Pokémon Company; nothing of theirs is included here.

## Task 91 validation repair (2026-10-04)

The user's authorization resolved the established Memory producer blocker: ValueHolder now imports from `#utils/value-holder`, its actual pinned upstream module. No assertion, case count, seed or comparison was removed. Fresh seed99/N4000 Memory replay: **4000 cases, 0 mismatches**. The separate dual-input suite was rerun on Node24.19.0/Vitest4.1.11 at upstream `8555c08`: four seeds ×2000 = **8000 identical, zero Memory/TM/RNG mismatches**, with 24,000 TM/RNG comparisons; explicit-empty/bounds checks pass, shared-list mutation fails, incomplete trace exits2.

Established seed99/N4000 options remain3950pass/0fail/50unported, victory4000pass/0fail, application3865pass/135fail (unchanged existing fusion gap, addressed separately in Task89/#104). Task91's pass gate requires no worsening of that established application baseline; those135failures are never credited as passes. Host test exits0, paired200-run digests `d1eb301b`/`d1eb301b`. CI must pass on the updated head before ready-for-review transition. Integration Task59 requires this API to merge on main; Task60 still requires Task59 and its documented prerequisites. No full reward/run/hardware lifecycle claim.
