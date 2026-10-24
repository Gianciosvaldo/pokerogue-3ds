#!/bin/sh
set -eu
UP=$(CDPATH= cd -- "${1:?pinned upstream checkout}" && pwd)
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BUILD=${BUILD:-$ROOT/build}
test "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d
test "$(git -C "$UP/locales" rev-parse HEAD)" = 270ed0a2938b2c3e2e34e7de940e84aecadf548b
test "$(git -C "$UP" ls-tree HEAD assets | cut -f1 | cut -d' ' -f3)" = 87426a79611f9d212c4dc8af58e2834a05b93725
git -C "$UP" diff --exit-code HEAD -- src >/dev/null
test "$(node -p 'process.versions.node.split(".")[0]')" = 22
mkdir -p "$BUILD" "$UP/test/tests/zz-dump"
cp "$ROOT/tests/held-form-items.producer.test.ts" "$UP/test/tests/zz-dump/held-form-items.producer.test.ts"
(cd "$UP" && HELD_FORM_OUT="$BUILD/held-form-items.native.json" node node_modules/vitest/vitest.mjs \
    run test/tests/zz-dump/held-form-items.producer.test.ts --maxWorkers=1)
python3 "$ROOT/tools/build-held-form-item-proof.py" "$BUILD/held-form-items.native.json" \
    "$BUILD/held-form-items.native.txt" "$BUILD/held-form-constructors.native.txt"
# Regenerate into a temporary directory: a mismatch must fail, not rewrite production tables.
TABLES=$(mktemp -d)
trap 'rm -rf "$TABLES"' EXIT HUP INT TERM
python3 "$ROOT/tools/build-held-form-item-tables.py" "$BUILD/held-form-items.native.json" "$TABLES"
cmp "$TABLES/pr_item_forms.c" "$ROOT/gen/out/pr_item_forms.c"
cmp "$TABLES/pr_item_forms.h" "$ROOT/gen/out/pr_item_forms.h"
make -C "$ROOT" build/held-form-items-test
"$ROOT/build/held-form-items-test" "$BUILD/held-form-items.native.txt"
