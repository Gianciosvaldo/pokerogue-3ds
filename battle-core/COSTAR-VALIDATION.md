# Costar stage and critical-tag copy

Pinned upstream: `8555c08c823b856cbec4eb99ca84ea52a955836d`; locales:
`270ed0a2938b2c3e2e34e7de940e84aecadf548b`. Node 22 fixtures capture native
BattleScene.randBattleSeedInt before GameManager construction and restore it
following natural startup. Gameplay RNG is not mocked.

## Pinned behavior

PostSummonCopyAllyStatsAbAttr overrides canApply: doubles must be active and the
ally must be active on the field. The generic dispatcher retains fainting,
suppression, passive and gain checks. Simulation does not copy stages or tags.
The actual apply copies all seven battle stat stages, then handles Dragon Cheer,
Focus Energy and Laser Focus in that order. Existing user tags are retained.
New tags use constructor defaults rather than copying the ally's remaining turns
or source move.

The source adds Dragon Cheer, then assigns the ally's Cheer stage to the user's
**first CritBoostTag class match**. An existing Focus Energy tag can be that
first match. For example, Focus Energy can change from two stages to one while
the newly added Cheer keeps its constructor's type-dependent value. Selecting
only the new Cheer tag would change both state and later critical-hit queries.
The shared critical-hit query already selects the first class match as native.

## Bounded source comparisons

Each seed (777 and 2034) contains 65,536 raw dispatcher/gain cases. All 131,072
compare exactly, including full 125-field frames for eight real actors, ordered
critical tags and their metadata, queue/context/ability names, translated text
and both RNG streams. Initial tags are created by actual native addTag calls;
the C setup independently runs its actual tag constructors and verifies their
results. Only pre-fixture presentation is cleared before the observed callback.

Cases cover singles/doubles, primary/passive/duplicate abilities, simulation,
gain filtering, suppression, fainting, switched actors and real reserves;
all three critical tag combinations/order; Dragon and other types; Stellar and
Dragon Tera states; levels 1, 50, 256 and 1,000,000. Each seed records 12,288
primary and 2,048 passive applications, including 1,344 applications that select
an existing Focus Energy tag through the class lookup. The old core fails 45,056 cases.
Eighteen compiled production faults are caught and fourteen malformed raw
captures are rejected.

The complete-turn fixture uses real Baton Pass entry, preserving the outgoing
user's transferable critical tags before Costar acts. Each 8,192-turn matrix
requires at least 2,048 actual primary and 2,048 passive applications, plus
existing-first-class coverage. It retains 4,096 wide combat controls and the
existing damage/ally/spread/faint/switch floors. Incoming species, Tera and tag
order vary. Bench level/Tera fixture changes are reset before later controls.
The full matrices and normal integration gates must pass before merge.

Complete-turn reproduction now refuses missing/duplicate/reordered source IDs,
nonzero source exceptions, incomplete parsing, uncovered cases and text/state
mismatches. This strict wrapper is also used by the recent Gulp, Tera, Keldeo/
Mimicry and ally-entry scripts. A real compiled missing-Costar counterfactual
checks that a partially covered general-runner result cannot pass this wrapper.

## Reproduction and limits

```sh
. /workspace/shared/activate-parity.sh
sh battle-core/tools/costar-diff.sh /path/to/pinned-upstream 777
sh battle-core/tools/costar-diff.sh /path/to/pinned-upstream 2034
```

These fixtures do not close every fusion/Transform, challenge, persistent-tag
interaction or engine-capacity boundary. Poison Puppeteer, further enabled
callbacks, the complete source inventory and wide-level Endless acceptance
remain open. See [the completion checklist](COMPLETION-CHECKLIST.md).
