#!/bin/sh
# Nicknamed Pokemon's names in messages against the pinned upstream's own getNameToRender / getPokemonNameWithAffix / pokemon.name.
# Usage: tools/nickname-diff.sh PINNED_UPSTREAM [N [SEED]]   (needs the setup-upstream.sh checkout and Node 22)
set -eu
UP=${1:?pinned upstream}; N=${2:-8192}; SEED=${3:-777}
HERE=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
UP=$(CDPATH= cd -- "$UP" && pwd)
[ "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d ] || exit 2
[ "$(git -C "$UP/locales" rev-parse HEAD)" = 270ed0a2938b2c3e2e34e7de940e84aecadf548b ] || exit 2
git -C "$UP" diff --quiet || exit 2
case "$(node --version)" in v22.*) ;; *) exit 2 ;; esac
make -s -C "$HERE" build/nickname-message-test
mkdir -p "$UP/test/tests/zz-dump" "$HERE/build"
cp "$HERE/tests/nickname-message.producer.test.ts" "$UP/test/tests/zz-dump/nickname-message.producer.test.ts"
TRACE="$HERE/build/nickname-message-$SEED.txt"
(cd "$UP" && NICKNAME_N="$N" NICKNAME_SEED="$SEED" NICKNAME_OUT="$TRACE" node node_modules/vitest/vitest.mjs run "test/tests/zz-dump/nickname-message.producer.test.ts" --maxWorkers=1) > "$HERE/build/nickname-message-source-$SEED.log" 2>&1
TURNTEST_LOCALES="$UP/locales/en" "$HERE/build/nickname-message-test" "$TRACE"
