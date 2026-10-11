#!/bin/sh
# Additive bounded Psych Up (CopyStatsAttr) turn suite; Node 22 and pinned external upstream.
# usage: tools/psych-up-diff.sh <upstream checkout> [N=2000] [seeds="777 9001 4242 31337"]
set -eu
UP=${1:?upstream checkout}; N=${2-2000}; SEEDS=${3-"777 9001 4242 31337"}
case "$N" in ''|*[!0-9]*) echo "N must be a positive integer" >&2; exit 2 ;; esac
[ "$N" -ge 20 ] || { echo "N must be >= 20" >&2; exit 2; }
[ -n "$SEEDS" ] || { echo "at least one seed required" >&2; exit 2; }
for SEED in $SEEDS; do
    case "$SEED" in ''|*[!0-9]*) echo "seed must be a non-negative integer" >&2; exit 2 ;; esac
done
python3 - "$N" "$SEEDS" <<'INPUTS'
import sys
n, seeds = sys.argv[1], sys.argv[2].split()
if not (20 <= int(n) <= 2147483647):
    raise SystemExit(2)
if len(set(seeds)) != len(seeds) or any(int(s) > 4294967295 for s in seeds):
    raise SystemExit(2)
INPUTS
HERE=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
UP=$(CDPATH= cd -- "$UP" && pwd)
[ "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d ] || { echo "upstream must be 8555c08c" >&2; exit 2; }
case "$(node --version)" in v22.*) ;; *) echo "Node 22 required" >&2; exit 2 ;; esac
BUILD=${BUILD:-"$HERE/build"}
make -s -C "$HERE" -j2 BUILD="$BUILD" "$BUILD/psych-up-test"
mkdir -p "$UP/test/tests/zz-dump"
cp "$HERE/tests/psych-up.producer.test.ts" "$UP/test/tests/zz-dump/"
OUT_DIR=$(mktemp -d "$BUILD/psych-up-evidence.XXXXXX")
echo "Psych Up evidence: $OUT_DIR"
status=0
for SEED in $SEEDS; do
    TRACE="$OUT_DIR/psych-up-$N-$SEED.txt"
    rm -f "$TRACE"
    (cd "$UP" && PSYCH_UP_N="$N" PSYCH_UP_SEED="$SEED" PSYCH_UP_OUT="$TRACE" node node_modules/vitest/vitest.mjs run test/tests/zz-dump/psych-up.producer.test.ts --maxWorkers=1) > "$OUT_DIR/psych-up-$N-$SEED-producer.log" 2>&1 \
        && r=0 || r=$?
    if [ "$r" != 0 ]; then
        echo "seed $SEED: producer exit $r (see $OUT_DIR/psych-up-$N-$SEED-producer.log)" >&2
        [ "$status" != 0 ] || status=$r
        continue
    fi
    sed -n '/PSYCH_UP cases=/p' "$OUT_DIR/psych-up-$N-$SEED-producer.log"
    if TURNTEST_LOCALES="$UP/locales/en" "$BUILD/psych-up-test" "$TRACE" > "$OUT_DIR/psych-up-$N-$SEED-replay.log" 2>&1; then r=0; else r=$?; fi
    grep -E '^(cases|covered|texts of covered|crit probes|FAIL|CRITPROBE FAIL|TEXT DIFF|PARSE|STATS)' "$OUT_DIR/psych-up-$N-$SEED-replay.log" || true
    echo "seed $SEED: psych-up-test exit $r"
    if [ "$r" != 0 ] && [ "$status" = 0 ]; then status=$r; fi
done
exit $status
