#!/bin/sh
# The Expert Pokemon Breeder (src/gc_breeder.c) against the pinned upstream: the real encounter on five party layouts (levels, natures, friendship with ties, fainted members, held items and shiny luck) in sixteen biomes (Space has its own three Cleffa) at waves 25..175 never ending in 0 or 1, all three options, a win and a loss each: the three offered Pokemon and their egg counts, the run RNG after onInit and after the option, the trainer (class, gender), its three Pokemon made by the real initBattleWithEnemyConfig (species, levels, IVs, natures, abilities, moves, Tera type, friendship, held Soothe Bells, ids), the instant Tera slot, the lone Pokemon and its items during the fight, then after a win (friendship, the others and their items back in upstream order, the eggs, the outro texts) or a loss (nothing earned, friendship 0, the failed outro), compared by tests/mystery_breeder_diff_test.c.
# usage: tools/breederdiff.sh <upstream checkout> [N=400] [seed=777]
set -eu
UP=${1:?pinned upstream dir}; N=${2:-400}; SEED=${3:-777}
HERE=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
UP=$(CDPATH= cd -- "$UP" && pwd)
NODE=${NODE:-node}; BUILD=${BUILD:-$HERE/build}
case "$("$NODE" --version)" in v22.*) ;; *) echo 'Node 22 required' >&2; exit 2;; esac
[ "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d ] || { echo 'Wrong upstream pin' >&2; exit 2; }
make -s -j2 -C "$HERE" BUILD="$BUILD" "$BUILD/mystery_breeder_diff_test"
mkdir -p "$UP/test/tests/zz-dump"
cp "$HERE/tests/mystery-breeder.producer.test.ts" "$UP/test/tests/zz-dump/"
TRACE=${GC_MYS_OUT:-$BUILD/breeder-$SEED.txt}
rm -f "${TRACE%.txt}-breeder.txt"
(cd "$UP" && GC_BRD_N="$N" GC_MYS_SEED="$SEED" GC_MYS_OUT="$TRACE" "$NODE" node_modules/vitest/vitest.mjs run --maxWorkers=1 test/tests/zz-dump/mystery-breeder.producer.test.ts)
"$BUILD/mystery_breeder_diff_test" "${TRACE%.txt}-breeder.txt"
