#!/bin/sh
set -eu
UP=${1:?pinned upstream checkout}; SEED=${2:-777}
HERE=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
UP=$(CDPATH= cd -- "$UP" && pwd)
test "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d
test "$(git -C "$UP/locales" rev-parse HEAD)" = 270ed0a2938b2c3e2e34e7de940e84aecadf548b
git -C "$UP" diff --quiet
test "$(node -p 'process.versions.node.split(".")[0]')" = 22
make -s -C "$HERE" build/keldeo-forms-test build/mimicry-test build/wide-combat-test
# Serial recursive builds avoid simultaneous writes to shared nested archives.
make -s -C "$HERE/../game-core" -j1 keldeo-host-test build/keldeo_learn_test
mkdir -p "$UP/test/tests/zz-dump"
cp "$HERE/tests/keldeo-forms.producer.test.ts" "$UP/test/tests/zz-dump/"
python3 "$HERE/tools/build-mimicry-proof.py" "$UP/test/tests/zz-dump/mimicry.producer.test.ts"
python3 "$HERE/tools/build-keldeo-learn-proof.py" "$UP/test/tests/zz-dump/keldeo-learn.producer.test.ts"
python3 "$HERE/tools/build-mimicry-turn-proof.py" "$UP/test/tests/zz-dump/mimicry-turns.producer.test.ts"
RAW="$HERE/build/keldeo-$SEED.txt"; MIMICRY="$HERE/build/mimicry-$SEED.txt"
LEARN="$HERE/build/keldeo-learn-$SEED.txt"; TURNS="$HERE/build/mimicry-turns-$SEED.txt"
(cd "$UP" && NODE_OPTIONS=--max-old-space-size=6144 KELDEO_SEED="$SEED" KELDEO_OUT="$RAW" node node_modules/vitest/vitest.mjs run test/tests/zz-dump/keldeo-forms.producer.test.ts --maxWorkers=1)
(cd "$UP" && NODE_OPTIONS=--max-old-space-size=6144 MIMICRY_SEED="$SEED" MIMICRY_OUT="$MIMICRY" node node_modules/vitest/vitest.mjs run test/tests/zz-dump/mimicry.producer.test.ts --maxWorkers=1)
(cd "$UP" && NODE_OPTIONS=--max-old-space-size=6144 KELDEO_LEARN_SEED="$SEED" KELDEO_LEARN_OUT="$LEARN" node node_modules/vitest/vitest.mjs run test/tests/zz-dump/keldeo-learn.producer.test.ts --maxWorkers=1)
(cd "$UP" && NODE_OPTIONS=--max-old-space-size=6144 DOUBLES_N=8192 DOUBLES_SEED="$SEED" DOUBLES_OUT="$TURNS" node node_modules/vitest/vitest.mjs run test/tests/zz-dump/mimicry-turns.producer.test.ts --maxWorkers=1)
export TURNTEST_LOCALES="$UP/locales/en"
python3 "$HERE/tools/check-complete-turns.py" "$HERE/build/wide-combat-test" "$TURNS"
python3 "$HERE/tools/check-keldeo-proof.py" "$RAW" "$MIMICRY" "$LEARN"
