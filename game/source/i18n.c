/* Upstream's i18next lookups over the locale JSON files in data.pak (see i18n.h). */
#include "i18n.h"

#include <ctype.h>
#include <jansson.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gm_enum_names.h"
#include "pak.h"
#include "menus.h"

static char g_lang[16] = "en";

#define NS_MAX 96
static struct {
  char lang[16];
  char ns[48];
  json_t* root; /* NULL = missing */
} g_ns[NS_MAX];
static int g_nns;

void i18n_unload(void) {
  for (int i = 0; i < g_nns; i++)
    if (g_ns[i].root) json_decref(g_ns[i].root);
  g_nns = 0;
}

void i18n_set_language(const char* lang) {
  snprintf(g_lang, sizeof g_lang, "%s", lang && *lang ? lang : "en");
}
const char* i18n_language(void) { return g_lang; }

/* upstream src/i18n-namespace-map.ts */
static const char* const NS_MAP[][2] = {
    {"titles", "trainer-titles"},
    {"moveTriggers", "move-trigger"},
    {"abilityTriggers", "ability-trigger"},
    {"battlePokemonForm", "pokemon-form-battle"},
    {"miscDialogue", "dialogue-misc"},
    {"battleSpecDialogue", "dialogue-final-boss"},
    {"doubleBattleDialogue", "dialogue-double-battle"},
    {"splashMessages", "splash-texts"},
    {"mysteryEncounterMessages", "mystery-encounter-texts"},
    {"biome", "biomes"},
};

static void kebab(const char* s, char* out, int n) {
  int o = 0;
  for (int i = 0; s[i] && o < n - 2; i++) {
    unsigned char c = (unsigned char)s[i];
    if (isupper(c) && i > 0 && (islower((unsigned char)s[i - 1]) || isdigit((unsigned char)s[i - 1]))) out[o++] = '-';
    out[o++] = (char)tolower(c);
  }
  out[o] = 0;
}

static void ns_file(const char* ns, char* out, int n) {
  for (size_t i = 0; i < sizeof NS_MAP / sizeof NS_MAP[0]; i++) {
    if (!strcmp(ns, NS_MAP[i][0])) {
      snprintf(out, n, "%s", NS_MAP[i][1]);
      return;
    }
  }
  if (!strncmp(ns, "mysteryEncounters/", 18)) {
    char k[64];
    kebab(ns + 18, k, sizeof k);
    snprintf(out, n, "mystery-encounters/%s-dialogue", k);
    return;
  }
  kebab(ns, out, n);
}

static json_t* ns_get(const char* lang, const char* ns) {
  for (int i = 0; i < g_nns; i++)
    if (!strcmp(g_ns[i].lang, lang) && !strcmp(g_ns[i].ns, ns)) return g_ns[i].root;
  char file[96], key[160];
  ns_file(ns, file, sizeof file);
  snprintf(key, sizeof key, "locales/%s/%s.json", lang, file);
  size_t len = 0;
  char* data = pak_read(key, &len);
  json_t* root = NULL;
  if (data) {
    json_error_t err;
    root = json_loadb(data, len, 0, &err);
    free(data);
  }
  if (g_nns < NS_MAX) {
    snprintf(g_ns[g_nns].lang, sizeof g_ns[g_nns].lang, "%s", lang);
    snprintf(g_ns[g_nns].ns, sizeof g_ns[g_nns].ns, "%s", ns);
    g_ns[g_nns].root = root;
    g_nns++;
  }
  return root;
}

/* "ns:a.b.c" -> the string, in lang */
static const char* lookup_in(const char* lang, const char* key) {
  char ns[48] = "menu";
  const char* path = key;
  const char* colon = strchr(key, ':');
  if (colon && colon - key < (int)sizeof ns) {
    memcpy(ns, key, colon - key);
    ns[colon - key] = 0;
    path = colon + 1;
  }
  json_t* node = ns_get(lang, ns);
  char part[96];
  while (node && *path) {
    const char* dot = strchr(path, '.');
    int len = dot ? (int)(dot - path) : (int)strlen(path);
    if (len >= (int)sizeof part) return NULL;
    memcpy(part, path, len);
    part[len] = 0;
    node = json_is_object(node) ? json_object_get(node, part) : NULL;
    path = dot ? dot + 1 : path + len;
  }
  return node && json_is_string(node) ? json_string_value(node) : NULL;
}

static const char* lookup(const char* key) {
  const char* s = lookup_in(g_lang, key);
  if (!s && strcmp(g_lang, "en")) {
    /* upstream fallbackLng: es-419 -> es-ES -> en, everything else -> en */
    if (!strcmp(g_lang, "es-419")) s = lookup_in("es-ES", key);
    if (!s) s = lookup_in("en", key);
  }
  return s;
}

const char* i18n_raw(const char* key) { return lookup(key); }
bool i18n_exists(const char* key) { return lookup(key) != NULL; }

