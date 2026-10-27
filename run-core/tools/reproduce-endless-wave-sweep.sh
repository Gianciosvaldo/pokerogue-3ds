#!/usr/bin/env bash
set -euo pipefail
repo=$(cd "$(dirname "$0")/../.." && pwd)
upstream=$(cd "${1:?usage: reproduce-endless-wave-sweep.sh PINNED_UPSTREAM OUTPUT_DIR}" && pwd)
mkdir -p "${2:?output directory required}"
output=$(cd "$2" && pwd)
[[ $(git -C "$upstream" rev-parse HEAD) == 8555c08c823b856cbec4eb99ca84ea52a955836d ]]
[[ $(git -C "$upstream/locales" rev-parse HEAD) == 270ed0a2938b2c3e2e34e7de940e84aecadf548b ]]
[[ -z $(git -C "$upstream" diff --name-only -- src test/framework test/helpers) ]]
[[ -z $(git -C "$upstream/locales" diff --name-only) ]]
node -e 'if(process.versions.node.split(".")[0]!=="22")throw Error("Use Node 22")'
producer_dir=$(mktemp -d "$upstream/test/tests/moves/endless-wave-sweep-XXXXXX")
trap 'rm -rf -- "$producer_dir"' EXIT
cp "$repo/run-core/tests/endless-wave-sweep.producer.test.ts" "$producer_dir/producer.test.ts"
(cd "$upstream"; DIFF_OUT="$output/native.txt" node node_modules/vitest/vitest.mjs run "$producer_dir/producer.test.ts" --maxWorkers=1 > "$output/native.log" 2>&1)
python3 "$repo/run-core/tools/check-endless-wave-sweep.py" --pack "$upstream" --raw "$output/native.txt" --output "$output/endless-wave-sweep.native.json.gz"
python3 - "$repo" "$output" <<'PY'
from pathlib import Path
import gzip,sys
repo,output=map(Path,sys.argv[1:])
assert gzip.decompress((output/'endless-wave-sweep.native.json.gz').read_bytes()) == gzip.decompress((repo/'run-core/tests/fixtures/endless-wave-sweep.native.json.gz').read_bytes())
print('Fresh 11,700-row native capsule is byte-identical after decompression')
PY
