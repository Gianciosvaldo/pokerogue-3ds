/* battle-core's events as battle screen events (see battle_events.h). */
#include "battle_events.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "i18n.h"
#include "pak.h"
#include "pr/events.h"
#include "pr_enums.h"

/* ------------------------------------------------------------------ text source */

#define LOC_MAX 48
static struct {
  char lang[16], file[48];
  char* text; /* NULL = missing */
  size_t len;
} g_loc[LOC_MAX];
static int g_nloc;

static const char* loc_file(const char* lang, const char* file, size_t* len) {
  for (int i = 0; i < g_nloc; i++)
    if (!strcmp(g_loc[i].lang, lang) && !strcmp(g_loc[i].file, file)) return *len = g_loc[i].len, g_loc[i].text;
  char key[128];
  snprintf(key, sizeof key, "locales/%s/%s.json", lang, file);
  size_t n = 0;
  char* t = pak_read(key, &n);
  if (g_nloc == LOC_MAX) { /* full: drop the oldest */
    free(g_loc[0].text);
    memmove(&g_loc[0], &g_loc[1], sizeof g_loc[0] * (LOC_MAX - 1));
    g_nloc--;
  }
  snprintf(g_loc[g_nloc].lang, sizeof g_loc[g_nloc].lang, "%s", lang);
  snprintf(g_loc[g_nloc].file, sizeof g_loc[g_nloc].file, "%s", file);
  g_loc[g_nloc].text = t;
  g_loc[g_nloc].len = n;
  g_nloc++;
  *len = n;
  return t;
}

static const char* text_get(void* ctx, const char* file, int fallback, size_t* len) {
  (void)ctx;
  /* upstream fallbackLng: es-419 -> es-ES -> en; everything else -> en */
  const char* lang = i18n_language();
  const char* chain[3];
  int n = 0;
  chain[n++] = lang;
  if (!strcmp(lang, "es-419")) chain[n++] = "es-ES";
  if (strcmp(lang, "en")) chain[n++] = "en";
  if (fallback < 0 || fallback >= n) return NULL;
  return loc_file(chain[fallback], file, len);
}

const pr_text_source* be_text_source(void) {
  static pr_text_source src = {text_get, NULL, NULL};
  src.lang = i18n_language();
  return &src;
}

/* ------------------------------------------------------------------ Pokemon */

void be_mon(const pr_battle* bt, const pr_pokemon* p, bv_mon* m) {
  memset(m, 0, sizeof *m);
  m->species = p->species;
  m->form = p->form;
  m->gender = p->gender;
  m->female = p->gender == 1;
  m->shiny = p->shiny;
  snprintf(m->name, sizeof m->name, "%s", i18n_species_name(p->species));
  m->level = p->level;
  m->hp = p->hp;
  m->max_hp = p->stats[STAT_HP];
  m->status = p->status;
  int8_t types[PR_MAX_TYPES];
  int n = pr_get_types(p, types, 0);
  for (int i = 0; i < n && i < 3; i++) m->types[m->n_types++] = types[i];
  for (int i = 0; i < p->n_moves && i < 4; i++) {
    int max = p->max_pp_override[i] >= 0 ? p->max_pp_override[i] : pr_moves[p->moves[i]].pp;
    m->moves[i] = p->moves[i];
    m->pp_max[i] = max;
    m->pp_left[i] = max - p->pp_used[i];
  }
  m->n_moves = p->n_moves;
  m->boss = p->is_boss;
  for (int i = 0; i < 7; i++) m->stat_stages[i] = p->stat_stages[i];
  (void)bt;
}

/* ------------------------------------------------------------------ events */

#define TXT_MAX 64
static char g_txt[TXT_MAX][256];

typedef struct {
  int hp, status;
  bool fainted, present;
} seen_t;

