#!/bin/sh
set -eu
UP=${1:?pinned upstream}; N=${2:-2000}; SEED=${3:-777}; KIND=${4:-both}
HERE=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
UP=$(CDPATH= cd -- "$UP" && pwd)
[ "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d ] || exit 2
[ "$(git -C "$UP/locales" rev-parse HEAD)" = 270ed0a2938b2c3e2e34e7de940e84aecadf548b ] || exit 2
git -C "$UP" diff --quiet || exit 2
case "$(node --version)" in v22.*) ;; *) exit 2 ;; esac
case "$KIND" in both|boundary|turn) ;; *) exit 2 ;; esac
make -s -C "$HERE" -j2 build/transform-test build/transform-turn-test
mkdir -p "$UP/test/tests/zz-dump"
if [ "$KIND" != turn ]; then
  cp "$HERE/tests/transform.producer.test.ts" "$UP/test/tests/zz-dump/"
  TRACE="$HERE/build/transform-boundary-$SEED.txt"
  (cd "$UP" && TRANSFORM_N="$N" TRANSFORM_SEED="$SEED" TRANSFORM_OUT="$TRACE" node node_modules/vitest/vitest.mjs run test/tests/zz-dump/transform.producer.test.ts --maxWorkers=1) > "$HERE/build/transform-boundary-source-$SEED.log" 2>&1
  "$HERE/build/transform-test" "$TRACE"
fi
if [ "$KIND" != boundary ]; then
  python3 "$HERE/tools/build-transform-turn-proof.py" producer "$UP/test/tests/zz-dump/transform-turn.producer.test.ts"
  TRACE="$HERE/build/transform-turn-$SEED.txt"
  (cd "$UP" && TRANSFORMTURN_N="$N" TRANSFORMTURN_SEED="$SEED" TRANSFORMTURN_OUT="$TRACE" node node_modules/vitest/vitest.mjs run test/tests/zz-dump/transform-turn.producer.test.ts --maxWorkers=1) > "$HERE/build/transform-turn-source-$SEED.log" 2>&1
  TURNTEST_LOCALES="$UP/locales/en" "$HERE/build/transform-turn-test" "$TRACE"
fi
