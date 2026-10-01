#!/bin/sh
# Generate upstream default save state and Dex updates, and compare against C game-core.
# Usage: tools/difftest.sh <upstream dir>
set -e
UP=${1:?upstream dir}
HERE=$(cd "$(dirname "$0")/.." && pwd)
BUILD=${BUILD:-$HERE/build}
mkdir -p "$BUILD" "$UP/test/tests/zz-dump"
make -s -C "$HERE" BUILD="$BUILD"
cp "$HERE/gen/dump-dex.test.ts" "$UP/test/tests/zz-dump/gc-dump-dex.test.ts"
cp "$HERE/gen/dump-dexupdates.test.ts" "$UP/test/tests/zz-dump/gc-dump-dexupdates.test.ts"
(cd "$UP" && DEX_DEFAULTS_OUT="$BUILD/gc_dex_defaults.txt" DEX_UPDATES_OUT="$BUILD/gc_dex_updates.txt" npx vitest run test/tests/zz-dump/gc-dump-dex.test.ts test/tests/zz-dump/gc-dump-dexupdates.test.ts >/dev/null)
"$BUILD/dextest" "$BUILD/gc_dex_defaults.txt"
"$BUILD/dexupdatetest" "$BUILD/gc_dex_updates.txt"
