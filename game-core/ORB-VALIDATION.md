# Toxic Orb / Flame Orb inventory bridge

The existing generic held-item path stores both Orb identities with a stable
Pokemon owner. `gc_items_to_battle` now maps those keys to the merged battle-core
`PR_MOD_TURN_STATUS_EFFECT` class, explicitly selecting TOXIC or BURN. Both sides
retain owner, stack and transferability; the recognized classes no longer enter
the opaque held-item fallback. Status application, guards, phase ordering, quiet
messages and RNG belong to battle-core's existing implementation (PR #148).

Orbs are not consumed. Reconciliation preserves their inventory entries, and
save/load rebuilds the class/effect from the item key without a save change.
This slice covers existing inventory and its battle boundary. Generic acquisition
still lacks upstream's shared Orb max-stack/duplicate replacement policy; this
does not claim full `BattleScene.addModifier`, item transfers, complete Task 57,
or full run/reward parity. Other item effects remain counted where unsupported.

## Reproduce

Use Node 22 (validated 22.23.3), the prepared external upstream checkout at
`8555c08c823b856cbec4eb99ca84ea52a955836d`, frozen dependencies, locales and
headless assets. The new wrapper enforces the pin and Node major. Case counts
must be positive multiples of 20 to cover both Orbs and all ten states.

```sh
make -C game-core test
game-core/tools/orbdiff.sh /external/pinned-upstream 2000 777
game-core/tools/orbdiff.sh /external/pinned-upstream 2000 9001
game-core/tools/orbdiff.sh /external/pinned-upstream 2000 4242
game-core/tools/orbdiff.sh /external/pinned-upstream 2000 31337
game-core/build/gcplay --auto 1000 --jobs 4
game-core/build/gcplay --auto 1000 --jobs 4 --save-test
```

`BUILD` and `GC_ORB_OUT` select external build/trace paths. The wrapper copies
the unchanged battle-core Orb producer to upstream. The new comparator includes
the unchanged full-turn comparator and inserts the actual inventory conversion
before each real turn. Unsupported fixture classes/virtual stacks are rejected.
It compares full state, modifiers, RNG, phases, all rendered texts and inventory
non-consumption, with strict requested-count/completion/coverage guards. No
established producer, comparator, seed, case count or exclusion was changed.

## Results

Validation base `7d438c2` includes Mint, vitamins, Wide Lens and Reviver Seed.

- Four seeds, 2,000 full turns each: all 8,000 covered and identical, zero
  skipped, parse/stat mismatches, uncovered turns or untranslated texts. Each
  seed maps 1,000 Toxic and 1,000 Flame entries on each side. Ten states each
  have 200 cases: normal application, existing status, immunity ability/type,
  Comatose, suppression, Synchronize, Poison Heal, wrong owner and Misty Terrain.
  Each seed records 800 status applications per Orb and 1,200 rejections per Orb;
  all fixture inventories remain unchanged.
- New host test: 235 checks for both identities/sides/owner forms, mixed inventory,
  recognized versus unknown classes, generic single-item storage, command turns,
  save/load status/effect/owner preservation and matching resumed HP/stats/PP/RNG.
  A post-turn save is correctly refused; persistence is tested at the next wave's
  legal command boundary. The included unchanged battle guard adds 14 checks.
- A temporary actual `gc_items.c` mutation swapping TOXIC/BURN produces 2,000
  full-state failures and 1,200 text failures of 2,000 turns (exit 1), with zero
  parse/stat errors. Missing footer and invalid effect transport exit 2. The
  first invalid-effect fixture attempt changed no record; it was corrected to
  mutate a real MOD record before recording the rejection result.
- Host suite passes before/after: 32,731 / 32,729 resume checks, 39 existing
  item-bridge checks and 641,335 existing application checks. Both paired
  200-run digests remain `a6662b64`. All four Mint traces still pass 16,000
  free/purchased comparisons on the integrated source.
- Before/after 1,000-run checks: all runs ended, no hangs/crashes/timeouts;
  49,305 / 49,293 turns. Both save/no-save pairs match: before `b465d096`,
  after `b72f38a9`. After-save: 14,307 round trips, maximum save 11,582 bytes.
  Recognized effects change outcomes; chooser/digest code is unchanged. The
  aggregate opaque held-item counter moves 53,279 to 53,254; this aggregate is
  affected by subsequent run choices and is not an exact per-Orb effect tally.

The comparator's inherited formatting warnings were retained with its unchanged
source. These checks establish the bounded host/source bridge. Required host and
3DS CI must pass on the actual PR head; console hardware remains unrun.
