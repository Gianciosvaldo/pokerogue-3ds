/*
 * bgmtest: the music chooser (game/source/bgm_choose.c) against what upstream's own Battle.getBgmOverride answered
 * (bgm_expected.c, dumped by game/tools/dump-bgm.test.ts from the pinned upstream 8555c08):
 *   - every wave of a Classic run (a wild Magikarp),
 *   - every species and form, both music preferences, in the End and away from it, at wave 100 and at the final wave,
 *   - every trainer type, variant and preference before and after the battle starts,
 *   - a mystery encounter in its default mode,
 * and the parts that are not upstream's answers to a question: the biome music, the trainer victory music, the title date.
 * Every track the chooser can name is also checked against the pack's track list when BGM_TRACKS names a file with one track
 * per line (the 178 names in bgm.pak: `git ls-tree` of the assets pin's audio/bgm, without the extension).
 *
 * Then the player (bgm_player.c) and the scene calls (bgm.c) over a fake DSP (snd_fake.c): a track plays once however often it is asked
 * for, a fade ends in silence and is cancelled by asking for the track again, a track the pack lacks leaves silence, a wave, a trainer
 * (encounter music, battle music at its first send-out, victory music) and the end of a run play and change what upstream's would.
 *
 *   make bgmtest && build/bgmtest [BGM_TRACKS=file]
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "bgm.h"
#include "bgm_choose.h"
#include "bgm_expected.h"
#include "gm_enum_names.h"
#include "pr_data.h"
#include "pr_enums.h"
#include "snd_fake.h"

static int checks, fails;
static char (*tracks)[80];
static int n_tracks;

#define CHECK(cond, ...)              \
  do {                                \
    checks++;                         \
    if (!(cond)) {                    \
      if (fails++ < 40) {             \
        printf("FAIL: " __VA_ARGS__); \
        printf("\n");                 \
      }                               \
    }                                 \
  } while (0)

static const char* show(const char* s) { return s ? s : "(none)"; }
static bool only_a(const char* key) { return !strcmp(key, "audio/bgm/a"); }
static bool same(const char* a, const char* b) { return (!a && !b) || (a && b && !strcmp(a, b)); }

/* every name the chooser returns is a track of the pack (when a track list was given) */
static int named_seen, player_checks, scene_checks;
static void note(const char* name) {
  if (!name) return;
  named_seen++;
  if (!n_tracks) return;
  for (int i = 0; i < n_tracks; i++)
    if (!strcmp(tracks[i], name)) return;
  CHECK(0, "track \"%s\" is not in the pack's track list", name);
}

static bgm_scene wild(int wave, int biome, bool all_gens, bool final_boss, int species, int form) {
  bgm_scene s;
  memset(&s, 0, sizeof s);
  s.wave = wave;
  s.biome = biome;
  s.battle_type = BATTLE_TYPE_WILD;
  s.all_gens = all_gens;
  s.final_boss = final_boss;
  s.trainer_type = -1;
  s.enemy[0].species = (uint16_t)species;
  s.enemy[0].form = (uint8_t)form;
  s.n_enemy = 1;
  return s;
}

static void load_tracks(const char* path) {
  FILE* f = fopen(path, "r");
  if (!f) {
    printf("cannot read the track list %s\n", path);
    exit(2);
  }
  tracks = calloc(400, sizeof *tracks);
  char line[80];
  while (n_tracks < 400 && fgets(line, sizeof line, f)) {
    line[strcspn(line, "\r\n")] = 0;
    if (line[0]) snprintf(tracks[n_tracks++], sizeof tracks[0], "%s", line);
  }
  fclose(f);
}

