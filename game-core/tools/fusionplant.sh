#!/bin/sh
# DNA Splicers: show the comparator can fail. Replays one trace (from tools/fusiondiff.sh) through copies of game-core, each with one line
# of src/gc_fusion.c (or gc_items.c) changed, and prints each one's totals. Every copy must exit 1.
# Not planted: the fusion ability set by hand next to pr_refresh_ability (the refresh derives the same one once the ability lookup is known), the Gimmighoul tracker a Gimmighoul second member brings (needs one holding its tracker), and the Math.round edge
# (js_round against floor(x + 0.5) differs only when the average hp lands on 0.49999999999999994: planted below, caught only by a trace that has one).
# Usage: tools/fusionplant.sh <trace>   (FUSION_PLANT_FROM=<name> starts at that plant, FUSION_PLANT_ONLY=<name> runs just that one)
set -e
TRACE=$(CDPATH= cd -- "$(dirname "${1:?trace}")" && pwd)/$(basename "$1")
HERE=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
ROOT=$(CDPATH= cd -- "$HERE/.." && pwd)
WORK=${WORK:-/tmp/game-core-fusion-plant}
fails=0; total=0
started=0
plant() {
  NAME=$1; EXPR=$2; FILE=${3:-gc_fusion.c}
  [ "$NAME" != "${FUSION_PLANT_FROM:-}" ] || started=1
  [ -z "${FUSION_PLANT_FROM:-}" ] || [ "$started" = 1 ] || return 0
  [ -z "${FUSION_PLANT_ONLY:-}" ] || [ "$NAME" = "$FUSION_PLANT_ONLY" ] || return 0
  W="$WORK/$NAME"
  rm -rf "$W"; mkdir -p "$W/game-core"
  (cd "$HERE" && tar cf - --exclude=./build .) | tar xf - -C "$W/game-core"
  for d in battle-core run-core ai-core reward-core; do ln -s "$ROOT/$d" "$W/$d"; done
  sed -i "$EXPR" "$W/game-core/src/$FILE"
  n=$(diff "$HERE/src/$FILE" "$W/game-core/src/$FILE" | grep -c '^>' || true)
  [ "$n" = 1 ] || { echo "$NAME: the change didn't apply ($n lines)"; exit 2; }
  make -s -j2 -C "$W/game-core" BUILD="$W/build" "$W/build/fusion_diff_test" >/dev/null 2>&1
  st=0; "$W/build/fusion_diff_test" "$TRACE" >"$W/out.txt" 2>&1 || st=$?
  echo "$NAME: exit $st, $(grep 'TOTAL' "$W/out.txt")"
  total=$((total + 1))
  [ "$st" = 1 ] || fails=$((fails + 1))
}
plant old-max-hp 's/const int old_max = a->b.stats\[STAT_HP\];/const int old_max = a->b.stats[STAT_HP] + 1;/'
plant hp-average 's|(double)a->b.hp / old_max) / 2.0;|(double)a->b.hp / old_max) / 2.1;|'
plant hp-rounding 's/js_round((double)old_max \* pct)/floor((double)old_max * pct + 0.5)/'
plant fainted-hp-floor 's/^        p->hp = 1;/        p->hp = 2;/'
plant fainted-takes-status '/^        p->hp = 1;/{n;s/take_status(p, q);/(void)q;/}'
plant coin-inverted 's/gc_stub_rand(run, 2) == 0/gc_stub_rand(run, 2) == 1/'
plant status-none-kept 's/if (!has_status(p->status)) take_status(p, q);/if (0) take_status(p, q);/'
plant coin-without-status 's/else if (has_status(q->status) \&\& gc_stub_rand/else if (gc_stub_rand/'
plant learn-order 's/donor_moves\[i\], 0)/donor_moves[n_donor_moves - 1 - i], 0)/'
plant learn-place 's/RP_LEARN_MOVE, now, donor_moves/RP_LEARN_MOVE, primary, donor_moves/'
plant items-not-moved 's/^    gc_items_fuse_transfer(run, donor_id, primary);/    (void)donor_id;/'
plant stats-not-recalculated 's/^    gc_mon_recalc(run, a);/    (void)a;/'
plant tracker-not-pruned 's/^    gc_me_prune_items(run);/    (void)0;/'
plant donor-items-kept 's/^    gc_items_drop_held_of(run, donor_id);/    (void)donor_id;/'
plant field-pointer-shift 's/field_at\[f\] > donor ? field_at\[f\] - 1 : field_at\[f\]/field_at[f]/'
plant field-pointer-donor 's/run->bt.field\[f\] = field_at\[f\] == donor ? NULL : /run->bt.field[f] = /'
plant fusion-luck 's/^    a->fusion_luck = r->fusion_luck;/    a->fusion_luck = 0;/'
plant fusion-variant 's/^    a->fusion_variant = r->fusion_variant;/    a->fusion_variant = 0;/'
plant fusion-shiny 's/^    a->fusion_shiny = r->fusion_shiny;/    a->fusion_shiny = false;/'
plant evolution-pause 's/^    a->pause_evolutions = r->pause_evolutions;/    a->pause_evolutions = false;/'
plant fusion-gender 's/^    p->fusion_gender = r->fusion_gender;/    p->fusion_gender = 0;/'
plant locked-items-stay 's/if (!force \&\& (src.non_transferable/if ((src.non_transferable/' gc_items.c
plant orb-class 's/^    return (!strcmp(a->key, "TOXIC_ORB") .*$/    return false;/' gc_items.c
[ "$fails" = 0 ] || { echo "$fails of $total planted bug(s) not caught"; exit 1; }
echo "all $total planted bugs caught"
