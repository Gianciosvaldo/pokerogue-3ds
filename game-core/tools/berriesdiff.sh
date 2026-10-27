#!/bin/sh
# Berries Abound (src/gc_berries.c) against the pinned upstream: the real encounter's three options on random parties, compared by tests/mystery_berries_diff_test.c: the boss, the five-berry shop, the race, the enraged boss, the berries callback and the experience.
# usage: tools/berriesdiff.sh <upstream checkout> [N=400] [seed=777]
set -eu
UP=${1:?pinned upstream dir}; N=${2:-400}; SEED=${3:-777}
HERE=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
UP=$(CDPATH= cd -- "$UP" && pwd)
NODE=${NODE:-node}; BUILD=${BUILD:-$HERE/build}
case "$("$NODE" --version)" in v22.*) ;; *) echo 'Node 22 required' >&2; exit 2;; esac
[ "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d ] || { echo 'Wrong upstream pin' >&2; exit 2; }
make -s -j2 -C "$HERE" BUILD="$BUILD" "$BUILD/mystery_berries_diff_test"
mkdir -p "$UP/test/tests/zz-dump"
cp "$HERE/tests/mystery-berries.producer.test.ts" "$UP/test/tests/zz-dump/"
TRACE=${GC_MYS_OUT:-$BUILD/berries-$SEED.txt}
rm -f "${TRACE%.txt}-berries.txt"
(cd "$UP" && GC_BRY_N="$N" GC_MYS_SEED="$SEED" GC_MYS_OUT="$TRACE" "$NODE" node_modules/vitest/vitest.mjs run --maxWorkers=1 test/tests/zz-dump/mystery-berries.producer.test.ts)
"$BUILD/mystery_berries_diff_test" "${TRACE%.txt}-berries.txt"
