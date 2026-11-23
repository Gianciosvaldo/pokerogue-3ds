/*
 * challenge_select: Challenge Mode's challenge screen (upstream's ChallengesSelectUiHandler, SelectChallengePhase), shown after the slot list and
 * before the starter select. The rows are the challenges game-core plays so far (gc_challenge_played_mask: only Fresh Start for now; each
 * later challenge's PR adds its row), each with its values; Left and Right change the value of the row under the cursor (no wrap, as upstream's
 * increaseValue / decreaseValue stop at the ends), Up and Down move over the rows (wrapping). A starts, as upstream's does, once one challenge
 * has a value above 0 (until then the Start label reads "None Selected" and A is an error); B goes back. A tap on a row steps its value
 * (back to 0 after the last), a tap on Start starts.
 *
 * The model (chm_*) is plain C for the host test; the screen (chsel_open) draws it on the bottom screen.
 */
#ifndef GM_CHALLENGE_SELECT_H
#define GM_CHALLENGE_SELECT_H

#include <stdbool.h>

#include "gc/run.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  gc_challenge_set v;            /* the values */
  int rows[GC_CHALLENGE_COUNT];  /* the challenge ids shown, in upstream's order */
  int n;                         /* how many */
  int cursor;                    /* 0..n-1: the challenge under the cursor */
} chm_model;

/* The rows for the challenges game-core plays, starting from `initial` (NULL: all off). Values game-core would refuse are reset to 0. */
void chm_init(chm_model* m, const gc_challenge_set* initial);
/* Up / Down: one row, wrapping */
void chm_move(chm_model* m, int dir);
/* Left / Right on a challenge's row: its value one step (no wrap). True when it changed. */
bool chm_change(chm_model* m, int dir);
/* A tap on a row: its value one step up, back to 0 after the last */
void chm_step(chm_model* m);
/* at least one challenge has a value */
bool chm_any(const chm_model* m);
/* A does something: the run can start with these values */
bool chm_can_start(const chm_model* m);
/* the challenge under the cursor, -1 when there are no rows */
int chm_current(const chm_model* m);
/* the locale key base of a challenge ("freshStart": challenges:<base>.name / .value.N / .desc.N) */
const char* chm_key(int challenge);

typedef struct {
  gc_challenge_set values;  /* what the screen starts with (the last choice when it comes back from the starter select) */
  void (*on_start)(const gc_challenge_set* set, void* arg);
  void (*on_back)(void* arg);
  void* arg;
} chsel_args;

void chsel_open(const chsel_args* a);

#ifdef __cplusplus
}
#endif
#endif
