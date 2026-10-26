#!/usr/bin/env bash
set -euo pipefail
repo=$(cd "$(dirname "$0")/../.." && pwd)
upstream=$(cd "${1:?usage: reproduce-turn-order.sh PINNED_UPSTREAM OUTPUT_DIR}" && pwd)
mkdir -p "${2:?output directory required}"
output=$(cd "$2" && pwd)
[[ $(git -C "$upstream" rev-parse HEAD) == 8555c08c823b856cbec4eb99ca84ea52a955836d ]]
[[ -z $(git -C "$upstream" diff --name-only -- src test/framework test/helpers) ]]
node -e 'if(process.versions.node.split(".")[0]!=="22")throw Error("Use Node 22")'
producer_dir=$(mktemp -d "$upstream/test/tests/moves/turn-order-XXXXXX")
trap 'rm -rf -- "$producer_dir"' EXIT
cp "$repo/battle-core/tests/turn-order.producer.test.ts" "$producer_dir/history.test.ts"
cp "$repo/battle-core/tests/turn-order-command.producer.test.ts" "$producer_dir/command.test.ts"
(cd "$upstream"; TURN_ORDER_OUT="$output/turn-order.json" node node_modules/vitest/vitest.mjs run "$producer_dir/history.test.ts" --maxWorkers=1 > "$output/history.log" 2>&1)
(cd "$upstream"; TURN_ORDER_COMMAND_OUT="$output/turn-order-command.json" node node_modules/vitest/vitest.mjs run "$producer_dir/command.test.ts" --maxWorkers=1 > "$output/command.log" 2>&1)
for stem in turn-order turn-order-command; do
 python3 "$repo/battle-core/tools/build-$stem-proof.py" "$output/$stem.json" "$output/$stem.native.inc"
 cmp "$output/$stem.native.inc" "$repo/battle-core/tests/$stem.native.inc"
done
python3 - "$repo" "$output" <<'PY'
from pathlib import Path
import gzip,sys
repo,output=map(Path,sys.argv[1:])
for stem in ['turn-order','turn-order-command']:
 assert (output/f'{stem}.json').read_bytes()==gzip.decompress((repo/f'battle-core/tests/fixtures/{stem}.native.json.gz').read_bytes())
print('Fresh native captures and fixtures are byte-identical to the retained proof')
PY
