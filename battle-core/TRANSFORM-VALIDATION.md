# Ditto, Transform and Imposter

Base `0e376150af9309c5881a5e757e65062bcb1792c1` (#253). Actual source
`8555c08c823b856cbec4eb99ca84ea52a955836d`, locales
`270ed0a2938b2c3e2e34e7de940e84aecadf548b`, Node 22.23.3 / GCC 14.2.

The real Transform effect and Imposter dispatcher now queue the shared
`PokemonTransformPhase`. Imposter selects one random eligible opponent, once,
including natural doubles. The pinned restrictions reject transformed users or
targets, fusions, illusions and a target's Substitute. No additional mainline
restrictions or ability exclusions are invented. The phase itself checks only
whether its target disappeared, as upstream does.

Temporary identity, ability, gender, ATK through Speed (including fractional
overrides), seven stat stages, types and moves follow the pin. Power Trick is
removed before copying. HP and permanent species/form/gender/level/IVs/nature,
identity, passive and skin stay with the user. Copied moves start with zero PP
used and PP Ups and `min(base PP, 5)` maximum PP; Sketch retains one PP. Types
exclude the target's Tera but keep the user's Tera and added type, including the
pin's UNKNOWN typing behavior. The copied ability's gain callbacks follow the
visual boundary.

`Pokemon.moveset` remains permanent. Separate temporary arrays represent
`summonData.moveset`; `PR_MOVES`, `PR_PP_USED`, `PR_PP_UP`, `PR_MAX_PP` and
`PR_N_MOVES` select the effective collection. Battle, AI, command and item
consumers use them. A queued move reference retains the collection it originally
selected, so Transform cannot redirect an old move's PP to a new copy. Learning
and evolution prerequisites retain the pinned permanent-move reads and writes.
Switching, fainting and summon reset discard the temporary state.

Effective form reads are separated from the pin's permanent `getFormKey` and
`getSpeciesForm(true)` reads. Notably, copying a G-Max image does not give Ditto
G-Max sprite scale. Species powders in this pin inspect the permanent species:
Metal Powder and Quick Powder continue to qualify real Ditto after Transform.

The application receives an immutable Transform display event, with Imposter's
extra sound distinguished from the move animation. Effective image/gender and
temporary move/type/stage information retain the user's name, HP, level and
shiny variant. The existing menu sprite loader supplies gender, form, shiny and
variant images/palettes. Event snapshots and the converter track a new party
member before its Imposter event, avoiding stale data from the previous member.
Save format 2 adds hashed temporary fields; normal older saves remain usable.
Active transformed saves require the complete temporary schema. Permanent
moves/PP and the user's shiny variant survive a round trip and later reset.

## Actual-source proof

```sh
source /workspace/shared/activate-parity.sh
for seed in 777 9001 4242 31337; do
  battle-core/tools/transform-diff.sh /path/to/pinned/upstream 2000 "$seed"
done
make -C game-core transform-host-test
make -C game/tests transform_visual_test
game/tests/build/transform_visual_test
```

All four seeds pass 2,000 owning phase cases: **8,000 exact cases**, no skips or
unsupported/overflowed results. Each seed has 1,581 executed transformations
and 532 Imposter activations. The 104-number projection checks permanent and
effective identity, original and copied moves/PP, all raw/temporary stats and
stages, types/Tera, restrictions, missing/self targets, gender/shiny/variant,
raw Mega/Max flags and sprite scale; global/battle RNG and sound flags are exact.
Only presentation methods are stubbed; the actual move attribute, ability
dispatcher/selection, shared phase and Pokemon getters execute.

The separate full-turn producer derives an additive fixture from the unchanged
Speed Boost scaffold and uses the unchanged comparison rules. Four seeds pass
another **8,000 complete turns**, all covered, exact state/RNG/phase order and
identical rendered English text, with zero parse/stat mismatches. Two of its
24 states execute a real Transform turn first and then record the following
complete turn using a copied move; this verifies independent PP consumption.
Other states include both sides, Protect/Substitute, fractional stats, Tera,
added types, copied ability gain/turn callbacks, KO/faint/switch interruption and
Mega images. New transport fields describe temporary moves; the established
permanent `MOVES` record and comparator remain untouched.

Twenty-six independently compiled runtime mutations are detected: HP/max HP,
species/gender/variant/passive copying, raw versus temporary stats, stages,
Power Trick, PP caps/Sketch/used PP/PP Ups, Tera/added type, copied ability gain,
sound, move-reference collection, reset, every eligibility exclusion, random
selection and fainted opponents. Missing gain and the wrong PP collection are
caught by full turns. Twenty-one malformed fixtures return exit 2; the intact
fixture returns zero.

The save/learn/reset host check passes **69 assertions**. The visual adapter
passes **59 assertions**, including an actual switch into Imposter and a G-Max
image with Ditto's scale. A 10,000-run application check passes with zero
screen/run mismatches, stuck runs or model violations: 119,527 waves and 483,965
commands. Paired 1,000-run host soaks agree at digest `2f0bb182`, 49,260 turns
and 14,491 save/load round trips; every run ends without crashes/timeouts/stalls.

The unchanged original comparator passes 24,000 retained turns without coverage
loss. Exact covered counts are plain 1,964; rich 1,932; switch 1,891; summon
1,871 to **1,891**. Items, seeds 777/9001/4242/31337, are
1,944/1,948/1,945/1,957 to **1,945/1,948/1,945/1,958**; trainer counts
1,949/1,948/1,947/1,959 to **1,951/1,950/1,947/1,959**. Another 72,000 retained
dedicated full turns (Moody, knockout stats, leave-weather, Wonder Skin,
priority, Protean/Libero, field priority, multi-hit abilities and Beat Up) pass.
Retained score and flee numeric phase oracles also pass. These fixed corpus
counts are not a battle-core completion percentage.

## Fixture development and limits

The initial fixture was corrected to use string attribute lookup, the actual
ability dispatcher module, test override setters, and the source's party-based
field lookup for a missing target. It supplies the real wave seed for copied
Intimidate. Setup-only tag presentation is cleared before a turn begins.
Permanent and temporary collections were separated after retained summon
records exposed their distinction. The application's fake screen gained the
new event and checks the effective identity/moves while retaining permanent
HP/level/name/skin assertions; no existing coverage floors were lowered. The
10,000-run check then exposed one stale incoming-party payload, reproduced and
fixed with a dedicated switch regression. Source methods, established
producers/comparators, tolerances, seeds and counts were not modified.

The owning doubles proof is a **phase boundary**, not full battle-wide doubles
parity. Fusion transformation remains forbidden by upstream. Arbitrary mystery
custom sprite scales, fusion palette behavior, wide levels above 255, other
unported ability/form/partner lifecycles and final-boss/run prerequisites remain
separate gates. Local sanitizer runs disable LeakSanitizer because of ptrace;
normal CI remains required. No hardware/emulator rendering validation is
claimed by these host and source checks.

## Mainline rules and the pinned target

The pinned copy phase stores `target.getSpeciesForm()`, preserving the current
form rather than looking up the target's original base form. Its acquisition
code has no G-Max-to-base exception. `SpeciesFormChange.canChange` and the scene
form dispatcher also have no blanket `isTransformed()` rejection; each registered
trigger supplies its own conditions. These are PokéRogue pin semantics, not a
claim of exact mainline Pokémon mechanics. A mainline restriction would be an
explicit custom deviation from the current parity target.
