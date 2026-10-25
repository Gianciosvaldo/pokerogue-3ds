# Playoff balance and reliability audit

The native audit runs real FootballMode matches, EXP prompts, evolution choices,
shop purchases and mega selection. It never forces wins or awards fake currency.
Seeds 1–512 are repeated for four legal classic-budget drafts. The controller
uses Dive on short yardage, Slants otherwise with every fourth snap a Vertical,
and Zone on long distance or Man otherwise. It accepts the first evolution,
skips new moves, buys affordable stones before vitamins, and selects owned megas.
Contact stays Balanced/Standard. This is one simple policy, not optimal play.

The previous 1x–2.5x difficulty ramp and 500 * round² EXP produced no clears in
2,048 attempts. The ramp now increases by 16/256 per round (ending at 1.375x),
and EXP is 2000 * round² so evolution participates earlier in the short season.
First-round match difficulty is unchanged. Existing saved active matches retain
their stored difficulty; future rounds use the new curve.

| Draft | Previous clears / 512 | Revised clears / 512 |
| --- | ---: | ---: |
| Bulbasaur, Charmander, Squirtle | 0 | 2 |
| Nincada, Abra, Pidgey | 0 | 2 |
| Mewtwo, Pidgey | 0 | 33 |
| Mawile, Bulbasaur | 0 | 0 |

The revised sample covers 4,716 matches and 284,647 snaps; the longest match
was 103 snaps. All runs terminated and conserved earned currency exactly.
The Mawile draft remains difficult under this policy. These results do not
establish human win rates, equal draft strength, or final item/mega balance.
Visual and device performance testing remain deferred to the next Windows build.

Reproduce from the repository root:

```sh
make -C football-core balance-baseline-report balance-report
football-core/build/balance_baseline 512 > /tmp/balance-before.json
football-core/build/balance_report 512 > /tmp/balance-after.json
python3 football-core/tools/check_balance_report.py /tmp/balance-after.json
```

Committed before/after JSON lives in `tests/fixtures/`. Repeated revised runs
were byte-identical locally. `make -C football-core test` runs the revised audit
and checks termination, round progression, reward conservation, bounded wallets,
purchase counts and affordability. It deliberately avoids exact per-draft win-rate
gates. The baseline executable overrides only the two pacing constants and uses
the same current mechanics and policy, isolating that comparison.

Checkpoint regressions also reject valid-checksum but semantically invalid phase,
round, wins, wallet, RNG, mega, boost and shop-count values, preserve the profile,
and reject oversized files before allocation. Existing checksum, truncation,
backup and version migration checks remain in the suite.
