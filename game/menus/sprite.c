/* Battle sprites: upstream PokemonSpecies.getSpriteId / getSpriteAtlasPath
 * and the animation Pokemon.loadAssets builds (frames 0001..0400 that exist,
 * 10 per second, looping). */
#include <jansson.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fusion_palette.h"
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

/* Where a sheet comes from: its key and, for a shiny variant that recolours the normal sprite, the colour map and the variant in it */
typedef struct {
  char key[96], map[96];
  int variant;
} sheet_src;

static void sheet_pick(sheet_src* s, bool back, bool female, const char* id, bool shiny, int variant) {
  const char* b = back ? "back/" : "";
  const char* f = female ? "female/" : "";
  int kind = variant_kind(back, female, id, variant);
  int kind0 = variant_kind(back, female, id, 0);
  s->map[0] = 0;
  s->variant = variant;
  if (!shiny) {
    snprintf(s->key, sizeof s->key, "images/pokemon/%s%s%s", b, f, id);
  } else if (kind < 0 || (!variant && !kind0)) {
    snprintf(s->key, sizeof s->key, "images/pokemon/%sshiny/%s%s", b, f, id);
  } else if (kind == 2) {
    snprintf(s->key, sizeof s->key, "images/pokemon/variant/%s%s%s_%d", b, f, id, variant + 1);
  } else {
    /* colour map over the normal sprite (upstream's sprite shader) */
    snprintf(s->key, sizeof s->key, "images/pokemon/%s%s%s", b, f, id);
    snprintf(s->map, sizeof s->map, "images/pokemon/variant/%s%s%s.json", b, f, id);
  }
}

static rr_atlas* load_one(bool back, bool female, const char* id, bool shiny, int variant, const rr_recolor_pair* extra, int nextra) {
  sheet_src s;
  sheet_pick(&s, back, female, id, shiny, variant);
  return rr_atlas_load_key_maps(s.key, s.map[0] ? s.map : NULL, s.variant, extra, nextra);
}

/* The look of one half of a Pokemon */
typedef struct {
  uint16_t species;
  uint8_t form;
  int8_t gender;
  bool shiny;
  uint8_t variant;
} look;

/* Pass 0..3 of the search for a sprite: the female sheet of the form, of the species, then the male ones. Forms whose formSpriteKey is "" share
 * the base sprite, and only species with gender differences have female sprites; neither is in battle-core's tables, so fall back through
 * what exists. False: skip this pass */
static bool pass_id(const look* l, int pass, bool* fem, char id[48]) {
  const char* fk = menu_form_key(l->species, l->form);
  const bool female = l->gender == MG_FEMALE && !no_gender_diff(fk);
  *fem = pass < 2 ? female : false;
  if (pass < 2 && !female) return false;
  const bool with_form = (pass % 2) == 0;
  if (with_form && !*fk) return false;
  if (with_form)
    snprintf(id, 48, "%u-%s", l->species, fk);
  else
    snprintf(id, 48, "%u", l->species);
  return true;
}

/* ---- fused Pokemon: the first species' sprite recoloured towards the fusion half's palette (fusion_palette.c) ---- */

static bool sheet_exists(const sheet_src* s) {
  pak_entry e;
  return pak_find(NULL, s->key, &e);
}

/* The sheet menu_sprite_load ends up drawing for a look, found without loading it */
static bool find_sheet(const look* l, bool back, sheet_src* out) {
  for (int pass = 0; pass < 4; pass++) {
    bool fem;
    char id[48];
    if (!pass_id(l, pass, &fem, id)) continue;
    sheet_pick(out, back, fem, id, l->shiny, l->variant);
    if (sheet_exists(out)) return true;
    if (l->shiny) {
      sheet_pick(out, back, fem, id, false, 0);
      if (sheet_exists(out)) return true;
    }
  }
  return false;
}

typedef struct {
  bool used;
  look a, b;
  int n;
  rr_recolor_pair pairs[FP_MAX_PAIRS];
} fusion_entry;

#define FUSION_CACHE 8
static fusion_entry fcache[FUSION_CACHE];
static int fnext;

