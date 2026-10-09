# Canonical Wide Lens acquisition caller

Pinned source: `8555c08c823b856cbec4eb99ca84ea52a955836d`, Node 22. This
slice connects canonical Wide Lens acquisition to the existing inventory and
accuracy bridges. It does not complete Task 96 or broad held-item lifecycle parity.

The raw adapter uses reward-core's cap-three, first-exact-holder operation.
Stable party IDs map through party order to reward indices. Existing inventory
order, constructor args (-1 or 0), and holder identity remain intact; only shadow
args normalize. Canonical Multi Lens stocks (cap two), Reviver Seed and Golden
Punch also normalize their constructor args in that shadow context. Wildcard
Wide Lens stock does not match the target; first full duplicate wins even when a
later duplicate has capacity.

The public source selection filter rejects a first matching full stack with
`modifierType:ModifierType.PokemonHeldItemModifierType.extra.tooMany`, before
Scene mutation. The independent direct Scene path instead emits
`battle:itemStackFull` and applies the tier-default ball. Its free/paid success
and wallet behavior are proved separately. Successful persistent Scene additions
and increments return false upstream: free selections finish; paid selections
report an error after mutation and do not charge. Successful Scene updates remove
orphan wildcard Wide Lens records and recalculate the party. Raw acquisition,
public full refusal, and direct full ball replacement preserve those records.

The adapter atomically refuses unknown keys, virtual/detached records, missing
holders, malformed canonical stacks/args/timers, invalid party IDs/order,
challenges, and projection overflow beyond 96 items. Scene recalculation also
refuses fusions, stat-changing stock, nonpositive opaque stocks, and orphan held classes other than Wide
Lens. These guards bound the source updateModifiers claim. An unhandled public
caller reaches the established counted fallback; this is not claimed as source
parity or zero-fallback coverage. No battle-core implementation or established
fixture was changed; temporary booster full turns remain flagged unsupported.

## Evidence

Four seeds `777 9001 4242 31337`, 2,000 cases each, pass both the unchanged
reward raw source producer through the new game adapter (8,000) and the new
actual Scene/SelectModifierPhase producer (8,000). The latter invokes actual
source selection filters, addModifier, updateModifiers and calculateStats;
graphics/UI callbacks are bounded. It compares acceptance, paid error/finish,
first-match behavior, full replacement ball deltas, wallet, inventory order and
holder IDs, all 54 modeled party fields, profile and RNG preservation, and actual
accuracy-helper effects. Source PokemonData, achievement and game-stat snapshots
remain unchanged. Literal full-message presence is checked upstream; exact
rendered message text parity is excluded (the game event key is compared).

Actual accuracy changes occur in all 8,000 cases, separate from mapping visits.
Source Scene inventory changes by seed: 949, 954, 940, 939; direct full fallback
and public filter refusal both occur in every seed. Guards cover 25 raw adapter
cases and 16 atomic raw contexts and two nonpositive Scene contexts, plus public saves/loads and actual loaded
accuracy effects. Wrong-target, omitted Scene wildcard cleanup, and incorrect
paid-success C mutations each fail the actual source replay; a missing footer
fails parsing. Older failed drafts of this new fixture are not passing evidence.

All 40 established bridge/temp replays pass. All 16 established apply/candy/ball/
option replay outputs are byte-identical. Original Mint/Reviver/Lens save fixtures
with stock counts 113/130/199 load. `make -j2 -C game-core test` passes with paired
200-run digest `74adeffb`.

Before/after 1,000 runs, normal and `--save-test`, `--jobs 8`: all end, zero
stuck/crash/timeout, 50,394 turns; both after digests `30c8d270` versus before
`43f24f9f`. Saved runs retain 14,679 round trips and maximum 11,472 bytes. The
only ledger row change is `reward taken without its own effect`: 145 runs/178
uses → 148 runs/185 uses. Previously opaque unsupported Wide fallback now counts
this stand-in explicitly; the digest includes that accounting. No other reported
reward-choice or stand-in rows change. This is explicit bounded refusal evidence,
not a claim to have removed every Wide fallback.

Reproduce from the repository root:

```sh
. /workspace/shared/activate-parity.sh
make -j2 -C game-core test
for seed in 777 9001 4242 31337; do
  game-core/tools/wideacquirediff.sh /workspace/shared/upstream-lane-c 2000 "$seed"
done
game-core/build/gcplay --auto 1000 --jobs 8
game-core/build/gcplay --auto 1000 --jobs 8 --save-test
```

Saved full evidence: `/workspace/validation/lane-c/five/wide-acquire`.
