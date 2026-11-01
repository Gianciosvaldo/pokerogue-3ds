#!/bin/sh
# The evil teams' fixed fights in the run (src/gc_encounter.c) against the pinned upstream: the cases are the ones tests/evil.producer.test.ts writes (the real
# fixed config's trainer of the Town Youngster at wave 5 and the grunt fights at 35, 62, 64 and 112, plus the admins and bosses when DIFF_WAVES names them: their
# templates, levels and every member's constructor state), replayed through gc_new_battle as the app starts the wave and compared by tests/gym_e4_diff_test.c
# (the same comparator as tools/gymdiff.sh). Upstream rolls a grunt's gender and double variant with Math.random: the producer hands it known rolls and the
# comparator pins the same ones in the run.
# usage: tools/evildiff.sh <upstream checkout> [N=3000] [seed=777]   (DIFF_WAVES=5,35,62,64,112 by default)
set -eu
UP=${1:?pinned upstream dir}; N=${2:-3000}; SEED=${3:-777}
HERE=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
UP=$(CDPATH= cd -- "$UP" && pwd)
NODE=${NODE:-node}; BUILD=${BUILD:-$HERE/build}
case "$("$NODE" --version)" in v22.*) ;; *) echo 'Node 22 required' >&2; exit 2;; esac
[ "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d ] || { echo 'Wrong upstream pin' >&2; exit 2; }
make -s -j2 -C "$HERE" BUILD="$BUILD" "$BUILD/gym_e4_diff_test"
mkdir -p "$UP/test/tests/zz-dump"
cp "$HERE/tests/evil.producer.test.ts" "$UP/test/tests/zz-dump/"
TRACE=${GC_EVIL_OUT:-$BUILD/evil-$SEED.txt}
rm -f "$TRACE"
(cd "$UP" && DIFF_N="$N" DIFF_SEED="$SEED" DIFF_OUT="$TRACE" DIFF_WAVES="${DIFF_WAVES:-5,35,62,64,112}" "$NODE" node_modules/vitest/vitest.mjs run --maxWorkers=1 test/tests/zz-dump/evil.producer.test.ts >/dev/null)
"$BUILD/gym_e4_diff_test" "$TRACE"
