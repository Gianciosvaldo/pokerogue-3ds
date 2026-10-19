/*
 * char_sprite (chs_*): upstream's CharSprite (src/ui/containers/char-sprite.ts): the character portrait that slides in from the right edge of the
 * field while someone talks (the rival at the end of a Classic run), with the expression changing as the text goes (@c{variant}).
 *
 * The atlas is gfx.pak's images/character/<name> (upstream's `c_<name>` texture), a frame per expression. The portrait stands with its
 * feet at y = 138 of the 320 x 180 field, centred at x = 218 (field width - 102) once in; it comes in from x = 352 over 750 ms
 * (Cubic.easeOut) and goes out the same way (Cubic.easeIn); a new expression fades in over the old one in 250 ms (Sine.easeIn).
 * Time is the caller's: chs_update(dt) in ms, chs_draw() in the field's ui space.
 */
#ifndef GM_CHAR_SPRITE_H
#define GM_CHAR_SPRITE_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CHS_X_OUT 352.0f /* field width + 32 */
#define CHS_X_IN 218.0f  /* field width - 102 */
#define CHS_Y 138.0f     /* field height - 42 */
#define CHS_SLIDE_MS 750.0f
#define CHS_FADE_MS 250.0f

/* Brings the portrait in: `atlas` is the gfx.pak key ("images/character/rival_f"), `variant` the frame. Shown already with the same atlas:
 * only the expression changes. Nothing is drawn when the atlas is missing, and nothing else is affected. */
void chs_show(const char* atlas, const char* variant);
/* The next expression (fades in over the current one). No-op when nothing is shown or it is the one already on. */
void chs_set_variant(const char* variant);
/* Takes the portrait out; chs_busy() is true until it is gone. */
void chs_hide(void);
/* Forgets it at once (a new screen). */
void chs_reset(void);

void chs_update(float dt_ms);
void chs_draw(void);

/* Sliding in or out: upstream's showCharacter / hide promises are pending. */
bool chs_busy(void);
/* In, or on its way in (upstream's `shown`). */
bool chs_shown(void);
/* Where it stands now, for tests. */
float chs_x(void);

#ifdef __cplusplus
}
#endif
#endif
