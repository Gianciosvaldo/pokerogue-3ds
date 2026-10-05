#!/bin/sh
# Task 73 doubles turn suite. Node 22; pinned checkout outside this repository.
# usage: [DOUBLES_RICH=1] tools/doublesdiff.sh <upstream checkout> [cases] [seed]
set -eu
UP=${1:?upstream checkout}; N=${2:-2000}; SEED=${3:-777}
HERE=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
UP=$(CDPATH= cd -- "$UP" && pwd)
BUILD=${BUILD:-"$HERE/build"}
case "$(node --version)" in v22.*) ;; *) echo "Node 22 required" >&2; exit 2 ;; esac
make -s -C "$HERE" BUILD="$BUILD"
mkdir -p "$UP/test/tests/zz-dump"
cp "$HERE/tests/doubles.producer.test.ts" "$UP/test/tests/zz-dump/"
RICH=${DOUBLES_RICH:-0}
case "$RICH" in 1) NAME="doubles-rich-$SEED" ;; *) RICH=0; NAME="doubles-$SEED" ;; esac
TRACE="$BUILD/$NAME.txt"
(cd "$UP" && DOUBLES_RICH="$RICH" DOUBLES_N="$N" DOUBLES_SEED="$SEED" DOUBLES_OUT="$TRACE" npx vitest run test/tests/zz-dump/doubles.producer.test.ts) > "$BUILD/$NAME-producer.log" 2>&1
sed -n '/DOUBLES cases=/p' "$BUILD/$NAME-producer.log"
TURNTEST_LOCALES="$UP/locales/en" "$BUILD/doublestest" "$TRACE"
