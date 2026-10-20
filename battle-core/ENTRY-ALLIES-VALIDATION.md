# Hospitality and Curious Medicine

Pin: `8555c08c823b856cbec4eb99ca84ea52a955836d`; locales:
`270ed0a2938b2c3e2e34e7de940e84aecadf548b`. Node 22 producers capture native
BattleScene.randBattleSeedInt before constructing GameManager and restore it
following natural doubles startup. Gameplay RNG is not mocked.

Hospitality's registered PostSummonAllyHealAbAttr checks only that its ally is
active on the field. It queues PokemonHealPhase even at full HP. Its amount is
one quarter of the **user's** maximum HP, rounded with toDmgValue; the target's
maximum HP is not the basis. The registered showAnim option is true. Its text
uses the ally's affixed name and the user's raw name. Curious Medicine has the
same ally gate; it directly clears all seven battle stat stages and queues the
native message, even if all stages were already zero. Both overrides omit the
parent canApply gate and skip mutation in simulation. Generic ability checks,
passives, suppression and ability-gain filtering remain owned by the dispatcher.

The source fixtures use actual applyAbAttrs and applyOnGainAbAttrs. Each seed
contains the original 32,768 cases plus 16,384 switching-state boundaries;
version 2 transport requires at least all 49,152 cases. They compare all eight
actor frames, queue levels/order, cached ability/name metadata, complete heal
options/message/amount and both RNG streams. Cases include singles and doubles,
field and real party reserves, primary/passive/duplicate abilities, fainting,
suppression, simulation, full/missing ally HP, mixed/zero stat stages and levels
1, 50, 256 and 1,000,000. Historical version 1 captures remain readable at their
original 32,768-case floor. The old core fails 22,528 of the original cases.

The complete-turn proof uses actual Baton Pass switches into Hospitality and
Curious Medicine, including passives. Each 8,192-turn matrix retains 4,096 wide
combat controls and requires at least 1,024 native applications of each new
callback in each ability slot (primary and passive). Passthrough observers count
these during the native apply, before the dispatcher clears its reused params.
Existing wide damage, ally/spread, faint and switch coverage floors
remain active. Full source comparisons and normal integration gates must pass
before merge; raw callback samples alone do not establish owning-turn parity.

An initial switch fixture retained the framework's Ball Fetch override, so its
primary cases did not activate the intended ability. Those captures remain
supplemental passive evidence. The corrected fixture disables that override for
the entry cases, verifies each incoming ability, restores it for the unchanged
wide controls and regenerates both entire matrices. No expected source state
is rewritten and no required cases are dropped.

```sh
. /workspace/shared/activate-parity.sh
sh battle-core/tools/entry-allies-diff.sh /path/to/pinned-upstream 777
sh battle-core/tools/entry-allies-diff.sh /path/to/pinned-upstream 2034
```

This slice does not implement Costar's stage/critical-tag copy, close other
remaining callbacks, establish all fusion/Transform interactions or finish
wide-level Endless acceptance. See [the completion checklist](COMPLETION-CHECKLIST.md).
