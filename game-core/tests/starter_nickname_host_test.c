/* A starter's nickname kept in the system profile across runs (upstream: StarterPreferences.nickname, set by the starter select's Rename, given to the
 * Pokemon by SelectStarterPhase, kept in the starter preferences of the save). Needs no upstream checkout.
 *  - the table: sets, replaces (a shorter name leaves no tail of the longer one), clears (the profile is byte for byte the one that never had it), stays
 *    sorted, one entry per species, refuses a species that is not a starter, text gc_nickname_valid refuses, and a new nickname when the table is full
 *    (replacing or clearing one still works), each time without changing anything;
 *  - the save: a profile with no nickname has no section (the bytes an older build wrote), the nicknames come back as they were and the rest of the
 *    profile with them, a save from before them loads with none;
 *  - damaged data: a nickname that is not text, not a starter's, empty, a duplicate, past the table or in a record the build cannot read is dropped and
 *    the profile (and its other nicknames) loads; a flipped byte still refuses the whole save. */
#include "../src/internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static gc_system sys, back, before;
static uint8_t buf[1 << 20], buf0[1 << 20], buf2[1 << 20];
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr, "starter nickname FAIL line %d: %s\n", __LINE__, #x); exit(1); } } while (0)

enum { BULBASAUR = 1, IVYSAUR = 2, CHARMANDER = 4, SQUIRTLE = 7, PIKACHU = 25 };
#define HIRAGANA "\xE3\x81\x82"
#define FIRE "\xF0\x9F\x94\xA5"

static const char *LONGEST = HIRAGANA HIRAGANA HIRAGANA HIRAGANA HIRAGANA HIRAGANA HIRAGANA HIRAGANA HIRAGANA HIRAGANA
                             HIRAGANA HIRAGANA HIRAGANA HIRAGANA HIRAGANA HIRAGANA HIRAGANA HIRAGANA HIRAGANA HIRAGANA;   /* 20 characters, 60 bytes */
static const char *TEN_FIRES = FIRE FIRE FIRE FIRE FIRE FIRE FIRE FIRE FIRE FIRE;   /* 20 UTF-16 units, 40 bytes */

static uint32_t fnv32(const char *s)
{
    uint32_t h = 2166136261u;
    while (*s) { h ^= (uint8_t)*s++; h *= 16777619u; }
    return h;
}
static uint32_t crc32_of(const uint8_t *p, size_t n)
{
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < n; i++) {
        c ^= p[i];
        for (int k = 0; k < 8; k++) c = c >> 1 ^ (0xEDB88320u & (0u - (c & 1)));
    }
    return ~c;
}
static bool contains(const uint8_t *p, size_t n, const void *pat, size_t m)
{
    for (size_t i = 0; i + m <= n; i++) if (!memcmp(p + i, pat, m)) return true;
    return false;
}
/* does the save carry the section (its name's hash, then a length, at a section boundary: a 4-byte match elsewhere is a 1 in 2^32 chance per offset) */
static bool has_section(const uint8_t *p, size_t n)
{
    const uint32_t h = fnv32("starter_nicknames");
    return contains(p + 20, n - 20, &h, 4);
}

/* the table's own rules: in use first, sorted, one entry per species, text never empty, nothing after the terminator, every species a starter */
static void check_table(const gc_system *s)
{
    CHECK(s->n_starter_nicknames >= 0 && s->n_starter_nicknames <= GC_STARTER_NICKNAMES);
    for (int i = 0; i < GC_STARTER_NICKNAMES; i++) {
        const gc_starter_nickname *e = &s->starter_nicknames[i];
        if (i >= s->n_starter_nicknames) {
            static const gc_starter_nickname none;
            CHECK(!memcmp(e, &none, sizeof none));
            continue;
        }
        CHECK(e->text[0] && gc_starter_cost(e->species) >= 0 && gc_nickname_valid(e->text));
        if (i) CHECK(s->starter_nicknames[i - 1].species < e->species);
        for (size_t k = strlen(e->text); k < sizeof e->text; k++) CHECK(e->text[k] == 0);
    }
}

static size_t save_profile(const gc_system *s, uint8_t *to)
{
    const size_t n = gc_system_save(s, to, sizeof buf);
    CHECK(n > 20);
    return n;
}

