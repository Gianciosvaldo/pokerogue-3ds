# Poison Puppeteer status callback

Pinned upstream: `8555c08c823b856cbec4eb99ca84ea52a955836d`; locales:
`270ed0a2938b2c3e2e34e7de940e84aecadf548b`. Native battle RNG is captured
before constructing GameManager and restored after natural battle startup.
Gameplay RNG is not mocked.

## Pinned behavior

ConfusionOnStatusEffectAbAttr's override checks its constructor's effects Set,
a non-fainted opponent and opponent.canAddTag(CONFUSED). Generic dispatcher
fainting, suppression and passive gates still apply. The enabled constructor
contains POISON and TOXIC; the C implementation reads the generated Set members.
Simulation checks gates without drawing RNG or adding confusion.

The real apply draws an inclusive duration from 2 through 5 using the user's
battle RNG, then calls addTag on the opponent. It ignores the result of addTag.
canAddTag checks existing tags and ability immunity, while Misty Terrain is
checked later by the actual tag constructor. Therefore a grounded Misty target
can spend the duration draw and show the ability even though no tag is added.
Own Tempo prevents activation before this draw. Terrain immunity text depends
on the actual current MoveEffectPhase being a status move.

The call passes the opponent's ID as source, but the pinned ConfusedTag
constructor does not store this ID. Its stored sourceId remains undefined
(encoded as -1), and its absent source move defaults to NONE. Both are retained.

ObtainStatusEffectPhase now invokes the actual dispatcher with the original
status source as user and the status recipient as opponent, after the existing
PostSetStatus callbacks and ignoreAbilities reset.

## Bounded validation

The two raw matrices (seeds 777 and 2034) each contain 65,536 cases. All 131,072
compare exactly: full eight-actor frames, confusion presence/turns/source move/ID,
regular and dynamic queues, cached ability names, translated text, and both RNG
streams. Cases vary all field and bench roles, primary/passive/duplicate
abilities, simulation, suppression, fainting, existing confusion, all status
effect values, Own Tempo and Levitate (including passive/suppressed variants),
Misty Terrain, Flying/Ground Tera, phase context, and levels through 1,000,000.
Each seed records 9,856 applies, 3,376 added tags and 1,552 duration draws without
a resulting tag. All four durations occur.

Full-turn validation and final integration gates are in progress. These raw
matrices alone do not establish completion of the status lifecycle or battle
core. The complete source inventory and wide-level Endless acceptance remain open.

## Reproduction

```sh
. /workspace/shared/activate-parity.sh
sh battle-core/tools/poison-puppeteer-diff.sh /path/to/pinned-upstream 777
sh battle-core/tools/poison-puppeteer-diff.sh /path/to/pinned-upstream 2034
```

See [the completion checklist](COMPLETION-CHECKLIST.md) for remaining scope.
