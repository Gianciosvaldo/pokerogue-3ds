/*
 * gamestats_model: what upstream's Game Stats screen shows (src/ui/handlers/game-stats-ui-handler.ts, its `displayStats` table), as plain
 * data: for each of the 40 stats in upstream's order, its label key, whether it is a hidden one, and its value as text. No drawing, no i18n:
 * the screen (gamestats_screen.c) looks the labels up. A hidden stat whose value is 0 is shown with the label "???" (gsm_label_hidden).
 *
 * The numbers come from the profile (gc_system: stats and gstats, filled by game-core's Game Stats recorders; the starter and Pokedex
 * counts are counted from the dex). Play time is passed in: the profile's plus what the device has pending (system_data.h).
 */
#ifndef GM_GAMESTATS_MODEL_H
#define GM_GAMESTATS_MODEL_H

#include <stdbool.h>
#include <stdint.h>

#include "gc/system.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GSM_COUNT 40

typedef struct {
  const char* label_key; /* i18n key, "gameStatsUiHandler:totalBattles" */
  bool hidden;           /* shown as "???" while the value is 0 */
  char value[40];
} gsm_row;

/* The abbreviations formatFancyLargeNumber uses for 10^3, 10^6 and 10^9 ("k", "M", "B": common:abrThousand...), index 0 is "" (10^0). */
#define GSM_ABBR 4

/* Fills rows[0..GSM_COUNT-1]. `abbr` (may be NULL: the English ones) is the four abbreviations. */
void gsm_build(const gc_system* sys, uint32_t play_seconds, const char* const abbr[GSM_ABBR], gsm_row rows[GSM_COUNT]);

/* The label to show: the row's own, or "???" (returned as NULL: the caller shows its "???") for a hidden stat whose value is 0. True: show "???". */
bool gsm_label_hidden(const gsm_row* r);

/* upstream's getPlayTimeString: "dd:hh:mm:ss" */
void gsm_play_time_string(uint32_t seconds, char* out, int n);
/* upstream's formatFancyLargeNumber(n) (3 decimals, abbreviations as above) */
void gsm_fancy_number(uint32_t n, const char* const abbr[GSM_ABBR], char* out, int n_out);

/* Scrolling: the screen shows GSM_PER_COL rows per column in GSM_COLUMNS columns; the position moves one row of the table (two stats) at a time. */
#define GSM_COLUMNS 2
#define GSM_PER_COL 9
#define GSM_PER_PAGE (GSM_COLUMNS * GSM_PER_COL)
/* the highest scroll position: ceil((GSM_COUNT - GSM_PER_PAGE) / GSM_COLUMNS) */
#define GSM_MAX_CURSOR ((GSM_COUNT - GSM_PER_PAGE + GSM_COLUMNS - 1) / GSM_COLUMNS)

#ifdef __cplusplus
}
#endif
#endif
