# game/: the PokeRogue 3DS app

The game itself: the screens, their input and drawing, built on the renderer, packs, sound and
saves from [`assets-pipeline/app-src/`](../assets-pipeline/app-src/), and linking the logic cores
([`battle-core/`](../battle-core/), [`run-core/`](../run-core/), and `ai-core/`, `reward-core/`,
`game-core/` once they exist). The cores are read-only here.

Status: the framework is in, and the battle screen plays a demo battle whose turns are **run by
battle-core** and shown from its events (`battle-core/include/pr/events.h`): the texts upstream
shows, move animations, hits and HP bars, stat changes, faints, switching. The enemy's moves come
from ai-core. A Pokemon in the air or underground (Fly, Dig...) disappears and comes back as
upstream's SemiInvulnerableTag makes it do. POKEMON and a faint open the menus thread's party screen (`menus/`) on the bottom
screen to switch; BALL opens the Poke Ball list (throwing one says catching isn't in yet). The battle itself is built by a stand-in (`battle_setup.c`), which goes when the app
drives game-core's run (`game-core/include/gc/run.h`).

## Build and run
From the repo root (the whole repo is mounted because the game links the cores):

```
docker run --rm -v "$PWD":/src -w /src/game devkitpro/devkitarm make
```

That gives `game/pokerogue.3dsx`. It needs the packs (`gfx.pak`, `data.pak`, `font.pak`; `snd.pak`
for sound) in `sdmc:/3ds/pokerogue/`, built with [`assets-pipeline/`](../assets-pipeline/). Without
them it shows a message saying so.

In the emulator rig ([`tools/azahar-libretro/`](../tools/azahar-libretro/)), put the packs in
`DIR/Azahar/sdmc/3ds/pokerogue/` and run
`lr-run azahar_libretro.so game/pokerogue.3dsx DIR frames=700 shot=620:out.png press=640:a`.

Controls: upstream's gamepad layout by button position. A = ACTION, B = CANCEL, START = MENU,
SELECT = STATS, Y = CYCLE_NATURE, X = CYCLE_TERA, L = CYCLE_FORM, R = CYCLE_SHINY,
ZL = CYCLE_GENDER, ZR = CYCLE_ABILITY. START + SELECT together quits.

## Screens
Upstream draws everything on one 320x180 canvas. Here:
- **Top screen (400x240)**: the battle field at 1:1, centred (the field and its info boxes).
- **Bottom screen (320x240)**: upstream's `ui` container, bottom-aligned, so the message and
  command strip (upstream y -48..0) is the last 48 rows. Full-screen menus (party, summary,
  starter select...) use `UI_BOTTOM_CANVAS`: a whole 320x180 canvas, centred.

Every position in the code is upstream's own number in one of these spaces (see `ui_space` in
[`include/ui.h`](include/ui.h)), so layouts can be copied from upstream's handlers directly.

