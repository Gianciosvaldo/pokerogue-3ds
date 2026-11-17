/*
 * achv_toast: upstream's AchvBar as one service for the whole app. Upstream keeps a single bar in the UI container (ui.achvBar): whatever earns an
 * achievement or a voucher calls showAchv, the bar slides in from the right edge of the field, stays, slides out, and the next one waits its
 * turn, on any screen and without taking the input. Here the bars are queued with atoast_push; ui_tick updates the queue (atoast_update) and draws
 * the bar over every screen and over the fade (atoast_draw, with toast_view.c's drawing of it).
 *
 * Timing, as upstream's and as the Classic win's bars had it before they moved here: 500 ms in (Sine.easeOut), in place until 10 s after the bar
 * began, 500 ms out (Sine.easeIn), the achievement sound ("se/achv") as each begins. One deviation, for the custom achievements only (`brisk`): when
 * more bars are waiting behind it, a brisk bar stays 3 s instead of 10, so a profile that unlocks twenty at once is not twenty bars of 10 s.
 */
#ifndef GM_ACHV_TOAST_H
#define GM_ACHV_TOAST_H

#include <stdbool.h>

#include "toast_view.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ATOAST_IN_MS 500.0f
#define ATOAST_OUT_AT_MS 10000.0f
#define ATOAST_BRISK_OUT_AT_MS 3000.0f
#define ATOAST_OUT_MS 500.0f
#define ATOAST_QUEUE 48

/* The words of a custom achievement's bar (game-core's gc/achv.h table): its name, what it asks and its reward ("Reward: 1 Egg Voucher"), and the
 * art for that reward (a voucher's own icon, an egg's) in the tier of the bar. False for an id outside the table. */
typedef struct {
  char title[64];
  char desc[192];
  char score[16];     /* none: the custom achievements have no points */
  const char* icon;   /* frame of the "items" atlas (a string that lives for the whole run of the program) */
  int tier;           /* 0..4 */
} atoast_text;
bool atoast_text_of(int achv_id, atoast_text* out);

/* Queues a bar (the strings are copied, `icon` must stay valid). It begins at once, with its sound, when none is up. `brisk`: see above. A full
 * queue drops the bar. */
void atoast_push(const tv_info* t, bool brisk);

/* Queues the bar of the custom achievement `achv_id` (brisk). Nothing for an id outside the table. */
void atoast_push_achv(int achv_id);

/* ui_tick: time passes, dt in ms of game time; the next bar begins when the one before it has left. */
void atoast_update(float dt);

/* ui_tick, once per physical screen after the screens and the fade: draws the bar that is up on the top screen (nothing on the bottom one). */
void atoast_draw(void);

/* A bar is up or waiting. */
bool atoast_active(void);

/* Drops the bar that is up and the ones waiting. */
void atoast_reset(void);

#ifdef __cplusplus
}
#endif
#endif
