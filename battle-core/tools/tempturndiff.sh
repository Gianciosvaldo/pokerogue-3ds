#!/bin/sh
# Additive bounded Temporary item turn suite; Node 22 and pinned external upstream.
set -eu
UP=${1:?upstream checkout}; N=${2:-2000}; SEED=${3:-777}
HERE=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
UP=$(CDPATH= cd -- "$UP" && pwd)
[ "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d ] || exit 2
BUILD=${BUILD:-"$HERE/build"}
case "$(node --version)" in v22.*) ;; *) echo "Node 22 required" >&2; exit 2 ;; esac
make -s -C "$HERE" -j2 BUILD="$BUILD"
mkdir -p "$UP/test/tests/zz-dump"
cp "$HERE/tests/temporary-turn.producer.test.ts" "$UP/test/tests/zz-dump/"
TRACE="$BUILD/temporary-turn-$SEED.txt"
(cd "$UP" && TEMPITEM_N="$N" TEMPITEM_SEED="$SEED" TEMPITEM_OUT="$TRACE" node node_modules/vitest/vitest.mjs run test/tests/zz-dump/temporary-turn.producer.test.ts --maxWorkers=1) > "$BUILD/temporary-turn-$SEED-producer.log" 2>&1
sed -n '/TEMPITEM cases=/p' "$BUILD/temporary-turn-$SEED-producer.log"
TURNTEST_LOCALES="$UP/locales/en" "$BUILD/tempturntest" "$TRACE"