static int starter_ids[1024], n_starters;
static void collect_starters(void)
{
    n_starters = gc_starter_cost_count;
    CHECK(n_starters > GC_STARTER_NICKNAMES + 1 && n_starters <= 1024);   /* there are more starters than the table holds */
    for (int i = 0; i < n_starters; i++) starter_ids[i] = gc_starter_costs[i].id;
}

static void layout_cases(void)
{
    /* the memory a profile pays for it: every gc_system in the app (and in gc_classic.c, gc_gacha.c, gc_achv.c, gacha_host.c) carries the table; the
     * fields are 8,196 bytes, sizeof(gc_system) grew by 8,192 (the count sits in padding) */
    CHECK(sizeof(gc_starter_nickname) == 64);
    CHECK(sizeof sys.starter_nicknames == 128 * 64 && sizeof sys.starter_nicknames + sizeof sys.n_starter_nicknames == 8196);
    /* the longest text there is fits with its terminator */
    CHECK(gc_nickname_valid(LONGEST) && strlen(LONGEST) == 60 && strlen(LONGEST) < GC_STARTER_NICKNAME_BYTES);
    CHECK(gc_nickname_valid(TEN_FIRES) && strlen(TEN_FIRES) == 40);
    CHECK(gc_starter_cost(IVYSAUR) < 0 && gc_starter_cost(BULBASAUR) >= 0 && gc_starter_cost(CHARMANDER) >= 0 && gc_starter_cost(SQUIRTLE) >= 0);
    CHECK(gc_starter_cost(PIKACHU) >= 0);
}

