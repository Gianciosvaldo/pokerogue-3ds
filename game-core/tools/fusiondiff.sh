#!/bin/sh
# DNA Splicers (src/gc_fusion.c) against the pinned upstream: the real FusePokemonModifierType filter and FusePokemonModifier ->
# PlayerPokemon.fuse() on random parties with held items, compared by tests/fusion_diff_test.c.
# usage: tools/fusiondiff.sh <upstream checkout> [N=600] [seed=777]
set -eu
UP=${1:?pinned upstream dir}; N=${2:-600}; SEED=${3:-777}
HERE=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
UP=$(CDPATH= cd -- "$UP" && pwd)
NODE=${NODE:-node}; BUILD=${BUILD:-$HERE/build}
case "$("$NODE" --version)" in v22.*) ;; *) echo 'Node 22 required' >&2; exit 2;; esac
[ "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d ] || { echo 'Wrong upstream pin' >&2; exit 2; }
make -s -j2 -C "$HERE" BUILD="$BUILD" "$BUILD/fusion_diff_test"
mkdir -p "$UP/test/tests/zz-dump"
cp "$HERE/tests/fusion.producer.test.ts" "$UP/test/tests/zz-dump/"
TRACE=${GC_FUSION_OUT:-$BUILD/fusion-$SEED.txt}
rm -f "$TRACE"
(cd "$UP" && GC_FUSION_N="$N" GC_FUSION_SEED="$SEED" GC_FUSION_OUT="$TRACE" "$NODE" node_modules/vitest/vitest.mjs run --maxWorkers=1 test/tests/zz-dump/fusion.producer.test.ts)
"$BUILD/fusion_diff_test" "$TRACE"
