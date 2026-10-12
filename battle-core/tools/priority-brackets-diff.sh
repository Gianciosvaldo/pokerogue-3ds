#!/bin/sh
set -eu
UP=${1:?upstream checkout}; N=${2:-2000}; SEED=${3:-777}
HERE=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
UP=$(CDPATH= cd -- "$UP" && pwd)
[ "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d ] || exit 2
case "$(node --version)" in v22.*) ;; *) exit 2 ;; esac
make -s -C "$HERE" -j2 build/priority-brackets-test
mkdir -p "$UP/test/tests/zz-dump"
python3 "$HERE/tools/build-priority-brackets-source.py" "$HERE/tests/speed-boost.producer.test.ts" "$UP/test/tests/zz-dump/priority-brackets.producer.test.ts"
TRACE="$HERE/build/priority-brackets-$SEED.txt"
rm -f "$TRACE"
(cd "$UP" && BRACKETS_N="$N" BRACKETS_SEED="$SEED" BRACKETS_OUT="$TRACE" node node_modules/vitest/vitest.mjs run test/tests/zz-dump/priority-brackets.producer.test.ts --maxWorkers=1) > "$HERE/build/priority-brackets-$SEED-source.log" 2>&1
sed -n '/BRACKETS cases=/p' "$HERE/build/priority-brackets-$SEED-source.log"
TURNTEST_LOCALES="$UP/locales/en" "$HERE/build/priority-brackets-test" "$TRACE"
