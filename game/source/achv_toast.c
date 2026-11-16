/* The achievement / voucher bar as one queue for the whole app (see achv_toast.h). The words of the custom achievements are achv_toast_text.c's. */
#include "achv_toast.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "ui.h"

typedef struct {
  char title[96];
  char desc[192];
  char score[24];
  char icon[40];
  int tier;
  bool brisk;
} atoast_item;

static struct {
  atoast_item q[ATOAST_QUEUE];
  int head, n;      /* the queue is a ring: q[head] is the bar that is up when `active` */
  bool active;
  float ms;         /* how long the bar that is up has been up */
  float leave_at;   /* when it starts to leave */
} T;

static void start(void) {
  T.active = true;
  T.ms = 0.0f;
  T.leave_at = ATOAST_OUT_AT_MS;
  ui_play_sound("se/achv");
}

void atoast_reset(void) { memset(&T, 0, sizeof T); }

bool atoast_active(void) { return T.active || T.n > 0; }

void atoast_push(const tv_info* t, bool brisk) {
  if (!t || T.n >= ATOAST_QUEUE) return;
  atoast_item* it = &T.q[(T.head + T.n) % ATOAST_QUEUE];
  memset(it, 0, sizeof *it);
  snprintf(it->title, sizeof it->title, "%s", t->title ? t->title : "");
  snprintf(it->desc, sizeof it->desc, "%s", t->desc ? t->desc : "");
  snprintf(it->score, sizeof it->score, "%s", t->score ? t->score : "");
  snprintf(it->icon, sizeof it->icon, "%s", t->icon ? t->icon : "");
  it->tier = t->tier < 0 ? 0 : t->tier > 4 ? 4 : t->tier;
  it->brisk = brisk;
  T.n++;
  if (!T.active) start();
}

void atoast_update(float dt) {
  if (!T.active) return;
  const atoast_item* it = &T.q[T.head];
  const float before = T.ms;
  T.ms += dt;
  /* a brisk bar with others waiting does not stay for the full 10 s: it leaves at 3 s, or now when that has passed (from where it is) */
  if (it->brisk && T.n > 1 && T.leave_at > ATOAST_BRISK_OUT_AT_MS && before < T.leave_at)
    T.leave_at = before > ATOAST_BRISK_OUT_AT_MS ? before : ATOAST_BRISK_OUT_AT_MS;
  if (T.ms >= T.leave_at + ATOAST_OUT_MS) {
    T.head = (T.head + 1) % ATOAST_QUEUE;
    T.n--;
    T.active = false;
    if (T.n > 0) start();
  }
}

/* Sine.easeOut in, Sine.easeIn out */
static float slide_of(void) {
  if (T.ms < ATOAST_IN_MS) return sinf(T.ms / ATOAST_IN_MS * 1.5707963f);
  if (T.ms >= T.leave_at) {
    const float s = cosf((T.ms - T.leave_at) / ATOAST_OUT_MS * 1.5707963f); /* 1 - Sine.easeIn */
    return s < 0.0f ? 0.0f : s;
  }
  return 1.0f;
}

void atoast_draw(void) {
  if (!T.active || !ui_space(UI_FIELD)) return;
  const atoast_item* it = &T.q[T.head];
  const tv_info info = {it->title, it->desc, it->score, it->icon, it->tier};
  tv_draw(&info, slide_of());
}