static void table_cases(void)
{
    gc_system_init_defaults(&sys);
    check_table(&sys);
    CHECK(sys.n_starter_nicknames == 0);
    for (int i = 0; i < n_starters; i++) CHECK(!*gc_system_starter_nickname(&sys, (uint16_t)starter_ids[i]));
    CHECK(!*gc_system_starter_nickname(NULL, BULBASAUR) && !*gc_system_starter_nickname(&sys, 0) && !*gc_system_starter_nickname(&sys, 65535));
    static gc_system fresh;
    gc_system_init_defaults(&fresh);
    CHECK(!memcmp(&sys, &fresh, sizeof sys));

    /* set in an order that is not the table's: it comes out sorted, each name on its own species */
    CHECK(gc_system_set_starter_nickname(&sys, SQUIRTLE, "Shell") == GC_STARTER_NICK_OK);
    CHECK(gc_system_set_starter_nickname(&sys, BULBASAUR, "Sparky") == GC_STARTER_NICK_OK);
    CHECK(gc_system_set_starter_nickname(&sys, PIKACHU, FIRE "Blaze" FIRE) == GC_STARTER_NICK_OK);
    CHECK(gc_system_set_starter_nickname(&sys, CHARMANDER, LONGEST) == GC_STARTER_NICK_OK);
    check_table(&sys);
    CHECK(sys.n_starter_nicknames == 4);
    CHECK(sys.starter_nicknames[0].species == BULBASAUR && sys.starter_nicknames[1].species == CHARMANDER && sys.starter_nicknames[2].species == SQUIRTLE &&
          sys.starter_nicknames[3].species == PIKACHU);
    CHECK(!strcmp(gc_system_starter_nickname(&sys, BULBASAUR), "Sparky") && !strcmp(gc_system_starter_nickname(&sys, SQUIRTLE), "Shell"));
    CHECK(!strcmp(gc_system_starter_nickname(&sys, PIKACHU), FIRE "Blaze" FIRE) && !strcmp(gc_system_starter_nickname(&sys, CHARMANDER), LONGEST));
    CHECK(!*gc_system_starter_nickname(&sys, IVYSAUR) && !*gc_system_starter_nickname(&sys, 3) && !*gc_system_starter_nickname(&sys, 5));   /* the neighbours have none */

    /* replaced: one entry still, no tail of the longer name (the save and the comparisons read every byte) */
    CHECK(gc_system_set_starter_nickname(&sys, CHARMANDER, "Em") == GC_STARTER_NICK_OK);
    check_table(&sys);
    CHECK(sys.n_starter_nicknames == 4 && !strcmp(gc_system_starter_nickname(&sys, CHARMANDER), "Em"));
    CHECK(gc_system_set_starter_nickname(&sys, CHARMANDER, "Em") == GC_STARTER_NICK_OK && sys.n_starter_nicknames == 4);   /* the same again */
    /* the text is text, not a template */
    CHECK(gc_system_set_starter_nickname(&sys, CHARMANDER, "{{pokemonName}} 100%") == GC_STARTER_NICK_OK);
    CHECK(!strcmp(gc_system_starter_nickname(&sys, CHARMANDER), "{{pokemonName}} 100%"));
    CHECK(gc_system_set_starter_nickname(&sys, CHARMANDER, "Em") == GC_STARTER_NICK_OK);

    /* refused: nothing changes, and the answer says why */
    before = sys;
    const uint16_t other = (uint16_t)starter_ids[n_starters - 1];   /* a starter nobody has named */
    CHECK(other != BULBASAUR && other != CHARMANDER && other != SQUIRTLE && other != PIKACHU && !*gc_system_starter_nickname(&sys, other));
    CHECK(gc_system_set_starter_nickname(&sys, IVYSAUR, "Nope") == GC_STARTER_NICK_NOT_A_STARTER);
    CHECK(gc_system_set_starter_nickname(&sys, 0, "Nope") == GC_STARTER_NICK_NOT_A_STARTER);
    CHECK(gc_system_set_starter_nickname(&sys, GC_MAX_SPECIES_ID + 1, "Nope") == GC_STARTER_NICK_NOT_A_STARTER);
    CHECK(gc_system_set_starter_nickname(&sys, 65535, "Nope") == GC_STARTER_NICK_NOT_A_STARTER);
    CHECK(gc_system_set_starter_nickname(&sys, IVYSAUR, "") == GC_STARTER_NICK_NOT_A_STARTER && gc_system_set_starter_nickname(&sys, IVYSAUR, NULL) == GC_STARTER_NICK_NOT_A_STARTER);
    CHECK(gc_system_set_starter_nickname(NULL, BULBASAUR, "Nope") == GC_STARTER_NICK_NOT_A_STARTER);
    static const char *bad[] = {"123456789012345678901", "Mr. Pikachu the Great!", "a\x01" "b", "a\n", "\xC3", "\xED\xA0\x80", "\xF0\x9F\x94", "\xC0\x80",
                                "\x7F", "\xC2\x85", "\xF0\x9F\x94\xA5\xF0\x9F\x94\xA5\xF0\x9F\x94\xA5\xF0\x9F\x94\xA5\xF0\x9F\x94\xA5\xF0\x9F\x94\xA5\xF0\x9F\x94\xA5\xF0\x9F\x94\xA5\xF0\x9F\x94\xA5\xF0\x9F\x94\xA5\xF0\x9F\x94\xA5"};
    for (size_t k = 0; k < sizeof bad / sizeof bad[0]; k++) {
        CHECK(gc_system_set_starter_nickname(&sys, BULBASAUR, bad[k]) == GC_STARTER_NICK_BAD_TEXT);   /* a starter that has one: it is kept */
        CHECK(gc_system_set_starter_nickname(&sys, other, bad[k]) == GC_STARTER_NICK_BAD_TEXT);       /* one that has none: it stays without */
    }
    CHECK(!memcmp(&sys, &before, sizeof sys));

    /* cleared with "" and with NULL: the later entries move up, the freed slot is zero again */
    CHECK(gc_system_set_starter_nickname(&sys, BULBASAUR, "") == GC_STARTER_NICK_OK);
    check_table(&sys);
    CHECK(sys.n_starter_nicknames == 3 && !*gc_system_starter_nickname(&sys, BULBASAUR) && sys.starter_nicknames[0].species == CHARMANDER);
    CHECK(gc_system_set_starter_nickname(&sys, SQUIRTLE, NULL) == GC_STARTER_NICK_OK);
    check_table(&sys);
    CHECK(sys.n_starter_nicknames == 2 && !strcmp(gc_system_starter_nickname(&sys, PIKACHU), FIRE "Blaze" FIRE) && !strcmp(gc_system_starter_nickname(&sys, CHARMANDER), "Em"));
    CHECK(gc_system_set_starter_nickname(&sys, BULBASAUR, "") == GC_STARTER_NICK_OK && sys.n_starter_nicknames == 2);   /* clearing what is not there is fine */
    CHECK(gc_system_set_starter_nickname(&sys, CHARMANDER, "") == GC_STARTER_NICK_OK && gc_system_set_starter_nickname(&sys, PIKACHU, "") == GC_STARTER_NICK_OK);
    check_table(&sys);
    CHECK(!memcmp(&sys, &fresh, sizeof sys));   /* the profile that never had one, byte for byte */

    /* a count the memory cannot hold (damaged memory; the table is not meant to be written by hand) is read as the table's size at most, never past
     * it: nothing here can tell the answer apart, it is for a run under a sanitizer (see the PR) */
    CHECK(gc_system_set_starter_nickname(&sys, BULBASAUR, "Sparky") == GC_STARTER_NICK_OK);
    sys.n_starter_nicknames = 100000;
    CHECK(gc_system_starter_nickname(&sys, BULBASAUR) != NULL && gc_system_starter_nickname(&sys, 65535) != NULL);
    sys.n_starter_nicknames = -3;
    CHECK(!*gc_system_starter_nickname(&sys, BULBASAUR));
    sys.n_starter_nicknames = 1;
    CHECK(gc_system_set_starter_nickname(&sys, BULBASAUR, "") == GC_STARTER_NICK_OK);
}

