# Rototiller party-condition validation

Pinned upstream `8555c08c823b856cbec4eb99ca84ea52a955836d` defines the move
condition over both complete battle parties. Each party member qualifies when
its current type includes Grass and `isGrounded()` is true. The source does not
exclude benched or fainted party members. The native closure previously returned
an unported result for a non-field party member instead of evaluating it.

The closure now scans both party arrays and uses the native current-type and
grounding helpers. The seven retained producer observations cover no Grass,
fielded Grass, player bench Grass, fainted player bench Grass, enemy bench
Grass, airborne Grass-only parties, and empty party arrays. The producer
invokes the actual `allMoves[MoveId.ROTOTILLER].applyConditions` source method. The native replay
invokes the generated move condition closure and rejects unsupported reports.

To recreate the source trace and compare it byte-for-byte with the retained
fixture, then run the native replay:

```sh
BUILD=/tmp/rototiller-proof battle-core/tools/rototiller-diff.sh \
  /workspace/shared/upstream-8555c08
```

The script verifies both pinned source revisions and the upstream checkout's
clean state, requires Node 24, captures `randBattleSeedInt` before constructing
`GameManager`, and restores it after battle startup. This proof closes only the
party-wide Rototiller condition; it does not establish completion of move
conditions or battle-core more broadly.
