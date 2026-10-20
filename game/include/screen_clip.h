/*
 * screen_clip: the GPU scissor for the game framework's screens.
 *
 * The 3DS draws into a framebuffer that is the screen turned a quarter; a quad that sticks out past the bottom edge shows up as a
 * line at the top (seen in Azahar with the save slot list, whose next slot starts below the screen). The menus layer already has a
 * scissor (menus/mui.c's mui_clip); this is a door to it for files that can't include menus.h (it clashes with ui_text.h).
 */
#ifndef GM_SCREEN_CLIP_H
#define GM_SCREEN_CLIP_H

#ifdef __cplusplus
extern "C" {
#endif

/* Nothing is drawn outside the physical screen being drawn, from here until clip_off. Call it in a draw callback. */
void clip_to_screen(void);
/* Nothing is drawn outside (x, y, w, h) of the physical screen being drawn, until clip_off. */
void clip_rect(float x, float y, float w, float h);
void clip_off(void);

#ifdef __cplusplus
}
#endif
#endif
