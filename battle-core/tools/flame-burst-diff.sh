#!/bin/sh
set -eu
UP=${1:?pinned upstream checkout}
HERE=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
UP=$(CDPATH= cd -- "$UP" && pwd)
[ "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d ] || exit 2
[ "$(git -C "$UP/locales" rev-parse HEAD)" = 270ed0a2938b2c3e2e34e7de940e84aecadf548b ] || exit 2
git -C "$UP" diff --quiet || exit 2
case "$(node --version)" in v22.*) ;; *) exit 2 ;; esac
make -s -C "$HERE" build/flame-burst-test
mkdir -p "$UP/test/tests/zz-dump"
cp "$HERE/tests/flame-burst.producer.test.ts" "$UP/test/tests/zz-dump/"
TRACE="$HERE/build/flame-burst.txt"
(cd "$UP" && FLAMEBURST_OUT="$TRACE" node node_modules/vitest/vitest.mjs run test/tests/zz-dump/flame-burst.producer.test.ts --maxWorkers=1) > "$HERE/build/flame-burst-source.log" 2>&1
TURNTEST_LOCALES="$UP/locales/en" "$HERE/build/flame-burst-test" "$TRACE"
