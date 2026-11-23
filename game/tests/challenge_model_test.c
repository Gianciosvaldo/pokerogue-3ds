/* challenge_model_test: the challenge screen's model (source/challenge_model.c), button by button, over real game-core (which challenges
 * are played, their maxima). Exit 1 on a failed check. */
#include <stdio.h>
#include <string.h>

#include "challenge_select.h"
#include "pr_enums.h"

static int checks, failures;
#define CHECK(c, ...) do { checks++; if (!(c)) { failures++; printf("FAIL: "); printf(__VA_ARGS__); printf("\n"); } } while (0)

int main(void) {
  /* the rows are the played challenges that have a maximum above 0: Fresh Start only, for now */
  chm_model m;
  chm_init(&m, NULL);
  const uint32_t played = gc_challenge_played_mask();
  int expect = 0;
  for (int id = 0; id < GC_CHALLENGE_COUNT; id++)
    if ((played >> id) & 1u && gc_challenge_max(id) > 0) expect++;
  CHECK(m.n == expect && m.n >= 1, "a row for each played challenge with values (%d of %d)", m.n, expect);
  CHECK(m.rows[0] == CHALLENGES_FRESH_START || m.n > 1, "Fresh Start is a row");
  CHECK(!chm_any(&m) && !chm_can_start(&m), "nothing is chosen at first: A is an error (None Selected)");
  CHECK(chm_current(&m) == m.rows[0], "the cursor is on the first row");
  CHECK(!strcmp(chm_key(CHALLENGES_FRESH_START), "freshStart") && !strcmp(chm_key(CHALLENGES_MOVESET_RANDOMIZER), "movesetRandomizer") &&
        !strcmp(chm_key(CHALLENGES_LOWER_MAX_STARTER_COST), ""), "locale keys: upstream's ids, none for the two that are not in allChallenges");

  /* Left / Right: one step, no wrap */
  const int fs = CHALLENGES_FRESH_START;
  while (chm_current(&m) != fs) chm_move(&m, 1);
  CHECK(!chm_change(&m, -1) && m.v.value[fs] == 0, "Left at 0 does nothing");
  CHECK(chm_change(&m, 1) && m.v.value[fs] == 1 && chm_any(&m) && chm_can_start(&m), "Right: Fresh Start on, A starts");
  const int max = gc_challenge_max(fs);
  for (int i = 0; i < 5; i++) chm_change(&m, 1);
  CHECK(m.v.value[fs] == max && !chm_change(&m, 1), "Right stops at the maximum (%d)", max);
  CHECK(chm_change(&m, -1) && m.v.value[fs] == max - 1, "Left steps back");

  /* a tap steps and wraps to 0 after the last */
  m.v.value[fs] = (uint8_t)max;
  chm_step(&m);
  CHECK(m.v.value[fs] == 0 && !chm_any(&m), "a tap after the last value is off again");
  chm_step(&m);
  CHECK(m.v.value[fs] == 1, "a tap turns it on");

  /* Up / Down wrap over the rows */
  const int first = chm_current(&m);
  for (int i = 0; i < m.n; i++) chm_move(&m, 1);
  CHECK(chm_current(&m) == first, "Down over every row is back where it was");
  chm_move(&m, -1);
  CHECK(chm_current(&m) == m.rows[m.n - 1], "Up from the first row is the last");

  /* prefilled values: those game-core plays are kept, what it would refuse (above the max, a challenge not played) is reset */
  gc_challenge_set in = {{0}};
  in.value[fs] = 1;
  in.value[CHALLENGES_MOVESET_RANDOMIZER] = 1;
  in.value[CHALLENGES_SINGLE_TYPE] = 200;
  chm_init(&m, &in);
  CHECK(m.v.value[fs] == 1, "a prefilled Fresh Start is kept");
  for (int id = 0; id < GC_CHALLENGE_COUNT; id++)
    if (id != fs && !((played >> id) & 1u)) CHECK(m.v.value[id] == 0, "challenge %d, which is not played, is reset", id);
  in.value[fs] = 9;
  chm_init(&m, &in);
  CHECK(m.v.value[fs] == 0, "a value above the maximum is reset");
  CHECK(gc_challenge_set_valid(&m.v, NULL), "what the model hands out is a valid set");

  printf("%d checks, %d failed\n", checks, failures);
  if (failures) return 1;
  puts("OK");
  return 0;
}
