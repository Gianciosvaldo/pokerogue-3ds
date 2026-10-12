#!/bin/sh
set -eu
UP=${1:?upstream checkout}
HERE=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
UP=$(CDPATH= cd -- "$UP" && pwd)
[ "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d ] || exit 2
case "$(node --version)" in v22.*) ;; *) exit 2 ;; esac
make -s -C "$HERE" -j2 build/consecutive-move-test
mkdir -p "$UP/test/tests/zz-dump"
cp "$HERE/tests/consecutive-move.producer.test.ts" "$UP/test/tests/zz-dump/"
TRACE="$HERE/build/consecutive-move.txt"
rm -f "$TRACE"
(cd "$UP" && HISTORY_OUT="$TRACE" node node_modules/vitest/vitest.mjs run test/tests/zz-dump/consecutive-move.producer.test.ts --maxWorkers=1) > "$HERE/build/consecutive-move-source.log" 2>&1
sed -n '/HISTORY cases=/p' "$HERE/build/consecutive-move-source.log"
TURNTEST_LOCALES="$UP/locales/en" "$HERE/build/consecutive-move-test" "$TRACE"
