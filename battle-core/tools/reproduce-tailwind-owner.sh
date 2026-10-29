#!/usr/bin/env bash
set -euo pipefail
repo=$(cd "$(dirname "$0")/../.." && pwd)
upstream=$(cd "${1:?usage: reproduce-tailwind-owner.sh PINNED_UPSTREAM OUTPUT_DIR}" && pwd)
mkdir -p "${2:?output directory required}"
output=$(cd "$2" && pwd)
[[ $(git -C "$upstream" rev-parse HEAD) == 8555c08c823b856cbec4eb99ca84ea52a955836d ]]
[[ $(git -C "$upstream/locales" rev-parse HEAD) == 270ed0a2938b2c3e2e34e7de940e84aecadf548b ]]
[[ -z $(git -C "$upstream" diff --name-only -- src test/framework test/helpers) ]]
[[ -z $(git -C "$upstream/locales" diff --name-only) ]]
node -e 'if(process.versions.node.split(".")[0]!=="22")throw Error("Use Node22")'
producer_dir=$(mktemp -d "$upstream/test/tests/moves/tailwind-owner-XXXXXX")
trap 'rm -rf -- "$producer_dir"' EXIT
cp "$repo/battle-core/tests/tailwind-owner.producer.test.ts" "$producer_dir/producer.test.ts"
(cd "$upstream"; TAILWIND_OWNER_OUT="$output/native.txt.gz" TAILWIND_OWNER_LOCALES_OUT="$output/locales.json.gz" node node_modules/vitest/vitest.mjs run "$producer_dir/producer.test.ts" --maxWorkers=1 > "$output/native.log" 2>&1)
python3 - "$repo" "$output" <<'PY'
from pathlib import Path
import gzip,sys
repo,output=map(Path,sys.argv[1:])
for new,old in [('native.txt.gz','tailwind-owner.native.txt.gz'),('locales.json.gz','tailwind-owner.locales.json.gz')]:
 assert gzip.decompress((output/new).read_bytes())==gzip.decompress((repo/'battle-core/tests/fixtures'/old).read_bytes())
print('Fresh native Tailwind owner records and pinned locales are byte-identical')
PY
