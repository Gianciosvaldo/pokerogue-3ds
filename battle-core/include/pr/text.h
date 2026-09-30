/* Battle message text: what upstream's MessagePhase and ui.showText would show.
 *
 * The engine never builds strings. Each message is a locale key plus its arguments
 * (pr_msg), kept as upstream's i18next.t(key, {...}) call has them, and pr_msg_format turns
 * it into text for one language. The locale files themselves (upstream's locales/<lang>/ JSON files)
 * are not in this repository: the game reads them from data.pak, the turn test from an
 * upstream checkout. The caller hands them over through pr_text_source.
 *
 * i18next rules followed: namespaces (the key's "ns:" prefix; file names are the kebab-case
 * namespace, or upstream's namespaceMap), dotted key paths, {{name}} interpolation with no
 * escaping, $t(key) nesting, plural suffixes (_one / _other for count), and the language
 * fallback the caller's source applies. Not done yet: the Korean postposition post-processor,
 * plural categories other than one/other. */
#ifndef PR_TEXT_H
#define PR_TEXT_H
#include <stddef.h>
#include <stdint.h>

/* What an argument's value is, which decides how it becomes text */
typedef enum {
    PR_ARG_NONE,
    PR_ARG_INT,            /* a number; the argument named "count" also picks the plural form */
    PR_ARG_POKEMON,        /* getPokemonNameWithAffix: "Wild X" / "Foe X" for enemies (v: pr_msg_pokemon) */
    PR_ARG_POKEMON_NAME,   /* the name alone, getNameToRender() (v: pr_msg_pokemon) */
    PR_ARG_MOVE,           /* a MoveId's name, with upstream's " (N)" / " (P)" unless v has PR_MSG_MOVE_BARE */
    PR_ARG_ABILITY,        /* an AbilityId's name */
    PR_ARG_TYPE,           /* a PokemonType's name */
    PR_ARG_STAT,           /* a Stat's name (getStatKey) */
    PR_ARG_STATS,          /* v: pr_msg_stats(); the names joined as upstream's stat change message does */
    PR_ARG_KEY,            /* another locale key, translated (s) */
    PR_ARG_STR,            /* text as it is (s): only for values upstream passes untranslated */
    PR_ARG_TEMPLATE,       /* text whose $t(key) segments are translated (s), as i18next nests a value
                            * after interpolating it: Trainer.getName()'s "$t(titles:gymLeader) $t(trainerNames:brock)" */
} pr_msg_arg_kind;

typedef struct {
    const char *name;      /* the {{name}} it fills */
    uint8_t kind;          /* pr_msg_arg_kind */
    uint32_t v;
    const char *s;         /* PR_ARG_KEY / PR_ARG_STR / PR_ARG_TEMPLATE: a string that outlives the message */
} pr_msg_arg;

#define PR_MSG_MAX_ARGS 5

/* One message. key == NULL: its text isn't ported yet (args[0].s may name the engine source
 * line that made it); key "": upstream's empty text box. */
typedef struct {
    const char *key;       /* "battle:useMove"; a string literal */
    uint8_t n_args;
    pr_msg_arg args[PR_MSG_MAX_ARGS];
} pr_msg;

/* A Pokemon as a message names it, fixed when the message is made (upstream builds the
 * string then): species, form, and whether it is an enemy and which affix it gets. */
#define PR_MSG_POKEMON_ENEMY  0x01u   /* isEnemy(): gets an affix */
#define PR_MSG_POKEMON_FOE    0x02u   /* hasTrainer() or the final boss: "Foe", else "Wild" */
static inline uint32_t pr_msg_pokemon(uint16_t species, uint8_t form, uint8_t flags)
{
    return (uint32_t)species << 16 | (uint32_t)form << 8 | flags;
}

#define PR_MSG_MOVE_BARE 0x80000000u

/* Stats in order for PR_ARG_STATS: up to 7, 4 bits each, the count in the top 4 bits */
static inline uint32_t pr_msg_stats(const int8_t *stats, int n)
{
    uint32_t v = (uint32_t)(n > 7 ? 7 : n) << 28;
    for (int i = 0; i < n && i < 7; i++) v |= (uint32_t)(stats[i] & 15) << (4 * i);
    return v;
}

/* Where the text comes from. get() returns the JSON text of one locale file (file "battle"
 * for keys "battle:..."): fallback 0 in the player's language, 1, 2... in the languages
 * i18next falls back to for a key it lacks (es-419: es-ES, then en; others: en), NULL past
 * the last. The returned text must stay valid while formatting. */
typedef struct {
    const char *(*get)(void *ctx, const char *file, int fallback, size_t *len);
    void *ctx;
    const char *lang;      /* for plural forms; NULL = "en" */
} pr_text_source;

/* The message as text, NUL-terminated in out (cut to cap - 1 bytes). Returns the length it
 * needed. A key the locale lacks comes out as the key without its namespace, as i18next does. */
size_t pr_msg_format(const pr_msg *m, const pr_text_source *src, char *out, size_t cap);

/* The raw string at key ("battle:useMove", "move:tackle.name") with no interpolation, unescaped
 * into out. Returns its length, or 0 when missing. */
size_t pr_text_lookup(const pr_text_source *src, const char *key, char *out, size_t cap);

/* upstream's file name for a namespace ("abilityTriggers" -> "ability-trigger") */
void pr_text_ns_file(const char *ns, char *out, size_t cap);

#endif