static void full_cases(void)
{
    gc_system_init_defaults(&sys);
    /* the first 128 starters in a scrambled order */
    unsigned x = 12345;
    int order[1024];
    for (int i = 0; i < n_starters; i++) order[i] = i;
    for (int i = n_starters - 1; i > 0; i--) { x = x * 1103515245u + 12345u; const int j = (int)((x >> 8) % (unsigned)(i + 1)); const int t = order[i]; order[i] = order[j]; order[j] = t; }
    char name[32];
    for (int k = 0; k < GC_STARTER_NICKNAMES; k++) {
        snprintf(name, sizeof name, "n%d", starter_ids[order[k]]);
        CHECK(gc_system_set_starter_nickname(&sys, (uint16_t)starter_ids[order[k]], name) == GC_STARTER_NICK_OK);
        if (k % 16 == 0) check_table(&sys);
    }
    check_table(&sys);
    CHECK(sys.n_starter_nicknames == GC_STARTER_NICKNAMES);
    for (int k = 0; k < GC_STARTER_NICKNAMES; k++) {
        snprintf(name, sizeof name, "n%d", starter_ids[order[k]]);
        CHECK(!strcmp(gc_system_starter_nickname(&sys, (uint16_t)starter_ids[order[k]]), name));
    }
    const uint16_t extra = (uint16_t)starter_ids[order[GC_STARTER_NICKNAMES]], held = (uint16_t)starter_ids[order[0]];
    CHECK(!*gc_system_starter_nickname(&sys, extra));
    /* full: a new one is refused and nothing changes; replacing and clearing still work */
    before = sys;
    CHECK(gc_system_set_starter_nickname(&sys, extra, "One too many") == GC_STARTER_NICK_FULL);
    CHECK(!memcmp(&sys, &before, sizeof sys));
    CHECK(gc_system_set_starter_nickname(&sys, extra, "\xC3") == GC_STARTER_NICK_BAD_TEXT);   /* the text is judged first */
    CHECK(gc_system_set_starter_nickname(&sys, held, "Renamed") == GC_STARTER_NICK_OK && !strcmp(gc_system_starter_nickname(&sys, held), "Renamed"));
    CHECK(sys.n_starter_nicknames == GC_STARTER_NICKNAMES);
    check_table(&sys);
    CHECK(gc_system_set_starter_nickname(&sys, extra, "") == GC_STARTER_NICK_OK && sys.n_starter_nicknames == GC_STARTER_NICKNAMES);   /* clearing one that is not there */
    CHECK(gc_system_set_starter_nickname(&sys, held, "") == GC_STARTER_NICK_OK && sys.n_starter_nicknames == GC_STARTER_NICKNAMES - 1);
    check_table(&sys);
    CHECK(gc_system_set_starter_nickname(&sys, extra, "In") == GC_STARTER_NICK_OK && sys.n_starter_nicknames == GC_STARTER_NICKNAMES);   /* a place freed */
    check_table(&sys);
    CHECK(!strcmp(gc_system_starter_nickname(&sys, extra), "In") && !*gc_system_starter_nickname(&sys, held));
    CHECK(gc_system_set_starter_nickname(&sys, held, "Again") == GC_STARTER_NICK_FULL);
}

