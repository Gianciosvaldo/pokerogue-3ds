#!/bin/sh
# A Trainer's Test (src/gc_trtest.c) against the pinned upstream: the real encounter at waves 100..180 on five party layouts, onInit's stat trainer, the two options (the fight made by
# initBattleWithEnemyConfig for Buck/Cheryl/Marley/Mira/Riley: gender roll, party levels, the six members and their closures' postProcess, enemy items; the reward shop and the Epic Egg; or
# the refusal's heal, Rare Egg and heal-only shop), the run RNG after onInit and after the option, compared by tests/mystery_trtest_diff_test.c.
# usage: tools/trtestdiff.sh <upstream checkout> [N=300] [seed=777]
set -eu
UP=${1:?pinned upstream dir}; N=${2:-300}; SEED=${3:-777}
HERE=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
UP=$(CDPATH= cd -- "$UP" && pwd)
NODE=${NODE:-node}; BUILD=${BUILD:-$HERE/build}
case "$("$NODE" --version)" in v22.*) ;; *) echo 'Node 22 required' >&2; exit 2;; esac
[ "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d ] || { echo 'Wrong upstream pin' >&2; exit 2; }
make -s -j2 -C "$HERE" BUILD="$BUILD" "$BUILD/mystery_trtest_diff_test"
mkdir -p "$UP/test/tests/zz-dump" "$BUILD"
cp "$HERE/tests/mystery-trtest.producer.test.ts" "$UP/test/tests/zz-dump/"
TRACE=${GC_MYS_OUT:-$BUILD/trtest-$SEED.txt}
rm -f "${TRACE%.txt}-trainer.txt"
(cd "$UP" && GC_TRN_N="$N" GC_MYS_SEED="$SEED" GC_MYS_OUT="$TRACE" "$NODE" node_modules/vitest/vitest.mjs run --maxWorkers=1 test/tests/zz-dump/mystery-trtest.producer.test.ts)
"$BUILD/mystery_trtest_diff_test" "${TRACE%.txt}-trainer.txt"
