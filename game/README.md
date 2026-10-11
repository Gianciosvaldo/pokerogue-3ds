# game/: the PokeRogue 3DS app

The game itself: the screens, their input and drawing, built on the renderer, packs, sound and
saves from [`assets-pipeline/app-src/`](../assets-pipeline/app-src/), and linking the logic cores
([`battle-core/`](../battle-core/), [`run-core/`](../run-core/), and `ai-core/`, `reward-core/`,
`game-core/` once they exist). The cores are read-only here.

Status: the app boots into a run. **game-core's run loop** (`game-core/include/gc/run.h`) plays the waves, the
experience, level ups, new moves, evolution, rewards, biome changes and game over, and `run_screen.c` shows them
with the battle screen: each turn is **run by battle-core** and shown from its events
(`battle-core/include/pr/events.h`): the texts upstream shows, move animations, hits and HP bars, stat changes,
faints, switching. The enemy's moves come from ai-core. A Pokemon in the air or underground (Fly, Dig...)
disappears and comes back as upstream's SemiInvulnerableTag makes it do. A Substitute doll stands in front of its
Pokemon, which steps aside and fades (PokemonAnimPhase's add, pre-move, post-move and remove animations). POKEMON,
a faint and a catch that needs a release open the menus thread's party screen (`menus/`) on the bottom screen;
BALL opens the Poke Ball list; a new move with four known opens menus' learn-move dialog and summary page.

The app opens on the **title screen** (Continue, New Game, Load Game) and a run is saved to the SD card between waves.
The flow is in [`app_flow.c`](source/app_flow.c): title, the five save slots (`slot_screen.c`: a slot's lead Pokemon, wave,
biome, score and money; a slot that can't be loaded says why and is never touched), the run (`run_screen.c`), and the
game over and clear screens (`result_screen.c`). A run is written with game-core's own `gc_run_save` by
[`run_saves.c`](source/run_saves.c) at each of game-core's save points (`gc_run_can_save`: after the wave's setup, before
the first turn), through `save.c` (temporary file with a checksum, renamed into place, the previous save kept as `.bak`).
It is never written during an animation: `run_screen.c` waits until the screen is idle and tries again at the next wave
if the card refuses. Game over and a win clear the slot, as upstream's `tryClearSession` does.

New Game goes through the **starter select** (`menus/starter.c`, hosted by `starter_host.c`): the species the dex says are caught,
a team of up to six within the Classic cost budget (10), Begin / Exit. What can be picked, what it costs and whether game-core
takes the team is game-core's own (`gc_starter_is_available`, `gc_starter_value`, `gc_starter_budget`, `gc_run_set_starters`),
glued in [`starter_data.c`](source/starter_data.c). The dex and the starter unlocks are game-core's `gc_system`, kept on the card
as `save/system.sav` by [`system_data.c`](source/system_data.c): read when the app first needs it, attached to each run, and
written with each save point of a run and when a run ends, only when it changed. A file that can't be read (damaged, or from a newer
version) is never overwritten: the player is told, starts with the defaults, and nothing is saved to it until it is removed. Backing
out of the starter select leaves the slot's old run as it was; it is cleared only once a team is confirmed. A developer's team in
`rundriver.txt` skips the screen and runs without system data, as before.

The end of a run has a **post-mortem** (custom content, task C03): the run driver hands the raw battle events of every drain to
[`combat_observer.c`](source/combat_observer.c), which feeds custom-core's bounded history reducer (the last 5 turns, 64 events each),
and the result screen's A opens [`postmortem_screen.c`](source/postmortem_screen.c): how the run ended and the player's Pokemon that fell
last on the top screen, the last turns' hits, heals, HP losses, faints and switches on the bottom (LEFT/RIGHT turn, UP/DOWN scroll, A
continue, B back). It says what the battle log showed and no more: **who dealt a hit, with what move, and why HP was lost are not in the
events and are never guessed**, so the screen says "source not recorded". A turn that lost events says so and its totals read "at least". The
history is held in memory for this session only; nothing is saved and no Nemesis is created (C04 and later).

