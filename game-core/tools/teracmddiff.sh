#!/bin/sh
# The player's Terastallize command (src/gc_battle.c: gc_can_terastallize, gc_battle_command_screen, gc_battle_player_command) against the pinned
# upstream: canTerastallize, CommandUiHandler.canTera, getTeraType and CommandPhase.handleCommand(TERA) on a real singles and doubles battle with
# random species, forms, fusion halves, Tera types, moves and PP, Tera Orb and arena counter, compared by tests/tera_command_diff_test.c.
# usage: tools/teracmddiff.sh <upstream checkout> [N=600] [seed=777]
set -eu
UP=${1:?pinned upstream dir}; N=${2:-600}; SEED=${3:-777}
HERE=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
UP=$(CDPATH= cd -- "$UP" && pwd)
NODE=${NODE:-node}; BUILD=${BUILD:-$HERE/build}
case "$("$NODE" --version)" in v22.*) ;; *) echo 'Node 22 required' >&2; exit 2;; esac
[ "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d ] || { echo 'Wrong upstream pin' >&2; exit 2; }
make -s -j2 -C "$HERE" BUILD="$BUILD" "$BUILD/tera_command_diff_test"
mkdir -p "$UP/test/tests/zz-dump"
cp "$HERE/tests/tera-command.producer.test.ts" "$UP/test/tests/zz-dump/"
TRACE=${GC_TERA_OUT:-$BUILD/tera-command-$SEED.txt}
rm -f "$TRACE"
(cd "$UP" && GC_TERA_N="$N" GC_TERA_SEED="$SEED" GC_TERA_OUT="$TRACE" "$NODE" node_modules/vitest/vitest.mjs run --maxWorkers=1 test/tests/zz-dump/tera-command.producer.test.ts >"$BUILD/tera-command-$SEED.log" 2>&1) || { echo "producer failed: $BUILD/tera-command-$SEED.log" >&2; exit 2; }
"$BUILD/tera_command_diff_test" "$TRACE"
