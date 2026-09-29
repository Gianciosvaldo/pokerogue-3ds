/* Battle sprites: upstream PokemonSpecies.getSpriteId / getSpriteAtlasPath
 * and the animation Pokemon.loadAssets builds (frames 0001..0400 that exist,
 * 10 per second, looping). */
#include <jansson.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "menus.h"
#include "pak.h"
#include "pr_data.h"

const pr_species* menu_species(uint16_t id);
const char* menu_form_key(uint16_t species, int form);

/* images/pokemon/variant/_masterlist.json: which variants have colour maps (1)
 * or their own sprite sheet (2) */
static json_t* masterlist(void) {
  static json_t* j;
  static bool tried;
  if (!tried) {
    tried = true;
    size_t len;
    char* d = pak_read("images/pokemon/variant/_masterlist.json", &len);
    if (d) {
      j = json_loadb(d, len, 0, NULL);
      free(d);
    }
  }
  return j;
}

static int variant_kind(bool back, bool female, const char* id, int variant) {
  json_t* c = masterlist();
  if (c && back) c = json_object_get(c, "back");
  if (c && female) c = json_object_get(c, "female");
  if (c) c = json_object_get(c, id);
  if (!c || !json_is_array(c)) return -1; /* no variant set */
  json_t* v = json_array_get(c, (size_t)variant);
  return v ? (int)json_integer_value(v) : 0;
}

/* The mega/primal/gigantamax forms don't use the female sprite (getBaseSpriteKey) */
static bool no_gender_diff(const char* fk) {
  static const char* const keys[] = {"mega", "mega-x", "mega-y", "mega-z", "mega-original", "mega-curly",
                                     "mega-droopy", "mega-stretchy", "primal", "gigantamax"};
  for (size_t i = 0; i < sizeof keys / sizeof keys[0]; i++)
    if (!strcmp(fk, keys[i])) return true;
  return false;
}

static rr_atlas* load_one(bool back, bool female, const char* id, bool shiny, int variant) {
  char key[96], map[96];
  const char* b = back ? "back/" : "";
  const char* f = female ? "female/" : "";
  int kind = variant_kind(back, female, id, variant);
  int kind0 = variant_kind(back, female, id, 0);
  if (!shiny) {
    snprintf(key, sizeof key, "images/pokemon/%s%s%s", b, f, id);
    return rr_atlas_load_key(key);
  }
  if (kind < 0 || (!variant && !kind0)) {
    snprintf(key, sizeof key, "images/pokemon/%sshiny/%s%s", b, f, id);
    return rr_atlas_load_key(key);
  }
  if (kind == 2) {
    snprintf(key, sizeof key, "images/pokemon/variant/%s%s%s_%d", b, f, id, variant + 1);
    return rr_atlas_load_key(key);
  }
  /* colour map over the normal sprite (upstream's sprite shader) */
  snprintf(key, sizeof key, "images/pokemon/%s%s%s", b, f, id);
  snprintf(map, sizeof map, "images/pokemon/variant/%s%s%s.json", b, f, id);
  return rr_atlas_load_key_variant(key, map, variant);
}

bool menu_sprite_load(menu_sprite* s, const menu_mon* m, bool back) {
  memset(s, 0, sizeof *s);
  const char* fk = menu_form_key(m->species, m->form);
  bool female = m->gender == MG_FEMALE && !no_gender_diff(fk);
  /* Forms whose formSpriteKey is "" share the base sprite, and only species
   * with gender differences have female sprites; neither is in battle-core's
   * tables, so fall back through what exists. */
  for (int pass = 0; pass < 4 && !s->atlas; pass++) {
    bool fem = pass < 2 ? female : false;
    if (pass < 2 && !female) continue;
    bool with_form = (pass % 2) == 0;
    if (with_form && !*fk) continue;
    char id[48];
    if (with_form)
      snprintf(id, sizeof id, "%u-%s", m->species, fk);
    else
      snprintf(id, sizeof id, "%u", m->species);
    s->atlas = load_one(back, fem, id, m->shiny, m->variant);
    if (!s->atlas && m->shiny) s->atlas = load_one(back, fem, id, false, 0);
  }
  if (!s->atlas) return false;
  char name[16];
  for (int i = 1; i <= MENU_SPRITE_MAX_FRAMES; i++) {
    snprintf(name, sizeof name, "%04d.png", i);
    const rr_frame* f = rr_atlas_find(s->atlas, name);
    if (f) s->frames[s->n++] = f;
  }
  if (!s->n && rr_atlas_count(s->atlas)) s->frames[s->n++] = rr_atlas_frame(s->atlas, 0);
  return true;
}

void menu_sprite_free(menu_sprite* s) {
  rr_atlas_free(s->atlas);
  memset(s, 0, sizeof *s);
}

const rr_frame* menu_sprite_frame(const menu_sprite* s, uint32_t ms) {
  if (!s->n) return NULL;
  return s->frames[(ms / 100) % (uint32_t)s->n];
}
