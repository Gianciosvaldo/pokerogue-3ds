# reward-core: the rewards after a wave

Native C port of PokeRogue's player rewards: the reward options after a wave (pools, weights, luck upgrades, rerolls, the Lock Capsule's locked tiers, the copy of the screen after a TM), the fixed rewards some waves give, and next what each reward does when it is picked. Like run-core, it is diff-tested against upstream `pagefaultgames/pokerogue` @ `8555c08`. It reads battle-core's and run-core's RNG, run state and generated tables **read-only**; `make` builds both libraries into reward-core's own build folder.

The API is [`include/pr/reward.h`](include/pr/reward.h). The run loop (game-core) fills a `pr_reward_ctx` from the run before each reward screen: the party as the rolls see it, the player's items, ball counts, unlocks, the arena. A few of its fields are other ports' answers, marked "caller" in the header: `canSetStatus` (battle-core), `canApplyAbility`, the learnable level moves and the level-move list the TM filter leaves out (learnsets, not ported yet), each attack move's types for item spawns. The diff test takes those from upstream, so reward-core is checked given correct inputs.

## Task 96 prerequisite: canonical Multi Lens acquisition (lane B, 2026-10-09)

`pr_reward_add_canonical_held_item` now accepts Multi Lens with its pinned cap2.
This fills the raw acquisition prerequisite; the existing battle hit-count
implementation (#157) and loaded-inventory bridge (#172) are separate work.
The actual `PokemonMultiHitModifier` factory starts with real stack1 and virtual0;
`PersistentModifier.add` returns the result of incrementing the first exact
class/holder match. A full first duplicate refuses rather than searching later
entries. Wildcard -1 is not an acquisition match. Other holder/class entries,
order and active flags survive; appended entries start inactive.

Multi Lens inventory records now require stack1..2, owner-1 or a valid party
index, and arg/count0 on every supported canonical-held acquisition. Malformed
Multi records previously treated as opaque now refuse unchanged, including when
acquiring Wide Lens or Reviver Seed. Unique party IDs and zero virtual stacks
remain caller requirements. Other known unsupported families remain opaque.
Matching stacks/full refusals work at the256-slot capacity; append at capacity
refuses unchanged. No party, wallet, RNG or non-inventory context changes.

This API is raw `.add(modifiers,false)`, not Scene.addModifier tier fallback,
stat updates, purchase/selection flow, effects, transfer/cleanup or persistence.
Missing/detached holders, custom constructors/incoming quantities, and virtual
additions remain excluded. Task96 and full held-item lifecycle remain partial.
Source boundaries: pinned `modifier.ts` PersistentModifier.add/incrementStack,
PokemonHeldItemModifier.match, PokemonMultiHitModifier.getMaxHeldItemCount;
`modifier-type.ts` PokemonMultiHitModifierType and MULTI_LENS factory.

Measured Node22.23.3/Vitest4.1.11 at the named pin: four seeds777/9001/4242/31337
at2000 each, all8000 identical, no failures/skips/caught exceptions. 29 host
checks cover malformed inventory/context, all-slot capacity, duplicate first
match, class/holder/wildcard isolation and nonmutation. Complete factory-backed
inventories include order/active flags; upstream checks full PokemonData,
wallet and global/battle RNG, while C checks full noninventory context/run.

```text
MULTIHELD cases=2000 identical=2000 failed=0 skipped=0 thrown=0 full=1178 added=400 stacked=422 wildcard=667 duplicate=800 reordered=1000 fainted=1007
MULTIHELD cases=2000 identical=2000 failed=0 skipped=0 thrown=0 full=1202 added=400 stacked=398 wildcard=667 duplicate=800 reordered=1000 fainted=1016
MULTIHELD cases=2000 identical=2000 failed=0 skipped=0 thrown=0 full=1210 added=400 stacked=390 wildcard=667 duplicate=800 reordered=1000 fainted=1018
MULTIHELD cases=2000 identical=2000 failed=0 skipped=0 thrown=0 full=1202 added=400 stacked=398 wildcard=667 duplicate=800 reordered=1000 fainted=1012
```

Real canonical-held C source mutants on seed777 produce145 wildcard-matching,
337 later-duplicate-search and123 premature-cap failures (each exits1).
Missing footer, fractional case count, extra footer field and nonfinite RNG
traces exit2; fractional producer requests and negative seeds also exit2.
The wrappers validate counts/seeds before touching a trace output.

Unchanged baseline766bd05 before/after logs are byte-identical: reward options
3950pass/0fail/50unported, victory/application/Memory4000/0 each; Mint, vitamin,
Lure, money, temporary, Soothe, Wide, Reviver and Golden suites each4×2000 exact.
No established producer, comparator, seed, tolerance, exclusion or count changed.
`make -C game-core -j2 test` passes, including200-run normal/saved digest74adeffb.
Both1000 normal and saved H runs end1000/1000 with0clear/stuck/crash/timeout,
50394turns and digest43f24f9f;14679save/load round trips, max11472bytes.
Full before/after1000 logs (including every stand-in count) are byte-identical.
These H results establish host consistency, not upstream whole-run parity.

Reproduce:
```sh
. /workspace/shared/activate-parity.sh
for seed in 777 9001 4242 31337; do
  BUILD=/tmp/reward-multiheld-build sh reward-core/tools/multihelddiff.sh /path/to/upstream-8555c08 2000 "$seed"
done
BUILD=/tmp/reward-multiheld-build sh reward-core/tools/multiheldplant.sh /tmp/reward-multiheld-build/multiheld-777.txt
make -C game-core -j2 test
game-core/build/gcplay --auto 1000 --jobs 8
game-core/build/gcplay --auto 1000 --jobs 8 --save-test
```

## Task 96 prerequisite: canonical Golden Punch acquisition (lane B, 2026-10-08)

`pr_reward_add_canonical_held_item` now supports Golden Punch in addition to
Wide Lens and Reviver Seed. The actual `DamageMoneyRewardModifier` factory's
held cap is5: append stack1 or increment the first exact-holder Punch to5;
FULL refuses unchanged and never searches a later duplicate. Other class/holder
entries, wildcard records, active flags and order survive. Complete party and
RNG/money invariance demonstrate acquisition has no damage reward effect.

Newly supported Golden Punch entries are validated on every supported family
acquisition: stack1..5, owner-1 or valid index, arg/count0. Previously opaque
malformed Punch records now refuse when acquiring Wide Lens or Reviver Seed,
without mutation. Other known classes remain opaque. Both earlier focused
canonical-held suites remain unchanged and are replayed at full counts.
The API requires unique party IDs and canonical ordinary/nonvirtual records;
invalid shape/append capacity refuses, while matching stacking/full refusal
works at256 slots. Missing/detached-holder and transient stacks, custom incoming
quantities, raw wildcard Scene persistence, cleanup/transfers, full Scene tier
replacement/update/raw return and selection/purchase behavior stay excluded.
Battle damage money/Amulet Coin effects, wallet transport, achievements and
save/game-core lifecycle are separately owned; this does not replace or expand
the existing `pr_reward_damage_money_multiplier` helper. Task96 remains partial.

Measured with Node22.23.3/Vitest4.1.11 at8555c08c823b856cbec4eb99ca84ea52a955836d,
using actual factories/classes and `.add(modifiers,false)`, complete inventories
(including order/active), party PokemonData, money, global/battle RNG invariance;
C compares the entire non-inventory context and run unchanged. Zero skipped or
caught exceptions. `GOLDENHELD guards=28 no-mutation/capacity/first-match checks passed`. Supplemental host guards cover capacity256, invalid known
family shape/party IDs/target, full refusal and first matching duplicate.

```text
GOLDENHELD cases=2000 identical=2000 failed=0 skipped=0 thrown=0 full=964 added=400 stacked=636 wildcard=667 duplicate=800 reordered=1000 fainted=1007
GOLDENHELD cases=2000 identical=2000 failed=0 skipped=0 thrown=0 full=948 added=400 stacked=652 wildcard=667 duplicate=800 reordered=1000 fainted=1016
GOLDENHELD cases=2000 identical=2000 failed=0 skipped=0 thrown=0 full=959 added=400 stacked=641 wildcard=667 duplicate=800 reordered=1000 fainted=1018
GOLDENHELD cases=2000 identical=2000 failed=0 skipped=0 thrown=0 full=946 added=400 stacked=654 wildcard=667 duplicate=800 reordered=1000 fainted=1012
```

Actual C source mutants (seed777; every mutant exits1; missing footer exits2):

```text
GOLDENHELD cases=2000 identical=1815 failed=185 skipped=0 thrown=0 full=964 added=400 stacked=636 wildcard=667 duplicate=800 reordered=1000 fainted=1007
canonical held wildcard source mutant rejected (exit 1)
GOLDENHELD cases=2000 identical=1722 failed=278 skipped=0 thrown=0 full=964 added=400 stacked=636 wildcard=667 duplicate=800 reordered=1000 fainted=1007
canonical held first source mutant rejected (exit 1)
GOLDENHELD cases=2000 identical=1950 failed=50 skipped=0 thrown=0 full=964 added=400 stacked=636 wildcard=667 duplicate=800 reordered=1000 fainted=1007
canonical held cap source mutant rejected (exit 1)
```

Unchanged traces replay before/after baseline `492a0ea`: options3950pass/0fail/
50unported (8052screens), victory4000/0 (2055items/2459screens), application4000/0
(115text cases), Memory4000/0. Mint/vitamin/Lure/money/temp/Soothe each retain
4seeds×2000=8000 identical, with33/31/26/33/25/25 host guards respectively.
All corresponding logs are byte-identical; no original producer/comparator,
seed, coverage rule or generated table changes. Prior new canonical-held traces
are also replayed unchanged in subsequent family extensions.

H passes (`resume regression: 32697 checks passed`; reward_apply641335). The200-run pair is
`without saves: runs' digest: 79d84c65` (saved identical). Full normal/saved1000-run before/after outputs
are byte-identical per mode, including every stub and coverage count:

```text
1000 runs: 1000 ended (0 cleared), 0 stuck, 0 crashed, 0 timed out; 49206 turns (49.2 per run)
final wave:  0-9:125 10-19:708 20-29:156 30-39:10 40-49:1
runs' digest: c2937ca0

stand-ins (runs that used it, times):
  enemy moveset (last 4 level moves)                   1000 runs     14701
  double battle played as single                        762 runs      1544
  evolution condition not checked                         2 runs         3
  biome weather roll                                   1000 runs      1851
  enemy run-core can't generate (stand-in)             1000 runs      2258
  post-battle abilities, Pay Day, lapsing items           3 runs         4
  trainer's name without its title in a message         307 runs       402
  reward taken without its own effect                   139 runs       169
1000 runs: 1000 ended (0 cleared), 0 stuck, 0 crashed, 0 timed out; 49206 turns (49.2 per run)
final wave:  0-9:125 10-19:708 20-29:156 30-39:10 40-49:1
runs' digest: c2937ca0
save/load round trips: 14291 (largest save 11400 bytes of 131072)
```

This API has no runtime caller added, so H digests are retained from the stated
baseline. Host consistency does not establish upstream whole-run parity.

Exact commands (port root; every producer runs serially with one worker):

```sh
. /workspace/shared/activate-parity.sh
# Repeat for777 9001 4242 31337:
BUILD=/workspace/validation/lane-b/ten/golden-punch/build sh reward-core/tools/goldenhelddiff.sh /workspace/shared/upstream-lane-b 2000 <seed>
# Script runs in upstream, with GOLDEN_HELD_N=2000,
# GOLDEN_HELD_SEED=<seed>, GOLDEN_HELD_OUT=/workspace/validation/lane-b/ten/golden-punch/build/goldenheld-<seed>.txt:
# node node_modules/vitest/vitest.mjs run --maxWorkers=1 test/tests/zz-dump/golden-held.producer.test.ts
# Then replays through /workspace/validation/lane-b/ten/golden-punch/build/goldenheldtest.
BUILD=/workspace/validation/lane-b/ten/golden-punch/build sh reward-core/tools/goldenheldplant.sh /workspace/validation/lane-b/ten/golden-punch/build/goldenheld-777.txt
# Remove final Z footer to build/truncated.txt; replay must exit2:
/workspace/validation/lane-b/ten/golden-punch/build/goldenheldtest /workspace/validation/lane-b/ten/golden-punch/build/truncated.txt
sh /workspace/validation/lane-b/ten/regressions.sh 492a0ea /workspace/validation/lane-b/ten/golden-punch 'mint vitamin lure moneyitem tempbooster soothebell wideheld reviverheld'
sh /workspace/validation/lane-b/ten/full-h.sh /workspace/validation/lane-b/ten/golden-punch
# full-h builds baseline reward-core extraction against identical read-only cores,
# then both binaries run --auto 1000 --jobs 8 and --auto 1000 --jobs 8 --save-test,
# comparing complete per-mode logs. make -s -C game-core -j2 test is included.
```

External helper sources, before/after logs, fullH and four complete oracle traces
remain at `/workspace/validation/lane-b/ten/golden-punch`; no traces/upstream source/assets or binaries are committed.
Actual-head host and3DS CI are required before matched-SHA merge; skipped
conditional jobs are not credited. This is a bounded Task96 prerequisite.

## Task 96 prerequisite: canonical Reviver Seed acquisition (lane B, 2026-10-08)

The shared `pr_reward_add_canonical_held_item` now also supports Reviver Seed,
using the real `PokemonInstantReviveModifier` factory/class and max-held-count1.
Canonical stack1 appends; any first same-holder Reviver entry is already full
and refuses unchanged. No STACKED outcome is possible for ordinary Seeds. A
wildcard raw record is distinct from a concrete holder; duplicate full records
never cause replacement or revival. Stock is independent of current HP/status.
Wide Lens support and its complete source traces remain unchanged.

The guard now validates Reviver records (owner-1 or valid index, arg/count0,
stack1) on every supported acquisition. Previously opaque malformed Reviver
records therefore refuse instead of passing through when acquiring Wide Lens;
this deliberate supported-subset narrowing occurs before mutation. Other known
families remain opaque. Unique IDs/capacity, ordinary/nonvirtual caller guarantees,
canonical constructors, active/order preservation and the raw-only boundary are
the same. No Scene tier fallback/update/raw return, selection/purchases,
transfer/cleanup, battle revival/status/Commander/phase/consumption effects or
game-core/save lifecycle is implemented by acquisition. Raw wildcard fixtures do
not claim Scene persistence (missing-holder maxStackCount0/cleanup still apply).

Measured with Node22.23.3/Vitest4.1.11 at8555c08c823b856cbec4eb99ca84ea52a955836d,
using actual factories/classes and `.add(modifiers,false)`, complete inventories
(including order/active), party PokemonData, money, global/battle RNG invariance;
C compares the entire non-inventory context and run unchanged. Zero skipped or
caught exceptions. `REVIVERHELD guards=26 no-mutation/capacity/first-match checks passed`. Supplemental host guards cover capacity256, invalid known
family shape/party IDs/target, full refusal and first matching duplicate.

```text
REVIVERHELD cases=2000 identical=2000 failed=0 skipped=0 thrown=0 full=1600 added=400 stacked=0 wildcard=667 duplicate=800 reordered=1000 fainted=1006
REVIVERHELD cases=2000 identical=2000 failed=0 skipped=0 thrown=0 full=1600 added=400 stacked=0 wildcard=667 duplicate=800 reordered=1000 fainted=1004
REVIVERHELD cases=2000 identical=2000 failed=0 skipped=0 thrown=0 full=1600 added=400 stacked=0 wildcard=667 duplicate=800 reordered=1000 fainted=1005
REVIVERHELD cases=2000 identical=2000 failed=0 skipped=0 thrown=0 full=1600 added=400 stacked=0 wildcard=667 duplicate=800 reordered=1000 fainted=1011
```

Actual C source mutants (seed777; every mutant exits1; missing footer exits2):

```text
REVIVERHELD cases=2000 identical=1963 failed=37 skipped=0 thrown=0 full=1600 added=400 stacked=0 wildcard=667 duplicate=800 reordered=1000 fainted=1006
canonical held wildcard source mutant rejected (exit 1)
REVIVERHELD cases=2000 identical=1545 failed=455 skipped=0 thrown=0 full=1600 added=400 stacked=0 wildcard=667 duplicate=800 reordered=1000 fainted=1006
canonical held first source mutant rejected (exit 1)
REVIVERHELD cases=2000 identical=1545 failed=455 skipped=0 thrown=0 full=1600 added=400 stacked=0 wildcard=667 duplicate=800 reordered=1000 fainted=1006
canonical held cap source mutant rejected (exit 1)
```

Unchanged traces replay before/after baseline `2a63abd`: options3950pass/0fail/
50unported (8052screens), victory4000/0 (2055items/2459screens), application4000/0
(115text cases), Memory4000/0. Mint/vitamin/Lure/money/temp/Soothe each retain
4seeds×2000=8000 identical, with33/31/26/33/25/25 host guards respectively.
All corresponding logs are byte-identical; no original producer/comparator,
seed, coverage rule or generated table changes. Prior new canonical-held traces
are also replayed unchanged in subsequent family extensions.

H passes (`resume regression: 32697 checks passed`; reward_apply641335). The200-run pair is
`without saves: runs' digest: 79d84c65` (saved identical). Full normal/saved1000-run before/after outputs
are byte-identical per mode, including every stub and coverage count:

```text
1000 runs: 1000 ended (0 cleared), 0 stuck, 0 crashed, 0 timed out; 49206 turns (49.2 per run)
final wave:  0-9:125 10-19:708 20-29:156 30-39:10 40-49:1
runs' digest: c2937ca0

stand-ins (runs that used it, times):
  enemy moveset (last 4 level moves)                   1000 runs     14701
  double battle played as single                        762 runs      1544
  evolution condition not checked                         2 runs         3
  biome weather roll                                   1000 runs      1851
  enemy run-core can't generate (stand-in)             1000 runs      2258
  post-battle abilities, Pay Day, lapsing items           3 runs         4
  trainer's name without its title in a message         307 runs       402
  reward taken without its own effect                   139 runs       169
1000 runs: 1000 ended (0 cleared), 0 stuck, 0 crashed, 0 timed out; 49206 turns (49.2 per run)
final wave:  0-9:125 10-19:708 20-29:156 30-39:10 40-49:1
runs' digest: c2937ca0
save/load round trips: 14291 (largest save 11400 bytes of 131072)
```

This API has no runtime caller added, so H digests are retained from the stated
baseline. Host consistency does not establish upstream whole-run parity.

Exact commands (port root; every producer runs serially with one worker):

```sh
. /workspace/shared/activate-parity.sh
# Repeat for777 9001 4242 31337:
BUILD=/workspace/validation/lane-b/ten/reviver-seed/build sh reward-core/tools/reviverhelddiff.sh /workspace/shared/upstream-lane-b 2000 <seed>
# Script runs in upstream, with REVIVER_HELD_N=2000,
# REVIVER_HELD_SEED=<seed>, REVIVER_HELD_OUT=/workspace/validation/lane-b/ten/reviver-seed/build/reviverheld-<seed>.txt:
# node node_modules/vitest/vitest.mjs run --maxWorkers=1 test/tests/zz-dump/reviver-held.producer.test.ts
# Then replays through /workspace/validation/lane-b/ten/reviver-seed/build/reviverheldtest.
BUILD=/workspace/validation/lane-b/ten/reviver-seed/build sh reward-core/tools/reviverheldplant.sh /workspace/validation/lane-b/ten/reviver-seed/build/reviverheld-777.txt
# Remove final Z footer to build/truncated.txt; replay must exit2:
/workspace/validation/lane-b/ten/reviver-seed/build/reviverheldtest /workspace/validation/lane-b/ten/reviver-seed/build/truncated.txt
sh /workspace/validation/lane-b/ten/regressions.sh 2a63abd /workspace/validation/lane-b/ten/reviver-seed 'mint vitamin lure moneyitem tempbooster soothebell wideheld'
sh /workspace/validation/lane-b/ten/full-h.sh /workspace/validation/lane-b/ten/reviver-seed
# full-h builds baseline reward-core extraction against identical read-only cores,
# then both binaries run --auto 1000 --jobs 8 and --auto 1000 --jobs 8 --save-test,
# comparing complete per-mode logs. make -s -C game-core -j2 test is included.
```

External helper sources, before/after logs, fullH and four complete oracle traces
remain at `/workspace/validation/lane-b/ten/reviver-seed`; no traces/upstream source/assets or binaries are committed.
Actual-head host and3DS CI are required before matched-SHA merge; skipped
conditional jobs are not credited. This is a bounded Task96 prerequisite.

## Task 96 prerequisite: canonical Wide Lens acquisition (lane B, 2026-10-08)

`pr_reward_add_canonical_held_item(ctx,item,target)` initially supported Wide
Lens only: the canonical factory's accuracyAmount=5 and maximum held count3.
It models `PersistentModifier.add(modifiers,false)` for one new real stack:
append stack1 or increment the **first** same-family, exact-holder entry. A full
first match returns FULL without trying later duplicate entries. Wildcard
owner-1 entries do not match a concrete holder during acquisition. Other holder
items and inventory order are preserved. FULL is a supported no-effect refusal;
UNSUPPORTED covers malformed inputs, unsupported families and append capacity.
Matching entries can stack/refuse even when all256 inventory slots are occupied.

Party IDs must be unique; target valid; all inventory IDs known. Wide Lens
records require ordinary stacks1..3, owner-1 or a valid party index, arg/count0.
Other known families remain opaque. Existing active flags survive; newly appended
entries use false. All other context, party and run RNG remain unchanged. The
caller guarantees canonical constructors and zero virtual stacks, which the
reward inventory cannot represent. Custom accuracy amounts, virtual/addVirtual
flows, missing/detached holders, multi-stack incoming items and oversize existing
stacks are outside this boundary. Raw wildcard fixtures are not acquired here:
getMaxStackCount is0 without an actual holder, and Scene cleanup would remove
them. Class-level raw inventory parity does not establish wildcard persistence.

This does not replace generic `pr_reward_apply`, port Scene.addModifier's tier
replacement/stat updates/raw return, selection/purchase messages or money,
held-item cleanup/transfer, battle accuracy, game-core transport or saves. Those
callers/effects remain separately owned and checked. Task96 remains partial.

Measured with Node22.23.3/Vitest4.1.11 at8555c08c823b856cbec4eb99ca84ea52a955836d,
using actual factories/classes and `.add(modifiers,false)`, complete inventories
(including order/active), party PokemonData, money, global/battle RNG invariance;
C compares the entire non-inventory context and run unchanged. Zero skipped or
caught exceptions. 25 host checks pass. Supplemental host guards cover capacity256, invalid known
family shape/party IDs/target, full refusal and first matching duplicate.

```text
WIDEHELD cases=2000 identical=2000 failed=0 skipped=0 thrown=0 full=1071 added=400 stacked=529 wildcard=667 duplicate=800 reordered=1000 fainted=990
WIDEHELD cases=2000 identical=2000 failed=0 skipped=0 thrown=0 full=1072 added=400 stacked=528 wildcard=667 duplicate=800 reordered=1000 fainted=1052
WIDEHELD cases=2000 identical=2000 failed=0 skipped=0 thrown=0 full=1074 added=400 stacked=526 wildcard=667 duplicate=800 reordered=1000 fainted=963
WIDEHELD cases=2000 identical=2000 failed=0 skipped=0 thrown=0 full=1068 added=400 stacked=532 wildcard=667 duplicate=800 reordered=1000 fainted=968
```

Actual C source mutants (seed777; every mutant exits1; missing footer exits2):

```text
WIDEHELD cases=2000 identical=1847 failed=153 skipped=0 thrown=0 full=1071 added=400 stacked=529 wildcard=667 duplicate=800 reordered=1000 fainted=990
canonical held wildcard source mutant rejected (exit 1)
WIDEHELD cases=2000 identical=1697 failed=303 skipped=0 thrown=0 full=1071 added=400 stacked=529 wildcard=667 duplicate=800 reordered=1000 fainted=990
canonical held first source mutant rejected (exit 1)
WIDEHELD cases=2000 identical=1927 failed=73 skipped=0 thrown=0 full=1071 added=400 stacked=529 wildcard=667 duplicate=800 reordered=1000 fainted=990
canonical held cap source mutant rejected (exit 1)
```

Unchanged traces replay before/after baseline `432ee53`: options3950pass/0fail/
50unported (8052screens), victory4000/0 (2055items/2459screens), application4000/0
(115text cases), Memory4000/0. Mint/vitamin/Lure/money/temp/Soothe each retain
4seeds×2000=8000 identical, with33/31/26/33/25/25 host guards respectively.
All corresponding logs are byte-identical; no original producer/comparator,
seed, coverage rule or generated table changes. Prior new canonical-held traces
are also replayed unchanged in subsequent family extensions.

H passes (`resume regression: 32697 checks passed`; reward_apply641335). The200-run pair is
`without saves: runs' digest: 79d84c65` (saved identical). Full normal/saved1000-run before/after outputs
are byte-identical per mode, including every stub and coverage count:

```text
1000 runs: 1000 ended (0 cleared), 0 stuck, 0 crashed, 0 timed out; 49206 turns (49.2 per run)
final wave:  0-9:125 10-19:708 20-29:156 30-39:10 40-49:1
runs' digest: c2937ca0

stand-ins (runs that used it, times):
  enemy moveset (last 4 level moves)                   1000 runs     14701
  double battle played as single                        762 runs      1544
  evolution condition not checked                         2 runs         3
  biome weather roll                                   1000 runs      1851
  enemy run-core can't generate (stand-in)             1000 runs      2258
  post-battle abilities, Pay Day, lapsing items           3 runs         4
  trainer's name without its title in a message         307 runs       402
  reward taken without its own effect                   139 runs       169
1000 runs: 1000 ended (0 cleared), 0 stuck, 0 crashed, 0 timed out; 49206 turns (49.2 per run)
final wave:  0-9:125 10-19:708 20-29:156 30-39:10 40-49:1
runs' digest: c2937ca0
save/load round trips: 14291 (largest save 11400 bytes of 131072)
```

This API has no runtime caller added, so H digests are retained from the stated
baseline. Host consistency does not establish upstream whole-run parity.

Exact commands (port root; every producer runs serially with one worker):

```sh
. /workspace/shared/activate-parity.sh
# Repeat for777 9001 4242 31337:
BUILD=/workspace/validation/lane-b/ten/wide-lens/build sh reward-core/tools/widehelddiff.sh /workspace/shared/upstream-lane-b 2000 <seed>
# Script runs in upstream, with WIDE_HELD_N=2000,
# WIDE_HELD_SEED=<seed>, WIDE_HELD_OUT=/workspace/validation/lane-b/ten/wide-lens/build/wideheld-<seed>.txt:
# node node_modules/vitest/vitest.mjs run --maxWorkers=1 test/tests/zz-dump/wide-held.producer.test.ts
# Then replays through /workspace/validation/lane-b/ten/wide-lens/build/wideheldtest.
BUILD=/workspace/validation/lane-b/ten/wide-lens/build sh reward-core/tools/wideheldplant.sh /workspace/validation/lane-b/ten/wide-lens/build/wideheld-777.txt
# Remove final Z footer to build/truncated.txt; replay must exit2:
/workspace/validation/lane-b/ten/wide-lens/build/wideheldtest /workspace/validation/lane-b/ten/wide-lens/build/truncated.txt
sh /workspace/validation/lane-b/ten/regressions.sh 432ee53 /workspace/validation/lane-b/ten/wide-lens 'mint vitamin lure moneyitem tempbooster soothebell'
sh /workspace/validation/lane-b/ten/full-h.sh /workspace/validation/lane-b/ten/wide-lens
# full-h builds baseline reward-core extraction against identical read-only cores,
# then both binaries run --auto 1000 --jobs 8 and --auto 1000 --jobs 8 --save-test,
# comparing complete per-mode logs. make -s -C game-core -j2 test is included.
```

External helper sources, before/after logs, fullH and four complete oracle traces
remain at `/workspace/validation/lane-b/ten/wide-lens`; no traces/upstream source/assets or binaries are committed.
Actual-head host and3DS CI are required before matched-SHA merge; skipped
conditional jobs are not credited. This is a bounded Task96 prerequisite.

## Task 96 prerequisite: Soothe Bell primitives (lane B, 2026-10-08)

`pr_reward_add_soothe_bell(ctx,target)` mirrors real `PersistentModifier.add`:
append a stack-one held Bell or increment the first same-holder Bell to its
three-stack cap. FULL is a supported no-effect refusal; it never uses a later
duplicate's spare capacity. Existing wildcard entries do not match a concrete
holder during acquisition. Addition preserves other inventory order and all
non-inventory context. Invalid arguments/shape/capacity refuse before mutation.

`pr_reward_soothe_bell_gain` models **singular** `Scene.applyModifier`: only the
first eligible target or wildcard Bell applies `floor(base*(1+0.5*stack))`.
It returns the raw amount and optional matched inventory index (-1 if absent),
without changing context, party friendship or RNG. Whole `Pokemon.addFriendship`
is excluded: its nonpositive short circuit, 255/rare-candy caps, starter candy
progress, timed-event/fusion multipliers, achievements and ribbons are not
ported by this helper. Zero base is supported **raw-helper** behavior;
`addFriendship` short-circuits <=0 before invoking any Bell.

Party IDs must be unique, target valid, base finite 0..1000000, inventory IDs
known and Soothe entries nonvirtual stack 1..3, arg/count zero, owner -1 or a
valid party index. Other known entries are opaque. Existing active values are
preserved; new entries use false. Refusals preserve balance/output arguments
and the whole context. This does not port acquisition through Scene.addModifier
(raw return/stat updates/fallback), phase purchases, held cleanup/transfer,
Candy/game-core integration or saves. Existing wildcard raw records are tested
via actual classes, but the API never acquires them: absent holders have
getMaxStackCount 0 and Scene cleanup would remove them. Raw wildcard support is
not a claim that those records survive the wider scene lifecycle. Task 96 stays
partial; existing Candy friendship behavior is not expanded by this PR.

Node 22.23.3/Vitest4.1.11, pin 8555c08c823b856cbec4eb99ca84ea52a955836d.
Real factories/PokemonFriendshipBoosterModifier instances, real `.add`, and
singular scene `.applyModifier` generate before/after gain, matching index,
status and complete inventories. Supplemental real `.applyModifiers` calls
prove singular-vs-all coverage. Upstream party serialization/money/battle/global
RNG invariance and whole non-inventory C context/run invariance are checked.
All records/fields/footer are strict; zero caught/skipped cases/exceptions.
This is held acquisition/raw boost parity, not full friendship or stat transport.

All four seeds 777/9001/4242/31337: 2000/2000 identical each (8000 total), zero
mismatches/skips/throws. Each has FULL 800/ADDED 400/STACKED 800, 667 wildcard
matches, 800 duplicate cases, 335 zero-base cases, 665 fractional bases and 999
cases where singular output differs from applying all Bells. 25 host refusal/
capacity checks pass, plus exact floor/first/wildcard/optional-output checks.
Actual C wildcard-acquisition/all-owned-boost/wrong-cap mutants reject
582/533/122 seed777 cases (exit1). Missing footer exits2.

Established seed99/N4000 traces replay byte-identically against 188f2d7:
options 3950/0/50 unported (8052 screens), victory 4000/0 (2055 items/2459 screens),
application 4000/0 (115 text cases), Memory 4000/0. Existing Mint/vitamin/Lure/
money/temporary four-seed2000 traces each remain 8000/8000 identical with
33/31/26/33/25 guards. All original producers/comparators/seeds/counts and
generated tables are unchanged. Host H passes: resume 32657/apply 641335,
Money 110/vitamin 187/Mint 107, paired 200-run 39213240.

Full H before/after baseline 188f2d7 logs are byte-identical: each normal/saved
1000 ends 1000/clear0/stuck0/crash0/timeout0,49100 turns. All four digests 2473e509,
saved 14271 roundtrips, largest save 11373/131072 bytes. The difference from the
previous temporary API's 403b442f comes from already-merged game-core Money #159,
not this read-only raw Bell API. Nonzero stub rows (runs/times) stay:
movesets1000/14681, doubles761/1543, evolution2/3, weather1000/1847,
ungenerated enemies1000/2258, post-battle3/4, trainer messages307/402,
reward without effect139/169. Zero categories remain zero; no stub removal or
whole-run upstream parity is claimed.

Exact commands, durable evidence outside checkout (old traces explicitly reused):

```sh
. /workspace/shared/activate-parity.sh
E=/workspace/validation/lane-b/soothe-bell
for seed in 777 9001 4242 31337; do
 BUILD="$E/build" sh reward-core/tools/soothebelldiff.sh /workspace/shared/upstream-lane-b 2000 "$seed"
done
sh /workspace/validation/lane-b/run-reward-regressions.sh 188f2d7 "$E" 'mint vitamin lure moneyitem tempbooster'
for seed in 777 9001 4242 31337; do
 "$E/build/soothebelltest" "$E/build/soothebell-$seed.txt"
done
BUILD="$E/build" sh reward-core/tools/soothebellplant.sh "$E/build/soothebell-777.txt"
sed '$d' "$E/build/soothebell-777.txt" > "$E/truncated.txt"
"$E/build/soothebelltest" "$E/truncated.txt" # expected exit2
make -s -C game-core -j2 BUILD="$E/baseline-game" REWARD_CORE="$E/baseline/reward-core" REWARD_LIB="$E/baseline-build/libreward.a" AI_LIB="$PWD/game-core/build/ai-core/libai.a" RUN_LIB="$PWD/game-core/build/run-core/librun.a" CORE_LIB="$PWD/game-core/build/run-core/battle-core/libprcore.a" "$E/baseline-game/gcplay"
"$E/baseline-game/gcplay" --auto 1000 --jobs 8 > "$E/before-normal1000.log"
"$E/baseline-game/gcplay" --auto 1000 --jobs 8 --save-test > "$E/before-saved1000.log"
game-core/build/gcplay --auto 1000 --jobs 8 > "$E/after-normal1000.log"
game-core/build/gcplay --auto 1000 --jobs 8 --save-test > "$E/after-saved1000.log"
cmp "$E/before-normal1000.log" "$E/after-normal1000.log"
cmp "$E/before-saved1000.log" "$E/after-saved1000.log"
```

The retained external regression helper extracts baseline reward-core, builds
with j2 and read-only dependency paths, compares before/after logs for four
established suites plus all 20 previous focused traces, then runs
`make -s -C game-core -j2 test`. Its source and full logs remain in the durable
validation directory. No established producer is rerun/edited by this gate.

## Task 96 prerequisite: canonical temporary boosters (lane B, 2026-10-08)

`pr_reward_add_temp_booster(ctx,item,arg)` and `pr_reward_lapse_temp_boosters`
mirror real `TempStatStageBoosterModifier`/`TempCritBoosterModifier` acquisition
and lapsing at pinned 8555c08. Six X items use stat args ATK..ACC; Dire Hit uses
arg 0. All have canonical 5-battle duration. The first matching stat/class is
refreshed to 5, preserving stack/order and reporting REFRESHED for `se/restore`;
FULL is a supported no-effect refusal. New entries append stack 1. Lapse ticks
and removes only these two families, preserving unrelated modifiers (including
Lure timers). Invalid shape/arguments/capacity refuse without mutation.

Inputs require known item IDs, global owner -1, nonvirtual stack 1..2 and remaining
count 1..5. Callers guarantee canonical maxBattles; `setNewBattleCount` changes
an untransported upstream field and is excluded. Existing active flags are
preserved; new entries use false. Other known inventory entries are opaque.
Scene.addModifier tier fallback/stat refresh/return value, selection/purchases,
full BattleEnd, saves and game-core integration are excluded. Task 96 stays
partial. Battle-core already owns stat/crit effects: this API does not duplicate
or replace those implementations. The oracle compares raw projections through
its existing read-only helpers, including ACC+1, other supported stats+0.2,
Dire Hit+1 per object, ignored stack magnitude, and unchanged HP/EVA probes;
final stat/accuracy/critical-stage pipeline clamps are outside this test.
Focused effect projections fit battle-core's96-object-per-side bound; the
acquisition API independently supports reward-core's256-entry inventory.

Node22.23.3/Vitest4.1.11, upstream 8555c08c823b856cbec4eb99ca84ea52a955836d.
Real constructors/add/scene applyModifiers/lapse/removal provide the oracle;
all intermediate inventory/result/restore-sound/stat/crit fields are replayed,
with strict footer and upstream party/money/battle/global RNG invariance plus
whole non-inventory C context/run invariance. No cases/exceptions caught or
skipped. This is a modifier prerequisite, not party/stat transport parity.

| Seed | Identical/cases | Failed | Duplicate stat/class cases | Stack-two initial objects | Lapse ticks |
| --- | --- | --- | --- | --- | --- |
| 777 | 2000/2000 | 0 | 497 | 4172 | 8013 |
| 9001 | 2000/2000 | 0 | 497 | 4184 | 7992 |
| 4242 | 2000/2000 | 0 | 497 | 4057 | 8028 |
| 31337 | 2000/2000 | 0 | 497 | 4169 | 8007 |

Every seed/family has added/refreshed/full/expired cases. Seed777 counts below
are in ATK/DEF/SPATK/SPDEF/SPD/ACC/Dire Hit order:

- Added 72/72/72/72/72/72/72; refreshed 143/72/143/72/143/71/142.
- Full 71/142/71/142/71/142/71; expired 814/783/648/849/785/830/833.

8000/8000 identical; 25 host guard/capacity checks plus opaque Lure expiry pass.
Actual C refresh/early-expiry/wrong-stat-match mutants fail 72/1763/404 seed777
cases (exit1), and missing footer exits2. Established seed99/N4000 traces replay
byte-identically against base 1c984b7: options 3950/0/50 unported, 8052 screens;
victory 4000/0 (2055 items/2459 screens), application 4000/0 (115 text cases),
Memory 4000/0. Mint/vitamin/Lure/money four-seed2000 traces each stay 8000/8000
identical with 33/31/26/33 guards. Existing producers/comparators/seeds/counts and
generated tables remain unchanged. H passes with paired200-run digest 803ad8cf.

Exact commands/evidence outside checkout (prior traces explicitly reused):

```sh
. /workspace/shared/activate-parity.sh
E=/workspace/validation/lane-b/temp-boosters
for seed in 777 9001 4242 31337; do
 BUILD="$E/build" sh reward-core/tools/tempboosterdiff.sh /workspace/shared/upstream-lane-b 2000 "$seed"
done
BUILD="$E/build" sh reward-core/tools/tempboosterplant.sh "$E/build/tempbooster-777.txt"
sed '$d' "$E/build/tempbooster-777.txt" > "$E/truncated.txt"
"$E/build/tempboostertest" "$E/truncated.txt" # expected exit2
sh /workspace/validation/lane-b/run-reward-regressions.sh 1c984b7 "$E"
```

The durable regression helper extracts baseline reward-core outside the checkout,
builds both libraries with j2 and read-only battle/run paths, compares before/after
logs for reward/victory/apply/Memory and all 16 focused traces, then runs
`make -s -C game-core -j2 test`. Its full source and all before/after logs are
retained beside evidence; no existing producer is rerun or modified in that gate.

Full H contract also passes before/after against extracted reward baseline1c984b7:
1000 normal +1000 saved each end1000/clear0/stuck0/crash0/timeout0,49284 turns;
all four digests403b442f,14302 saved round trips,largest save11582/131072 bytes.
Before/after full logs are byte-identical. Nonzero stub counts (runs/times):
enemy movesets1000/14713, doubles-as-single763/1565, evolution2/3,
biome weather1000/1850, ungenerated enemies1000/2269, post-battle3/4,
trainer message306/400, reward-without-effect371/467. Zero stub categories
remain zero; this API creates no game-core caller and does not remove stubs.

```sh
E=/workspace/validation/lane-b/temp-boosters
make -s -C game-core -j2 BUILD="$E/baseline-game" REWARD_CORE="$E/baseline/reward-core" REWARD_LIB="$E/baseline-build/libreward.a" AI_LIB="$PWD/game-core/build/ai-core/libai.a" RUN_LIB="$PWD/game-core/build/run-core/librun.a" CORE_LIB="$PWD/game-core/build/run-core/battle-core/libprcore.a" "$E/baseline-game/gcplay"
"$E/baseline-game/gcplay" --auto 1000 --jobs 8 > "$E/before-normal1000.log"
"$E/baseline-game/gcplay" --auto 1000 --jobs 8 --save-test > "$E/before-saved1000.log"
game-core/build/gcplay --auto 1000 --jobs 8 > "$E/after-normal1000.log"
game-core/build/gcplay --auto 1000 --jobs 8 --save-test > "$E/after-saved1000.log"
cmp "$E/before-normal1000.log" "$E/after-normal1000.log"
cmp "$E/before-saved1000.log" "$E/after-saved1000.log"
```

One initial baseline link invocation used nonexistent cached
`game-core/build/battle-core/libprcore.a` (exit2); correcting the actual nested
run-core/battle-core path above succeeds. The failed log is retained and is
not counted as validation. Header descriptions were corrected before final CI;
no runtime source or established check changed for that documentation update.

## Task 96 prerequisite: money-item monetary application (lane B, 2026-10-08)

`pr_reward_apply_money_item(ctx, item, &money, &result)` mirrors the monetary
state of pinned `MoneyRewardModifier.apply` for **Nugget / Big Nugget / Relic
Gold**, using multipliers **1 / 2.5 / 10**. It reads run-core's wave-money
calculator, applies every Amulet Coin object **in inventory order** with its
own floored 20%-per-stack bonus, then caps the balance at
**9007199254740991** (`Number.MAX_SAFE_INTEGER`). It does not apply Happy Hour.
The optional result distinguishes the nominal amount from actual credited
money. Success remains true at the cap even when credit is zero, matching
upstream. Context, party, inventory and RNG remain unchanged.

The bounded input contract is wave **1..10000**, party **1..6** with known
primary/fusion species, finite money **0..MAX_SAFE_INTEGER** (fractional
balances allowed), and ordinary nonvirtual inventories. Amulet entries require
owner -1, arg/count zero, stack **1..5**; duplicate objects are supported.
Other known-item entries are opaque. Invalid shapes, money, capacity or species
and **primary/fused Gimmighoul anywhere in the party** are refused before any
context, balance or result mutation. Gholdengo is supported: upstream checks
Gimmighoul's exact species ID rather than its evolution family.

This is monetary-state parity, not the complete item/phase lifecycle. Real
Gimmighoul application additionally creates an EvoTracker and triggers scene
modifier updates; that path remains unsupported. Rendering, MoneyAchv unlocks,
selection/purchase/affordability, scene-wide ModifierAchv checks, saves and
game-core integration remain caller-owned and unvalidated. The upstream
producer counts the real `Scene.addMoney` requests for UI update, positive
animation and MoneyAchv validation (one each), intercepting those boundaries
rather than claiming their implementation. Generic `pr_reward_apply` has no
money transport and is unchanged. Task 96 remains partial.

Node22.23.3/Vitest4.1.11, upstream
`8555c08c823b856cbec4eb99ca84ea52a955836d`: the oracle uses real factories,
`MoneyRewardModifier.apply`, scene wave-money calculation, real Amulet class
application and real `Scene.addMoney` clamp. Every produced record/field is
consumed strictly. Upstream party serialization, inventory, battle/global RNG
invariance and C whole-context/run invariance are checked. This is not a
party/stat transport differential. Each family includes waves 1 and 10000 plus
wave-set boundaries, capped/fractional balances and coin combinations.

| Seed | Identical / cases | Mismatches | Capped balances | Fusion mons | Gholdengo mons | Coin-order-sensitive cases |
| --- | --- | --- | --- | --- | --- | --- |
| 777 | 2000 / 2000 | 0 | 1201 | 3508 | 2000 | 473 |
| 9001 | 2000 / 2000 | 0 | 1189 | 3494 | 1945 | 474 |
| 4242 | 2000 / 2000 | 0 | 1178 | 3414 | 1957 | 491 |
| 31337 | 2000 / 2000 | 0 | 1189 | 3447 | 1910 | 502 |

Each seed covers **667/667/666** Nugget/Big/Relic cases, **1332** cases with
multiple coin objects and **333** fractional starting balances. **8000/8000
identical**, zero skipped/thrown. **33** host no-mutation refusal guards pass,
plus cap success, optional result, fractional credit and a full inventory of 256 coin objects with finite large amount. Actual C-source multiplier/reversed-order/
missing-cap mutations reject **667/473/410** seed777 cases (exit1); missing
footer exits2. Unsupported Gimmighoul has host refusal proof, not an upstream
successful-application differential or an omitted-effect success.

Unchanged established seed99/N4000 traces replay byte-identically against main
`0a10696` and this API: options **3950/0/50 unported**, 8052 screens; victory
**4000/0**, 2055 items/2459 screens; application **4000/0**, 115 text cases;
Memory **4000/0**. Existing Mint, vitamin and Lure four-seed2000 traces each
retain **8000/8000 identical**, with **33/31/26** guards. All established
producers/comparators/seeds/counts and generated tables remain unchanged.
Host `make -s -C game-core -j2 test` passes: resume **32729**, reward
application **641335**, Mint **107**, paired200-run digest **a6662b64**.

Exact commands (durable evidence outside the checkout; existing traces are
replayed, not represented as newly generated):

```sh
. /workspace/shared/activate-parity.sh
E=/workspace/validation/lane-b/money-items
mkdir -p "$E/baseline"
git archive 0a10696 reward-core | tar -x -C "$E/baseline"
for seed in 777 9001 4242 31337; do
  BUILD="$E/build" sh reward-core/tools/moneyitemdiff.sh /workspace/shared/upstream-lane-b 2000 "$seed"
done
make -s -j2 -C "$E/baseline/reward-core" BUILD="$E/baseline-build" BATTLE_CORE="$PWD/battle-core" RUN_CORE="$PWD/run-core" CORE_LIB="$E/build/battle-core/libprcore.a" RUN_LIB="$E/build/run-core/librun.a"
for mode in rewards victory apply memory; do
  TRACE=/workspace/validation/lane-b/vitamins/baseline-build/$mode-cases.txt
  "$E/baseline-build/${mode}test" "$TRACE" > "$E/before-$mode.log"
  "$E/build/${mode}test" "$TRACE" > "$E/after-$mode.log"
  cmp "$E/before-$mode.log" "$E/after-$mode.log"
done
for seed in 777 9001 4242 31337; do
  for type in mint vitamin lure; do
    case "$type" in
      mint) TRACE=/workspace/validation/lane-b/vitamins/mint-regression/mint-$seed.txt;;
      vitamin) TRACE=/workspace/validation/lane-b/vitamins/build/vitamin-$seed.txt;;
      lure) TRACE=/workspace/validation/lane-b/lures/build/lure-$seed.txt;;
    esac
    "$E/baseline-build/${type}test" "$TRACE" > "$E/before-$type-$seed.log"
    "$E/build/${type}test" "$TRACE" > "$E/after-$type-$seed.log"
    cmp "$E/before-$type-$seed.log" "$E/after-$type-$seed.log"
  done
done
BUILD="$E/build" sh reward-core/tools/moneyitemplant.sh "$E/build/moneyitem-777.txt"
sed '$d' "$E/build/moneyitem-777.txt" > "$E/truncated.txt"
"$E/build/moneyitemtest" "$E/truncated.txt" # expected exit2
make -s -C game-core -j2 test
```

## Task 96 prerequisite: canonical Lure primitives (lane B, 2026-10-07)

`pr_reward_add_lure`, `pr_reward_lapse_lures` and `pr_reward_lure_chance`
mirror pinned `LapsingPersistentModifier.add/lapse` and
`DoubleBattleChanceBoosterModifier.apply` for Lure/Super Lure/Max Lure's
canonical **10/15/30** battle durations. Addition appends a new stack-one
entry, refreshes the **first** matching duration to its original maximum, or
returns FULL without mutation. Refresh preserves stack and list order and
requests `se/restore` through its distinct REFRESHED status. Duplicate objects
retain upstream's first-match behavior. Every object divides the raw chance
denominator by four, even with stack two. Each lapse ticks **Lures only** and
removes expired objects while preserving the remaining inventory order.

This is a modifier prerequisite, not the `pr_reward_apply` or
`BattleScene.addModifier` boundary. Full-item tier fallback, scene stat refresh,
purchases/selection phases, complete BattleEnd processing, other lapsing
families, game-core/save integration, wave/ability chance rules, final minimum-one
clamp and RNG rolls are excluded. Task 96 remains partial. The caller guarantees
canonical maximum durations: `setNewBattleCount` changes an upstream field not
transported here and is unsupported even if its current count fits these bounds.
Lures require owner -1, arg zero, nonvirtual stack 1..2 and remaining count
1..maximum. Unsupported shapes/capacity leave context and output arguments
unchanged. FULL is a supported no-effect result. Unrelated known-item entries
are opaque, so this API never validates their holder/stat semantics or updates
party stats. Lure `active` has no upstream counterpart: existing values are
preserved and new entries use false. Chance accepts finite nonnegative values.

Node22.23.3/Vitest4.1.11, upstream
`8555c08c823b856cbec4eb99ca84ea52a955836d`: real factory constructors, modifier
`add`, scene `applyModifiers`, real `lapse` and scene removal provide the oracle.
Every intermediate inventory, result, refresh sound and denominator is replayed
strictly; upstream party serialization/money/battle/global RNG invariance and C
whole non-inventory context/run invariance are checked. No exceptions or cases
are caught/skipped. These are modifier-state comparisons, not a party/stat
transport differential or full run lifecycle test.

| Seed | Identical / cases | Mismatches | Expired Lure / Super / Max | Stack-two initial objects | Lapse ticks |
| --- | --- | --- | --- | --- | --- |
| 777 | 2000 / 2000 | 0 | 1218 / 1115 / 604 | 1921 | 35611 |
| 9001 | 2000 / 2000 | 0 | 1197 / 1123 / 619 | 1921 | 35565 |
| 4242 | 2000 / 2000 | 0 | 1208 / 1108 / 629 | 2086 | 36111 |
| 31337 | 2000 / 2000 | 0 | 1223 / 1096 / 587 | 1928 | 35309 |

Each seed has added **167/167/167**, refreshed **333/167/333**, full
**167/333/166**, and **498** duplicate-duration cases. **8000/8000 identical**,
zero skipped/thrown; **26** host guard/capacity checks plus opaque-item and
stack/expiry checks pass. Actual C-source refresh/early-expiry/chance mutations
reject **167/1557/1114** seed777 cases (exit1); missing footer exits2.

The unchanged established seed99/N4000 traces from the vitamin task replay
byte-identically against merged base `ab94a380` and this API: options
**3950/0/50 unported**, 8052 screens; victory **4000/0**, 2055 items/2459
screens; application **4000/0**, 115 text cases; Memory **4000/0**. Existing
Mint and vitamin four-seed2000 traces each retain **8000/8000 identical**, with
**33/31** guards. No established producer/comparator/seed/count is changed.

Exact commands (evidence outside the checkout; baseline traces reused rather
than represented as newly generated):

```sh
. /workspace/shared/activate-parity.sh
E=/workspace/validation/lane-b/lures
mkdir -p "$E/baseline"
git archive ab94a38022313493b5b1414d931740042da646eb reward-core | tar -x -C "$E/baseline"
for seed in 777 9001 4242 31337; do
  BUILD="$E/build" sh reward-core/tools/lurediff.sh /workspace/shared/upstream-lane-b 2000 "$seed"
done
make -s -j2 -C reward-core BUILD="$E/build"
make -s -j2 -C "$E/baseline/reward-core" BUILD="$E/baseline-build" BATTLE_CORE="$PWD/battle-core" RUN_CORE="$PWD/run-core" CORE_LIB="$E/build/battle-core/libprcore.a" RUN_LIB="$E/build/run-core/librun.a"
for mode in rewards victory apply memory; do
  TRACE=/workspace/validation/lane-b/vitamins/baseline-build/$mode-cases.txt
  "$E/baseline-build/${mode}test" "$TRACE" > "$E/before-$mode.log"
  "$E/build/${mode}test" "$TRACE" > "$E/after-$mode.log"
  cmp "$E/before-$mode.log" "$E/after-$mode.log"
done
for seed in 777 9001 4242 31337; do
  for type in mint vitamin; do
    case "$type" in
      mint) TRACE=/workspace/validation/lane-b/vitamins/mint-regression/mint-$seed.txt;;
      vitamin) TRACE=/workspace/validation/lane-b/vitamins/build/vitamin-$seed.txt;;
    esac
    "$E/baseline-build/${type}test" "$TRACE" > "$E/before-$type-$seed.log"
    "$E/build/${type}test" "$TRACE" > "$E/after-$type-$seed.log"
    cmp "$E/before-$type-$seed.log" "$E/after-$type-$seed.log"
  done
done
BUILD="$E/build" sh reward-core/tools/lureplant.sh "$E/build/lure-777.txt"
sed '$d' "$E/build/lure-777.txt" > "$E/truncated.txt"
"$E/build/luretest" "$E/truncated.txt" # expected exit2
make -s -C game-core -j2 test
```

## Task 96 prerequisite: bounded vitamins (lane B, 2026-10-07)

`pr_reward_apply_vitamin` implements real `BaseStatBoosterModifierType` selection,
`PersistentModifier.add` and `BattleScene.updateModifiers` at pinned `8555c08`.
All six vitamins use the selected holder's IV as their stack cap. IV zero is
inoperable; an existing matching stack exactly at the IV cap is rejected. An
accepted application adds or increments the holder/stat entry in original list
order, then refreshes **every** party member's stats/HP through battle-core's
read-only calculator. Fusion, effective nature and permanent/passive Wonder
Guard are included. Base/custom nature, IVs and the other transported party
fields remain unchanged. HP Up preserves absolute damage, keeps a full target
full and does not revive a fainted target.

The detailed API separates inventory addition from upstream's raw return:
**pinned `BattleScene.addModifier` returns false for an ordinary persistent
vitamin even when it adds the item**. Real `SelectModifierPhase.applyModifier`
therefore reports a purchase error, leaves money unchanged and keeps the screen
open for cost >= 0; a free reward (cost -1) ends normally. Result flags preserve
this behavior. Generic `pr_reward_apply(...BASE_STAT_BOOSTER...)` applies a free
reward and returns whether its item was added. Shop offerings, affordability,
rendering, saves and game-core integration are caller-owned and unvalidated;
this does not claim that the normal shop offers vitamins.

Inputs must be ordinary, nonvirtual, positive persistent inventories with valid
holder indices and unique holder/stat vitamin entries at or below the IV cap.
The API refuses malformed inputs, full capacity, levels outside 1..255, Flip
Stat, unfused Spliced Endless, wildcard held ownership, Evolution Tracker
cleanup, and Soul Dew/Shuckle Juice/Old Gateau/Macho Brace inventories before
context mutation. Other custom species/stat overrides remain unsupported.
Mint's existing five stat-inventory guards remain in place: this does not expand
Mint or game-core stat-item support, or complete Task 96.

The additive `vitamin.producer.test.ts` invokes real selection, construction,
persistent addition, stat refresh and the private purchase/free application
method. Only UI error and phase-end boundaries are recorded; no application or
stat method is replaced. `vitamintest` strictly compares 65 named numeric party
fields, the entire test inventory, status/result flags, unchanged money and RNG.
Unexpected queues/exceptions fail; version/count/order/footer are mandatory.
Test inventories include vitamins on other holders/stats, Leftovers and Healing
Charm. Status-object counters, other caller fields, virtual/over-cap inventories
and the surrounding UI/run lifecycle are outside this bounded transport.

Node22.23.3/Vitest4.1.11, upstream `8555c08c823b856cbec4eb99ca84ea52a955836d`:

| Seed | Identical / cases | Mismatches | Fusion | Fainted | Custom nature | Nonbase forms | Wonder Guard |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 777 | 2000 / 2000 | 0 | 684 | 555 | 1018 | 960 | 371 |
| 9001 | 2000 / 2000 | 0 | 624 | 543 | 985 | 962 | 321 |
| 4242 | 2000 / 2000 | 0 | 658 | 554 | 1013 | 944 | 309 |
| 31337 | 2000 / 2000 | 0 | 659 | 540 | 1024 | 969 | 321 |

Each seed exercises all six vitamins: applied 200/201/201/200/199/199,
inoperable 67/66/66/67/67/67, full 67/67/66/66/67/67, with **799** added paid
items producing the upstream error/no deduction and **401** free rewards ending.
Each stat has both paid and free accepted applications. **8000/8000 identical**,
zero skipped/thrown; random levels are 1..100 (higher supported levels remain
undiffed here). **31** host refusal checks preserve the whole context; partial,
full and fainted HP behavior and detailed/generic application checks pass.
An actual C-source cap=31 mutation fails **776/2000** seed777 cases (exit1).
Ownership/money/stat boundary mutations fail **1200/799/2000** cases (exit1);
an absent footer exits2.

Fresh established seed99/N4000 traces at baseline `9cd7a03` replay byte-identically
before/after: options **3950/0/50 unported**, 8052 screens; victory **4000/0**,
2055 items/2459 screens; application **4000/0**, 115 with text; Memory **4000/0**.
Unchanged Mint suite: four seeds x2000, **8000/8000 identical**, 33 guards pass.
Host `make -s -C game-core -j2 test` passes with paired 200-run digests
**a6662b64** and no reward/run lifecycle claim beyond that gate.

Exact commands (persistent evidence outside the checkout):

```sh
. /workspace/shared/activate-parity.sh
E=/workspace/validation/lane-b/vitamins
mkdir -p "$E/baseline"
git archive 9cd7a03 reward-core | tar -x -C "$E/baseline"
export BATTLE_CORE="$PWD/battle-core" RUN_CORE="$PWD/run-core"
BUILD="$E/baseline-build" sh "$E/baseline/reward-core/tools/difftest.sh" /workspace/shared/upstream-lane-b 4000 99 all
for seed in 777 9001 4242 31337; do
  BUILD="$E/build" sh reward-core/tools/vitamindiff.sh /workspace/shared/upstream-lane-b 2000 "$seed"
  BUILD="$E/mint-regression" sh reward-core/tools/mintdiff.sh /workspace/shared/upstream-lane-b 2000 "$seed"
done
make -s -C reward-core -j2
reward-core/build/vitamintest --selftest
BUILD="$E/build" sh reward-core/tools/vitaminplant.sh "$E/build/vitamin-777.txt"
for mutation in owner money stats; do
  reward-core/build/vitamintest "$E/build/vitamin-777.txt" --mutate-$mutation # expected exit1
done
sed '$d' "$E/build/vitamin-777.txt" > "$E/truncated.txt"
reward-core/build/vitamintest "$E/truncated.txt" # expected exit2
for mode in rewards victory apply memory; do
  "$E/baseline-build/${mode}test" "$E/baseline-build/${mode}-cases.txt" > "$E/before-${mode}.log"
  reward-core/build/${mode}test "$E/baseline-build/${mode}-cases.txt" > "$E/after-${mode}.log"
  cmp "$E/before-${mode}.log" "$E/after-${mode}.log"
done
make -s -C game-core -j2 test
```

## Task 96 prerequisite: bounded Nature Mint application (lane B, 2026-10-07)

`pr_reward_apply_mint(ctx, nature, party_idx)` and the `PR_ITEM_MINT` path of
`pr_reward_apply` mirror pinned upstream `PokemonNatureChangeModifierType`'s
selection filter and `PokemonNatureChangeModifier.apply`. A different nature
sets **custom** nature, recalculates six permanent stats and preserves HP using
the upstream max-HP adjustment rules; fainting does not forbid a Mint. Base
nature remains unchanged. Callers must supply `has_explicit_nature`,
`base_nature`, `custom_nature` (-1 means absent), and the consistent effective
`nature`, plus real species/forms, IVs, level, current stats/HP, permanent
ability/passive and inventory. battle-core's stat calculator is used read-only.

Successful applications retain a `{species, nature}` request in
`nature_unlock_events` (primary species, not fusion). The caller must execute
`GameData.unlockSpeciesNature`'s policy: only an already unlocked root permits
the nature bit to be applied to that exact species and every prevolution. The
caller consumes requests in order, clears the count and persists dex/custom
nature. The eight-request capacity refuses a ninth application before mutation.
This API records the call boundary; it does not implement or validate that dex
policy, save format or game-core adapter.

The supported subset includes all 25 natures, ordinary and fused species/forms,
existing custom natures, levels **1..255** (the battle-core calculator's level
field is eight bits), and permanent/passive Wonder Guard. Refusals mean two
different things: matching the effective nature is upstream's **no effect**;
missing/invalid caller inputs or an unsupported stat configuration are an
**unsupported application**, which an adapter must keep counted rather than
crediting as applied. Target-owned or wildcard (owner -1) vitamins, Soul Dew, Shuckle Juice, Old Gateau
and Macho Brace are refused because their permanent-stat effects are not
transported here. Flip Stat and unfused Spliced Endless are also refused.
Other holders' stat items do not affect the selected target. Custom species/stat
overrides, broader stat-item inventories and full reward/run lifecycle parity
remain outside this contract. No RNG is consumed.

The additive `tests/mint.producer.test.ts` runs real selection, construction,
`scene.addModifier`, `setCustomNature`, `calculateStats` and
`unlockSpeciesNature`; a wrapper records the unlock call and still calls its
original implementation. Unexpected queues/exceptions fail the test. The new
strict comparator `build/minttest` compares 65 named numeric party fields
(including base/custom/effective nature, IVs, stats, HP/status, species/forms,
abilities and move/PP data), all test inventory entries (Leftovers and Healing
Charm), the request and global RNG. The producer asserts battle RNG is unchanged.
Status-object counters, untransported caller fields and caller dex results are
not part of these 65 fields. Version, count, ordering and completion footer are
mandatory; no skipped or caught/thrown cases are accepted.
The random differential cases use levels 1..100; higher supported levels have
not been checked against this upstream producer.

Node **22.23.3**, Vitest **4.1.11**, upstream
`8555c08c823b856cbec4eb99ca84ea52a955836d`:

| Seed | Identical / cases | Mismatches | Fusion | Fainted | Existing custom nature | Non-base forms | Wonder Guard |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 777 | 2000 / 2000 | 0 | 674 | 164 | 963 | 964 | 312 |
| 9001 | 2000 / 2000 | 0 | 647 | 196 | 987 | 952 | 330 |
| 4242 | 2000 / 2000 | 0 | 645 | 189 | 975 | 932 | 339 |
| 31337 | 2000 / 2000 | 0 | 683 | 195 | 959 | 944 | 347 |

Each nature has 60 accepted and 20 rejected selections per seed: 8000/8000
identical overall, zero skipped/thrown. Separate host guards verify **33**
refusals preserve the entire context, including invalid metadata/bounds,
unsupported stats, levels 256/65535 and queue exhaustion; capacity, fainted application, base
nature and HP checks pass. These host refusal checks are distinct from upstream
selection comparisons. A real C-source mutant overwriting base nature fails
1480/2000 seed-777 cases (exit 1). Boundary base/stat mutations fail 2000/2000;
wrong unlock species fails 1500/2000. An absent completion footer exits 2.

Established Node-22 seed-99/N=4000 traces replayed before and after with
byte-identical output: reward options **3950 pass / 0 fail / 50 unported**, 8052
screens; victory **4000 pass / 0 fail**, 2055 items / 2459 reward screens;
application **4000 pass / 0 fail**, 115 cases with text compared; Memory **4000
cases / 0 mismatches**. Established producers, comparators, seeds and counts
are unchanged; the 50 Moveset Randomizer cases remain unported.

The separate established evolution-state, dual-learnset and trainer-reward
producers were also regenerated at N=2000 on all four seeds. Before/after
comparisons are byte-identical: evolution **8000/8000 identical**, dual inputs
**8000/8000 identical** (24,000 TM/RNG comparisons), trainers **8000 cases / 0
mismatches**. Trainer F/V/G counts are 808/598/594, 811/614/575, 842/577/581 and
830/567/603 respectively; the intentionally tested upstream short-party throws
remain coverage, not skipped cases. `make -s -C game-core -j2 test` passes,
including **32725** resume checks, **641335** reward-application checks and
identical 200-run save/no-save digests **a6662b64**. This does not credit game-core
with applying Mints; its integration is a separate lane-C task.

Commands used (from the port unless a subshell changes directory; dependencies
are in the external pinned harness, never committed):

```sh
. /workspace/shared/activate-parity.sh
make -s -C reward-core -j2
cp reward-core/tests/mint.producer.test.ts /workspace/shared/upstream-lane-b/test/tests/zz-dump/
(cd /workspace/shared/upstream-lane-b && MINT_N=2000 MINT_SEED=777 MINT_OUT=/tmp/mint-777.txt node node_modules/vitest/vitest.mjs run --maxWorkers=1 test/tests/zz-dump/mint.producer.test.ts)
reward-core/build/minttest /tmp/mint-777.txt
for seed in 9001 4242 31337; do
  BUILD=/tmp/mint-check sh reward-core/tools/mintdiff.sh /workspace/shared/upstream-lane-b 2000 "$seed"
done
reward-core/build/minttest --selftest
BUILD=/tmp/mint-check sh reward-core/tools/mintplant.sh /tmp/mint-777.txt
for mutation in base unlock stats; do
  reward-core/build/minttest /tmp/mint-777.txt --mutate-$mutation # expected exit 1
done
sed '$d' /tmp/mint-777.txt > /tmp/mint-truncated.txt
reward-core/build/minttest /tmp/mint-truncated.txt # expected exit 2
# Original reward-core source extracted from c7e54c3 for the baseline:
mkdir -p /tmp/mint-baseline
git archive c7e54c3 reward-core | tar -x -C /tmp/mint-baseline
export BATTLE_CORE="$PWD/battle-core" RUN_CORE="$PWD/run-core"
BUILD=/tmp/mint-baseline-build sh /tmp/mint-baseline/reward-core/tools/difftest.sh /workspace/shared/upstream-lane-b 4000 99 all
for mode in rewards victory apply memory; do
  /tmp/mint-baseline-build/${mode}test /tmp/mint-baseline-build/${mode}-cases.txt > /tmp/mint-before-${mode}.log
  reward-core/build/${mode}test /tmp/mint-baseline-build/${mode}-cases.txt > /tmp/mint-after-${mode}.log
  cmp /tmp/mint-before-${mode}.log /tmp/mint-after-${mode}.log
done
# Established prerequisite suites: generate once, replay with both versions.
for suite in evolution-state learnset-inputs trainer-rewards; do
  cp "reward-core/tests/$suite.producer.test.ts" /workspace/shared/upstream-lane-b/test/tests/zz-dump/
done
for seed in 777 9001 4242 31337; do
  for suite in evolution-state learnset-inputs trainer-rewards; do
    trace=/tmp/mint-regression-${suite}-${seed}.txt
    (cd /workspace/shared/upstream-lane-b && EVO_N=2000 EVO_SEED="$seed" EVO_OUT="$trace" DIFF_N=2000 DIFF_SEED="$seed" DIFF_OUT="$trace" node node_modules/vitest/vitest.mjs run --maxWorkers=1 "test/tests/zz-dump/$suite.producer.test.ts")
    case "$suite" in evolution-state) executable=evolutionstatetest;; learnset-inputs) executable=learnsetinputtest;; trainer-rewards) executable=trainerrewardstest;; esac
    /tmp/mint-baseline-build/$executable "$trace" > /tmp/mint-before-${suite}-${seed}.txt
    reward-core/build/$executable "$trace" > /tmp/mint-after-${suite}-${seed}.txt
    cmp /tmp/mint-before-${suite}-${seed}.txt /tmp/mint-after-${suite}-${seed}.txt
  done
done
make -s -C game-core -j2 test
```


## Task 89: evolution application state (2026-10-04)

Checked against upstream `8555c08c823b856cbec4eb99ca84ea52a955836d` on
2026-10-03, port base `cdac97d859cd39ef96200570287efb28b77bc287`, Node 22.23.3 /
GCC 13.3.0. The original regression blockers were repaired with the user's authorization; current results below use Node 24.19.0 and preserve the existing assertions.

Evolution items now retain `pr_reward_ctx.evolution_events` rather than returning
success without the upstream `EvolutionPhase` request. Events carry Pokemon ID,
primary/target species, fusion status, item, pre/evolution form keys, evolution
level and last level. Entries follow enqueue order (`unshiftNew` calls); the
caller consumes them in reverse order, clears consumed entries, and resolves the
Pokemon by ID. An eight-entry capacity reports overflow and refuses the ninth
application without overwriting requests. Newly transported IVs, six stats,
ability indices, shiny/variant/luck and fusion attributes remain unchanged at
this application boundary. Regeneration adds the upstream evolution form key to
the generated evolution rows; no generated file was hand-edited.

Explicit item selection/application follows upstream validation even when
`pauseEvolutions` is set; that flag filters offered item generation, which is
unchanged. Fusion gender conditions read fusionGender during application.
**Success means queued, not evolved.** game-core still needs to consume the
request and perform EvolutionPhase/evolve, ability/form/stat/move updates,
dex/starter bookkeeping and save/resume. Form-change item execution and DNA
Splicers full-state lifecycle are not validated by this slice.

The separate producer `tests/evolution-state.producer.test.ts` calls real
EvolutionItemModifierType selection, modifier construction and `scene.addModifier`.
Only the queue boundary is intercepted; any other queued phase or exception
fails. Its C comparator is `build/evolutionstatetest`. Each record compares all
63 named numeric party fields (including species/forms, fusion metadata, ability,
IVs, stats, HP/status, EXP, nature, Tera type, friendship and move/PP data), the
entire test inventory (Leftovers and Healing Charm, including ownership), global
seeded RNG and every evolution request. Upstream battle RNG is asserted unchanged.
Version/count/footer and record ordering are strict: missing data cannot pass.
Custom Pokemon overrides, status-object counters, broader inventories and actual
phase execution are outside this bounded application-stage contract.

Four stones are exercised: Fire/Vulpix, Water/Shellder, Thunder/Pikachu and
Dawn/Snorunt, both primary and fusion evolutions, including gender rejection.

| Seed | Cases / identical | Mismatches | Accepted (Fire/Water/Thunder/Dawn) | Rejected | Fusion requests |
|---|---|---|---|---|---|
| 777 | 2000 / 2000 | 0 | 227/259/255/85 | 273/241/245/415 | 266 |
| 9001 | 2000 / 2000 | 0 | 248/267/260/79 | 252/233/240/421 | 313 |
| 4242 | 2000 / 2000 | 0 | 256/247/230/80 | 244/253/270/420 | 276 |
| 31337 | 2000 / 2000 | 0 | 255/258/272/66 | 245/242/228/434 | 287 |

Total 8000/8000 identical, zero skipped/thrown. Every stone succeeds and rejects
on every seed. Unchanged production reward_apply.c replayed through the new
transport/comparator fails 847/868/831/880 cases respectively; this baseline does
not claim that the old comparator checked these fields. Species and ability
mutations at the C application boundary each yield 2000/2000 mismatches (exit 1).
Deleting the completion footer yields exit 2. Queue capacity/overflow checks pass.


## Task 89 validation repair (2026-10-04)

The user authorized repairing established inputs without removing assertions. Fusion now uses actual form/IV/nature data through battle-core's stat calculator, assigns HP from the upstream pre-fusion maximum and clamps after modifier refresh. The application producer records the real unseeded status-helper draw; it does not replace randomness with a fixed result. C requires that input when both valid statuses can be selected and refuses missing/invalid input before mutation. A sprite-only mock prevents destroyed display fixtures from throwing; upstream item/selection/stat/status logic still runs and any thrown case fails the producer. Form-item selection matches upstream item-trigger lookup without incorrectly requiring phase activation conditions. Hardcore rejects a fainted primary. Memory imports ValueHolder from its actual module and preserves duplicate used-TM history as upstream does.

Node24.19.0/Vitest4.1.11, GCC13.3, upstream `8555c08`: established application **4000/4000 seed99**, plus **2000/2000 each seeds777/9001/4242/31337**, zero failures/thrown. The existing comparisons remain HP/status/PP/results; they do not establish complete fusion state. The separate bounded evolution suite still compares63 named party fields, inventory/RNG/queues: **8000/8000 identical**, zero mismatches/skipped/thrown. Species/ability mutations each fail2000/2000; flipped recorded status draws fail6/4000; missing draw/stat inputs and incomplete evolution footer exit2. Missing/invalid caller status-choice checks preserve the original context and both real choices pass.

The complete established `difftest.sh <upstream> 4000 99 all` now exits0: options3950pass/0fail/50unported, victory4000pass/0fail, apply4000pass/0fail, Memory4000cases/0mismatches. Host test exits0 with paired200-run digests `d1eb301b`/`d1eb301b`. CI must pass on the updated head before ready-for-review transition. Evolution success still means queued; game-core must execute evolution and persist lifecycle state. Custom/stat-modifying held inventories, complete DNA Splicers state/phase/transfer lifecycle, form-change phase execution, upstream whole-run and hardware parity remain unvalidated. No missing differential is credited as passing.

Exact commands (from port unless stated otherwise):

```sh
PATH=/root/.npm/_npx/52027bd8fc0022aa/node_modules/node/bin:$PATH sh reward-core/tools/redump.sh /tmp/pokerogue-upstream
make -s -C reward-core
cp reward-core/tests/evolution-state.producer.test.ts /tmp/pokerogue-upstream/test/tests/zz-dump/
# In /tmp/pokerogue-upstream, for each seed 777, 9001, 4242, 31337:
EVO_N=2000 EVO_SEED=<seed> EVO_OUT=/tmp/task89-evo-<seed>.txt /root/.npm/_npx/52027bd8fc0022aa/node_modules/node/bin/node node_modules/vitest/vitest.mjs run test/tests/zz-dump/evolution-state.producer.test.ts
# Back in port:
reward-core/build/evolutionstatetest /tmp/task89-evo-<seed>.txt
reward-core/build/evolutionstatetest /tmp/task89-evo-777.txt --mutate-species
reward-core/build/evolutionstatetest /tmp/task89-evo-777.txt --mutate-ability
PATH=/root/.npm/_npx/52027bd8fc0022aa/node_modules/node/bin:$PATH BUILD=/tmp/task89-regression sh reward-core/tools/difftest.sh /tmp/pokerogue-upstream 4000 99 all
PATH=/root/.npm/_npx/52027bd8fc0022aa/node_modules/node/bin:$PATH BUILD=/tmp/task89-victory5 sh reward-core/tools/difftest.sh /tmp/pokerogue-upstream 20000 5 victory
# In external upstream after copying fusion-randomness.probe.test.ts there:
/root/.npm/_npx/52027bd8fc0022aa/node_modules/node/bin/node node_modules/vitest/vitest.mjs run test/tests/zz-dump/fusion-randomness.probe.test.ts
```

Each evolution producer reports one test passed. No 3DS/emulator result or full
run evolution parity is claimed. Node 22 follows HANDOFF; upstream's declared
Node >=24.9.0/runtime conflict remains as documented by Task 88.


## What works, and how it was checked
| Piece | Upstream | State |
|---|---|---|
| The item catalogue: 109 `modifierTypes`, every type a generator can make (624: berries, vitamins, X items, type boosters, species boosters, TMs, evolution and form change items, mints, Tera Shards, Shuckle Juice, Black Sludge) with its English name and group, stack and held limits, the PLAYER pool (86 entries), the TM tiers and each species form's TM list, the item evolutions and item form changes with their conditions | `src/modifier/modifier-type.ts`, `init-modifier-pools.ts`, species data | Dumped by `gen/dump-rewards.test.ts` into `gen/out/`; the dump fails if upstream adds a weight function or a form change condition the port doesn't know |
| The 55 weight functions of the PLAYER pool (potions, revives, PP items, lures, balls at 99, vouchers by reroll count, Eviolite, Leek, the orbs, Mystical Rock, Memory Mushroom, DNA Splicers, Tera items, the wave-199/189 skips...) | `init-modifier-pools.ts` | Matches |
| `regenerateModifierPoolThresholds(party, PLAYER, rerollCount)`: each generator rolls in pool order; an item the player holds at its stack limit weighs 0, except held and form change items (a generator that came up empty counts as neither) | `modifier-type.ts` | Matches, including the RNG state |
| The generators: berries, vitamins, X items, attack type boosters (3 per type at most), species stat boosters, the three TM tiers (`getCompatibleTms(true, true, true)`), evolution items (paused evolutions, forms, conditions), form change items (Mega Bracelet and Dynamax Band, conditions, the held ones left out, Necrozma's rule), mints, Tera Shards | `modifier-type.ts` | Matches |
| `getNewModifierTypeOption` (the tier roll, luck upgrades, the retry after an empty generator, a tier down after 100), `getModifierTypeOptionWithRetry` (duplicate names or groups, where two undefined groups count as the same, as upstream; Hardcore turning down revives), `getPlayerModifierTypeOptions` with `CustomModifierSettings` (guaranteed options, items and tiers, fill remaining, luck upgrades off), `withTierFromPool` (with upstream's `if (defaultTier)` quirk: a Common-only item keeps an undefined tier) | `modifier-type.ts` | Matches |
| `SelectModifierPhase.start`: `resetSeed` on a first roll, the thresholds (kept for a copy), `getModifierCount` (3, Golden and Silver Poké Balls, custom counts) | `src/phases/select-modifier-phase.ts` | Matches |
| The fixed rewards of a won wave: Lock Capsule at classic wave 165, EXP Share, EXP Charm, Super EXP Charm (with `offsetGym`), Golden Poké Ball, vouchers every 50 endless waves, the daily run's, and whether a reward screen follows | `src/phases/victory-phase.ts` | Matches |

### Test runs
`tools/difftest.sh <upstream> <N> <seed> rewards`: each case sets a random game mode (challenge runs get Fresh Start, Hardcore and other challenges at random), run seed, wave (a sixth of them on the waves the rules single out), unlocks, the fusion event, the dex entries the conditions read, biome, weather and ball counts (some at 99), and a real party of 1 to 6 with random species (half from the species the rules name), forms, fusions, levels, HP (full, fainted, in between), statuses, natures, genders, abilities, passives, movesets with used PP and PP Ups, used TMs and shininess, then random held items and persistent items (stacks up to their limits, lures part-way through). One case in 8 stacks attack moves of one type. Then it runs up to three screens in a row as `SelectModifierPhase` would: a first roll or a reroll, a reroll (rerollCount 1 to 3, with the Lock Capsule's tiers half the time), a copy of the screen (the old options, or random custom settings), a sixth of them with random `CustomModifierSettings`. Every option (item, generated argument, tier, luck upgrade, English name), the option count and the RNG state after each screen are compared.

| Seed | Cases | Screens | Pass | Mismatches | Unported |
|---|---|---|---|---|---|
| 7 | 1,500 | 3,068 | 1,480 | 0 | 20 |
| 99 | 4,000 | 8,052 | 3,950 | 0 | 50 |
| 777 | 4,000 | 8,020 | 3,964 | 0 | 36 |

The unported cases are all Moveset Randomizer runs (its TM shuffle isn't ported; the port reports it instead of guessing).

`tools/difftest.sh <upstream> 20000 <seed> victory` runs upstream's own `VictoryPhase.start()` with the phase queue stubbed, for a random mode, wave (a third on the waves the rules name, some up to 1,300), `offsetGym` and battle type: seed 5, 20,000 cases, 0 mismatches (9,984 items, 12,402 reward screens).

Planted bugs (the seed-99 cases replayed through a changed port): 16 of 17 are caught, from the Poké Ball weight ignoring the game mode (958 cases) and two undefined groups not counting as duplicates (1,901) to the attack type booster's cap of 3 (5), the Mystical Rock's full stack (4) and a held item's stack limit (4). The one not caught is Necrozma's rule, which can't change anything with today's data: the Ultranecrozium Z and the two N-items it looks for are never offered from the same form (checked in `gen/out/reward_data.c`).

## Memory Mushroom / move relearning

Task 43 adds the two-stage Memory Mushroom path. `pr_reward_memory_moves` mirrors
`PlayerPokemon.getLearnableLevelMoves()`: previous level/evolution/pre-evolution/relearner rows,
eligible unlocked egg moves, and used TMs are combined, currently-known moves are removed, and the
stable upstream source ordering is preserved. `pr_reward_memory_begin` represents
`PartyUiMode.REMEMBER_MOVE_MODIFIER`; `pr_reward_memory_choose` represents the queued
`LearnMovePhase(..., LearnMoveType.MEMORY, cost)` and records that the reward screen is copied
before the consumable is applied.

The caller supplies parsed learnset rows, unlocked egg moves and challenge egg-relearn availability
in `pr_reward_mon`; reward-core owns the Memory Mushroom filtering/order and target state, not the UI rendering.

## Card 97: the trainers' own rewards (PR #132, 2026-10-05)

Ported from upstream `8555c08`, in `src/trainer_rewards.c` (API in `include/pr/reward.h`, data dumped into `gen/out/reward_data.c` by `gen/dump-rewards.test.ts`), merged in PR #132 (commit `705a79e`):

| Piece | Upstream | Port |
|---|---|---|
| The reward screen settings of the rival's and the evil team boss's fixed battles, on the victory path (`getFixedBattle(w)?.customModifierRewardSettings`) and the biome path (`isFixedBattle(w) ? ... : undefined`, when a challenge turns the party heal off) | `fixed-battle-configs.ts`, `game-mode.ts`, `victory-phase.ts`, `select-biome-phase.ts` | `pr_reward_fixed_battle_settings`. The settings per mode and wave are dumped (the dump fails on a setting other than `guaranteedModifierTiers`, `allowLuckUpgrades`, `fillRemaining`) |
| Single Generation's `FIXED_BATTLES` hook: which waves it takes over, the boss reward settings it sets, and the `randSeedItem` draws its Elite Four and champion cases make on the run RNG | `challenge.ts` `SingleGenerationChallenge.applyFixedBattle` | Same function; settings dumped per value 1 to 9, draws ported |
| `TrainerVictoryPhase.start()`: `MoneyRewardPhase(moneyMultiplier)`, a `ModifierRewardPhase` per `modifierRewardFuncs` item (rival fights 1, 2, 4), and the boss voucher. `validateVoucher` unlocks the voucher (and counts it) the first time; the voucher item is only queued once it was already unlocked, one tier down unless `getUpgradeUnlockedVouchers()` | `trainer-victory-phase.ts`, `battle-scene.ts` `validateVoucher`, `system/voucher.ts` | `pr_reward_trainer_victory_rewards`. isBoss, the voucher type (upstream's own `vouchers` table), the money multiplier and the reward items are dumped per TrainerType |
| `Trainer.genModifiers(party)`: the seven `genModifiersFunc` closures (Red, Steven, Wallace, Kieran, Guzma's rematch, rival fights 5 and 6), including where upstream throws on a short party | `trainer-config.ts`, `field/trainer.ts` | `pr_reward_trainer_items`. The dump fails if upstream gives another trainer a `genModifiersFunc` |

Not covered here: game-core does not call any of this yet (its `trainer_victory` in `game-core/src/gc_phases.c` guesses the voucher from the money multiplier and queues it on the first win, which is the opposite of upstream, and its reward screens pass no fixed battle settings); run-core's `pr_run_generate_enemy_items` still refuses the seven trainers with a `genModifiersFunc` (merging their items with the random ones is run-core's). The victory message (`randSeedItem` under `executeWithSeedOffset`, in a UI callback), the Breeders in Space achievement and timed events' fixed battle rewards are not ported.

### How it is checked
`tools/trainerdiff.sh <upstream> 2000 <seed>` (refuses to run unless `node --version` is v22) copies `tests/trainer-rewards.producer.test.ts` into the upstream checkout and replays its cases through `build/trainerrewardstest`. Three kinds of case, all on upstream's code:
- **F**, fixed battles: random mode, Single Generation value (a third of the cases are challenge runs, two thirds of those with it on), wave (two thirds on the fixed battle waves), offsetGym, path, run RNG. The victory path runs `VictoryPhase.start()` with the phase queue stubbed; the biome path evaluates select-biome-phase's expression on upstream's GameMode. Compared: every queued item in order, every field of the settings (a setting the port has no field for is a mismatch), the RNG state after.
- **V**, `TrainerVictoryPhase.start()` with a real trainer config, a random voucher unlock state and upgrade flag: every queued phase in order with its argument (any phase the port doesn't make is a mismatch), whether the voucher is unlocked after, the four voucher count changes, the RNG state.
- **G**, `Trainer.genModifiers(party)` on 0 to 6 real `EnemyPokemon` (random species, half the time the closure's species at its slot; random ability index, fusions, passives): every item, its species stat booster key, holder and stack, or that upstream threw; the RNG state.

The inputs are what upstream had before the call (RNG state, unlock state, each enemy's species, `getAbility(true)` and passive), never derived from the answers. An exception outside the call a G case tests fails the producer; the file's last line counts the cases, so a cut file exits 2. The comparator exits 3 when a family went unexercised.

Node v22.22.0, upstream `8555c08`:

| Seed | Cases | Mismatches | F / V / G cases | F with settings (victory / biome), Single Generation cases that drew | V: reward funcs (RIVAL / 2 / 4), vouchers plain / upgraded, unlocked now | G: cases with items (RED STEVEN WALLACE KIERAN GUZMA_2 RIVAL_5 RIVAL_6), threw |
|---|---|---|---|---|---|---|
| 777 | 2,000 | 0 | 808 / 598 / 594 | 74 / 36, 23 | 39 / 48 / 43, 96 / 89, 208 | 57 72 72 10 6 15 13, 13 |
| 9001 | 2,000 | 0 | 811 / 614 / 575 | 89 / 50, 17 | 46 / 44 / 52, 106 / 92, 213 | 62 62 45 10 10 18 15, 15 |
| 4242 | 2,000 | 0 | 842 / 577 / 581 | 89 / 44, 15 | 45 / 46 / 45, 100 / 74, 192 | 68 75 60 8 11 13 19, 13 |
| 31337 | 2,000 | 0 | 830 / 567 / 603 | 75 / 37, 21 | 46 / 46 / 48, 98 / 88, 187 | 60 65 80 10 11 15 14, 10 |

Every settings wave (25, 55, 95, 115, 145, 165, 195) has cases on every seed. Planted bugs, `tools/trainerplant.sh <trace>` (one line of `src/trainer_rewards.c` changed per copy), total mismatching cases on seeds 777 / 9001 / 4242 / 31337, every run exit 1:

| Planted bug | Mismatches |
|---|---|
| Voucher unlock check inverted | 393 / 411 / 366 / 373 |
| Passive ability ignored | 4 / 6 / 7 / 10 |
| Champion hook draws 5 instead of 6 | 3 / 3 / 4 / 4 |
| Rival's bird: Guts on any species | 8 / 4 / 3 / 5 |
| Fixed battles' `allowLuckUpgrades` always true | 110 / 139 / 133 / 112 |
| Money multiplier always 1 | 549 / 559 / 528 / 511 |

## Not done yet
- Wiring card 97's trainer rewards into game-core and the trainers' own items into run-core's enemy items (see Card 97).
- What each reward does when it is picked or used (healing, revives, PP, TMs, evolution items, vitamins, Rare Candy, held items into the battle's modifier list, lures, charms, exp items). Next in this folder.
- The Moveset Randomizer's TM shuffle (reported as `PR_REWARD_UNPORTED_CHALLENGE`), timed events' fixed battle rewards, and the dev override `ITEM_REWARD_OVERRIDE`.
- Rare items (form change items' effects, DNA Splicers, Mega and Tera) wait for M2, as the beta plan says; they already appear in the options.

## Build and test
```
make                                   # build/libreward.a, build/rewardstest, build/victorytest
tools/redump.sh <upstream>             # regenerate gen/out/ from upstream
tools/difftest.sh <upstream> 4000 99   # rewards and victory cases, replayed through the port
tools/trainerdiff.sh <upstream> 2000 777   # card 97: trainer rewards (Node 22 only)
tools/trainerplant.sh <trace>          # card 97: six planted bugs replayed on a trace; each must fail
```
The upstream checkout is the one `battle-core/tools/setup-upstream.sh` prepares. To look into a mismatch: rerun the producer with `DIFF_ONLY=<case>` (it prints every pool entry's weight and every generator result for that case to stderr), and replay with `DEBUG_CASE=<case>` against a build made with `CFLAGS="-O0 -g -std=c99 -DPR_REWARD_DEBUG"`, which prints the port's.

The following Task91 results describe its separate prerequisite PR#107. This branch incorporates that API; combined replay still passes12000 applications,8000 evolution-state cases and8000 dual-input cases with zero mismatches. Merge#107 before#104 to keep each task's review/merge separate.

## Task 91: Separate TM and relearner caller inputs (2026-10-04)

Task 59 cannot supply one accurate `level_moves` array for two upstream contracts. TM generation excludes `getLevelMoves({includeEvolutionMoves:true, includeRelearnerMoves:true})`; Memory Mushroom reads `startingLevel:1` plus pre-evolution moves. `pr_reward_mon` now accepts a separate, explicitly selected relearn list with level/source metadata. Explicit mode excludes the base moveset, as upstream does even under summon moveset overrides, and permits an empty list. Legacy callers retain their original input path. Used-TM history duplicates survive filtering against the earlier level/egg rows, matching actual upstream behavior.

An additive producer invokes both actual upstream getters, `getLearnableLevelMoves()` and all three TM generators in the same case. C compares the resulting Memory rows and each TM/RNG result. Node 22.23.3 / Vitest 4.1.11, GCC 13.3, upstream `8555c08c823b856cbec4eb99ca84ea52a955836d`:

| Seed | Identical / total | Mismatches | Distinct lists | Fusion | Summon moveset overrides |
| --- | --- | --- | --- | --- | --- |
| 777 | 2000 / 2000 | 0 | 322 | 978 | 995 |
| 9001 | 2000 / 2000 | 0 | 324 | 974 | 992 |
| 4242 | 2000 / 2000 | 0 | 293 | 990 | 1021 |
| 31337 | 2000 / 2000 | 0 | 284 | 961 | 998 |

8000 cases, 24,000 TM/RNG comparisons, zero thrown/skipped cases. A planted return to the shared-list contract fails 945/2000 cases; truncation is rejected (exit 2). Three host-only boundary checks cover explicit empty, legacy fallback and oversized input; the upstream random cases produced zero empty raw relearn lists.

```sh
# Copy the new producer into the external pinned checkout's test/tests/zz-dump/:
DIFF_N=2000 DIFF_SEED=777 DIFF_OUT=/abs/dual-777.txt \
  node node_modules/vitest/vitest.mjs run test/tests/zz-dump/learnset-inputs.producer.test.ts
# Repeat for 9001, 4242, 31337, then in the port:
make -C reward-core
reward-core/build/learnsetinputtest /abs/dual-777.txt
reward-core/build/learnsetinputtest /abs/dual-777.txt --mutate-shared-list
reward-core/build/learnsetinputtest --selftest
```

Established seed-99 / N=4000 producers and unchanged comparators were run before/after on the same traces: reward options **3950 pass / 0 fail / 50 unported**, victory **4000 pass / 0 fail**, application **3865 pass / 135 fail**. Each before/after output is byte-identical; application failures are the existing fusion gap, not passing validation.


## Task 91 validation repair (2026-10-04)

The user's authorization resolved the established Memory producer blocker: ValueHolder now imports from `#utils/value-holder`, its actual pinned upstream module. No assertion, case count, seed or comparison was removed. Fresh seed99/N4000 Memory replay: **4000 cases, 0 mismatches**. The separate dual-input suite was rerun on Node24.19.0/Vitest4.1.11 at upstream `8555c08`: four seeds ×2000 = **8000 identical, zero Memory/TM/RNG mismatches**, with 24,000 TM/RNG comparisons; explicit-empty/bounds checks pass, shared-list mutation fails, incomplete trace exits2.

Established seed99/N4000 options remain3950pass/0fail/50unported, victory4000pass/0fail, application3865pass/135fail (unchanged existing fusion gap, addressed separately in Task89/#104). Task91's pass gate requires no worsening of that established application baseline; those135failures are never credited as passes. Host test exits0, paired200-run digests `d1eb301b`/`d1eb301b`. CI must pass on the updated head before ready-for-review transition. Integration Task59 requires this API to merge on main; Task60 still requires Task59 and its documented prerequisites. No full reward/run/hardware lifecycle claim.


