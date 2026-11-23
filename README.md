# PokeRogue for the Old 3DS

A native C port of [PokeRogue](https://github.com/pagefaultgames/pokerogue) for the original Nintendo 3DS, with offline SD-card saves and optional custom content. The app runs real battles and run progression, mystery encounters, Endless modes, a custom Classic (Bizarre) mode, and a separate Pokémon football mode.

**Battle-core is still being completed.** This overview describes merged code through `327a70f` (2026-11-19), not exhaustive upstream parity or acceptance on every console. Almost nothing listed below has been seen on a real console: each PR records what was tested on a host and what was not. An older Windows builder contains the revision recorded in its `files/VERSION.txt`, not necessarily everything below.

**Code only for Pokémon content:** Pokémon and PokeRogue art, music, fonts, logos and generated `.pak` files are built locally from pinned upstream sources for your own SD card. Do not commit or redistribute those assets. CI can provide the bare homebrew application; it still needs locally generated packs. The custom pictures and music for the added characters are files the project owner supplied (see [sd-builder](sd-builder/README.md)); they are not Pokémon or PokeRogue content, and their licence is the owner's to settle.

## Additions

The demo has become an integrated run app. Two complete change indexes record every first-parent commit: [2026-10-04 to 10-25](docs/CHANGES-SINCE-2026-10-04.md) (301 entries) and [2026-10-25 to 11-19](docs/CHANGES-SINCE-2026-10-25.md) (265 entries), including individual mechanics and fixes. The earlier READMEs are preserved as historical evidence: [2026-10-04](docs/history/README-2026-10-04.md) and [2026-10-25](docs/history/README-2026-10-25.md).

### Game modes

- The **Mode** row of the New Game settings offers **Classic**, **Classic (Bizarre)**, **Endless** and **Endless (Spliced)**. Bizarre is the custom-content mode and replaces the old “Custom: yes/no” row. Bizarre and the Endless modes unlock by winning Classic, as upstream gates Endless; profiles with a recorded Classic win are recognized. Saves from the old Custom row load as Bizarre.
- Endless covers the enemy buff phases, upstream’s starter rules (a 15-point budget, fusion for Spliced), clear rewards, the Eternatus item lock and learning/resume behavior at high waves. Native comparisons for the supported Endless waves are retained.
- The final boss now plays its message, speech and music and transforms into Eternamax for the double-battle second phase; Inverse Battle and Challenge-mode naming are honored.
- Gym Leaders, the Elite Four and Champions are restored in runs, with their Terastallization, abilities, forms and fixed moves; their teams are as strong as their round. The Town Youngster, evil-team grunts, admins and bosses are real fights with generator-accurate movesets. Some details are still missing; the PRs ([#509](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/509), [#510](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/510), [#520](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/520), [#536](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/536)) list them.

### Runs and presentation

- Title screen, Continue/New Game/Load Game, five save slots, run renaming, Save and Quit, run history, game-over and Classic-clear results, unlocks, vouchers and ribbons. Saves retain backups and reject invalid data; saving is restricted to supported run boundaries. A “Will you switch?” question at a wave’s start no longer drops the pending save when a run is loaded 3–5 waves back; waves with a mystery encounter are still not saved (documented gap).
- Doubles: separate commands, a target choice drawn on the field (flash, foe first, tappable tiles), a corrected foe platform and layout, and fixes for a lone player Pokémon at turn end. X/Y in battle cycle the bottom-screen view and the game speed.
- The player’s Terastallize command (button, icon in the info boxes, battle-sprite look); fused Pokémon (DNA Splicers) shown as both halves in battle and the party, with unsplice, candy and level-up handling; boss info box, HP-bar dividers and shields; stat-stage readouts that show what a stat really moved.
- Run info pages with larger text over the whole 400×240 top screen; animations, ME screens and dialogue portraits that use the full top screen; an upstream-style achievement bar on any screen.
- A **timed-event** panel on the title screen shows which events are on (see below).

An attempted “Black 2” redesign of the battle bottom screen is **not** part of the app: it was reverted and remains an unmerged draft ([PR #680](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/680)).

### Starters, eggs, shop and persistent progression

- Starter filtering and sorting, customization of known moves, natures, shiny tiers, abilities and passives; L/ZL cycle caught forms and genders; teams over 10 points up to a 15-point budget; a fix for starter sprites that vanished at random.
- Species candy, voucher-based egg acquisition, hatching with sprites and animation, a luck display and three scheduled Pokérus starters per three-hour period. Both Mega Stones of a species and the Kyurem/Calyrex form items appear in the shop, and the Lock Capsule’s Reroll works again with its lock on.
- Mystery-encounter selection follows upstream’s rules, including per-run caps and biome lists; a bug that let almost no encounter type come up is fixed ([PR #614](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/614)).

### Mystery encounters

All 31 upstream encounter types are reachable. Encounters ported with upstream comparison tests include Dark Deal, Fight or Flight, Slumbering Snorlax, Training Session, Berries Abound, Delibird-y, the Global Trade System, Mysterious Challengers, Mysterious Chest, The Strong Stuff, Fiery Fallout, Teleporting Hijinks, Uncommon Breed, Dancing Lessons, Absolute Avarice, Safari Zone, The Winstrate Challenge, A Trainer’s Test, Clowning Around, Weird Dream (transformation, alternate-team fight, backdrop), Bug-Type Superfan, The Expert Pokémon Breeder, Trash to Treasure, Fun and Games and the no-battle families (Department Store Sale, Shady Vitamin Dealer, Lost at Sea, Field Trip, Part Timer, An Offer You Can’t Refuse, the Pokémon Salesman). A real question screen draws each encounter’s intro, options, tooltips and party/move picks; check each PR for what was tested only on a host.

### Battle mechanics and fixes

Beyond the October work (doubles callbacks, held-item owners, Terastallization phases, long-history proofs, Macho Brace, Revival Blessing and more): permanent maximum HP across recoil/costs/healing/Substitute/Healing Wish owners; the 48 public arena-tag keys; Tailwind callbacks through Court Change; Eviolite clones; Shedinja’s extra-party evolution and shared IVs; full TM history beyond 64 moves; weighted wild and boss movesets, including fused bosses; fused-Pokémon ability, type, move and candy handling; Multi Lens stacking; Fell Stinger stat-stage display; and customPokemonData types in `getTypes`.

See the [battle-core README](battle-core/README.md), [completion checklist](battle-core/COMPLETION-CHECKLIST.md) and [deviation register](battle-core/DEVIATIONS.md). Encounter-end charge cancellation deliberately fixes user-reported behavior retained by the pinned upstream; its validation is recorded in [PR #409](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/409).

### Custom content

All of the following is the project owner’s own design, not upstream behavior, and is offered in Classic (Bizarre) runs unless noted.

- **Rematches and Nemeses.** Custom Run Legacy reports record the final blow and support persistent Nemeses, a radar, rematch scheduling and claims. Since the Bizarre mode, rematches and the Nemesis radar apply to plain Classic **and** Bizarre runs (not Endless). A rematch Nemesis cannot be fled from or caught.
- **Capsule machine.** Voucher-to-egg transactions with reveal presentation, touch interaction and durable transaction handling.
- **JoJo challengers.** 34 characters (the first 17 from October; Caesar to Giorno added 2026-11-15) are drawn into eligible trainer waves of Bizarre runs at a 30% chance, each at most once a run and at most one per wave, with their own intro (“A Bizarre Foe appears!”), lines, teams and music. Their pictures are drawn at 1:1 so sprites up to 99 px are not cut. Persisted state is a 64-bit dealt mask.
- **Stands and passives.** Pucci’s Made in Heaven (replacing a Speed Boost stand-in), DIO’s The World (time stops on one or two turns a battle, with its own sounds) and Kars’s Perfect Being (the last Pokémon nullifies physical damage and halves special).
- **Pillar Men** (Esidisi, Wamuu, Kars) encounter, with silhouette screens and its own theme.
- **D’Arby the Gambler** (Soul Poker; waves 40 to 160, once per run, needs four or more Pokémon): a deck-and-wager card game on a table with face-down cards, a lie-detector callout that appears only after a loss, an “All-In” that opens a Master and a Rogue reward screen, and the achievement “Gambler’s Downfall”. Accepting his deal seals the wagered Pokémon, with their held items, in his chips: they leave the party but are saved with the run. A wrong “You Cheated!” loses them for the run. D’Arby returns as a trainer rematch (17.5% per eligible single-trainer wave, certain on each from wave 165 to 172, never after 172) with Thievul, Klefki and Cofagrigus followed by the sealed Pokémon; beating him gives “Debt Paid in Full” and the chance to take each Pokémon back (at full health, no status; with six in the party it replaces a chosen member) or leave it for good ([PR #686](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/686)). That PR records no 3DS or emulator run, and its return screens are not a save point.
- **Jobin’s beetle duel** (Great tier; waves 20 to 140, once per run, any biome, Bizarre only, the player can decline): a gentleman’s bet on a drum, each side a beetle (Heracross, Pinsir, Scizor, Vikavolt, Golisopod, Orbeetle, Lokix, Rabsca, Kleavor, Ledian or Crustle; an own one, or a loaned one when none is on offer). Both sides have a Grip bar, shown like HP bars, and the player a Suspicion bar; Horn Lock, Dig In and Flank beat each other in a circle, the Stand Trick takes Grip at the price of Suspicion (caught at 100%: the bet is void and Jobin’s team fights, with half the prize money), and Jobin’s Speed King telegraphs a stronger attack the round before (sometimes a bluff). Rewards, penalties and the two achievements follow the owner’s spec; the lines are in `game/source/custom_texts.h`. The drum is the owner’s art; his Jobin and Speed King sprites are still to come, and the encounter falls back to his plain `jobin.png` where a pose is missing. **Not tried on a console.**
- **Achievements.** 156 custom achievements (JoJo “Defeat X”, a “Flawless X” for each of the 34 challengers, “Twenty Faces”, run/collection/egg groups, trainer, region and football achievements, D’Arby’s two, Jobin’s two) with an upstream-style bar and a list screen. Titles follow the owner’s own wording.
- **Timed events** (the owner’s own events, driven by the console clock, not upstream’s event calendar): Lucky Catch, Sweet Tooth, Shiny Day (every 3rd day), Voucher Plus Day (every 2nd day), Shiny Trainers (13:00–14:00), Golden Hour (20:00–21:00), Charm Hour, Lucky Species Hour, Candy Jar Day, Delibirdy Day, Fusion Day, Waiting Room and Snowy Town. The numbers come from upstream’s own event effects at the pin; the days and hours are the owner’s draft and have not been tuned on hardware.
- **Stand Arrow and Tonio’s Trattoria** (the other planned Bizarre-only encounters) are **not built**: they wait for the owner’s specs.

### Football

The title’s Football entry opens a separate live 22-player mode: a 55-point profile-based draft (a Pokémon per position), editable offense/defense lineups with pick-up-and-swap, movement, manual snaps, passing, running, tackling, coverage, 20 offensive plays and eight defensive schemes with audited coverage, kickoffs, punts, field goals, extra points, two-point tries and returns, and interceptions that stay live. The **Football Playoffs** are now thirteen matches (8 + 4 + 1) against the Kanto line-up with type teams and an opponent card; members gain friendship and ten levels per won round, evolve by their level (including species that never could), earn species candy for plays, and the shop sells evolution items, vitamins and Mega Stones; a won match gives an egg voucher (a Premium one for the title), and a finished playoff restarts by itself. Presentation includes a reworked stadium (turf, end zones, big side-on goal posts, benches, stands, crowd), route lines, sound effects and menus, and upbeat match music. It uses `football_live.sav`; START retains access to the older six-player mode and its `football.sav`. Ordinary run saves remain separate. See [live football controls and limits](football-core/LIVE.md).

## Validation and remaining work

The replication target is upstream game `8555c08c823b856cbec4eb99ca84ea52a955836d`, locales `270ed0a2938b2c3e2e34e7de940e84aecadf548b` and assets `87426a79611f9d212c4dc8af58e2834a05b93725`. This is not a claim of parity with today’s upstream beta. Recorded reference proofs use Node 22 where specified.

This README does not cite a passing CI run for the current head of `main`: the last full CI run recorded on `main` is [run 40233368164](https://github.com/Gianciosvaldo/pokerogue-3ds/actions/runs/40233368164) (2026-10-25), and later work was validated on its own PR heads (CI was blocked by the repository’s GitHub Actions spending limit from 2026-11-17). Each PR records the commands and counts it ran. Native upstream comparisons are separately recorded in feature validation reports and are not implied by a host test pass.

Battle-core still needs its complete enabled-source inventory reconciled with implementation and proof references, remaining fixed-size/overflow and received-hit histories audited, unresolved identity/form/fusion and phase callers closed, and wide-level Endless behavior validated across its owning callers. Full-run upstream comparisons and hardware acceptance remain separate from bounded source proofs. Read the [completion checklist](battle-core/COMPLETION-CHECKLIST.md) before describing battle-core as finished; older task tables and benchmark results are historical scopes, not a completion percentage.

Known gaps in the app include mystery-encounter waves that are not saved, upstream achievements not yet ported, and the evil-team and gym details listed in the PRs named above.

## Repository layout

| Folder | Purpose |
| --- | --- |
| [game](game/README.md) | Native dual-screen app, menus, animation, audio and run presentation |
| [battle-core](battle-core/README.md) | Battle state, mechanics, phases and source-comparison evidence |
| [run-core](run-core/README.md) | Encounter, trainer, wave and enemy generation |
| [ai-core](ai-core/README.md) | Enemy command selection and scoring |
| [reward-core](reward-core/README.md) | Reward generation and application |
| [game-core](game-core/README.md) | Integrated run orchestration, progression, mystery encounters, profile data and saves |
| [custom-core](custom-core/README.md) | Legacy/Nemesis data, reducer, codec, scheduling, JoJo roster, timed events and custom trainer policy |
| [football-core](football-core/LIVE.md) | Separate live football engine and career integration |
| [assets-pipeline](assets-pipeline/README.md) | Local upstream asset/data pack generation and shared renderer/loaders |
| [sd-builder](sd-builder/README.md) | Windows builder, local asset setup and SD-card installation |
| [bench-native](bench-native/README.md), [bench-gfx](bench-gfx/README.md) | Historical real-console timing and rendering measurements |
| [benchmark](benchmark/RESULTS.md), [skeleton](skeleton/README.md) | Earlier QuickJS feasibility work and native demo baseline |
| [tools/azahar-libretro](tools/azahar-libretro/README.md) | Headless emulator test tooling |

## Building and testing

From the repository root, build the homebrew app with devkitPro:

```sh
docker run --rm -v "$PWD":/src -w /src/game devkitpro/devkitarm make
```

The whole repository is mounted because the app links its logic cores. `game/` is the current app; `assets-pipeline/app-src/` supplies shared renderer, pack, audio and save support, while its own entry point is an older demo.

On Windows, follow the [SD builder instructions](sd-builder/README.md) to generate assets and prepare a local installation (the builder also produces a one-file `.3dsx` or `.cia` with the packs inside). Elsewhere, use `assets-pipeline/build_sd.py` as described in the [asset pipeline](assets-pipeline/README.md). This documentation update does not publish a new builder or bundle copyrighted packs.

Representative host checks are:

```sh
make -C game-core test
game/tests/run_host_tests.sh
make -C custom-core unit-test san-test
make -C football-core test
```

The [CI workflow](.github/workflows/ci.yml) lists the retained battle regression runners, failure controls and ARM checks. Reproducing native upstream comparisons also needs the pinned upstream checkout and each report’s producer instructions. Screenshots and pixel-test captures containing Pokémon art are not included in this repository.

## License

PokeRogue’s code is AGPL-3.0, and this port derives from it. Pokémon is © Nintendo, Game Freak and The Pokémon Company; their assets are not included here.
