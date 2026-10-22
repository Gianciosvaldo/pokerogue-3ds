# Football mode: attribute foundation

First module of the football playoff mode. The season state machine, playcalling,
rendering, and mode entry point are subsequent work; this PR does not expose a
playable mode yet.

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
The application build is deliberately unchanged until that integration exists.

Validation: `make -C football-core test` checks formula expectations, kg unit
conversion, saturation, failure atomicity, dual-type scoring, and all 1,500
existing form records. Host C99 build uses warnings as errors. A 3DS compiler is
not installed in this environment, so device compilation/performance remains
unverified.
