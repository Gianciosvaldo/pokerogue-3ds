#!/bin/sh
set -eu
UP=${1:?upstream checkout}; N=${2:-2000}; SEED=${3:-777}
HERE=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
UP=$(CDPATH= cd -- "$UP" && pwd)
[ "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d ] || exit 2
case "$(node --version)" in v22.*) ;; *) exit 2 ;; esac
make -s -C "$HERE" -j2 build/knockout-stats-test
mkdir -p "$UP/test/tests/zz-dump"
python3 "$HERE/tools/build-knockout-stats-source.py" "$HERE/tests/fell-stinger.producer.test.ts" "$UP/test/tests/zz-dump/knockout-stats.producer.test.ts"
TRACE="$HERE/build/knockout-stats-$SEED.txt"
rm -f "$TRACE"
(cd "$UP" && KNOCKOUT_STATS_N="$N" KNOCKOUT_STATS_SEED="$SEED" KNOCKOUT_STATS_OUT="$TRACE" node node_modules/vitest/vitest.mjs run test/tests/zz-dump/knockout-stats.producer.test.ts --maxWorkers=1) > "$HERE/build/knockout-stats-$SEED-source.log" 2>&1
sed -n '/KNOCKOUT_STATS cases=/p; /KNOCKOUT_STATS reached=/p' "$HERE/build/knockout-stats-$SEED-source.log"
TURNTEST_LOCALES="$UP/locales/en" "$HERE/build/knockout-stats-test" "$TRACE"
