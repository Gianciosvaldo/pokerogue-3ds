#!/usr/bin/env bash
set -euo pipefail
repo=$(cd "$(dirname "$0")/../.." && pwd)
upstream=$(cd "${1:?usage: reproduce-closure-identity.sh PINNED_UPSTREAM OUTPUT_DIR}" && pwd)
mkdir -p "${2:?output directory required}"
output=$(cd "$2" && pwd)
[[ $(git -C "$upstream" rev-parse HEAD) == 8555c08c823b856cbec4eb99ca84ea52a955836d ]]
[[ $(git -C "$upstream/locales" rev-parse HEAD) == 270ed0a2938b2c3e2e34e7de940e84aecadf548b ]]
[[ $(git -C "$upstream" ls-tree HEAD assets | awk '{print $3}') == 87426a79611f9d212c4dc8af58e2834a05b93725 ]]
[[ -z $(git -C "$upstream" diff --name-only -- src test/framework test/helpers) ]]
[[ -z $(git -C "$upstream/locales" diff --name-only) ]]
node -e 'if(process.versions.node.split(".")[0]!=="22")throw Error("Use Node 22")'
producer_dir=$(mktemp -d "$upstream/test/tests/moves/closure-identity-XXXXXX")
trap 'rm -rf -- "$producer_dir"' EXIT
cp "$repo/battle-core/gen/dump-upstream.test.ts" "$producer_dir/producer.test.ts"
(cd "$upstream"; DUMP_OUT="$output/native" node node_modules/vitest/vitest.mjs run "$producer_dir/producer.test.ts" --maxWorkers=1 > "$output/native.log" 2>&1)
python3 - "$repo" "$output" <<'PY'
from pathlib import Path
import gzip,hashlib,json,sys
repo,output=map(Path,sys.argv[1:])
data=dict(source_pin='8555c08c823b856cbec4eb99ca84ea52a955836d',
          locale_pin='270ed0a2938b2c3e2e34e7de940e84aecadf548b',
          asset_pin='87426a79611f9d212c4dc8af58e2834a05b93725',
          producer_sha256=hashlib.sha256((repo/'battle-core/gen/dump-upstream.test.ts').read_bytes()).hexdigest(),
          files={p.name:json.loads(p.read_text()) for p in sorted((output/'native').glob('*.json'))})
raw=(json.dumps(data,separators=(',',':'))+'\n').encode()
(output/'closure-identity.native.json.gz').write_bytes(gzip.compress(raw,mtime=0))
assert raw==gzip.decompress((repo/'battle-core/tests/fixtures/closure-identity.native.json.gz').read_bytes())
print('Fresh native registry dump is byte-identical after decompression')
PY
