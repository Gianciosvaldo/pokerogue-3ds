#!/bin/sh
set -eu
UP=${1:?upstream checkout}; N=${2:-2000}; SEED=${3:-777}
HERE=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
UP=$(CDPATH= cd -- "$UP" && pwd)
[ "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d ] || exit 2
case "$(node --version)" in v22.*) ;; *) exit 2 ;; esac
make -s -C "$HERE" -j2 build/leave-weather-test
mkdir -p "$UP/test/tests/zz-dump"
python3 "$HERE/tools/build-leave-weather-source.py" "$HERE/tests/trainer-switch.producer.test.ts" "$UP/test/tests/zz-dump/leave-weather.producer.test.ts"
TRACE="$HERE/build/leave-weather-$SEED.txt"
rm -f "$TRACE"
(cd "$UP" && LEAVE_WEATHER_N="$N" LEAVE_WEATHER_SEED="$SEED" LEAVE_WEATHER_OUT="$TRACE" node node_modules/vitest/vitest.mjs run test/tests/zz-dump/leave-weather.producer.test.ts --maxWorkers=1) > "$HERE/build/leave-weather-$SEED-source.log" 2>&1
sed -n '/LEAVE_WEATHER cases=/p; /LEAVE_WEATHER leaves/p' "$HERE/build/leave-weather-$SEED-source.log"
TURNTEST_LOCALES="$UP/locales/en" "$HERE/build/leave-weather-test" "$TRACE"
