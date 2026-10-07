#!/bin/sh
# Bounded Mint application oracle. Adds a producer; established gates remain unchanged.
# Usage: tools/mintdiff.sh <pinned upstream dir> [N=2000] [seed=777]
set -eu
UP=${1:?upstream dir}; N=${2:-2000}; SEED=${3:-777}
HERE=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
UP=$(CDPATH= cd -- "$UP" && pwd)
BUILD=${BUILD:-/tmp/reward-mint-build}
case "$(node --version)" in v22.*) ;; *) echo 'Node 22 required' >&2; exit 2 ;; esac
[ "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d ] \
  || { echo 'Wrong upstream pin' >&2; exit 2; }
mkdir -p "$BUILD" "$UP/test/tests/zz-dump"
make -s -j2 -C "$HERE" BUILD="$BUILD"
cp "$HERE/tests/mint.producer.test.ts" "$UP/test/tests/zz-dump/"
TRACE="$BUILD/mint-$SEED.txt"
rm -f "$TRACE"
(cd "$UP" && MINT_N="$N" MINT_SEED="$SEED" MINT_OUT="$TRACE" \
  node node_modules/vitest/vitest.mjs run --maxWorkers=1 test/tests/zz-dump/mint.producer.test.ts \
  >"$BUILD/mint-$SEED.log" 2>&1) \
  || { echo "Mint producer failed; see $BUILD/mint-$SEED.log" >&2; exit 2; }
"$BUILD/minttest" "$TRACE"
