# Wide Lens inventory bridge

Existing Wide Lens inventory now maps to the owning battle-core accuracy class
(#149) with explicit amount 5, stable holder, real stack and transferability on
both sides. Battle-core applies the bonus at the actual hit check, retaining its
variable/always-hit/OHKO behavior, Fog/Gravity floors and RNG ordering. Lenses
are not consumed. The adapter reconciles surviving stack/ownership and preserves
detached enemy loot owner 0 separately from wildcard -1 before rebuild/save.

The bounded recognized shape is real stack 1..3, nonnegative transient virtual
stack, effective real+virtual <=3, and argument -1 or 0. Other shapes stay opaque.
Generic acquisition max-stock/default replacement and item transfers remain
separate; this does not complete Task 104 or Scene.addModifier parity.

`gc_item.virtual_stack` explicitly transports transient Wide Lens state for the
real source fixtures. Ordinary game entries start zero. Pinned ModifierData
serializes only real stacks and reconstructs virtual stacks as zero; the C save
likewise omits this field and explicitly resets it on load. Six source-backed
valid-holder reconstructions compare real stacks, lost virtual stacks and actual
accuracy amounts. Virtual fixtures intentionally do not claim normal/save
outcome equivalence. The existing port's own wildcard/detached save contract is
preserved separately; this is not whole upstream ModifierData/session migration.
No virtual acquisition/simulation lifecycle is implemented. Non-Wide virtual
entries remain opaque; bounded Mint/vitamin/money adapters refuse nonzero virtual
inventory rather than silently drop that newly expressible caller input, retaining
counted fallback storage.

## Reproduce

Node 22.23.3, pinned prepared external upstream
`8555c08c823b856cbec4eb99ca84ea52a955836d`, frozen dependencies/locales/assets:

```sh
make -j2 -C game-core test
for task_seed in 777 9001 4242 31337; do
  game-core/tools/lensdiff.sh /external/pinned-upstream 2000 "$task_seed"
done
game-core/tools/lensstoragediff.sh /external/pinned-upstream
game-core/build/gcplay --auto 1000 --jobs 8
game-core/build/gcplay --auto 1000 --jobs 8 --save-test
```

`BUILD`, `GC_LENS_OUT` and `GC_LENS_STORAGE_OUT` select output paths. The turn
wrapper copies the unchanged owning producer and includes its unchanged full
turn comparator/guards. It inserts actual game inventory conversion before the
turn, compares full source battle state/RNG/phases/rendered texts/modifiers, then
checks raw inventory non-consumption and rebuilt owner/class/real+virtual stack/
amount/transferability. Requested count/completion footer and transport bounds
are enforced. No established producer/comparator/seed/count/exclusion was changed.
The separately added storage producer runs real ModifierData serialization and
reconstruction on valid holders; every source row is replayed through C save/load.

## Results

Base main `d0291d6` (Reviver plus Golden Punch/temporary booster prerequisites).
Evidence: `/workspace/validation/lane-c/eight/wide`.

- Four seeds x 2,000 full turns: all 8,000 covered/identical, all texts identical;
  zero skips, parse/stat mismatches, uncovered or untranslated turns. Per seed:
  2,000 mappings on each side, 167 virtual-stack cases. Twelve states include
  plain/Fog/Gravity/both, OHKO, always hit, wrong owner, virtual stack, Poison
  Toxic, No Guard, stages and status moves. Numeric source accuracy probes are
  retained by the owning oracle. Included owning guard: 16 checks.
- 195 default host checks; 199 including a true pre-field Wide Lens save;
  259 including the six real ModifierData rows. Tests cover both sides,
  real+virtual caps, wildcard/detached mapping, legal command saves/resumed
  hit checks, non-consumption, virtual loss/reset and older zero defaults.
  Non-Wide virtual opacity and counted Mint/vitamin/money fallback are proved.
- Actual temporary `gc_items.c` mutation decrementing transported real Lens
  stack: committed boundary rejects the first inventory change, exit 1. A
  separate observational replay of the same mutant through the unchanged
  owning full-turn comparator (without the new inventory postcondition) reports
  2,000 full-state failures and 38 genuine rendered hit/miss text differences,
  zero parse/stat errors/uncovered turns, exit 1. This diagnostic does not waive
  the committed invariant or credit a passing check. Missing footer and amount
  261 transport exit 2. All mutant files stay outside the repository.
- Existing four-seed healing/candy/ball/options outputs remain byte-identical;
  exclusions/constructor-stat diagnostics remain uncredited. Mint/vitamin/money
  retain 16,000 comparisons each; Orb and Seed retain 8,000 full turns each.
- H passes: 32,697 resume checks, 39 existing bridge and 641,335 existing
  application checks. Paired 200-run digest changes `3e264621` to `79d84c65`.
  Both 1,000-run modes end every run with no hangs/crashes/timeouts: 49,206
  turns, paired before `3492b259`, after `c2937ca0`; 14,291 save round trips,
  maximum 11,400 bytes. Chooser/digest code is unchanged. An external observed
  adapter copy with an accuracy-helper linker wrapper reproduces `c2937ca0`:
  60 real player hit-check accuracy bonuses, zero enemy ones; 229 recognized
  player mapping visits, zero opaque Lens visits. Visits repeat across waves
  and are not effects/acquisitions; 26 taken Lenses are acquisition tallies.
  Enemy effects are established by the focused source fixtures.

Compiler warnings from unchanged included comparators and positional aggregate
initializers after the additive item fields were retained; no warning/check was
suppressed. Actual-head host/3DS CI is required before merge. Hardware, complete
upstream run/frontend/acquisition parity and transient creator lifecycle are unrun.
