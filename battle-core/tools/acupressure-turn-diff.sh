#!/bin/sh
set -eu
UP=${1:?pinned upstream checkout}
HERE=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
UP=$(CDPATH= cd -- "$UP" && pwd)
[ "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d ] || exit 2
[ "$(git -C "$UP/locales" rev-parse HEAD)" = 270ed0a2938b2c3e2e34e7de940e84aecadf548b ] || exit 2
git -C "$UP" diff --quiet || exit 2
case "$(node --version)" in v22.*) ;; *) exit 2 ;; esac
make -s -C "$HERE" build/doublestest
mkdir -p "$UP/test/tests/zz-dump"
python3 "$HERE/tools/build-acupressure-turn-proof.py" "$UP/test/tests/zz-dump/acupressure-turn.producer.test.ts"
TRACE="$HERE/build/acupressure-turns.txt"
(cd "$UP" && DOUBLES_RICH=0 DOUBLES_N=8194 DOUBLES_SEED=777 DOUBLES_OUT="$TRACE" node node_modules/vitest/vitest.mjs run test/tests/zz-dump/acupressure-turn.producer.test.ts --maxWorkers=1) > "$HERE/build/acupressure-turns-source.log" 2>&1
TURNTEST_LOCALES="$UP/locales/en" "$HERE/build/doublestest" "$TRACE" > "$HERE/build/acupressure-turns-replay.log"
rg -q '^covered 8194: pass 8194, fail 0; not covered 0$' "$HERE/build/acupressure-turns-replay.log"
cat "$HERE/build/acupressure-turns-replay.log"
