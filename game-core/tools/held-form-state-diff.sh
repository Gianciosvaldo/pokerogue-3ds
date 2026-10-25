#!/bin/sh
set -eu
UP=${1:?pinned upstream checkout}
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
BUILD=${BUILD:-$ROOT/game-core/build}
# Retain the complete native callback matrix; its producer also observes actual
# constructor, clone and ModifierData restoration for every item identity.
BUILD="$BUILD" "$ROOT/battle-core/tools/held-form-items-diff.sh" "$UP"
make -C "$ROOT/game-core" BUILD=build build/held_form_items_host_test
"$ROOT/game-core/build/held_form_items_host_test" "$BUILD/held-form-constructors.native.txt" \
    "$ROOT/game-core/tests/fixtures/held-form.pr399.save"
