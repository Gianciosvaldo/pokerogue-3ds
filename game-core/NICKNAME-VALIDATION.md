# Nicknames in game-core

Source: `8555c08c823b856cbec4eb99ca84ea52a955836d`. Locales: `270ed0a2938b2c3e2e34e7de940e84aecadf548b`. Needs battle-core's nickname change (`pr_pokemon.nickname`, `pr_arg_pokemon_plain_name`).

Upstream's `Pokemon.nickname` is set by the party screen's Rename (`PartyUiHandler.processRenameOption`) and by the starter select's Rename
(`SelectStarterPhase`: `starterPokemon.nickname = starter.nickname`), and it is part of the saved Pokemon. This change gives game-core what
the app needs to do the same; the Rename screens themselves are the app's change.

## What game-core now does

- `gc_nickname_valid(text)`: what a nickname may be. At most 20 UTF-16 code units (the rename form's input has `maxLength: 20`, which counts
  UTF-16 units: an emoji is two), valid UTF-8, no control characters (C0, DEL, C1). `NULL` and `""` are valid (none). Spaces are text: the form
  trims what it submits (`sanitizeInputs`), so the caller trims, and a nickname keeps the inner spaces it is given.
- `gc_run_set_nickname(run, idx, text)`: sets, replaces or (with `""` / `NULL`) clears the nickname of `party[idx]`; false and no change for a
  bad slot or text. Not an event: the names read the nickname, the app redraws.
- `gc_starter.nickname`: a starter's nickname reaches its Pokemon (`gc_mon_new_player`); `gc_run_set_starters` refuses the whole list, with a
  reason, if one of them is not valid.
- The run's save has a `nicknames` section (and a `darby_nicknames` section for D'Arby's sealed chips), written **only when some Pokemon has a
  nickname**. A run with none writes the bytes it wrote before this change, and a save from before it loads with none. The text is stored as
  plain UTF-8 (upstream stores it base64 encoded inside its JSON; the port's save is its own format).
- A nickname that is not valid text in a loaded save (cut short, not UTF-8, a control character, too long, no terminator) is dropped and
  counted in `load_dropped`; the Pokemon stays and the run loads. A nickname only names a Pokemon, so it is not worth refusing a run for. A save whose
  nickname section has the wrong shape is refused as damaged, like the other sections. The save's checksum is untouched.
- `evolutionDone` ("Your {{pokemonName}} evolved into {{evolvedPokemonName}}!") names the species for `evolvedPokemonName` (upstream:
  `this.pokemon.name`) and the nickname for `pokemonName` (`preEvolvedPokemonName` = `getPokemonNameWithAffix`). A nickname stays through an evolution, as upstream's.
- Every other message that names a party Pokemon already goes through `pr_arg_pokemon` / `pr_arg_pokemon_name`, so it shows the nickname once
  battle-core's change is in. Upstream's mystery encounter code has no `pokemon.name` use (checked by searching `data/mystery-encounters`), so
  its messages show the nickname too.
- `gc_run.internal_` grew 1 KB for the 88 bytes `pr_pokemon` gained (that is in battle-core's change).

## A starter's nickname across runs (the system profile)

Upstream's starter select has a Rename for a caught starter; the text is `starterPreferences[speciesId].nickname`, saved with the system data, and
`SelectStarterPhase` gives it to the Pokemon (`starterPokemon.nickname = starter.nickname`). This change gives the profile the storage; the app's Rename
and the use at run start are the next change.

- `gc_system.starter_nicknames[]` / `n_starter_nicknames`: a table sorted by species, one entry per species, each `{species, text[61]}` (a valid nickname
  is at most 60 bytes: 20 three-byte characters; `_Static_assert` against `PR_NICKNAME_MAX_CHARS`). The unused entries are all zero, so two profiles with
  the same nicknames are byte for byte the same however they got there (the tests compare whole profiles).
- `gc_system_starter_nickname(sys, species)` reads it ("" = none, also for a non-starter), `gc_system_set_starter_nickname(sys, species, text)` sets,
  replaces or (with `""` / `NULL`) clears it and answers `GC_STARTER_NICK_OK`, `_NOT_A_STARTER` (species not in `gc_starter_costs`), `_BAD_TEXT`
  (`gc_nickname_valid` says no) or `_FULL`. Nothing changes unless it answers OK.
- **Memory.** The table makes every `gc_system` 8,192 bytes bigger (`sizeof` 760,248 to 768,440; the 4-byte count sits in padding that was there). A slot for each of the 572 starters would cost 640 x 61 = 39 KB, and there are five
  copies of a profile in the app: `g_sys` (`system_data.c`), the static `next` in `gc_classic.c` and in `gc_gacha.c`, the static `fresh` in `gc_achv.c`,
  and a temporary `malloc` in `gacha_host.c`. So a table for every starter would add about 156 KB of static memory (and 39 KB transient) against about
  32 KB (and 8 KB) for this one. **The price is a limit upstream does not have:** a 129th starter cannot get a nickname until one is cleared. It is one
  constant (`GC_STARTER_NICKNAMES`). Neither figure was measured on a 3DS (no devkitARM here); they are `sizeof` and the count of copies.
- The save has a `starter_nicknames` section, written **only when a starter has a nickname**: a profile with none writes the bytes it wrote before
  (proved against `origin/main`, below), and a profile with nicknames loads in a build that does not know the section (checked once by hand against
  the base build: it skips the unknown section and reads the rest). A nickname in a loaded save that is not valid text, not a starter's, empty, a
  duplicate (the later record wins), unterminated or past the table is dropped and counted; the profile and the other nicknames load. The save's
  checksum is untouched.

## Not done

- **Enemy nicknames:** the one encounter that gives enemies a nickname, the Expert Pokemon Breeder's Cleffa (`EnemyPartyConfig.nickname`), now sets them
  (`gc_breeder.c`: "Ace", and in the Space biome "Clefablest" and "<species> the Great"); see `tests/mystery_breeder_diff_test.c`. No other upstream
  encounter names an enemy (checked by searching `data/mystery-encounters` for `nickname`).
- The app does not use the profile's starter nicknames yet (the starter select's Rename and applying one when a starter is picked are the app's change,
  after this one); until then only a starter's nickname handed to `gc_run_set_starters` is used.
- Not upstream's limits: upstream keeps a nickname for every starter it has preferences for; the profile here holds at most `GC_STARTER_NICKNAMES` (128)
  of the 572 starters (see below).
- Stricter than upstream in one respect: upstream's input takes any typed character. Control characters and invalid UTF-8 are refused here
  (the 3DS keyboard cannot type them, and a damaged save may hold them).
- A rename form's text that is longer than 20 units because it was pre-filled (upstream pre-fills the form with `getNameToRender()`, a fusion's blended
  name can be longer than 20): `gc_nickname_valid` refuses it, so the app has to cut it to 20 units first. Upstream stores it as submitted.

