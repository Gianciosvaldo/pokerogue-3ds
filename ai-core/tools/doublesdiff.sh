#!/bin/sh
# Task 74: the doubles AI suite. Generates N upstream double-battle enemy commands and replays them
# through ai-core (build/doublestest). Node 22; pinned upstream checkout outside this repository.
# Usage: tools/doublesdiff.sh <upstream dir> [N] [seed]
set -eu
UP=${1:?upstream dir}; N=${2:-2000}; SEED=${3:-777}
HERE=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
UP=$(CDPATH= cd -- "$UP" && pwd)
BUILD=${BUILD:-/tmp/ai-core-build}
case "$(node --version)" in v22.*) ;; *) echo "ai-core: Node 22 is required, found $(node --version)" >&2; exit 2 ;; esac
mkdir -p "$BUILD" "$UP/test/tests/zz-dump"
make -s -C "$HERE" BUILD="$BUILD"
cp "$HERE/tests/doubles.producer.test.ts" "$UP/test/tests/zz-dump/"
TRACE="$BUILD/ai-doubles-$SEED.txt"
(cd "$UP" && AIDBL_SEED="$SEED" AIDBL_N="$N" AIDBL_OUT="$TRACE" \
    npx vitest run test/tests/zz-dump/doubles.producer.test.ts > "$BUILD/ai-doubles-$SEED-producer.log" 2>&1) \
  || { tail -30 "$BUILD/ai-doubles-$SEED-producer.log"; exit 1; }
grep "AI DOUBLES cases=" "$BUILD/ai-doubles-$SEED-producer.log" | sed 's/^ *//'
"$BUILD/doublestest" "$TRACE"
