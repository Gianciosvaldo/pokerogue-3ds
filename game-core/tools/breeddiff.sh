#!/bin/sh
# Uncommon Breed (src/gc_breed.c) against the pinned upstream: the real encounter's three options on random parties (levels, charming moves, fainted members, held berries, shiny luck) in ten biomes at waves 10n+2..4, compared by tests/mystery_breed_diff_test.c: which options are offered and the third option's primary Pokemon, the run RNG after onInit and the option, the Pokemon onInit rolls (species, level, IVs, nature, ability, moves with the egg move), the fight (its enemy from the data source, the egg move's start effect, the items, the post-summon stages and text), the berries the food takes, the second egg move, the IVs rolled again, the Pokemon that joins the party, the experience, the rewards and the texts.
# usage: tools/breeddiff.sh <upstream checkout> [N=400] [seed=777]
set -eu
UP=${1:?pinned upstream dir}; N=${2:-400}; SEED=${3:-777}
HERE=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
UP=$(CDPATH= cd -- "$UP" && pwd)
NODE=${NODE:-node}; BUILD=${BUILD:-$HERE/build}
case "$("$NODE" --version)" in v22.*) ;; *) echo 'Node 22 required' >&2; exit 2;; esac
[ "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d ] || { echo 'Wrong upstream pin' >&2; exit 2; }
make -s -j2 -C "$HERE" BUILD="$BUILD" "$BUILD/mystery_breed_diff_test"
mkdir -p "$UP/test/tests/zz-dump"
cp "$HERE/tests/mystery-breed.producer.test.ts" "$UP/test/tests/zz-dump/"
TRACE=${GC_MYS_OUT:-$BUILD/breed-$SEED.txt}
rm -f "${TRACE%.txt}-breed.txt"
(cd "$UP" && GC_BRD_N="$N" GC_MYS_SEED="$SEED" GC_MYS_OUT="$TRACE" "$NODE" node_modules/vitest/vitest.mjs run --maxWorkers=1 test/tests/zz-dump/mystery-breed.producer.test.ts)
"$BUILD/mystery_breed_diff_test" "${TRACE%.txt}-breed.txt"
