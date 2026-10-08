# Reviver Seed inventory bridge

Canonical one-stack Seeds map to battle-core's `PR_MOD_INSTANT_REVIVE` (#152),
with owner/transferability preserved on both sides. FaintPhase itself owns the
half-HP restoration, status clearing, early return, text, phase order and RNG.
`gc_items_from_battle` now persists actual Seed consumption before rebuild/save;
repeated synchronization cannot recreate a consumed Seed. Inventory beyond the
96-entry battle prefix remains intact, including after prefix removals.

Malformed stacks/arguments and duplicate Seed objects for one holder stay opaque.
The existing generic acquisition path still lacks full max-stock/default-reward
replacement policy. This is existing-inventory battle/persistence parity, not
full acquisition, transfers, general loot distribution or Task 104 completion.
The pinned source also supports wildcard loaded owner -1: inventory owner zero
maps to -1 and the source-backed wildcard FaintPhase behavior is covered.

The differential exposed a separate source ownership state: a rejected enemy
Seed on Memento becomes detached transferable loot, with battle owner **0**,
while retaining its stack. This differs from wildcard **-1**. The additive
`gc_item.detached` flag preserves that distinction; `pokemon_id` retains the
original holder ID as provenance, while the flag overrides battle ownership to
0. Named save fields persist it without a format-version bump. Older saves omit
the field and load false. Detached-versus-new acquisition/stacking still follows
the generic unvalidated policy; invalid restacking becomes opaque rather than
receiving a recognized effect. The flag does not implement future loot pickup.

## Reproduce

Node 22.23.3, pinned prepared external upstream
`8555c08c823b856cbec4eb99ca84ea52a955836d`, frozen dependencies/locales/assets:

```sh
make -j2 -C game-core test
for task_seed in 777 9001 4242 31337; do
  game-core/tools/seeddiff.sh /external/pinned-upstream 2000 "$task_seed"
done
game-core/build/gcplay --auto 1000 --jobs 8
game-core/build/gcplay --auto 1000 --jobs 8 --save-test
```

`BUILD` and `GC_SEED_OUT` select build/trace paths. The wrapper copies the
unchanged owning battle-core producer, includes its unchanged full-turn
comparator/guard checks, inserts real inventory conversion before the turn and
reconciles afterward. It compares source full battle state, RNG, phase queues,
all rendered texts and modifiers, plus actual inventory consumption, rebuilt
owner/class/stack/transferability and repeated synchronization. All records,
requested counts and completion footer are guarded. No established producer,
comparator, seed, case count or exclusion was changed.

## Results

Base main `6de6910`. Durable evidence:
`/workspace/validation/lane-c/eight/reviver`.

- Four seeds x 2,000 full turns: all 8,000 covered/identical and all texts
  identical; zero parse/stat mismatches, skips, untranslated or uncovered turns.
  Each seed: 1,000 Seeds per side, 700 consumed per side, ten states with 200
  cases each. States cover statuses/stages, wrong holder, Memento, surviving
  damage, wildcard, Unburden and both Leftovers orderings. Rejected enemy Memento
  preserves detached loot ownership. Included owning guards: 12 checks.
- 126 new host checks cover actual command resurrection, both sides and exact/
  wildcard owners, saved command/resume effects, no re-created consumable,
  duplicate/invalid-stock opacity, 96/128 prefix overflow and repeated sync,
  detached serialization/rebuild/provenance and generic restacking/drop guards.
  A true older save written against the baseline schema loads successfully:
  130 checks including old Seed ownership/default-false. The older Mint save
  also passes its unchanged 113-check reader. The detached save probe uses a
  synthesized otherwise legal command boundary; source turns independently
  prove how detached loot is reached, not a complete future loot/save flow.
- An actual temporary `gc_items.c` mutant disabling Seed reconciliation fails
  the first real consumption case (inventory 1, source battle remaining 0),
  exit 1. Missing footer and stack-2 input transport exit 2. Mutants are external.
- Existing four-seed healing/candy/ball/options traces remain byte-identical to
  baseline. Existing exclusions/constructor-stat diagnostics stay uncredited.
  Mint/vitamin/money each retain 16,000 comparisons; Orb retains 8,000 full turns.
- H passes: 32,697 resume checks, 39 existing item-bridge checks, 641,335 existing
  application checks; paired 200-run digest changes `39213240` to `3e264621`.
  Both 1,000-run modes end all runs, no hang/crash/timeout. Before: 49,100 turns,
  digest `2473e509`; after: 49,206 turns, digest `3492b259`, 14,291 save round
  trips, maximum 11,400 bytes. Chooser/digest code is unchanged. Aggregate opaque
  held-item counts move 52,065 to 52,180 due to subsequent run outcomes; this is
  not a per-Seed effect count. The 25 taken Seeds are acquisition tallies.
  An external observed adapter copy reproduces digest `3492b259` and counts nine
  real player Seed consumptions/revivals, zero enemy ones, 213 recognized player
  mapping visits and zero opaque Seed visits; mapping visits repeat across waves
  and are not distinct acquisitions. Enemy effects are proved by source fixtures.

Required actual-head host/3DS CI remains a merge gate. Console and complete
upstream run/frontend/acquisition parity are not claimed.
