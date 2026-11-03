#!/bin/sh
# Teleporting Hijinks (src/gc_teleport.c) against the pinned upstream: the real encounter's three options on random parties (Steel and Electric Pokemon, fainted members, money short of, exactly at and above the price, held items) at waves 10n+2..4, compared by tests/mystery_teleport_diff_test.c: which options are offered, the price, the run RNG, the money paid, the new biome and the arena it leaves, the rewards, the experience of the helper, the boss (level, IVs, nature, ability, moves, bars, items), the texts and the post-summon stage changes.
# usage: tools/strongdiff.sh <upstream checkout> [N=400] [seed=777]
set -eu
UP=${1:?pinned upstream dir}; N=${2:-400}; SEED=${3:-777}
HERE=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
UP=$(CDPATH= cd -- "$UP" && pwd)
NODE=${NODE:-node}; BUILD=${BUILD:-$HERE/build}
case "$("$NODE" --version)" in v22.*) ;; *) echo 'Node 22 required' >&2; exit 2;; esac
[ "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d ] || { echo 'Wrong upstream pin' >&2; exit 2; }
make -s -j2 -C "$HERE" BUILD="$BUILD" "$BUILD/mystery_teleport_diff_test"
mkdir -p "$UP/test/tests/zz-dump"
cp "$HERE/tests/mystery-teleport.producer.test.ts" "$UP/test/tests/zz-dump/"
TRACE=${GC_MYS_OUT:-$BUILD/teleport-$SEED.txt}
rm -f "${TRACE%.txt}-teleport.txt"
(cd "$UP" && GC_TLP_N="$N" GC_MYS_SEED="$SEED" GC_MYS_OUT="$TRACE" "$NODE" node_modules/vitest/vitest.mjs run --maxWorkers=1 test/tests/zz-dump/mystery-teleport.producer.test.ts)
"$BUILD/mystery_teleport_diff_test" "${TRACE%.txt}-teleport.txt"
