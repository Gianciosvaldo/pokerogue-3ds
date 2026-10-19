/*
 * gacha_screen: the capsule machine's menu (task C09; docs/CUSTOM-CONTENT-CONTRACTS.md, section 2.15), opened from the title.
 *
 * Top screen (gacha_view.h, upstream's egg gacha screen): the three machines side by side, the chosen one centred and the strip sliding
 * on LEFT / RIGHT, the voucher counters and the option list. Bottom screen: the egg count against the capacity and the five options of
 * the machine in detail, each as the source defines it (gacha_host.h reads the table from game-core: which voucher, how many, how many
 * eggs). An option that cannot be bought is greyed and says why with the source's own refusal. A asks to pull; the question defaults to NO. Yes
 * runs ONE purchase through profile_tx and opens the capsule screen (C07) on its committed result.
 *
 * Buttons: LEFT / RIGHT machine, UP / DOWN option, A pull (then Yes / No), B leave. A held button does its action once (the auto-repeat
 * of a held button is ignored by every action; only the cursor moves keep repeating). The framework routes no touch input to ui
 * screens (only the shop's own layer reads the touch screen), so there is no touch path to a purchase: the buttons are the whole
 * control.
 *
 * Opening it first settles what a restart left behind: a purchase begun and not finished is finished (the same transaction, the same
 * result), and a committed pull that was never shown is shown, once. Neither pulls again. Until a shown result is closed no new
 * purchase can start.
 *
 * The words are the custom content's own (plain English). Costs, odds, capacity and the order eggs are shown in are the source's.
 */
#ifndef GM_GACHA_SCREEN_H
#define GM_GACHA_SCREEN_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  void (*on_exit)(void* arg);   /* B on the menu: called once, from the screen's input; the callee leaves the screen */
  void* arg;
  const char* (*species_name)(int species);  /* the reveal's row names (i18n_species_name); NULL: rows say "Egg <id>" */
} gachas_args;

/* Makes the machine the screen (ui_set, with a fade): the menu, or, when a restart left a purchase unfinished or a result not shown, the
 * capsule screen on that result first (closing it opens the menu). False (nothing opened) for NULL. */
bool gacha_screen_open(const gachas_args* a);

#ifdef __cplusplus
}
#endif
#endif
