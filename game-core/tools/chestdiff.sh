#!/bin/sh
# The Mysterious Chest (src/gc_chest.c) against the pinned upstream: the real encounter's two options on random parties with the chest's roll steered into every reward band, both sides of each band edge and the trap, compared by tests/mystery_chest_diff_test.c: onInit's boss config, the roll, the shop each band sets and its message, the Pokemon that faints, the game over, the trap fight (boss, form, level, segments, moves, items, run RNG) and the leave.
# usage: tools/chestdiff.sh <upstream checkout> [N=400] [seed=777]
set -eu
UP=${1:?pinned upstream dir}; N=${2:-400}; SEED=${3:-777}
HERE=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
UP=$(CDPATH= cd -- "$UP" && pwd)
NODE=${NODE:-node}; BUILD=${BUILD:-$HERE/build}
case "$("$NODE" --version)" in v22.*) ;; *) echo 'Node 22 required' >&2; exit 2;; esac
[ "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d ] || { echo 'Wrong upstream pin' >&2; exit 2; }
make -s -j2 -C "$HERE" BUILD="$BUILD" "$BUILD/mystery_chest_diff_test"
mkdir -p "$UP/test/tests/zz-dump"
cp "$HERE/tests/mystery-chest.producer.test.ts" "$UP/test/tests/zz-dump/"
TRACE=${GC_MYS_OUT:-$BUILD/chest-$SEED.txt}
rm -f "${TRACE%.txt}-chest.txt"
(cd "$UP" && GC_CHE_N="$N" GC_MYS_SEED="$SEED" GC_MYS_OUT="$TRACE" "$NODE" node_modules/vitest/vitest.mjs run --maxWorkers=1 test/tests/zz-dump/mystery-chest.producer.test.ts)
"$BUILD/mystery_chest_diff_test" "${TRACE%.txt}-chest.txt"
