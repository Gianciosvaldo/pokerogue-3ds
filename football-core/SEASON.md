# Seven-match playoff progression

`playoff_season.c` owns DRAFT -> READY -> MATCH -> EXP -> SHOP transitions.
Seven consecutive wins clear the run; a loss eliminates it. A match starts only
when the previous EXP prompts and shop are complete. `FootballMatchSpec` supplies
the engine with a deterministic seed and difficulty from 1x to 2.5x. The football
match simulation remains the engine's responsibility; report its result once via
`football_season_finish_match`.

Use one dedicated `gc_run` kept static or allocated once outside the frame loop.
Call `football_season_new`, then draft 1–6 distinct base-form, first-stage species.
The six-slot roster matches the current port's party capacity; eleven-player
rosters would require a separate roster/evolution extension. Costs, profile
unlocks, discounts and budget use the same classic starter functions. This run
must not also be passed to classic battle setup, `gc_run_set_starters`,
`gc_run_choose`, or classic save/load. Football save serialization and a main-menu
entry are not implemented by this module.

The narrow `game-core/src/gc_football.c` adapter reuses the existing EXP, level-up,
move-learning and evolution phases without starting a classic battle. Drain
`gc_run_next_event` between calls to `football_season_tick`; each tick executes at
most eight phases and yields before exhausting the bounded event buffer. When
WAIT is returned, show `gc_run_screen`. Use `football_season_choose` for move
learning and evolve/cancel. For evolution, list `gc_football_evolution_choices`
and pass the selected index. Split evolutions preserve existing validation rules.
Nincada explicitly chooses Ninjask OR Shedinja, replacing one slot; it never adds
a second Pokemon. Classic evolution behavior is unchanged.

A winning match awards increasing EXP (500 * round squared per teammate) and
currency (400 + 100 * round). Classic level caps advance on synthetic waves
20, 40, ..., 140. Base football attributes follow the requested base-stat formulas;
level-ups enable evolution rather than adding unspecified football multipliers.
Evolution refreshes attributes and affinity while retaining purchased bonuses.

Every shop offers Carbos and Protein, each sold once, adding two SPD or STR
(up to +20 per attribute and the overall cap 99). These are football-mode effects;
they do not invoke the classic vitamin purchase path, whose pinned behavior has
separate semantics. The wallet is football-only. A deterministic 10% roll can add
a compatible mega/primal item from the existing form-change item catalog. Purchase
installs it in the existing held-item inventory; activation belongs to the engine's
form-change handling and is not implemented as an automatic evolution here.
An owned form-change item cannot be bought twice for that holder.

`football_shop_asset_key` and each offer's `classic_arg` identify the existing
BASE_STAT_BOOSTER / FORM_CHANGE_ITEM assets for the classic menu asset adapter;
no new item artwork is required. Rendering and touch menus are separate systems.

`make -C football-core test` exercises all seven rounds, repeated result rejection,
draft restrictions and costs, EXP and Shedinja choice through the real shared
phase implementations, purchases, sold offers and elimination. CI runs this suite
and includes football sources in the normal 3DS build and stack checks.
