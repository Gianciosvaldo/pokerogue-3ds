#!/bin/sh
set -eu
UP=${1:?pinned upstream}; N=${2:-1000}; SEED=${3:-777}; LEVEL_MAX=${4:-255}
HERE=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
UP=$(CDPATH= cd -- "$UP" && pwd)
[ "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d ] || exit 2
case "$(node --version)" in v22.*) ;; *) exit 2 ;; esac
make -s -C "$HERE" -j2 build/boss-score-test
mkdir -p "$UP/test/tests/zz-dump"
cp "$HERE/tests/boss-score.producer.test.ts" "$UP/test/tests/zz-dump/"
TRACE="$HERE/build/boss-score-$SEED.txt"
rm -f "$TRACE"
(cd "$UP" && BOSSSCORE_LEVEL_MAX="$LEVEL_MAX" BOSSSCORE_N="$N" BOSSSCORE_SEED="$SEED" BOSSSCORE_OUT="$TRACE" node node_modules/vitest/vitest.mjs run test/tests/zz-dump/boss-score.producer.test.ts --maxWorkers=1) > "$HERE/build/boss-score-$SEED-source.log" 2>&1
sed -n '/BOSSSCORE cases=/p' "$HERE/build/boss-score-$SEED-source.log"
"$HERE/build/boss-score-test" "$TRACE"
