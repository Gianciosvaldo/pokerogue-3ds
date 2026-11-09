#!/bin/sh
# Pokemon.getTypes() with customPokemonData.types / fusionCustomPokemonData.types against the pinned upstream.
# usage: tools/custom-types-diff.sh <upstream checkout> [N=4000] [seeds="777 9001 4242 31337"]
set -eu
UP=${1:?upstream checkout}; N=${2-4000}; SEEDS=${3-"777 9001 4242 31337"}
case "$N" in ''|*[!0-9]*) echo "N must be a positive integer" >&2; exit 2 ;; esac
[ "$N" -ge 1000 ] || { echo "N must be >= 1000" >&2; exit 2; }
for SEED in $SEEDS; do case "$SEED" in ''|*[!0-9]*) echo "seed must be a non-negative integer" >&2; exit 2 ;; esac; done
HERE=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
UP=$(CDPATH= cd -- "$UP" && pwd)
[ "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d ] || { echo "upstream must be 8555c08c" >&2; exit 2; }
case "$(node --version)" in v22.*) ;; *) echo "Node 22 required" >&2; exit 2 ;; esac
BUILD=${BUILD:-"$HERE/build"}
make -s -C "$HERE" -j2 BUILD="$BUILD" "$BUILD/custom-types-test"
mkdir -p "$UP/test/tests/zz-dump" "$BUILD/custom-types"
cp "$HERE/tests/custom-types.producer.test.ts" "$UP/test/tests/zz-dump/"
status=0
for SEED in $SEEDS; do
    TRACE="$BUILD/custom-types/custom-types-$N-$SEED.txt"
    rm -f "$TRACE"
    (cd "$UP" && CUSTOM_TYPES_N="$N" CUSTOM_TYPES_SEED="$SEED" CUSTOM_TYPES_OUT="$TRACE" node node_modules/vitest/vitest.mjs run test/tests/zz-dump/custom-types.producer.test.ts --maxWorkers=1) > "$BUILD/custom-types/producer-$SEED.log" 2>&1 \
        && r=0 || r=$?
    if [ "$r" != 0 ]; then echo "seed $SEED: producer exit $r (see $BUILD/custom-types/producer-$SEED.log)" >&2; [ "$status" != 0 ] || status=$r; continue; fi
    if "$BUILD/custom-types-test" "$TRACE"; then r=0; else r=$?; fi
    echo "seed $SEED: custom-types-test exit $r"
    [ "$r" = 0 ] || [ "$status" != 0 ] || status=$r
done
exit $status
