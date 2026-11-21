# Nicknames in the app

Source: `8555c08c823b856cbec4eb99ca84ea52a955836d`. Locales: `270ed0a2938b2c3e2e34e7de940e84aecadf548b`. Needs battle-core's and game-core's nickname changes
(`pr_pokemon.nickname`, `gc_run_set_nickname`, `gc_nickname_valid`, and for the starter screen `gc_system_starter_nickname` / `gc_system_set_starter_nickname`).

Upstream's party screen has a **Rename** option (`PartyUiHandler.processRenameOption`): it opens a form pre-filled with `getNameToRender({useIllusion: false})`
and a submit sets `pokemon.nickname` (an empty submit clears it). This change gives the app that option and shows the nickname where upstream shows
`getNameToRender()`.

## What the app now does

- **Rename in the party screen.** The option is in the common options after Summary (upstream's order is Summary, Pokedex, Rename, then the rest; there is
  still no Pokedex screen). It is offered where `party_events.on_rename` is given: the battle's switch screen, the reward screens' Check Team and
  every `ps_open_*` of `party_screen.c`. Choosing it opens the `name_entry` keyboard over the party (`name_entry_args.overlay`, a `ui_push`; before it
  replaced the screen below), titled `menu:renamePokemon`, labelled `menu:nickname`, buttons `menu:rename` / `menu:cancel`, at most 20 characters
  (`PR_NICKNAME_MAX_CHARS`).
- **The form starts with the name the Pokemon shows now** (`name_entry_args.initial`): the nickname, else the species name or a fusion's blended name.
  `load_initial` keeps whole UTF-8 characters, at most `max_chars` of them, and stops at the first byte that is not part of a well-formed sequence.
- **Submit** calls `gc_run_set_nickname` on the right member: the reward screens list the run's party order (`party_order`), the battle's switch screen
  battle-core's order, and both are mapped to the member in `run->party[]`. An empty text clears the nickname. Cancel changes nothing. A text the run
  refuses (the keyboard cannot make one; an over-long initial text could) plays the error sound and changes nothing. The party lists the new name
  at once and works again (`party_resume`).
