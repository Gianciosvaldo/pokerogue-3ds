#!/bin/sh
# The Egg List screen: show egglist_screen_test can fail. Each plant changes one line of a copy of source/egglist_screen.c, rebuilds the test with
# it and runs it; every copy must exit 1.
# Usage: tests/egglist_screen_plant.sh   (from any folder)
set -e
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
WORK=${WORK:-/tmp/egglist-screen-plant}
fails=0; total=0
rm -rf "$WORK"; mkdir -p "$WORK"
plant() {
  NAME=$1; EXPR=$2
  cp "$HERE/../source/egglist_screen.c" "$WORK/egglist_screen.c"
  sed -i "$EXPR" "$WORK/egglist_screen.c"
  n=$(diff "$HERE/../source/egglist_screen.c" "$WORK/egglist_screen.c" | grep -c '^>' || true)
  [ "$n" = 1 ] || { echo "$NAME: the change didn't apply ($n lines)"; exit 2; }
  make -s -C "$HERE" ELS_SCREEN="$WORK/egglist_screen.c" ELS_OUT="$WORK/t" "$WORK/t" >/dev/null 2>&1 || { echo "$NAME: the changed copy doesn't build"; exit 2; }
  st=0; "$WORK/t" >"$WORK/out.txt" 2>&1 || st=$?
  echo "$NAME: exit $st, $(tail -1 "$WORK/out.txt")"
  total=$((total + 1))
  [ "$st" = 1 ] || fails=$((fails + 1))
}
plant name-one-pixel-down       's/text_draw(TS_SUMMARY, 8, 68,/text_draw(TS_SUMMARY, 8, 69,/'
plant date-one-pixel-down       's/text_draw(TS_EGG_LIST, 8, 91,/text_draw(TS_EGG_LIST, 8, 92,/'
plant wrap-width                's/#define WRAP_W 90.0f/#define WRAP_W 91.0f/'
plant type-line-shows-waves     's/text_draw(TS_EGG_LIST, 8, 152, d.type, &o);/text_draw(TS_EGG_LIST, 8, 152, d.waves, \&o);/'
plant egg-picture-moved         's/img_draw("images\/egg\/egg", frame, 54, 37,/img_draw("images\/egg\/egg", frame, 54, 38,/'
plant icon-scale                's/ro.scale_x = ro.scale_y = 0.5f;/ro.scale_x = ro.scale_y = 0.6f;/'
plant icon-x-off-by-one         's/(float)(ICON_X0 + (i % EGL_COLS) \* CELL - 2)/(float)(ICON_X0 + (i % EGL_COLS) * CELL - 1)/'
plant bob-period                's/((int)(ui_now() \/ 200.0f)) \& 1/((int)(ui_now() \/ 300.0f)) \& 1/'
plant bob-moves-all-icons       's/(i == S.L.cursor \&\& bob ? 2 : 0)/(bob ? 2 : 0)/'
plant cursor-x-off-by-one       's/(float)(ICON_X0 + 1 + (c % EGL_COLS) \* CELL - 2)/(float)(ICON_X0 + (c % EGL_COLS) * CELL - 2)/'
plant zone-ignored              's/S.args.tz_offset ? S.args.tz_offset(S.eggs\[S.L.detail\].timestamp_ms) : 0/0/'
plant eggs-copied-short         's/memcpy(S.eggs, a->eggs, (size_t)S.n \* sizeof S.eggs\[0\]);/memcpy(S.eggs, a->eggs, (size_t)(S.n - 1) * sizeof S.eggs[0]);/'
plant leave-repeats             's/^      S.left = true;/      S.left = false;/'
plant bar-at-nine-rows          's/if (total_rows <= EGL_ROWS) return;/if (total_rows < EGL_ROWS) return;/'
plant empty-message-moved       's/text_draw(TS_WINDOW, 12, 78,/text_draw(TS_WINDOW, 12, 79,/'
plant left-right-swapped        's/egl_input(&S.L, (sg_dir)(b - BTN_UP))/egl_input(\&S.L, (sg_dir)(b == BTN_LEFT ? SG_RIGHT : b == BTN_RIGHT ? SG_LEFT : b - BTN_UP))/'
plant sound-on-every-press      's/if (S.n > 0 \&\& egl_input(&S.L, (sg_dir)(b - BTN_UP))) ui_play_select();/if (S.n > 0 \&\& (egl_input(\&S.L, (sg_dir)(b - BTN_UP)) || 1)) ui_play_select();/'
plant tap-cell-width            's/const int col = (int)(gx \/ CELL), row = (int)(gy \/ CELL);/const int col = (int)(gx \/ (CELL + 1)), row = (int)(gy \/ CELL);/'
plant hint-zone                 's/if (y >= HINT_Y - 8)/if (y >= HINT_Y + 8)/'
plant tap-on-empty-cell-moves   's/if (egl_cell_egg(&S.L, cell) < 0) return false; \/\* an empty cell \*\//if (egl_cell_egg(\&S.L, cell) < -1) return false; \/\* an empty cell *\//'
plant eggs-over-capacity        's/a->n > GC_MAX_EGGS ? GC_MAX_EGGS : a->n/a->n > GC_MAX_EGGS ? GC_MAX_EGGS + 1 : a->n/'
echo "plants: $total run, $fails not caught"
[ "$fails" = 0 ]
