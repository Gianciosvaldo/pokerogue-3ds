/*
 * ball_menu: upstream's BallUiHandler (src/ui/handlers/ball-ui-handler.ts), the Poke Ball list the
 * BALL command opens, over the bottom-right corner of the bottom screen.
 */
#ifndef GM_BALL_MENU_H
#define GM_BALL_MENU_H

/* Upstream's PokeballType, without the unused LUXURY_BALL (pokeballCounts leaves it out). */
enum { BALL_POKE, BALL_GREAT, BALL_ULTRA, BALL_ROGUE, BALL_MASTER, BALL_TYPES };

/* Opens the list with the player's ball counts (upstream pokeballCounts). on_pick gets the ball
 * type (only one the player has; a ball with none plays the error sound and stays), or -1 when
 * the player picks Cancel or presses CANCEL. The menu closes itself before on_pick. The cursor
 * stays where it was between openings, as upstream's handler keeps it. */
void ball_menu_open(const int counts[BALL_TYPES], void (*on_pick)(int ball, void* arg), void* arg);

#endif
