#!/bin/sh
set -eu
UP=${1:?usage: native-pokerus-trap.sh /path/to/pinned-upstream}
HERE=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
UP=$(CDPATH= cd -- "$UP" && pwd)
[ "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d ]
[ "$(git -C "$UP/locales" rev-parse HEAD)" = 270ed0a2938b2c3e2e34e7de940e84aecadf548b ]
git -C "$UP" diff --quiet
case "$(node --version)" in v22.*) ;; *) echo 'Node22 required' >&2; exit 2 ;; esac
mkdir -p "$HERE/build" "$UP/test/tests/zz-dump"
cp "$HERE/tests/pokerus-trap.producer.test.ts" "$UP/test/tests/zz-dump/pokerus-trap.producer.test.ts"
TRACE="$HERE/build/pokerus-trap.native.txt"
(cd "$UP" && POKERUS_TRAP_OUT="$TRACE" node node_modules/vitest/vitest.mjs run test/tests/zz-dump/pokerus-trap.producer.test.ts --maxWorkers=1) > "$HERE/build/pokerus-trap-native.log" 2>&1
make -C "$HERE" build/pokerus_trap_test
"$HERE/build/pokerus_trap_test" "$TRACE"
cmp "$TRACE" "$HERE/tests/fixtures/pokerus-trap.native.txt"
