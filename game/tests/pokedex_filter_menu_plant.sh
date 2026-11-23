#!/bin/sh
# The Pokedex filter menu: show pokedex_screen_test can fail. Each plant changes one line of a copy of source/pokedex_filter_menu.c, rebuilds the test with
# it and runs it; every copy must exit 1.
# Usage: tests/pokedex_filter_menu_plant.sh   (from any folder)
set -e
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
WORK=${WORK:-/tmp/pokedex-filter-menu-plant}
fails=0; total=0
rm -rf "$WORK"; mkdir -p "$WORK"
plant() {
  NAME=$1; EXPR=$2
  cp "$HERE/../source/pokedex_filter_menu.c" "$WORK/pokedex_filter_menu.c"
  sed -i "$EXPR" "$WORK/pokedex_filter_menu.c"
  n=$(diff "$HERE/../source/pokedex_filter_menu.c" "$WORK/pokedex_filter_menu.c" | grep -c '^>' || true)
  [ "$n" = 1 ] || { echo "$NAME: the change didn't apply ($n lines)"; exit 2; }
  make -s -C "$HERE" PXS_MENU="$WORK/pokedex_filter_menu.c" PXS_OUT="$WORK/t" "$WORK/t" >/dev/null 2>&1 || { echo "$NAME: the changed copy doesn't build"; exit 2; }
  st=0; "$WORK/t" >"$WORK/out.txt" 2>&1 || st=$?
  echo "$NAME: exit $st, $(tail -1 "$WORK/out.txt")"
  total=$((total + 1))
  [ "$st" = 1 ] || fails=$((fails + 1))
}
plant last-generation-clearable 's/    if (m) f->gens = m;/    f->gens = m;/'
plant last-caught-clearable     's/    if (m) f->caught = m;/    f->caught = m;/'
plant passive-order             's/static const uint8_t passive\[\] = {PDF_OFF, PDF_ON, PDF_UNLOCKABLE, PDF_EXCLUDE};/static const uint8_t passive[] = {PDF_OFF, PDF_ON, PDF_EXCLUDE, PDF_UNLOCKABLE};/'
plant reduction-skips-two       's/static const uint8_t reduction\[\] = {PDF_OFF, PDF_ON, PDF_ONE, PDF_TWO, PDF_UNLOCKABLE, PDF_EXCLUDE};/static const uint8_t reduction[] = {PDF_OFF, PDF_ON, PDF_ONE, PDF_UNLOCKABLE, PDF_EXCLUDE, PDF_EXCLUDE};/'
plant exclude-label-no          's/st = "Locked";/st = "No";/'
plant passive-on-label          's/st = "Unlocked";/st = "Yes";/'
plant change-not-reported       '0,/if (M.a.on_change) M.a.on_change(M.a.arg);/s//;/'
plant toggle-not-reported       '/^  ui_play_select();$/{n;s/if (M.a.on_change) M.a.on_change(M.a.arg);/;/}'
plant done-reopens              's/default: return; \/\* Done \*\//default: show(FM_TOP); return;/'
plant reset-keeps-sort          's/\*f = fresh;/fresh.sort = f->sort, *f = fresh;/'
plant seen-steps-the-wrong-column 's/case FR_SEEN: f->seen = step(f->seen, yes_no, 3); break;/case FR_SEEN: f->encountered = step(f->encountered, yes_no, 3); break;/'
plant order-not-flipped         's/case FR_ORDER: f->descending = !f->descending; break;/case FR_ORDER: break;/'
plant sort-stops-early          's/f->sort = (uint8_t)((f->sort + 1) % PDF_SORT_COUNT);/f->sort = (uint8_t)((f->sort + 1) % (PDF_SORT_COUNT - 1));/'
plant caught-list-without-uncaught 's/{PDF_C_NORMAL, "Normal"}, {PDF_C_UNCAUGHT, "Not caught"}};/{PDF_C_NORMAL, "Normal"}, {PDF_C_NORMAL, "Not caught"}};/'
plant button-counts-nothing     's/const int k = pdf_active(f);/const int k = 0;/'
echo "plants: $total run, $fails not caught"
[ "$fails" = 0 ]