## Framework (for anyone writing a screen)
| Header | What it is |
|---|---|
| [`ui.h`](include/ui.h) | The screen stack (`ui_set` / `ui_push` / `ui_pop`, upstream's setMode / setOverlayMode / revertMode), buttons (upstream's `Button` enum, 250 ms repeat), game time and one-shot timers (`ui_after`), fades, sounds (`ui_play_select`, `ui_play_sound("se/hit")`), drawing spaces (`ui_space`). A screen is a `ui_screen` with `enter/leave/resume/input/update/draw` callbacks. |
| [`ui_text.h`](include/ui_text.h) | Upstream's `TextStyle`s (`TS_MESSAGE`, `TS_WINDOW`, `TS_BATTLE_INFO`, `TS_PARTY`...) with their fonts, sizes, colours and shadows, and Phaser's text layout: baselines and line heights measured from Chromium, `lineSpacing`, word wrap, alignment, typing (`chars`). Pass upstream's `addTextObject` position and style. |
| [`i18n.h`](include/i18n.h) | `i18n_t(out, n, "battle:useMove", "pokemonNameWithAffix", name, ..., NULL)` over the locale JSON in data.pak: namespaces, `{{vars}}`, `$t()` nesting, `_context` and `_one/_other` plurals, fallback to English. Name helpers for species, moves, abilities, types, stats. |
| [`ui_widgets.h`](include/ui_widgets.h) | Images from gfx.pak by upstream key (`img_draw("images/ui/cursor", NULL, x, y, 0.5, 0.5)`, animation frames), `ui_window` (addWindow's 9-slice), `ui_cursor`, `msgbox` (typing, `$` pages, prompt arrow, `@d`/`@s`), `optlist` (OptionSelectUiHandler), `ui_push_options` and `ui_push_confirm` overlays. |
| [`tween.h`](include/tween.h) | Phaser's easing functions and a small tween. |
| [`battle_view.h`](include/battle_view.h) | The battle screen: `bv_begin`, `bv_play(events)`, `bv_ask_command`. |
| [`battle_events.h`](include/battle_events.h) | battle-core's turn events as battle screen events, with their text rendered from data.pak (`pr_msg_format`). Catches up at the end of a turn on anything battle-core doesn't emit yet (HP, faints). |
| [`battle_anim.h`](include/battle_anim.h) | Upstream's battle animation player (`BattleAnim` in `battle-anims.ts`): move, common and charge animations from data.pak, their graphic cells, Pokemon copies, backgrounds, sounds, layers. `ba_preload_move` at battle start. |
| [`party_screen.h`](include/party_screen.h) | `menus/`' party and summary screens as an overlay on the bottom screen, filled from battle-core's party: `ps_open(bt, PS_SWITCH or PS_FAINT_SWITCH, on_pick)`. |
| [`ball_menu.h`](include/ball_menu.h) | Upstream's `BallUiHandler`: the Poke Ball list with counts, `ball_menu_open(counts, on_pick)`. |
| [`battle_setup.h`](include/battle_setup.h) | Stand-in: builds a wild singles battle for the demo until game-core's run drives the app. |
| [`gen/gm_enum_names.h`](gen/gm_enum_names.h) | Names of battle-core's enums (`gm_enum_camel(GM_E_SPECIES_ID, id, ...)` gives upstream's i18n key). Regenerate with `python3 game/tools/gen_enum_names.py` when battle-core's enums change. |

New screens go in their own source files; `menus/` (party, summary, starter select, reward
select, learn move, biome map) is picked up by the Makefile when it exists.

## PC check
`game/tests/` builds the stand-in battle and battle-core on the PC and prints each turn's events
with their English text (needs upstream's `locales/en`):
```
make -C game/tests && game/tests/build/evtest path/to/pokerogue/locales/en 2
```

Switching through the stand-in battle (a switch turn, then faint switches until the party is
out; exits 0 when the level 80 enemy wins, as it should):
```
make -C game/tests build/switchtest && game/tests/build/switchtest path/to/pokerogue/locales/en
```

The animation player runs on the PC over every upstream animation file (all moves from both
sides, the common and charge animations), with ASan, and prints how long each one takes (needs
upstream's assets checkout and libjansson):
```
make -C game/tests animtest && game/tests/build/animtest path/to/pokerogue-assets
```

## Checked against upstream
- Text metrics: Phaser 3.90's `MeasureText` in Chromium for both fonts at every size used (ascent,
  descent, line pitch; the message box's is 14.83 px). The baked fonts' ascents match except
  pkmnems at 8 px (PIL 12, Chromium 11), corrected in the tables.
- Layout numbers from upstream `8555c08`: message and command windows, the fight menu and move
  info, both info boxes (name, level, HP bar and numbers, EXP bar, types, status), sprite anchors.
- Timings: encounter slide 2000 ms, info boxes 1000 ms in and 500 ms out, HP bar
  `clamp(|dHP| x 5, 250, 5000)` ms, damage blink 6 x 100 ms, 20 ms per character, 1500 ms message
  wait, fades 250 / 100 / 250 ms.
- Checked by eye in Azahar screenshots, not pixel-diffed against upstream yet.

## Not done yet
- Driving the app from game-core's run (starters, waves, EXP, rewards) instead of the stand-in.
- Message timing per text (upstream's 500 ms for "X used Y!", prompts after faints): asked of
  battle-core; every text waits 1.5 s for now.
- In move animations: the grey part of a sprite's tone (upstream's shader pushes it far past
  grey; not reproduced), Substitute, and the KO colour inversion.
- Catching and fleeing (battle-core), so a thrown ball and RUN are placeholders.
- Trainer battles, doubles, stat stage and shiny effects, boss HP
  segments, weather and field effects on the field.
- Sound without snd.pak is silent (the packs in the test rig have no sound yet).
- Forms, variants and gender sprite differences in sprite keys.
- Title, save slots, starter select and every other screen in the beta plan.
