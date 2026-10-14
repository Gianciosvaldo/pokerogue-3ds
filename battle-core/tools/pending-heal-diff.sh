#!/bin/sh
set -eu
UP=${1:?pinned upstream}; MODE=${2:-turn}; N=${3:-8192}; SEED=${4:-777}
HERE=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
UP=$(CDPATH= cd -- "$UP" && pwd)
[ "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d ] || exit 2
[ "$(git -C "$UP/locales" rev-parse HEAD)" = 270ed0a2938b2c3e2e34e7de940e84aecadf548b ] || exit 2
git -C "$UP" diff --quiet || exit 2
case "$(node --version)" in v22.*) ;; *) exit 2 ;; esac
case "$MODE" in api|double-api|turn|follow) ;; *) exit 2 ;; esac
make -s -C "$HERE" -j2 build/pending-heal-test build/pending-turn-test
mkdir -p "$UP/test/tests/zz-dump"
TRACE="$HERE/build/pending-heal-$MODE-$SEED.txt"
if [ "$MODE" = api ] || [ "$MODE" = double-api ]; then
 NAME=pending-heal
 cp "$HERE/tests/pending-heal.producer.test.ts" "$UP/test/tests/zz-dump/$NAME.producer.test.ts"
else
 NAME=pending-turn
 python3 "$HERE/tools/build-pending-turn-proof.py" producer "$UP/test/tests/zz-dump/$NAME.producer.test.ts"
fi
FOLLOW=0
[ "$MODE" != follow ] || FOLLOW=1
DOUBLE=0
[ "$MODE" != double-api ] || DOUBLE=1
(cd "$UP" && PENDING_FOLLOW="$FOLLOW" PENDING_DOUBLE="$DOUBLE" PENDING_N="$N" PENDING_SEED="$SEED" PENDING_OUT="$TRACE" PENDINGTURN_N="$N" PENDINGTURN_SEED="$SEED" PENDINGTURN_OUT="$TRACE" node node_modules/vitest/vitest.mjs run "test/tests/zz-dump/$NAME.producer.test.ts" --maxWorkers=1) > "$HERE/build/pending-heal-$MODE-source-$SEED.log" 2>&1
TURNTEST_LOCALES="$UP/locales/en" "$HERE/build/$NAME-test" "$TRACE"
