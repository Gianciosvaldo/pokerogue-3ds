#!/bin/sh
set -eu
UP=${1:?pinned upstream}; MODE=${2:-turn}; N=${3:-8192}; SEED=${4:-777}
HERE=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
UP=$(CDPATH= cd -- "$UP" && pwd)
[ "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d ] || exit 2
[ "$(git -C "$UP/locales" rev-parse HEAD)" = 270ed0a2938b2c3e2e34e7de940e84aecadf548b ] || exit 2
git -C "$UP" diff --quiet || exit 2
case "$(node --version)" in v22.*) ;; *) exit 2 ;; esac
case "$MODE" in api|turn) ;; *) exit 2 ;; esac
make -s -C "$HERE" -j2 build/form-interaction-test build/form-interaction-turn-test
mkdir -p "$UP/test/tests/zz-dump"
TRACE="$HERE/build/form-interaction-$MODE-$SEED.txt"
if [ "$MODE" = api ]; then
 NAME=form-interaction
 cp "$HERE/tests/form-interaction.producer.test.ts" "$UP/test/tests/zz-dump/$NAME.producer.test.ts"
else
 NAME=form-interaction-turn
 python3 "$HERE/tools/build-form-interaction-turn-proof.py" producer "$UP/test/tests/zz-dump/$NAME.producer.test.ts"
fi
(cd "$UP" && FORMINTER_N="$N" FORMINTER_SEED="$SEED" FORMINTER_OUT="$TRACE" FORMINTERTURN_N="$N" FORMINTERTURN_SEED="$SEED" FORMINTERTURN_OUT="$TRACE" node node_modules/vitest/vitest.mjs run "test/tests/zz-dump/$NAME.producer.test.ts" --maxWorkers=1) > "$HERE/build/form-interaction-$MODE-source-$SEED.log" 2>&1
TURNTEST_LOCALES="$UP/locales/en" "$HERE/build/$NAME-test" "$TRACE"
