#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
UP=$(CDPATH= cd -- "${1:?upstream checkout required}" && pwd)
N=${2:-280}
SEED=${3:-777}
NODE=${NODE:-node}
test "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d
test "$("$NODE" -p 'process.versions.node.split(".")[0]')" = 22
TRACE=${LC_OUT:-$(mktemp /tmp/gc-lifecycle-XXXXXX)}
mkdir -p "$UP/test/tests/zz-dump"
cp "$ROOT/tests/reward-lifecycle.producer.test.ts" "$UP/test/tests/zz-dump/"
make -C "$ROOT" build/reward_lifecycle_test
cd "$UP"
LC_N="$N" LC_SEED="$SEED" LC_OUT="$TRACE" "$NODE" node_modules/vitest/vitest.mjs run test/tests/zz-dump/reward-lifecycle.producer.test.ts
"$ROOT/build/reward_lifecycle_test" "$TRACE"
printf 'Lifecycle trace: %s\n' "$TRACE"
