#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
UP=$(CDPATH= cd -- "$1" && pwd)
[ "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d ]
[ "$(git -C "$UP/locales" rev-parse HEAD)" = 270ed0a2938b2c3e2e34e7de940e84aecadf548b ]
git -C "$UP" diff --exit-code HEAD -- src >/dev/null
git -C "$UP/locales" diff --exit-code HEAD >/dev/null
node -e 'if(process.versions.node.split(".")[0]!=="22")process.exit(1)'
mkdir -p "$ROOT/work" "$UP/test/tests/zz-dump"
cp "$ROOT/game-core/tests/endless_terminal.producer.test.ts" "$UP/test/tests/zz-dump/"
make -C "$ROOT/game-core" -j1 build/endless_terminal_test build/endless_terminal_host_test
for SEED in 777 2034; do
 for PREFIX in '' 1; do
  if [ -n "$PREFIX" ]; then STEM=endless-terminal-prefix; else STEM=endless-terminal; fi
  (cd "$UP" && ENDLESS_TERMINAL_PREFIX="$PREFIX" ENDLESS_TERMINAL_SEED="$SEED" ENDLESS_TERMINAL_OUT="$ROOT/work/$STEM-$SEED.txt" node node_modules/vitest/vitest.mjs run --maxWorkers=1 test/tests/zz-dump/endless_terminal.producer.test.ts)
  "$ROOT/game-core/build/endless_terminal_test" "$ROOT/work/$STEM-$SEED.txt"
 done
done
"$ROOT/game-core/build/endless_terminal_host_test"
python3 "$ROOT/game-core/tools/check-endless-terminal-proof.py" "$ROOT/work/endless-terminal-prefix-777.txt"
