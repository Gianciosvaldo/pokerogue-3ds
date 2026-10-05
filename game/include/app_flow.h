/*
 * app_flow: how the app moves between its screens: title -> (Continue | New Game -> a slot | Load Game -> a slot) -> the run
 * -> game over or clear -> title.
 *
 *   Continue   loads the slot used last (run_saves.h's last slot), as upstream's Continue loads lastSessionSlot.
 *   New Game   asks for a slot first (an empty one, or a run to overwrite after a yes/no); a slot whose file can't be loaded is
 *              never overwritten.
 *   Load Game  the slot list with Load Game / Delete Run on each.
 *   The run    is saved into its slot at every save point (run_screen.c).
 *   Game over / clear: the result screen, and the slot is cleared (upstream's tryClearSession in PostGameOverPhase).
 *
 * A slot that can't be loaded (damaged, made by a newer version) says so on the slot list and in the message after Continue; the
 * game never repairs, overwrites or deletes it on its own.
 */
#ifndef GM_APP_FLOW_H
#define GM_APP_FLOW_H

#include "run_screen.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Shows the title. `first_run` is the debug config (rundriver.txt: seed, starters) used for the first New Game of this
 * session only; its `entropy` is the base of every run's randomness. */
void app_start(const rs_config* first_run);

#ifdef __cplusplus
}
#endif
#endif
