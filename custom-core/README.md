# custom-core

The portable data model of the custom content mode ("Legacy": post-mortem, Nemesis roster, run-bound rematch
schedule, transaction ledger). Task C01 of `docs/CUSTOM-CONTENT-ADVANCED-AGENT.md`.

The contract, its limits, the audit it rests on and every choice still open for review are in
`docs/CUSTOM-CONTENT-CONTRACTS.md`. This folder only implements that contract.

## What is here

| Path | Content |
|---|---|
| `include/cc/limits.h` | version-1 limits, ID ceilings, storage budgets |
| `include/cc/types.h` | primitive-data structs: observation, event, turn, history, terminal summary, Nemesis record and roster, schedule, counters, ledger, metadata |
| `include/cc/wire.h` | byte widths of the persisted layout (no codec yet) |
| `include/cc/model.h` | validators, record lifecycle, identifier allocators, ledger, checked size arithmetic |
| `src/` | the implementation; C99, no allocation, no I/O, no globals except the constant window table |
| `tests/` | `model_test.c` (behavior), `layout_test.c` (wire sizes, budgets, ID ceilings against battle-core's generated enums) |
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

## What it deliberately does not do

No live hooks into battle-core, game-core or `game/`, no save codec or file I/O, no wallet, no spawning, no UI. The
module cannot touch a save file or the RNG, so it cannot change normal saves or battle results. Those belong to
tasks C02 onward and start only after this one is merged.
