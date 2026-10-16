#!/bin/sh
set -eu
UP=${1:?pinned upstream checkout}
HERE=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
UP=$(CDPATH= cd -- "$UP" && pwd)
[ "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d ] || exit 2
[ "$(git -C "$UP/locales" rev-parse HEAD)" = 270ed0a2938b2c3e2e34e7de940e84aecadf548b ] || exit 2
git -C "$UP" diff --quiet || exit 2
case "$(node --version)" in v22.*) ;; *) exit 2 ;; esac
make -s -C "$HERE" -j4 build/wide-level-test build/wide-boss-score-test
mkdir -p "$UP/test/tests/zz-dump"
cp "$HERE/tests/wide-level.producer.test.ts" "$HERE/tests/boss-score.producer.test.ts" "$UP/test/tests/zz-dump/"
TRACE="$HERE/build/wide-level.txt"; SCORE="$HERE/build/wide-score.txt"
(cd "$UP" && WIDELEVEL_OUT="$TRACE" node node_modules/vitest/vitest.mjs run test/tests/zz-dump/wide-level.producer.test.ts --maxWorkers=1) > "$HERE/build/wide-level-source.log" 2>&1
(cd "$UP" && BOSSSCORE_N=1000 BOSSSCORE_LEVEL_MAX=1000 BOSSSCORE_OUT="$SCORE" node node_modules/vitest/vitest.mjs run test/tests/zz-dump/boss-score.producer.test.ts --maxWorkers=1) > "$HERE/build/wide-score-source.log" 2>&1
"$HERE/build/wide-level-test" "$TRACE"
"$HERE/build/wide-boss-score-test" "$SCORE"
