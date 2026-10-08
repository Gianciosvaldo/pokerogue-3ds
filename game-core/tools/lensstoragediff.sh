#!/bin/sh
# Additive valid-holder ModifierData storage oracle; Node22 and pinned source.
set -eu
UP=${1:?pinned upstream dir}; N=${2:-2000}; SEED=${3:-777}
HERE=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
UP=$(CDPATH= cd -- "$UP" && pwd)
NODE=${NODE:-node}
case "$("$NODE" --version)" in v22.*) ;; *) echo 'Node 22 required' >&2; exit 2;; esac
[ "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d ] || { echo 'Wrong upstream pin' >&2; exit 2; }
BUILD=${BUILD:-$HERE/build}
mkdir -p "$BUILD" "$UP/test/tests/zz-dump"
make -s -j2 -C "$HERE" BUILD="$BUILD" "$BUILD/lens_host_test"
cp "$HERE/tests/lens-storage.producer.test.ts" "$UP/test/tests/zz-dump/"
TRACE=${GC_LENS_STORAGE_OUT:-$BUILD/lens-storage.txt}
rm -f "$TRACE"
(cd "$UP" && GC_LENS_STORAGE_OUT="$TRACE" "$NODE" node_modules/vitest/vitest.mjs run --maxWorkers=1 test/tests/zz-dump/lens-storage.producer.test.ts)
"$BUILD/lens_host_test" --storage "$TRACE"
