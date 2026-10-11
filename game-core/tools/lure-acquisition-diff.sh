#!/bin/sh
# Additive bounded canonical Soothe acquisition turn suite; Node 22 and pinned external upstream.
# usage: tools/lure-acquisition-diff.sh <upstream checkout> [N=2000] [seeds="777 9001 4242 31337"]
set -eu
UP=${1:?upstream checkout}; N=${2-2000}; SEEDS=${3-"777 9001 4242 31337"}
case "$N" in ''|*[!0-9]*) echo "N must be a positive integer" >&2; exit 2 ;; esac
[ "$N" -ge 120 ] || { echo "N must be >= 120" >&2; exit 2; }
[ -n "$SEEDS" ] || { echo "at least one seed required" >&2; exit 2; }
for SEED in $SEEDS; do
    case "$SEED" in ''|*[!0-9]*) echo "seed must be a non-negative integer" >&2; exit 2 ;; esac
done
python3 - "$N" "$SEEDS" <<'INPUTS'
import sys
n, seeds = sys.argv[1], sys.argv[2].split()
if not (120 <= int(n) <= 100000):
    raise SystemExit(2)
if len(set(seeds)) != len(seeds) or any(int(s) > 4294967295 for s in seeds):
    raise SystemExit(2)
INPUTS
HERE=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
UP=$(CDPATH= cd -- "$UP" && pwd)
[ "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d ] || { echo "upstream must be 8555c08c" >&2; exit 2; }
case "$(node --version)" in v22.*) ;; *) echo "Node 22 required" >&2; exit 2 ;; esac
BUILD=${BUILD:-"$HERE/build"}
make -s -C "$HERE" -j2 BUILD="$BUILD" "$BUILD/lure_acquisition_test"
mkdir -p "$UP/test/tests/zz-dump"
cp "$HERE/tests/lure-acquisition.producer.test.ts" "$UP/test/tests/zz-dump/"
OUT_DIR=$(mktemp -d "$BUILD/lure-acquisition-evidence.XXXXXX")
echo "Lure acquisition evidence: $OUT_DIR"
status=0
for SEED in $SEEDS; do
 TRACE="$OUT_DIR/caller-$N-$SEED.txt"
 (cd "$UP" && LUREACQ_N="$N" LUREACQ_SEED="$SEED" LUREACQ_OUT="$TRACE" node node_modules/vitest/vitest.mjs run test/tests/zz-dump/lure-acquisition.producer.test.ts --maxWorkers=1) > "$TRACE.source.log" 2>&1 && r=0 || r=$?
 if [ "$r" = 0 ]; then GC_LOCALES="$UP/locales/en" "$BUILD/lure_acquisition_test" "$TRACE" > "$TRACE.replay.log" 2>&1 && r=0 || r=$?; cat "$TRACE.replay.log"; fi
 echo "caller seed $SEED exit=$r"
 [ "$r" = 0 ] || [ "$status" != 0 ] || status=$r
done
exit "$status"
