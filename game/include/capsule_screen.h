/*
 * capsule_screen: the capsule-machine presentation prototype (task C07; docs/CUSTOM-CONTENT-CONTRACTS.md, section 2.13).
 *
 * A debug / test preview, not a purchase: it is given a fixed list of results (cap_fixture, capsule_model.h) and plays the machine over
 * them. Top screen: the capsules drop from the chute, bounce, settle in reading order and open. Bottom screen: the machine's controls
 * (the knob, with its button), then the list of what was shown. Nothing here touches a wallet, a profile, an unlock or any random
 * number generator; the motion is a fixed function of each result's position, so it never chooses or changes a result. The normal game
 * never opens it: the only way in is the debug file (rundriver.txt, "capsule=<1..25>"), and a pull in it says it is a preview.
 *
 * Buttons: A pulls the knob (once); during the drop B or START skips to the finished result (the supplied results are shown at once,
 * nothing is pulled again); when it is finished UP / DOWN scroll the list and A, B or START leave (after a short pause so a button still
 * being held does not close it); B leaves before the pull. A held button does its action once: the auto-repeat of a held button is
 * ignored (ui_input_repeating). The framework routes no touch input to ui screens (only the shop's own layer reads the touch screen), so
 * there is no touch gesture; the buttons are the whole control.
 *
 * Callbacks: on_settled is called once per opening, when the results are all shown, whether the time ran out or the player skipped. It
 * is never called if the screen is left before that. on_exit is called once per opening, from the screen's input, when the player
 * leaves; the callee leaves the screen. Neither is called from leave() or from a timer, and a callback of an earlier opening never
 * fires into a later one.
 *
 * The text is the custom content's own (plain English).
 *
 * Task C09 opens it for a purchase (caps_args.live, autoplay): the same screen, the same motion, the same callbacks; only the words
 * differ and the pull is accepted on open. The results are still the fixture's, shown in its order.
 */
#ifndef GM_CAPSULE_SCREEN_H
#define GM_CAPSULE_SCREEN_H

#include <stdbool.h>

#include "capsule_model.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  CAPS_EXIT_BEFORE_REVEAL,  /* left before the results were all shown */
  CAPS_EXIT_AFTER_REVEAL,   /* left after the results were shown */
} caps_exit;

typedef struct {
  const cap_fixture* fixture;                  /* copied when the screen opens */
  void (*on_settled)(void* arg);               /* may be NULL */
  void (*on_exit)(caps_exit how, void* arg);   /* may be NULL (then the screen can only be left by whoever opened it) */
  void* arg;
  /* Task C09: the screen shows a committed purchase instead of the preview. `live` changes the words only (eggs, not a preview: the
   * results were decided and paid for before this screen opened); `autoplay` accepts the pull as it opens, so there is no second
   * decision to make. The motion, the skip and the callbacks are the preview's. */
  bool live;
  bool autoplay;
  /* The machine whose art is drawn behind the eggs (0..2, gacha_view.h's machines); the preview and anything else draw machine 0. */
  uint8_t machine;
} caps_args;

/* Makes the preview the screen (ui_set, with a fade). False (nothing opened) for NULL or an invalid fixture (none, more than 25, a bad
 * tier or label). */
bool capsule_screen_open(const caps_args* a);

/* Which frame of the machine's hatch (1 closed .. 4 open) the model's time calls for: closed before the pull and once settled, opening at
 * 12 frames a second from the first capsule leaving the chute, held open until the last one has left, then closing. Pure: a function of
 * the model's state only (upstream's "open" / "close" animations of gacha_hatch, one egg at a time there, one burst here). */
int capsule_hatch_frame(const cap_model* m);

/* The fixed fixture of the debug entry: n results (1..25), tiers and labels from the position only. False for n outside 1..25. */
bool capsule_debug_fixture(int n, cap_fixture* out);

#ifdef __cplusplus
}
#endif
#endif
