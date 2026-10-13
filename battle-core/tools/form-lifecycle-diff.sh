#!/bin/sh
set -eu
UP=${1:?pinned upstream}; N=${2:-4000}; SEED=${3:-777}; KIND=${4:-both}
HERE=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
UP=$(CDPATH= cd -- "$UP" && pwd)
[ "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d ] || exit 2
[ "$(git -C "$UP/locales" rev-parse HEAD)" = 270ed0a2938b2c3e2e34e7de940e84aecadf548b ] || exit 2
git -C "$UP" diff --quiet || exit 2
case "$(node --version)" in v22.*) ;; *) exit 2 ;; esac
case "$KIND" in both|api|phase) ;; *) exit 2 ;; esac
make -s -C "$HERE" -j2 build/form-lifecycle-test
mkdir -p "$UP/test/tests/zz-dump"
if [ "$KIND" != phase ]; then
 cp "$HERE/tests/form-lifecycle.producer.test.ts" "$UP/test/tests/zz-dump/"
 TRACE="$HERE/build/form-lifecycle-$SEED.txt"
 (cd "$UP" && FORMLIFE_N="$N" FORMLIFE_SEED="$SEED" FORMLIFE_OUT="$TRACE" node node_modules/vitest/vitest.mjs run test/tests/zz-dump/form-lifecycle.producer.test.ts --maxWorkers=1) > "$HERE/build/form-lifecycle-source-$SEED.log" 2>&1
 "$HERE/build/form-lifecycle-test" "$TRACE"
fi
if [ "$KIND" != api ]; then
 cp "$HERE/tests/quiet-form.producer.test.ts" "$UP/test/tests/zz-dump/"
 TRACE="$HERE/build/quiet-form-$SEED.txt"
 (cd "$UP" && QUIETFORM_N="$N" QUIETFORM_SEED="$SEED" QUIETFORM_OUT="$TRACE" node node_modules/vitest/vitest.mjs run test/tests/zz-dump/quiet-form.producer.test.ts --maxWorkers=1) > "$HERE/build/quiet-form-source-$SEED.log" 2>&1
 TURNTEST_LOCALES="$UP/locales/en" "$HERE/build/form-lifecycle-test" "$TRACE" --phase
fi
