# Unfused wild boss movesets

After the ordinary-wild follow-up, the adapter still rejected every boss that
its small clean factory did not cover. The existing weighted helper already
supported unfused bosses, including their species' passive. Both fresh native
constructor observations and the original rejected adapter inputs match that
helper. The public adapter now permits that existing unfused-boss path.

The generation change is one condition. Fused bosses, manually enabled passive
flags, trainer streams, challenges, unsupported modes and post-moveset form
callbacks keep their previous boundaries. The clean factory still runs first.
There is no new generation algorithm, save format or struct field.

## Native observations

Upstream `8555c08c823b856cbec4eb99ca84ea52a955836d`, locales
`270ed0a2938b2c3e2e34e7de940e84aecadf548b`, assets
`87426a79611f9d212c4dc8af58e2834a05b93725`, Node 22. Native battle RNG is
captured before GameManager and restored after startup. Both producers capture
the actual native EnemyPokemon generator and verify its base Pokemon method
has remained intact. No output is calculated by C and no case is skipped.

The constructor producer observes actual `scene.addEnemyPokemon` with boss and
shiny-lock enabled in Classic. Its 1,016 inputs comprise 18 primary species at
levels 1/50/70/100/65536/70000 in both formats (216), plus Kingdra, Kyogre,
Groudon and Darmanitan at levels 100/70000 crossed with all ten weather values,
all five terrain values and both formats (800). Actual Weather/Terrain objects
provide those contexts. It records the generator's input stats, ability,
passive identity, boss segment count, full level pool, ordered moves and PP
metadata, and pre/post-generation RNG.

**Boss IV improvement happens after generation.** The whole constructor's
post-RNG state is recorded separately and is not substituted for the generator's
post-state. The controller test uses the observed generation input, before
that later IV improvement. It does not compare pre-generation stats with
post-improvement stats or claim a complete boss constructor-finish proof.

The second producer runs the actual native generator on all 750 originally
rejected unfused-boss adapter snapshots, checking native stats, ability and
boss/passive status against each original input. The expected outputs come
from native generation. These are raw generator observations, not native
full-encounter captures. The original 2,900 adapter/constructor inputs and
assertion-count seeds remain bound by their retained hash. Their first small
factory positives still use that independent factory oracle; the former boss
rejections now have independent native outputs rather than weakened assertions.

Both captures and the generated fixture reproduce byte for byte. Evolved
primaries expose level-pool sources EVOLUTION=4 and PREVO=6, as defined by pinned
`enums/learnable-move-source.ts`; source markers are retained and compared
exactly. Native Pichu prevolution-form fallback warnings are retained as source
behavior, with the resulting pool checked against C.

## Actual owners and scope

The public adapter previously failed the first new case while the existing
helper independently matched all 750 raw and 1,016 constructor observations.
After connecting it, the native owner test checks all 1,766 cases through the
public adapter, helper and refresh: 162,543 comparisons of complete ordered
pools and sources, stats/ability, observed passive identity, move/PP metadata
and published native RNG state. Previous-battle modifiers are poisoned. An
injected late unported operation checks unchanged Pokémon/RNG rejection and
recovery through the actual public adapter.

The retained suite checks all 2,100 original adapter cases, now supported, and
all 800 ordinary wild constructors, with 59,369 checks. All old positive clean
factory cases, 794 formerly rejected ordinary wild inputs, unsupported raw
boundaries and the fixed Recover save checks remain. The ordinary-wild native
owner proof retains its 1,613 cases and 168,244 checks. Phaser publishes four
RNG components; C's seed-mash scratch is not an extra native observation.

Run `python3 game-core/tools/check-wild-boss-movesets-proof.py` for transport,
actual owners and compiled/malformed/forged controls. Set
`WILD_BOSS_MOVESETS_SANITIZE=1` for scoped ASan/UBSan/LSan on the adapter,
generator and encounter unit; supporting archives remain unsanitized. Normal
core/app/custom/save/football tests and the ARM build/stack check remain required.

This does not close fused boss/passive interactions, trainer/TM/egg generation,
post-moveset form callbacks, every possible ability combination, complete boss
finish/encounter generation, full Endless acceptance or hardware verification.

The validation runner detects twelve compiled faults, seventeen malformed
captures/corpora and five forged outputs. Scoped ASan/UBSan/LSan passes all
1,766 native cases. Complete normal CI and the ARM build remain the merge gate.

## Shield redraw regression

The unchanged 10,000-run app corpus exposed a display defect twice after the
more varied boss encounters became reachable: a boss lost a shield, then
Transform or an Illusion redraw copied an older visual actor and restored its
old shield index. The converter already emitted the correct shield-break
event, but had not advanced the actor cache used for full redraw payloads. It
now advances both representations together, including settled catch-up.

A real core turn (Rattata's Tackle followed by a boss Ditto's Transform) fails
on the old converter's Transform payload and passes with the fix. Event-only
Transform and Illusion regressions also verify that a redraw preserves the
preceding shield loss. Retained transform tests pass 73 checks; shield tests
pass 400 fights / 1,501 turns / 16,086 checks. Scoped ASan/UBSan/LSan covers the
converter and both test harnesses, with core/AI archives unsanitized. No
assertion, input seed or coverage floor was removed. Normal app CI remains
required on the final head.

For local diagnosis the app runner links the real app sources to the retained
shared core/AI/run/reward archives. It reproduces the original failure at run
1,110 / wave 10 before the fix. Both original 10,000-run sequences (standard
and external event storage) pass after the fix, with zero model violations and
the unchanged failing-CI simulation digest `843695f9`. The canonical app build
and complete host script still run in final CI.
