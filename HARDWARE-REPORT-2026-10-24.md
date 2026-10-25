# Encounter text, Nemesis capture and held-item descriptions

Pinned reference: upstream `8555c08c823b856cbec4eb99ca84ea52a955836d`,
locales `270ed0a2938b2c3e2e34e7de940e84aecadf548b`.

Wild doubles used `battle:doubleWildAppeared`, a key absent from pinned locales.
The encounter now uses upstream's `multiWildAppeared` with both Pokemon names.
Trainer doubles use the native `trainerAppearedDouble` branch. The regression
stops the actual encounter immediately before field setup and checks all four
wild/trainer × single/double messages through the real formatter and pinned
English locale fixture, including both names and line breaks (26 checks).
It is an intro-boundary test, not a claim of complete encounter parity.

Opening an own held item's summary description now suppresses the underlying
Fight/Ball/Pokemon/Run window, party tray and message prompt. Previously the
retained command footer was drawn before the description. The real draw-path
regression covers top/bottom screens, command/retained idle states and restoring
all controls after the description closes.

Nemesis capture previously rejected every double battle, even when the actual
faint hook identified one precise enemy. Verified ordinary wild finishers from
doubles can now be remembered, with source traits and the actual attacker ID
retained. Their rematches still occur only in eligible single battles. Unknown
attribution, bosses, fusion, Tera, trainer ownership, scripted encounters and
rematch enemies retain their existing restrictions. Classic runs still do not
create Nemeses: capture and rematch records belong to Custom Runs.

The actual custom-run regression retains the original 24 seeded defeats as a
prefix and extends to 96. It observes both opponents before combat, then checks
the remembered identity, level, nature, ability, passive, IVs, moves, items and
report against the exact finishing enemy. Both positions must occur: the initial
run passed with left 1 / right 6 doubles finishers, 96 records and 25,818 total
checks across attribution, rematches, saves and crash recovery. Core model,
insertion/codec and negative-attribution tests include the doubles provenance.
The old mutation that removed doubles rejection is replaced by its reverse,
which incorrectly rejects a verified doubles finisher. Other mutation cases stay.
Normal custom-core unit/sanitizer tests are included in CI; dedicated custom
Task63 remains skipped.

Transform PP needs no gameplay change. Pinned `PokemonTransformPhase` constructs
copied moves with `Math.min(m.getMove().pp, 5)`: most copied moves start at five PP,
and moves whose base PP is smaller keep that lower maximum. The retained
`transform_visual_test` already checks Sketch at one PP and Tackle at five.

No builder or release is included. Normal core/football, app/custom/save and
actual 3DS build/stack CI remain required before merge.
