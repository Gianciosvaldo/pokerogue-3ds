#!/bin/sh
# Three actual money-item C mutants outside the checkout; each must fail replay.
set -eu
TRACE=$(CDPATH= cd -- "$(dirname "${1:?trace}")" && pwd)/$(basename "$1")
HERE=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
ROOT=$(CDPATH= cd -- "$HERE/.." && pwd)
BUILD=${BUILD:-/tmp/reward-moneyitem-build}
make -s -j2 -C "$HERE" BUILD="$BUILD"
WORK=$(mktemp -d /tmp/reward-moneyitem-plant.XXXXXX)
trap 'rm -rf "$WORK"' EXIT HUP INT TERM
for mutation in multiplier order cap; do
  python3 - "$HERE/src/money_item.c" "$WORK/money_item.c" "$mutation" <<'PY'
import pathlib, sys
source = pathlib.Path(sys.argv[1]).read_text()
changes = {
 'multiplier': ('multiplier = 2.5;', 'multiplier = 2;'),
 'order': ('double amount = pr_run_get_money_reward', 'for (int j = 0; j < count / 2; j++) { int tmp = stacks[j]; stacks[j] = stacks[count - j - 1]; stacks[count - j - 1] = tmp; }\n    double amount = pr_run_get_money_reward'),
 'cap': ('balance = fmin(before + amount, MAX_SAFE_INTEGER);', 'balance = item == PR_ITEM_BIG_NUGGET ? before + amount : fmin(before + amount, MAX_SAFE_INTEGER);'),
}
old,new = changes[sys.argv[3]]
assert source.count(old) == 1
pathlib.Path(sys.argv[2]).write_text(source.replace(old,new))
PY
  CC=${CC:-cc}
  CFLAGS=${CFLAGS:--O2 -g -std=c99 -Wall -Wextra -Wno-unused-parameter -ffp-contract=off}
  "$CC" $CFLAGS -I"$HERE/include" -I"$HERE/gen/out" -I"$ROOT/run-core/include" \
    -I"$ROOT/battle-core/include" -I"$ROOT/battle-core/gen/out" \
    "$HERE/tests/moneyitemtest.c" "$WORK/money_item.c" "$BUILD/libreward.a" \
    "$BUILD/run-core/librun.a" "$BUILD/battle-core/libprcore.a" -lm -o "$WORK/moneyitemtest"
  STATUS=0
  "$WORK/moneyitemtest" "$TRACE" >"$WORK/result.log" 2>&1 || STATUS=$?
  sed -n '/^MONEYITEM cases=/p' "$WORK/result.log"
  [ "$STATUS" = 1 ] || { cat "$WORK/result.log"; echo "$mutation mutant not rejected with exit1 ($STATUS)" >&2; exit 1; }
  echo "Money-item $mutation source mutant rejected (exit 1)"
done
