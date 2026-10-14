#!/bin/sh
set -eu
UP=${1:?pinned upstream}; KIND=${2:-fusion}; MODE=${3:-api}; N=${4:-8192}; SEED=${5:-777}
HERE=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
UP=$(CDPATH= cd -- "$UP" && pwd)
[ "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d ] || exit 2
[ "$(git -C "$UP/locales" rev-parse HEAD)" = 270ed0a2938b2c3e2e34e7de940e84aecadf548b ] || exit 2
git -C "$UP" diff --quiet || exit 2
case "$(node --version)" in v22.*) ;; *) exit 2 ;; esac
case "$KIND" in fusion|transform) ;; *) exit 2 ;; esac
case "$MODE" in api) NAME="$KIND-callback";; turn) NAME="$KIND-callback-turn";; *) exit 2;; esac
make -s -C "$HERE" "build/$NAME-test"
mkdir -p "$UP/test/tests/zz-dump"
if [ "$MODE" = api ]; then cp "$HERE/tests/$NAME.producer.test.ts" "$UP/test/tests/zz-dump/$NAME.producer.test.ts"; else python3 "$HERE/tools/build-$NAME-proof.py" producer "$UP/test/tests/zz-dump/$NAME.producer.test.ts"; fi
TRACE="$HERE/build/$NAME-$SEED.txt"
(cd "$UP" && NATIVEFORM_N="$N" NATIVEFORM_SEED="$SEED" NATIVEFORM_NATURAL=1 NATIVEFORM_PHASE=1 NATIVEFORM_OUT="$TRACE" FUSIONCALLBACKTURN_N="$N" FUSIONCALLBACKTURN_SEED="$SEED" FUSIONCALLBACKTURN_OUT="$TRACE" TRANSFORMCALLBACKTURN_N="$N" TRANSFORMCALLBACKTURN_SEED="$SEED" TRANSFORMCALLBACKTURN_OUT="$TRACE" node node_modules/vitest/vitest.mjs run "test/tests/zz-dump/$NAME.producer.test.ts" --maxWorkers=1) > "$HERE/build/$NAME-source-$SEED.log" 2>&1
if [ "$MODE" = api ]; then TURNTEST_LOCALES="$UP/locales/en" "$HERE/build/$NAME-test" "$TRACE" --phase; else TURNTEST_LOCALES="$UP/locales/en" "$HERE/build/$NAME-test" "$TRACE"; fi
