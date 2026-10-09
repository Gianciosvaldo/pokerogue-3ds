#!/bin/sh
# Bounded multiheld canonical Multi Lens acquisition oracle, Node22 only.
set -eu
UP=${1:?upstream dir}; N=${2:-2000}; SEED=${3:-777}
case "$N" in ''|*[!0-9]*) echo 'integer N required' >&2; exit 2;; esac
case "$SEED" in ''|*[!0-9]*) echo 'unsigned integer seed required' >&2; exit 2;; esac
[ "$N" -ge 120 ] && [ "$N" -le 100000 ] && [ "$SEED" -le 4294967295 ] || exit 2
HERE=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
UP=$(CDPATH= cd -- "$UP" && pwd)
BUILD=${BUILD:-/tmp/reward-multiheld-build}
case "$(node --version)" in v22.*) ;; *) echo 'Node 22 required' >&2; exit 2 ;; esac
[ "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d ] || exit 2
mkdir -p "$BUILD" "$UP/test/tests/zz-dump"
make -s -j2 -C "$HERE" BUILD="$BUILD"
cp "$HERE/tests/multi-held.producer.test.ts" "$UP/test/tests/zz-dump/"
TRACE="$BUILD/multiheld-$SEED.txt"
rm -f "$TRACE"
(cd "$UP" && MULTI_HELD_N="$N" MULTI_HELD_SEED="$SEED" MULTI_HELD_OUT="$TRACE" \
  node node_modules/vitest/vitest.mjs run --maxWorkers=1 test/tests/zz-dump/multi-held.producer.test.ts \
  >"$BUILD/multiheld-$SEED.log" 2>&1) || { echo "producer failed: $BUILD/multiheld-$SEED.log" >&2; exit 2; }
"$BUILD/multiheldtest" "$TRACE"
