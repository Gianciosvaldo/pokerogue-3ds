# PokeRogue for the Old 3DS

A native C port of [PokeRogue](https://github.com/pagefaultgames/pokerogue) for the original Nintendo 3DS, with offline SD-card saves and optional custom content. The app runs real battles and run progression, and includes a separate Pokémon football mode.

**Battle-core is still being completed.** This overview describes merged code through `414fde5` (2026-10-25), not exhaustive upstream parity or acceptance on every console. An older Windows builder contains the revision recorded in its `files/VERSION.txt`, not necessarily everything below.

**Code only:** Pokémon and PokeRogue art, music, fonts, logos and generated `.pak` files are built locally from pinned upstream sources for your own SD card. Do not commit or redistribute those assets. CI can provide the bare homebrew application; it still needs locally generated packs.

## Additions since October 4

The demo has become an integrated run app. The [complete change index](docs/CHANGES-SINCE-2026-10-04.md) records all 301 intervening first-parent commits, including individual mechanics and fixes. The [previous README](docs/history/README-2026-10-04.md) is preserved as historical evidence.

### Runs and presentation

- Title screen, Continue/New Game/Load Game, five save slots, run renaming, Save and Quit, run history, game-over and Classic-clear results, unlocks, vouchers and ribbons. Saves retain backups and reject invalid data; saving is restricted to supported run boundaries.
- Run-driven encounters, trainer battles, Classic rival fights and trainer dialogue, catching and full-party release, fleeing, experience, move learning, evolution choices, reward selection, shop rerolls and biome selection. Reward options use reward-core rather than the old fixed demonstration pool.
- Doubles encounters and lure-driven encounter chances, separate commands and target selection, and undoing the first Pokémon’s selection. Doubles layout, sprite transitions and menu responsiveness have received fixes.
- Battle music and animations, player/trainer presentation, boss HP segments, stat-stage inspection for your team and opponents, persistent key/foe item displays, held-item inspection, transfer and discard.
- TM compatibility labels, cancellation and descriptions; PP Up display corrections; localized move-name and trapping-message fixes; slower ability-description scrolling; bottom-screen level-up stat changes; and fixes for menu/command stack crashes.

The app’s entry flow remains focused on ordinary Classic and Custom Runs. Core support for other modes and high-wave generation does not establish a complete selectable Daily/Endless/Challenge interface. See the [app README](game/README.md) for controls and implementation details.

### Starters, eggs and persistent progression

- Starter filtering and sorting, plus pre-run customization of known moves, unlocked natures, shiny tiers and abilities, plus passive selection. Caught natures and natures obtained through mints persist in the profile.
- Species candy from the implemented upstream catch, hatch and friendship owners. Candy can unlock a passive, purchase same-species eggs and reduce a starter’s cost twice. A custom direct hidden-ability purchase uses that species’ passive-unlock cost; species without a hidden ability do not offer it.
- A luck display, profile unlock persistence and three scheduled Pokérus starters per three-hour period.
- Voucher-based egg acquisition and victory-driven hatch progression, hatched Pokémon sprites and animation, egg-move information, and a nature/stat/IV radar display. Hatching updates the starter profile rather than adding the Pokémon directly to the active party.
- IV Scanner prompts and persistent starter, candy, friendship and Classic-win records.

### Battle mechanics and fixes

Merged work includes doubles callbacks such as Helping Hand, Acupressure, After You, partner cures/healing, redirection, Commander/Order Up and pledges; additional abilities, forms, Transform/copy behavior, suppression and switching interactions; and held-item acquisition, theft, loss and transfer owners, including Grip Claw, Baton, Mini Black Hole and Pickup.

Further additions include Macho Brace growth and its 50-stack bonuses, Gimmighoul coin/evolution tracking, held form-item activation, Revival Blessing, Terastallization phases, and wider level/stat/damage/experience and save handling. Fury Cutter, Rollout and Echoed Voice retain their source-defined chains across waves when appropriate. Self-Destruct/Explosion user fainting and interrupted Dig/Fly/Dive/Phantom Force-style charge cleanup have regression coverage.

Long-history work now preserves Last Resort, nonvirtual move queries, Protect streaks, Conversion 2/Lock-On searches and repeated-faint/Retaliate/trainer-Tera callers beyond earlier history-window limits. These proofs cover their documented boundaries; other finite histories and wide-level callers remain under audit.

See the [battle-core README](battle-core/README.md), [completion checklist](battle-core/COMPLETION-CHECKLIST.md) and [deviation register](battle-core/DEVIATIONS.md). Encounter-end charge cancellation deliberately fixes user-reported behavior retained by the pinned upstream; its validation is recorded in [PR #409](https://github.com/Gianciosvaldo/pokerogue-3ds/pull/409).

### Custom Runs and football

- Custom Run Legacy reports record the final blow and support persistent Nemeses, radar, rematch scheduling, optional encounters and claims. Nemeses originate from Custom Runs. Report, roster and transaction persistence are integrated with the app.
- The capsule machine supports voucher-to-egg transactions, reveal presentation and touch interaction, with durable transaction handling. Additional mystery encounter ports include Department Store Sale, Shady Vitamin Dealer, Lost at Sea, Field Trip, Part Timer, An Offer You Can’t Refuse, the Pokémon Salesman and Fun and Games.
- JoJo challengers in Custom Runs: character/team tables, biome-based spawn policy, run-screen introductions and dialogue, sprite keys, and persisted encounter state.
- The title’s Football entry opens a separate live 22-player mode: a 55-point profile-based draft, editable offense/defense lineups, movement, manual snaps, passing, running, tackling, coverage, playcalling, a seven-round playoff, experience, evolution/move choices, shops and compatible Mega forms. It uses `football_live.sav`; START retains access to the older six-player mode and its `football.sav`. Ordinary run saves remain separate. See [live football controls and limits](football-core/LIVE.md).

## Validation and remaining work

The replication target is upstream game `8555c08c823b856cbec4eb99ca84ea52a955836d`, locales `270ed0a2938b2c3e2e34e7de940e84aecadf548b` and assets `87426a79611f9d212c4dc8af58e2834a05b93725`. This is not a claim of parity with today’s upstream beta. Recorded reference proofs use Node 22 where specified.

[CI run 40207779956](https://github.com/Gianciosvaldo/pokerogue-3ds/actions/runs/40207779956) passed on main `9c9f6e0`: core/custom/save/football checks, app host tests, the actual ARM build and startup-stack checks. Normal tests remain required; only the dedicated custom-only Task63 check is skipped in the current work. Native upstream comparisons are separately recorded in feature validation reports and are not implied by a host test pass.

Battle-core still needs its complete enabled-source inventory reconciled with implementation and proof references, remaining fixed-size/overflow and received-hit histories audited, unresolved identity/form/fusion and phase callers closed, and wide-level Endless behavior validated across its owning callers. Full-run upstream comparisons and hardware acceptance remain separate from bounded source proofs. Read the [completion checklist](battle-core/COMPLETION-CHECKLIST.md) before describing battle-core as finished; older task tables and benchmark results are historical scopes, not a completion percentage.

## Repository layout

| Folder | Purpose |
| --- | --- |
| [game](game/README.md) | Native dual-screen app, menus, animation, audio and run presentation |
| [battle-core](battle-core/README.md) | Battle state, mechanics, phases and source-comparison evidence |
| [run-core](run-core/README.md) | Encounter, trainer, wave and enemy generation |
| [ai-core](ai-core/README.md) | Enemy command selection and scoring |
| [reward-core](reward-core/README.md) | Reward generation and application |
| [game-core](game-core/README.md) | Integrated run orchestration, progression, profile data and saves |
| [custom-core](custom-core/README.md) | Legacy/Nemesis data, reducer, codec, scheduling and custom trainer policy |
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

On Windows, follow the [SD builder instructions](sd-builder/README.md) to generate assets and prepare a local installation. Elsewhere, use `assets-pipeline/build_sd.py` as described in the [asset pipeline](assets-pipeline/README.md). This documentation update does not publish a new builder or bundle copyrighted packs.

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
