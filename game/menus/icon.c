/* Party icons: upstream PokemonSpecies.getIconAtlasKey / getIconId and
 * BattleScene.addPokemonIcon. Species data comes from battle-core's generated
 * tables (read-only). */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "menus.h"
#include "pr_data.h"

static const pr_species* species_of(uint16_t id) {
  int lo = 0, hi = PR_SPECIES_COUNT - 1;
  while (lo <= hi) {
    int mid = (lo + hi) / 2;
    if (pr_species_table[mid].id == id) return &pr_species_table[mid];
    if (pr_species_table[mid].id < id)
      lo = mid + 1;
    else
      hi = mid - 1;
  }
  return NULL;
}

static const char* form_key_of(const pr_species* sp, int form) {
  if (!sp || sp->nforms <= 1) return "";
  if (form >= sp->nforms) form = sp->nforms - 1;
  const char* k = pr_forms[sp->form0 + form].form_key;
  return k ? k : "";
}

/* getIconId's species with female icons */
static bool female_icon(uint16_t id, const char* form_key) {
  switch (id) {
    case 84: case 85: case 255: case 256: case 443: case 444: case 449: case 450: case 521: case 592: case 593:
      return true; /* Doduo, Dodrio, Torchic, Combusken, Gible, Gabite, Hippopotas, Hippowdon, Unfezant, Frillish, Jellicent */
    case 154: case 257: case 445: case 668: /* Meganium, Blaziken, Garchomp, Pyroar: not their megas */
      return strcmp(form_key, "mega") && strcmp(form_key, "mega-z");
    default:
      return false;
  }
}

typedef struct {
  const rr_frame* f;
} icon_frame;

/* Finds the frame; falls back like upstream (a missing variant icon shows the
 * plain icon) and further to the base form when a form has no icon. */
static const rr_frame* icon_find(uint16_t id, int form, bool female, bool shiny, int variant) {
  const pr_species* sp = species_of(id);
  int gen = sp ? sp->generation : 1;
  const char* fk = form_key_of(sp, form);
  /* Zacian/Zamazenta crowned sprite keys; Dudunsparce has one icon for both forms */
  if ((id == 888 || id == 889) && !strncmp(fk, "behemoth", 8)) fk = "crowned";
  if (id == 982) fk = "";
  char base[48], key[40], name[64];
  snprintf(base, sizeof base, "%u%s", id, female && female_icon(id, fk) ? "-f" : "");
  const rr_frame* f = NULL;
  for (int pass = 0; pass < 2 && !f; pass++) {
    const char* formpart = pass == 0 ? fk : "";
    char suffix[32];
    snprintf(suffix, sizeof suffix, "%s%s", *formpart ? "-" : "", formpart);
    if (shiny) {
      snprintf(key, sizeof key, "pokemon_icons_%dv", gen);
      snprintf(name, sizeof name, "%s%s_%d", base, suffix, variant + 1);
      f = mui_frame(key, name);
      if (!f) {
        snprintf(key, sizeof key, "pokemon_icons_%d", gen);
        snprintf(name, sizeof name, "%ss%s", base, suffix);
        f = mui_frame(key, name);
      }
    }
    if (!f) {
      snprintf(key, sizeof key, "pokemon_icons_%d", gen);
      snprintf(name, sizeof name, "%s%s", base, suffix);
      f = mui_frame(key, name);
    }
  }
  return f;
}

void menu_draw_icon_tinted(const menu_mon* m, float x, float y, float ox, float oy, float scale, rr_color tint) {
  const rr_frame* a = icon_find(m->species, m->form, m->gender == MG_FEMALE, m->shiny, m->variant);
  if (!a) return;
  rr_draw_opts o = RR_DEFAULT_OPTS;
  o.scale_x = o.scale_y = scale;
  o.tint = tint;
  /* the icon sprite has origin (0.5, 0) inside a container shifted by the origin */
  float cx = x - a->src_w * scale * (ox - 0.5f);
  float cy = y - a->src_h * scale * oy;
  if (!m->fusion) {
    o.origin_x = 0.5f;
    rr_draw(a, floorf(cx), floorf(cy), &o);
    return;
  }
  const rr_frame* b =
      icon_find(m->fusion_species, m->fusion_form, m->fusion_gender == MG_FEMALE, m->fusion_shiny, m->fusion_variant);
  if (!b) {
    o.origin_x = 0.5f;
    rr_draw(a, floorf(cx), floorf(cy), &o);
    return;
  }
  /* addPokemonIcon: the head is the top of the base icon, cut to
   * (hA + hB) / 4 rows (ceil when hA <= hB, else floor); the body is the
   * fusion icon from that height down. Heights are the trimmed (cut) sizes;
   * both are drawn at the average of their trim offsets. */
  int ha = a->h, hb = b->h;
  int head = (ha <= hb) ? (int)ceilf((ha + hb) / 4.0f) : (int)floorf((ha + hb) / 4.0f);
  if (head > ha) head = ha;
  float fy = (a->off_y + b->off_y) / 2.0f;
  rr_frame top = *a, bot = *b;
  top.h = (int16_t)head;
  top.off_y = 0;
  top.src_h = top.h;
  int body = hb - head;
  if (body < 0) body = 0;
  bot.y = (int16_t)(b->y + head);
  bot.h = (int16_t)body;
  bot.off_y = 0;
  bot.src_h = bot.h;
  float left_a = cx - a->src_w * 0.5f * scale + a->off_x * scale, left_b = cx - b->src_w * 0.5f * scale + b->off_x * scale;
  rr_draw_opts p = RR_DEFAULT_OPTS;
  p.scale_x = p.scale_y = scale;
  p.tint = tint;
  rr_draw(&top, floorf(left_a), floorf(cy + fy * scale), &p);
  rr_draw(&bot, floorf(left_b), floorf(cy + (fy + head) * scale), &p);
}

void menu_draw_icon_scaled(const menu_mon* m, float x, float y, float ox, float oy, float scale) {
  menu_draw_icon_tinted(m, x, y, ox, oy, scale, RR_RGBA(255, 255, 255, 255));
}

void menu_draw_icon(const menu_mon* m, float x, float y, float ox, float oy) { menu_draw_icon_scaled(m, x, y, ox, oy, 1.0f); }
