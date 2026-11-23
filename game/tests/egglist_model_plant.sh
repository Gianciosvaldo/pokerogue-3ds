#!/bin/sh
# The Egg List's model: show egglist_model_test can fail. Each plant changes one line of a copy of source/egglist_model.c or source/scroll_grid.c,
# rebuilds the test with it and runs it on the fixture; every copy must exit 1.
# Usage: tests/egglist_model_plant.sh [fixture]   (from any folder; the fixture defaults to fixtures/egg-list.native.txt.gz)
set -e
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
FIX=${1:-$HERE/fixtures/egg-list.native.txt.gz}
WORK=${WORK:-/tmp/egglist-model-plant}
fails=0; total=0
rm -rf "$WORK"; mkdir -p "$WORK"
# plant <name> <model|grid> <sed expression>
plant() {
  NAME=$1; WHICH=$2; EXPR=$3
  cp "$HERE/../source/egglist_model.c" "$WORK/egglist_model.c"
  cp "$HERE/../source/scroll_grid.c" "$WORK/scroll_grid.c"
  if [ "$WHICH" = model ]; then F=egglist_model.c; else F=scroll_grid.c; fi
  sed -i "$EXPR" "$WORK/$F"
  n=$(diff "$HERE/../source/$F" "$WORK/$F" | grep -c '^>' || true)
  [ "$n" = 1 ] || { echo "$NAME: the change didn't apply ($n lines)"; exit 2; }
  make -s -C "$HERE" EGM_MODEL="$WORK/egglist_model.c" EGM_GRID="$WORK/scroll_grid.c" EGM_OUT="$WORK/t" "$WORK/t" >/dev/null 2>&1 || { echo "$NAME: the changed copy doesn't build"; exit 2; }
  st=0; "$WORK/t" "$FIX" >"$WORK/out.txt" 2>&1 || st=$?
  echo "$NAME: exit $st, $(tail -1 "$WORK/out.txt")"
  total=$((total + 1))
  [ "$st" = 1 ] || fails=$((fails + 1))
}
plant manaphy-id-divisor        model 's/e->id % 204u == 0/e->id % 205u == 0/'
plant manaphy-any-tier          model 's/e->tier == EGG_TIER_COMMON \&\& //'
plant key-is-tier-plus-one      model 's/(unsigned)e->tier);/(unsigned)e->tier + 1);/'
plant epic-shows-rare           model 's/else if (e->tier == 2) tier = t->tier\[2\];/else if (e->tier == 2) tier = t->tier[1];/'
plant waves-soon-below-5        model 's/w <= 5 ? 0/w < 5 ? 0/'
plant waves-close-below-15      model 's/w <= 15 ? 1/w < 15 ? 1/'
plant waves-notclose-below-50   model 's/w <= 50 ? 2/w < 50 ? 2/'
plant date-month-one-digit      model 's/"%s, %02d\/%d\/%lld"/"%s, %d\/%d\/%lld"/'
plant date-epoch-off-by-a-day   model 's/z += 719468;/z += 719469;/'
plant date-ignores-local-time   model 's/+ tz_offset_s;/;/'
plant date-weekday-shifted      model 's/{"Thu", "Fri", "Sat", "Sun", "Mon", "Tue", "Wed"}/{"Fri", "Sat", "Sun", "Mon", "Tue", "Wed", "Thu"}/'
plant date-range-edge           model 's/timestamp_ms > JS_MAX_TIME_MS/timestamp_ms >= JS_MAX_TIME_MS/'
plant legendary-names-own-species model 's/name(gc_gacha_featured_species(e->timestamp_ms), ud)/name(e->species, ud)/'
plant legendary-no-parentheses  model 's/"%s (%s)", t->gacha_legendary/"%s [%s]", t->gacha_legendary/'
plant event-shows-shiny-text    model 's/snprintf(out->type, sizeof out->type, "%s", t->event_type)/snprintf(out->type, sizeof out->type, "%s", t->gacha_shiny)/'
plant species-not-substituted   model 's/!strncmp(p, "{{species}}", 11)/!strncmp(p, "{{Species}}", 11)/'
plant panel-ignores-scroll      model 's/s->detail = cursor + sg_item_offset(&s->grid);/s->detail = cursor;/'
plant cell-ignores-scroll       model 's/return cell >= 0 \&\& cell < shown ? cell + offset : -1;/return cell >= 0 \&\& cell < shown ? cell : -1;/'
plant ceil-becomes-floor        grid  's/return a <= 0 ? 0 : (a + b - 1) \/ b;/return a <= 0 ? 0 : a \/ b;/'
plant max-scroll-one-more       grid  's/const int max_scroll = imax(0, total_rows - on_screen_rows);/const int max_scroll = imax(0, total_rows - on_screen_rows + 1);/'
plant up-wrap-keeps-cursor      grid  's/if (new_cursor > last_visible) new_cursor -= g->cols;/if (new_cursor > last_visible + g->cols) new_cursor -= g->cols;/'
plant down-not-clamped          grid  's/set_cursor(g, imin(g->cursor + g->cols, g->total - item_offset - 1))/set_cursor(g, g->cursor + g->cols)/'
plant left-last-row-skipped     grid  's/g->scroll == max_scroll \&\& row == on_screen_rows - 1/g->scroll == max_scroll \&\& row == on_screen_rows/'
plant right-stops-at-end        grid  's/g->cursor + item_offset < g->total - 1) {/g->cursor + item_offset < g->total - 2) {/'
plant past-end-cursor           grid  's/g->total - g->scroll \* g->cols - 1);/g->total - g->scroll * g->cols);/'
plant up-wraps-to-top           grid  's/success = set_scroll_cursor(g, max_scroll, new_cursor, true);/success = set_scroll_cursor(g, 0, new_cursor, true);/'
plant down-wrap-column-lost     grid  's/success = set_scroll_cursor(g, 0, g->cursor % g->cols, true);/success = set_scroll_cursor(g, 0, 0, true);/'
echo "plants: $total run, $fails not caught"
[ "$fails" = 0 ]
