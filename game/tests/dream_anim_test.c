/* Weird Dream's transformation timeline (source/dream_anim.c) against upstream's own loops: fixtures/dream-anim.txt is made by
 * dream_anim.producer.mjs, which copies the particle code of animations.ts statement for statement. Also the sequence's states at the moments
 * the tweens change, the schedule for 1..6 Pokemon and the container's fades. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/dream_anim.h"

static int checks;
#define CHECK(x, ...) do { checks++; if (!(x)) { fprintf(stderr, "dream_anim_test line %d: %s\n", __LINE__, #x); exit(1); } } while (0)
#define NEAR(a, b, e) (fabsf((float)(a) - (float)(b)) <= (e))
#define FRAME_MS (1000.0 / 60.0)

static int find(const da_particle* ps, int n, float x, float y, float scale)
{
  for (int i = 0; i < n; i++) if (NEAR(ps[i].x, x, 0.01f) && NEAR(ps[i].y, y, 0.01f) && NEAR(ps[i].scale, scale, 0.0005f)) return i;
  return -1;
}

static void replay(const char* path)
{
  FILE* f = fopen(path, "r");
  CHECK(f != NULL);
  char kind[4];
  int pos, a, b, nframes, npicks, particles = 0, counts = 0;
  double total = 0;
  static da_particle ps[DA_MAX_PARTICLES];
  while (fscanf(f, "%3s", kind) == 1) {
    if (kind[0] == 'T') {
      CHECK(fscanf(f, "%lf", &total) == 1);
      CHECK(NEAR(da_resolve_ms(), 4700 + total + 900 + 150 + 1500 + 3000, 0.01f));
      continue;
    }
    if (kind[0] == 'N') {
      int t, want;
      CHECK(fscanf(f, "%d %d", &t, &want) == 2);
      CHECK(da_particles((float)t, 0, ps, DA_MAX_PARTICLES) == want);
      counts++;
      continue;
    }
    /* S: pos, cycle, i; A: pos, i, 0; C: pos, speed, i */
    CHECK(fscanf(f, "%d %d %d %d %d", &pos, &a, &b, &nframes, &npicks) == 5);
    /* when its frame 0 happens (one frame after its counter began, for the arc and the spiral: the counter's first lap) */
    const double ce = da_old_free_ms();   /* 4700 + the cycle: checked against the producer's sum at the T line */
    double born;
    if (kind[0] == 'S') born = 2000 + (a + 1) * 8 * FRAME_MS;
    else if (kind[0] == 'A') born = 3700 + 1 * FRAME_MS;
    else born = ce + (a == 8 ? 32 * FRAME_MS : 0);
    for (int k = 0; k < npicks; k++) {
      int fr;
      double x, y, s;
      CHECK(fscanf(f, "%d %lf %lf %lf", &fr, &x, &y, &s) == 4);
      const float t = (float)(born + fr * FRAME_MS + 0.5);
      const int n = da_particles(t, pos, ps, DA_MAX_PARTICLES);
      if (find(ps, n, (float)x, (float)y, (float)s) < 0) fprintf(stderr, "miss %s pos %d a %d b %d frame %d want %.3f %.3f %.3f t %.1f n %d\n", kind, pos, a, b, fr, x, y, s, t, n);
      CHECK(find(ps, n, (float)x, (float)y, (float)s) >= 0);
      particles++;
    }
  }
  fclose(f);
  CHECK(particles > 1000 && counts == 16);
}

