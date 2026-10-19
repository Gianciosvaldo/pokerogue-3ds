/*
 * dialogue_view: the name box of a dialogue (upstream's BattleMessageUiHandler.showNameText): the small window with the speaker's name that
 * sits on the top edge of the message window while someone talks. namebox is upstream's art (images/ui/namebox, a frame per window style),
 * a 9-slice with 8 / 8 / 5 / 5 px borders, 16 px tall and as wide as the name plus 16.
 */
#ifndef GM_DIALOGUE_VIEW_H
#define GM_DIALOGUE_VIEW_H

#ifdef __cplusplus
extern "C" {
#endif

/* Draws the name box with its top-left at (x, y) of the bottom screen (320 x 240); `window_style` is upstream's uiWindowStyle (1..5). */
void dv_namebox(float x, float y, const char* name, int window_style);

#ifdef __cplusplus
}
#endif
#endif