typedef struct {
  const char* name[12];
  char value[12][128];
  int n;
} args_t;

static const char* arg_get(const args_t* a, const char* name, int len) {
  for (int i = 0; i < a->n; i++)
    if ((int)strlen(a->name[i]) == len && !strncmp(a->name[i], name, len)) return a->value[i];
  return NULL;
}

static void expand(const char* src, const args_t* a, char* out, int n, int depth);

static void put(char* out, int n, int* o, const char* s, int len) {
  for (int i = 0; i < len && *o < n - 1; i++) out[(*o)++] = s[i];
}

static void expand(const char* src, const args_t* a, char* out, int n, int depth) {
  int o = 0;
  for (const char* p = src; *p && o < n - 1;) {
    if (p[0] == '{' && p[1] == '{') {
      const char* end = strstr(p + 2, "}}");
      if (end) {
        const char* name = p + 2;
        while (*name == ' ') name++;
        const char* ne = name;
        while (ne < end && *ne != ',' && *ne != ' ') ne++;
        const char* v = arg_get(a, name, (int)(ne - name));
        if (v) put(out, n, &o, v, (int)strlen(v));
        p = end + 2;
        continue;
      }
    }
    if (p[0] == '$' && p[1] == 't' && p[2] == '(' && depth < 4) {
      const char* end = strchr(p + 3, ')');
      if (end) {
        char k[128];
        int len = (int)(end - (p + 3));
        const char* comma = memchr(p + 3, ',', len);
        if (comma) len = (int)(comma - (p + 3));
        if (len < (int)sizeof k) {
          memcpy(k, p + 3, len);
          k[len] = 0;
          const char* s = lookup(k);
          char sub[512];
          expand(s ? s : k, a, sub, sizeof sub, depth + 1);
          put(out, n, &o, sub, (int)strlen(sub));
        }
        p = end + 1;
        continue;
      }
    }
    out[o++] = *p++;
  }
  out[o] = 0;
}

const char* i18n_t(char* out, int n, const char* key, ...) {
  args_t a;
  a.n = 0;
  va_list ap;
  va_start(ap, key);
  const char* name;
  while ((name = va_arg(ap, const char*)) != NULL && a.n < 12) {
    const char* v = va_arg(ap, const char*);
    a.name[a.n] = name;
    snprintf(a.value[a.n], sizeof a.value[a.n], "%s", v ? v : "");
    a.n++;
  }
  va_end(ap);

  const char* s = NULL;
  char k[160];
  const char* ctx = arg_get(&a, "context", 7);
  const char* count = arg_get(&a, "count", 5);
  if (ctx && *ctx) {
    snprintf(k, sizeof k, "%s_%s", key, ctx);
    s = lookup(k);
  }
  if (!s && count) {
    snprintf(k, sizeof k, "%s_%s", key, atoi(count) == 1 ? "one" : "other");
    s = lookup(k);
  }
  if (!s) s = lookup(key);
  if (!s) {
    /* i18next shows the key without its namespace */
    const char* colon = strchr(key, ':');
    s = colon ? colon + 1 : key;
  }
  expand(s, &a, out, n, 0);
  return out;
}

const char* i18n_resolve(char* out, int n, const char* text) {
  args_t a;
  a.n = 0;
  expand(text ? text : "", &a, out, n, 0);
  return out;
}

/* ---- names ---- */
static char g_rot[8][96];
static int g_rot_i;
static char* rot(void) {
  g_rot_i = (g_rot_i + 1) & 7;
  return g_rot[g_rot_i];
}

const char* i18n_species_name(int id) {
  char k[96], c[64];
  snprintf(k, sizeof k, "pokemon:%s", gm_enum_camel(GM_E_SPECIES_ID, id, c, sizeof c));
  char* out = rot();
  return i18n_t(out, 96, k, NULL);
}

const char* i18n_move_name(int id) {
  return menu_move_name((uint16_t)id);
}

const char* i18n_ability_name(int id) {
  char k[96], c[64];
  snprintf(k, sizeof k, "ability:%s.name", gm_enum_camel(GM_E_ABILITY_ID, id, c, sizeof c));
  char* out = rot();
  return i18n_t(out, 96, k, NULL);
}

const char* i18n_type_name(int type) {
  char k[96], c[64];
  snprintf(k, sizeof k, "pokemonInfo:type.%s", gm_enum_camel(GM_E_POKEMON_TYPE, type, c, sizeof c));
  char* out = rot();
  return i18n_t(out, 96, k, NULL);
}

const char* i18n_stat_name(int stat) {
  char k[96], c[64];
  snprintf(k, sizeof k, "pokemonInfo:stat.%s", gm_enum_lower(GM_E_STAT, stat, c, sizeof c));
  char* out = rot();
  return i18n_t(out, 96, k, NULL);
}
