/*
 * field_border: the biome's background widened over the top screen's bands.
 *
 * Upstream's field is 320 x 180 and the 3DS's top screen 400 x 240: a 40 px band at each side and a 30 px one above and below. Nothing is
 * scaled (lorenzo, 2026-10-19: widen the background, never scale the field): the background is mirrored out from each edge of the
 * field's own copy, so it meets it seamlessly. A temporary look until the bands get a design of their own.
 */
#ifndef GM_FIELD_BORDER_H
#define GM_FIELD_BORDER_H

#ifdef __cplusplus
extern "C" {
#endif

/* Draws the bands of the top screen (physical pixels, UI_PHYS_TOP) from the 320 x 180 background image `key` ("images/arenas/<biome>_bg").
 * Without the image the bands are black. Drawn after the field, it also hides what slides past the field's edge. */
void field_border_draw(const char* key);

#ifdef __cplusplus
}
#endif
#endif
