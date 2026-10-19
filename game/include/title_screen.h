/*
 * title_screen: upstream's title (src/ui/handlers/title-ui-handler.ts, the options from title-phase.ts's showOptions).
 *
 * Top screen: the biome background of the last save (or the plains) and the logo from gfx.pak at upstream's place (centred,
 * 8 px from the top). Bottom screen: the option list in its corner: Continue (only with a last save), New Game, Load Game, Run History
 * (history_screen.h) and, last,
 * the custom content's own "Nemesis Radar" (radar_screen.h: a read-only look at the remembered enemies; upstream has no such entry)
 * and "Capsule Machine" (gacha_screen.h: the voucher-to-egg machines; upstream reaches them from the in-run menu, which this app has only
 * with Save and Quit so far), then "Custom Run" and "Custom Load" (custom_run.h: a Classic run with the optional Nemesis rematch, and its own five save
 * slots; New Game and Load Game never see them).
 *
 * Not on it, on purpose: Daily Run and Settings (no such screens yet), the player count and the account line (the
 * game is fully offline), the splash message and the version text.
 */
#ifndef GM_TITLE_SCREEN_H
#define GM_TITLE_SCREEN_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum { TITLE_CONTINUE, TITLE_NEW_GAME, TITLE_LOAD_GAME, TITLE_HISTORY, TITLE_RADAR, TITLE_CAPSULE, TITLE_CUSTOM_RUN, TITLE_CUSTOM_LOAD } title_choice;

typedef struct {
  bool can_continue;   /* shows the Continue entry */
  int biome;           /* BiomeId of the background, -1 = the plains */
  void (*on_choose)(title_choice c, void* arg);
  void* arg;
} title_args;

/* Makes the title the screen (ui_set, with a fade). */
void title_screen_open(const title_args* a);

#ifdef __cplusplus
}
#endif
#endif
