#!/bin/sh
# Generate upstream default save state and compare against C game-core system defaults.
# Usage: tools/difftest.sh <upstream dir>
set -e
UP=${1:?upstream dir}
HERE=$(cd "$(dirname "$0")/.." && pwd)
BUILD=${BUILD:-$HERE/build}
mkdir -p "$BUILD" "$UP/test/tests/zz-dump"
make -s -C "$HERE" BUILD="$BUILD"
cp "$HERE/gen/dump-dex.test.ts" "$UP/test/tests/zz-dump/gc-dump-dex.test.ts"
(cd "$UP" && DEX_DEFAULTS_OUT="$BUILD/gc_dex_defaults.txt" npx vitest run test/tests/zz-dump/gc-dump-dex.test.ts >/dev/null)
"$BUILD/dextest" "$BUILD/gc_dex_defaults.txt"
