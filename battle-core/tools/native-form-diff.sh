#!/bin/sh
set -eu
UP=${1:?pinned upstream}; N=${2:-2000}; SEED=${3:-777}; MODE=${4:-turn}
HERE=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
UP=$(CDPATH= cd -- "$UP" && pwd)
[ "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d ] || exit 2
[ "$(git -C "$UP/locales" rev-parse HEAD)" = 270ed0a2938b2c3e2e34e7de940e84aecadf548b ] || exit 2
git -C "$UP" diff --quiet || exit 2
case "$(node --version)" in v22.*) ;; *) exit 2 ;; esac
case "$MODE" in api|phase|natural|aura|turn) ;; *) exit 2 ;; esac
make -s -C "$HERE" -j2 build/native-form-test build/native-form-turn-test
mkdir -p "$UP/test/tests/zz-dump"
TRACE="$HERE/build/native-form-$MODE-$SEED.txt"
if [ "$MODE" = turn ]; then
 python3 "$HERE/tools/build-native-form-turn-proof.py" producer "$UP/test/tests/zz-dump/native-form-turn.producer.test.ts"
 (cd "$UP" && NATIVEFORMTURN_N="$N" NATIVEFORMTURN_SEED="$SEED" NATIVEFORMTURN_OUT="$TRACE" node node_modules/vitest/vitest.mjs run test/tests/zz-dump/native-form-turn.producer.test.ts --maxWorkers=1) > "$HERE/build/native-form-turn-source-$SEED.log" 2>&1
 TURNTEST_LOCALES="$UP/locales/en" "$HERE/build/native-form-turn-test" "$TRACE"
else
 cp "$HERE/tests/native-form.producer.test.ts" "$UP/test/tests/zz-dump/native-form.producer.test.ts"
 PHASE=0; NATURAL=0; HP=0; AURA=0
 [ "$MODE" = api ] || PHASE=1
 case "$MODE" in natural|aura) NATURAL=1; HP=1 ;; esac
 [ "$MODE" != aura ] || AURA=1
 (cd "$UP" && NATIVEFORM_N="$N" NATIVEFORM_SEED="$SEED" NATIVEFORM_PHASE="$PHASE" NATIVEFORM_NATURAL="$NATURAL" NATIVEFORM_HP_MATRIX="$HP" NATIVEFORM_AURA="$AURA" NATIVEFORM_OUT="$TRACE" node node_modules/vitest/vitest.mjs run test/tests/zz-dump/native-form.producer.test.ts --maxWorkers=1) > "$HERE/build/native-form-$MODE-source-$SEED.log" 2>&1
 if [ "$PHASE" = 1 ]; then
  TURNTEST_LOCALES="$UP/locales/en" "$HERE/build/native-form-test" "$TRACE" --phase
 else
  TURNTEST_LOCALES="$UP/locales/en" "$HERE/build/native-form-test" "$TRACE"
 fi
fi
