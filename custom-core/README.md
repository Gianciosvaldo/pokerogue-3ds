# custom-core

The portable data model of the custom content mode ("Legacy": post-mortem, Nemesis roster, run-bound rematch
schedule, transaction ledger) and the bounded history reducer. The initial model/reducer work follows tasks C01 and C02 of
`docs/CUSTOM-CONTENT-ADVANCED-AGENT.md`.

The contract, its limits, the audit it rests on and every choice still open for review are in
`docs/CUSTOM-CONTENT-CONTRACTS.md`. This folder only implements that contract.

## What is here

| Path | Content |
|---|---|
| `include/cc/limits.h` | version-1 limits, ID ceilings, storage budgets |
| `include/cc/types.h` | primitive-data structs: observation, event, turn, history, terminal summary, Nemesis record and roster, schedule, counters, ledger, metadata |
| `include/cc/wire.h` | byte widths of the persisted layout; codec API is in `codec.h` |
| `include/cc/model.h` | validators, record lifecycle, identifier allocators, ledger, checked size arithmetic |
| `include/cc/reducer.h` | C02: the reducer (`cc_reduce`), its state and result, per-turn totals |
| `src/` | the implementation; C99, no allocation, no I/O, no globals except the constant window table |
| `include/cc/tevents.h`, `src/tevents.c`, `src/tevents_table.c` | timed events: the schedule model (daily / every N days / an hour a day on the console's local clock), the combination of overlapping events and the draft event table; nothing reads the effects yet |
| `tests/` | model, reducer, codec, scheduling, policy and timed-event checks; layout checks cover wire sizes, budgets and generated ID ceilings |
| `tools/mutations.py` | the mutation check: each listed single change to the sources must make a test fail |

## Run

```
make -C custom-core test            # unit + layout, the same under ASan/UBSan, then the mutation check
make -C custom-core unit-test       # fast loop
make -C custom-core report          # sizeof and encoded-size bounds of this build
make -C custom-core arm-check       # compile-only for the 3DS CPU flags (needs arm-none-eabi-gcc)
make -C custom-core CC=clang BUILD=build/clang unit-test
python3 custom-core/tools/mutations.py -k nemesis   # a subset of the mutations
```

`.github/workflows/custom-content.yml` runs the same targets.

## Timed events

lorenzo's own events (2026-11-16): short repeating windows on the 3DS clock, shorter the more an event gives (daily, every two or three
days, one hour a day), with effects taken from upstream's own events (shiny odds, shiny catch rate, luck, candy rate, trainer shiny chance,
voucher upgrade). A second round adds lists and flags: items given after a Classic wave, luck-boosted species, bonus items of the Delibird-y
encounter, boosted fusions, a COMMON Delibird-y and snow in Town (lists are joined without duplicates, flags are OR-ed). `include/cc/tevents.h` holds the model and the rules (time is always an argument, local wall-clock seconds; overlapping events
take the strongest value of each field and never stack). `src/tevents_table.c` is a DRAFT list for lorenzo to cut. This module only decides:
the game reads `cc_tevents_effects()` at the moment of each roll in later PRs.

## Integration and boundaries

`include/cc/codec.h` and `src/codec.c` implement the record codec;
`include/cc/scheduler.h` owns rematch scheduling, and `include/cc/jojo.h` adds
custom trainer tables and spawn policy. The app/game-core integrations observe
runs, persist reports and rosters, drive Nemesis encounters and expose the radar,
claims and capsule UI. See the [current feature overview](../README.md).

The portable module itself remains free of filesystem/UI dependencies. Callers
own observation, storage and encounter installation; the reducer consumes their
explicit batches. The app now installs the JoJo team hook for Custom Runs and persists its roll
state in the custom overlay. Normal run saves and football saves keep their separate owners.
