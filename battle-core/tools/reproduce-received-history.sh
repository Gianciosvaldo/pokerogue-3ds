#!/usr/bin/env bash
set -euo pipefail
repo=$(cd "$(dirname "$0")/../.." && pwd)
upstream=$(cd "${1:?usage: reproduce-received-history.sh PINNED_UPSTREAM OUTPUT_DIR}" && pwd)
mkdir -p "${2:?output directory required}"
output=$(cd "$2" && pwd)
[[ $(git -C "$upstream" rev-parse HEAD) == 8555c08c823b856cbec4eb99ca84ea52a955836d ]]
[[ -z $(git -C "$upstream" diff --name-only -- src test/framework test/helpers) ]]
node -e 'if(process.versions.node.split(".")[0]!=="22")throw Error("Use the recorded Node 22 reference runtime")'
producer_dir=$(mktemp -d "$upstream/test/tests/moves/received-XXXXXX")
trap 'rm -rf -- "$producer_dir"' EXIT
cp "$repo/battle-core/tests/received-history.producer.test.ts" "$producer_dir/history.test.ts"
cp "$repo/battle-core/tests/received-history-command.producer.test.ts" "$producer_dir/command.test.ts"
(cd "$upstream"; RECEIVED_HISTORY_OUT="$output/received-history.json" node node_modules/vitest/vitest.mjs run "$producer_dir/history.test.ts" --maxWorkers=1 > "$output/history.log" 2>&1)
(cd "$upstream"; RECEIVED_COMMAND_OUT="$output/received-command.json" node node_modules/vitest/vitest.mjs run "$producer_dir/command.test.ts" --maxWorkers=1 > "$output/command.log" 2>&1)
python3 "$repo/battle-core/tools/build-received-history-proof.py" "$output/received-history.json" "$output/received-history.native.inc"
python3 "$repo/battle-core/tools/build-received-command-proof.py" "$output/received-command.json" "$output/received-command.native.inc"
cmp "$output/received-history.native.inc" "$repo/battle-core/tests/received-history.native.inc"
cmp "$output/received-command.native.inc" "$repo/battle-core/tests/received-command.native.inc"
python3 - "$repo" "$output" <<'PY'
from pathlib import Path
import gzip,sys
repo,output=map(Path,sys.argv[1:])
for stem in ['received-history','received-command']:
 assert (output/f'{stem}.json').read_bytes()==gzip.decompress((repo/f'battle-core/tests/fixtures/{stem}.native.json.gz').read_bytes())
print('Fresh native captures and fixtures are byte-identical to the retained proof')
PY