static void save_cases(void)
{
    const char *why = NULL;
    gc_system_init_defaults(&sys);
    /* a profile with something in it, and no nickname: no section, the bytes an older build wrote */
    sys.starters[BULBASAUR].candy_count = 7;
    sys.starters[CHARMANDER].friendship = 40;
    sys.stats.total_battles = 1234;
    sys.voucher_counts[1] = 3;
    const size_t len0 = save_profile(&sys, buf0);
    CHECK(!has_section(buf0, len0));
    memset(&back, 0xA5, sizeof back);
    CHECK(gc_system_load(&back, buf0, len0, &why) && back.n_starter_nicknames == 0);
    check_table(&back);

    /* nicknames: the section, and every one back in its own species */
    CHECK(gc_system_set_starter_nickname(&sys, SQUIRTLE, "Shell") == GC_STARTER_NICK_OK && gc_system_set_starter_nickname(&sys, BULBASAUR, LONGEST) == GC_STARTER_NICK_OK);
    CHECK(gc_system_set_starter_nickname(&sys, PIKACHU, TEN_FIRES) == GC_STARTER_NICK_OK && gc_system_set_starter_nickname(&sys, CHARMANDER, "{{x}}") == GC_STARTER_NICK_OK);
    const size_t len1 = save_profile(&sys, buf);
    CHECK(len1 > len0 && has_section(buf, len1));
    memset(&back, 0xA5, sizeof back);
    CHECK(gc_system_load(&back, buf, len1, &why));
    check_table(&back);
    CHECK(back.n_starter_nicknames == 4);
    CHECK(!strcmp(gc_system_starter_nickname(&back, SQUIRTLE), "Shell") && !strcmp(gc_system_starter_nickname(&back, BULBASAUR), LONGEST));
    CHECK(!strcmp(gc_system_starter_nickname(&back, PIKACHU), TEN_FIRES) && !strcmp(gc_system_starter_nickname(&back, CHARMANDER), "{{x}}"));
    CHECK(!memcmp(back.starter_nicknames, sys.starter_nicknames, sizeof sys.starter_nicknames));
    /* the rest of the profile is what it was */
    CHECK(back.starters[BULBASAUR].candy_count == 7 && back.starters[CHARMANDER].friendship == 40 && back.stats.total_battles == 1234 && back.voucher_counts[1] == 3);
    CHECK(save_profile(&back, buf2) == len1 && !memcmp(buf, buf2, len1));   /* loading and saving again changes nothing */
    /* only the nicknames differ from the profile without any: cleared again, the bytes of the one that never had them */
    for (int i = 0; i < 4; i++) {
        static const uint16_t which[4] = {BULBASAUR, CHARMANDER, SQUIRTLE, PIKACHU};
        CHECK(gc_system_set_starter_nickname(&sys, which[i], "") == GC_STARTER_NICK_OK);
    }
    CHECK(save_profile(&sys, buf2) == len0 && !memcmp(buf0, buf2, len0) && !has_section(buf2, len0));
    /* a save without them loads over a profile that has some: none are left behind */
    CHECK(gc_system_set_starter_nickname(&back, PIKACHU, "Stale") == GC_STARTER_NICK_OK);
    CHECK(gc_system_load(&back, buf0, len0, &why) && back.n_starter_nicknames == 0 && !*gc_system_starter_nickname(&back, PIKACHU));
    check_table(&back);

    /* a full table comes back whole */
    gc_system_init_defaults(&sys);
    char name[32];
    for (int k = 0; k < GC_STARTER_NICKNAMES; k++) {
        snprintf(name, sizeof name, "full%d", k);
        CHECK(gc_system_set_starter_nickname(&sys, (uint16_t)starter_ids[k], name) == GC_STARTER_NICK_OK);
    }
    CHECK(sys.n_starter_nicknames == GC_STARTER_NICKNAMES);
    size_t n = save_profile(&sys, buf);
    memset(&back, 0xA5, sizeof back);
    CHECK(gc_system_load(&back, buf, n, &why) && back.n_starter_nicknames == GC_STARTER_NICKNAMES);
    CHECK(!memcmp(back.starter_nicknames, sys.starter_nicknames, sizeof sys.starter_nicknames));
    check_table(&back);

    /* a damaged save (a flipped byte) is still refused as a whole: the checksum is not weakened */
    buf[n / 2] ^= 0x40;
    CHECK(!gc_system_load(&back, buf, n, &why));
}

