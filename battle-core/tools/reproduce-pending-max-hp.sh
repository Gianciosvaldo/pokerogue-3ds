#!/usr/bin/env bash
set -euo pipefail
repo=$(cd "$(dirname "$0")/../.." && pwd)
upstream=$(cd "${1:?usage: reproduce-pending-max-hp.sh PINNED_UPSTREAM OUTPUT_DIR}" && pwd)
mkdir -p "${2:?output directory required}"
output=$(cd "$2" && pwd)
[[ $(git -C "$upstream" rev-parse HEAD) == 8555c08c823b856cbec4eb99ca84ea52a955836d ]]
[[ $(git -C "$upstream/locales" rev-parse HEAD) == 270ed0a2938b2c3e2e34e7de940e84aecadf548b ]]
[[ -z $(git -C "$upstream" diff --name-only -- src test/framework test/helpers) ]]
[[ -z $(git -C "$upstream/locales" diff --name-only) ]]
node -e 'if(process.versions.node.split(".")[0]!=="22")throw Error("Use Node22")'
producer_dir=$(mktemp -d "$upstream/test/tests/moves/pending-max-hp-XXXXXX")
trap 'rm -rf -- "$producer_dir"' EXIT
cp "$repo/battle-core/tests/pending-heal.producer.test.ts" "$producer_dir/producer.test.ts"
(cd "$upstream"; PENDING_HP_CONTEXT=1 PENDING_DOUBLE=1 PENDING_N=8192 PENDING_SEED=777 PENDING_OUT="$output/native.txt" node node_modules/vitest/vitest.mjs run "$producer_dir/producer.test.ts" --maxWorkers=1 > "$output/native.log" 2>&1)
python3 - "$repo" "$output" "$upstream" <<'PYCODE'
from pathlib import Path
import gzip,json,sys
repo,output,upstream=map(Path,sys.argv[1:])
raw=(output/'native.txt').read_bytes()
assert raw==gzip.decompress((repo/'battle-core/tests/fixtures/pending-max-hp.native.txt.gz').read_bytes())
(output/'native.txt.gz').write_bytes(gzip.compress(raw,mtime=0));(output/'native.txt').unlink()
locales=json.loads(gzip.decompress((repo/'battle-core/tests/fixtures/pending-max-hp.locales.json.gz').read_bytes()))
for name,content in locales.items():assert content==(upstream/'locales/en'/f'{name}.json').read_text()
print('Fresh native pending-heal HP records and pinned locales are byte-identical')
PYCODE