static void test_states(void)
{
  da_state s;
  const float R = da_resolve_ms();
  CHECK(NEAR(R, 13373.088f, 0.05f) && NEAR(da_end_ms(), R + 150 + 2000, 0.001f));
  da_state_at(0, &s);
  CHECK(s.old_vis && s.old_alpha == 0 && s.otint_alpha == 0 && !s.ntint_vis && !s.new_vis);
  da_state_at(1000, &s);
  CHECK(s.old_alpha > 0.4f && s.old_alpha < 0.6f);   /* Cubic.easeInOut at one half */
  da_state_at(2000, &s);
  CHECK(NEAR(s.old_alpha, 1, 0.001f) && s.old_vis && s.otint_alpha == 0);
  da_state_at(2500, &s);
  CHECK(NEAR(s.otint_alpha, 0.5f, 0.001f) && s.old_vis);                  /* the white tint rises linearly */
  da_state_at(3000, &s);
  CHECK(!s.old_vis && NEAR(s.otint_alpha, 1, 0.001f) && s.otint_vis && !s.ntint_vis);
  da_state_at(4699, &s);
  CHECK(!s.ntint_vis && NEAR(s.otint_scale, 1, 0.001f));
  da_state_at(4700, &s);
  CHECK(s.ntint_vis && NEAR(s.ntint_scale, 0.25f, 0.001f) && NEAR(s.otint_scale, 1, 0.001f) && !s.new_vis);
  /* the first step (c = 1.5) lasts 333.3 ms forth: at its end the old silhouette is at 0.25 and the new one at 1 */
  da_state_at(4700 + 500.0f / 1.5f, &s);
  CHECK(NEAR(s.otint_scale, 0.25f, 0.002f) && NEAR(s.ntint_scale, 1, 0.002f));
  da_state_at(4700 + 2 * 500.0f / 1.5f, &s);   /* and back at its end */
  CHECK(NEAR(s.otint_scale, 1, 0.002f) && NEAR(s.ntint_scale, 0.25f, 0.002f));
  const float ce = da_old_free_ms();
  da_state_at(ce - 1, &s);
  CHECK(s.otint_vis && !s.new_vis && NEAR(s.otint_scale, 0.25f, 0.05f));
  da_state_at(ce + 0.05f, &s);
  CHECK(!s.otint_vis && s.new_vis && NEAR(s.new_alpha, 1, 0.001f) && NEAR(s.ntint_scale, 1, 0.001f) && s.ntint_vis);
  da_state_at(ce + 900 + 150 + 750, &s);   /* half way through the new silhouette's fade (linear) */
  CHECK(NEAR(s.ntint_alpha, 0.5f, 0.001f) && s.new_vis && s.new_alpha == 1);
  da_state_at(R - 1, &s);
  CHECK(!s.ntint_vis && s.new_alpha == 1);
  da_state_at(R + 150 + 1000, &s);
  CHECK(NEAR(s.new_alpha, 0.5f, 0.001f));
  da_state_at(da_end_ms() + 1, &s);
  CHECK(!s.new_vis && !s.old_vis && !s.otint_vis && !s.ntint_vis);
  CHECK(da_new_load_ms() == 3700 && da_old_free_ms() == ce && da_new_load_ms() < da_old_free_ms());
}

static void test_schedule(void)
{
  float st[DA_MAX];
  int pos[DA_MAX];
  const float R = da_resolve_ms();
  CHECK(NEAR(da_schedule(1, st, pos), R, 0.01f) && st[0] == 0 && pos[0] == DA_CENTER);
  CHECK(NEAR(da_schedule(3, st, pos), 3 * R, 0.01f));
  for (int i = 0; i < 3; i++) CHECK(NEAR(st[i], i * R, 0.01f) && pos[i] == DA_CENTER);   /* back to back, centred */
  CHECK(NEAR(da_schedule(4, st, pos), 2 * R, 0.01f));
  CHECK(pos[0] == DA_CENTER && pos[1] == DA_LEFT && pos[2] == DA_RIGHT && pos[3] == DA_CENTER);
  CHECK(st[0] == 0 && st[1] == 4000 && st[2] == 8000 && NEAR(st[3], R, 0.01f));
  CHECK(NEAR(da_schedule(6, st, pos), 8000 + 2 * R, 0.01f));
  CHECK(pos[4] == DA_LEFT && pos[5] == DA_RIGHT && NEAR(st[4], 4000 + R, 0.01f) && NEAR(st[5], 8000 + R, 0.01f));
  float x, y;
  da_offset(DA_CENTER, &x, &y);
  CHECK(x == 0 && y == 0);
  da_offset(DA_LEFT, &x, &y);
  CHECK(x == -100 && y == -15);
  da_offset(DA_RIGHT, &x, &y);
  CHECK(x == 100 && y == -15);
}

static void test_container(void)
{
  CHECK(da_container_alpha(0, -1) == 0);
  CHECK(NEAR(da_container_alpha(1500, -1), 0.5f, 0.001f));
  CHECK(da_container_alpha(3000, -1) == 1 && da_container_alpha(30000, -1) == 1);
  CHECK(da_container_alpha(20000, 20000) == 1);
  CHECK(NEAR(da_container_alpha(21500, 20000), 0.5f, 0.001f));
  CHECK(da_container_alpha(23000, 20000) == 0 && da_container_alpha(40000, 20000) == 0);
}

int main(void)
{
  replay("fixtures/dream-anim.txt");
  test_states();
  test_schedule();
  test_container();
  printf("dream_anim_test: %d checks passed\n", checks);
  return 0;
}
