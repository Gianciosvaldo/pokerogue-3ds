#!/usr/bin/env bash
set -euo pipefail
repo=$(cd "$(dirname "$0")/../.." && pwd)
upstream=$(cd "${1:?usage: reproduce-fusion-evolution.sh PINNED_UPSTREAM OUTPUT_DIR}" && pwd)
mkdir -p "${2:?output directory required}"
output=$(cd "$2" && pwd)
[[ $(git -C "$upstream" rev-parse HEAD) == 8555c08c823b856cbec4eb99ca84ea52a955836d ]]
[[ $(git -C "$upstream/locales" rev-parse HEAD) == 270ed0a2938b2c3e2e34e7de940e84aecadf548b ]]
[[ -z $(git -C "$upstream" diff --name-only -- src test/framework test/helpers) ]]
[[ -z $(git -C "$upstream/locales" diff --name-only) ]]
node -e 'if(process.versions.node.split(".")[0]!=="22")throw Error("Use Node22")'
producer_dir=$(mktemp -d "$upstream/test/tests/moves/fusion-evolution-XXXXXX")
trap 'rm -rf -- "$producer_dir"' EXIT
cp "$repo/game-core/tests/fusion-evolution.producer.test.ts" "$producer_dir/producer.test.ts"
(cd "$upstream"; FUSION_EVOLUTION_OUT="$output/native.json" node node_modules/vitest/vitest.mjs run "$producer_dir/producer.test.ts" --maxWorkers=1 > "$output/native.log" 2>&1)
python3 "$repo/game-core/tools/build-fusion-evolution-proof.py" "$output/native.json" "$output/native.inc"
cmp "$output/native.inc" "$repo/game-core/tests/fixtures/fusion-evolution.native.inc"
python3 - "$repo" "$output" <<'PY'
from pathlib import Path
import gzip,sys
repo,output=map(Path,sys.argv[1:])
assert (output/'native.json').read_bytes()==gzip.decompress((repo/'game-core/tests/fixtures/fusion-evolution.native.json.gz').read_bytes())
print('Fresh native fusion evolution capture and C include are byte-identical')
PY
