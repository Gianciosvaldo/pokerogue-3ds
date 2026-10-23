# Fused Eviolite, species boosters and Leek

Pinned upstream `8555c08c823b856cbec4eb99ca84ea52a955836d`, locales
`270ed0a2938b2c3e2e34e7de940e84aecadf548b`, Node 22.

In `src/modifier/modifier.ts`, EvolutionStatBoosterModifier checks both original
species' evolution eligibility. It multiplies by 1.5 when both can evolve,
1.25 when exactly one can evolve, and does nothing when neither can evolve.
Its shouldApply calls Pokemon.isMax, which checks form keys in both fusion
components. SpeciesStatBoosterModifier and SpeciesCritBoosterModifier accept
either original species. They do not use a summoned/Transform species override.
The C helpers now follow these rules, and the canonical full-turn owner admits
the items. Invalid canonical metadata and other explicit limits remain guarded.

## Evidence

`build-fused-held-proof.py` extends the retained actual-turn producer. It runs
native methods, constructors and MoveEffectPhase through naturally started
battles. Native BattleScene.randBattleSeedInt is captured before GameManager
construction and restored after startup. Recording wrappers delegate the actual
source methods and count applications; they do not compute expected effects.

The required matrix has 2,560 cases per seed: eight families (Eviolite, six
species boosters and Leek), both sides, ten item/turn states and sixteen fusion
arrangements. It includes full/half/absent evolution boosts, eligibility in
either/neither component, wrong side/owner, wildcard ownership, duplicate raw
stock, stat stages, Max forms in either half, fixed damage and Splash. Every
family, state and fusion arrangement has a required observation count.

Both seeds 777 and 2034 pass all 2,560 cases: **5,120 full turns**, zero skipped,
uncovered, parse/state/stat/RNG/message mismatches. The comparator retains the
existing turn/queue/text/state comparisons, adds the existing critical-item
fields and requires all cases to reach TurnStartPhase. It retains all 63 stat
metadata guards and 24 critical-item guards. Eight compiled arithmetic,
eligibility and caller faults are detected; six malformed captures are rejected.

The original ordinary stat-item and critical-item producers each retain all
2,000 seed-777 full turns and pass exactly. Their four stat and three critical
fusion admission checks remain required and now expect admission, backed by the
new native matrix. The producers now use captured native battle RNG rather than
the framework's replacement. A stale `%d` effective-stat formatter was corrected
to `%.17g` for the existing double-returning API; expected values are unchanged.

The actual game command/target/turn and save/load owner passes 64 arrangements
(1,152 checks), covering all eight families in either doubles player slot and
four fusion arrangements. Saves load with zero dropped data and continue with
identical HP, RNG and choices. It runs in the normal game-core test target.
The full game-core suite passes; saved/unsaved digests remain `5073f991`.

Retained Pickup 4,400, transferred-stat-item 960, theft 32,768, Grip Claw 32,768,
Baton 256 and scene transfer/Mini Black Hole 3,608 comparisons pass unchanged
against the modified library. Normal app/custom/save tests and actual ARM build
and stack checks remain PR merge gates. Only dedicated custom-only Task63 is
skipped. No SD-card builder is produced.

## Fixture isolation and remaining limits

An initial seed-2034 capture carried detached postBattleLoot object references
between otherwise independent cases. Two cases consequently hit the C's
96-entry loot limit; the strict comparator failed. The producer now clears that
list with the other per-case state. All the same required cases were rerun
against native source. No capacity guard was disabled, no count increased and
no case dropped. The initial captures and failure diagnostics are preserved in
the workspace alongside the final captures.

This isolation correction does not prove that 96 entries suffice for a real
Endless run. Modifier, loot, game inventory and history capacities still need a
source-backed audit. Generic raw constructor parameters and interactions outside
this matrix remain separate. This slice closes the explicit fusion-item
refusal; it does not declare battle core complete.

Reproduce:

```sh
. /workspace/shared/activate-parity.sh
sh battle-core/tools/fused-held-diff.sh /path/to/pinned-upstream
TURNTEST_LOCALES=/path/to/pinned-upstream/locales/en \
  python3 battle-core/tools/check-fused-held-proof.py \
  battle-core/build/fused-held-777.txt
make -C game-core fused-held-host-test
```
