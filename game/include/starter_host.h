/*
 * starter_host: puts the starter select screen (menus/starter.c) on the UI stack and feeds it from the system data
 * (starter_data.h). The screen draws on both 3DS screens as upstream's 320 x 180 canvas: the species, the team and the cost on the
 * bottom one, the species large with its moves on the top one.
 *
 * Like the other menus hosted in the app (reward_host.h) the screen reads no game state itself; this is where the system data
 * and game-core's rules meet it.
 */
#ifndef GM_STARTER_HOST_H
#define GM_STARTER_HOST_H

#include <stdbool.h>

#include "gc/run.h"
#include "gc/system.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  /* "Begin with these Pokemon?" answered Yes, with a team game-core accepts (checked against the system data and the cost budget).
   * The screen stays until the caller replaces it with another one. */
  void (*on_start)(const gc_starter* team, int n, void* arg);
  /* "Do you want to exit?" answered Yes. */
  void (*on_exit)(void* arg);
  void* arg;
  bool (*save_profile)(void* arg); /* NULL: sysd_save; injectable for host tests */
} sh_events;

/* Shows the screen (the UI fades to it, replacing the one on top) with the species `sys` has caught. `notice` (may be NULL) is a
 * message in the box that waits for A. False when there is nothing to choose from; nothing is then shown. */
bool sh_open(gc_system* sys, const char* notice, const sh_events* ev);

#ifdef __cplusplus
}
#endif
#endif