- **In a battle** the info box of a Pokemon on the field is renamed too (`bv_mon_of`): the battle view holds a copy of the name made when the Pokemon
  was sent out. A disguised Pokemon is left alone (its disguise's name stays).
- **Names shown.** `mon_display_name()` (fused_name.h) is `getNameToRender`: nickname, else species or fusion blend. Used by: the party list and
  summary (`menu_mon.name`, the Check Team, reward and switch lists), the battle view (`be_mon`: info boxes and switch-in),
  and the move-refusal text under the Fight menu. A disguise hides the nickname (the disguise's name shows) and the real Pokemon is named by its nickname
  again when the disguise breaks; a nickname stays through a form change. The ME member list shows it too.
- **Buffers.** A nickname is up to 80 bytes of UTF-8 in the Pokemon (60 in practice: 20 three-byte characters); `menu_mon.name` and `bv_mon.name` are
  84 bytes (`MENU_NAME_BYTES`, `BV_NAME_BYTES`; a `_Static_assert` ties the second to `PR_NICKNAME_BYTES`). Both were 40; every copy of them in `menus/` and
  `source/` that was smaller than 84 was checked (`party.c` slot name, `battle_view.c` info box name, `run_screen.c`). `bv_mon` is copied four times on
  the stack in `be_convert_events` (+176 bytes in that frame); the app's startup stack is at least 512 KB (`check_stack_size.py` enforces it), so the growth is not a stack
  risk. It was **not measured on ARM** (no devkitARM here).

## The starter select's Rename (kept across runs)

Upstream's starter select has a Rename in the options menu of a caught species: the form is filled with the **stored nickname** (empty when there is none;
not the species name, unlike the party's), a submit sets `starterPreferences[speciesId].nickname` (an empty one clears it), the panel shows it in the
species' place, it is saved with the system data when the screen is left, and `SelectStarterPhase` gives it to the Pokemon (`starterPokemon.nickname =
starter.nickname`). Here:

- **The row.** `starter_events.rename` (menus/starter.h) puts **Rename** in the options menu after Customize and before Cancel (upstream: after
  Favourites, before the candy purchases; this screen's Customize holds the candy purchases). The row is there only when the caller gives the event.
  Adding a row moved the menu's other rows up by 16 px (it is anchored to the bottom edge), so `test_touch` taps Add to Party at y 117 instead of 132.
- **The form** is `name_entry` as an overlay, the same words as the party's Rename, at most 20 characters, starting with the stored nickname.
  `starter_host.c` (`rename_starter`, `rename_done`) keeps it with the species in the profile (`ss_set_nickname`: only a caught species; the rest is
  `gc_system_set_starter_nickname`) and **writes the profile at once** (not when the screen is left, as upstream does): the settle-then-change-then-write pattern of the
  Customize lists, a write that fails puts the old nickname back and says "The profile could not be saved. Nothing was changed.". The same text is not
  written again. Cancel changes nothing.
- **Full.** The profile holds at most 128 nicknamed starters (game-core's `GC_STARTER_NICKNAMES`, a memory trade-off: upstream has no limit). A new nickname
  for a 129th starter plays the error sound and says "Only 128 starters can have a nickname at once. Clear one first."; replacing or clearing still works.
- **The panel** (`ss_describe`) names the starter by its nickname, on both screens. The bottom panel cuts a name of more than 15 UTF-16 units to 12 and "..."
  (`truncateString(name, 15)`, as it always did for names); upstream shows the whole nickname when the panel is drawn and cuts it only right after a rename, here
  it is always cut on the bottom screen (where the gender sign sits beside the name; the fit of 20 wide characters was not looked at on a console) and shown whole on the top screen. The cut counts
  UTF-16 units, not bytes (kana are 3 bytes, an emoji is 2 units) and never splits a character; it also stops cutting a localized species name that has
  more than 15 bytes but fewer than 16 characters.
- **The run.** `ss_starter` copies the stored nickname into `gc_starter.nickname`, so every team handed to `gc_run_set_starters` (the screen's Start, and the
  random team the soak runs use) carries it, and `gc_mon_new_player` gives it to the Pokemon. It comes back after a restart (`sysd_save` writes it, the
  profile loads it) and is cleared with an empty text.
- The filter's sort by name still sorts by the species' name.

## Not done

- Run history, the post-mortem and the radar still show species names (the history keeps species, not Pokemon; adding the nickname grows every history
  entry in the save).
- Messages that game-core stored as words before this change (Wish, Healing Wish, Lunar Dance) keep the species (battle-core's PR says so).
- Enemy nicknames (the Expert Pokemon Breeder's Cleffa): not in this change.
- Not run on a console or in Azahar (no Azahar screenshots were asked for); the 3DS build (`build-3ds`) only runs in CI.

## Checks

All commands from the repository root, from a clean `game/tests/build` (that Makefile does not track header changes, so a stale build can pass).

| Check | Command | Result |
| --- | --- | --- |
| Rename option, form text, member mapping, cancel / empty / bad text, battle info box, summary names (party_item_test) | `game/tests/run_host_tests.sh` | `party_item_test: ... Rename passed` |
| Text entry: the starting text (whole characters, the limit, malformed bytes), overlay, trimming | same | `name_entry_test`: 351 checks, 0 failed |
| Names the battle view is given: nickname, 80-byte nickname, emoji, fusion, disguise, disguise breaking, form change, `mon_display_name` into a small buffer | same | `fused_name_test`: 28 checks, 0 failures |
| The starter select's Rename (row only with the event, form text, set / replace / clear, cancel, same text not written, failed write undone, refusals, 128 full, long names in the panel, nickname in the run's starter) | same | `starter_ui_test`: 23,006 checks, 0 failed |
| The starter profile with the real system data file (written as a change, restart, same bytes, a run gets the nickname, clearing restores the plain file) | same | `starter_data_test`: 553,646 checks, 0 failed |
| All `game/` host tests | `rm -rf game/tests/build && game/tests/run_host_tests.sh` | 89 sections of output (72 test programs and 17 `runtest` runs), `game/tests: all passed`, exit 0 |
| Planted faults | `python3 game/tests/check_nickname_ui_faults.py` (after `make -C game-core build/libgc.a` and its other libraries; in CI) | 49 of 49 caught (see the list at the top of the script) |
| game-core and battle-core suites | `make -C game-core -k test`, the CI `test` job's history, proof and `check_mystery_ui_faults.py` steps | run by hand for the nickname game-core changes (#691, #693: exit 0; `nickname_host_test` 303 checks, 0 failures; `starter_nickname_host_test` 76,291 checks, 0 failed; 19 of 19 planted faults); the starter select change touches no game-core file, so the CI `test` job is the check for this head |

The planted faults cover: the option not offered or not handled, the wrong member (slot instead of the party order, battle order replaced by the run's),
cancel clearing the name, no error sound, the party not resumed or the entry not popped, the list not refreshed, the form starting empty or not as an
overlay, the info box not renamed, renamed for a disguise or for any Pokemon, the view ignoring the nickname, a disguise showing it, a broken disguise
not naming the real Pokemon (and from the wrong side), the three forms of the form-change rule, and the entry's starting text (not loaded, past the limit,
a torn character) and overlay.

A first version of the rename list test did not catch "list keeps the old name"; the test now draws the list after the rename and counts the new name.
