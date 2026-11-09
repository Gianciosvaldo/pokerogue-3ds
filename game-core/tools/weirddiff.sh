#!/bin/sh
# Weird Dream (src/gc_weird.c: the transformation and the refusal) against the pinned upstream: the real encounter at waves 30..140 on four party layouts (3 to 6 members) with random species,
# levels, natures, IVs, moves with PP used, passives, Pokerus, luck, statuses, HP and held items of every kind, and part-filled dex entries for the species the party will become; onInit's
# transformations (the species by base stat total with the process-wide ranges, the new Pokemon the constructor made), then the option: the run RNG around it, the party (every field), the
# items, the dex entries, the rewards and the dialogue, compared by tests/mystery_weird_diff_test.c.
# usage: tools/weirddiff.sh <upstream checkout> [N=300] [seed=777]
set -eu
UP=${1:?pinned upstream dir}; N=${2:-300}; SEED=${3:-777}
HERE=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
UP=$(CDPATH= cd -- "$UP" && pwd)
NODE=${NODE:-node}; BUILD=${BUILD:-$HERE/build}
case "$("$NODE" --version)" in v22.*) ;; *) echo 'Node 22 required' >&2; exit 2;; esac
[ "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d ] || { echo 'Wrong upstream pin' >&2; exit 2; }
make -s -j2 -C "$HERE" BUILD="$BUILD" "$BUILD/mystery_weird_diff_test"
mkdir -p "$UP/test/tests/zz-dump" "$BUILD"
cp "$HERE/tests/mystery-weird.producer.test.ts" "$UP/test/tests/zz-dump/"
TRACE=${GC_MYS_OUT:-$BUILD/weird-$SEED.txt}
rm -f "${TRACE%.txt}-weird.txt"
(cd "$UP" && GC_W=42 GC_WD_N="$N" GC_MYS_SEED="$SEED" GC_MYS_OUT="$TRACE" "$NODE" node_modules/vitest/vitest.mjs run --maxWorkers=1 test/tests/zz-dump/mystery-weird.producer.test.ts)
"$BUILD/mystery_weird_diff_test" "${TRACE%.txt}-weird.txt"
