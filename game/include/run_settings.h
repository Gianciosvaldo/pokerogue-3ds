/*
 * run_settings: the screen New Game opens before the slot list (this port's own, lorenzo's layout; upstream has no such step), and the
 * settings it keeps on the SD card.
 *
 *   Mode        Classic, Classic (Bizarre), Challenge, Endless, Endless (Spliced): the last four once their unlock is earned (Challenge with Endless, as upstream lists them) (a Classic win opens Bizarre
 *               and Endless; the Spliced one needs a winning party with a fusion). Remembered as the next screen's default. Bizarre is the
 *               Classic run with the JoJo trainers and the Pillar Men on (custom_run.h); Classic and Bizarre both have the Nemesis rematches.
 *   Game Speed  upstream's gameSpeed setting (settings-ui-items.ts): Slow 2, Normal 3, Fast 4, Turbo 5. Upstream divides every tween,
 *               timer and text delay by it (system/game-speed.ts); ui.c does the same by running the screens' clock that much faster
 *               (ui_set_game_speed). It applies at once and is remembered.
 *   Start       on_start with the values (then the slot list).
 *
 * B goes back (on_back). Left / Right change the highlighted value; A on a value row changes it too.
 */
#ifndef GM_RUN_SETTINGS_H
#define GM_RUN_SETTINGS_H

#include <stdbool.h>

#include "gc/run.h"

#ifdef __cplusplus
extern "C" {
#endif

enum { RSET_SPEED_SLOW = 2, RSET_SPEED_NORMAL = 3, RSET_SPEED_FAST = 4, RSET_SPEED_TURBO = 5 };

/* The Mode row's own value for Classic (Bizarre): not a game mode of game-core's (the run is a Classic one), so it sits after upstream's list. */
#define RSET_MODE_BIZARRE 5

typedef struct {
  int speed; /* RSET_SPEED_* */
  int mode;  /* GAME_MODES_CLASSIC / CHALLENGE / ENDLESS / SPLICED_ENDLESS, or RSET_MODE_BIZARRE */
} rset_values;

/* The saved values ("settings" on the SD card), or the defaults (Classic, Normal) when there is none or it can't be read. */
rset_values rset_load(void);
/* Writes them; false when the card refused. */
bool rset_save(const rset_values* v);
/* The speed the screens run at: upstream's gameSpeed over the port's written durations, which are upstream's own (a speed of 3 makes
 * them three times shorter, as upstream's Normal does). */
float rset_time_scale(int speed);
/* The speed the in-battle Y button moves to: Slow, Normal, Fast, Turbo, back to Slow. A speed that is none of them (the football game runs at 1)
 * goes to Normal. */
int rset_next_speed(int speed);
/* The preset a live game speed (ui_game_speed) stands for; Normal for a scale that is none of them. */
int rset_speed_from_scale(float scale);
/* The locale key of the preset's name ("settings:gameSpeedFast"); Normal's for a speed that is none of them. */
const char* rset_speed_key(int speed);

typedef struct {
  rset_values values;  /* what the screen starts with */
  bool bizarre_unlocked;
  bool endless_unlocked;
  bool spliced_endless_unlocked;
  void (*on_start)(const rset_values* v, void* arg);
  void (*on_back)(void* arg);
  void* arg;
} rset_args;

/* Pushes the screen over the one below (the title stays on the top screen). It pops itself before on_start / on_back. */
void rset_open(const rset_args* a);

#ifdef __cplusplus
}
#endif
#endif
