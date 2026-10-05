#!/bin/sh
# Additive bounded Dancer suite. Node 22; pinned checkout outside this repository.
set -eu
UP=${1:?upstream checkout}; N=${2:-2000}; SEED=${3:-777}
HERE=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
UP=$(CDPATH= cd -- "$UP" && pwd)
BUILD=${BUILD:-"$HERE/build"}
case "$(node --version)" in v22.*) ;; *) echo "Node 22 required" >&2; exit 2 ;; esac
make -s -C "$HERE" BUILD="$BUILD"
mkdir -p "$UP/test/tests/zz-dump"
cp "$HERE/tests/dancer.producer.test.ts" "$UP/test/tests/zz-dump/"
TRACE="$BUILD/dancer-$SEED.txt"
(cd "$UP" && DANCER_N="$N" DANCER_SEED="$SEED" DANCER_OUT="$TRACE" npx vitest run test/tests/zz-dump/dancer.producer.test.ts) > "$BUILD/dancer-$SEED-producer.log" 2>&1
sed -n '/DANCER cases=/p' "$BUILD/dancer-$SEED-producer.log"
TURNTEST_LOCALES="$UP/locales/en" "$BUILD/dancertest" "$TRACE"
