/*
 * capsule_model: the motion and the timeline of the capsule-machine presentation (task C07; docs/CUSTOM-CONTENT-CONTRACTS.md, section 2.13).
 *
 * Pure: no drawing, no ui, no clock, no random numbers, no allocation, no wallet, profile or unlock. It is given an immutable fixture (the
 * results to show: up to 25 items, each a tier and a label) and turns elapsed time into where each capsule is and whether it is open.
 * Nothing it does chooses, changes or grants a result: the motion is a fixed function of the item's position in the fixture, so the same
 * fixture always plays the same way and a result shown is the result supplied.
 *
 * Timeline (ms since the pull was accepted): the knob turns for 200 ms, then the capsules drop from the chute one every 16 ms, bottom row
 * first, each falling under gravity to its own slot and bouncing there a few times with a damped bounce, then each opens for 120 ms. The
 * whole thing is SETTLED when every capsule is open, or at 1500 ms at the latest: at that point whatever is still moving is put at its slot
 * and opened, so the time to the final result is bounded whatever the integration did. The slot of item i is by the item's position in the
 * fixture (reading order, 5 to a row), never by where its capsule landed.
 *
 * Time: dt is in milliseconds (ui.h). cap_step ignores dt <= 0 and NaN, clamps one call to 100 ms (a long pause is not a jump), and
 * integrates in fixed 8 ms steps (at most 16 per call), so the result does not depend on the frame rate.
 */
#ifndef GM_CAPSULE_MODEL_H
#define GM_CAPSULE_MODEL_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CAP_MAX 25          /* the largest gacha pull */
#define CAP_LABEL 32
#define CAP_TIERS 4         /* common, rare, epic, legendary: a colour and a word, nothing else */
#define CAP_COLS 5

#define CAP_STEP_MS 8.0f
#define CAP_MAX_STEPS 16
#define CAP_DT_CLAMP_MS 100.0f
#define CAP_PULL_MS 200.0f
#define CAP_SPAWN_GAP_MS 16.0f
#define CAP_OPEN_MS 120.0f
#define CAP_DEADLINE_MS 1500.0f

/* The field the capsules move in is the top screen's 320 x 180 canvas (ui.h, UI_FIELD). */
#define CAP_FIELD_W 320
#define CAP_FIELD_H 180
#define CAP_RADIUS 10

typedef struct {
  uint8_t tier;               /* 0 .. CAP_TIERS - 1 */
  char label[CAP_LABEL];      /* what the result is called, NUL-terminated, not empty */
} cap_item;

/* The immutable result contract: what to show. The model copies it and never writes to it. */
typedef struct {
  uint8_t n;                  /* 1 .. CAP_MAX */
  cap_item item[CAP_MAX];
} cap_fixture;

typedef enum {
  CAP_IDLE,      /* waiting for the pull */
  CAP_PULL,      /* the knob is turning */
  CAP_DROP,      /* capsules falling, bouncing, opening */
  CAP_SETTLED,   /* every capsule is open: the results are shown */
} cap_stage;

typedef struct {
  float x, y;          /* centre, in the field's pixels */
  float vx, vy;        /* pixels per ms */
  float sx, sy;        /* its slot */
  float spawn_ms;      /* when it leaves the chute */
  float open;          /* 0 closed .. 1 open */
  uint8_t phase;       /* 0 waiting in the chute, 1 falling, 2 landed */
} cap_body;

typedef struct {
  cap_fixture fx;
  cap_stage stage;
  float t;             /* ms since the pull */
  float acc;           /* time not yet integrated */
  float knob;          /* 0 .. 1 of a turn */
  bool skipped;
  bool settled_seen;   /* cap_take_settled has reported the settle */
  cap_body body[CAP_MAX];
} cap_model;

/* True when the fixture is usable: 1..CAP_MAX items, each tier in range and each label NUL-terminated within its buffer and not empty. */
bool cap_fixture_valid(const cap_fixture* f);

/* Starts a model in CAP_IDLE with a copy of the fixture. False (and *m zeroed) for NULL or an invalid fixture. */
bool cap_init(cap_model* m, const cap_fixture* f);

/* The slot of item i of n: where its capsule ends up, in field pixels. */
void cap_slot(int n, int i, float* x, float* y);

/* Accepts the pull: only from CAP_IDLE (true once, false for every later call). */
bool cap_pull(cap_model* m);

/* Advances by dt ms (see the header). Does nothing in CAP_IDLE or CAP_SETTLED. */
void cap_step(cap_model* m, float dt_ms);

/* Jumps to CAP_SETTLED now with every capsule at its slot and open. True when it did (the pull had been accepted and it was not settled
 * yet), false otherwise. */
bool cap_skip(cap_model* m);

/* True exactly once, the first time it is asked after the model became CAP_SETTLED (by time or by cap_skip). */
bool cap_take_settled(cap_model* m);

#ifdef __cplusplus
}
#endif
#endif