## Checks

| Check | Command | Result |
| --- | --- | --- |
| Text rules, setter, starters, save and load, none written when none exists, damaged nicknames, D'Arby's chips, a turn's message, evolution messages | `make -C game-core build/nickname_host_test && game-core/build/nickname_host_test` (in `make -C game-core test`, so in CI) | 303 checks, 0 failures |
| That test finds planted faults | `python3 game-core/tools/nickname-guards.py` (CI) | 17 of 17 caught (nickname dropped at the starter, emoji counted as one unit, C1 controls / overlong / surrogates / lone continuation bytes allowed, a tail left after a shorter name, bad text or slot taken, bad starter list taken, section always written, never written, never checked, a drop not counted, the chip section never written or checked, the evolved name showing the nickname) |
| A run with no nickname saves the bytes `origin/main` saves | `sh game-core/tools/save-bytes-diff.sh origin/main 200` (by hand: it needs the base ref) | 6,835 saves from 187 runs (13 starter lists refused by both), lengths and hashes identical; with the section forced on, it reports a difference |

| A starter's nickname in the profile: the table (sorted, one per species, no tail, nothing changes on a refusal, the full table), the save (no section when none, the nicknames and the rest of the profile back, byte for byte after a clear), damaged and hand-made sections (appended to a real save with the checksum recomputed: 200 records, a text field of another size, a cut block, a missing species, an unknown extra field) | `make -C game-core build/starter_nickname_host_test && game-core/build/starter_nickname_host_test` (in `make -C game-core test`, so in CI) | 76,291 checks, 0 failures (also under ASan + UBSan with `gc_system.c` and `gc_save.c` compiled with them) |
| That test finds planted faults | `python3 game-core/tools/starter-nickname-guards.py` (CI) | 19 of 19 caught (lookup skipping the match, a lookup that ignores the species, a non-starter or bad text taken, an empty text stored, a duplicate appended, a tail left, no room made, the count not kept on insert or clear, a full table taking one more, a gap or a slot left on clear, the section always or never written, the last nickname not saved, the section not loaded, an empty record kept, a bad record refusing the profile) |
| A profile with no nickname saves the bytes `origin/main` saves | the same `save-bytes-diff.sh` (it now also hashes the profile at every point a run is saved) | 6,835 profile saves next to the 6,835 run saves, identical; with the section forced on, it reports a difference |

The 17 faults leave two mutants out because no input tells them from the code: the byte-length limit in `gc_nickname_valid` and the terminator test in `check_nicknames` are implied by the 20-unit
limit (81 bytes of text are always more than 20 units); they only keep a read of damaged data inside the buffer.

Not covered: a nickname section that exists but has the wrong shape (the save's format is hash-keyed and checksummed, so it cannot be built by hand in a test; the code path is the one every other section uses).