Stand-ins in the run flow (listed in [`run_screen.h`](include/run_screen.h)): the starters of a run with no starter select (the host
tests' later runs) are a random team, reward names are the item key in words, evolution is automatic, a
mystery encounter is a message and its first usable option. The words on the game over and clear screens, and the
slot list's messages ("The save is damaged...", "Made by a newer version of the game.") are plain English, not upstream's
strings. Seeing the ending screens without playing 200 waves: `game/tests/mkfixture.c` writes a run one turn from the end.

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
| [`battle_events.h`](include/battle_events.h) | battle-core's turn events as battle screen events, with their text rendered from data.pak (`pr_msg_format`). Catches up at the end of a turn on anything battle-core doesn't emit yet (HP, faints), while the battle goes on. |
| [`battle_anim.h`](include/battle_anim.h) | Upstream's battle animation player (`BattleAnim` in `battle-anims.ts`): move, common and charge animations from data.pak, their graphic cells, Pokemon copies, backgrounds, sounds, layers. `ba_preload_move` at battle start. |
| [`party_screen.h`](include/party_screen.h) | `menus/`' party and summary screens as an overlay on the bottom screen, filled from battle-core's party: `ps_open(bt, PS_SWITCH or PS_FAINT_SWITCH, on_pick)`. |
| [`ball_menu.h`](include/ball_menu.h) | Upstream's `BallUiHandler`: the Poke Ball list with counts, `ball_menu_open(counts, on_pick)`. |
| [`run_screen.h`](include/run_screen.h) | The run driver: owns the `gc_run`, turns its events into battle screen events and its screens (command, switch, learn a move, evolve, rewards, biome, game over) into the menus the game has, and gives each answer to `gc_run_choose`. `rs_start(&cfg)` boots a run. |
| [`combat_observer.h`](include/combat_observer.h) | The custom content's view of a run: `co_observe` takes the raw WAVE / TURN / BATTLE events of one drain (once, from `run_screen.c`'s `drain()`), gives each battle range to the reducer as a batch, and `co_finish` captures a report that owns its data. Never changes the run. |
| [`postmortem_screen.h`](include/postmortem_screen.h) | The post-mortem screen over that report, opened from the result screen. |
| [`gen/gm_enum_names.h`](gen/gm_enum_names.h) | Names of battle-core's enums (`gm_enum_camel(GM_E_SPECIES_ID, id, ...)` gives upstream's i18n key). Regenerate with `python3 game/tools/gen_enum_names.py` when battle-core's enums change. |

New screens go in their own source files; `menus/` (party, summary, starter select, reward
select, learn move, biome map) is picked up by the Makefile when it exists.

## PC check
`game/tests/` builds on the PC against the cores. **The run driver** (`run_screen.c` and the converter) plays whole
runs against a fake battle screen and menus that check every screen event against a model of what the real screen
would show:
```
make -C game/tests runtest && game/tests/build/runtest 1000      # runs, then optionally the first run number
RUNTEST_VERBOSE=1 game/tests/build/runtest 200                    # each violation with its trail of screen events
```
It exits 1 on a violation, a Pokemon the converter could not find, a screen that doesn't match the run at a command,
a stuck driver or a run that stops making progress. It does not show how anything looks.
`RUNTEST_SYS=1` (or `all`: every starter unlocked) gives the runs system data, as the app does, and with `RUNTEST_SAVE` every save
point serializes it and loads it back.

**The starter select and the system data** have host tests of their own (`make -C game/tests <name> && game/tests/build/<name>`):
`startertest` (the model: every button, the cursor, the team and the cost), `starter_data_test` (the system file on the card, and the
list, panel and team against game-core's own functions, with a fuzz of random systems), `starter_ui_test` (the screen over a fake
drawing layer, with the host that hands the team to the app), and `flowtest` (New Game, Continue and Load around it). CI does not
run them; the 3DS build is the only check CI makes of `game/`.

