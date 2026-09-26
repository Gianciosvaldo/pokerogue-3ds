#!/bin/sh
# Prepare an upstream checkout for the dump + difftest (needs git, node 22, network).
# Usage: tools/setup-upstream.sh <dir> [commit]
set -e
DIR=${1:?dir}; COMMIT=${2:-8555c08}
HERE=$(cd "$(dirname "$0")/.." && pwd)
[ -d "$DIR/.git" ] || git clone --filter=blob:none https://github.com/pagefaultgames/pokerogue.git "$DIR"
cd "$DIR"
git checkout -q "$COMMIT"
git submodule update --init --depth 1 locales
corepack enable >/dev/null 2>&1 || true
pnpm install --ignore-scripts
# The few asset JSONs the headless test scene fetches (no images or audio).
ASSETS=$(git ls-tree HEAD assets | awk '{print $3}')
BASE=https://raw.githubusercontent.com/pagefaultgames/pokerogue-assets/$ASSETS
mkdir -p assets/battle-anims assets/images/pokemon/variant
for f in exp-sprites.json starter-colors.json biome-bgm-loop-points.json battle-anims/tackle.json \
         images/pokemon/variant/_masterlist.json images/pokemon/variant/_exp_masterlist.json; do
  curl -sSf -o "assets/$f" "$BASE/$f"
done
mkdir -p test/tests/zz-dump
cp "$HERE/gen/dump-upstream.test.ts" "$HERE/tests/difftest.producer.test.ts" test/tests/zz-dump/
echo "upstream ready at $DIR ($COMMIT)"
