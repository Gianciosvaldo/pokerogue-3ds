#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$(dirname "$0")/../.." && pwd)
upstream=${1:?Usage: reproduce-two-turn-end.sh PINNED_UPSTREAM OUTPUT_DIRECTORY}
out=${2:?Output directory required}
mkdir -p "$out"
out=$(cd "$out" && pwd)
test "$(git -C "$upstream" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d
test "$(git -C "$upstream/locales" rev-parse HEAD)" = 270ed0a2938b2c3e2e34e7de940e84aecadf548b
test -z "$(git -C "$upstream" status --porcelain --untracked-files=no -- src)"
target="$upstream/test/tests/moves/two-turn-end.producer.test.ts"
cp "$root/game-core/tests/two-turn-end.producer.test.ts" "$target"
(cd "$upstream" && CHARGE_OUT="$out/native.json" pnpm exec vitest run test/tests/moves/two-turn-end.producer.test.ts --maxWorkers=1) > "$out/native.log" 2>&1
python3 "$root/game-core/tools/build-two-turn-end-proof.py" "$out/native.json" "$out/native.inc"
cmp "$out/native.inc" "$root/game-core/tests/fixtures/two-turn-end.native.inc"
echo '28 native cases reproduced byte-identical; 17 charging IDs retained'
