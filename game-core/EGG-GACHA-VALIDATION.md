# Bounded egg generation and voucher gacha (Task 80)

`gc_system_gacha_pull` supplies an offline purchase/generation API. It executes
capacity and voucher admission, debit, all three machines and five purchase
options, tier/last-pull guarantees, egg/unlock pity, generated properties, pull
statistics and display order. It does not install a gacha screen or hatch eggs.
Task 80 remains partial; Task 81 and general egg lifecycle work remain separate.

## Source and inputs

The reference is upstream `8555c08c823b856cbec4eb99ca84ea52a955836d`, Node
22.23.3, actual `EggGachaUiHandler.handleVoucherSelectAction`, `consumeVouchers`,
`pullEggs`, `Egg`, `EggData` constructors and daily legendary selection.
Only animation/label plumbing and account persistence are bypassed. Actual
purchase admission/error keys, creation and shuffle run; no copied expected
formula supplies the source results. Constructor `EggData` snapshots are checked;
`EggData.toEgg` rehydration/migration is excluded because it can rederive fields.

Upstream draws seed text, tier and ID through `Math.random`, independently of
scene RNG. The producer substitutes a supplied Phaser entropy stream for that
external entropy, and supplies a fixed offline clock. The C API receives those
two separate RNG states and clock explicitly; it does not equate Math.random
with ordinary scene RNG. Source temporary seed scopes restore scene RNG, while
actual display shuffle advances it. Both streams and entropy draw counts are
compared independently. Creation/storage order and displayed permutation are
compared separately.

The generated 571-entry eligibility/cost/tier table comes from the real pinned
species registry and actual `hasVariants` after loading the standard variant
masterlist. `gen/dump-egg-pool.test.ts` verifies SHA-256
`c776d4ad0eb7fe63f00b76bc1cfc0dde4bf60404195a2a1634ef3b2e3dcf9b50`.
Experimental assets/toggles are excluded. Daily featured selection uses actual
UTC-day rotation with supplied nonnegative Date-range milliseconds
(0 through 8640000000000000). Tests span 365 offline days; no live-service policy
or clock synchronization is claimed.

Admission is bounded to valid machine/option/RNG states, representable uint32
balances/statistics/pity, unlock pity 0–10, and valid inventory counts/species.
Counters reserve maximum batch headroom (25 statistic increments, 50 pity
increments, 650 entropy draws). Gacha capacity is the source 99, while the
existing generic egg-storage helper retains its old 100-slot contract. Capacity
is checked before cost. Refusal is atomic for profile, RNG and output. Unsupported
empty filtered pools also refuse atomically locally; source exception handling
outside the proven contexts is not claimed. Free/admin/override pulls,
SAME_SPECIES/EVENT eggs, rendered messages, `saveAll`, hatching, egg moves applied
to offspring and account/network storage are excluded.

## Persistence and comparisons

Generated records retain ID, hatch waves, tier, species, source machine, variant,
shiny, egg-move index, hidden-ability override and timestamp. Five pull statistics
and two four-entry pity arrays have named system-save fields. Old files omit
these names and load zeros; the generic `gc_system_add_egg` initializes the new
metadata to zero. Existing save field meanings are unchanged. The new host test
checks dense profile save/load and stripped-new-field compatibility.

Each source case compares all ten egg fields, both pity arrays, four voucher
balances, five pull counters, full old stock and display permutation, both RNGs,
entropy draws and actual featured species. Source party `PokemonData`, money,
dex/starter/unlock/achievement/voucher-unlock profile and unrelated statistics
must stay unchanged. C compares the whole expected system profile and reloads
it after every case. This is constructor/purchase proof, not a gameplay clear.

## Results and reproduction

Four seeds (777, 9001, 4242, 31337), 2,000 cases each, pass exactly: **8,000 cases,
6,708 purchases, 924 capacity refusals, 368 cost refusals and 67,280 generated
eggs**. Observed eggs include 673 shiny, 261 nonstandard variants, 200
Manaphy/Phione, 1,943 legendary and 387 featured legendary selections. There are
442 observed last-pull guarantees and 8,000 exact profile reloads. All machines
and purchase options have positive observed counts. The new host fixture passes
191 checks.

```sh
. /workspace/shared/activate-parity.sh
make -j2 -C game-core all
make -j2 -C game-core test
# Repeat for SEED=777,9001,4242,31337:
game-core/tools/egggachadiff.sh /workspace/shared/upstream-lane-c 2000 777
# Replay an existing trace without rerunning upstream:
game-core/build/egg_gacha_diff_test /workspace/validation/lane-c/eight/egg-gacha/egg-777.txt
```

Eight actual C mutants are detected: wrong voucher debit, removed last guarantee,
wrong legendary pity increment, removed variant, wrong featured species,
removed display shuffle, wrong entropy alphabet and omitted profile commit.
Twelve malformed transports (including overflowing version/count/pool tokens)
and six malformed count/seed requests refuse. The comparator requires exact
manifest, record counts, field ranges, permutations, footer observations and
EOF. Coverage cannot pass with zero meaningful effects.

All **60 retained bridge/temp/Wide/Multi/Classic source replays** pass unchanged;
all **16 existing apply/candy/ball/options outputs** remain byte-identical.
Original encounter-save checks pass Mint 113, Seed 130, Lens 199 and current
Classic 149. Original system/dex/save and other host suites also pass.

On baseline `97bdad7a6715939b9f0b9f352c00f3b4fd0efaa5`, before/after normal and
saved 1,000-run legacy output is byte-identical: digest `75bb4db1`, 50,410 turns,
14,702 reloads and max run-save size 11,472. The paired 200-run host digest stays
`6cd5fc16`. Commands are `gcplay --auto 1000 --jobs 8`, with and without
`--save-test`.

The separate 100-run `gcstress` diagnostic remains exactly paired per seed:
100 reach wave 100, 94 die, six report final-boss ball refusals, zero clears,
crashes or timeouts, 25,824 turns and 18,820 reloads (max 13,693 run-save bytes).
Before/after gameplay rows, choices and all stub counts are identical. Its
full-profile digest changes `cad23d60` → `3f95345d` solely because system-save
serialization now includes the new zero-valued named fields; this diagnostic
hash includes the serialized profile. Normal/saved new digests match. The earlier
Speed Boost integration changed trajectories before this task's baseline.

```sh
python3 game-core/tests/endgame_diagnostic_checks.py game-core/build/gcstress 100
game-core/build/gcstress --runs 100 --jobs 8
game-core/build/gcstress --runs 100 --jobs 8 --save-test
```

Evidence: `/workspace/validation/lane-c/eight/egg-gacha/` contains source traces,
source/replay logs, retained outputs, before/after soaks and diagnostic runs,
actual mutants, malformed transport/request results and old-save checks.
