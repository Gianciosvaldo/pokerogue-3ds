#!/bin/sh
# Task 68: generate N loaded-Illusion cases in upstream and replay them through ai-core, then replay them
# again with the identity of every disguise planted (--mutate-identity), which must fail.
# Usage: tools/illusiondiff.sh <upstream dir> [N] [seed]
# The upstream checkout is the one battle-core/tools/setup-upstream.sh prepares; Node 22 is required.
# ILLTEST_STATE=1 is not offered: use AITEST_STATE=1 for the state of a failing case, as with difftest.sh.
set -e
UP=${1:?upstream dir}; N=${2:-2000}; SEED=${3:-12345}
HERE=$(cd "$(dirname "$0")/.." && pwd)
BUILD=${BUILD:-/tmp/ai-core-build}
case "$(node --version)" in
v22.*) ;;
*) echo "ai-core: Node 22 is required, found $(node --version)" >&2; exit 1 ;;
esac
mkdir -p "$BUILD" "$UP/test/tests/zz-dump"
make -s -C "$HERE" BUILD="$BUILD"
cp "$HERE/tests/illusion.producer.test.ts" "$UP/test/tests/zz-dump/"
(cd "$UP" && AIILL_SEED=$SEED AIILL_N=$N AIILL_OUT=$BUILD/ai-illusion-cases.txt \
    npx vitest run test/tests/zz-dump/illusion.producer.test.ts > "$BUILD/ai-illusion-producer.log" 2>&1) \
  || { tail -30 "$BUILD/ai-illusion-producer.log"; exit 1; }
grep "Illusion AI producer" "$BUILD/ai-illusion-producer.log" | sed 's/^ *//'
echo "== replay"
"$BUILD/illusiontest" "$BUILD/ai-illusion-cases.txt"
echo "== replay with the disguises' identity planted (must fail)"
set +e
"$BUILD/illusiontest" "$BUILD/ai-illusion-cases.txt" --mutate-identity > "$BUILD/ai-illusion-mutated.txt"
status=$?
set -e
grep -E "^covered|^loaded illusion" "$BUILD/ai-illusion-mutated.txt"
if [ $status -eq 0 ]; then echo "PLANTED MUTATION NOT DETECTED"; exit 1; fi
echo "planted mutation detected"
