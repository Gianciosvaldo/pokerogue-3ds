#!/bin/sh
# Actual source APIs/cleanup or owning singles turns; tracked upstream untouched.
set -eu
UP=${1:?pinned upstream};MODE=${2:-gas};N=${3:-8192};SEED=${4:-777}
HERE=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
UP=$(CDPATH= cd -- "$UP" && pwd)
[ "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d ] || exit 2
[ "$(git -C "$UP/locales" rev-parse HEAD)" = 270ed0a2938b2c3e2e34e7de940e84aecadf548b ] || exit 2
git -C "$UP" diff --quiet || exit 2
case "$(node --version)" in v22.*) ;; *) exit 2 ;; esac
case "$MODE" in gas|weather|bulk|imprison|cleanup|turn) ;; *) exit 2 ;; esac
make -s -C "$HERE" -j2 build/gas-lifecycle-test build/arena-remove-all-test build/imprison-test build/reset-arena-test build/gas-turn-test
mkdir -p "$UP/test/tests/zz-dump"
TRACE="$HERE/build/arena-$MODE-$SEED.txt"
case "$MODE" in
 gas) NAME=gas-lifecycle;; weather) NAME=gas-weather;; bulk) NAME=arena-remove-all;; imprison) NAME=imprison;; cleanup) NAME=arena-cleanup;; turn) NAME=gas-turn;;
esac
if [ "$MODE" = turn ]; then
 python3 "$HERE/tools/build-gas-turn-proof.py" producer "$UP/test/tests/zz-dump/$NAME.producer.test.ts"
else
 cp "$HERE/tests/$NAME.producer.test.ts" "$UP/test/tests/zz-dump/$NAME.producer.test.ts"
fi
(cd "$UP" && GASLIFE_N="$N" GASLIFE_SEED="$SEED" GASLIFE_OUT="$TRACE" ARENAREMOVE_N="$N" ARENAREMOVE_SEED="$SEED" ARENAREMOVE_OUT="$TRACE" IMPRISON_N="$N" IMPRISON_OUT="$TRACE" RESETARENA_N="$N" RESETARENA_SEED="$SEED" RESETARENA_OUT="$TRACE" GASTURN_N="$N" GASTURN_SEED="$SEED" GASTURN_OUT="$TRACE" node node_modules/vitest/vitest.mjs run "test/tests/zz-dump/$NAME.producer.test.ts" --maxWorkers=1) > "$HERE/build/arena-$MODE-source-$SEED.log" 2>&1
export TURNTEST_LOCALES="$UP/locales/en"
case "$MODE" in
 gas) "$HERE/build/gas-lifecycle-test" "$TRACE";;
 weather) "$HERE/build/gas-lifecycle-test" "$TRACE" --weather;;
 bulk) "$HERE/build/arena-remove-all-test" "$TRACE";;
 imprison) "$HERE/build/imprison-test" "$TRACE";;
 cleanup) "$HERE/build/reset-arena-test" "$TRACE" --effects;;
 turn) "$HERE/build/gas-turn-test" "$TRACE";;
esac
