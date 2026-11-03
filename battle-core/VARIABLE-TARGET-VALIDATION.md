# Dynamic move targets in doubles

Pinned source `8555c08c823b856cbec4eb99ca84ea52a955836d` evaluates a move's
`VariableTargetAttr` against each living opponent before collecting selectable
targets. Expanding Force changes from `NEAR_OTHER` to `ALL_NEAR_ENEMIES` only on
Psychic Terrain while grounded. Tera Starstorm does so for Terapagos if it is
Terastallized or has a Tera pre-turn command.

`tests/variable-target.producer.test.ts` captures five real-source observations
from a natural doubles field. It captures
`BattleScene.prototype.randBattleSeedInt` before constructing `GameManager` and
restores it after battle start, before target selection. The checked-in source
fixture is compared with the native `pr_get_move_targets` implementation by
`tests/variable-target-test.c`; the source cases retain the ordered targets and
the `multiple` flag. All five cases match, including ordinary target selection,
the grounded Psychic Terrain spread conversion, Terapagos in base/Tera states,
and the pre-commanded Tera case.

This closes only these two dynamic target-selection branches. It does not prove
their full damage, UI selection, AI choice or complete-turn interactions, and it
does not close the broader doubles/source inventory. Reproduce the pinned
producer and native comparison on Node 24 (the pin's `.nvmrc`) with
`BUILD=/tmp/variable-target battle-core/tools/variable-target-diff.sh /workspace/shared/upstream-8555c08`.
