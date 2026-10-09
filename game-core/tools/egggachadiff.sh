#!/bin/sh
# Real pinned gacha admission/debit + Egg/EggData, explicit entropy/time inputs.
set -eu
UP=${1:?pinned upstream dir}; N=${2:-2000}; SEED=${3:-777}
case "$N" in ''|*[!0-9]*) echo 'integer N required' >&2; exit 2;; esac
case "$SEED" in ''|*[!0-9]*) echo 'unsigned seed required' >&2; exit 2;; esac
[ "$N" -ge 120 ] && [ "$N" -le 100000 ] && [ "$SEED" -le 4294967295 ] || exit 2
HERE=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
UP=$(CDPATH= cd -- "$UP" && pwd)
NODE=${NODE:-node}
case "$("$NODE" --version)" in v22.*) ;; *) echo 'Node22 required' >&2; exit 2;; esac
[ "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d ] || exit 2
[ "$(sha256sum "$UP/assets/images/pokemon/variant/_masterlist.json" | cut -d' ' -f1)" = c776d4ad0eb7fe63f00b76bc1cfc0dde4bf60404195a2a1634ef3b2e3dcf9b50 ] || exit 2
BUILD=${BUILD:-$HERE/build}
mkdir -p "$BUILD" "$UP/test/tests/zz-dump"
make -s -j2 -C "$HERE" BUILD="$BUILD" "$BUILD/egg_gacha_diff_test" "$BUILD/egg_gacha_host_test"
cp "$HERE/tests/egg-gacha.producer.test.ts" "$UP/test/tests/zz-dump/"
TRACE="$BUILD/egg-$SEED.txt"
rm -f "$TRACE"
(cd "$UP" && GC_EGG_N="$N" GC_EGG_SEED="$SEED" GC_EGG_OUT="$TRACE" "$NODE" node_modules/vitest/vitest.mjs run --maxWorkers=1 test/tests/zz-dump/egg-gacha.producer.test.ts)
"$BUILD/egg_gacha_diff_test" "$TRACE"
"$BUILD/egg_gacha_host_test"
