#!/bin/sh
set -eu
UP=${1:?upstream checkout}; N=${2:-2000}; SEED=${3:-777}
HERE=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
UP=$(CDPATH= cd -- "$UP" && pwd)
[ "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d ] || exit 2
case "$(node --version)" in v22.*) ;; *) exit 2 ;; esac
make -s -C "$HERE" -j2 build/protean-test
mkdir -p "$UP/test/tests/zz-dump"
python3 "$HERE/tools/build-protean-source.py" "$HERE/tests/speed-boost.producer.test.ts" "$UP/test/tests/zz-dump/protean.producer.test.ts"
TRACE="$HERE/build/protean-$SEED.txt"
rm -f "$TRACE"
(cd "$UP" && PROTEAN_N="$N" PROTEAN_SEED="$SEED" PROTEAN_OUT="$TRACE" node node_modules/vitest/vitest.mjs run test/tests/zz-dump/protean.producer.test.ts --maxWorkers=1) > "$HERE/build/protean-$SEED-source.log" 2>&1
sed -n '/PROTEAN cases=/p' "$HERE/build/protean-$SEED-source.log"
TURNTEST_LOCALES="$UP/locales/en" "$HERE/build/protean-test" "$TRACE"