**The combat observer and the post-mortem** (custom content, C03): `runtest` also checks, at the end of every run, that the combat report is
there, matches the run (outcome, wave, biome), holds a history custom-core accepts, and that no real event was refused, dropped, lost or
counted twice (its battle ranges must equal the ones the screen played). Their own tests, on literal fixtures:
```
make -C game/tests observer_test && game/tests/build/observer_test        # the observer + reducer: drains, switches, faints, overflow, replay/skip...
make -C game/tests postmortem_test && game/tests/build/postmortem_test    # the screen's words, pages, buttons, and the result screen's flow into it
python3 game/tests/observer_mutations.py                                  # one-line mutations of the wiring and the screen; every one must make a test fail
```
`flowtest` checks that the result screen is given the report of the run that ended (and none after a new run starts).

The single-turn converter tests use `tests/battle_setup.c`, a test-only fixture that builds a battle-core battle by hand
(the app no longer has it). `game/tests/` prints each turn's events with their English text (needs upstream's
`locales/en`):
```
make -C game/tests && game/tests/build/evtest path/to/pokerogue/locales/en 2
```

Switching through the fixture battle (a switch turn, then faint switches until the party is
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
- The post-mortem (C03) does not know who finished the run: no event names an attacker or a move, and battle-core is not changed to say so. It
  does not save anything (C04), creates no Nemesis, and needs the 3DS or Azahar with the real packs to be seen: its layout is checked on the
  host with a fake font 6 px wide per character, not on screen. Pokemon are named by species and form only (stable ids are not derived).
- The real evolution screen (evolution is automatic), the Pokedex, eggs, music; mystery encounter screens.
- The starter select is the basic legal team only. Missing from it (each a task of its own, none faked): the filter bar and sort, the
  cycle buttons (shiny, form, gender, ability, nature, tera), IVs, moves, nature management, candy purchases, favourites, rename,
  the Pokedex page, egg moves, Pokerus, the tutorial, challenges, Daily and Endless. UP from the first row wraps (upstream goes to the
  filter bar). A caught species that game-core refuses with its defaults (a prevolution reached through a form it can't have: Litleo
  after a female Pyroar) is not listed; see `starter_data.h`. Not measured: what writing `system.sav` costs on a real SD card.
- Runs started from the starter select now carry system data, so a run updates the dex, candy, friendship and Classic win records
  (game-core's `gc_system_*` calls in the run). Runs already saved are not back-filled into the dex.
- Around the save slots (not part of card 113): "Save and Quit" and "Rename Run" in the menu, the run history, unlocks and vouchers on a win, the rival's end dialogue, timed events, and any online or cloud save.
- Message timing per text (upstream's 500 ms for "X used Y!", prompts after faints): asked of
  battle-core; every text waits 1.5 s for now.
- In move animations: the grey part of a sprite's tone (upstream's shader pushes it far past
  grey; not reproduced), hitting a Substitute doll (animations aimed at the doll, `onSubstitute`),
  and the KO colour inversion.
- Catching and fleeing are ported in battle-core (Task 5 / Task 6) and wired in game-core's run loop (`GC_CH_BALL` in PR #55, Task 94 / PR #124 catch audit, `GC_CH_RUN` in Task 77); catching animation and full-party release flow in the app are ported (Card 114 / PR #135); reward, shop, and biome map screens are driven by the run (Card 112 / PR #129); the app's `run_screen.c` handles ball choices and run commands.
- Trainer battles, doubles, stat stage and shiny effects, boss HP
  segments, weather and field effects on the field.
- Sound without snd.pak is silent (the packs in the test rig have no sound yet).
- Forms, variants and gender sprite differences in sprite keys.
- Every other screen in the beta plan that is not listed above as done.
