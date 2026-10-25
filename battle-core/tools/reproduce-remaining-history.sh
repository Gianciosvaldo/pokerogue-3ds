#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$(dirname "$0")/../.." && pwd)
upstream=${1:?Usage: reproduce-remaining-history.sh PINNED_UPSTREAM OUTPUT_DIRECTORY}
out=${2:?Output directory required}
mkdir -p "$out"
out=$(cd "$out" && pwd)
test "$(git -C "$upstream" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d
test "$(git -C "$upstream/locales" rev-parse HEAD)" = 270ed0a2938b2c3e2e34e7de940e84aecadf548b
test -z "$(git -C "$upstream" status --porcelain --untracked-files=no -- src)"
target="$upstream/test/tests/moves/remaining-history.producer.test.ts"
cp "$root/battle-core/tests/remaining-history.producer.test.ts" "$target"
(cd "$upstream" && REMAINING_HISTORY_OUT="$out/native.json" pnpm exec vitest run test/tests/moves/remaining-history.producer.test.ts --maxWorkers=1) > "$out/native.log" 2>&1
python3 "$root/battle-core/tools/build-remaining-history-proof.py" "$out/native.json" "$out/native.inc"
cmp "$out/native.inc" "$root/battle-core/tests/remaining-history.native.inc"
echo '192 native histories / 9600 caller observations reproduced byte-identical'
