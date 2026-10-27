#!/bin/sh
# Slumbering Snorlax (src/gc_snorlax.c) against the pinned upstream: the real encounter's three options on random parties, compared by tests/mystery_snorlax_diff_test.c: onInit's boss config, the fight (boss, status, moves, items, start-of-battle Snore, rewards, run RNG), the rest and the steal.
# usage: tools/snorlaxdiff.sh <upstream checkout> [N=400] [seed=777]
set -eu
UP=${1:?pinned upstream dir}; N=${2:-400}; SEED=${3:-777}
HERE=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
UP=$(CDPATH= cd -- "$UP" && pwd)
NODE=${NODE:-node}; BUILD=${BUILD:-$HERE/build}
case "$("$NODE" --version)" in v22.*) ;; *) echo 'Node 22 required' >&2; exit 2;; esac
[ "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d ] || { echo 'Wrong upstream pin' >&2; exit 2; }
make -s -j2 -C "$HERE" BUILD="$BUILD" "$BUILD/mystery_snorlax_diff_test"
mkdir -p "$UP/test/tests/zz-dump"
cp "$HERE/tests/mystery-snorlax.producer.test.ts" "$UP/test/tests/zz-dump/"
TRACE=${GC_MYS_OUT:-$BUILD/snorlax-$SEED.txt}
rm -f "${TRACE%.txt}-snorlax.txt"
(cd "$UP" && GC_SNO_N="$N" GC_MYS_SEED="$SEED" GC_MYS_OUT="$TRACE" "$NODE" node_modules/vitest/vitest.mjs run --maxWorkers=1 test/tests/zz-dump/mystery-snorlax.producer.test.ts)
"$BUILD/mystery_snorlax_diff_test" "${TRACE%.txt}-snorlax.txt"
