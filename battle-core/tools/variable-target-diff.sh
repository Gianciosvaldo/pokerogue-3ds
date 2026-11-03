#!/bin/sh
set -eu
UP=${1:?pinned upstream checkout}
HERE=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
UP=$(CDPATH= cd -- "$UP" && pwd)
[ "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d ] || exit 2
[ "$(git -C "$UP/locales" rev-parse HEAD)" = 270ed0a2938b2c3e2e34e7de940e84aecadf548b ] || exit 2
git -C "$UP" diff --quiet || exit 2
case "$(node --version)" in v24.*) ;; *) exit 2 ;; esac
BUILD=${BUILD:-$HERE/build}
mkdir -p "$BUILD" "$UP/test/tests/zz-dump"
PRODUCER="$UP/test/tests/zz-dump/variable-target-proof-$$.producer.test.ts"
TRACE="$BUILD/variable-target-source.txt"
trap 'rm -f "$PRODUCER"' EXIT HUP INT TERM
cp "$HERE/tests/variable-target.producer.test.ts" "$PRODUCER"
(cd "$UP" && VARIABLE_TARGET_OUT="$TRACE" node node_modules/vitest/vitest.mjs run "test/tests/zz-dump/$(basename "$PRODUCER")" --maxWorkers=1) > "$BUILD/variable-target-source.log" 2>&1
make -s -C "$HERE" BUILD="$BUILD/native" "$BUILD/native/variable-target-test"
"$BUILD/native/variable-target-test" "$TRACE"
