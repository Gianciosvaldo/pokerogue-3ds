#!/bin/sh
# Card 94: replays upstream's catch (ball rules, AttemptCapturePhase, addToParty, the party screen's release) through
# game-core. Usage: tools/catchdiff.sh <upstream checkout> [flow cases] [ball-rule states] [seed]
# Needs Node 22 and the upstream checkout at the pinned commit; the trace is written to $CATCH_OUT.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
UP=$(CDPATH= cd -- "${1:?upstream checkout required}" && pwd)
N=${2:-64}
RULES=${3:-3000}
SEED=${4:-777}
NODE=${NODE:-node}
test "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d
test "$("$NODE" -p 'process.versions.node.split(".")[0]')" = 22
TRACE=${CATCH_OUT:-$(mktemp /tmp/gc-catch-XXXXXX)}
mkdir -p "$UP/test/tests/zz-dump"
cp "$ROOT/tests/catch.producer.test.ts" "$UP/test/tests/zz-dump/gc-catch.producer.test.ts"
make -C "$ROOT" build/catch_diff_test
cd "$UP"
CATCH_N="$N" CATCH_RULES="$RULES" CATCH_SEED="$SEED" CATCH_OUT="$TRACE" "$NODE" node_modules/vitest/vitest.mjs run test/tests/zz-dump/gc-catch.producer.test.ts
"$ROOT/build/catch_diff_test" "$TRACE"
printf 'Catch trace: %s\n' "$TRACE"
