#!/bin/sh
# Three actual canonical held C mutants outside the checkout; each must fail replay.
set -eu
TRACE=$(CDPATH= cd -- "$(dirname "${1:?trace}")" && pwd)/$(basename "$1")
HERE=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
ROOT=$(CDPATH= cd -- "$HERE/.." && pwd)
BUILD=${BUILD:-/tmp/reward-wideheld-build}
make -s -j2 -C "$HERE" BUILD="$BUILD"
WORK=$(mktemp -d /tmp/reward-wideheld-plant.XXXXXX)
trap 'rm -rf "$WORK"' EXIT HUP INT TERM
for mutation in wildcard first cap; do
  python3 - "$HERE/src/canonical_held.c" "$WORK/canonical_held.c" "$mutation" <<'PY'
import pathlib, sys
source = pathlib.Path(sys.argv[1]).read_text()
changes = {
 'wildcard': ('&& ctx->items[j].owner == party_idx) {', '&& (ctx->items[j].owner == party_idx || (party_idx == 1 && ctx->items[j].owner == -1))) {'),
 'first': ('if (ctx->items[j].stack == cap) return PR_REWARD_HELD_FULL;', 'if (ctx->items[j].stack == cap) { if (party_idx == 1) continue; return PR_REWARD_HELD_FULL; }'),
 'cap': ('if (ctx->items[j].stack == cap)', 'if (ctx->items[j].stack == cap || (party_idx == 1 && ctx->items[j].stack == cap - 1))'),
}
old,new = changes[sys.argv[3]]
assert source.count(old) == 1
pathlib.Path(sys.argv[2]).write_text(source.replace(old,new))
PY
  CC=${CC:-cc}
  CFLAGS=${CFLAGS:--O2 -g -std=c99 -Wall -Wextra -Wno-unused-parameter -ffp-contract=off}
  "$CC" $CFLAGS -I"$HERE/include" -I"$HERE/gen/out" -I"$ROOT/run-core/include" \
    -I"$ROOT/battle-core/include" -I"$ROOT/battle-core/gen/out" \
    "$HERE/tests/wideheldtest.c" "$WORK/canonical_held.c" "$BUILD/libreward.a" \
    "$BUILD/run-core/librun.a" "$BUILD/battle-core/libprcore.a" -lm -o "$WORK/wideheldtest"
  STATUS=0
  "$WORK/wideheldtest" "$TRACE" >"$WORK/result.log" 2>&1 || STATUS=$?
  sed -n '/^WIDEHELD cases=/p' "$WORK/result.log"
  [ "$STATUS" = 1 ] || { cat "$WORK/result.log"; echo "$mutation mutant not rejected with exit1 ($STATUS)" >&2; exit 1; }
  echo "canonical held $mutation source mutant rejected (exit 1)"
done
