/* The scenes' music (see bgm.h). The track is bgm_choose's; bgm_play plays it. */
#include "bgm.h"

#include <stdio.h>
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
  s->final_boss = (r->run.mode == GAME_MODES_CLASSIC || r->run.mode == GAME_MODES_CHALLENGE) && pr_run_is_wave_final(r->run.mode, W.wave);
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

static const char* (*foe_source)(const gc_run*);
void bgm_set_foe_source(const char* (*foe_id)(const gc_run* run)) { foe_source = foe_id; }

/* The JoJo trainer's own theme, when there is one in the pack: custom/<id>; the three Pillar Men share custom/pillar_men. */
#define PILLAR_TRACK "custom/pillar_men"
static const char* custom_theme(const gc_run* r, char buf[BGM_NAME_LEN]) {
  const char* id = foe_source ? foe_source(r) : NULL;
  if (!id || !*id) return NULL;
  snprintf(buf, BGM_NAME_LEN, "custom/%s", id);
  if (bgm_has(buf)) return buf;
  if (!strcmp(id, "esidisi") || !strcmp(id, "wamuu") || !strcmp(id, "kars")) {
    snprintf(buf, BGM_NAME_LEN, "%s", PILLAR_TRACK);
    if (bgm_has(buf)) return buf;
  }
  return NULL;
}

static const char* track_now(const gc_run* r, char buf[BGM_NAME_LEN]) {
  const char* own = custom_theme(r, buf);
  if (own) return own;
  bgm_scene s;
  scene_of(r, &s);
  return bgm_scene_track(&s, buf);
}

void bgm_on_mystery(int type) {
  if (type == GC_ME_PILLAR_MEN && bgm_has(PILLAR_TRACK)) {
    bgm_play_faded(PILLAR_TRACK, 500, 750);
    return;
  }
  const char* track = bgm_mystery_track(type, true); /* all_gens: the default music preference */
  if (track) bgm_play_faded(track, 500, 750);
}

void bgm_on_title(void) {
  time_t t = time(NULL);
  struct tm tm;
  memset(&tm, 0, sizeof tm);
  if (!localtime_r(&t, &tm)) tm.tm_mon = 5;
  bgm_play(bgm_title_track(tm.tm_mon, tm.tm_mday));
}

void bgm_on_football_match(unsigned seed, unsigned round) { bgm_play(bgm_football_track(seed, round)); }

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

void bgm_on_final_boss_phase_two(const gc_run* run) {
  char buf[BGM_NAME_LEN];
  bgm_play(track_now(run, buf));
}

void bgm_on_foe_sent_out(const gc_run* run) {
  if (W.battle_type != BATTLE_TYPE_TRAINER || W.started) return;
  char buf[BGM_NAME_LEN];
  W.started = true;
  bgm_play(track_now(run, buf));
}

void bgm_on_trainer_defeated(int trainer_type) { bgm_play(bgm_trainer_victory(trainer_type)); }

void bgm_on_run_end(bool clear) { bgm_fade_out(clear ? 10000.0f : 5000.0f); }
