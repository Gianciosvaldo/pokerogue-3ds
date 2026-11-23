#!/bin/sh
# The Pokedex list's model: show pokedex_model_test can fail. Each plant changes one line of a copy of source/pokedex_model.c, rebuilds the test with it
# and runs it on the fixture; every copy must exit 1.
# Usage: tests/pokedex_model_plant.sh [fixture]   (from any folder; the fixture defaults to fixtures/pokedex-list.native.txt.gz)
set -e
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
FIX=${1:-$HERE/fixtures/pokedex-list.native.txt.gz}
WORK=${WORK:-/tmp/pokedex-model-plant}
fails=0; total=0
rm -rf "$WORK"; mkdir -p "$WORK"
plant() {
  NAME=$1; EXPR=$2
  cp "$HERE/../source/pokedex_model.c" "$WORK/pokedex_model.c"
  sed -i "$EXPR" "$WORK/pokedex_model.c"
  n=$(diff "$HERE/../source/pokedex_model.c" "$WORK/pokedex_model.c" | grep -c '^>' || true)
  [ "$n" = 1 ] || { echo "$NAME: the change didn't apply ($n lines)"; exit 2; }
  make -s -C "$HERE" PXM_MODEL="$WORK/pokedex_model.c" PXM_OUT="$WORK/t" "$WORK/t" >/dev/null 2>&1 || { echo "$NAME: the changed copy doesn't build"; exit 2; }
  st=0; "$WORK/t" "$FIX" >"$WORK/out.txt" 2>&1 || st=$?
  echo "$NAME: exit $st, $(tail -1 "$WORK/out.txt")"
  total=$((total + 1))
  [ "$st" = 1 ] || fails=$((fails + 1))
}
plant starter-is-itself            's/return d \&\& d->starter > 0 ? (uint16_t)d->starter : species;/return species;/'
plant caught-not-masked-by-starter 's/ \& sys->dex\[starter\].caught_attr//'
plant caught-not-masked-by-unlocks '/dex\[starter\]/s/ \& gc_species_full_unlocks_data(species);$/;/'
plant seen-ignores-starter         's/return sys->dex\[pdx_starter_of(species)\].caught_attr != 0;/return false;/'
plant seen-ignores-own-entry       's/if (sys->dex\[species\].seen_attr) return true;/if (0) return true;/'
plant tint-grey-and-black-swapped  "s/return pdx_is_seen(s->sys, sp) ? 'g' : 'b';/return pdx_is_seen(s->sys, sp) ? 'b' : 'g';/"
plant stale-cells-overwritten      's/if (first + i < s->n) s->cell_species\[i\] = s->ids\[first + i\];/if (1) s->cell_species[i] = s->ids[first + i];/'
plant up-scrolls-a-row-early       's/if (s->scroll > 0 \&\& current_row - s->scroll == 0) {/if (s->scroll > 0 \&\& current_row - s->scroll == 1) {/'
plant down-scrolls-a-row-late      's/if (current_row - s->scroll == PDX_ROWS - 1) {/if (current_row - s->scroll == PDX_ROWS) {/'
plant down-wrap-loses-column       's/return set_cursor(s, s->cursor % PDX_COLS);/return set_cursor(s, s->cursor);/'
plant left-edge-only-first-cell    's/if (s->cursor % PDX_COLS == 0) {/if (s->cursor == 0) {/'
plant right-ignores-short-last-row 's/(current_row < num_rows - 1 ? PDX_COLS - 1 : (n - 1) % PDX_COLS)/(PDX_COLS - 1)/'
plant panel-ignores-first-form     's/const int form = caught ? current_form(sys, sp, caught) : 0;/const int form = 0;/'
plant panel-ignores-form-preference 's/if ((st->preference_flags \& 8) \&\& /if (0 \&\& /'
plant types-need-the-form-caught   's/if (!form_caught \&\& !pdx_is_seen(sys, sp)) return;/if (!form_caught) return;/'
plant name-shown-for-seen-starter  's/out->name_shown = sys->dex\[sp\].seen_attr != 0 || sys->dex\[sp\].caught_attr != 0;/out->name_shown = pdx_is_seen(sys, sp);/'
plant dex-number-modulo            's/out->dex_number = sp % 2000;/out->dex_number = sp % 1000;/'
plant move-succeeds-only-if-moved  's/return true; \/\* the container always has a species/return false; \/\* the container always has a species/'
echo "plants: $total run, $fails not caught"
[ "$fails" = 0 ]
