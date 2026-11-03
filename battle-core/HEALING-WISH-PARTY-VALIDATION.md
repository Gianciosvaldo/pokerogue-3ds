# Healing Wish and Lunar Dance condition validation

Pinned upstream composes two party checks for Healing Wish and Lunar Dance. The
first compares the player's active party count with the current battler count;
the move-specific `failIfLastInPartyCondition` then checks whether the user's
own party has any active Pokémon off the field. The native C port represents
these as the ordered `player_bench_left` and `side_bench_left` closures, and the
combined result must preserve both source checks.

The retained source producer invokes the actual
`allMoves[MoveId.HEALING_WISH/LUNAR_DANCE].applyConditions` method and captures
each intrinsic condition separately. Its six cases cover an ordinary reserve,
no reserve, a living reserve while one field member is fainted, a fainted-only
reserve, a reserve on the opposing side, and an enemy user with a player-side
reserve. The native replay compares each closure result and the combined move
eligibility, with no unsupported reports.

Reproduce against the pinned upstream and locales with:

```sh
BUILD=/tmp/healing-wish-party \
  battle-core/tools/healing-wish-party-diff.sh /workspace/shared/upstream-8555c08
```

The producer captures `BattleScene.prototype.randBattleSeedInt` before
constructing `GameManager` and restores the native implementation after battle
startup. This closes these specific source-owned condition cases; broader
party, switch, and move-condition interactions remain under review.
