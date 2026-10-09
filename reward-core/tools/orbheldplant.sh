#!/bin/sh
# Four actual Orb acquisition C source mutants; established checks untouched.
set -eu
TRACE=$(CDPATH= cd -- "$(dirname "${1:?trace}")" && pwd)/$(basename "$1")
HERE=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
ROOT=$(CDPATH= cd -- "$HERE/.." && pwd)
BUILD=${BUILD:-/tmp/reward-orbheld-build}
make -s -j2 -C "$HERE" BUILD="$BUILD"
WORK=$(mktemp -d /tmp/reward-orbheld-plant.XXXXXX)
trap 'rm -rf "$WORK"' EXIT HUP INT TERM
for mutation in wildcard class cap identity; do
  python3 - "$HERE/src/canonical_orb.c" "$WORK/canonical_orb.c" "$mutation" <<'PY'
import pathlib, sys
source = pathlib.Path(sys.argv[1]).read_text()
changes = {
 'wildcard': ('&& ctx->items[j].owner == party_idx)', '&& (ctx->items[j].owner == party_idx || (party_idx == 1 && ctx->items[j].owner == -1)))'),
 'class': ('if (is_orb(ctx->items[j].item)\n', 'if ((party_idx == 1 ? ctx->items[j].item == item : is_orb(ctx->items[j].item))\n'),
 'cap': ('return PR_REWARD_HELD_FULL;', '{ if (party_idx == 1) ctx->items[j].stack++; return PR_REWARD_HELD_FULL; }'),
 'identity': ('.item=item,.owner=party_idx', '.item=(party_idx == 1 ? PR_ITEM_TOXIC_ORB : item),.owner=party_idx'),
}
old,new = changes[sys.argv[3]]
assert source.count(old) == 1
pathlib.Path(sys.argv[2]).write_text(source.replace(old,new))
PY
  CC=${CC:-cc}
  CFLAGS=${CFLAGS:--O2 -g -std=c99 -Wall -Wextra -Wno-unused-parameter -ffp-contract=off}
  "$CC" $CFLAGS -I"$HERE/include" -I"$HERE/gen/out" -I"$ROOT/run-core/include" \
    -I"$ROOT/battle-core/include" -I"$ROOT/battle-core/gen/out" \
    "$HERE/tests/orbheldtest.c" "$WORK/canonical_orb.c" "$BUILD/libreward.a" \
    "$BUILD/run-core/librun.a" "$BUILD/battle-core/libprcore.a" -lm -o "$WORK/orbheldtest"
  STATUS=0
  "$WORK/orbheldtest" "$TRACE" >"$WORK/result.log" 2>&1 || STATUS=$?
  sed -n '/^ORBHELD cases=/p' "$WORK/result.log"
  [ "$STATUS" = 1 ] || { cat "$WORK/result.log"; echo "$mutation mutant not rejected with exit1 ($STATUS)" >&2; exit 1; }
  echo "Orb acquisition $mutation source mutant rejected (exit 1)"
done
