#!/bin/sh
# Mystery encounter actions (the real encounters' own option code, card 82's harness) against the pinned upstream,
# one seed per call. Families so far: Department Store Sale, Shady Vitamin Dealer.
# usage: tools/mysterydiff.sh <upstream checkout> [N=2000] [seed=777]
set -eu
UP=${1:?pinned upstream dir}; N=${2:-2000}; SEED=${3:-777}
HERE=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
UP=$(CDPATH= cd -- "$UP" && pwd)
NODE=${NODE:-node}; BUILD=${BUILD:-$HERE/build}
case "$("$NODE" --version)" in v22.*) ;; *) echo 'Node 22 required' >&2; exit 2;; esac
[ "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d ] || { echo 'Wrong upstream pin' >&2; exit 2; }
make -s -j2 -C "$HERE" BUILD="$BUILD" "$BUILD/mystery_actions_diff_test" "$BUILD/mystery_shady_diff_test"
mkdir -p "$UP/test/tests/zz-dump"
cp "$HERE/tests/mystery-actions.producer.test.ts" "$UP/test/tests/zz-dump/"
TRACE=${GC_MYS_OUT:-$BUILD/game-mystery-$SEED.txt}
SHADY=${TRACE%.txt}-shady.txt
rm -f "$TRACE" "$SHADY"
(cd "$UP" && GC_MYS_N="$N" GC_MYS_SEED="$SEED" GC_MYS_OUT="$TRACE" "$NODE" node_modules/vitest/vitest.mjs run --maxWorkers=1 test/tests/zz-dump/mystery-actions.producer.test.ts)
"$BUILD/mystery_actions_diff_test" "$TRACE"
"$BUILD/mystery_shady_diff_test" "$SHADY"
