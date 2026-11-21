# Nicknames in the app

Source: `8555c08c823b856cbec4eb99ca84ea52a955836d`. Locales: `270ed0a2938b2c3e2e34e7de940e84aecadf548b`. Needs battle-core's and game-core's nickname changes
(`pr_pokemon.nickname`, `gc_run_set_nickname`, `gc_nickname_valid`).

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

## Not done

- Run history, the post-mortem and the radar still show species names (the history keeps species, not Pokemon; adding the nickname grows every history
  entry in the save).
- Messages that game-core stored as words before this change (Wish, Healing Wish, Lunar Dance) keep the species (battle-core's PR says so).
- Enemy nicknames (the Expert Pokemon Breeder's Cleffa) and the starter screen's Rename with per-species storage (upstream `StarterPreferences`):
  not in this change.
- Not run on a console or in Azahar (no Azahar screenshots were asked for); the 3DS build (`build-3ds`) only runs in CI.

## Checks

All commands from the repository root, from a clean `game/tests/build` (that Makefile does not track header changes, so a stale build can pass).

| Check | Command | Result |
| --- | --- | --- |
| Rename option, form text, member mapping, cancel / empty / bad text, battle info box, summary names (party_item_test) | `game/tests/run_host_tests.sh` | `party_item_test: ... Rename passed` |
| Text entry: the starting text (whole characters, the limit, malformed bytes), overlay, trimming | same | `name_entry_test`: 351 checks, 0 failed |
| Names the battle view is given: nickname, 80-byte nickname, emoji, fusion, disguise, disguise breaking, form change, `mon_display_name` into a small buffer | same | `fused_name_test`: 28 checks, 0 failures |
| All `game/` host tests | `game/tests/run_host_tests.sh` | 89 test programs plus `runtest`, `game/tests: all passed` |
| Planted faults | `python3 game/tests/check_nickname_ui_faults.py` (after `make -C game-core test`; in CI) | 25 of 25 caught (see the list at the top of the script) |
| game-core and battle-core suites on this head | `make -C game-core -k test`, the CI `test` job's history, proof and `check_mystery_ui_faults.py` steps | exit 0; `nickname_host_test` 303 checks, 0 failures; mystery UI 67 faults rejected |

The planted faults cover: the option not offered or not handled, the wrong member (slot instead of the party order, battle order replaced by the run's),
cancel clearing the name, no error sound, the party not resumed or the entry not popped, the list not refreshed, the form starting empty or not as an
overlay, the info box not renamed, renamed for a disguise or for any Pokemon, the view ignoring the nickname, a disguise showing it, a broken disguise
not naming the real Pokemon (and from the wrong side), the three forms of the form-change rule, and the entry's starting text (not loaded, past the limit,
a torn character) and overlay.

A first version of the rename list test did not catch "list keeps the old name"; the test now draws the list after the rename and counts the new name.
