#!/bin/sh
# Fiery Fallout (src/gc_fiery.c) against the pinned upstream: the real encounter's three options on random parties (Fire types, fire resistant abilities, fainted members, statuses, Leaf Guard users), the wave's own weather (none, Sunny, rain, the primal ones...) and held items at waves 10n+2..4 from 42 on, compared by tests/mystery_fiery_diff_test.c: which options are offered and the third option's primary Pokemon, the weather onInit leaves and its texts, the run RNG, the two Volcarona (levels, IVs, nature, ability, gender, moves, items), the Fire Spin effects, the rewards callback (the lead's attack type booster), the 20% damage, the burn and Heatproof, the experience and the post-summon stages.
# usage: tools/fierydiff.sh <upstream checkout> [N=400] [seed=777]
set -eu
UP=${1:?pinned upstream dir}; N=${2:-400}; SEED=${3:-777}
HERE=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
UP=$(CDPATH= cd -- "$UP" && pwd)
NODE=${NODE:-node}; BUILD=${BUILD:-$HERE/build}
case "$("$NODE" --version)" in v22.*) ;; *) echo 'Node 22 required' >&2; exit 2;; esac
[ "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d ] || { echo 'Wrong upstream pin' >&2; exit 2; }
make -s -j2 -C "$HERE" BUILD="$BUILD" "$BUILD/mystery_fiery_diff_test"
mkdir -p "$UP/test/tests/zz-dump"
cp "$HERE/tests/mystery-fiery.producer.test.ts" "$UP/test/tests/zz-dump/"
TRACE=${GC_MYS_OUT:-$BUILD/fiery-$SEED.txt}
rm -f "${TRACE%.txt}-fiery.txt"
(cd "$UP" && GC_FIE_N="$N" GC_MYS_SEED="$SEED" GC_MYS_OUT="$TRACE" "$NODE" node_modules/vitest/vitest.mjs run --maxWorkers=1 test/tests/zz-dump/mystery-fiery.producer.test.ts)
"$BUILD/mystery_fiery_diff_test" "${TRACE%.txt}-fiery.txt"
