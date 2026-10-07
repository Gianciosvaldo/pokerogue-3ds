#!/bin/sh
# Demonstrate that the Mint oracle catches an actual C source mutation.
# Never modifies the checkout or commits a mutant. Usage: mintplant.sh <trace>
set -eu
TRACE=$(CDPATH= cd -- "$(dirname "${1:?trace}")" && pwd)/$(basename "$1")
HERE=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
ROOT=$(CDPATH= cd -- "$HERE/.." && pwd)
BUILD=${BUILD:-/tmp/reward-mint-build}
make -s -j2 -C "$HERE" BUILD="$BUILD"
WORK=$(mktemp -d /tmp/reward-mint-plant.XXXXXX)
trap 'rm -rf "$WORK"' EXIT HUP INT TERM
python3 - "$HERE/src/mint.c" "$WORK/mint.c" <<'PY'
import pathlib, sys
source = pathlib.Path(sys.argv[1]).read_text()
old = 'm->custom_nature = nature; m->nature = nature;'
new = 'm->base_nature = nature; m->custom_nature = nature; m->nature = nature;'
assert source.count(old) == 1
pathlib.Path(sys.argv[2]).write_text(source.replace(old, new))
PY
CC=${CC:-cc}
CFLAGS=${CFLAGS:--O2 -g -std=c99 -Wall -Wextra -Wno-unused-parameter -ffp-contract=off}
# Resolve the mutant symbol before the normal archive; the normal Mint object is not pulled.
"$CC" $CFLAGS -I"$HERE/include" -I"$HERE/gen/out" -I"$ROOT/run-core/include" \
  -I"$ROOT/battle-core/include" -I"$ROOT/battle-core/gen/out" \
  "$HERE/tests/minttest.c" "$WORK/mint.c" "$BUILD/libreward.a" \
  "$BUILD/run-core/librun.a" "$BUILD/battle-core/libprcore.a" -lm -o "$WORK/minttest"
STATUS=0
"$WORK/minttest" "$TRACE" >"$WORK/result.log" 2>&1 || STATUS=$?
sed -n '/^MINT cases=/p' "$WORK/result.log"
[ "$STATUS" = 1 ] || { echo "Mint base-nature mutant was not rejected (exit $STATUS)" >&2; exit 1; }
echo 'Mint base-nature source mutant rejected (exit 1)'