int main(int argc, char** argv) {
  const char* list = getenv("BGM_TRACKS");
  for (int i = 1; i < argc; i++)
    if (!strncmp(argv[i], "BGM_TRACKS=", 11)) list = argv[i] + 11;
  if (list && *list) load_tracks(list);
  char buf[BGM_NAME_LEN];

  /* ---- every wave of a Classic run */
  for (int w = 0; w <= 200; w++) {
    const bgm_scene s = wild(w, BIOME_ID_PLAINS, true, w == 200, 129, 0);
    const char* got = bgm_battle_override(&s, buf);
    CHECK(same(got, bgm_expected_wave[w]), "wave %d: got %s, upstream %s", w, show(got), show(bgm_expected_wave[w]));
    note(got);
  }

  /* ---- every species and form, in the same order the dump made them: the table is walked, so a case the chooser has no
   * species or form for is a leftover, and a combination with no case must answer "no override" */
  int cur = 0, cur_final = 0, combos = 0;
  for (int i = 0; i < PR_SPECIES_COUNT; i++) {
    const pr_species* sp = &pr_species_table[i];
    const int nforms = sp->nforms > 0 ? sp->nforms : 1;
    for (int f = 0; f < nforms; f++) {
      for (int all = 0; all < 2; all++)
        for (int end = 0; end < 2; end++) {
          const bgm_scene s = wild(100, end ? BIOME_ID_END : BIOME_ID_PLAINS, all, false, sp->id, f);
          const char* got = bgm_battle_override(&s, buf);
          const char* want = NULL;
          if (cur < bgm_expected_wild_count) {
            const bgm_wild_case* c = &bgm_expected_wild[cur];
            if (c->species == sp->id && c->form == f && c->all_gens == all && c->end_biome == end) {
              want = c->expect;
              cur++;
            }
          }
          CHECK(same(got, want), "species %d form %d all_gens %d end %d: got %s, upstream %s", sp->id, f, all, end, show(got), show(want));
          note(got);
          combos++;
        }
      const bgm_scene fin = wild(200, BIOME_ID_END, true, true, sp->id, f);
      const char* got = bgm_battle_override(&fin, buf);
      const char* want = "battle_final_encounter";
      if (cur_final < bgm_expected_final_count) {
        const bgm_wild_case* c = &bgm_expected_final[cur_final];
        if (c->species == sp->id && c->form == f) {
          want = c->expect;
          cur_final++;
        }
      }
      CHECK(same(got, want), "final boss, species %d form %d: got %s, upstream %s", sp->id, f, show(got), show(want));
      note(got);
    }
  }
  CHECK(cur == bgm_expected_wild_count, "%d of upstream's %d wild cases name a species or form the chooser's table does not have",
        bgm_expected_wild_count - cur, bgm_expected_wild_count);
  CHECK(cur_final == bgm_expected_final_count, "%d of upstream's %d final boss cases were not reached",
        bgm_expected_final_count - cur_final, bgm_expected_final_count);

  /* ---- trainers */
  CHECK(gm_trainer_bgm_count > 200, "the trainer table has %d rows", gm_trainer_bgm_count);
  int trainer_cases = 0;
  for (int i = 0; i < gm_trainer_bgm_count; i++) {
    const gm_trainer_bgm* t = &gm_trainer_bgms[i];
    for (int v = 0; v < 3; v++)
      for (int all = 0; all < 2; all++)
        for (int started = 0; started < 2; started++) {
          bgm_scene s;
          memset(&s, 0, sizeof s);
          s.wave = 50;
          s.biome = BIOME_ID_PLAINS;
          s.battle_type = BATTLE_TYPE_TRAINER;
          s.trainer_type = t->trainer_type;
          s.trainer_variant = v;
          s.started = started;
          s.all_gens = all;
          const char* got = bgm_battle_override(&s, buf);
          const char* want = bgm_expected_trainer[i][v * 4 + all * 2 + started];
          CHECK(same(got, want), "trainer %d variant %d all_gens %d started %d: got %s, upstream %s", t->trainer_type, v, all, started,
                show(got), show(want));
          note(got);
          trainer_cases++;
        }
    note(t->victory);
    note(bgm_trainer_victory(t->trainer_type));
  }

  /* ---- a mystery encounter in its default mode */
  {
    bgm_scene s = wild(30, BIOME_ID_PLAINS, true, false, 129, 0);
    s.battle_type = BATTLE_TYPE_MYSTERY_ENCOUNTER;
    s.me_default = true;
    const char* got = bgm_battle_override(&s, buf);
    CHECK(same(got, bgm_expected_me_default), "mystery encounter: got %s, upstream %s", show(got), show(bgm_expected_me_default));
  }

  /* ---- the biome music is the lower-case biome name, for every biome the game has */
  int biomes = 0;
  for (int b = 0; b < 100; b++) {
    const char* name = gm_enum_name(GM_E_BIOME_ID, b);
    if (!name) continue;
    char want[64];
    for (int k = 0; (want[k] = (char)(name[k] >= 'A' && name[k] <= 'Z' ? name[k] + 32 : name[k])); k++) {
    }
    const char* got = bgm_arena_track(b, buf);
    CHECK(!strcmp(got, want), "biome %d: got %s, want %s", b, got, want);
    note(got);
    biomes++;
  }
  CHECK(biomes == 35, "%d biomes", biomes);
  /* a scene with no override plays the biome's music; an override wins */
  {
    const bgm_scene plain = wild(30, BIOME_ID_FOREST, true, false, 129, 0);
    CHECK(!strcmp(bgm_scene_track(&plain, buf), "forest"), "wave 30 in the forest plays %s", bgm_scene_track(&plain, buf));
    const bgm_scene early = wild(3, BIOME_ID_FOREST, true, false, 129, 0);
    CHECK(!strcmp(bgm_scene_track(&early, buf), "battle_wild"), "wave 3 plays %s", bgm_scene_track(&early, buf));
    const bgm_scene bad = wild(30, 77, true, false, 129, 0);
    CHECK(!strcmp(bgm_scene_track(&bad, buf), "plains"), "an unknown biome plays %s", bgm_scene_track(&bad, buf));
  }

  /* ---- the title music: src/phases/title-phase.ts:42, now.getMonth() === 11 || (getMonth() === 0 && getDate() <= 15) */
  for (int m = 0; m < 12; m++)
    for (int d = 1; d <= 31; d++) {
      const char* want = (m == 11 || (m == 0 && d <= 15)) ? "winter_title" : "title";
      CHECK(!strcmp(bgm_title_track(m, d), want), "title music on %d/%d is %s", m + 1, d, bgm_title_track(m, d));
    }
  note("title");
  note("winter_title");
  note("menu");

  /* ---- the unknown trainer and the default victory music */
  CHECK(bgm_trainer_of(99999) == NULL, "a trainer type that is not in the table was found");
  CHECK(!strcmp(bgm_trainer_victory(99999), "bw/victory_trainer"), "default victory music");

  /* ---- the player, over the fake DSP */
  {
    const int before = checks;
    snd_fake_reset();
    bgm_play("title");
    CHECK(!strcmp(SND.key, "audio/bgm/title") && SND.starts == 1 && !strcmp(bgm_current(), "title") && SND.volume == 1.0f,
          "play: %s, %d starts", SND.key, SND.starts);
    bgm_play("title");
    CHECK(SND.starts == 1, "the track already playing was started again (%d starts)", SND.starts);
    bgm_play("menu");
    CHECK(!strcmp(SND.key, "audio/bgm/menu") && SND.starts == 2 && !strcmp(bgm_current(), "menu"), "switch: %s, %d starts", SND.key,
          SND.starts);
    bgm_stop();
    CHECK(!SND.key[0] && !bgm_current()[0], "stop: %s / %s", SND.key, bgm_current());
    bgm_play("menu");
    bgm_play(NULL);
    CHECK(!SND.key[0] && !bgm_current()[0], "play(NULL) did not stop the music");
    bgm_play("menu");
    bgm_play("");
    CHECK(!SND.key[0] && !bgm_current()[0], "play(\"\") did not stop the music");

    /* a fade: the volume falls linearly, then the track is dropped and the volume is back for the next one */
    bgm_play("a");
    bgm_fade_out(100);
    bgm_tick(50);
    CHECK(SND.key[0] && SND.volume > 0.49f && SND.volume < 0.51f, "half way through a fade the volume is %f", SND.volume);
    bgm_tick(60);
    CHECK(!SND.key[0] && !bgm_current()[0] && SND.volume == 1.0f, "after a fade: %s, volume %f", SND.key, SND.volume);
    bgm_tick(1000);
    bgm_play("a");
    bgm_tick(1000);
    CHECK(!strcmp(bgm_current(), "a"), "a track started after a fade was cut by it");
    /* a fade with nothing playing does nothing, and does not cut what starts next */
    bgm_stop();
    bgm_fade_out(100);
    bgm_play("a");
    bgm_tick(1000);
    CHECK(!strcmp(bgm_current(), "a"), "a fade asked while silent cut the next track");
    /* asking for the track that is fading out keeps it, at full volume */
    bgm_fade_out(100);
    bgm_tick(50);
    bgm_play("a");
    CHECK(SND.volume == 1.0f && !strcmp(bgm_current(), "a"), "asking for the fading track: volume %f", SND.volume);
    bgm_tick(1000);
    CHECK(!strcmp(bgm_current(), "a") && SND.key[0], "the fade went on after the track was asked for again");
    /* a different track while one fades: the new one plays at full volume and nothing cuts it */
    bgm_fade_out(100);
    bgm_tick(50);
    bgm_play("b");
    CHECK(!strcmp(SND.key, "audio/bgm/b") && SND.volume == 1.0f, "a new track over a fade: %s, volume %f", SND.key, SND.volume);
    bgm_tick(1000);
    CHECK(!strcmp(bgm_current(), "b") && SND.key[0], "the old fade cut the new track");
    bgm_fade_out(0);
    CHECK(!SND.key[0] && !bgm_current()[0], "a fade of 0 ms did not stop the music");
    /* a track the pack lacks: silence (the old track is gone too), and the next track that exists plays */
    snd_fake_reset();
    {
      SND.has = only_a;
      bgm_stop();
      bgm_play("a");
      bgm_play("missing");
      CHECK(!SND.key[0] && !bgm_current()[0] && SND.refused == 1, "a missing track left %s / %s (%d refused)", SND.key, bgm_current(),
            SND.refused);
      bgm_play("a");
      CHECK(!strcmp(SND.key, "audio/bgm/a"), "a track after a missing one did not play");
      SND.has = NULL;
    }
    /* no DSP: nothing plays and nothing is remembered, so the call is made again when it comes back */
    snd_fake_reset();
    bgm_stop();
    SND.ready = false;
    bgm_play("a");
    CHECK(!SND.key[0] && !bgm_current()[0] && !SND.starts, "music without a DSP: %s / %s", SND.key, bgm_current());
    SND.ready = true;
    bgm_play("a");
    CHECK(!strcmp(SND.key, "audio/bgm/a"), "music did not play once the DSP was ready");
    bgm_stop();
    player_checks = checks - before;
  }

  /* ---- the scene calls, over the same fake DSP and a run built by hand */
  {
    const int before = checks;
    static gc_run R;
    memset(&R, 0, sizeof R);
    R.run.mode = GAME_MODES_CLASSIC;
    R.trainer_type = -1;
    snd_fake_reset();
    bgm_stop();

    time_t now = time(NULL);
    struct tm tm;
    localtime_r(&now, &tm);
    bgm_on_title();
    CHECK(!strcmp(bgm_current(), bgm_title_track(tm.tm_mon, tm.tm_mday)), "title music %s on %d/%d", bgm_current(), tm.tm_mon + 1,
          tm.tm_mday);
    bgm_on_starter_select();
    CHECK(!strcmp(bgm_current(), "menu"), "starter select music %s", bgm_current());

    /* a wild wave 3 in the plains: the menu music fades with the screen, the wave's music starts at black */
    R.battle_type = BATTLE_TYPE_WILD;
    R.n_enemy = 1;
    R.enemy[0].b.species = 129;
    bgm_on_wave_coming(&R, 3, BIOME_ID_PLAINS, BATTLE_TYPE_WILD);
    bgm_tick(300);
    CHECK(!bgm_current()[0], "the old music did not fade out before the new wave (%s)", bgm_current());
    bgm_on_wave(&R, 3, BIOME_ID_PLAINS, BATTLE_TYPE_WILD);
    CHECK(!strcmp(bgm_current(), "battle_wild"), "wave 3: %s", bgm_current());
    /* wave 4 is the same music: not restarted, not faded */
    int starts = SND.starts;
    bgm_on_wave_coming(&R, 4, BIOME_ID_PLAINS, BATTLE_TYPE_WILD);
    bgm_tick(1000);
    bgm_on_wave(&R, 4, BIOME_ID_PLAINS, BATTLE_TYPE_WILD);
    CHECK(SND.starts == starts && !strcmp(bgm_current(), "battle_wild") && SND.key[0], "wave 4 restarted or cut the music");
    /* wave 5 is the biome's */
    bgm_on_wave_coming(&R, 5, BIOME_ID_PLAINS, BATTLE_TYPE_WILD);
    bgm_tick(300);
    bgm_on_wave(&R, 5, BIOME_ID_PLAINS, BATTLE_TYPE_WILD);
    CHECK(!strcmp(bgm_current(), "plains"), "wave 5: %s", bgm_current());
    /* the next wave in the same biome: the same music, uninterrupted */
    starts = SND.starts;
    bgm_on_wave_coming(&R, 6, BIOME_ID_PLAINS, BATTLE_TYPE_WILD);
    bgm_tick(1000);
    bgm_on_wave(&R, 6, BIOME_ID_PLAINS, BATTLE_TYPE_WILD);
    CHECK(SND.starts == starts && !strcmp(bgm_current(), "plains") && SND.key[0], "wave 6 restarted or cut the biome's music");
    /* a trainer's first send-out is only the first, and a wild Pokemon's is none */
    bgm_on_foe_sent_out(&R);
    CHECK(SND.starts == starts, "a wild battle's send-out changed the music");

    /* a trainer with encounter music (and the dialogue it needs): the encounter music, then the battle music at its first send-out */
    const gm_trainer_bgm* enc = NULL;
    const gm_trainer_bgm* plain = NULL;
    for (int i = 0; i < gm_trainer_bgm_count; i++) {
      const gm_trainer_bgm* t = &gm_trainer_bgms[i];
      if (!enc && t->has_encounter && (t->has_messages & 1) && t->encounter[0] && t->mixed) enc = t;
      if (!plain && !t->has_encounter && t->mixed) plain = t;
    }
    CHECK(enc && plain, "no trainer with / without encounter music in the table");
    if (enc && plain) {
      R.battle_type = BATTLE_TYPE_TRAINER;
      R.trainer_type = enc->trainer_type;
      R.trainer_variant = 0;
      R.n_enemy = 2;
      R.enemy[0].b.species = 25;
      R.enemy[1].b.species = 26;
      bgm_on_wave_coming(&R, 10, BIOME_ID_PLAINS, BATTLE_TYPE_TRAINER);
      bgm_tick(300);
      bgm_on_wave(&R, 10, BIOME_ID_PLAINS, BATTLE_TYPE_TRAINER);
      char want[BGM_NAME_LEN];
      snprintf(want, sizeof want, "encounter_%s", enc->encounter[0]); /* Battle.getBgmOverride: `encounter_${getEncounterBgm()}` */
      CHECK(!strcmp(bgm_current(), want), "trainer %d's encounter music: %s, upstream %s", enc->trainer_type, bgm_current(), want);
      bgm_on_foe_sent_out(&R);
      CHECK(!strcmp(bgm_current(), enc->mixed), "trainer %d's battle music: %s, upstream %s", enc->trainer_type, bgm_current(),
            show(enc->mixed));
      starts = SND.starts;
      bgm_on_foe_sent_out(&R); /* its second Pokemon */
      CHECK(SND.starts == starts, "the trainer's second Pokemon restarted the music");
      bgm_on_trainer_defeated(&R);
      CHECK(!strcmp(bgm_current(), bgm_trainer_victory(enc->trainer_type)), "victory music %s, want %s", bgm_current(),
            bgm_trainer_victory(enc->trainer_type));

      /* a trainer with no encounter music: the battle music from the start, and the send-out changes nothing */
      R.trainer_type = plain->trainer_type;
      bgm_on_wave_coming(&R, 20, BIOME_ID_PLAINS, BATTLE_TYPE_TRAINER);
      bgm_tick(300);
      bgm_on_wave(&R, 20, BIOME_ID_PLAINS, BATTLE_TYPE_TRAINER);
      CHECK(!strcmp(bgm_current(), plain->mixed), "trainer %d's battle music: %s, upstream %s", plain->trainer_type, bgm_current(),
            show(plain->mixed));
      starts = SND.starts;
      bgm_on_foe_sent_out(&R);
      CHECK(SND.starts == starts, "the send-out restarted the music of a trainer with no encounter music");
    }

    /* the Classic final boss */
    R.battle_type = BATTLE_TYPE_WILD;
    R.trainer_type = -1;
    R.n_enemy = 1;
    R.enemy[0].b.species = 129;
    bgm_on_wave_coming(&R, 200, BIOME_ID_END, BATTLE_TYPE_WILD);
    bgm_tick(300);
    bgm_on_wave(&R, 200, BIOME_ID_END, BATTLE_TYPE_WILD);
    CHECK(!strcmp(bgm_current(), "battle_final_encounter"), "wave 200: %s", bgm_current());

    /* the end of a run: 10 s after a win, 5 s after a loss */
    bgm_on_run_end(true);
    bgm_tick(5001);
    CHECK(SND.key[0] && SND.volume > 0.4f && SND.volume < 0.6f, "5 s into the win's fade: volume %f", SND.volume);
    bgm_tick(5000);
    CHECK(!SND.key[0], "the win's fade did not end after 10 s");
    bgm_play("a");
    bgm_on_run_end(false);
    bgm_tick(2500);
    CHECK(SND.key[0] && SND.volume > 0.4f && SND.volume < 0.6f, "2.5 s into the loss's fade: volume %f", SND.volume);
    bgm_tick(2600);
    CHECK(!SND.key[0], "the loss's fade did not end after 5 s");
    scene_checks = checks - before;
  }

  printf(
      "bgmtest: %d checks (%d waves, %d species x form x preference x biome, %d trainer cases, %d biomes, %d track names%s; %d player, %d "
      "scene), %d failed\n",
      checks, 201, combos, trainer_cases, biomes, named_seen, n_tracks ? " all in the pack" : "; no track list given", player_checks,
      scene_checks, fails);
  return fails ? 1 : 0;
}
