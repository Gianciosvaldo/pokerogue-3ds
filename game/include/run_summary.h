/*
 * run_summary: draws what a run looks like (run_saves.h's rsv_info): mode and wave, biome, score, money and the party with
 * levels. The save slot list's top screen and the game over / clear screens show it the same way.
 */
#ifndef GM_RUN_SUMMARY_H
#define GM_RUN_SUMMARY_H

#include "run_saves.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Height of what rsum_draw draws. */
#define RSUM_HEIGHT 128

/* Draws the summary with its top left corner at (x, y) in the current drawing space: four lines of text 18 px apart, then
 * the party in a row (icons 48 px apart, a level under each). Draws nothing for a slot that is not RSV_OK. */
void rsum_draw(const rsv_info* in, float x, float y);

#ifdef __cplusplus
}
#endif
#endif
