/* The capsule machine's motion and timeline (see capsule_model.h). */
#include "capsule_model.h"

#include <math.h>
#include <string.h>

#define GRAVITY 0.0016f        /* pixels per ms squared */
#define RESTITUTION 0.35f
#define MIN_BOUNCE 0.08f       /* below this speed after a bounce the capsule stays down */
#define CHUTE_Y (-14.0f)
#define CHUTE_X 160.0f
#define SLOT_DX 40.0f
#define SLOT_DY 28.0f
#define FLOOR_Y 164.0f

bool cap_fixture_valid(const cap_fixture* f) {
  if (!f || f->n < 1 || f->n > CAP_MAX) return false;
  for (int i = 0; i < f->n; i++) {
    if (f->item[i].tier >= CAP_TIERS) return false;
    const void* nul = memchr(f->item[i].label, 0, CAP_LABEL);
    if (!nul || nul == f->item[i].label) return false;
  }
  return true;
}

/* Reading order, 5 to a row, the rows centred on the field and the last row centred on itself, resting on the floor. */
void cap_slot(int n, int i, float* x, float* y) {
  const int rows = (n + CAP_COLS - 1) / CAP_COLS;
  const int row = i / CAP_COLS;
  const int in_row = row == rows - 1 ? n - row * CAP_COLS : CAP_COLS;
  const int col = i % CAP_COLS;
  *x = CAP_FIELD_W / 2.0f + ((float)col - ((float)in_row - 1.0f) / 2.0f) * SLOT_DX;
  *y = FLOOR_Y - (float)(rows - 1 - row) * SLOT_DY;
}

bool cap_init(cap_model* m, const cap_fixture* f) {
  if (!m) return false;
  memset(m, 0, sizeof *m);
  if (!cap_fixture_valid(f)) return false;
  m->fx = *f;
  m->stage = CAP_IDLE;
  for (int i = 0; i < f->n; i++) {
    cap_body* b = &m->body[i];
    cap_slot(f->n, i, &b->sx, &b->sy);
    b->x = CHUTE_X + (float)((i * 37) % 61 - 30); /* a fixed spread at the chute, from the position only */
    b->y = CHUTE_Y;
    /* bottom row first: the item at the end of the fixture leaves the chute first */
    b->spawn_ms = CAP_PULL_MS + CAP_SPAWN_GAP_MS * (float)(f->n - 1 - i);
    const float h = b->sy - CHUTE_Y;
    const float fall = sqrtf(2.0f * h / GRAVITY);
    b->vx = (b->sx - b->x) / fall;  /* arrives over its slot with the first touch */
  }
  return true;
}

bool cap_pull(cap_model* m) {
  if (!m || m->stage != CAP_IDLE) return false;
  m->stage = CAP_PULL;
  m->t = 0;
  m->acc = 0;
  return true;
}

static void settle_all(cap_model* m) {
  for (int i = 0; i < m->fx.n; i++) {
    cap_body* b = &m->body[i];
    b->x = b->sx;
    b->y = b->sy;
    b->vx = b->vy = 0;
    b->open = 1;
    b->phase = 2;
  }
  m->knob = 1.0f;
  m->stage = CAP_SETTLED;
}

static void advance(cap_model* m, float dt) {
  m->t += dt;
  if (m->t >= CAP_DEADLINE_MS) {
    settle_all(m);
    return;
  }
  m->knob = m->t >= CAP_PULL_MS ? 1.0f : m->t / CAP_PULL_MS;
  if (m->stage == CAP_PULL && m->t >= CAP_PULL_MS) m->stage = CAP_DROP;
  if (m->stage != CAP_DROP) return;
  bool all_open = true;
  for (int i = 0; i < m->fx.n; i++) {
    cap_body* b = &m->body[i];
    if (b->phase == 0) {
      if (m->t >= b->spawn_ms) b->phase = 1;
      else {
        all_open = false;
        continue;
      }
    }
    if (b->phase == 1) {
      b->vy += GRAVITY * dt;
      b->y += b->vy * dt;
      b->x += b->vx * dt;
      if ((b->vx > 0 && b->x > b->sx) || (b->vx < 0 && b->x < b->sx)) b->x = b->sx;
      if (b->y >= b->sy) {
        b->y = b->sy;
        b->x = b->sx;
        b->vx = 0;
        b->vy = -b->vy * RESTITUTION;
        if (-b->vy < MIN_BOUNCE) {
          b->vy = 0;
          b->phase = 2;
        }
      }
    }
    if (b->phase == 2 && b->open < 1.0f) {
      b->open += dt / CAP_OPEN_MS;
      if (b->open > 1.0f) b->open = 1.0f;
    }
    if (!(b->phase == 2 && b->open >= 1.0f)) all_open = false;
  }
  if (all_open) m->stage = CAP_SETTLED;
}

void cap_step(cap_model* m, float dt_ms) {
  if (!m || (m->stage != CAP_PULL && m->stage != CAP_DROP)) return;
  if (!(dt_ms > 0)) return;  /* zero, negative and NaN */
  if (dt_ms > CAP_DT_CLAMP_MS) dt_ms = CAP_DT_CLAMP_MS;
  m->acc += dt_ms;
  for (int steps = 0; steps < CAP_MAX_STEPS && m->acc >= CAP_STEP_MS && (m->stage == CAP_PULL || m->stage == CAP_DROP); steps++) {
    m->acc -= CAP_STEP_MS;
    advance(m, CAP_STEP_MS);
  }
}

bool cap_skip(cap_model* m) {
  if (!m || (m->stage != CAP_PULL && m->stage != CAP_DROP)) return false;
  m->skipped = true;
  settle_all(m);
  return true;
}

bool cap_take_settled(cap_model* m) {
  if (!m || m->stage != CAP_SETTLED || m->settled_seen) return false;
  m->settled_seen = true;
  return true;
}
