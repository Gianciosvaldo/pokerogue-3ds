#!/bin/sh
# Additive inventory bridge oracle using unchanged battle-core Multi Lens fixtures.
set -eu
UP=${1:?pinned upstream dir}; N=${2:-2000}; SEED=${3:-777}
HERE=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
UP=$(CDPATH= cd -- "$UP" && pwd)
NODE=${NODE:-node}
case "$("$NODE" --version)" in v22.*) ;; *) echo 'Node 22 required' >&2; exit 2;; esac
[ "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d ] || { echo 'Wrong upstream pin' >&2; exit 2; }
BUILD=${BUILD:-$HERE/build}
mkdir -p "$BUILD" "$UP/test/tests/zz-dump"
make -s -j2 -C "$HERE" BUILD="$BUILD" "$BUILD/multi_diff_test" "$BUILD/multi_host_test"
cp "$HERE/../battle-core/tests/multi-lens.producer.test.ts" "$UP/test/tests/zz-dump/"
TRACE=${GC_MULTI_OUT:-$BUILD/game-multi-$SEED.txt}
rm -f "$TRACE"
PRODUCER_LOG="$TRACE.producer.log"
REPLAY_LOG="$TRACE.replay.log"
if ! (cd "$UP" && MULTILENS_N="$N" MULTILENS_SEED="$SEED" MULTILENS_OUT="$TRACE" "$NODE" node_modules/vitest/vitest.mjs run --maxWorkers=1 test/tests/zz-dump/multi-lens.producer.test.ts) >"$PRODUCER_LOG" 2>&1; then
    cat "$PRODUCER_LOG"; exit 1
fi
cat "$PRODUCER_LOG"
if ! TURNTEST_LOCALES="$UP/locales/en" "$BUILD/multi_diff_test" "$TRACE" >"$REPLAY_LOG" 2>&1; then
    cat "$REPLAY_LOG"; exit 1
fi
cat "$REPLAY_LOG"
SOURCE=$(sed -n 's/.*countApplied=\([0-9]*\).*damageApplied=\([0-9]*\).*/\1 \2/p' "$PRODUCER_LOG")
CORE=$(awk '/^Multi actual count gains / {split($5,c,"/"); split($8,d,"/"); print c[1]+c[2],d[1]+d[2]}' "$REPLAY_LOG")
[ -n "$SOURCE" ] && [ "$SOURCE" = "$CORE" ] || { echo "Multi actual-effect counters differ: source $SOURCE core $CORE" >&2; exit 1; }
"$BUILD/multi_host_test"
