#!/bin/sh
# Actual C mutants outside the checkout, all must fail the real-class replay.
set -eu
TRACE=$(CDPATH= cd -- "$(dirname "${1:?trace}")" && pwd)/$(basename "$1")
HERE=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
ROOT=$(CDPATH= cd -- "$HERE/.." && pwd)
BUILD=${BUILD:-/tmp/reward-lure-build}
make -s -j2 -C "$HERE" BUILD="$BUILD"
WORK=$(mktemp -d /tmp/reward-lure-plant.XXXXXX)
trap 'rm -rf "$WORK"' EXIT HUP INT TERM
for mutation in refresh expiry chance; do
  python3 - "$HERE/src/lure.c" "$WORK/lure.c" "$mutation" <<'PY'
import pathlib, sys
source = pathlib.Path(sys.argv[1]).read_text()
changes = {
 'refresh': ('ctx->items[j].battle_count = max;', 'ctx->items[j].battle_count = max == 15 ? 14 : max;'),
 'expiry': ('--it.battle_count == 0', '--it.battle_count < 2'),
 'chance': ('value /= 4;', 'value /= ctx->items[j].item == PR_ITEM_MAX_LURE ? 2 : 4;'),
}
old,new = changes[sys.argv[3]]
assert source.count(old) == 1
pathlib.Path(sys.argv[2]).write_text(source.replace(old,new))
PY
  CC=${CC:-cc}
  CFLAGS=${CFLAGS:--O2 -g -std=c99 -Wall -Wextra -Wno-unused-parameter -ffp-contract=off}
  "$CC" $CFLAGS -I"$HERE/include" -I"$HERE/gen/out" -I"$ROOT/run-core/include" \
    -I"$ROOT/battle-core/include" -I"$ROOT/battle-core/gen/out" \
    "$HERE/tests/luretest.c" "$WORK/lure.c" "$BUILD/libreward.a" \
    "$BUILD/run-core/librun.a" "$BUILD/battle-core/libprcore.a" -lm -o "$WORK/luretest"
  STATUS=0
  "$WORK/luretest" "$TRACE" >"$WORK/result.log" 2>&1 || STATUS=$?
  sed -n '/^LURE cases=/p' "$WORK/result.log"
  [ "$STATUS" = 1 ] || { cat "$WORK/result.log"; echo "$mutation mutant not rejected with exit1 ($STATUS)" >&2; exit 1; }
  echo "Lure $mutation source mutant rejected (exit 1)"
done
