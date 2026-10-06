#!/bin/sh
# Generate N upstream reward-application cases and replay them through game-core's gc_port_apply_reward on a real run:
#  - reward-core's own apply producer, unchanged (healing, revive, PP and PP Up items; Task 60), and
#  - game-core's candy producer, tests/candy.producer.test.ts (Rare Candy and Rarer Candy; Task 96), and
#  - game-core's pokeball producer, tests/pokeball.producer.test.ts (Poke Balls taken and given, with the text; Task 96).
# Usage: tools/applydiff.sh <upstream dir> [N] [seed]
# The upstream checkout is the one battle-core/tools/setup-upstream.sh prepares (pinned commit); needs Node 22.
set -e
UP=${1:?upstream dir}; N=${2:-4000}; SEED=${3:-777}
HERE=$(cd "$(dirname "$0")/.." && pwd)
BUILD=${BUILD:-$HERE/build}
NODE=${NODE:-node}
test "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d
test "$("$NODE" -p 'process.versions.node.split(".")[0]')" = 22
mkdir -p "$BUILD" "$UP/test/tests/zz-dump"
make -s -C "$HERE" BUILD="$BUILD" "$BUILD/reward_apply_diff_test"
cp "$HERE/../reward-core/tests/apply.producer.test.ts" "$UP/test/tests/zz-dump/gc-apply.producer.test.ts"
cp "$HERE/tests/candy.producer.test.ts" "$UP/test/tests/zz-dump/gc-candy.producer.test.ts"
cp "$HERE/tests/pokeball.producer.test.ts" "$UP/test/tests/zz-dump/gc-pokeball.producer.test.ts"
(cd "$UP" && DIFF_SEED=$SEED DIFF_N=$N DIFF_OUT="$BUILD/gc-apply-$SEED.txt" npx vitest run --no-cache test/tests/zz-dump/gc-apply.producer.test.ts >/dev/null)
(cd "$UP" && DIFF_SEED=$SEED DIFF_N=$N DIFF_OUT="$BUILD/gc-candy-$SEED.txt" npx vitest run --no-cache test/tests/zz-dump/gc-candy.producer.test.ts >/dev/null)
(cd "$UP" && DIFF_SEED=$SEED DIFF_N=$N DIFF_OUT="$BUILD/gc-pokeball-$SEED.txt" npx vitest run --no-cache test/tests/zz-dump/gc-pokeball.producer.test.ts >/dev/null)
# every comparator runs even when an earlier one found mismatches; the script's exit code and TOTAL say the sum
TOTAL=0
FAIL=0
run_compare() {
    echo "== $1 seed $SEED"
    OUTF="$BUILD/gc-compare-$SEED.out"
    GC_LOCALES="$UP/locales/en" "$BUILD/reward_apply_diff_test" "$2" >"$OUTF" || FAIL=1
    cat "$OUTF"
    N_BAD=$(sed -n 's/^TOTAL mismatches in this trace: //p' "$OUTF")
    TOTAL=$((TOTAL + ${N_BAD:-0}))
}
run_compare "apply (healing, revive, PP)" "$BUILD/gc-apply-$SEED.txt"
run_compare "candy" "$BUILD/gc-candy-$SEED.txt"
run_compare "pokeball" "$BUILD/gc-pokeball-$SEED.txt"
echo "TOTAL mismatches: $TOTAL (seed $SEED, N $N)"
[ "$FAIL" = 0 ] && [ "$TOTAL" = 0 ]
