#!/bin/sh
# Dancing Lessons (src/gc_dance.c) against the pinned upstream: the real encounter's three options on random parties (levels, dance moves, fainted members, held items, shiny luck) in eleven biomes at waves 10n+2..4 (n >= 3), compared by tests/mystery_dance_diff_test.c: which options are offered and the third option's primary Pokemon, the run RNG after onInit and the option, the Oricorio onInit makes (form by biome, level, IVs, nature, ability, moves with Revelation Dance), the fight (its enemy from the data source as a boss, the Revelation Dance start effect, the Baton reward, the items, the post-summon stages and text), the lesson (the phase queued), the show (the move it learns, the Oricorio that joins the party), the rewards and the texts.
# usage: tools/dancediff.sh <upstream checkout> [N=400] [seed=777]
set -eu
UP=${1:?pinned upstream dir}; N=${2:-400}; SEED=${3:-777}
HERE=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
UP=$(CDPATH= cd -- "$UP" && pwd)
NODE=${NODE:-node}; BUILD=${BUILD:-$HERE/build}
case "$("$NODE" --version)" in v22.*) ;; *) echo 'Node 22 required' >&2; exit 2;; esac
[ "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d ] || { echo 'Wrong upstream pin' >&2; exit 2; }
make -s -j2 -C "$HERE" BUILD="$BUILD" "$BUILD/mystery_dance_diff_test"
mkdir -p "$UP/test/tests/zz-dump"
cp "$HERE/tests/mystery-dance.producer.test.ts" "$UP/test/tests/zz-dump/"
TRACE=${GC_MYS_OUT:-$BUILD/dance-$SEED.txt}
rm -f "${TRACE%.txt}-dance.txt"
(cd "$UP" && GC_DNC_N="$N" GC_MYS_SEED="$SEED" GC_MYS_OUT="$TRACE" "$NODE" node_modules/vitest/vitest.mjs run --maxWorkers=1 test/tests/zz-dump/mystery-dance.producer.test.ts)
"$BUILD/mystery_dance_diff_test" "${TRACE%.txt}-dance.txt"
