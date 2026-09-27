# PokeRogue for the Old 3DS

A native C port of [PokeRogue](https://github.com/pagefaultgames/pokerogue) for the original (Old) 3DS, plus custom content. Offline, homebrew (.3dsx), saves to the SD card.
Upstream pins: game `8555c08`, assets `056a1f4`, locales `270ed0a`.

**This repo holds code only.** Pokémon and PokeRogue art, music, fonts, logos and generated `.pak` files are built locally from upstream onto your own SD card and must never be committed or shared. `.gitignore` enforces this, along with built binaries (`.3dsx`, `.elf`) and the prebuilt emulator core.

## Layout
| Folder | What it is | Status |
|---|---|---|
| [`docs/feasibility.md`](docs/feasibility.md) | Feasibility audit of the port | Done |
| [`benchmark/`](benchmark/) | Upstream game logic run headless in QuickJS, on PC and 3DS | Done. Old 3DS median turn 2.98 s, so the JS route was dropped for a native port ([RESULTS](benchmark/RESULTS.md)) |
| [`skeleton/`](skeleton/) | First app: `rr` renderer over citro2d, SD saves, battle demo scene | Done, tested in Azahar |
| [`assets-pipeline/`](assets-pipeline/) | Builds the 5 SD packs (gfx, snd, bgm, font, data) from upstream, and `app-src/`: the skeleton extended with pack, sound and font loaders | Done, tested in Azahar |
| [`battle-core/`](battle-core/) | Native C battle logic, diff-tested against upstream | In progress: RNG, stats, damage, and the abilities (passives included), move effects, held items, battler/arena tags and turn history (move histories, attacks received, turn order) that affect damage match upstream in every covered test case, and in single battles only 6 random cases in about 110,000 still hit unported code ([README](battle-core/README.md)) |
| [`run-core/`](run-core/) | Native C run logic around the battles, diff-tested against upstream; reads battle-core, never changes it | In progress: wave rules (enemy levels, trainer waves, boss segments, level cap, double-battle chance, new-battle seeding) and the shop's economy (money, shop items and prices, Black Sludge, Amulet Coin, reroll cost) match upstream in every test case; reward pools next ([README](run-core/README.md)) |
| [`tools/azahar-libretro/`](tools/azahar-libretro/) | Headless emulator test rig (binaries not in git) | See its README |

`assets-pipeline/app-src/` is the current app; `skeleton/src/` is kept as the earlier baseline.

## Building
- App: `docker run --rm -v "$PWD/assets-pipeline/app-src":/src -w /src devkitpro/devkitarm make` (details in [`skeleton/README.md`](skeleton/README.md)).
- SD card packs: `python3 assets-pipeline/build_sd.py ...` (details in [`assets-pipeline/README.md`](assets-pipeline/README.md)).

The READMEs mention screenshots and pixel-test images. Those contain Pokémon art, so they are not in this repo.

## License
PokeRogue's code is AGPL-3.0, and this port derives from it. Pokémon is © Nintendo, Game Freak and The Pokémon Company; nothing of theirs is included here.