/* writes into the table past the setter's checks, as damaged memory or a hand-made file would */
static void put_raw(gc_system *s, int i, uint16_t species, const char *text)
{
    memset(&s->starter_nicknames[i], 0, sizeof s->starter_nicknames[i]);
    s->starter_nicknames[i].species = species;
    size_t n = 0;
    while (n < GC_STARTER_NICKNAME_BYTES && text[n]) n++;
    memcpy(s->starter_nicknames[i].text, text, n);
}

static void damaged_cases(void)
{
    const char *why = NULL;
    /* records the saver writes as they are: a text that is not valid, a species that is not a starter, an empty text, a duplicate, an unsorted order */
    gc_system_init_defaults(&sys);
    sys.starters[BULBASAUR].candy_count = 9;
    put_raw(&sys, 0, SQUIRTLE, "Shell");            /* unsorted, kept */
    put_raw(&sys, 1, IVYSAUR, "NotAStarter");       /* dropped */
    put_raw(&sys, 2, CHARMANDER, "\xC3");           /* cut short: dropped */
    put_raw(&sys, 3, PIKACHU, "123456789012345678901");   /* 21 characters: dropped */
    put_raw(&sys, 4, 0, "Zero");                    /* dropped */
    put_raw(&sys, 5, BULBASAUR, "First");           /* a duplicate: the later record wins */
    put_raw(&sys, 6, BULBASAUR, "Second");
    put_raw(&sys, 7, SQUIRTLE, "");                 /* empty: dropped, it does not clear the one above */
    put_raw(&sys, 8, 65535, "Far");                 /* dropped */
    put_raw(&sys, 9, CHARMANDER, "a\x01" "b");      /* a control character: dropped */
    sys.n_starter_nicknames = 10;
    size_t n = save_profile(&sys, buf);
    memset(&back, 0xA5, sizeof back);
    CHECK(gc_system_load(&back, buf, n, &why));
    check_table(&back);
    CHECK(back.n_starter_nicknames == 2);
    CHECK(!strcmp(gc_system_starter_nickname(&back, SQUIRTLE), "Shell") && !strcmp(gc_system_starter_nickname(&back, BULBASAUR), "Second"));
    CHECK(!*gc_system_starter_nickname(&back, CHARMANDER) && !*gc_system_starter_nickname(&back, PIKACHU) && !*gc_system_starter_nickname(&back, IVYSAUR));
    CHECK(back.starters[BULBASAUR].candy_count == 9);   /* the profile is not lost with them */

    /* no terminator in the stored bytes */
    gc_system_init_defaults(&sys);
    put_raw(&sys, 0, BULBASAUR, "Keep");
    memset(sys.starter_nicknames[1].text, 'a', sizeof sys.starter_nicknames[1].text);
    sys.starter_nicknames[1].species = SQUIRTLE;
    sys.n_starter_nicknames = 2;
    n = save_profile(&sys, buf);
    memset(&back, 0xA5, sizeof back);
    CHECK(gc_system_load(&back, buf, n, &why) && back.n_starter_nicknames == 1 && !strcmp(gc_system_starter_nickname(&back, BULBASAUR), "Keep"));
    check_table(&back);

    /* hand-made sections appended to a real save (the checksum and length recomputed): what another build, or damage under a good checksum, could leave */
    gc_system_init_defaults(&sys);
    sys.starters[BULBASAUR].candy_count = 11;
    const size_t base = save_profile(&sys, buf0);
    CHECK(!has_section(buf0, base));
    for (int variant = 0; variant < 4; variant++) {
        /* variant 0: 200 records (more than the table holds); 1: a text field of another size (a build with a longer limit); 2: a block cut short; 3: no
         * species field */
        uint8_t body[64 * 1024];
        size_t m = 0;
        const uint16_t text_size = variant == 1 ? 81 : GC_STARTER_NICKNAME_BYTES;
        const int count = variant == 0 ? 200 : variant == 2 ? 5 : 3;
        const int nf = variant == 3 ? 1 : 2;
        const uint16_t sizes[2] = {2, text_size};
        const char *names[2] = {"species", "text"};
        const int first = variant == 3 ? 1 : 0;
        uint16_t nf16 = (uint16_t)nf;
        memcpy(body + m, &nf16, 2); m += 2;
        for (int f = 0; f < nf; f++) {
            const int which = first + f;
            const uint32_t h = fnv32(names[which]);
            memcpy(body + m, &h, 4); m += 4;
            memcpy(body + m, &sizes[which], 2); m += 2;
        }
        const uint16_t count16 = (uint16_t)count;
        memcpy(body + m, &count16, 2); m += 2;
        const int written = variant == 2 ? 2 : count;   /* a cut block: 5 records announced, 2 present */
        for (int k = 0; k < written; k++) {
            char text[128];
            memset(text, 0, sizeof text);
            snprintf(text, sizeof text, "v%d-%d", variant, k);
            const uint16_t species = (uint16_t)starter_ids[k];
            if (nf == 2) { memcpy(body + m, &species, 2); m += 2; }
            memcpy(body + m, text, text_size); m += text_size;
        }
        const uint32_t sh = fnv32("starter_nicknames"), sl = (uint32_t)m;
        memcpy(buf, buf0, base);
        memcpy(buf + base, &sh, 4);
        memcpy(buf + base + 4, &sl, 4);
        memcpy(buf + base + 8, body, m);
        const size_t total = base + 8 + m;
        const uint32_t body_len = (uint32_t)(total - 20), crc = crc32_of(buf + 20, body_len);
        memcpy(buf + 12, &body_len, 4);
        memcpy(buf + 16, &crc, 4);
        memset(&back, 0xA5, sizeof back);
        CHECK(gc_system_load(&back, buf, total, &why));
        check_table(&back);
        CHECK(back.starters[BULBASAUR].candy_count == 11);
        if (variant == 0) {
            CHECK(back.n_starter_nicknames == GC_STARTER_NICKNAMES);   /* the first 128 of the 200 */
            for (int k = 0; k < GC_STARTER_NICKNAMES; k++) {
                char want[32];
                snprintf(want, sizeof want, "v0-%d", k);
                CHECK(!strcmp(gc_system_starter_nickname(&back, (uint16_t)starter_ids[k]), want));
            }
            CHECK(!*gc_system_starter_nickname(&back, (uint16_t)starter_ids[GC_STARTER_NICKNAMES]));
        } else {
            CHECK(back.n_starter_nicknames == 0);
        }
    }
    /* and a build that wrote it with a field this one does not know loads the ones it does (an extra field after the text) */
    {
        uint8_t body[256];
        size_t m = 0;
        const uint16_t nf = 3, count = 1;
        memcpy(body + m, &nf, 2); m += 2;
        const uint32_t h[3] = {fnv32("species"), fnv32("text"), fnv32("future")};
        const uint16_t sz[3] = {2, GC_STARTER_NICKNAME_BYTES, 4};
        for (int f = 0; f < 3; f++) { memcpy(body + m, &h[f], 4); m += 4; memcpy(body + m, &sz[f], 2); m += 2; }
        memcpy(body + m, &count, 2); m += 2;
        const uint16_t species = SQUIRTLE;
        char text[GC_STARTER_NICKNAME_BYTES] = "Shell";
        const uint32_t future = 7;
        memcpy(body + m, &species, 2); m += 2;
        memcpy(body + m, text, sizeof text); m += sizeof text;
        memcpy(body + m, &future, 4); m += 4;
        const uint32_t sh = fnv32("starter_nicknames"), sl = (uint32_t)m;
        memcpy(buf, buf0, base);
        memcpy(buf + base, &sh, 4);
        memcpy(buf + base + 4, &sl, 4);
        memcpy(buf + base + 8, body, m);
        const size_t total = base + 8 + m;
        const uint32_t body_len = (uint32_t)(total - 20), crc = crc32_of(buf + 20, body_len);
        memcpy(buf + 12, &body_len, 4);
        memcpy(buf + 16, &crc, 4);
        CHECK(gc_system_load(&back, buf, total, &why) && back.n_starter_nicknames == 1 && !strcmp(gc_system_starter_nickname(&back, SQUIRTLE), "Shell"));
    }
}

int main(void)
{
    collect_starters();
    layout_cases();
    table_cases();
    full_cases();
    save_cases();
    damaged_cases();
    printf("starter nickname test: %u checks, 0 failed\n", checks);
    return 0;
}
