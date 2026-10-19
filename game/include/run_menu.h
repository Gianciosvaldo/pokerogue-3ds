/*
 * run_menu: the menu START opens during a run (upstream's MenuUiHandler, src/ui/handlers/menu-ui-handler.ts), with the entries
 * this app has. Today that is "Save and Quit"; upstream's other entries (settings, achievements, stats, egg list, Pokedex, manage
 * data, community, log out) have no screen here or are online features, so they are not listed.
 *
 * Save and Quit (MenuOptions.SAVE_AND_QUIT, only while a battle exists): when the battle is past its first turn the menu says
 * "You will lose any progress since the beginning of the battle. Proceed?" (menuUiHandler:losingProgressionWarning) and asks Yes / No;
 * Yes goes back to the title. Upstream's saveAll(true, true, true, true) writes the CACHED session and system data, that is, what
 * was saved at the start of the battle, and reloading continues from there; this app writes its save at that same moment (run_screen.c's
 * save point), so quitting writes nothing new and the caller only has to leave the run (run_screen.h, rs_quit).
 *
 * Layout (upstream's, in the bottom screen's UI_BOTTOM_UI space): the option window along the right edge, the full height of the
 * canvas; the message box in the 48 rows at the bottom left, as wide as what the window leaves; the Yes / No box at the window's left
 * edge. B closes the menu, and so does START (ui-inputs.ts buttonMenu).
 *
 * Plain English, not upstream's: the one sentence shown when the last save of this battle did not reach the SD card (the warning above
 * would be wrong then: more than the battle would be lost).
 */
#ifndef GM_RUN_MENU_H
#define GM_RUN_MENU_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  int turn;      /* the battle's turn (upstream's currentBattle.turn): the warning is shown when it is past 1 */
  bool unsaved;  /* the SD card refused the save of this battle: quitting goes back to the last save that did get written */
  void (*on_quit)(void* arg);   /* Save and Quit was confirmed (the menu has already left the stack) */
  void (*on_close)(void* arg);  /* the menu was closed without quitting (the menu has already left the stack) */
  void* arg;
} runmenu_args;

/* Pushes the menu over the screen below (ui_push). False when it is already open. */
bool run_menu_open(const runmenu_args* a);
bool run_menu_is_open(void);

#ifdef __cplusplus
}
#endif
#endif
