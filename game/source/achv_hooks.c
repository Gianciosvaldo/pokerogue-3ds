/* What the run driver tells the custom achievements (see achv_hooks.h). */
#include "achv_hooks.h"

#include <stdio.h>
#include <string.h>

#include "gc/achv.h"

#define QUEUE 8
#define MSG_LEN 192

static struct {
  bool tracking;
  int wave;
  int jojo;       /* the challenger of the tracked battle, -1 none */
  int fainted0;   /* party members at 0 HP when the battle began */
  bool counted;
  char q[QUEUE][MSG_LEN];
  int nq, qi;
  char out[MSG_LEN];
} H;

void ach_reset(void) { memset(&H, 0, sizeof H); }

static int fainted_count(const gc_run* run)
{
  int n = 0;
  for (int i = 0; i < run->n_party; i++)
    if (run->party[i].b.hp <= 0) n++;
  return n;
}

/* a hash of the run's seed: the same for the whole run, different between runs */
static uint32_t run_tag(const gc_run* run)
{
  uint32_t h = 2166136261u;
  for (size_t i = 0; i < run->run.seed.len && i < 64; i++) h = (h ^ run->run.seed.u[i]) * 16777619u;
  return h ? h : 1;
}

static void queue(const char* a, const char* b)
{
  if (H.nq >= QUEUE) return;
  snprintf(H.q[H.nq++], MSG_LEN, "%s\n%s", a, b);
}

bool ach_on_screen(const gc_run* run, int kind, int jojo)
{
  if (!run || !run->sys) return false;
  if (kind == GC_SCREEN_COMMAND) {
    if (!H.tracking || H.wave != run->wave) {
      H.tracking = true;
      H.wave = run->wave;
      H.jojo = jojo;
      H.fainted0 = fainted_count(run);
      H.counted = false;
    }
    return false;
  }
  if (!H.tracking || H.counted || H.wave != run->wave || H.jojo < 0) return false;
  /* the battle ended: a win leaves its reward screen (or the end of the run), a defeat the game over */
  if (kind != GC_SCREEN_REWARD && kind != GC_SCREEN_CLEAR) return false;
  H.counted = true;
  gc_system* sys = run->sys;
  gc_achv_jojo_won(sys, H.jojo, run_tag(run), fainted_count(run) <= H.fainted0);
  char seed[32];
  snprintf(seed, sizeof seed, "%08xw%d", (unsigned)run_tag(run), (int)run->wave);
  gc_achv_new fresh;
  gc_achv_evaluate(sys, seed, 0, &fresh);
  for (int i = 0; i < fresh.n; i++) {
    const gc_achv_def* d = &gc_achv_defs[fresh.id[i]];
    char a[96], b[96];
    snprintf(a, sizeof a, "Achievement unlocked: %s!", d->name);
    snprintf(b, sizeof b, "You received %s!", gc_achv_reward_name(d));
    queue(a, b);
  }
  return true;
}

const char* ach_take_message(void)
{
  if (H.qi >= H.nq) {
    H.nq = H.qi = 0;
    return NULL;
  }
  snprintf(H.out, sizeof H.out, "%s", H.q[H.qi++]);
  return H.out;
}