int be_convert(const pr_battle* bt, const pr_engine* e, const bv_mon* shown_player, const bv_mon* shown_enemy,
               bv_event* out, int max) {
  int n = 0, nt = 0;
  seen_t seen[4] = {{0}};
  seen[0] = (seen_t){shown_player->hp, shown_player->status, shown_player->hp <= 0, true};
  seen[2] = (seen_t){shown_enemy->hp, shown_enemy->status, shown_enemy->hp <= 0, true};
  static bv_mon sent[4];
#define PUT(...)                                    \
  do {                                              \
    if (n < max) out[n++] = (bv_event){__VA_ARGS__}; \
  } while (0)

  for (int i = 0; i < e->n_events; i++) {
    const pr_event* ev = &e->events[i];
    int b = ev->battler;
    bool single = b == 0 || b == 2; /* singles: the screen shows battlers 0 and 2 */
    switch (ev->type) {
      case PR_EV_TEXT:
        if (!ev->msg.key || nt >= TXT_MAX) break;
        pr_msg_format(&ev->msg, be_text_source(), g_txt[nt], sizeof g_txt[nt]);
        /* value = callbackDelay, id = prompt, once battle-core fills them (0 = upstream's defaults) */
        PUT(.type = BEV_MESSAGE, .battler = -1, .text = g_txt[nt], .a = ev->value, .prompt = ev->id == 1);
        nt++;
        break;
      case PR_EV_MOVE_ANIM:
        if (single) PUT(.type = BEV_MOVE_ANIM, .battler = b, .target = ev->target, .a = ev->id);
        break;
      case PR_EV_DAMAGE:
        if (!single) break;
        PUT(.type = BEV_DAMAGE, .battler = b, .a = ev->field[b].hp, .b = ev->id);
        seen[b].hp = ev->field[b].hp;
        break;
      case PR_EV_HP:
        if (!single) break;
        PUT(.type = BEV_HEAL, .battler = b, .a = ev->field[b].hp);
        seen[b].hp = ev->field[b].hp;
        break;
      case PR_EV_STAT_STAGES:
        if (!single) break;
        for (int s = STAT_ATK; s <= STAT_EVA; s++)
          if (ev->value & (1 << s)) PUT(.type = BEV_STAT_STAGES, .battler = b, .a = s, .b = ev->id ? 1 : -1);
        break;
      case PR_EV_ABILITY_SHOW:
        if (single) PUT(.type = BEV_ABILITY, .battler = b, .a = ev->id, .b = ev->value);
        break;
      case PR_EV_FAINT:
        if (!single || seen[b].fainted) break;
        PUT(.type = BEV_FAINT, .battler = b);
        seen[b].fainted = true;
        seen[b].present = false;
        break;
      case PR_EV_SWITCH_OUT:
        if (!single || !seen[b].present) break;
        PUT(.type = BEV_WITHDRAW, .battler = b);
        seen[b].present = false;
        break;
      case PR_EV_SWITCH_IN: {
        if (!single) break;
        int side = b >= 2, slot = ev->value;
        if (slot < 0 || slot >= bt->n_party[side]) break;
        be_mon(bt, bt->party[side][slot], &sent[b]);
        sent[b].hp = ev->field[b].hp;
        sent[b].status = ev->field[b].status;
        PUT(.type = BEV_SUMMON, .battler = b, .mon = &sent[b]);
        seen[b] = (seen_t){ev->field[b].hp, ev->field[b].status, false, true};
        break;
      }
      default: /* CHARGE_ANIM, COMMON_ANIM, POKEMON_ANIM, ABILITY_HIDE, SHINY_SPARKLE: not drawn yet */
        break;
    }
    /* status icons follow the snapshot */
    for (int k = 0; k < 4; k += 2)
      if (seen[k].present && ev->field[k].species && ev->field[k].status != seen[k].status) {
        PUT(.type = BEV_STATUS, .battler = k, .a = ev->field[k].status);
        seen[k].status = ev->field[k].status;
      }
  }

  /* catch up on what the events didn't show */
  for (int k = 0; k < 4; k += 2) {
    const pr_pokemon* p = bt->field[k];
    if (!p || !seen[k].present) continue;
    if (p->hp < seen[k].hp) PUT(.type = BEV_DAMAGE, .battler = k, .a = p->hp, .b = HIT_RESULT_INDIRECT);
    else if (p->hp > seen[k].hp) PUT(.type = BEV_HEAL, .battler = k, .a = p->hp);
    if (p->status != seen[k].status) PUT(.type = BEV_STATUS, .battler = k, .a = p->status);
    if (p->hp <= 0 && !seen[k].fainted) PUT(.type = BEV_FAINT, .battler = k);
  }
#undef PUT
  return n;
}
