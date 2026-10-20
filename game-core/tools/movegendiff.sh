#!/bin/sh
# The generic wild moveset generator (src/gc_movegen.c) and Move.calculateEffectivePower against the pinned upstream: upstream's own
# generateAndPopulateMoveset on random Pokemon (species, form, level, ability slot, gender, nature, IVs, weather), the moves, the
# stats and the next draws of the RNG compared, then every damaging move's effective power for Pokemon with each power ability.
# usage: tools/movegendiff.sh <upstream checkout> [N=1500] [seed=777]   (GC_MG_E = effective-power Pokemon, default 60)
set -eu
UP=${1:?pinned upstream dir}; N=${2:-1500}; SEED=${3:-777}
HERE=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
UP=$(CDPATH= cd -- "$UP" && pwd)
NODE=${NODE:-node}; BUILD=${BUILD:-$HERE/build}
case "$("$NODE" --version)" in v22.*) ;; *) echo 'Node 22 required' >&2; exit 2;; esac
[ "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d ] || { echo 'Wrong upstream pin' >&2; exit 2; }
make -s -j2 -C "$HERE" BUILD="$BUILD" "$BUILD/movegen_diff_test"
mkdir -p "$UP/test/tests/zz-dump"
cp "$HERE/tests/movegen.producer.test.ts" "$UP/test/tests/zz-dump/"
TRACE=${GC_MG_OUT:-$BUILD/movegen-$SEED.txt}
rm -f "$TRACE"
(cd "$UP" && GC_MG_N="$N" GC_MG_E="${GC_MG_E:-60}" GC_MG_SEED="$SEED" GC_MG_OUT="$TRACE" "$NODE" node_modules/vitest/vitest.mjs run --maxWorkers=1 test/tests/zz-dump/movegen.producer.test.ts)
"$BUILD/movegen_diff_test" "$TRACE"
