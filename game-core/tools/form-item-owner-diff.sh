#!/bin/sh
set -eu
UP=$(CDPATH= cd -- "${1:?pinned upstream checkout}" && pwd)
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
PROOF_OUT=${PROOF_OUT:-$ROOT/game-core/build/form-item-owner-native}
mkdir -p "$PROOF_OUT"
# This also verifies upstream/source/locales/assets pins and regenerates the
# registration facts independently of production C tables.
BUILD="$PROOF_OUT" sh "$ROOT/battle-core/tools/held-form-items-diff.sh" "$UP"
cp "$ROOT/game-core/tests/form-item-owner.producer.test.ts" "$UP/test/tests/zz-dump/"
cp "$ROOT/game-core/tests/form-item-phase.producer.test.ts" "$UP/test/tests/zz-dump/"
(cd "$UP" && FORM_OWNER_OUT="$PROOF_OUT/form-item-owner.native.json" node node_modules/vitest/vitest.mjs \
    run test/tests/zz-dump/form-item-owner.producer.test.ts --maxWorkers=1)
for MODE in normal focus fusion; do
    FOCUS=0
    FUSION=0
    if test "$MODE" = focus; then FOCUS=1; fi
    if test "$MODE" = fusion; then FUSION=1; fi
    (cd "$UP" && FORM_PHASE_FOCUS="$FOCUS" FORM_PHASE_FUSION="$FUSION" \
        FORM_PHASE_OUT="$PROOF_OUT/form-item-phase.$MODE.native.json" node node_modules/vitest/vitest.mjs \
        run test/tests/zz-dump/form-item-phase.producer.test.ts --maxWorkers=1)
done
python3 "$ROOT/game-core/tools/build-form-item-owner-proof.py" "$PROOF_OUT/form-item-owner.native.json" "$PROOF_OUT/menu.native.txt"
python3 "$ROOT/game-core/tools/build-form-item-acquisition-proof.py" "$PROOF_OUT/form-item-owner.native.json" "$PROOF_OUT/acquisition.native.txt"
python3 "$ROOT/game-core/tools/build-form-item-phase-proof.py" "$PROOF_OUT/held-form-items.native.json" \
    "$PROOF_OUT/form-item-phase.normal.native.json" "$PROOF_OUT/form-item-phase.focus.native.json" \
    "$PROOF_OUT/form-item-phase.fusion.native.json" "$PROOF_OUT/phase.native.txt"
make -C "$ROOT/game-core" BUILD=build build/form_item_menu_host_test build/form_item_acquisition_host_test build/form_item_phase_host_test
for KIND in menu acquisition phase; do
    TURNTEST_LOCALES="$UP/locales/en" "$ROOT/game-core/build/form_item_${KIND}_host_test" "$PROOF_OUT/$KIND.native.txt"
done
