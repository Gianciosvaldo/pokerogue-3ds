#!/bin/sh
set -eu
UP=${1:?pinned upstream checkout}; SEED=${2:-777}
HERE=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
UP=$(CDPATH= cd -- "$UP" && pwd)
test "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d
test "$(git -C "$UP/locales" rev-parse HEAD)" = 270ed0a2938b2c3e2e34e7de940e84aecadf548b
git -C "$UP" diff --quiet
git -C "$UP/locales" diff --quiet
test "$(node -p 'process.versions.node.split(".")[0]')" = 22
make -s -C "$HERE" build/double-chance-test build/double-generation-test
mkdir -p "$UP/test/tests/zz-dump" "$HERE/../work"
export TURNTEST_LOCALES="$UP/locales/en"
for KIND in raw generation trainer fixed; do
 case "$KIND" in
 raw) BUILDER=build-double-chance-proof.py; OUT_VAR=DOUBLE_CHANCE_OUT; RUNNER=double-chance-test ;;
 generation) BUILDER=build-double-chance-generation-proof.py; OUT_VAR=DOUBLE_GENERATION_OUT; RUNNER=double-generation-test ;;
 trainer) BUILDER=build-double-chance-trainer-proof.py; OUT_VAR=DOUBLE_TRAINER_OUT; RUNNER=double-generation-test ;;
 fixed) BUILDER=build-double-chance-fixed-proof.py; OUT_VAR=DOUBLE_TRAINER_OUT; RUNNER=double-generation-test ;;
 esac
 python3 "$HERE/tools/$BUILDER" "$UP/test/tests/zz-dump/double-$KIND.producer.test.ts"
 OUT="$HERE/build/double-$KIND-$SEED.txt"
 (cd "$UP" && env NODE_OPTIONS=--max-old-space-size=4096 DOUBLE_CHANCE_SEED="$SEED" "$OUT_VAR=$OUT" node node_modules/vitest/vitest.mjs run "test/tests/zz-dump/double-$KIND.producer.test.ts" --maxWorkers=1)
 "$HERE/build/$RUNNER" "$OUT"
done
python3 "$HERE/tools/check-double-chance-proof.py" "$HERE/build/double-raw-$SEED.txt" "$HERE/build/double-generation-$SEED.txt" "$HERE/build/double-trainer-$SEED.txt"
python3 "$HERE/tools/check-double-fixed-proof.py" "$HERE/build/double-fixed-$SEED.txt"
