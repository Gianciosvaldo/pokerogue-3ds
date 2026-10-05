# game/menus: the menu screens

The second screens workstream of the [beta plan](../../docs/beta-plan.md): party, summary, reward select, starter select, learn move and the next-biome choice. Each screen is a C port of upstream's handler at `8555c08` (`src/ui/handlers/*-ui-handler.ts`), using upstream's own positions, images, text styles and strings.

The screens don't read battle-core or game-core state. The caller fills plain display structs (`menu_mon` in `menus.h`) and gets the player's choices back through callbacks, the way upstream's handlers call their `selectCallback`. So the game app can drive them from whatever run state it has.

## Status

| Screen | Upstream | State |
|---|---|---|
| Party | `party-ui-handler.ts` (2,341 lines) | Done. All 14 modes are ported. Run in Azahar: switching (with the fainted-Pokémon message), item use, move items with PP, TMs (Able, Not able, Learned), Memory Mushroom, held item transfer with quantities, discard, release from the team check (confirm and goodbye line), singles and doubles layouts. Written but not yet run: faint switch, pre-battle switch, Revival Blessing, DNA Splicer, release when catching with a full party |
| Summary | `summary-ui-handler.ts` (1,413 lines) | Done. Run in Azahar: the three pages with their slides, the passive and IV toggles, move select and swap, paging through the party from the party screen, shiny variant sprite and star, status and Pokérus box, move effect box |
| Reward select | `modifier-select-ui-handler.ts` | Done. Shop and Reward selection UI (`shop_ui.c`) driven by game-core (`GC_SCREEN_REWARD`), including item targeting via party screen and Lock Capsule toggle. Hosted in the app by `../source/reward_host.c` (card 113). |
| Starter select | `starter-select-ui-handler.ts` | To do |
| Learn move | `learn-move-phase.ts` + the summary in learn-move mode | Done. Run in Azahar end to end: prompt, yes/no, "Which move should be forgotten?", the summary, "1, 2, and… Poof!", "learned" |
| Next biome | option select after a Map | Done. Biome Map selection UI (`biome_map.c`) driven by game-core (`GC_SCREEN_BIOME`). Hosted in the app by `../source/reward_host.c` (card 113). |

Not ported yet in the summary: the hover tooltips (no pointer), the tera type icon, the rainbow on a maxed candy count, held item stack counts (the `item-count` bitmap font isn't in the SD packs yet) and the fusion palette swap on the big sprite (fusions show the base species' sprite). Forms whose sprite is shared with the base form, and female sprites, are found by trying the file and falling back, because battle-core's tables don't carry `formSpriteKey` or `genderDiffs`.

Not ported yet in the party screen (hidden, not faked): the Pokédex option (no Pokédex screen yet), Rename (needs the 3DS keyboard), form-change item toggles in the reward phase (Mega and Tera items are M2), and the move info box beside the Memory Mushroom list.

## Files
- `menus.h`: display structs and each screen's API.
- `mui.h`, `mui.c`: the small UI layer the screens draw with: upstream text styles (fonts, colours, placement), Phaser's word wrap, BBCode colour tags, clipping (for upstream's masks), windows, cached images, English strings from `data.pak`, the typing message box with its prompt arrow, and a yes/no box. It stands in until the game framework (the first screens thread, `game/`) has its own versions; switching over only touches these two files.
- `party.c`: the party screen.
- `summary.c`: the summary screen (DEFAULT and LEARN_MOVE modes).
- `learnmove.c`: the learn-move dialog (upstream `LearnMovePhase`'s messages and yes/no boxes around the summary). The host shows the text and the yes/no box in its own message box and gets the answer as game-core's `GC_CH_FORGET` wants it: the slot, or -1 for not learned.
- `sprite.c`: battle sprites (forms, female, shiny, variant colour maps or variant sheets from the masterlist) and their 10 fps animation.
- `menus_names.c`: upstream i18n keys for moves, biomes and natures and the type atlas frame names, generated from battle-core's `pr_enums.h` by `tools/gen_names.py`.
- `icon.c`: Pokémon party icons (form, female, shiny and variant icons, fusion halves), from battle-core's species tables.
- `menus_data.c`: names from battle-core's tables (move names).
- `test/`: `menutest`, a standalone app that runs the screens with a made-up party. Every choice is logged to the bottom screen and to `sd:/3ds/pokerogue/menutest.log`. L/R switch the party mode, SELECT toggles a double battle. `sd:/3ds/pokerogue/menutest.txt` holding `<mode> <double>` picks the starting mode; mode 14 opens the summary, 15 the summary in learn-move mode, 16 the whole learn-move dialog (with a stand-in message box).

## Building and running the test app
```
docker run --rm -v "$PWD":/src -w /src/game/menus/test devkitpro/devkitarm make
```
from the repo root. It needs `gfx.pak`, `font.pak` and `data.pak` on the SD card (`assets-pipeline/build_sd.py --only gfx,font,data`). In the Azahar rig (`tools/azahar-libretro/`), for example:
```
lr-run azahar_libretro.so game/menus/test/menutest.3dsx DATA frames=300 press=200:down press=220:a shot=290:out.png
```

## Text metrics
Upstream draws every text object at canvas size (96 px for the default 16 px text) and scales it by 1/6, so where a line's glyphs land depends on Phaser's measured font metrics. I measured them with Phaser 3.90 (upstream's version) in headless Chromium using upstream's two fonts:

| Font, canvas size | Ascent | Line height | Text box height (default padding) |
|---|---|---|---|
| emerald 96 px (16) | 72 | 84 | 90 (15 px) |
| emerald 72 px (12) | 54 | 63 | 69 |
| emerald 66 / 64 / 60 / 56 px | 50 / 48 / 46 / 42 | 58 / 56 / 54 / 49 | |
| emerald 76 px (13) | 58 | 68 | 74 |
| pkmnems 48 px (8) | 66 | 66 (descent 0) | 84 with the party style's padding (14 px) |

In canvas px unless marked. Multi-line text advances by line height plus `lineSpacing` (5 canvas px for `addTextObject`), so 14.8 px per line for 16 px text, not 16. The baked fonts in `font.pak` put the baseline 12 px below the line top for both emerald 16 and pkmnems 8, which puts pkmnems text 1 px lower than upstream. `mui.c` corrects both.
