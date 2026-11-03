#!/bin/sh
set -eu
UP=${1:?pinned upstream checkout}
HERE=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
UP=$(CDPATH= cd -- "$UP" && pwd)
[ "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d ] || exit 2
[ "$(git -C "$UP/locales" rev-parse HEAD)" = 270ed0a2938b2c3e2e34e7de940e84aecadf548b ] || exit 2
git -C "$UP" diff --quiet || exit 2
case "$(node --version)" in v24.*) ;; *) exit 2 ;; esac
BUILD=${BUILD:-$HERE/build}
mkdir -p "$BUILD" "$UP/test/tests/zz-dump"
PRODUCER="$UP/test/tests/zz-dump/healing-wish-party-proof-$$.producer.test.ts"
TRACE="$BUILD/healing-wish-party-source.txt"
trap 'rm -f "$PRODUCER"' EXIT HUP INT TERM
cp "$HERE/tests/healing-wish-party.producer.test.ts" "$PRODUCER"
(cd "$UP" && HEALING_WISH_PARTY_OUT="$TRACE" node node_modules/vitest/vitest.mjs run "test/tests/zz-dump/$(basename "$PRODUCER")" --maxWorkers=1) > "$BUILD/healing-wish-party-source.log" 2>&1
cmp "$TRACE" "$HERE/tests/fixtures/healing-wish-party-source.txt"
make -s -C "$HERE" BUILD="$BUILD/native" "$BUILD/native/healing-wish-party-test"
"$BUILD/native/healing-wish-party-test" "$TRACE"
