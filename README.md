# PokeRogue for the Old 3DS

A native C port of [PokeRogue](https://github.com/pagefaultgames/pokerogue) for the original (Old) 3DS, plus custom content. Offline, homebrew (.3dsx), saves to the SD card.
Upstream pins: game `8555c08`, with its submodule pins assets `87426a7` and locales `270ed0a`.

**This repo holds code only.** Pokémon and PokeRogue art, music, fonts, logos and generated `.pak` files are built locally from upstream onto your own SD card and must never be committed or shared. `.gitignore` enforces this, along with built binaries (`.3dsx`, `.elf`) and the prebuilt emulator core.

## Parity target and verified scope

The replication target is upstream game `8555c08`, not an unspecified current release. The public upstream beta was inspected separately at `4f0969875845fb412c5b0af8ed4bc006484a8f2a` and changes independently; changes after this pin require a separate source audit and deliberately reviewed target update. Generated data and assets must stay aligned with the documented pins.

See [the remaining parity tasks](docs/PARITY-TASKS.md) for 33 task cards (57–89), dependencies, folder ownership and validation gates. [HANDOFF](docs/HANDOFF.md) remains the contribution contract. Run-core belongs to its existing outside-session owner, and full run-driven presentation remains deferred until an explicitly authorized console/emulator task. Host soaks establish internal consistency; only recorded upstream comparisons establish parity for the compared scope. None of the planned checks in that roadmap should be read as completed results.

The source audit still finds a last-four-level-moves enemy fallback and a fixed reward option pool in `game-core/src/gc_stubs.c`. Upstream reward options and portions of battle/run generation being implemented in their own cores do not automatically remove these run integration gaps. Fixed-team generation also needs separate checks of its actual run lifecycle. Doubles, form/fusion/reward lifecycle, progression/eggs/modes, presentation and hardware acceptance require further bounded work; the roadmap does not claim an exhaustive audit of every move and ability.

The existing `game-core/tests/mystery_diff_test.c` reads TYPE/OPTION while ignoring RESULT_MONEY, and its producer catches UI errors. Its mystery encounter traces do not establish action or money parity. A correction to an established check must be reported under HANDOFF; an additive, separately reviewed harness is proposed in Task 82. No unavailable upstream differential is counted as passing.

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
| [`ai-core/`](ai-core/) | Enemy command/scoring logic | Historical singles coverage exists; the current 5,000-case regression rerun has 2/9/9/12 mismatches across four seeds. Focused berry restriction fix is a draft with 268 upstream comparisons passing |
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
- [Task 57 draft #97](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/97): seven held-item keys mapped and consumed berry/White Herb stacks reconciled on both sides; 648 actual upstream adapter/item-loss comparisons and 39 lifecycle checks pass. The standard 200-run save pair is `1d0a6108`/`1d0a6108`; expanded 1,000-run digests differ on both original main and the draft. This prevents a completed parity claim (Task87).
- [Task 58 draft #98](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/98): actual BerryModifier recognition for Stuff Cheeks; 268 upstream comparisons and 404 host checks pass. The untouched AI suite still has 2/9/9/12 failures, reproduced identically with original C (Task88). No tolerance/check/seed/count was weakened.
- Public-beta drift: 42 commits after the pin (merge commits included), changing moveset weighting/fixed moves, passive Trace, trapping/DoT, No Retreat, Revival Blessing, Uncommon Breed, summary move deletion and UI/assets. Latest-beta parity remains a separately scoped migration.
- PR #86 audit: run #63 failed save equality (`71f2f276`/`e95ed218`) while its 3DS build passed. Temporary `R48W` output is absent from the merged final patch and current main; final-head run #80 passed host/3DS. Four rival comparisons were later actually logged in run #75 (12,000 fights, zero mismatches), on an earlier head; see the [audit details](docs/PARITY-TASKS.md#pr-86-ci-and-diagnostic-audit) for exact commits and the remaining final-head/lifecycle gap.
- Remaining verification: final changed heads require their own recorded checks. Previously tested commits and host builds cannot establish differential results for a later head.

## License
PokeRogue's code is AGPL-3.0, and this port derives from it. Pokémon is © Nintendo, Game Freak and The Pokémon Company; nothing of theirs is included here.
