/* Host check of the battle adapter: runs a few turns of a stand-in battle through battle-core
 * and prints the events and their text. Build: make -C game/tests; run: game/tests/build/evtest
 * [locales dir] (upstream's locales/en, not in this repo). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "battle_setup.h"
#include "pr/events.h"
#include "pr/text.h"
#include "pr_enums.h"

static const char* g_dir = "/home/user/up/pokerogue/locales/en";

static const char* locale_get(void* ctx, const char* file, int fallback, size_t* len) {
  (void)ctx;
  static struct { char name[64]; char* text; size_t len; } cache[96];
  static int n;
  if (fallback) return NULL;
  for (int i = 0; i < n; i++)
    if (!strcmp(cache[i].name, file)) return *len = cache[i].len, cache[i].text;
  if (n >= 96) return NULL;
  char path[512];
  snprintf(path, sizeof path, "%s/%s.json", g_dir, file);
  FILE* f = fopen(path, "rb");
  char* t = NULL;
  size_t l = 0;
  if (f) {
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    t = malloc(sz + 1);
    l = fread(t, 1, sz, f);
    t[l] = 0;
    fclose(f);
  }
  snprintf(cache[n].name, sizeof cache[n].name, "%s", file);
  cache[n].text = t;
  cache[n].len = l;
  n++;
  *len = l;
  return t;
}

static const char* const TYPES[] = {"NONE", "TEXT", "MOVE_ANIM", "CHARGE_ANIM", "COMMON_ANIM", "DAMAGE", "HP",
                                    "STAT_STAGES", "ABILITY_SHOW", "ABILITY_HIDE", "POKEMON_ANIM", "FAINT",
                                    "SWITCH_OUT", "SWITCH_IN", "SHINY_SPARKLE"};

int main(int argc, char** argv) {
  if (argc > 1) g_dir = argv[1];
  pr_text_source src = {locale_get, NULL, "en"};
  static bs_battle b;
  int seed = argc > 2 ? atoi(argv[2]) : 1;
  char s[32];
  snprintf(s, sizeof s, "demo%d", seed);
  bs_begin(&b, BIOME_ID_GRASS, 5, s);
  int pm[] = {MOVE_ID_BLAZE_KICK, MOVE_ID_CLOSE_COMBAT, MOVE_ID_BRAVE_BIRD, MOVE_ID_SWORDS_DANCE};
  int em[] = {MOVE_ID_EARTHQUAKE, MOVE_ID_DRAGON_CLAW, MOVE_ID_SWORDS_DANCE, MOVE_ID_ROCK_SLIDE};
  bs_add(&b, 0, SPECIES_ID_BLAZIKEN, 50, pm, 4);
  bs_add(&b, 1, SPECIES_ID_GARCHOMP, 48, em, 4);
  for (int t = 0; t < 8 && !bs_outcome(&b); t++) {
    bs_turn_fight(&b, t % 4);
    printf("--- turn %d (stop %d, unported %d, deferred %d)\n", t + 1, b.eng.stop, b.bt.n_unported, b.bt.n_deferred);
    for (int i = 0; i < b.eng.n_events; i++) {
      const pr_event* e = &b.eng.events[i];
      char text[512] = "";
      if (e->type == PR_EV_TEXT) {
        if (e->msg.key) pr_msg_format(&e->msg, &src, text, sizeof text);
        else snprintf(text, sizeof text, "<no key>");
        for (char* p = text; *p; p++)
          if (*p == '\n') *p = '|';
      }
      printf("  %-13s b=%d t=%d id=%d v=%d  hp %d/%d vs %d/%d  %s\n", e->type < 15 ? TYPES[e->type] : "?", e->battler,
             e->target, e->id, e->value, e->field[0].hp, e->field[0].max_hp, e->field[2].hp, e->field[2].max_hp, text);
    }
    if (getenv("TRACE")) for (int i = 0; i < b.eng.n_trace; i++) printf("  trace %s\n", b.eng.trace[i]);
    for (int i = 0; i < b.bt.n_unported; i++) printf("  unported: %s\n", pr_unported_name(b.bt.unported[i]));
  }
  printf("outcome %d\n", bs_outcome(&b));
  return 0;
}
