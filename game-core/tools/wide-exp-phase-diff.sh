#!/bin/sh
set -eu
UP=${1:?pinned upstream checkout}; SEED=${2:-777}
HERE=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
UP=$(CDPATH= cd -- "$UP" && pwd)
test "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d
test "$(git -C "$UP/locales" rev-parse HEAD)" = 270ed0a2938b2c3e2e34e7de940e84aecadf548b
git -C "$UP" diff --quiet
test "$(node -p 'process.versions.node.split(".")[0]')" = 22
make -s -C "$HERE" build/wide_exp_phase_diff_test wide-exp-phase-host-test
mkdir -p "$UP/test/tests/zz-dump"
cp "$HERE/tests/wide-exp-phase.producer.test.ts" "$UP/test/tests/zz-dump/"
TRACE="$HERE/build/wide-exp-phase-$SEED.txt"
(cd "$UP" && WIDEEXP_SEED="$SEED" WIDEEXP_OUT="$TRACE" node node_modules/vitest/vitest.mjs run test/tests/zz-dump/wide-exp-phase.producer.test.ts --maxWorkers=1)
TURNTEST_LOCALES="$UP/locales/en" "$HERE/build/wide_exp_phase_diff_test" "$TRACE"
TURNTEST_LOCALES="$UP/locales/en" python3 "$HERE/tools/check-wide-exp-phase-proof.py" "$TRACE"
