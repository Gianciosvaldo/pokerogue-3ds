/* Battle message text (include/pr/text.h): i18next's t() for the keys the battle uses, over
 * upstream's own locale files, which the caller provides. */
#include "pr/text.h"
#include "pr/battle.h"
#include <stdio.h>
#include <string.h>

/* ---- output with truncation: n counts what the full text needs ---- */
typedef struct { char *s; size_t cap, n; } obuf;

static void put(obuf *o, const char *s, size_t n)
{
    for (size_t i = 0; i < n; i++, o->n++)
        if (o->s && o->n + 1 < o->cap) o->s[o->n] = s[i];
}
static void puts_(obuf *o, const char *s) { put(o, s, strlen(s)); }
static void end(obuf *o)
{
    if (o->s && o->cap) o->s[o->n < o->cap ? o->n : o->cap - 1] = 0;
}

/* ---- JSON: find a dotted key path in an object, read a string ---- */
static const char *ws(const char *p, const char *e)
{
    while (p < e && (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t')) p++;
    return p;
}

static const char *skip_string(const char *p, const char *e)
{
    for (p++; p < e && *p != '"'; p++)
        if (*p == '\\') p++;
    return p < e ? p + 1 : e;
}

static const char *skip_value(const char *p, const char *e)
{
    p = ws(p, e);
    if (p >= e) return e;
    if (*p == '"') return skip_string(p, e);
    if (*p == '{' || *p == '[') {
        int depth = 0;
        for (; p < e; p++) {
            if (*p == '"') { p = skip_string(p, e) - 1; continue; }
            if (*p == '{' || *p == '[') depth++;
            else if ((*p == '}' || *p == ']') && --depth == 0) return p + 1;
        }
        return e;
    }
    while (p < e && *p != ',' && *p != '}' && *p != ']') p++;
    return p;
}

/* The value of member `name` in the object at p, or NULL */
static const char *member(const char *p, const char *e, const char *name, size_t nlen)
{
    p = ws(p, e);
    if (p >= e || *p != '{') return NULL;
    p++;
    for (;;) {
        p = ws(p, e);
        if (p >= e || *p != '"') return NULL;
        const char *k = p + 1, *ke = skip_string(p, e) - 1;
        p = ws(ke + 1, e);
        if (p >= e || *p != ':') return NULL;
        p = ws(p + 1, e);
        if ((size_t)(ke - k) == nlen && !memcmp(k, name, nlen)) return p;
        p = ws(skip_value(p, e), e);
        if (p >= e || *p != ',') return NULL;
        p++;
    }
}

/* the string at a dotted path, as a pointer to its opening quote */
static const char *find_path(const char *json, size_t len, const char *path)
{
    const char *p = json, *e = json + len;
    for (;;) {
        const char *dot = strchr(path, '.');
        size_t n = dot ? (size_t)(dot - path) : strlen(path);
        p = member(p, e, path, n);
        if (!p) return NULL;
        if (!dot) return *p == '"' ? p : NULL;
        path = dot + 1;
    }
}

static void put_utf8(obuf *o, unsigned c)
{
    char b[4];
    if (c < 0x80) { b[0] = (char)c; put(o, b, 1); }
    else if (c < 0x800) { b[0] = (char)(0xC0 | c >> 6); b[1] = (char)(0x80 | (c & 63)); put(o, b, 2); }
    else if (c < 0x10000) {
        b[0] = (char)(0xE0 | c >> 12); b[1] = (char)(0x80 | (c >> 6 & 63)); b[2] = (char)(0x80 | (c & 63));
        put(o, b, 3);
    } else {
        b[0] = (char)(0xF0 | c >> 18); b[1] = (char)(0x80 | (c >> 12 & 63));
        b[2] = (char)(0x80 | (c >> 6 & 63)); b[3] = (char)(0x80 | (c & 63));
        put(o, b, 4);
    }
}


/* getFusedSpeciesName uses JS UTF-16 slices, including its literal character
 * class and match-array-length slice quirk. Work in code units, not UTF-8 bytes. */
typedef struct { uint16_t c[256]; size_t n; } name_units;
static int name_decode(const char *s, name_units *u)
{
    u->n = 0;
    while (*s) {
        unsigned c = (unsigned char)*s++, k = 0;
        if (c >= 0xf0) { c &= 7; k = 3; }
        else if (c >= 0xe0) { c &= 15; k = 2; }
        else if (c >= 0xc0) { c &= 31; k = 1; }
        else if (c >= 0x80) return 0;
        while (k--) {
            unsigned d = (unsigned char)*s++;
            if ((d & 0xc0) != 0x80) return 0;
            c = (c << 6) | (d & 63);
        }
        if (c > 0x10ffff || u->n + (c > 0xffff ? 2 : 1) > 255) return 0;
        if (c > 0xffff) {
            c -= 0x10000; u->c[u->n++] = (uint16_t)(0xd800 + (c >> 10));
            c = 0xdc00 + (c & 1023);
        }
        u->c[u->n++] = (uint16_t)c;
    }
    return 1;
}
static name_units name_slice(const name_units *u, size_t start, size_t end)
{
    name_units v = {{0}, 0};
    if (start > u->n) start = u->n;
    if (end > u->n) end = u->n;
    if (end > start) { v.n = end - start; memcpy(v.c, u->c + start, v.n * sizeof *v.c); }
    return v;
}
static unsigned name_lower(unsigned c)
{
    if ((c >= 'A' && c <= 'Z') || (c >= 0xc0 && c <= 0xd6) || (c >= 0xd8 && c <= 0xde)) return c + 32;
    return c;
}
static int name_letter(unsigned c) { c = name_lower(c); return c >= 'a' && c <= 'z'; }
static int name_vowel(unsigned c)
{
    c = name_lower(c);
    return c < 128 && strchr("aeiou(?:y$)-'", (int)c) != NULL;
}
/* Leftmost two ASCII letters; lazy middle; A's final class is greedy. */
static int name_match(const name_units *u, int a, size_t *start, size_t *end)
{
    for (size_t i = 0; i + 2 < u->n; i++) {
        if (!name_letter(u->c[i]) || !name_letter(u->c[i + 1])) continue;
        for (size_t j = i + 2; j < u->n; j++) if (name_vowel(u->c[j])) {
            *start = i; *end = j + 1;
            if (a) while (*end < u->n && name_vowel(u->c[*end])) (*end)++;
            return 1;
        }
    }
    return 0;
}
static void name_affixes(name_units *u, name_units *prefix, name_units *suffix)
{
    prefix->n = suffix->n = 0;
    for (size_t i = 1; i < u->n; i++) if (u->c[i] == ' ') {
        /* ^[^ ]+ does not match a leading space. */
        if (u->c[0] != ' ') { *prefix = name_slice(u, 0, i + 1); *u = name_slice(u, i + 1, u->n); }
        break;
    }
    for (size_t i = u->n; i > 0; i--) if (u->c[i - 1] == ' ') {
        if (i < u->n) { *suffix = name_slice(u, i - 1, u->n); u->n = i - 1; }
        break;
    }
}
static void name_put(obuf *o, const name_units *u)
{
    for (size_t i = 0; i < u->n; i++) {
        unsigned c = u->c[i];
        if (c >= 0xd800 && c <= 0xdbff && i + 1 < u->n && u->c[i+1] >= 0xdc00 && u->c[i+1] <= 0xdfff) {
            c = 0x10000 + ((c - 0xd800) << 10) + u->c[++i] - 0xdc00;
        } else if (c >= 0xd800 && c <= 0xdfff) c = 0xfffd;
        put_utf8(o, c);
    }
}
size_t pr_fused_species_name(const char *a, const char *b, char *out, size_t cap)
{
    obuf o = {out, cap, 0};
    name_units A, B, ap, bp, as, bs, fa, fb;
    if (!a || !b || !name_decode(a, &A) || !name_decode(b, &B)) { end(&o); return 0; }
    name_affixes(&A, &ap, &as); name_affixes(&B, &bp, &bs);
    size_t sa = 0, ea = 0, sb = 0, eb = 0;
    int ma = name_match(&A, 1, &sa, &ea), mb = name_match(&B, 0, &sb, &eb);
    int multi_a = 0, multi_b = 0;
    for (size_t i = 0; i < A.n; i++) if (A.c[i] == ' ') multi_a = 1;
    for (size_t i = 0; i < B.n; i++) if (B.c[i] == ' ') multi_b = 1;
    fa = multi_a || !ma ? A : name_slice(&A, sa, ea);
    if (multi_b) {
        size_t i = B.n; while (i && B.c[i - 1] != ' ') i--;
        fb = name_slice(&B, i, B.n);
    } else if (mb) {
        name_units prev = name_slice(&B, sb + 2, eb);
        int hyphen = 0;
        for (size_t i = 0; i < prev.n; i++) if (prev.c[i] == '-' || prev.c[i] == '\'') hyphen = 1;
        fb = name_slice(&B, eb, B.n);
        if (hyphen) {
            memmove(fb.c + prev.n, fb.c, fb.n * sizeof *fb.c);
            memcpy(fb.c, prev.c, prev.n * sizeof *fb.c); fb.n += prev.n;
        }
        if (!fb.n) fb = prev;
        /* For multiword A, this comparison uses its final word, which has
         * the same final code unit as the later reconstructed full name. */
        if (fa.n && fb.n && fa.c[fa.n - 1] == fb.c[0]) {
            unsigned last = fa.c[fa.n - 1];
            if (last == 'a' || last == 'i' || last == 'u') fb = name_slice(&fb, 1, fb.n);
            else {
                size_t i = 0; while (i < fb.n && fb.c[i] == last) i++;
                if (i && i < fb.n) fb = name_slice(&fb, i, fb.n);
            }
        }
    } else fb = B;
    if (fb.n) fb.c[0] = (uint16_t)name_lower(fb.c[0]);
    if (fa.n == 4 && fa.c[0] == 'R' && fa.c[1] == 'a' && fa.c[2] == 'p' && fa.c[3] == 'i') {
        fa.c[fa.n++] = 'd';
        if (fb.n == 2 && fb.c[0] == 'n' && fb.c[1] == 'g') fb = name_slice(&B, B.n > 3 ? B.n - 3 : 0, B.n);
    }
    name_put(&o, ap.n ? &ap : &bp); name_put(&o, &fa); name_put(&o, &fb); name_put(&o, bs.n ? &bs : &as);
    end(&o); return o.n;
}

static unsigned hex4(const char *p)
{
    unsigned v = 0;
    for (int i = 0; i < 4; i++) {
        char c = p[i];
        v = v * 16 + (unsigned)(c >= 'a' ? c - 'a' + 10 : c >= 'A' ? c - 'A' + 10 : c - '0');
    }
    return v;
}

/* the JSON string at p (its opening quote), unescaped */
static void unescape(obuf *o, const char *p, const char *e)
{
    for (p++; p < e && *p != '"'; p++) {
        if (*p != '\\') { put(o, p, 1); continue; }
        if (++p >= e) break;
        switch (*p) {
        case 'n': put(o, "\n", 1); break;
        case 't': put(o, "\t", 1); break;
        case 'r': put(o, "\r", 1); break;
        case 'b': put(o, "\b", 1); break;
        case 'f': put(o, "\f", 1); break;
        case 'u': {
            if (e - p < 5) return;
            unsigned c = hex4(p + 1);
            p += 4;
            if (c >= 0xD800 && c < 0xDC00 && e - p >= 7 && p[1] == '\\' && p[2] == 'u') {
                unsigned lo = hex4(p + 3);
                if (lo >= 0xDC00 && lo < 0xE000) { c = 0x10000 + ((c - 0xD800) << 10) + (lo - 0xDC00); p += 6; }
            }
            put_utf8(o, c);
            break;
        }
        default: put(o, p, 1); break;   /* \" \\ \/ */
        }
    }
}

/* ---- namespaces and keys ---- */
void pr_text_ns_file(const char *ns, char *out, size_t cap)
{
    /* src/i18n-namespace-map.ts */
    static const char *const map[][2] = {
        {"titles", "trainer-titles"}, {"moveTriggers", "move-trigger"}, {"abilityTriggers", "ability-trigger"},
        {"battlePokemonForm", "pokemon-form-battle"}, {"miscDialogue", "dialogue-misc"},
        {"battleSpecDialogue", "dialogue-final-boss"}, {"doubleBattleDialogue", "dialogue-double-battle"},
        {"splashMessages", "splash-texts"}, {"mysteryEncounterMessages", "mystery-encounter-texts"}, {"biome", "biomes"},
    };
    for (size_t i = 0; i < sizeof map / sizeof map[0]; i++)
        if (!strcmp(ns, map[i][0])) { snprintf(out, cap, "%s", map[i][1]); return; }
    /* toKebabCase: a word starts at each capital after a lower-case letter or digit */
    obuf o = {out, cap, 0};
    for (const char *p = ns; *p; p++) {
        char c = *p;
        if (c >= 'A' && c <= 'Z') {
            if (p > ns && ((p[-1] >= 'a' && p[-1] <= 'z') || (p[-1] >= '0' && p[-1] <= '9'))) put(&o, "-", 1);
            c = (char)(c - 'A' + 'a');
        }
        put(&o, &c, 1);
    }
    end(&o);
}

/* the string at "ns:path" through the source's languages, as a pointer to its opening quote */
static const char *find_key(const pr_text_source *src, const char *key, const char **json_end)
{
    const char *colon = strchr(key, ':');
    char ns[48], file[64];
    const char *path = key;
    if (colon && (size_t)(colon - key) < sizeof ns) {
        memcpy(ns, key, (size_t)(colon - key));
        ns[colon - key] = 0;
        path = colon + 1;
    } else {
        snprintf(ns, sizeof ns, "menu");   /* defaultNS */
    }
    pr_text_ns_file(ns, file, sizeof file);
    for (int fb = 0; src && src->get && fb < 4; fb++) {
        size_t len = 0;
        const char *json = src->get(src->ctx, file, fb, &len);
        if (!json) break;
        const char *s = find_path(json, len, path);
        if (s) { *json_end = json + len; return s; }
    }
    return NULL;
}

static bool key_exists(const pr_text_source *src, const char *key)
{
    const char *e;
    return find_key(src, key, &e) != NULL;
}

size_t pr_text_lookup(const pr_text_source *src, const char *key, char *out, size_t cap)
{
    obuf o = {out, cap, 0};
    const char *e, *s = find_key(src, key, &e);
    if (s) unescape(&o, s, e);
    end(&o);
    return o.n;
}

/* i18next's plural suffix (Intl.PluralRules, cardinal) for the languages upstream ships;
 * one/other only for now */
static const char *plural_suffix(const pr_text_source *src, long n)
{
    const char *lang = src && src->lang ? src->lang : "en";
    bool zero_one = !strncmp(lang, "fr", 2) || !strncmp(lang, "pt-BR", 5);
    bool none = !strncmp(lang, "ja", 2) || !strncmp(lang, "ko", 2) || !strncmp(lang, "zh", 2);
    if (none) return "other";
    if (n == 1 || (zero_one && n == 0)) return "one";
    return "other";
}

static const pr_msg_arg *find_arg(const pr_msg *m, const char *name, size_t n)
{
    for (int i = 0; i < m->n_args; i++)
        if (m->args[i].name && strlen(m->args[i].name) == n && !memcmp(m->args[i].name, name, n)) return &m->args[i];
    return NULL;
}

/* ---- names ---- */
static const pr_species *species_by_id(uint16_t id)
{
    int lo = 0, hi = PR_SPECIES_COUNT - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (pr_species_table[mid].id == id) return &pr_species_table[mid];
        if (pr_species_table[mid].id < id) lo = mid + 1; else hi = mid - 1;
    }
    return NULL;
}

static void format_key(obuf *o, const char *key, const pr_msg *args, const pr_text_source *src, int depth);

/* what i18next shows for a key it lacks: the key without its namespace */
static const char *missing_key(const char *key)
{
    const char *c = strchr(key, ':');
    return c ? c + 1 : key;
}

static void lookup_into(obuf *o, const pr_text_source *src, const char *key)
{
    const char *e, *s = find_key(src, key, &e);
    if (s) unescape(o, s, e);
    else puts_(o, missing_key(key));
}

/* PokemonSpecies.getName(formIndex): the species name, wrapped for Mega, Primal, G-Max and
 * E-Max forms (battlePokemonForm:<form key>) */
static void species_name(obuf *o, const pr_text_source *src, uint16_t id, uint8_t form)
{
    const pr_species *sp = species_by_id(id);
    if (!sp) { puts_(o, "MissingNo."); return; }
    char key[96], name[128];
    snprintf(key, sizeof key, "pokemon:%s", pr_species_i18n[sp - pr_species_table]);
    obuf nb = {name, sizeof name, 0};
    lookup_into(&nb, src, key);
    end(&nb);
    const char *wrap = NULL;
    if (sp->nforms && form < sp->nforms) {
        const char *fk = pr_forms[sp->form0 + form].form_key;
        if (!fk) fk = "";
        if (!strcmp(fk, "mega")) wrap = "mega";
        else if (!strcmp(fk, "primal")) wrap = "primal";
        else if (!strcmp(fk, "eternamax")) wrap = "eternamax";
        else if (!strcmp(fk, "mega-x")) wrap = "megaX";
        else if (!strcmp(fk, "mega-y")) wrap = "megaY";
        else if (!strcmp(fk, "mega-z")) wrap = "megaZ";
        else if (!strcmp(fk, "mega-original") || !strcmp(fk, "mega-curly") || !strcmp(fk, "mega-droopy")
                 || !strcmp(fk, "mega-stretchy")) wrap = "mega";
        else if (strstr(fk, "gigantamax")) wrap = "gigantamax";
    }
    if (!wrap) { puts_(o, name); return; }
    pr_msg m = {0};
    m.n_args = 1;
    m.args[0] = (pr_msg_arg){"pokemonName", PR_ARG_STR, 0, name, 0};
    snprintf(key, sizeof key, "battlePokemonForm:%s", wrap);
    format_key(o, key, &m, src, 1);
}

/* getPokemonNameWithAffix */
static void pokemon_name(obuf *o, const pr_text_source *src, uint32_t v, uint32_t fusion_v, bool affix)
{
    uint16_t species = (uint16_t)(v >> 16);
    uint8_t form = (uint8_t)(v >> 8), flags = (uint8_t)v;
    if (!fusion_v && (!affix || !(flags & PR_MSG_POKEMON_ENEMY))) { species_name(o, src, species, form); return; }
    char name[1024];
    obuf nb = {name, sizeof name, 0};
    species_name(&nb, src, species, form);
    end(&nb);
    if (fusion_v >> 16) {
        char second[512], fused[1024];
        obuf fb = {second, sizeof second, 0};
        species_name(&fb, src, (uint16_t)(fusion_v >> 16), (uint8_t)(fusion_v >> 8)); end(&fb);
        pr_fused_species_name(name, second, fused, sizeof fused);
        snprintf(name, sizeof name, "%s", fused);
    }
    if (!affix || !(flags & PR_MSG_POKEMON_ENEMY)) { puts_(o, name); return; }
    pr_msg m = {0};
    m.n_args = 1;
    m.args[0] = (pr_msg_arg){"pokemonName", PR_ARG_STR, 0, name, 0};
    format_key(o, (flags & PR_MSG_POKEMON_FOE) ? "battle:foePokemonWithAffix" : "battle:wildPokemonWithAffix", &m, src, 1);
}

static void stat_name(obuf *o, const pr_text_source *src, int stat)
{
    char key[64];
    snprintf(key, sizeof key, "pokemonInfo:stat.%s", stat >= 0 && stat < 8 ? pr_i18n_stat[stat] : "?");
    lookup_into(o, src, key);
}

static void format_key(obuf *o, const char *key, const pr_msg *m, const pr_text_source *src, int depth);

/* $t(key) at p: formats key into o and returns the text after it; NULL when p isn't one */
static const char *nest_at(obuf *o, const char *p, const pr_msg *m, const pr_text_source *src, int depth)
{
    if (depth >= 4 || p[0] != '$' || p[1] != 't' || p[2] != '(') return NULL;
    const char *close = strchr(p + 3, ')');
    if (!close) return NULL;
    char nk[128];
    size_t n = (size_t)(close - (p + 3));
    const char *comma = memchr(p + 3, ',', n);
    if (comma) n = (size_t)(comma - (p + 3));
    if (n >= sizeof nk) return NULL;
    memcpy(nk, p + 3, n);
    nk[n] = 0;
    format_key(o, nk, m, src, depth + 1);
    return close + 1;
}

static void put_arg(obuf *o, const pr_msg_arg *a, const pr_msg *m, const pr_text_source *src, int depth)
{
    char key[96];
    switch (a->kind) {
    case PR_ARG_MONEY_EN_US: {
        uint64_t bits = ((uint64_t)a->fusion_v << 32) | a->v;
        double value;
        char b[416];
        memcpy(&value, &bits, sizeof value);
        pr_money_en_us(value, b, sizeof b);
        puts_(o, b);
        break;
    }
    case PR_ARG_INT64: {
        char b[32];
        int64_t value = (int64_t)(((uint64_t)a->fusion_v << 32) | a->v);
        snprintf(b, sizeof b, "%lld", (long long)value);
        puts_(o, b);
        break;
    }
    case PR_ARG_INT: {
        char b[16];
        snprintf(b, sizeof b, "%ld", (long)(int32_t)a->v);
        puts_(o, b);
        break;
    }
    case PR_ARG_POKEMON: pokemon_name(o, src, a->v, a->fusion_v, true); break;
    case PR_ARG_POKEMON_NAME: pokemon_name(o, src, a->v, a->fusion_v, false); break;
    case PR_ARG_MOVE: {
        uint32_t id = a->v & ~PR_MSG_MOVE_BARE;
        if (!id || id >= PR_MOVE_COUNT) break;   /* MoveId.NONE's name is "" */
        snprintf(key, sizeof key, "move:%s.name", pr_move_text[id].key);
        lookup_into(o, src, key);
        if (!(a->v & PR_MSG_MOVE_BARE)) puts_(o, pr_move_text[id].name_append);
        break;
    }
    case PR_ARG_ABILITY:
        if (!a->v || a->v >= PR_ABILITY_COUNT) break;   /* AbilityId.NONE's name is "" */
        snprintf(key, sizeof key, "ability:%s.name", pr_abilities[a->v].key);
        lookup_into(o, src, key);
        if (pr_abilities[a->v].flags & (1u << 5)) puts_(o, " (N)");
        else if (pr_abilities[a->v].flags & (1u << 6)) puts_(o, " (P)");
        break;
    case PR_ARG_TYPE: {
        int t = (int)(int32_t)a->v;
        snprintf(key, sizeof key, "pokemonInfo:type.%s", t >= -1 && t < 19 ? pr_i18n_pokemon_type[t + 1] : "?");
        lookup_into(o, src, key);
        break;
    }
    case PR_ARG_STAT: stat_name(o, src, (int)a->v); break;
    case PR_ARG_STATS: {
        /* StatStageChangePhase.formatStatsFragment */
        int n = (int)(a->v >> 28);
        if (n >= 5) { lookup_into(o, src, "battle:stats"); break; }
        for (int i = 0; i < n; i++) {
            if (i && i == n - 1) {
                if (n > 2) puts_(o, ",");
                puts_(o, " ");
                lookup_into(o, src, "battle:statsAnd");
                puts_(o, " ");
            } else if (i) {
                puts_(o, ", ");
            }
            stat_name(o, src, (int)(a->v >> (4 * i) & 15));
        }
        break;
    }
    case PR_ARG_KEY: if (a->s) lookup_into(o, src, a->s); break;
    case PR_ARG_STR: if (a->s) puts_(o, a->s); break;
    case PR_ARG_TEMPLATE:
        /* i18next nests after interpolating, so $t() in an argument's value is translated too */
        for (const char *p = a->s; p && *p;) {
            const char *after = nest_at(o, p, m, src, depth);
            if (after) { p = after; continue; }
            put(o, p, 1);
            p++;
        }
        break;
    default: break;
    }
}

/* t(key, args): the plural form for count, $t() nesting, then {{name}} interpolation */
static void format_key(obuf *o, const char *key, const pr_msg *m, const pr_text_source *src, int depth)
{
    char pk[128];
    const char *use = key;
    const pr_msg_arg *count = find_arg(m, "count", 5);
    if (count) {
        int64_t n = count->kind==PR_ARG_INT64 ? (int64_t)(((uint64_t)count->fusion_v<<32)|count->v) : (int32_t)count->v;
        if (n == 0) {
            snprintf(pk, sizeof pk, "%s_zero", key);
            if (key_exists(src, pk)) use = pk;
        }
        if (use == key) {
            snprintf(pk, sizeof pk, "%s_%s", key, plural_suffix(src, n));
            if (key_exists(src, pk)) use = pk;
        }
    }
    char tmpl[1024];
    obuf tb = {tmpl, sizeof tmpl, 0};
    const char *e, *s = find_key(src, use, &e);
    if (!s) { puts_(o, missing_key(key)); return; }
    unescape(&tb, s, e);
    end(&tb);
    for (const char *p = tmpl; *p;) {
        const char *after = nest_at(o, p, m, src, depth);
        if (after) { p = after; continue; }
        if (p[0] == '{' && p[1] == '{') {
            const char *close = strstr(p + 2, "}}");
            if (close) {
                const char *a = p + 2, *b = close;
                const char *comma = memchr(a, ',', (size_t)(b - a));
                const char *ne = comma ? comma : b;
                while (a < ne && *a == ' ') a++;
                while (ne > a && ne[-1] == ' ') ne--;
                const pr_msg_arg *arg = find_arg(m, a, (size_t)(ne - a));
                if (arg) put_arg(o, arg, m, src, depth);
                else put(o, p, (size_t)(close + 2 - p));   /* skipOnVariables: a missing value stays as it is */
                p = close + 2;
                continue;
            }
        }
        put(o, p, 1);
        p++;
    }
}

size_t pr_msg_format(const pr_msg *m, const pr_text_source *src, char *out, size_t cap)
{
    obuf o = {out, cap, 0};
    if (m && m->key && m->key[0]) format_key(&o, m->key, m, src, 0);   /* key "": upstream's "" message */
    end(&o);
    return o.n;
}
