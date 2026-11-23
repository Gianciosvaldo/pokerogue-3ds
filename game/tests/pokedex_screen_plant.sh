#!/bin/sh
# The Pokedex list screen: show pokedex_screen_test can fail. Each plant changes one line of a copy of source/pokedex_screen.c, rebuilds the test with
# it and runs it; every copy must exit 1.
# Usage: tests/pokedex_screen_plant.sh   (from any folder)
set -e
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
WORK=${WORK:-/tmp/pokedex-screen-plant}
fails=0; total=0
rm -rf "$WORK"; mkdir -p "$WORK"
plant() {
  NAME=$1; EXPR=$2
  cp "$HERE/../source/pokedex_screen.c" "$WORK/pokedex_screen.c"
  sed -i "$EXPR" "$WORK/pokedex_screen.c"
  n=$(diff "$HERE/../source/pokedex_screen.c" "$WORK/pokedex_screen.c" | grep -c '^>' || true)
  [ "$n" = 1 ] || { echo "$NAME: the change didn't apply ($n lines)"; exit 2; }
  make -s -C "$HERE" PXS_SCREEN="$WORK/pokedex_screen.c" PXS_OUT="$WORK/t" "$WORK/t" >/dev/null 2>&1 || { echo "$NAME: the changed copy doesn't build"; exit 2; }
  st=0; "$WORK/t" >"$WORK/out.txt" 2>&1 || st=$?
  echo "$NAME: exit $st, $(tail -1 "$WORK/out.txt")"
  total=$((total + 1))
  [ "$st" = 1 ] || fails=$((fails + 1))
}
plant number-one-pixel-down     's/text_draw(TS_SUMMARY, NAME_X, NUMBER_Y, number, NULL);/text_draw(TS_SUMMARY, NAME_X, NUMBER_Y + 1, number, NULL);/'
plant name-one-pixel-down       's/text_draw(TS_SUMMARY, NAME_X, NAME_Y, name ? name : "???", NULL);/text_draw(TS_SUMMARY, NAME_X, NAME_Y + 1, name ? name : "???", NULL);/'
plant unseen-name-shown         's/const char\* name = p.name_shown ? i18n_species_name(p.species) : "???";/const char* name = i18n_species_name(p.species);/'
plant second-type-at-first      's/if (p.type2 >= 0) pxa_type(p.type2, TYPE_X + TYPE_DX, TYPE_Y);/if (p.type2 >= 0) pxa_type(p.type2, TYPE_X, TYPE_Y);/'
plant types-always-hidden       's/if (p.type1 >= 0) pxa_type(p.type1, TYPE_X, TYPE_Y);/if (p.type1 >= 99) pxa_type(p.type1, TYPE_X, TYPE_Y);/'
plant icon-scale                's/cell_y(i) + 2 - (i == S.L.cursor \&\& cur_caught \&\& bob ? 2 : 0), 0.5f, tint);/cell_y(i) + 2 - (i == S.L.cursor \&\& cur_caught \&\& bob ? 2 : 0), 0.6f, tint);/'
plant icon-x-off-by-one         's/pxa_icon(S.L.cell_species\[i\], cell_x(i) - 2,/pxa_icon(S.L.cell_species[i], cell_x(i) - 1,/'
plant icon-row-height           's/#define CELL_H 17/#define CELL_H 18/'
plant icon-column-width         's/#define CELL_W 18/#define CELL_W 17/'
plant grey-and-black-swapped    's/const rr_color tint = t == .c. ? RR_RGBA(255, 255, 255, 255) : t == .g. ? RR_RGBA(0x80, 0x80, 0x80, 255) : RR_RGBA(0, 0, 0, 255);/const rr_color tint = t == '"'"'c'"'"' ? RR_RGBA(255, 255, 255, 255) : t == '"'"'g'"'"' ? RR_RGBA(0, 0, 0, 255) : RR_RGBA(0x80, 0x80, 0x80, 255);/'
plant bob-period                's/((int)(ui_now() \/ 200.0f)) \& 1/((int)(ui_now() \/ 300.0f)) \& 1/'
plant bob-moves-all-icons       's/(i == S.L.cursor \&\& cur_caught \&\& bob ? 2 : 0)/(cur_caught \&\& bob ? 2 : 0)/'
plant bob-without-caught-line   's/(i == S.L.cursor \&\& cur_caught \&\& bob ? 2 : 0)/(i == S.L.cursor \&\& bob ? 2 : 0)/'
plant cursor-x-off-by-one       's/cell_x(S.L.cursor) - 1, cell_y(S.L.cursor) + 1, 0, 0);/cell_x(S.L.cursor), cell_y(S.L.cursor) + 1, 0, 0);/'
plant leave-repeats             's/^      S.left = true;/      S.left = false;/'
plant sound-on-every-press      's/} else if (moved \&\& mode == PDX_GRID) ui_play_select();/} else if (1) ui_play_select();/'
plant left-right-swapped        's/const bool moved = pdx_input(&S.L, (pdx_button)(b - BTN_UP), \&mode);/const bool moved = pdx_input(\&S.L, (pdx_button)(b == BTN_LEFT ? PDX_RIGHT : b == BTN_RIGHT ? PDX_LEFT : b - BTN_UP), \&mode);/'
plant handle-ignores-scroll     's/(float)S.L.scroll;/0.0f;/'
plant handle-height             's/PDX_ROWS \/ (float)total_rows;/PDX_ROWS \/ (float)total_rows + 1;/'
plant tap-cell-width            's/const int col = (int)(gx \/ CELL_W), row = (int)(gy \/ CELL_H);/const int col = (int)(gx \/ (CELL_W + 1)), row = (int)(gy \/ CELL_H);/'
plant tap-column-limit          's/if (col >= PDX_COLS || row >= PDX_ROWS) return false;/if (col > PDX_COLS || row >= PDX_ROWS) return false;/'
plant hint-zone                 's/if (y >= HINT_Y - 8)/if (y >= HINT_Y + 8)/'
plant profile-copy-writes       's/const bool cur_caught = pdx_cell_visible(&S.L, S.L.cursor) \&\& S.args.sys->dex\[pdx_starter_of(cur_species)\].caught_attr != 0;/const bool cur_caught = (((gc_system*)S.args.sys)->dex[1].seen_attr = 0, pdx_cell_visible(\&S.L, S.L.cursor) \&\& S.args.sys->dex[pdx_starter_of(cur_species)].caught_attr != 0);/'
plant filter-button-too-wide    's/#define FILTER_W 80/#define FILTER_W 200/'
plant cursor-kept-after-filter  's/^  S.L.cursor = 0;$/  S.L.cursor = S.L.cursor;/'
plant empty-list-draws-panel    's/if (!pdx_cell_visible(&S.L, S.L.cursor)) return; \/\* an empty list has no species under the cursor \*\//if (0) return;/'
plant empty-list-message-hidden 's/if (S.L.n == 0) text_draw(/if (S.L.n == -1) text_draw(/'
plant filter-applied-stale      's/pdx_apply_filter(&S.L, &S.F);/pdx_apply_filter(\&S.L, \&S.L.filter);/'
plant pokerus-not-given         's/S.L.pokerus = a->pokerus;/S.L.pokerus = NULL;/'
plant names-not-given           's/S.L.name = name_of;/S.L.name = NULL;/'
plant bar-entry-silent          '/if (mode == PDX_FILTER_BAR) {/{n;s/ui_play_select();/;/}'
plant bar-entry-no-menu         's/^        open_filter();$/        ;/'
echo "plants: $total run, $fails not caught"
[ "$fails" = 0 ]
