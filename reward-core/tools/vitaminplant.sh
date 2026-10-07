#!/bin/sh
# Demonstrate that the Vitamin oracle catches an actual C source mutation.
# Never modifies the checkout or commits a mutant. Usage: vitaminplant.sh <trace>
set -eu
TRACE=$(CDPATH= cd -- "$(dirname "${1:?trace}")" && pwd)/$(basename "$1")
HERE=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
ROOT=$(CDPATH= cd -- "$HERE/.." && pwd)
BUILD=${BUILD:-/tmp/reward-vitamin-build}
make -s -j2 -C "$HERE" BUILD="$BUILD"
WORK=$(mktemp -d /tmp/reward-vitamin-plant.XXXXXX)
trap 'rm -rf "$WORK"' EXIT HUP INT TERM
python3 - "$HERE/src/vitamin.c" "$WORK/vitamin.c" <<'PY'
import pathlib, sys
source = pathlib.Path(sys.argv[1]).read_text()
old = 'int cap = ctx->party[party_idx].ivs[stat];'
new = 'int cap = 31;'
assert source.count(old) == 1
pathlib.Path(sys.argv[2]).write_text(source.replace(old, new))
PY
CC=${CC:-cc}
CFLAGS=${CFLAGS:--O2 -g -std=c99 -Wall -Wextra -Wno-unused-parameter -ffp-contract=off}
# Resolve the mutant symbol before the normal archive; the normal Vitamin object is not pulled.
"$CC" $CFLAGS -I"$HERE/include" -I"$HERE/gen/out" -I"$ROOT/run-core/include" \
  -I"$ROOT/battle-core/include" -I"$ROOT/battle-core/gen/out" \
  "$HERE/tests/vitamintest.c" "$WORK/vitamin.c" "$BUILD/libreward.a" \
  "$BUILD/run-core/librun.a" "$BUILD/battle-core/libprcore.a" -lm -o "$WORK/vitamintest"
STATUS=0
"$WORK/vitamintest" "$TRACE" >"$WORK/result.log" 2>&1 || STATUS=$?
sed -n '/^VITAMIN cases=/p' "$WORK/result.log"
[ "$STATUS" = 1 ] || { echo "Vitamin IV-cap mutant was not rejected (exit $STATUS)" >&2; exit 1; }
echo 'Vitamin IV-cap source mutant rejected (exit 1)'
