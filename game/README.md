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

The app opens on the **title screen** (Continue, New Game, Load Game, Run History) and a run is saved to the SD card between waves.
The flow is in [`app_flow.c`](source/app_flow.c): title, the five save slots (`slot_screen.c`: a slot's lead Pokemon, wave,
biome, score and money; a slot that can't be loaded says why and is never touched), the run (`run_screen.c`), and the
game over and clear screens (`result_screen.c`). A run is written with game-core's own `gc_run_save` by
[`run_saves.c`](source/run_saves.c) at each of game-core's save points (`gc_run_can_save`: after the wave's setup, before
the first turn), through `save.c` (temporary file with a checksum, renamed into place, the previous save kept as `.bak`).
It is never written during an animation: `run_screen.c` waits until the screen is idle and tries again at the next wave
if the card refuses. Game over and a win clear the slot, as upstream's `tryClearSession` does.

START opens the **in-run menu** ([`run_menu.c`](source/run_menu.c), upstream's `MenuUiHandler`) on the command menu and on the reward screen,
over the bottom screen: the option window along the right edge and the message box at the bottom left, as upstream lays them out. It lists
**Save and Quit** only: upstream's other entries (settings, achievements, stats, egg list, Pokedex, manage data, community, log out) have no screen
here or are online features. Past the battle's first turn it asks the upstream warning ("You will lose any progress since the beginning of the
battle. Proceed?") and Yes / No first; Yes fades to the title. Upstream's `saveAll(true, true, true, true)` writes the *cached* session and system
data, which is what the battle's start wrote, so nothing new is written here either: the slot already holds the battle's start (Continue loads it),
and what the battle changed in memory and never wrote (a catch, the dex) is dropped: the system data is read from the card again (`app_flow.c`,
`on_run_quit`). If the last save of this battle was refused by the SD card the menu says so in its own words (not upstream's) and asks even in the
first turn. A custom run quits the same way (`cr_end`; its slot keeps its overlay).

**Rename Run** is the second entry of the Load Game list's slot menu (Load Game / Rename Run / Delete Run / Cancel; the custom list has it
too). Upstream types the name into a browser text input; here [`name_entry.c`](source/name_entry.c) is a keyboard of its own for the D-pad: the
form on the top screen (title, "Run Name", the text in a box, a count) and the keys on the bottom one (digits, letters, a few marks, the accented
vowels, Shift / Space / Delete, and the Rename and Cancel buttons). A types a key, B deletes (and cancels when nothing is typed), START renames, L/R is
Shift (the next letter, or all), X is a space, Y deletes. 20 characters at most (upstream's `maxLength`), trimmed, and an empty name changes nothing,
as upstream's `renameSession`. Upstream keeps the name in the session data; here it is a file of its own beside the slot's (`session_<n>_name`,
`run_saves.c`), keyed by a hash of the run's seed so a name never shows for another run, and removed with the slot (`rsv_clear`, `cr_clear`).
game-core's run save is not touched. A named run shows its name as the first line of its window on the list (the biome line makes room: the
top screen's backdrop shows the biome); a run nobody named shows the lines it always did. Differences from upstream: upstream gives every run a
default name (the mode and the number of runs played), this app does not (it keeps no count of runs), and upstream's `renameSession` bumps the save's
timestamp, which this app does not keep. A name file the card refused to remove is shown only for a run with the same seed (the case where it would be wrong: the same seed twice in one slot).

**Run History** is the entry after Load Game on the title menu (upstream: the menu's "Run History", `RunHistoryUiHandler` and `RunInfoUiHandler`;
[`history_screen.c`](source/history_screen.c)). A run that ends (a game over or a win; Save and Quit does not end one) adds an entry to
[`run_history.c`](source/run_history.c), the file `run_history` on the card (the app's own byte format with a magic and a version, CRC and
backup through `save.h`; 25 entries at most, adding to a full history drops the oldest, as upstream's `RUN_HISTORY_LIMIT`). An entry keeps what
the two screens show and none of the run's state: result, mode, wave, biome, score, money, when it ended, the run's name (Rename Run), the party
as it ended with what the Summary page needs (moves, ability, nature, stats, ivs, luck), and for a defeat what beat the run (the wild Pokemon, or
the trainer's name as game-core hands it out, `history_record.c`). The list is on the bottom screen (four runs in view, newest first: result,
mode and wave, the time, the party's icons with their levels) and the top screen shows the highlighted run in full (`run_summary.c`); A opens the
run's **info page** (the result, the trainer that won, mode, money and luck, the six party rows on the top screen; the highlighted Pokemon's
stats and moves on the bottom one, UP / DOWN picks the Pokemon), and A there opens the **Summary** page (the same `menus/summary.c` the party
screen hosts), B goes back. An empty history says "Empty". A history file that does not decode is never overwritten: the screen says it can't be
read and a run that ends meanwhile says it was not added (`on_run_end`). Differences from upstream: no browser session data (upstream keeps the
whole session of every finished run and rebuilds the Pokemon from it), so the entry has no run length, no player and held items, no Hall of Fame or
ending art, no trainer sprite on the info page and no personal-best tint; the time is UTC (the 3DS app keeps no time zone). Not covered by an
upstream differential (the screens are 3DS-only); `game/tests` has `run_history_test` (the file: round trip, fuzz, limit, unreadable, refused write,
the record built from a run) and `history_screen_test` (the screens, with the drawing replaced by fakes).

**What a Classic win gives** (upstream: `GameOverPhase.handleGameOver`, `awardFirstClassicCompletion`, `AchvBar`; [`win_gains.c`](source/win_gains.c),
[`toast_view.c`](menus/toast_view.c), [`result_screen.c`](source/result_screen.c)). game-core's `gc_classic_finish` already writes the win into the
system data (the Classic win achievement and its Golden voucher, Undefeated's unlocks, the win counts, the Plus voucher of a first win by a species,
the ribbon achievements, the Premium voucher of a later win); the app only did not tell the player. `run_config` takes a copy of the fields a win
changes when a run starts (`wg_snapshot`), and when a Classic run is won `on_run_end` takes the difference (`wg_collect`) and hands it to the
result screen. In upstream's order: the **achievement bars** on the top screen (the Classic win, the Golden voucher it unlocks, the unevolved-win
achievement, the ribbon milestones; one at a time, 500 ms in from the right edge, in place until 10 s after they began, 500 ms out, the achievement
sound at each; the art is upstream's `achv_bar` 9-slice, tier by score), the end card's line (A), then **one message per A** with the level-up
fanfare: what was unlocked (Endless Mode, Spliced Endless Mode, Mini Black Hole, Eviolite: "... has been unlocked."), each species that won Classic
for the first time ("X beat Classic Mode for the first time! You received 1x Egg Voucher Plus!"; the species and its starter's root, once each) and
the Premium voucher of a win that is not the first. Then the post-mortem or the title, as before. A game over is told none of it, and the
post-mortem's B comes back to the line without telling it again. The words are upstream's locale strings (`achv`, `battle`, `modifierType`, `voucher`);
the bar is at its art's own size with the 8 px pixel font (upstream draws it half size on a canvas six times larger than this screen).
Not here yet: the result screens' upstream strings (the line under the end card is still the stand-in). Sounds could not be checked in the emulator's screenshots: the code plays `se/achv` with each bar and `se/level_up_fanfare` with each
message, as the phases do. `game/tests`: `win_gains_test` (the real game-core: first win, second win by the same and by another
species, an evolved line, a fused Pikachu, duplicates, the ribbon milestones, an Endless or lost run, and invariants against the system data),
`result_gains_test` (the pages, the bars' easing and timing, the sounds, the box, the post-mortem's B) and `flowtest` (the app hands the
difference to the result screen).

**Trainer dialogue** (upstream: `EncounterPhase`, `TrainerVictoryPhase`, `CharSprite`, `showFieldOverlay`): `run_screen.c` turns `GC_EV_DIALOGUE` into `BEV_DIALOGUE`
(text from the locale key, speaker from the trainer's name, a portrait atlas only for the rivals). Before a fight the intro slides in with no message
(`BEV_INTRO_WILD` b=1), then the field darkens (0x424242, alpha .5, 500 ms), the portrait slides in (750 ms), the pages play with the name box at (12, -55)
on whichever screen shows the message, the portrait goes out and the field clears (250 ms), and then "X would like to battle!" follows. After a win the
defeated trainer slides back onto the platform (`BEV_TRAINER_SHOW`, 750 ms) and says its line. `@c{expression}` markers drive the portrait. Not done: the
"skip seen dialogue" setting. `runtest` checks the order of these events on every trainer wave; `trainer_render_test` the slide back.

**The rival's ending** (upstream: `GameOverPhase.handleGameOver`, `CharSprite`, `MessageUiHandler.showText`, `BattleMessageUiHandler.showNameText`;
[`char_sprite.c`](source/char_sprite.c), [`dialogue_view.c`](menus/dialogue_view.c), [`result_screen.c`](source/result_screen.c)). A Classic win
(`result_args.ending`, set by `on_run_end`) opens on the arena the last wave was fought in, in the black: the screen fades in over 500 ms, the rival
(images/character/rival_f, Ivy, as the player here is the male one, so `miscDialogue:ending`) slides in from the right edge of the field over 750 ms,
and her seven pages come one per A in the bottom screen's message box under a name box ("Ivy", upstream's `namebox` art); her expression changes
where the text says `@c{...}` (the new one fades in over the old in 250 ms); after the last A the screen fades out in 500 ms, she leaves under the
black (750 ms) and the end card fades in over 1 s before its line, as in EndCardPhase. The message box now understands `@c{variant}` (handed over
once with `msgbox_take_variant`), and a marker at the very start of a page fires on the timer's first tick, which shows no character, as upstream's
does (this also moves the first character of every message 20 ms later, as upstream). Differences from upstream: the dialogue text and name box are
on the bottom screen and the portrait on the top one (upstream has them in one field over the message window), the arena is drawn without its
battle zoom, the 10 s fade of the previous scene and the music fade are not done, no "skip seen dialogue" setting, and a female player's rival
(rival_m, `ending_female`) and the Endless ending ("Devs") are not built, as the app has no player gender and no Endless victory yet. `game/tests`:
`char_sprite_test` (the slide in and out, the expression fade, a second change in the middle of one, a missing atlas), `msgbox_test` (the real message
box: typing, pages, `@c` / `@d` / `@s` at the start and in the middle) and `result_gains_test` (the stages: fade, slide, pages, expressions, name box,
fade out, hide, end card fade, the line; no dialogue without text or on a defeat; the post-mortem's B comes back without her).

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
history is no longer lost when the app closes (C04): [`custom_store.c`](source/custom_store.c) keeps the last run's report with its
last turns, the id counters and the transaction ledger in one file, `save/custom.sav`, written once at the start of a run (the run's
durable uid) and once at its end (the report; before the run's slot is cleared). A file that can't be used (damaged, or from another
version) is never overwritten or removed: the result screen says why and nothing is stored until the player removes it. An ordinary run stores
the report with an unknown cause and inserts no Nemesis; a Custom Run (C10, below) can name the finishing enemy
(contracts sections 2.10 and 2.16).

The title screen has a **Nemesis Radar** (custom content, task C06): [`radar_screen.c`](source/radar_screen.c) lists the remembered enemies
of `custom.sav`, newest first, with a free Scout page for each (what the record remembers, "not recorded" for the rest). It is a read-only
look: it writes nothing, starts no rematch, spends nothing and says that rematches are offered during a Custom Run (C10). Only a custom
run creates records, so a fresh install shows an empty roster; [`mkcustom.c`](tests/mkcustom.c) writes a file with
records for the emulator (contracts section 2.12). Buttons only: the framework has no touch path for these screens.

There is a **capsule machine preview** (custom content, task C07): [`capsule_screen.c`](source/capsule_screen.c) over the pure
[`capsule_model.c`](source/capsule_model.c) plays a dual-screen capsule drop (top: up to 25 capsules fall from a chute, bounce, settle
and open; bottom: the knob, then the list of results) over a *fixed list of results it is given*. It is a debug / test preview, not a
purchase: normal play never opens it, it touches no wallet, profile, unlock or random number, and it is not connected to the gacha
until C09. Put `capsule=<1..25>` in `rundriver.txt` to open it before the title with that many fixed results. A pulls the knob, B or
START skips to the results, UP/DOWN scroll the list, A / B / START close it; a held button acts once (`ui_input_repeating`). No touch
(same reason as the Radar). Contracts section 2.13.

There is a **Capsule Machine** on the title (custom content, task C09; upstream reaches its gacha from the in-run menu, which only has Save and Quit so
far): [`gacha_screen.c`](source/gacha_screen.c) shows the three machines (LEFT/RIGHT) and their five options (UP/DOWN) with the real voucher
balances and egg count; an option the source refuses is greyed with the source's own words. A asks, the question opens on **No**, Yes buys. The
cost, the eggs given and the capacity are read from game-core by [`gacha_host.c`](source/gacha_host.c) (it pulls once per option on a scratch
profile), never from the labels. One accepted pull is one `profile_tx` transaction; the capsule screen (C07) then plays the committed result in
the source's display order and records it as shown. Opening the machine first finishes a purchase a restart left unfinished and shows eggs a
restart left unshown, once; neither pulls again. The result is eggs only: no species is marked caught, nothing hatches. A held button acts once;
no touch (same reason as the Radar). Contracts section 2.15.

There are **Custom Run** and **Custom Load** on the title (custom content, task C10): a Classic run with the optional hooks of game-core's
[`gc/custom.h`](../game-core/include/gc/custom.h) installed and the controller [`custom_run.c`](source/custom_run.c) behind them, saved in its own slot list
(`csession_<n>.sav`: the run bytes and the run's custom state in one file, never readable as an ordinary session). At the windows of the frozen
schedule (waves 27, 77, 127 and 177) it may ask, before any combat, whether to fight a remembered Nemesis; Yes builds that enemy through game-core's
own enemy construction at the wave's ordinary level (no scaling, boosts, boss segments or change to what the player carries), No gives the ordinary
encounter. A win pays one regular voucher, once. A verified wild finisher from a double battle can be remembered on its own; only an ordinary single wild encounter can be offered; the defeat that makes a Nemesis is named only
when battle-core's own state names the attacker. An ordinary run never installs a hook. If `custom.sav` cannot be used the custom entries say so and start
nothing. Contracts section 2.16.

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
for sound effects, `bgm.pak` for music) in `sdmc:/3ds/pokerogue/`, built with [`assets-pipeline/`](../assets-pipeline/). Without
them it shows a message saying so.

In the emulator rig ([`tools/azahar-libretro/`](../tools/azahar-libretro/)), put the packs in
`DIR/Azahar/sdmc/3ds/pokerogue/` and run
`lr-run azahar_libretro.so game/pokerogue.3dsx DIR frames=700 shot=620:out.png press=640:a`.

Controls: upstream's gamepad layout by button position. A = ACTION, B = CANCEL, START = MENU,
SELECT = STATS, Y = CYCLE_NATURE, X = CYCLE_TERA, L = CYCLE_FORM, R = CYCLE_SHINY,
ZL = CYCLE_GENDER, ZR = CYCLE_ABILITY. START + SELECT together quits.

## Screens
Upstream draws everything on one 320x180 canvas. Here:
- **Top screen (400x240)**: the battle field at 1:1, centred (the field and its info boxes), and upstream's message / command / fight strip over its last 48 rows (`draw_bottom(true)`, drawn through `UI_TOP_CANVAS_UI`, where upstream puts it; it covers the lower part of the player's Pokemon as upstream's does).
- **Window style**: `GM_DEFAULT_WINDOW_STYLE` (`ui_widgets.h`) is 5, the grey windows of the reference screenshots (lorenzo, 2026-10-12); upstream's own default is 1 (red). `main.c` hands it to the menus (`mui_set_window_style`), so every window uses the same style. Change the one constant to go back.
- **Bottom screen (320x240)**: upstream's `ui` container, bottom-aligned, so the message and
  command strip (upstream y -48..0) is the last 48 rows. Full-screen menus (party, summary,
  starter select...) use `UI_BOTTOM_CANVAS`: a whole 320x180 canvas, centred.

Every position in the code is upstream's own number in one of these spaces (see `ui_space` in
[`include/ui.h`](include/ui.h)), so layouts can be copied from upstream's handlers directly.

## Music
The app plays upstream's music from `bgm.pak` (built from the player's own copy of the assets, never in Git). Without the pack, or
without the DSP firmware (`sdmc:/3ds/dspfirm.cdc` on a console), it is silent and nothing else changes. Azahar's HLE audio also wants
that file to exist; any bytes will do for the emulator.

- **Which track**: `source/bgm_choose.c`, pure, so the host test can check every answer. Ported from upstream (`8555c08`):
  `Battle.getBgmOverride` (trainers' encounter and battle music, the Classic end waves, legendary / sub-legendary / mythical / Paradox wild
  Pokemon, wave 1-4's `battle_wild`, the final boss), the biome's music (`Arena.bgm`) and the title date rule
  (`winter_title` from December to January 15). The trainer table (`gen/gm_bgm_data.c`) is generated from upstream's `trainerConfigs`
  by `tools/dump-bgm.test.ts` (`tools/redump-bgm.sh <upstream dir>`).
- **Playing it**: `source/bgm_player.c` (`bgm_play` does nothing for the track already playing, as upstream's `playBgm`; a track the
  pack lacks leaves silence; `bgm_fade_out`) over `snd.c`; the loop point is the pack's, which is upstream's.
- **When**: `source/bgm.c`, called where upstream changes the music: the title and starter select screens as they come up, a new wave at
  black (the old music goes down with the screen when the track changes), a trainer's first Pokemon sent out (encounter music gives way
  to battle music), the trainer's defeat (its victory music) and the end of a run (a fade of 10 s after a win, 5 s after a loss). A
  track starts with a few SD card reads on the main thread, so it is only ever started at black or between two screen events, never
  while an animation plays.
- **Checked**: `make -C game/tests bgmtest && game/tests/build/bgmtest [BGM_TRACKS=<one track name per line>]` compares the chooser with
  what upstream's own `getBgmOverride` answered for every wave, species and form, trainer and variant (`tests/bgm_expected.c`,
  generated), then runs the player and the scene calls over a fake DSP (`tests/snd_fake.c`). `runtest` runs the real scene calls in
  every run and fails when a scene starts with other music than the chooser gives, a command is asked with the wrong music playing, a
  trainer's reward screen is not under its victory music, or a track starts while a battle animation plays.

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
| [`char_sprite.h`](include/char_sprite.h), [`dialogue_view.h`](include/dialogue_view.h) | The character portrait that slides in while someone talks (`chs_show` / `chs_set_variant` / `chs_hide`, upstream's `CharSprite`) and the dialogue's name box (`dv_namebox`); used by the rival's ending on the result screen. |
| [`win_gains.h`](include/win_gains.h), [`toast_view.h`](include/toast_view.h) | What a Classic win gave (`wg_snapshot` at a run's start, `wg_collect` at its end, `wg_toast_of` / `wg_message_of` for the words) and upstream's achievement bar drawn on the top screen. |
| [`radar_screen.h`](include/radar_screen.h) | The Nemesis Radar (C06): `radar_open` copies a roster and an optional schedule and shows the list and Scout pages. Read-only. `radar_assets.h` is its bridge to the menus layer (species types, icons, sprites, move and nature names). |
| [`capsule_model.h`](include/capsule_model.h), [`capsule_screen.h`](include/capsule_screen.h) | The capsule machine preview (C07): `capsule_model` is the pure motion and timeline (fixed 8 ms steps, dt in ms, clamped; settled by 1.5 s at the latest); `capsule_screen_open` plays it over an immutable `cap_fixture` and calls `on_settled` once and `on_exit` once from the player's input. |
| [`gacha_host.h`](include/gacha_host.h), [`gacha_screen.h`](include/gacha_screen.h) | The capsule machine (C09): `gacha_host` reads the option table from game-core, decides what can be bought, runs ONE purchase through `profile_tx` (busy while another is unfinished or unshown), builds the capsule fixture in the source's display order and records eggs as shown; `gacha_screen_open` is the menu (machine, option, question defaulting to No) and opens the capsule screen on the committed result. |
| [`profile_tx.h`](include/profile_tx.h) | The profile transaction owner (C08; C09 added the shown-eggs mark `ptx_unrevealed` / `ptx_mark_revealed`): a voucher-to-egg pull (`ptx_begin_pull` / `ptx_execute`) or a Nemesis reward claim (`ptx_begin_claim`) commits through ONE write-ahead envelope (`save/ptx.sav`: candidate profile bytes, gacha scene RNG, immutable result) and is rolled forward to `system.sav` and `custom.sav`; a retry or restart returns the same result and never pulls twice. `ptx_boot()` runs in `app_start`. |
| [`custom_store.h`](include/custom_store.h) | The custom content file on the SD card (`save/custom.sav`): `cs_begin_run` hands the observer a durable run uid (written first), `cs_commit` stores a finished run's report as one idempotent transaction, and a file it can't use is left exactly as it is. |
| [`custom_run.h`](include/custom_run.h) | The live rematch of a custom run (C10): `cr_begin` / `cr_load` / `cr_save` / `cr_clear` for the custom slot, `cr_on_screen` / `cr_on_run_end` for the run driver, the offer's species and message for the screen, `cr_settle_owed` at boot (a win whose voucher a crash left unclaimed). The hooks, the roster changes (each one `cs_txn`) and the voucher (`profile_tx`) are inside. |
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
drawing layer, with the host that hands the team to the app), and `flowtest` (New Game, Continue and Load around it).
CI runs all of them with `game/tests/run_host_tests.sh` (the `game-tests` job): the targets that need nothing outside the repo, and
`runtest` for 10000 runs and for 100 with system data and saves. Not in CI: `evtest` and `switchtest` (need upstream's `locales/en`),
`startertest` (compares against an upstream checkout), `animtest` and `anim_details_test` (need libjansson and upstream's animation
files), `mkfixture` (a tool) and `observer_mutations.py` (about 6 minutes).

**The combat observer and the post-mortem** (custom content, C03): `runtest` also checks, at the end of every run, that the combat report is
there, matches the run (outcome, wave, biome), holds a history custom-core accepts, and that no real event was refused, dropped, lost or
counted twice (its battle ranges must equal the ones the screen played). Their own tests, on literal fixtures:
```
make -C game/tests observer_test && game/tests/build/observer_test        # the observer + reducer: drains, switches, faints, overflow, replay/skip...
make -C game/tests postmortem_test && game/tests/build/postmortem_test    # the screen's words, pages, buttons, and the result screen's flow into it
make -C game/tests win_gains_test && game/tests/build/win_gains_test      # what a Classic win gave, over the real game-core
make -C game/tests result_gains_test && game/tests/build/result_gains_test # the result screen's bars, messages, sounds and the rival's ending
make -C game/tests char_sprite_test && game/tests/build/char_sprite_test    # the portrait's slide and expression fade
make -C game/tests msgbox_test && game/tests/build/msgbox_test              # the message box: typing, pages, @c / @d / @s markers
python3 game/tests/observer_mutations.py                                  # one-line mutations of the wiring and the screen; every one must make a test fail
```

**The custom store** (C04): `store_test` runs `custom_store.c` over the real `save.c` in a temporary folder (new install, restart, backup
fallback, every kind of unusable file left byte for byte as it was, a write that fails at each step of `save_write`, a replayed commit,
budgets); `flowtest` checks it through the app's flow (the file is created at a run's start, one report is stored at its end, the same
end twice stores nothing, a refusing card is told and retried once, an unusable file is reported). The pure codec and transaction are in
`custom-core` (`make -C custom-core test`).
```
make -C game/tests store_test && game/tests/build/store_test
```
`flowtest` checks that the result screen is given the report of the run that ended (and none after a new run starts).

**The live rematch** (C10): `custom_run_test` drives `custom_run.c` and game-core's real hooks with a mini driver over the real `save.c`, `custom_store.c`
and `profile_tx.c` in a temporary folder: real defeats that must create a Nemesis (and the ones that must not), the finisher gate, the four windows
and every way to skip them, accept / decline, the rematch matrix (won, lost, caught, fled; reload before and after the offer, accept, victory,
defeat and reward), the real enemy checked against the generated one and a control, five exact identity fixtures, saves (older, damaged,
mismatched, custom bytes as an ordinary session and the reverse) and a crash at every write of the reward. `runtest` has `RUNTEST_CUSTOM=1|strong`
(custom runs with offers and wins end to end through the real `run_screen.c`; `strong` makes the party win the rematch) and `RUNTEST_SAVE=resume`.
`flowtest` checks the title entries. `custom_run_mutations.py` removes the hook, the duplicate-grant guard, the gates and the reconcile rules one line
at a time; every mutation must make a test fail (the two documented exceptions are listed by the script).
```
make -C game/tests custom_run_test && game/tests/build/custom_run_test [extra seeds]
python3 game/tests/custom_run_mutations.py [mutation numbers]
RUNTEST_CUSTOM=strong game/tests/build/runtest 200 0
```

**The Nemesis Radar** (C06): `radar_test` runs `radar_screen.c` over custom-core's real validators with a fake drawing layer (every drawn
line must be on its screen, in a readable style, and free of words that offer a fight, an invasion or a purchase), plus a seeded fuzz of
button presses; `flowtest` checks the title entry (an unusable file is explained and left untouched, an absent file is not created,
a refused open is not silent, leaving returns to the title).
```
make -C game/tests radar_test && game/tests/build/radar_test [fuzz sessions]
python3 game/tests/radar_mutations.py     # one-line mutations of the screen and its hook; every one must make a test fail
make -C game/tests mkcustom && (cd some/folder && /path/to/build/mkcustom few|full|empty|junk|future)   # a custom.sav for the emulator
```

**The capsule preview** (C07): `capsule_test` runs the model and the screen with a fake drawing layer (every line on screen and
readable, no word that offers a purchase, unlock or currency, capsule rectangles inside the field) plus a seeded fuzz of buttons and
frame times; `flowtest` checks the debug entry (`capsule=7` opens the preview before the title, leaving it is the title, 26 / 0 / -3 are
ignored).
```
make -C game/tests capsule_test && game/tests/build/capsule_test [fuzz sessions]
python3 game/tests/capsule_mutations.py   # one-line mutations of the model, the screen and the debug entry; every one must make a test fail
```

**The profile transactions** (C08): `profile_tx_test` runs `profile_tx.c` over game-core's real gacha and the real `save.c` with a fault-injecting
layer under every file: a power cut or a failed write at every write of a pull, a claim and a refusal, and a seeded fuzz; after each, the card
holds exactly one coherent state and a retry ends in the clean run's result.
```
make -C game/tests profile_tx_test && game/tests/build/profile_tx_test [fuzz scenarios]
python3 game/tests/profile_tx_mutations.py   # one-line mutations of profile_tx.c and its hooks; every one must make a test fail
```

**The capsule machine** (C09): `gacha_host_test` checks the option table and the screen's "can be bought / refused with key" against game-core's own
answer over a grid of edge states, every machine x option against a direct `gc_system_gacha_pull` on the same inputs, refusals (they write nothing),
no second purchase, and a power cut and a failed write at every write of a purchase and of its shown-mark. `gacha_screen_test` drives the real menu and
the real capsule screen with buttons over a fake drawing layer (the question's default, held buttons, stale input, refusal rows, restarts at every
write, a seeded fuzz of buttons, frame times, held buttons, cuts and restarts). They share `gacha_test_common.h`.
```
make -C game/tests gacha_host_test gacha_screen_test && game/tests/build/gacha_host_test [states] && game/tests/build/gacha_screen_test [fuzz steps]
python3 game/tests/gacha_mutations.py   # one-line mutations of the machine; every one must make one of the two tests fail
```

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
- The post-mortem (C03) does not know who finished the run: no event names an attacker or a move, and battle-core is not changed to say so. Its
  report is stored (C04) but no Nemesis is ever created: that needs the finishing enemy's identity (C10). Its layout was seen in Azahar (Old 3DS mode), including a real faint and the "Last to fall" line (a scratch
  build only lowered the first Pokemon's HP); a clear and a list longer than ten rows were seen on screen only with scratch builds
  (a forced clear; a report of invented events). Pokemon are named by species and form only
  (stable ids are not derived).
- The real evolution screen (evolution is automatic), the Pokedex, eggs; mystery encounter screens.
- Music not done: the timed events' music replacements, the mystery encounters' own tracks (their encounters are stand-ins), the evolution,
  egg and heal fanfares (upstream plays them over the music; `snd.c` has one music stream), the Classic final boss's second stage (not in
  game-core), Daily / Endless / Challenge music, and a volume or music-preference setting (the app plays at full volume and follows
  upstream's default preference, all generations). What starting a track costs on a real console (the SD reads) is not measured.
- The starter select is the basic legal team only. Missing from it (each a task of its own, none faked): the
  cycle buttons (shiny, form, gender, ability, nature, tera), IVs, moves, nature management, candy purchases, favourites, rename,
  the Pokedex page, egg moves, Pokerus, the tutorial, challenges, Daily and Endless. UP from the first row wraps (upstream goes to the
  filter bar, which is a menu here: SELECT or the Filter button). A caught species that game-core refuses with its defaults (a prevolution reached through a form it can't have: Litleo
  after a female Pyroar) is not listed; see `starter_data.h`. Not measured: what writing `system.sav` costs on a real SD card.
- **Starter filter and sort** (`starter_filter.c`, upstream's `filterStarters` / `sortStarterSpecies`): SELECT or the Filter button above the list opens a menu with generation, type (a species fits on either type), caught (shiny tiers, normal), passive, cost reduction, won, hidden ability, egg affordable, Pokerus, the seven sorts and their order, Reset. Added beyond upstream at lorenzo's request: a rarity filter (ordinary, sub-legendary, legendary, mythical). Not built: Uncaught (the list is caught species only) and Favourite (no favourites). A change applies at once, puts the cursor on the first species and keeps the team (a hidden member keeps its cost and icon). A column cannot be emptied. The rules are tested in `starter_data_test`, the menu in `starter_ui_test`, the model's list swap in `startertest`. Not drawn like upstream's bar (drop-downs): a plain options list.
- Runs started from the starter select now carry system data, so a run updates the dex, candy, friendship and Classic win records
  (game-core's `gc_system_*` calls in the run). Runs already saved are not back-filled into the dex.
- Around the save slots (not part of card 113): timed events, and any online or cloud save. ("Save and Quit", "Rename Run", the run history and the unlocks, vouchers and ribbons of a win are done: see above.)
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

The ARM main thread reserves 512 KiB explicitly through libctru's startup stack
override. The real-command guard-page regression and built-ELF checks are described
in [STACK-CRASH-VALIDATION.md](STACK-CRASH-VALIDATION.md). Keep these checks when
changing startup or the Windows builder payload.
