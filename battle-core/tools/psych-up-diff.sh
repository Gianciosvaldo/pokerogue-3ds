#!/bin/sh
# Additive bounded Psych Up (CopyStatsAttr) turn suite; Node 22 and pinned external upstream.
# usage: tools/psych-up-diff.sh <upstream checkout> [N=2000] [seeds="777 9001 4242 31337"]
set -eu
UP=${1:?upstream checkout}; N=${2:-2000}; SEEDS=${3:-"777 9001 4242 31337"}
case "$N" in ''|*[!0-9]*) echo "N must be a positive integer" >&2; exit 2 ;; esac
[ "$N" -ge 20 ] || { echo "N must be >= 20" >&2; exit 2; }
for SEED in $SEEDS; do
    case "$SEED" in ''|*[!0-9]*) echo "seed must be a non-negative integer" >&2; exit 2 ;; esac
done
HERE=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
UP=$(CDPATH= cd -- "$UP" && pwd)
[ "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d ] || { echo "upstream must be 8555c08c" >&2; exit 2; }
case "$(node --version)" in v22.*) ;; *) echo "Node 22 required" >&2; exit 2 ;; esac
BUILD=${BUILD:-"$HERE/build"}
make -s -C "$HERE" -j2 BUILD="$BUILD" "$BUILD/psych-up-test"
mkdir -p "$UP/test/tests/zz-dump"
cp "$HERE/tests/psych-up.producer.test.ts" "$UP/test/tests/zz-dump/"
status=0
for SEED in $SEEDS; do
    TRACE="$BUILD/psych-up-$N-$SEED.txt"
    rm -f "$TRACE"
    (cd "$UP" && PSYCH_UP_N="$N" PSYCH_UP_SEED="$SEED" PSYCH_UP_OUT="$TRACE" node node_modules/vitest/vitest.mjs run test/tests/zz-dump/psych-up.producer.test.ts --maxWorkers=1) > "$BUILD/psych-up-$N-$SEED-producer.log" 2>&1 \
        || { echo "seed $SEED: producer failed (see $BUILD/psych-up-$N-$SEED-producer.log)" >&2; status=1; continue; }
    sed -n '/PSYCH_UP cases=/p' "$BUILD/psych-up-$N-$SEED-producer.log"
    if TURNTEST_LOCALES="$UP/locales/en" "$BUILD/psych-up-test" "$TRACE" > "$BUILD/psych-up-$N-$SEED-replay.log" 2>&1; then r=0; else r=$?; fi
    grep -E '^(cases|covered|texts of covered|crit probes|FAIL|CRITPROBE FAIL|TEXT DIFF|PARSE|STATS)' "$BUILD/psych-up-$N-$SEED-replay.log" || true
    echo "seed $SEED: psych-up-test exit $r"
    [ "$r" = 0 ] || status=1
done
exit $status
