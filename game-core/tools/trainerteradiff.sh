#!/bin/sh
# Trainer Terastallization (src/gc_battle.c: gc_trainer_gen_ai, gc_trainer_instant_teras) against the pinned upstream: the real Trainer.genAI run on every
# trainer config with a sown RNG, then config.trainerAI.instantTeras and Trainer.shouldTera per slot, on random trainer types, waves, party sizes and parties
# (Shedinja often), compared by tests/trainer_tera_diff_test.c.
set -eu
UP=${1:?pinned upstream dir}; N=${2:-600}; SEED=${3:-777}
HERE=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
UP=$(CDPATH= cd -- "$UP" && pwd)
NODE=${NODE:-node}; BUILD=${BUILD:-$HERE/build}
case "$("$NODE" --version)" in v22.*) ;; *) echo 'Node 22 required' >&2; exit 2;; esac
[ "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d ] || { echo 'Wrong upstream pin' >&2; exit 2; }
make -s -j2 -C "$HERE" BUILD="$BUILD" "$BUILD/trainer_tera_diff_test"
mkdir -p "$UP/test/tests/zz-dump"
cp "$HERE/tests/trainer-tera.producer.test.ts" "$UP/test/tests/zz-dump/"
TRACE=${GC_TTERA_OUT:-$BUILD/trainer-tera-$SEED.txt}
rm -f "$TRACE"
(cd "$UP" && GC_TTERA_N="$N" GC_TTERA_SEED="$SEED" GC_TTERA_OUT="$TRACE" "$NODE" node_modules/vitest/vitest.mjs run --maxWorkers=1 test/tests/zz-dump/trainer-tera.producer.test.ts >"$BUILD/trainer-tera-$SEED.log" 2>&1) || { echo "producer failed: $BUILD/trainer-tera-$SEED.log" >&2; exit 2; }
"$BUILD/trainer_tera_diff_test" "$TRACE"