static bool same_look(const look* x, const look* y) {
  return x->species == y->species && x->form == y->form && x->gender == y->gender && x->shiny == y->shiny && x->variant == y->variant;
}

/* One half's first frame and variant map for fp_compute; false when the sheet is missing or not an indexed one */
static bool half_image(const look* l, bool back, fp_image* img, fp_map* map, rr_recolor_pair* store, uint8_t** px) {
  sheet_src src;
  if (!find_sheet(l, back, &src)) return false;
  size_t len;
  void* blob = pak_read(src.key, &len);
  if (!blob) return false;
  int w, h;
  bool ok = fp_rrt_first_frame(blob, len, NULL, 0, &w, &h);
  if (ok) {
    *px = malloc((size_t)w * h * 4 + 4); /* +4: a frame with no pixels is still a buffer */
    ok = *px && fp_rrt_first_frame(blob, len, *px, (size_t)w * h * 4, &w, &h);
  }
  free(blob);
  if (!ok) return false;
  *img = (fp_image){*px, w, h};
  const int n = src.map[0] ? rr_variant_pairs(src.map, src.variant, store, 64) : 0;
  *map = (fp_map){(const uint32_t*)store, n};
  return true;
}

/* updateFusionPalette for a Pokemon, as colour pairs for the first species' sheet; the pairs of a look are kept (the battle asks for the front
 * and the back, and each screen again). 0: the sprite is drawn as it is */
static int fusion_pairs(const menu_mon* m, rr_recolor_pair out[FP_MAX_PAIRS]) {
  const look a = {m->species, m->form, m->gender, m->shiny, m->variant};
  const look b = {m->fusion_species, m->fusion_form, m->fusion_gender, m->fusion_shiny, m->fusion_variant};
  for (int i = 0; i < FUSION_CACHE; i++)
    if (fcache[i].used && same_look(&fcache[i].a, &a) && same_look(&fcache[i].b, &b)) {
      memcpy(out, fcache[i].pairs, sizeof fcache[i].pairs);
      return fcache[i].n;
    }
  uint8_t* px[4] = {0};
  fp_image img[4];
  fp_map maps[4];
  rr_recolor_pair store[4][64];
  bool ok = true;
  for (int i = 0; ok && i < 4; i++) ok = half_image(i < 2 ? &a : &b, i & 1, &img[i], &maps[i], store[i], &px[i]);
  static fp_result r;
  if (ok) ok = fp_compute(img, maps, &r);
  for (int i = 0; i < 4; i++) free(px[i]);
  fusion_entry* e = &fcache[fnext];
  fnext = (fnext + 1) % FUSION_CACHE;
  memset(e, 0, sizeof *e);
  e->used = true;
  e->a = a;
  e->b = b;
  for (int k = 0; ok && k < r.n; k++) {
    e->pairs[k].from = (uint32_t)r.from[k][0] << 24 | (uint32_t)r.from[k][1] << 16 | (uint32_t)r.from[k][2] << 8 | 0xFF;
    e->pairs[k].to = (uint32_t)r.to[k][0] << 24 | (uint32_t)r.to[k][1] << 16 | (uint32_t)r.to[k][2] << 8 | 0xFF;
    e->n = k + 1;
  }
  memcpy(out, e->pairs, sizeof e->pairs);
  return e->n;
}

bool menu_sprite_load(menu_sprite* s, const menu_mon* m, bool back) {
  memset(s, 0, sizeof *s);
  const look l = {m->species, m->form, m->gender, m->shiny, m->variant};
  rr_recolor_pair fusion[FP_MAX_PAIRS];
  const int nfusion = m->fusion && m->fusion_species ? fusion_pairs(m, fusion) : 0;
  for (int pass = 0; pass < 4 && !s->atlas; pass++) {
    bool fem;
    char id[48];
    if (!pass_id(&l, pass, &fem, id)) continue;
    s->atlas = load_one(back, fem, id, m->shiny, m->variant, fusion, nfusion);
    if (!s->atlas && m->shiny) s->atlas = load_one(back, fem, id, false, 0, fusion, nfusion);
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
