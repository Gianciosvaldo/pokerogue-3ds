/* What the run driver tells the custom achievements (see achv_hooks.h). */
#include "achv_hooks.h"

#include <stdio.h>
#include <string.h>

#include "gc/achv.h"
#include "gc/data.h"

#define QUEUE GC_ACHV_COUNT /* an unlock is told once, so the queue holds at most every achievement of the table */

static struct {
  bool tracking;
  int wave;
  int jojo;       /* the challenger of the tracked battle, -1 none */
  int fainted0;   /* party members at 0 HP when the battle began */
  bool counted;
  unsigned fielded; /* bit i: party member i has been on the field at a command screen */
  int balls;        /* Poke Balls thrown this run */
  bool loaded;      /* the run was resumed from a save: what happened before is unknown */
  bool ended;       /* the run's end was told */
  uint32_t darby_seen; /* the profile's D'Arby wins as of the last screen: a change is evaluated at once */
  uint32_t rematch_seen; /* the same for his rematch */
  uint8_t q[QUEUE]; /* the ids unlocked and not yet taken */
  int nq, qi;
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

static bool evolves(int species)
{
  for (int i = 0; i < gc_species_evo_count; i++)
    if (gc_species_evos[i].id == species) return gc_species_evos[i].n != 0;
  return false;
}

static int bits(unsigned v)
{
  int n = 0;
  for (; v; v &= v - 1) n++;
  return n;
}

void ach_on_ball(void) { H.balls++; }

void ach_reset_loaded(void)
{
  ach_reset();
  H.loaded = true;
}

/* what is met now is unlocked and queued for the app to tell (ach_take_unlock) */
static bool evaluate(gc_system* sys, const gc_run* run)
{
  char seed[32];
  snprintf(seed, sizeof seed, "%08xw%d", (unsigned)run_tag(run), (int)run->wave);
  gc_achv_new fresh;
  gc_achv_evaluate(sys, seed, 0, &fresh);
  for (int i = 0; i < fresh.n; i++)
    if (H.nq < QUEUE) H.q[H.nq++] = fresh.id[i];
  return fresh.n > 0;
}

bool ach_on_screen(const gc_run* run, int kind, int jojo, bool bizarre)
{
  if (!run || !run->sys) return false;
  gc_system* sys = run->sys;
  bool changed = false;
  if (kind == GC_SCREEN_COMMAND) {
    for (int i = 0; i < run->n_party && i < 6; i++)
      if (run->party[i].b.on_field && run->party[i].b.hp > 0) H.fielded |= 1u << i;
    if (!H.tracking || H.wave != run->wave) {
      H.tracking = true;
      H.wave = run->wave;
      H.jojo = jojo;
      H.fainted0 = fainted_count(run);
      H.counted = false;
      changed |= evaluate(sys, run); /* the Pokedex of the profile may have grown since the last wave */
    }
  }
  /* D'Arby was beaten (game-core counts it when the hand is won): his achievement is told on the next screen */
  if (sys->achv.darby_wins != H.darby_seen || sys->achv.darby_rematch_wins != H.rematch_seen) {
    H.darby_seen = sys->achv.darby_wins;
    H.rematch_seen = sys->achv.darby_rematch_wins;
    changed |= evaluate(sys, run);
  }
  /* a new best wave (waves reached in any run) */
  if (run->wave > 0 && (uint32_t)run->wave > sys->achv.best_wave) {
    gc_achv_wave_reached(sys, (uint32_t)run->wave);
    changed |= evaluate(sys, run);
  }
  /* the battle ended: a win leaves its reward screen (or the end of the run), a defeat the game over */
  if (H.tracking && !H.counted && H.wave == run->wave && H.jojo >= 0 && (kind == GC_SCREEN_REWARD || kind == GC_SCREEN_CLEAR)) {
    H.counted = true;
    gc_achv_jojo_won(sys, H.jojo, run_tag(run), fainted_count(run) <= H.fainted0);
    changed |= evaluate(sys, run);
    changed = true;
  }
  if ((kind == GC_SCREEN_CLEAR || kind == GC_SCREEN_GAME_OVER) && !H.ended) {
    H.ended = true;
    changed = true;
    if (kind == GC_SCREEN_CLEAR) {
      uint8_t feats = 0;
      bool all = run->n_party == 6;
      for (int i = 0; i < run->n_party; i++) all &= evolves(run->party[i].b.species);
      if (all) feats |= GC_FEAT_SIX_UNEVOLVED;
      if (!H.loaded && bits(H.fielded) == 1) feats |= GC_FEAT_SOLO;
      if (!H.loaded && H.balls == 0) feats |= GC_FEAT_NO_BALL;
      gc_achv_run_won(sys, bizarre, feats);
    }
    evaluate(sys, run);
  }
  return changed;
}

int ach_take_unlock(void)
{
  if (H.qi >= H.nq) {
    H.nq = H.qi = 0;
    return -1;
  }
  return H.q[H.qi++];
}
