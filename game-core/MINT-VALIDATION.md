# Bounded Nature Mint adapter (Task 96)

Mint rewards and purchases now call the merged reward-core Mint API through
`gc_port_apply_reward`. The adapter preserves original nature separately from
the effective custom nature, writes permanent stats and HP, and consumes the
species/nature unlock requests through the profile's root-caught gate and
prevolution chain. Catching a minted Pokemon still records its original nature,
as pinned upstream's `setPokemonSpeciesCaught` does. Other party fields and
inventory are preserved; a supported Mint is consumed, not stored as a held item.
The existing shop lifecycle handles payment and refusals.

Named save fields preserve both nature values. Older format-2 saves without the
new fields load with `has_custom_nature=false`, using their existing nature.
Invalid minted nature values are rejected. No format version change is needed.

Challenges and stat-altering inventory on the target (or wildcard ownership)
remain counted unsupported paths: vitamins, Soul Dew and the three mystery stat
items. The adapter does not claim full Task 96, full run parity, constructor stat
parity, or unsupported reward applications.

## Reproduce

Use Node 22 (validated with 22.23.3), upstream commit
`8555c08c823b856cbec4eb99ca84ea52a955836d`, its frozen dependencies/locales and
headless assets prepared by the established setup. Keep upstream and traces
outside tracked source.

```sh
make -C game-core test
game-core/tools/mintdiff.sh /external/pinned-upstream 2000 777
game-core/tools/mintdiff.sh /external/pinned-upstream 2000 9001
game-core/tools/mintdiff.sh /external/pinned-upstream 2000 4242
game-core/tools/mintdiff.sh /external/pinned-upstream 2000 31337
game-core/tools/applydiff.sh /external/pinned-upstream 4000 777
game-core/tools/rewarddiff.sh /external/pinned-upstream 1500 777
reward-core/tools/difftest.sh /external/pinned-upstream 4000 99 all
game-core/build/gcplay --auto 1000 --jobs 8
game-core/build/gcplay --auto 1000 --jobs 8 --save-test
```

The established apply/options checks were also run for seeds 9001, 4242 and
31337. `BUILD` selects the build directory; `GC_MINT_OUT` selects the new trace.
The additive Mint producer invokes upstream's real filter/newModifier/addModifier,
setCustomNature, stat calculation and profile unlock method. It fails on
unexpected queued phases and requires a completion footer. The comparator
rotates party storage and replays each fixture free and bought, comparing all
54 modeled party fields, complete fixture inventory, profile entries, result,
unlock requests and global RNG. Fixtures include forms, fusions, prior custom
natures, status/fainted Pokemon and level 255.

## Results

- Four seeds: 8,000 upstream cases, 16,000 free/bought comparisons, zero
  mismatches. Every seed covers all 25 natures (60 accepted and 20 refused each),
  667 root-locked / 1,333 unlocked targets and 286 level-255 targets.
- Host Mint test: 107 checks, or 113 with a save written by an executable built
  from pre-change source. Covers repeat Mints, preserved transient battle fields,
  caught-original nature, prevolution unlocks, refusals, guarded stand-ins, real
  free/shop command flows and save/load boundaries.
- Boundary mutations for original nature, stats and profile produced respectively
  3,512, 4,000 and 4,000 failures of 4,000 comparisons. An actual adapter-source
  mutation writing the effective nature as the original produced 2,952 failures.
  Missing completion footer exits 2. Mutants were temporary external copies.
- Established healing/candy/ball and reward-option outputs match before/after
  byte-for-byte with zero comparator mismatches. Their existing exclusions remain:
  healing traces exclude Hardcore and other item families; options retain the
  pre-existing constructor stat diagnostics (6,838–7,242 differing own-stat values
  per seed). Those differences are not credited as parity.
- Reward-core seed 99: options 3,950 identical / 50 unported, 8,052 screens;
  victory, application and Memory each 4,000 identical, zero mismatches.
- Host suite passes: 32,731 resume checks, 39 existing bridge checks, 641,335
  existing application checks; the paired 200-run digest remains `a6662b64`.
- 1,000-run checks: all ended, no stuck/crashed/timed-out runs. Before: 49,315
  turns, paired digest `5f646aee`; after: 49,305 turns, paired digest `b465d096`.
  After-save run: 14,308 round trips, largest save 11,582 bytes. The behavior
  changes the digest; chooser and digest code are unchanged. External observation
  of the same run counted 14 real Mint applications, one unsupported counted
  application and one refusal (15 taken total). Reward stand-ins drop from 434
  to 417; the remaining Mint stand-in is explicitly retained.

These are host/source differential results. Console hardware behavior is unrun;
the PR's required host and 3DS build CI must pass on its actual head before merge.
