#!/bin/sh
# The Winstrate Challenge (src/gc_winstrate.c) against the pinned upstream: the real encounter at waves 100..180 on four party layouts, the chain of five trainer fights (the gender roll, the party levels,
# every Pokemon's form, nature, ability slot, moves and held items and their locks), the run RNG after every step, the voucher and the Macho Brace reward, and the refusal's Rarer Candy,
# compared by tests/mystery_winstrate_diff_test.c.
# usage: tools/winstratediff.sh <upstream checkout> [N=350] [seed=777]
set -eu
UP=${1:?pinned upstream dir}; N=${2:-350}; SEED=${3:-777}
HERE=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
UP=$(CDPATH= cd -- "$UP" && pwd)
NODE=${NODE:-node}; BUILD=${BUILD:-$HERE/build}
case "$("$NODE" --version)" in v22.*) ;; *) echo 'Node 22 required' >&2; exit 2;; esac
[ "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d ] || { echo 'Wrong upstream pin' >&2; exit 2; }
make -s -j2 -C "$HERE" BUILD="$BUILD" "$BUILD/mystery_winstrate_diff_test"
mkdir -p "$UP/test/tests/zz-dump"
cp "$HERE/tests/mystery-winstrate.producer.test.ts" "$UP/test/tests/zz-dump/"
TRACE=${GC_MYS_OUT:-$BUILD/winstrate-$SEED.txt}
rm -f "${TRACE%.txt}-winstrate.txt"
(cd "$UP" && GC_WS_N="$N" GC_MYS_SEED="$SEED" GC_MYS_OUT="$TRACE" "$NODE" node_modules/vitest/vitest.mjs run --maxWorkers=1 test/tests/zz-dump/mystery-winstrate.producer.test.ts)
"$BUILD/mystery_winstrate_diff_test" "${TRACE%.txt}-winstrate.txt"
