/*
 * achv_screen: the custom Achievements list (game-core's gc/achv.h; upstream's own achievements screen has a different set).
 *
 * The bottom screen lists the achievements, seven at a time, each with its name, its reward and, on the right, "Done" or how far a counting
 * one has got ("3/5"); a finished one is dimmed. UP/DOWN move, LEFT/RIGHT page, a tap on a row points at it, B (or the hint) leaves. The top
 * screen has the highlighted one in full: name, what it asks for, its reward and its state. It only reads the profile.
 */
#ifndef GM_ACHV_SCREEN_H
#define GM_ACHV_SCREEN_H

#include "gc/achv.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  const gc_system* sys;          /* read on every frame, not copied */
  void (*on_exit)(void* arg);    /* B: back to where it came from */
  void* arg;
} achv_args;

/* Makes the list the screen (ui_set, with a fade). */
void achv_screen_open(const achv_args* a);

#ifdef __cplusplus
}
#endif
#endif
