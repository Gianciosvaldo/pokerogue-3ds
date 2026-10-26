/*
 * achv_hooks: what the run driver tells the custom achievements (game-core's gc/achv.h) while a Custom run is played.
 *
 * run_screen.c calls ach_on_screen with every screen the run reaches. On the first COMMAND screen of a wave the hook notes the battle (which
 * JoJo challenger is on the field, if any, and how many of the party have fainted); the first screen after it that is not a COMMAND is the
 * battle's end. When the battle was a challenger's and the party is still alive, the hook reports the win to the profile, lets game-core
 * unlock what is met (gc_achv_evaluate gives the rewards) and queues one message for each: "Achievement unlocked: <name>!" and "You received
 * <reward>!". The driver says them one at a time (ach_take_message) before it handles the screen, and writes the profile when ach_on_screen
 * returned true.
 *
 * Group B (the runs) uses the same calls in every run: the waves reached, and the end of the run (a won run's feats: six Pokemon that can
 * still evolve, only one Pokemon ever on the field at a command screen, no Poke Ball thrown). Only a Custom run has JoJo challengers. Approximations, said here: a Pokemon revived mid-battle and one that
 * fainted in its place hide each other (the count compares fainted Pokemon at the start and at the end), and a run loaded in the middle of
 * a challenger's battle counts it from its first COMMAND screen again, which is the save point's moment, so a win is never counted twice.
 */
#ifndef GM_ACHV_HOOKS_H
#define GM_ACHV_HOOKS_H

#include <stdbool.h>

#include "gc/run.h"

#ifdef __cplusplus
extern "C" {
#endif

/* `jojo` is the challenger on the field (an index of custom-core's table, GC_JOJO_*), or -1. Returns true when the profile changed and must be
 * written. Does nothing when the run has no system data. */
bool ach_on_screen(const gc_run* run, int kind, int jojo, bool custom);

/* A Poke Ball was thrown (a GC_CH_BALL accepted by game-core). */
void ach_on_ball(void);

/* Like ach_reset, for a run resumed from a save: what it did before the save is unknown, so it cannot earn the achievements that need the
 * whole run (Lone Hero, Ball-Free). */
void ach_reset_loaded(void);

/* The next queued message, or NULL. The text stays valid until the next call. */
const char* ach_take_message(void);

/* Forgets the battle being tracked and the queued messages (a new run starts or loads). */
void ach_reset(void);

#ifdef __cplusplus
}
#endif
#endif
