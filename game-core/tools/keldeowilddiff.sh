#!/bin/sh
# A wild Keldeo's form change (src/gc_moveset.c: gc_enemy_learned_form_target, gc_enemy_quiet_form_change) against the pinned upstream: upstream's own
# generateAndPopulateMoveset on a Keldeo of a random shape (game mode, boss or not, form, level, ability slot, nature, IVs, weather), the moves, the stats, the
# next draws of the RNG and the form change phase the call queued compared; then the queued QuietFormChangePhase run for a Pokemon on the field and visible, its
# form, ability, stats, HP, the dex flags and the text compared.
# usage: tools/keldeowilddiff.sh <upstream checkout> [N=1500] [seed=777]
set -eu
UP=${1:?pinned upstream dir}; N=${2:-1500}; SEED=${3:-777}
HERE=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
UP=$(CDPATH= cd -- "$UP" && pwd)
NODE=${NODE:-node}; BUILD=${BUILD:-$HERE/build}
case "$("$NODE" --version)" in v22.*) ;; *) echo 'Node 22 required' >&2; exit 2;; esac
[ "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d ] || { echo 'Wrong upstream pin' >&2; exit 2; }
make -s -j2 -C "$HERE" BUILD="$BUILD" "$BUILD/keldeo_wild_diff_test"
mkdir -p "$UP/test/tests/zz-dump"
cp "$HERE/tests/keldeo-wild.producer.test.ts" "$UP/test/tests/zz-dump/"
TRACE=${GC_KW_OUT:-$BUILD/keldeo-wild-$SEED.txt}
rm -f "$TRACE"
(cd "$UP" && GC_KW_N="$N" GC_KW_SEED="$SEED" GC_KW_OUT="$TRACE" "$NODE" node_modules/vitest/vitest.mjs run --maxWorkers=1 test/tests/zz-dump/keldeo-wild.producer.test.ts)
GC_LOCALES="$UP/locales/en" "$BUILD/keldeo_wild_diff_test" "$TRACE"
