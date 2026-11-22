#!/bin/sh
# The Game Stats screen: show gamestats_screen_test can fail. Each plant changes one line of a copy of source/gamestats_screen.c, rebuilds the test
# with it and runs it; every copy must exit 1.
# Usage: tests/gamestats_screen_plant.sh   (from any folder)
set -e
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
WORK=${WORK:-/tmp/gamestats-screen-plant}
fails=0; total=0
rm -rf "$WORK"; mkdir -p "$WORK"
plant() {
  NAME=$1; EXPR=$2
  cp "$HERE/../source/gamestats_screen.c" "$WORK/gamestats_screen.c"
  sed -i "$EXPR" "$WORK/gamestats_screen.c"
  n=$(diff "$HERE/../source/gamestats_screen.c" "$WORK/gamestats_screen.c" | grep -c '^>' || true)
  [ "$n" = 1 ] || { echo "$NAME: the change didn't apply ($n lines)"; exit 2; }
  make -s -C "$HERE" GSS_SCREEN="$WORK/gamestats_screen.c" GSS_OUT="$WORK/t" "$WORK/t" >/dev/null 2>&1 || { echo "$NAME: the changed copy doesn't build"; exit 2; }
  st=0; "$WORK/t" >"$WORK/out.txt" 2>&1 || st=$?
  echo "$NAME: exit $st, $(tail -1 "$WORK/out.txt")"
  total=$((total + 1))
  [ "$st" = 1 ] || fails=$((fails + 1))
}
plant second-column-x-off          's/(float)(8 + (col ? COL_BG_W : 0))/(float)(8 + (col ? COL_BG_W + 10 : 0))/'
plant row-height-off               's/const float y = (float)(ENTRY_Y0 + row \* ENTRY_H);/const float y = (float)(ENTRY_Y0 + row * 15);/'
plant value-x-off                  's/(float)(col ? COL_BG_W \* 2 - 8 : COL_BG_W - 8)/(float)(col ? COL_BG_W * 2 - 9 : COL_BG_W - 8)/'
plant cursor-can-pass-the-end      's/c > GSM_MAX_CURSOR || c == S.cursor/c > GSM_MAX_CURSOR + 1 || c == S.cursor/'
plant up-arrow-late                's/if (S.cursor > 0) arrow/if (S.cursor > 1) arrow/'
plant refused-scroll-plays-sound   's/if (set_cursor(S.cursor + (b == BTN_DOWN ? 1 : -1))) ui_play_select();/{ set_cursor(S.cursor + (b == BTN_DOWN ? 1 : -1)); ui_play_select(); }/'
plant hidden-not-masked            's/if (gsm_label_hidden(r))/if (0)/'
plant scroll-one-stat-not-two      's/const int idx = S.cursor \* GSM_COLUMNS + slot;/const int idx = S.cursor + slot;/'
plant play-time-ignores-the-clock  's/const uint32_t play = S.args.play_seconds ? S.args.play_seconds() : S.args.sys->gstats.play_time;/const uint32_t play = S.args.sys->gstats.play_time;/'
plant touch-halves-swapped         's/y < HEADER_H + STATS_BG_H \/ 2 ? BTN_UP : BTN_DOWN/y < HEADER_H + STATS_BG_H \/ 2 ? BTN_DOWN : BTN_UP/'
plant leaves-more-than-once        's/^      S.left = true;/      S.left = false;/'
plant header-without-name          's/i18n_t(S.header, sizeof S.header, "gameStatsUiHandler:stats", "username", guest, NULL);/i18n_t(S.header, sizeof S.header, "gameStatsUiHandler:stats", "username", "", NULL);/'
echo "plants: $total run, $fails not caught"
[ "$fails" = 0 ]
