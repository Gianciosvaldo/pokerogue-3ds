#!/bin/sh
# Dedicated metadata probe; source/C production and established tests unchanged.
set -eu
UP=${1:?upstream checkout}; N=${2:-2000}; SEED=${3:-4242}
HERE=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
UP=$(CDPATH= cd -- "$UP" && pwd)
[ "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d ] || exit 2
case "$(node --version)" in v22.*) ;; *) echo 'Node22 required' >&2; exit 2 ;; esac
python3 - "$N" "$SEED" <<'PY'
import sys
if not all(x.isdigit() for x in sys.argv[1:]) or not 120<=int(sys.argv[1])<=100000 or not 0<=int(sys.argv[2])<=4294967295:raise SystemExit(2)
PY
BUILD=${BUILD:-"$HERE/build"}
make -s -j2 -C "$HERE" BUILD="$BUILD" "$BUILD/optional-phase-test" "$BUILD/optional-phase-fixtures-test"
mkdir -p "$UP/test/tests/zz-dump"
python3 "$HERE/tools/build-optional-phase-source.py" "$HERE/tests/turntest.producer.test.ts" "$UP/test/tests/zz-dump/optional-phase.producer.test.ts"
cp "$HERE/tests/optional-phase-fixtures.producer.test.ts" "$UP/test/tests/zz-dump/"
EVIDENCE=$(mktemp -d "$BUILD/optional-phase-evidence.XXXXXX")
echo "Optional phase evidence: $EVIDENCE"
(cd "$UP" && TURN_TRAINER=1 TURN_N="$N" TURN_SEED="$SEED" TURN_OUT="$EVIDENCE/typed-$N-$SEED.txt" node node_modules/vitest/vitest.mjs run test/tests/zz-dump/optional-phase.producer.test.ts --maxWorkers=1) > "$EVIDENCE/typed.source.log" 2>&1
TURNTEST_LOCALES="$UP/locales/en" "$BUILD/optional-phase-test" "$EVIDENCE/typed-$N-$SEED.txt" > "$EVIDENCE/typed.replay.log" 2>&1
sed -n '/^cases /p; /^covered /p; /^texts of covered /p; /^Optional phase/p' "$EVIDENCE/typed.replay.log"
(cd "$UP" && OPTIONAL_OUT="$EVIDENCE/fixtures.txt" node node_modules/vitest/vitest.mjs run test/tests/zz-dump/optional-phase-fixtures.producer.test.ts --maxWorkers=1) > "$EVIDENCE/fixtures.source.log" 2>&1
"$BUILD/optional-phase-fixtures-test" "$EVIDENCE/fixtures.txt" > "$EVIDENCE/fixtures.replay.log" 2>&1
cat "$EVIDENCE/fixtures.replay.log"
