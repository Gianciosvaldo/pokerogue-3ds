/* achv_hooks_test: what the run driver tells the achievements (source/achv_hooks.c) over a bare game-core run and real system data.
 *
 *   make achv_hooks_test && build/achv_hooks_test
 *
 * Checks: a challenger beaten (the reward screen after its battle) is reported once and unlocks its achievement with its messages; a
 * wave with no challenger, a defeat, a second look at the same reward screen, and a run with no system data report nothing; a win with a
 * fainted Pokemon is not "clean", one without is; the run's seed tells the runs apart for "three in one run"; ach_reset forgets a battle. */
#include <stdio.h>
#include <string.h>

#include "achv_hooks.h"
#include "gc/achv.h"

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

static gc_run R;
static gc_system S;

static void new_run(int wave, int seed_char)
{
  memset(&R, 0, sizeof R);
  R.sys = &S;
  R.wave = wave;
  R.n_party = 3;
  for (int i = 0; i < 3; i++) R.party[i].b.hp = 50;
  R.run.seed.len = 4;
  for (int i = 0; i < 4; i++) R.run.seed.u[i] = (uint16_t)(seed_char + i);
}

/* a battle against `jojo` that ends in `end` (a screen kind); returns what ach_on_screen said at the end */
static bool battle(int jojo, int end, int faint_during)
{
  ach_on_screen(&R, GC_SCREEN_COMMAND, jojo);
  for (int i = 0; i < faint_during; i++) R.party[i].b.hp = 0;
  return ach_on_screen(&R, end, jojo);
}

static int drain(void)
{
  int n = 0;
  while (ach_take_message()) n++;
  return n;
}

int main(void)
{
  gc_system_init_defaults(&S);
  ach_reset();

  /* a challenger beaten: reported, unlocked, told in two lines */
  new_run(10, 'a');
  CHECK(battle(GC_JOJO_DIO, GC_SCREEN_REWARD, 0), "the win is reported");
  CHECK(gc_achv_unlocked(&S, GC_JOJO_DIO), "DIO is unlocked");
  CHECK(gc_achv_unlocked(&S, 29), "and so is Untouched (nobody fainted)");
  const char* m = ach_take_message();
  CHECK(m && strstr(m, "Achievement unlocked: DIO!") && strstr(m, "You received 1 Egg Voucher!"), "the first message names DIO and its reward: %s", m ? m : "(none)");
  m = ach_take_message();
  CHECK(m && strstr(m, "Untouched"), "the second names Untouched");
  CHECK(ach_take_message() == NULL, "nothing more");
  const uint32_t battles = S.achv.jojo_battles;
  CHECK(!ach_on_screen(&R, GC_SCREEN_REWARD, GC_JOJO_DIO) && S.achv.jojo_battles == battles, "the reward screen seen again counts nothing");

  /* no challenger, a defeat, no system data */
  new_run(11, 'a');
  CHECK(!battle(-1, GC_SCREEN_REWARD, 0), "an ordinary wave reports nothing");
  new_run(12, 'a');
  CHECK(!battle(GC_JOJO_KARS, GC_SCREEN_GAME_OVER, 3), "a defeat reports nothing");
  CHECK(!gc_achv_unlocked(&S, GC_JOJO_KARS), "and unlocks nothing");
  new_run(13, 'a');
  R.sys = NULL;
  CHECK(!battle(GC_JOJO_KARS, GC_SCREEN_REWARD, 0), "a run with no system data reports nothing");

  /* clean or not */
  gc_system_init_defaults(&S);
  new_run(20, 'b');
  CHECK(battle(GC_JOJO_GYRO, GC_SCREEN_REWARD, 1), "a win with a faint is reported");
  CHECK(S.achv.jojo_clean == 0 && !gc_achv_unlocked(&S, 29), "but it is not clean");
  drain();
  new_run(21, 'b');
  for (int i = 0; i < 3; i++) R.party[i].b.hp = 50;
  CHECK(battle(GC_JOJO_POLNAREFF, GC_SCREEN_CLEAR, 0), "the end of the run after the last wave counts as a win too");
  CHECK(S.achv.jojo_clean == 1 && gc_achv_unlocked(&S, 29), "and it was clean");
  drain();

  /* a faint that was already there when the battle began is not this battle's */
  gc_system_init_defaults(&S);
  new_run(30, 'c');
  R.party[2].b.hp = 0;
  CHECK(battle(GC_JOJO_JOSEPH, GC_SCREEN_REWARD, 0) && S.achv.jojo_clean == 1, "a Pokemon fainted before the battle does not spoil it");
  drain();

  /* three in one run, and a second run starts the count again */
  gc_system_init_defaults(&S);
  new_run(40, 'd');
  battle(GC_JOJO_KARS, GC_SCREEN_REWARD, 0);
  R.wave = 41;
  battle(GC_JOJO_GYRO, GC_SCREEN_REWARD, 0);
  drain();
  CHECK(!gc_achv_unlocked(&S, 27), "two in a run are not three");
  new_run(42, 'e');
  battle(GC_JOJO_DIAVOLO, GC_SCREEN_REWARD, 0);
  drain();
  CHECK(!gc_achv_unlocked(&S, 27) && S.achv.best_run_jojo == 2, "another run counts from one");
  R.wave = 43;
  battle(GC_JOJO_KIRA, GC_SCREEN_REWARD, 0);
  R.wave = 44;
  battle(GC_JOJO_PUCCI, GC_SCREEN_REWARD, 0);
  CHECK(gc_achv_unlocked(&S, 27), "three in that run");
  CHECK(drain() >= 1, "and it is told");

  /* ach_reset forgets the battle: its end is not a win any more */
  new_run(50, 'f');
  ach_on_screen(&R, GC_SCREEN_COMMAND, GC_JOJO_MISTA);
  ach_reset();
  CHECK(!ach_on_screen(&R, GC_SCREEN_REWARD, GC_JOJO_MISTA) && !gc_achv_unlocked(&S, GC_JOJO_MISTA), "after a reset the old battle is not counted");

  printf("achv_hooks_test: %d checks, %d failed\n", checks, failed);
  return failed ? 1 : 0;
}
