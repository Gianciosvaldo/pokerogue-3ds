/* achv_toast_test: the app's achievement bar (source/achv_toast.c, achv_toast_text.c): the queue, upstream's AchvBar timing, and the words and art
 * of a custom achievement's bar, over game-core's real table and a fake drawing layer.
 *
 *   make achv_toast_test && build/achv_toast_test
 *
 * Checks: a bar slides in over 500 ms (Sine.easeOut), stays until 10 s after it began, slides out over 500 ms (Sine.easeIn), with the achievement
 * sound as each begins; bars wait their turn; a "brisk" bar (the custom achievements') leaves after 3 s when others wait and keeps its 10 s when
 * none does, and one that has already stayed longer leaves from where it is when another arrives; the bar draws only on the top screen, only
 * while one is up; the strings are copied; a full queue drops the bar; a reset clears it; and every one of the table's achievements has a bar
 * with its name, what it asks, its reward, an icon and a tier from 0 to 4. */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "achv_toast.h"
#include "gc/achv.h"
#include "ui.h"

static int checks, failed;
#define CHECK(cond, ...)                                  \
  do {                                                    \
    checks++;                                             \
    if (!(cond)) {                                        \
      failed++;                                           \
      if (failed < 40) {                                  \
        printf("FAIL line %d: %s: ", __LINE__, #cond);    \
        printf(__VA_ARGS__);                              \
        printf("\n");                                     \
      }                                                   \
    }                                                     \
  } while (0)

/* ------------------------------------------------------------------ fakes */

static bool g_top = true;
bool ui_space(ui_space_id sp) {
  (void)sp;
  return g_top;
}

static char g_sounds[128][24];
static int n_sounds;
void ui_play_sound(const char* key) {
  if (n_sounds < 128) snprintf(g_sounds[n_sounds], sizeof g_sounds[0], "%s", key ? key : "(null)");
  n_sounds++;
}

static int n_draws;
static char d_title[96], d_desc[192], d_score[24], d_icon[40];
static int d_tier;
static float d_slide;
void tv_draw(const tv_info* t, float slide) {
  n_draws++;
  snprintf(d_title, sizeof d_title, "%s", t->title);
  snprintf(d_desc, sizeof d_desc, "%s", t->desc);
  snprintf(d_score, sizeof d_score, "%s", t->score);
  snprintf(d_icon, sizeof d_icon, "%s", t->icon);
  d_tier = t->tier;
  d_slide = slide;
}

/* ------------------------------------------------------------------ helpers */

static tv_info info(const char* title, int tier) {
  tv_info t = {title, "a description", "+50pt", "eviolite", tier};
  return t;
}

static bool drawn(void) {
  n_draws = 0;
  atoast_draw();
  return n_draws == 1;
}

static void run(float ms) {
  for (float t = 0; t < ms - 0.01f; t += 10) atoast_update(10);
}

static bool near(float a, float b) { return fabsf(a - b) < 1e-4f; }

static const float HALF_PI = 1.5707963f;

int main(void) {
  /* ---- one bar: in, in place, out, gone; the sound as it begins */
  atoast_reset();
  CHECK(!atoast_active() && !drawn(), "nothing is up at first");
  {
    const tv_info t = info("Bar", 2);
    atoast_push(&t, false);
  }
  CHECK(n_sounds == 1 && !strcmp(g_sounds[0], "se/achv"), "the achievement sound begins with the bar (%d)", n_sounds);
  CHECK(atoast_active() && drawn() && !strcmp(d_title, "Bar") && near(d_slide, 0.0f), "it is up, past the edge (%.3f)", d_slide);
  CHECK(!strcmp(d_desc, "a description") && !strcmp(d_score, "+50pt") && !strcmp(d_icon, "eviolite") && d_tier == 2, "with its words and art");
  run(250);
  drawn();
  CHECK(near(d_slide, sinf(0.5f * HALF_PI)), "250 ms in: Sine.easeOut (%.4f)", d_slide);
  run(250);
  drawn();
  CHECK(near(d_slide, 1.0f), "500 ms in: in place (%.4f)", d_slide);
  run(9000);
  drawn();
  CHECK(near(d_slide, 1.0f), "9.5 s: still in place");
  run(500);
  drawn();
  CHECK(near(d_slide, 1.0f), "10 s: it starts to leave");
  run(250);
  drawn();
  CHECK(near(d_slide, cosf(0.5f * HALF_PI)), "250 ms out: Sine.easeIn (%.4f)", d_slide);
  CHECK(atoast_active(), "still up");
  run(250);
  CHECK(!atoast_active() && !drawn() && n_sounds == 1, "10.5 s: gone, and no second sound");

  /* ---- two bars wait their turn */
  atoast_reset();
  n_sounds = 0;
  {
    const tv_info a = info("A", 0), b = info("B", 4);
    atoast_push(&a, false);
    atoast_push(&b, false);
  }
  CHECK(n_sounds == 1, "only the first begins (%d)", n_sounds);
  run(10250);
  drawn();
  CHECK(!strcmp(d_title, "A") && near(d_slide, cosf(0.5f * HALF_PI)), "A is leaving at 10.25 s");
  run(250);
  CHECK(n_sounds == 2 && !strcmp(g_sounds[1], "se/achv"), "B begins at 10.5 s with its own sound (%d)", n_sounds);
  drawn();
  CHECK(!strcmp(d_title, "B") && d_tier == 4 && near(d_slide, 0.0f), "B starts past the edge");
  run(500);
  drawn();
  CHECK(near(d_slide, 1.0f), "and comes in");
  run(10000);
  CHECK(!atoast_active() && n_sounds == 2, "after its 10.5 s the queue is empty");

  /* ---- brisk: 3 s when another waits, 10 s alone */
  atoast_reset();
  n_sounds = 0;
  {
    const tv_info a = info("A", 0), b = info("B", 0);
    atoast_push(&a, true);
    atoast_push(&b, true);
  }
  run(3000);
  drawn();
  CHECK(!strcmp(d_title, "A") && near(d_slide, 1.0f), "a brisk bar with one waiting is in place until 3 s");
  run(250);
  drawn();
  CHECK(near(d_slide, cosf(0.5f * HALF_PI)), "then slides out (%.4f)", d_slide);
  run(250);
  drawn();
  CHECK(n_sounds == 2 && !strcmp(d_title, "B") && near(d_slide, 0.0f), "and the next begins at 3.5 s");
  run(500);
  run(9500);
  drawn();
  CHECK(near(d_slide, 1.0f), "the last brisk one has nobody waiting: still in place at 10 s");
  run(500);
  CHECK(!atoast_active(), "and gone at 10.5 s");

  /* ---- a brisk bar that has stayed for 5 s when another arrives leaves from where it is (no jump) */
  atoast_reset();
  n_sounds = 0;
  {
    const tv_info a = info("A", 0), b = info("B", 0);
    atoast_push(&a, true);
    run(5000);
    drawn();
    CHECK(near(d_slide, 1.0f), "alone at 5 s: in place");
    atoast_push(&b, true);
  }
  run(10);
  drawn();
  CHECK(d_slide < 1.0f && d_slide > 0.99f && !strcmp(d_title, "A"), "it starts to leave from there (%.4f)", d_slide);
  run(240);
  drawn();
  CHECK(near(d_slide, cosf(0.5f * HALF_PI)), "250 ms after the arrival: halfway out (%.4f)", d_slide);
  run(250);
  drawn();
  CHECK(!strcmp(d_title, "B") && n_sounds == 2, "and B follows");

  /* ---- a bar that is not brisk keeps its 10 s even with others waiting (the win's bars are upstream's) */
  atoast_reset();
  {
    const tv_info a = info("A", 0), b = info("B", 0);
    atoast_push(&a, false);
    atoast_push(&b, true);
  }
  run(9500);
  drawn();
  CHECK(!strcmp(d_title, "A") && near(d_slide, 1.0f), "a bar of the win stays 10 s with a brisk one behind it");

  /* ---- only the top screen, only while one is up */
  atoast_reset();
  {
    const tv_info a = info("A", 0);
    atoast_push(&a, false);
  }
  g_top = false;
  CHECK(!drawn(), "nothing on the bottom screen");
  g_top = true;
  CHECK(drawn(), "the top screen has it");

  /* ---- the strings are copied */
  atoast_reset();
  {
    char title[32];
    snprintf(title, sizeof title, "Copied");
    const tv_info a = {title, "d", "s", "i", 1};
    atoast_push(&a, false);
    memset(title, 'x', sizeof title - 1);
  }
  drawn();
  CHECK(!strcmp(d_title, "Copied"), "the title was copied (%s)", d_title);
  {
    char big[400];
    memset(big, 'y', sizeof big - 1);
    big[sizeof big - 1] = 0;
    atoast_reset();
    const tv_info a = {big, big, big, big, 99};
    atoast_push(&a, false);
    drawn();
    CHECK(strlen(d_title) < 96 && strlen(d_desc) < 192 && d_tier == 4, "long strings are cut and a tier past 4 is 4 (%zu, %zu, %d)", strlen(d_title), strlen(d_desc), d_tier);
  }

  /* ---- a full queue drops the bar; a reset clears it */
  atoast_reset();
  n_sounds = 0;
  for (int i = 0; i < ATOAST_QUEUE + 5; i++) {
    const tv_info a = info("Q", 0);
    atoast_push(&a, true);
  }
  run(60000);
  for (int i = 0; i < 1000 && atoast_active(); i++) run(1000);
  CHECK(!atoast_active() && n_sounds == ATOAST_QUEUE, "%d fit, the rest are dropped (%d sounds)", ATOAST_QUEUE, n_sounds);
  {
    const tv_info a = info("A", 0);
    atoast_push(&a, false);
  }
  atoast_reset();
  CHECK(!atoast_active() && !drawn(), "a reset clears the bar");
  {
    const tv_info a = info("A", 0);
    n_sounds = 0;
    atoast_push(&a, false);
  }
  CHECK(n_sounds == 1 && atoast_active(), "and a new one begins as the first");

  /* ---- the bar of a custom achievement */
  atoast_reset();
  n_sounds = 0;
  CHECK(!atoast_text_of(-1, &(atoast_text){0}) && !atoast_text_of(GC_ACHV_COUNT, &(atoast_text){0}), "no bar for an id outside the table");
  atoast_push_achv(-1);
  atoast_push_achv(GC_ACHV_COUNT);
  CHECK(!atoast_active() && n_sounds == 0, "and none is pushed");
  atoast_push_achv(GC_JOJO_DIO);
  CHECK(atoast_active() && n_sounds == 1, "DIO's bar begins with the sound");
  drawn();
  CHECK(!strcmp(d_title, "DIO") && !strcmp(d_desc, "Defeat DIO.\nReward: 1 Egg Voucher") && !strcmp(d_icon, "coupon") && d_tier == 0 && d_score[0] == 0,
        "named, with what it asks and its reward: %s / %s / %s / %d", d_title, d_desc, d_icon, d_tier);
  atoast_push_achv(29);   /* Untouched: one Plus voucher */
  atoast_push_achv(32);   /* Ten Victories: one Premium voucher */
  run(3500);
  drawn();
  CHECK(!strcmp(d_title, "Untouched") && !strcmp(d_icon, "pair_of_tickets") && d_tier == 1 && strstr(d_desc, "Reward: 1 Egg Voucher Plus"),
        "the next is Untouched with the Plus ticket (%s, %s, %d)", d_title, d_icon, d_tier);
  run(3500);
  drawn();
  CHECK(!strcmp(d_title, "Ten Victories") && !strcmp(d_icon, "mystic_ticket") && d_tier == 2, "then Ten Victories with the mystic ticket (%s, %s, %d)", d_title, d_icon, d_tier);

  /* every achievement: a name, what it asks, a reward line, an icon and a tier */
  int golden = 0, lucky = 0, tickets[4] = {0}, shiny = 0;
  for (int id = 0; id < GC_ACHV_COUNT; id++) {
    atoast_text t;
    CHECK(atoast_text_of(id, &t), "id %d has a bar", id);
    const gc_achv_def* d = &gc_achv_defs[id];
    char want[256];
    snprintf(want, sizeof want, "%s\nReward: %s", d->desc, gc_achv_reward_name(d));
    CHECK(!strcmp(t.title, d->name) && !strcmp(t.desc, want), "id %d: its words (%s / %s)", id, t.title, t.desc);
    CHECK(t.icon && t.icon[0] && t.tier >= 0 && t.tier <= 4 && t.score[0] == 0, "id %d: an icon and a tier (%d)", id, t.tier);
    if (d->reward == GC_AREW_VOUCHER) {
      CHECK(t.tier == d->arg, "id %d: a voucher's tier is its type (%d, %d)", id, t.tier, d->arg);
      tickets[t.tier]++;
    } else if (d->reward == GC_AREW_EGG) {
      CHECK(t.tier == d->arg + 1, "id %d: an egg's tier is its own plus one", id);
      if (!strcmp(t.icon, "golden_egg")) golden++; else lucky++;
    } else {
      CHECK(t.tier == 4 && !strcmp(t.icon, "golden_egg"), "id %d: a guaranteed shiny is the top tier", id);
      shiny++;
    }
  }
  CHECK(tickets[0] > 0 && tickets[1] > 0 && tickets[2] > 0 && lucky > 0 && golden > 0 && shiny > 0, "the table uses every kind of art (%d %d %d %d, %d, %d, %d)", tickets[0], tickets[1], tickets[2], tickets[3], lucky, golden, shiny);

  printf("achv_toast_test: %d checks, %d failed\n", checks, failed);
  return failed ? 1 : 0;
}
