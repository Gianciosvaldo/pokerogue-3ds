#!/bin/sh
# Egg vouchers as rewards: show the comparator can fail. Replays one trace (from tools/voucherapplydiff.sh) through copies of
# game-core, each with one line of src/gc_stubs.c or src/gc_reward.c changed, and prints each one's totals. Every copy
# must exit 1.
# Usage: GC_LOCALES=<upstream dir>/locales/en tools/voucherapplyplant.sh <trace>
set -e
: "${GC_LOCALES:?GC_LOCALES must name upstream's locales/en folder}"
export GC_LOCALES
TRACE=$(CDPATH= cd -- "$(dirname "${1:?trace}")" && pwd)/$(basename "$1")
HERE=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
ROOT=$(CDPATH= cd -- "$HERE/.." && pwd)
WORK=${WORK:-/tmp/game-core-voucher-plant}
fails=0
plant() {
  NAME=$1; FILE=$2; EXPR=$3
  W="$WORK/$NAME"
  rm -rf "$W"; mkdir -p "$W/game-core"
  (cd "$HERE" && tar cf - --exclude=./build .) | tar xf - -C "$W/game-core"
  for d in battle-core run-core ai-core reward-core; do ln -s "$ROOT/$d" "$W/$d"; done
  sed -i "$EXPR" "$W/game-core/src/$FILE"
  n=$(diff "$HERE/src/$FILE" "$W/game-core/src/$FILE" | grep -c '^>' || true)
  [ "$n" = 1 ] || { echo "$NAME: the change didn't apply ($n lines)"; exit 2; }
  make -s -j2 -C "$W/game-core" BUILD="$W/build" "$W/build/voucher_apply_diff_test" >/dev/null 2>&1
  st=0; "$W/build/voucher_apply_diff_test" "$TRACE" >"$W/out.txt" 2>&1 || st=$?
  echo "$NAME: exit $st, $(grep '^cases' "$W/out.txt")"
  [ "$st" = 1 ] || fails=$((fails + 1))
}
plant pick-adds-nothing gc_stubs.c 's/^    if (give_voucher(run, k)) return true;/    if (0 \&\& give_voucher(run, k)) return true;/'
plant pick-adds-an-item gc_stubs.c 's/^    if (give_voucher(run, k)) return true;/    if (give_voucher(run, k)) { gc_item_add(run, k, -1, 0, 1); return true; }/'
plant pick-counted-as-stub gc_stubs.c 's/^    if (give_voucher(run, k)) return true;/    if (give_voucher(run, k)) { gc_stub(run, GC_STUB_REWARD_APPLY); return true; }/'
plant plus-counts-premium gc_stubs.c 's/\(VOUCHER_PLUS")) gc_system_add_voucher_count(run->sys, \)GC_VOUCHER_PLUS/\1GC_VOUCHER_PREMIUM/'
plant premium-counts-two gc_stubs.c 's/GC_VOUCHER_PREMIUM, 1);/GC_VOUCHER_PREMIUM, 2);/'
plant regular-counts-plus gc_stubs.c 's/GC_VOUCHER_REGULAR, 1);/GC_VOUCHER_PLUS, 1);/'
plant given-adds-nothing gc_stubs.c 's/^    if (give_voucher(run, key)) return -1;/    if (!strncmp(key, "VOUCHER", 7)) return -1;/'
plant text-count-two gc_reward.c 's/pr_arg_int("modifierCount", 1), pr_arg_key("voucherTypeName"/pr_arg_int("modifierCount", 2), pr_arg_key("voucherTypeName"/'
plant text-plus-named-regular gc_reward.c 's/"voucher:eggVoucherPlus"/"voucher:eggVoucher"/'
plant text-regular-unnamed gc_reward.c 's/^    const int voucher = !strcmp(key, "VOUCHER") ?/    const int voucher = 0 \&\& !strcmp(key, "VOUCHER") ?/'
[ "$fails" = 0 ] || { echo "$fails planted bug(s) not caught"; exit 1; }
echo "all ten planted bugs caught"
