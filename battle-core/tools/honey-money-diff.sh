#!/bin/sh
# Actual pinned callbacks/owning prefix/money owners; native RNG restored after GameManager startup.
set -eu
HERE=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
UP=$(CDPATH= cd -- "$1" && pwd)
[ "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d ]
[ "$(git -C "$UP/locales" rev-parse HEAD)" = 270ed0a2938b2c3e2e34e7de940e84aecadf548b ]
git -C "$UP" diff --exit-code HEAD -- src >/dev/null
git -C "$UP/locales" diff --exit-code HEAD >/dev/null
node -e 'if(process.versions.node.split(".")[0]!=="22")process.exit(1)'
mkdir -p "$HERE/work" "$UP/test/tests/zz-dump"
python3 "$HERE/battle-core/tools/build-honey-money-proof.py" "$UP/test/tests/zz-dump/honey-money.producer.test.ts"
python3 "$HERE/battle-core/tools/build-honey-owner-proof.py" "$UP/test/tests/zz-dump/honey-owner.producer.test.ts"
cp "$HERE/game-core/tests/wide_money.producer.test.ts" "$UP/test/tests/zz-dump/"
make -C "$HERE/battle-core" -j2 build/honey-money-test build/honey-owner-test build/money-number-test
make -C "$HERE/game-core" -j1 build/wide_money_test build/honey_money_host_test
export TURNTEST_LOCALES="$UP/locales/en"
for SEED in 777 2034; do
 (cd "$UP" && HONEY_MONEY_SEED="$SEED" HONEY_MONEY_OUT="$HERE/work/honey-money-$SEED.txt" node node_modules/vitest/vitest.mjs run --maxWorkers=1 test/tests/zz-dump/honey-money.producer.test.ts)
 "$HERE/battle-core/build/honey-money-test" "$HERE/work/honey-money-$SEED.txt"
 (cd "$UP" && HONEY_OWNER_SEED="$SEED" HONEY_OWNER_OUT="$HERE/work/honey-owner-$SEED.txt" node node_modules/vitest/vitest.mjs run --maxWorkers=1 test/tests/zz-dump/honey-owner.producer.test.ts)
 "$HERE/battle-core/build/honey-owner-test" "$HERE/work/honey-owner-$SEED.txt"
 (cd "$UP" && WIDE_MONEY_SEED="$SEED" WIDE_MONEY_OUT="$HERE/work/wide-money-$SEED.txt" node node_modules/vitest/vitest.mjs run --maxWorkers=1 test/tests/zz-dump/wide_money.producer.test.ts)
 "$HERE/game-core/build/wide_money_test" "$HERE/work/wide-money-$SEED.txt"
 MONEY_NUMBER_SEED="$SEED" MONEY_NUMBER_OUT="$HERE/work/money-number-$SEED.txt" node "$HERE/battle-core/tools/money-number.producer.mjs"
 "$HERE/battle-core/build/money-number-test" "$HERE/work/money-number-$SEED.txt"
done
"$HERE/game-core/build/honey_money_host_test"
python3 "$HERE/battle-core/tools/check-honey-money-proof.py" "$HERE/work/honey-money-777.txt" "$HERE/work/honey-owner-777.txt" "$HERE/work/wide-money-777.txt" "$HERE/work/money-number-777.txt"
