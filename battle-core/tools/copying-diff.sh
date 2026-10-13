#!/bin/sh
set -eu
UP=${1:?pinned upstream}; N=${2:-2000}; SEED=${3:-777}; KIND=${4:-both}
HERE=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
UP=$(CDPATH= cd -- "$UP" && pwd)
[ "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d ] || exit 2
[ "$(git -C "$UP/locales" rev-parse HEAD)" = 270ed0a2938b2c3e2e34e7de940e84aecadf548b ] || exit 2
git -C "$UP" diff --quiet || exit 2
case "$(node --version)" in v22.*) ;; *) exit 2 ;; esac
case "$KIND" in api|turn|both|transition) ;; *) exit 2 ;; esac
make -s -C "$HERE" -j2 build/copying-test build/copying-turn-test
mkdir -p "$UP/test/tests/zz-dump"
if [ "$KIND" = api ] || [ "$KIND" = both ]; then
 cp "$HERE/tests/copying.producer.test.ts" "$UP/test/tests/zz-dump/"
 TRACE="$HERE/build/copying-api-$SEED.txt"
 (cd "$UP" && COPYING_N="$N" COPYING_SEED="$SEED" COPYING_OUT="$TRACE" node node_modules/vitest/vitest.mjs run test/tests/zz-dump/copying.producer.test.ts --maxWorkers=1) > "$HERE/build/copying-api-source-$SEED.log" 2>&1
 TURNTEST_LOCALES="$UP/locales/en" "$HERE/build/copying-test" "$TRACE"
fi
if [ "$KIND" != api ]; then
 MODE=""; [ "$KIND" != transition ] || MODE=transition
 FILE="copying-$KIND.producer.test.ts"
 python3 "$HERE/tools/build-copying-turn-proof.py" producer "$UP/test/tests/zz-dump/$FILE" $MODE
 TRACE="$HERE/build/copying-$KIND-$SEED.txt"
 (cd "$UP" && COPYINGTURN_N="$N" COPYINGTURN_SEED="$SEED" COPYINGTURN_OUT="$TRACE" node node_modules/vitest/vitest.mjs run "test/tests/zz-dump/$FILE" --maxWorkers=1) > "$HERE/build/copying-$KIND-source-$SEED.log" 2>&1
 TURNTEST_LOCALES="$UP/locales/en" "$HERE/build/copying-turn-test" "$TRACE"
fi
