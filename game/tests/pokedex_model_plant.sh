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
plant gens-ignored                 's/!(gens \& (1u << (sp->generation - 1)))/0/'
plant second-type-ignored          's/if (form->type2 >= 0 \&\& form->type2 < 18/if (0 \&\& form->type2 >= 0 \&\& form->type2 < 18/'
plant uncaught-ignored             's/if ((cf \& PDF_C_UNCAUGHT) \&\& uncaught) caught_ok = true;/if ((cf \& PDF_C_UNCAUGHT) \&\& 0) caught_ok = true;/'
plant shiny2-counts-shiny3         's/(cf \& PDF_C_SHINY2) \&\& v2 \&\& !v3/(cf \& PDF_C_SHINY2) \&\& v2/'
plant filter-caught-unmasked       's/const uint64_t a = pdx_caught_attr(sys, species);/const uint64_t a = sys->dex[species].caught_attr;/'
plant passive-exclude-ignores-egg-moves 's/case PDF_EXCLUDE: if (!(progressable \&\& !passive_unlocked)) return false; break;/case PDF_EXCLUDE: if (passive_unlocked) return false; break;/'
plant passive-unlockable-when-unlocked 's/ \&\& !(st->passive_attr \& 1u);/;/'
plant reduction-unlockable-past-two 's/st->value_reduction < 2 \&\& st->candy_count >= reduction_costs/st->candy_count >= reduction_costs/'
plant reduction-two-as-one         's/case PDF_TWO: if (!two) return false; break;/case PDF_TWO: if (!one) return false; break;/'
plant empty-egg-row-is-progressable 's/return m\[0\] || m\[1\] || m\[2\] || m\[3\];/return true;/'
plant starter-of-the-line          's/if (!tri(f->starter, gc_starter_cost(species) >= 0)) return false;/if (!tri(f->starter, gc_starter_cost(starter) >= 0)) return false;/'
plant favorite-everyone            's/if (!tri(f->favorite, false)) return false;/if (!tri(f->favorite, true)) return false;/'
plant win-of-the-species           's/if (!tri(f->win, st->classic_win_count > 0)) return false;/if (!tri(f->win, sys->starters[species].classic_win_count > 0)) return false;/'
plant hidden-ability-exclude-any   's/!(species_has_ha \&\& !has_ha)/!(!has_ha)/'
plant seen-with-starter-shortcut   's/dex->seen_attr != 0 || dex->caught_attr != 0/pdx_is_seen(sys, species)/'
plant encountered-by-seen-attr     's/tri(f->encountered, dex->seen_count != 0)/tri(f->encountered, dex->seen_attr != 0)/'
plant egg-price-too-high           's/const bool egg = egg_cost >= 0 \&\& st->candy_count >= egg_cost;/const bool egg = egg_cost >= 0 \&\& st->candy_count >= egg_cost * 4;/'
plant pokerus-none                 's/const bool pokerus = s->pokerus \&\& s->pokerus(species);/const bool pokerus = 0;/'
plant descending-ignored           's/return (d < 0) != s->filter.descending ? -1 : 1;/return d < 0 ? -1 : 1;/'
plant sort-cost-of-the-species     '/case PDF_SORT_COST: d =/s/pdx_starter_of(a)/a/'
plant sort-candy-of-the-species    '/case PDF_SORT_CANDY: d =/s/starters\[pdx_starter_of(a)\]/starters[a]/'
plant sort-iv-five-values          's/for (int i = 0; i < 6; i++) t += g_sort_state->sys->dex\[id\].ivs\[i\];/for (int i = 0; i < 5; i++) t += g_sort_state->sys->dex[id].ivs[i];/'
plant sort-caught-of-the-starter   '/case PDF_SORT_CAUGHT: d =/s/dex\[a\].caught_count/dex[pdx_starter_of(a)].caught_count/'
plant sort-hatched-of-the-species  '/case PDF_SORT_HATCHED: d =/s/dex\[pdx_starter_of(a)\].hatched_count/dex[a].hatched_count/'
plant name-sort-first-three-letters 's/d = strcmp(na, s->name(b));/d = strncmp(na, s->name(b), 3);/'
plant filter-keeps-scroll          's/^  s->scroll = 0;$/  s->scroll = s->scroll;/'
echo "plants: $total run, $fails not caught"
[ "$fails" = 0 ]
