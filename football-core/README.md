# Football mode systems

The football architecture is split into attribute generation, playoff progression,
playcalling, and a dual-screen renderer. See [SEASON.md](SEASON.md) for the real
classic EXP/evolution adapter and [UI_RENDER.md](UI_RENDER.md) for input, front
sprite reuse, GPU flipping and render-loop integration. These systems provide the
requested engine-facing interfaces. [MATCH.md](MATCH.md) documents the match
resolver, AI, rules and score-to-season bridge. [APP.md](APP.md) covers the
Football Playoff title entry, complete screen flow, controls and session-only
lifetime. Football save/resume and mega activation remain separate work.

`include/football_stats.h` consumes battle-core's generated `pr_form` directly.
Initialize one `FootballDexMaxima` with
`football_stats_build_maxima(&maxima, pr_forms, PR_FORM_COUNT)`, then call
`football_stats_generate(&player, roster_id, form, &maxima)` at draft time and
after evolution. The existing roster retains EXP, evolution and item ownership.
The entire supported form table defines per-stat Pokédex maxima, including special
forms. If the supported Pokédex changes, rebuild the maxima. No universal 255
normalization is used. A zero base stat is invalid.

Weights convert `weight_x10` to kilograms before logarithmic normalization.
Weight saturates at 99 above 999 kg. Formula intermediates retain fractional
precision; stored attributes round to the nearest integer and clamp to 1–99.

Type affinity uses the arithmetic mean for two distinct types; a repeated type
counts once. Scalars affect position scoring, not stored attributes. Position
weight rows sum to one. Exact ties favor the earlier enum entry. Assignment is
per player; drafting must enforce roster position quotas separately. OL/DL and
TE/S are intentionally combined positions. The matrix is an initial balance
policy, with explicit entries for all 18 natural Pokémon types. Stellar is not
accepted as a species base type.

The module uses no allocations, I/O, graphics resources or platform-specific
APIs. Working storage is constant and small; logarithms run only during stat
generation. Build it with the 3DS sources and link libm when integrating the mode.
The application build compiles the football sources; existing classic entry points
remain unchanged.

Validation: `make -C football-core test` checks formula expectations, kg unit
conversion, saturation, failure atomicity, dual-type scoring, and all 1,500
existing form records. Host C99 build uses warnings as errors. GitHub CI also compiles the 3DS app and checks compiler stack reports. Device
visuals and runtime performance still require hardware or emulator validation.
