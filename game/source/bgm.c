/* The scenes' music (see bgm.h). The track is bgm_choose's; bgm_play plays it. */
#include "bgm.h"

#include <string.h>
#include <time.h>

#include "bgm_choose.h"
#include "pr_enums.h"

/* the battle on screen, as the last wave call saw it */
static struct {
  int wave, biome, battle_type;
  bool started; /* Battle.started: a trainer has sent out its first Pokemon */
} W;

static void scene_of(const gc_run* r, bgm_scene* s) {
  memset(s, 0, sizeof *s);
  s->wave = W.wave;
  s->biome = W.biome;
  s->battle_type = W.battle_type;
  s->me_default = W.battle_type == BATTLE_TYPE_MYSTERY_ENCOUNTER; /* the encounters are stand-ins that set no music of their own */
  s->started = W.started;
  s->final_boss = r->run.mode == GAME_MODES_CLASSIC && pr_run_is_wave_final(r->run.mode, W.wave);
  s->all_gens = true; /* the default music preference; the app has no settings screen */
  s->trainer_type = r->trainer_type;
  s->trainer_variant = r->trainer_variant;
  for (int i = 0; i < r->n_enemy && i < 6; i++) {
    s->enemy[s->n_enemy].species = r->enemy[i].b.species;
    s->enemy[s->n_enemy].form = r->enemy[i].b.form;
    s->n_enemy++;
  }
}

static void wave_call(const gc_run* r, int wave, int biome, int battle_type) {
  W.wave = wave;
  W.biome = biome;
  W.battle_type = battle_type;
  W.started = false; /* a new battle */
  (void)r;
}

static const char* track_now(const gc_run* r, char buf[BGM_NAME_LEN]) {
  bgm_scene s;
  scene_of(r, &s);
  return bgm_scene_track(&s, buf);
}

void bgm_on_title(void) {
  time_t t = time(NULL);
  struct tm tm;
  memset(&tm, 0, sizeof tm);
  if (!localtime_r(&t, &tm)) tm.tm_mon = 5;
  bgm_play(bgm_title_track(tm.tm_mon, tm.tm_mday));
}

void bgm_on_starter_select(void) { bgm_play("menu"); }

void bgm_on_wave_coming(const gc_run* run, int wave, int biome, int battle_type) {
  char buf[BGM_NAME_LEN];
  wave_call(run, wave, biome, battle_type);
  const char* track = track_now(run, buf);
  /* fadeOutPrevious: the old music goes down with the screen, when there is new music to come */
  if (strcmp(bgm_current(), track)) bgm_fade_out(250);
}

void bgm_on_wave(const gc_run* run, int wave, int biome, int battle_type) {
  char buf[BGM_NAME_LEN];
  wave_call(run, wave, biome, battle_type);
  bgm_play(track_now(run, buf));
}

void bgm_on_foe_sent_out(const gc_run* run) {
  if (W.battle_type != BATTLE_TYPE_TRAINER || W.started) return;
  char buf[BGM_NAME_LEN];
  W.started = true;
  bgm_play(track_now(run, buf));
}

void bgm_on_trainer_defeated(const gc_run* run) { bgm_play(bgm_trainer_victory(run->trainer_type)); }

void bgm_on_run_end(bool clear) { bgm_fade_out(clear ? 10000.0f : 5000.0f); }
