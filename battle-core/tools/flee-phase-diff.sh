#!/bin/sh
set -eu
UP=${1:?pinned upstream}; N=${2:-2000}; SEED=${3:-777}; TRANSIENT=${4:-0}
HERE=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
UP=$(CDPATH= cd -- "$UP" && pwd)
[ "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d ] || exit 2
[ "$(git -C "$UP/locales" rev-parse HEAD)" = 270ed0a2938b2c3e2e34e7de940e84aecadf548b ] || exit 2
case "$(node --version)" in v22.*) ;; *) exit 2 ;; esac
case "$TRANSIENT" in 0|1) ;; *) exit 2 ;; esac
make -s -C "$HERE" -j2 build/flee-phase-test
mkdir -p "$UP/test/tests/zz-dump"
cp "$HERE/tests/flee-phase.producer.test.ts" "$UP/test/tests/zz-dump/"
TRACE="$HERE/build/flee-phase-$SEED-$TRANSIENT.txt"
rm -f "$TRACE"
(cd "$UP" && FLEEPHASE_TRANSIENT="$TRANSIENT" FLEEPHASE_N="$N" FLEEPHASE_SEED="$SEED" FLEEPHASE_OUT="$TRACE" node node_modules/vitest/vitest.mjs run test/tests/zz-dump/flee-phase.producer.test.ts --maxWorkers=1) > "$HERE/build/flee-phase-$SEED-$TRANSIENT-source.log" 2>&1
sed -n '/FLEEPHASE cases=/p' "$HERE/build/flee-phase-$SEED-$TRANSIENT-source.log"
TURNTEST_LOCALES="$UP/locales/en" "$HERE/build/flee-phase-test" "$TRACE"
