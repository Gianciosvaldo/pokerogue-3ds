#!/bin/sh
# Records the turns the bench replays: runs battle-core's turntest producer inside an upstream
# checkout (prepared by ../battle-core/tools/setup-upstream.sh) once per kind of turn, into
# romfs/cases/. The records must come from the same battle-core commit the bench is built
# against (the record format grows with the engine), so rerun this after pulling battle-core.
# Also records damage.txt (the damage difftest's cases), whose calculations are timed one by one.
# Usage: tools/make-cases.sh <upstream dir> [N per kind] [seed] [damage cases]
set -e
UP=${1:?upstream dir}; N=${2:-400}; SEED=${3:-777}; DN=${4:-1000}
HERE=$(cd "$(dirname "$0")/.." && pwd)
BC=$HERE/../battle-core
OUT=$HERE/romfs/cases
mkdir -p "$OUT" "$UP/test/tests/zz-dump"
cp "$BC/tests/turntest.producer.test.ts" "$BC/tests/difftest.producer.test.ts" "$UP/test/tests/zz-dump/"
(
  cd "$UP"
  DIFF_OUT="$OUT/damage.txt" DIFF_N="$DN" DIFF_SEED="$SEED" \
    npx vitest run test/tests/zz-dump/difftest.producer.test.ts > "$OUT/damage.log" 2>&1 \
    || { echo "damage: producer failed, see $OUT/damage.log"; exit 1; }
) &
for m in plain RICH STATUS ONHIT WEATHER CALLED PROTECT ARENA; do
  (
    cd "$UP"
    # plain turns set no mode flag; TURN_PLAIN is not read by the producer
    if [ "$m" = plain ]; then flag=TURN_PLAIN; else flag=TURN_$m; fi
    env "$flag=1" TURN_OUT="$OUT/$m.txt" TURN_N="$N" TURN_SEED="$SEED" \
      npx vitest run test/tests/zz-dump/turntest.producer.test.ts > "$OUT/$m.log" 2>&1 \
      || { echo "$m: producer failed, see $OUT/$m.log"; exit 1; }
  ) &
done
wait
for m in plain RICH STATUS ONHIT WEATHER CALLED PROTECT ARENA; do
  [ "$(grep -c '^CASE ' "$OUT/$m.txt")" = "$N" ] || { echo "$m: expected $N cases"; exit 1; }
  rm -f "$OUT/$m.log"
done
[ -s "$OUT/damage.txt" ] || { echo "damage: no cases"; exit 1; }
rm -f "$OUT/damage.log"
REV=$(git -C "$BC" rev-parse --short HEAD)
DIRTY=$(git -C "$BC" diff --quiet HEAD -- . && echo "" || echo "-dirty")
UPREV=$(git -C "$UP" rev-parse --short HEAD)
printf 'battle-core %s%s, upstream %s, %s turns per kind, %s damage cases, seed %s\n' "$REV" "$DIRTY" "$UPREV" "$N" "$DN" "$SEED" > "$OUT/MANIFEST.txt"
cat "$OUT/MANIFEST.txt"
