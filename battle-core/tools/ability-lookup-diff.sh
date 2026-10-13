#!/bin/sh
set -eu
UP=${1:?pinned upstream}; N=${2:-2000}; SEED=${3:-777}
HERE=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
UP=$(CDPATH= cd -- "$UP" && pwd)
[ "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d ] || exit 2
[ "$(git -C "$UP/locales" rev-parse HEAD)" = 270ed0a2938b2c3e2e34e7de940e84aecadf548b ] || exit 2
git -C "$UP" diff --quiet || exit 2
case "$(node --version)" in v22.*) ;; *) exit 2 ;; esac
make -s -C "$HERE" -j2 build/ability-lookup-test
mkdir -p "$UP/test/tests/zz-dump"
cp "$HERE/tests/ability-lookup.producer.test.ts" "$UP/test/tests/zz-dump/"
TRACE="$HERE/build/ability-lookup-$SEED.txt"
(cd "$UP" && ABLOOKUP_N="$N" ABLOOKUP_SEED="$SEED" ABLOOKUP_OUT="$TRACE" node node_modules/vitest/vitest.mjs run test/tests/zz-dump/ability-lookup.producer.test.ts --maxWorkers=1) > "$HERE/build/ability-lookup-source-$SEED.log" 2>&1
TURNTEST_LOCALES="$UP/locales/en" "$HERE/build/ability-lookup-test" "$TRACE"
