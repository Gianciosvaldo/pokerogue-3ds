#!/bin/sh
set -eu
UP=${1:?upstream checkout}; N=${2:-2000}; SEED=${3:-777}
HERE=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
UP=$(CDPATH= cd -- "$UP" && pwd)
[ "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d ] || exit 2
case "$(node --version)" in v22.*) ;; *) exit 2 ;; esac
make -s -C "$HERE" -j2 build/wonder-skin-test build/wonder-skin-helper-test
mkdir -p "$UP/test/tests/zz-dump"
python3 "$HERE/tools/build-wonder-skin-source.py" "$HERE/tests/speed-boost.producer.test.ts" "$UP/test/tests/zz-dump/wonder-skin.producer.test.ts"
TRACE="$HERE/build/wonder-skin-$SEED.txt"
rm -f "$TRACE"
(cd "$UP" && WONDERSKIN_N="$N" WONDERSKIN_SEED="$SEED" WONDERSKIN_OUT="$TRACE" node node_modules/vitest/vitest.mjs run test/tests/zz-dump/wonder-skin.producer.test.ts --maxWorkers=1) > "$HERE/build/wonder-skin-$SEED-source.log" 2>&1
sed -n '/WONDERSKIN cases=/p' "$HERE/build/wonder-skin-$SEED-source.log"
TURNTEST_LOCALES="$UP/locales/en" "$HERE/build/wonder-skin-test" "$TRACE"
