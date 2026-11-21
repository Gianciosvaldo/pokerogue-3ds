/* Nicknames in game-core (upstream: Pokemon.nickname; PartyUiHandler.processRenameOption sets it from the rename form, SelectStarterPhase gives a
 * starter's, the run's save keeps it). Needs no upstream checkout: the few locale strings it reads are in the test.
 *  - gc_nickname_valid: what the rename form's input lets through (at most 20 UTF-16 code units, an emoji counts two) and what it never could
 *    (invalid UTF-8, control characters), so a bad text never reaches a Pokemon;
 *  - gc_run_set_nickname: sets, replaces and clears (a shorter name leaves no tail of the longer one), refuses a bad slot or text without changing anything;
 *  - a starter's nickname reaches its Pokemon, and a list with a bad one is refused whole;
 *  - the save: nicknames come back in the right slots, a run with none saves the bytes it saved before nicknames existed (no section), such a save loads
 *    with none, and a damaged nickname is dropped and counted without losing the run;
 *  - the messages: a turn's "used a move" names the nicknamed Pokemon, the enemy keeps the species name with its Wild/Foe affix, and the evolution's
 *    "evolved into" names the species and not the nickname (upstream: this.pokemon.name), while "is evolving" names the nickname. */
#include "../src/internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static gc_run run, back;
static gc_system sys;
static uint8_t buf[GC_SAVE_MAX], buf0[GC_SAVE_MAX], buf2[GC_SAVE_MAX];
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr, "nickname FAIL line %d: %s\n", __LINE__, #x); exit(1); } } while (0)

static const char *locale_get(void *ctx, const char *file, int fallback, size_t *len)
{
    (void)ctx;
    static const char battle[] = "{\"useMove\":\"{{pokemonNameWithAffix}} used {{moveName}}!\",\"wildPokemonWithAffix\":\"Wild {{pokemonName}}\","
                                 "\"foePokemonWithAffix\":\"Foe {{pokemonName}}\"}";
    static const char menu[] = "{\"evolving\":\"What?\\n{{pokemonName}} is evolving!\","
                               "\"evolutionDone\":\"Congratulations!\\nYour {{pokemonName}} evolved into {{evolvedPokemonName}}!\"}";
    static const char pokemon[] = "{\"bulbasaur\":\"Bulbasaur\",\"ivysaur\":\"Ivysaur\",\"charmander\":\"Charmander\",\"squirtle\":\"Squirtle\"}";
    if (fallback) return NULL;
    if (!strcmp(file, "battle")) { *len = sizeof battle - 1; return battle; }
    if (!strcmp(file, "menu")) { *len = sizeof menu - 1; return menu; }
    if (!strcmp(file, "pokemon")) { *len = sizeof pokemon - 1; return pokemon; }
    return NULL;
}
static const pr_text_source src = {locale_get, NULL, "en"};

static const char *text(const pr_msg *m)
{
    static char out[8][512];
    static int k;
    char *o = out[k++ % 8];
    pr_msg_format(m, &src, o, 512);
    return o;
}

static void drain(void)
{
    gc_event e;
    while (gc_run_next_event(&run, &e)) {}
}

static bool starters(gc_run *r, const char *a, const char *b, const char *c, const char **why)
{
    gc_starter st[3];
    memset(st, 0, sizeof st);
    const int species[3] = {SPECIES_ID_BULBASAUR, SPECIES_ID_CHARMANDER, SPECIES_ID_SQUIRTLE};
    const char *nick[3] = {a, b, c};
    for (int i = 0; i < 3; i++) {
        st[i].species = (uint16_t)species[i];
        st[i].tera_type = -1;
        memset(st[i].ivs, 15, 6);
        if (nick[i]) snprintf(st[i].nickname, sizeof st[i].nickname, "%s", nick[i]);
    }
    return gc_run_set_starters(r, st, 3, why);
}

/* a classic run (seed fixed) with the three starters, up to the first command screen, where a run can be saved */
static void new_run(const char *a, const char *b, const char *c)
{
    const char *why = NULL;
    gc_system_init_defaults(&sys);
    memset(&run, 0, sizeof run);
    gc_run_new(&run, GAME_MODES_CLASSIC, "nickname", 11);
    CHECK(starters(&run, a, b, c, &why));
    run.sys = &sys;
    for (int i = 0; i < 64 && !gc_run_can_save(&run); i++) {
        drain();
        if (gc_run_can_save(&run)) break;
        const gc_screen *s = gc_run_screen(&run);
        gc_choice ch;
        memset(&ch, 0, sizeof ch);
        if (s->kind == GC_SCREEN_COMMAND) break;
        ch.kind = GC_CH_CONFIRM;
        if (!gc_run_choose(&run, &ch, &why)) break;
    }
    CHECK(gc_run_can_save(&run));
}

static void valid_cases(void)
{
    static const struct { const char *s; bool ok; } t[] = {
        {NULL, true}, {"", true}, {"a", true}, {"Sparky", true}, {"Mr. Pikachu the Great!", false},   /* 22 characters */
        {"Mr. Pikachu the Grea", true}, {"12345678901234567890", true}, {"123456789012345678901", false},   /* 20 and 21 */
        {"  padded  ", true},                                   /* the caller trims; spaces are text */
        {"\xC3\x91" "and\xC3\xBA", true},                       /* Ñandú */
        {"\xE3\x81\x82\xE3\x81\x84\xE3\x81\x86\xE3\x81\x88\xE3\x81\x8A", true},   /* five hiragana */
        {"\xF0\x9F\x94\xA5" "Blaze" "\xF0\x9F\x94\xA5", true},  /* 2 + 5 + 2 = 9 units */
        {"\xF0\x9F\x94\xA5\xF0\x9F\x94\xA5\xF0\x9F\x94\xA5\xF0\x9F\x94\xA5\xF0\x9F\x94\xA5\xF0\x9F\x94\xA5\xF0\x9F\x94\xA5\xF0\x9F\x94\xA5\xF0\x9F\x94\xA5\xF0\x9F\x94\xA5", true},   /* ten emoji = 20 units */
        {"\xF0\x9F\x94\xA5\xF0\x9F\x94\xA5\xF0\x9F\x94\xA5\xF0\x9F\x94\xA5\xF0\x9F\x94\xA5\xF0\x9F\x94\xA5\xF0\x9F\x94\xA5\xF0\x9F\x94\xA5\xF0\x9F\x94\xA5\xF0\x9F\x94\xA5\xF0\x9F\x94\xA5", false},   /* eleven = 22 */
        {"a\xF0\x9F\x94\xA5" "bcdefghijklmnopqr", true},        /* 1 + 2 + 17 = 20 */
        {"a\xF0\x9F\x94\xA5" "bcdefghijklmnopqrs", false},       /* 21 */
        {"{{pokemonName}}", true}, {"100% $t(x)", true},        /* template syntax is text, not a template */
        {"\x80", false}, {"\xBF" "a", false},                   /* a continuation byte on its own */
        {"\xC3", false}, {"ab\xE3\x81", false}, {"\xF0\x9F\x94", false},   /* cut short */
        {"\xC0\x80", false}, {"\xE0\x80\x80", false}, {"\xF0\x80\x80\x80", false},   /* overlong NUL */
        {"\xC1\x81", false}, {"\xE0\x81\x81", false}, {"\xF0\x80\x81\x81", false}, {"\xE0\x9F\xBF", false},   /* overlong 'A' and U+07FF: printable */
        {"\xED\xA0\x80", false}, {"\xED\xBF\xBF", false},       /* UTF-16 surrogates */
        {"\xF4\x90\x80\x80", false}, {"\xF8\x88\x80\x80\x80", false}, {"\xFF", false}, {"\xFE", false},   /* beyond U+10FFFF, or no such lead byte */
        {"a\x01" "b", false}, {"a\n", false}, {"a\t", false}, {"\x7F", false}, {"\xC2\x85", false}, {"\xC2\x9F", false},   /* controls, DEL, C1 */
        {"\xC2\xA0", true},                                     /* no-break space is text */
    };
    for (size_t i = 0; i < sizeof t / sizeof t[0]; i++) {
        if (gc_nickname_valid(t[i].s) != t[i].ok) fprintf(stderr, "case %zu (%s)\n", i, t[i].s ? t[i].s : "NULL");
        CHECK(gc_nickname_valid(t[i].s) == t[i].ok);
    }
    /* whatever passes fits the 81 bytes it is kept in: 20 three-byte characters is the most (60) */
    char longest[PR_NICKNAME_BYTES * 2];
    memset(longest, 0, sizeof longest);
    for (int i = 0; i < 20; i++) memcpy(longest + i * 3, "\xE3\x81\x82", 3);
    CHECK(gc_nickname_valid(longest) && strlen(longest) < PR_NICKNAME_BYTES);
    memcpy(longest + 60, "\xE3\x81\x82", 3);
    CHECK(!gc_nickname_valid(longest));
}

static void set_cases(void)
{
    new_run(NULL, NULL, NULL);
    CHECK(run.n_party == 3);
    for (int i = 0; i < 3; i++) CHECK(!run.party[i].b.nickname[0]);
    CHECK(gc_run_set_nickname(&run, 1, "Ember"));
    CHECK(!strcmp(run.party[1].b.nickname, "Ember") && !run.party[0].b.nickname[0] && !run.party[2].b.nickname[0]);
    /* a shorter name leaves no tail of the longer one (the save and the comparison read all of the bytes) */
    CHECK(gc_run_set_nickname(&run, 1, "Em"));
    CHECK(!strcmp(run.party[1].b.nickname, "Em"));
    for (size_t k = 3; k < sizeof run.party[1].b.nickname; k++) CHECK(run.party[1].b.nickname[k] == 0);
    /* refused: nothing changes */
    CHECK(!gc_run_set_nickname(&run, -1, "x") && !gc_run_set_nickname(&run, 3, "x") && !gc_run_set_nickname(&run, 6, "x") && !gc_run_set_nickname(&run, 99, "x"));
    CHECK(!gc_run_set_nickname(&run, 1, "123456789012345678901") && !gc_run_set_nickname(&run, 1, "a\x01") && !gc_run_set_nickname(&run, 1, "\xC3"));
    CHECK(!strcmp(run.party[1].b.nickname, "Em"));
    CHECK(!gc_run_set_nickname(NULL, 0, "x"));
    /* "" and NULL clear it, as an empty submit does upstream */
    CHECK(gc_run_set_nickname(&run, 1, ""));
    CHECK(!run.party[1].b.nickname[0]);
    CHECK(gc_run_set_nickname(&run, 0, "Sparky") && gc_run_set_nickname(&run, 0, NULL));
    CHECK(!run.party[0].b.nickname[0]);
}

static void starter_cases(void)
{
    const char *why = NULL;
    new_run("Sparky", NULL, "\xF0\x9F\x94\xA5" "Blaze" "\xF0\x9F\x94\xA5");
    CHECK(!strcmp(run.party[0].b.nickname, "Sparky") && !run.party[1].b.nickname[0]);
    CHECK(!strcmp(run.party[2].b.nickname, "\xF0\x9F\x94\xA5" "Blaze" "\xF0\x9F\x94\xA5"));
    /* a list with one bad nickname is refused whole, and says why */
    gc_run fresh;
    memset(&fresh, 0, sizeof fresh);
    gc_run_new(&fresh, GAME_MODES_CLASSIC, "nickname-bad", 3);
    CHECK(!starters(&fresh, "Sparky", "123456789012345678901", NULL, &why));
    CHECK(why && strstr(why, "nickname") && fresh.n_party == 0);
    why = NULL;
    CHECK(!starters(&fresh, NULL, NULL, "\xC3", &why) && why && fresh.n_party == 0);
    why = NULL;
    CHECK(starters(&fresh, "Sparky", NULL, NULL, &why) && fresh.n_party == 3 && !strcmp(fresh.party[0].b.nickname, "Sparky"));
}

static size_t save_into(const gc_run *r, uint8_t *to)
{
    const size_t n = gc_run_save(r, to, GC_SAVE_MAX);
    CHECK(n > 0);
    return n;
}

static void save_cases(void)
{
    const char *why = NULL;
    /* a run with no nickname: the save an older build wrote (no section) */
    new_run(NULL, NULL, NULL);
    const size_t len0 = save_into(&run, buf0);
    memset(&back, 0xA5, sizeof back);
    CHECK(gc_run_load(&back, buf0, len0, &why) && !back.load_dropped);
    for (int i = 0; i < back.n_party; i++) CHECK(!back.party[i].b.nickname[0]);

    /* nicknames in slots 0 and 2: they come back in those slots */
    CHECK(gc_run_set_nickname(&run, 0, "Sparky") && gc_run_set_nickname(&run, 2, "\xF0\x9F\x94\xA5" "Blaze" "\xF0\x9F\x94\xA5"));
    const size_t len1 = save_into(&run, buf);
    CHECK(len1 > len0);
    memset(&back, 0xA5, sizeof back);
    CHECK(gc_run_load(&back, buf, len1, &why) && !back.load_dropped);
    CHECK(back.n_party == 3 && !strcmp(back.party[0].b.nickname, "Sparky") && !back.party[1].b.nickname[0]);
    CHECK(!strcmp(back.party[2].b.nickname, "\xF0\x9F\x94\xA5" "Blaze" "\xF0\x9F\x94\xA5"));
    for (size_t k = 7; k < sizeof back.party[0].b.nickname; k++) CHECK(back.party[0].b.nickname[k] == 0);
    CHECK(save_into(&back, buf2) == len1 && !memcmp(buf, buf2, len1));   /* loading and saving again changes nothing */
    /* the nickname is all that differs from the run without any */
    CHECK(gc_run_set_nickname(&run, 0, NULL) && gc_run_set_nickname(&run, 2, ""));
    CHECK(save_into(&run, buf2) == len0 && !memcmp(buf0, buf2, len0));   /* cleared again: the bytes of the run that never had one */

    /* a nickname that is not text is dropped (counted), the run and the others stay */
    static const char *bad[] = {"\xC3", "123456789012345678901", "a\nb", "\xED\xA0\x80", "\xF0\x9F\x94"};
    for (size_t k = 0; k < sizeof bad / sizeof bad[0]; k++) {
        CHECK(gc_run_set_nickname(&run, 2, "Keep"));
        snprintf(run.party[0].b.nickname, sizeof run.party[0].b.nickname, "%s", bad[k]);   /* straight into the Pokemon, past the checks */
        const size_t n = save_into(&run, buf);
        memset(&back, 0xA5, sizeof back);
        CHECK(gc_run_load(&back, buf, n, &why));
        CHECK(!back.party[0].b.nickname[0] && !strcmp(back.party[2].b.nickname, "Keep") && back.load_dropped == 1);
    }
    /* no terminating zero in the stored bytes */
    CHECK(gc_run_set_nickname(&run, 2, "Keep"));
    memset(run.party[0].b.nickname, 'a', sizeof run.party[0].b.nickname);
    size_t n = save_into(&run, buf);
    memset(&back, 0xA5, sizeof back);
    CHECK(gc_run_load(&back, buf, n, &why) && !back.party[0].b.nickname[0] && !strcmp(back.party[2].b.nickname, "Keep") && back.load_dropped == 1);
    /* two at once */
    snprintf(run.party[0].b.nickname, sizeof run.party[0].b.nickname, "%s", "\xC3");
    snprintf(run.party[1].b.nickname, sizeof run.party[1].b.nickname, "%s", "a\x01");
    n = save_into(&run, buf);
    memset(&back, 0xA5, sizeof back);
    CHECK(gc_run_load(&back, buf, n, &why) && !back.party[0].b.nickname[0] && !back.party[1].b.nickname[0] && !strcmp(back.party[2].b.nickname, "Keep") && back.load_dropped == 2);
    /* a damaged save (a flipped byte) is still refused as a whole: the checksum is not weakened */
    n = save_into(&run, buf);
    buf[n / 2] ^= 0x40;
    CHECK(!gc_run_load(&back, buf, n, &why));
}

/* D'Arby's sealed chips keep their nickname in a section of their own (written only when one has a nickname) */
static void chip_cases(void)
{
    const char *why = NULL;
    new_run("Sparky", NULL, NULL);
    gc_internal *in = gc_int(&run);
    in->chips.n = 1;
    in->chips.level[0] = 5;
    in->chips.mon[0] = run.party[2];
    CHECK(!in->chips.mon[0].b.nickname[0]);
    const size_t plain = save_into(&run, buf0);
    snprintf(in->chips.mon[0].b.nickname, sizeof in->chips.mon[0].b.nickname, "%s", "Spirit");
    size_t n = save_into(&run, buf);
    CHECK(n > plain);
    memset(&back, 0xA5, sizeof back);
    CHECK(gc_run_load(&back, buf, n, &why) && !back.load_dropped);
    CHECK(gc_int(&back)->chips.n == 1 && !strcmp(gc_int(&back)->chips.mon[0].b.nickname, "Spirit"));
    CHECK(!strcmp(back.party[0].b.nickname, "Sparky") && !back.party[2].b.nickname[0]);   /* the party's own are the party's */
    /* a save with chips and no chip nickname loads over it with none left behind */
    CHECK(gc_run_load(&back, buf0, plain, &why) && gc_int(&back)->chips.n == 1 && !gc_int(&back)->chips.mon[0].b.nickname[0]);
    /* a damaged one is dropped and counted, the chip stays */
    snprintf(in->chips.mon[0].b.nickname, sizeof in->chips.mon[0].b.nickname, "%s", "\xC3");
    n = save_into(&run, buf);
    memset(&back, 0xA5, sizeof back);
    CHECK(gc_run_load(&back, buf, n, &why) && back.load_dropped == 1 && gc_int(&back)->chips.n == 1 && !gc_int(&back)->chips.mon[0].b.nickname[0]);
}

/* the last text event of the message list with this key, from the run's events since the last drain */
static int find_msg(const gc_event *evs, int n, const char *key, int from)
{
    for (int i = from; i < n; i++)
        if (evs[i].kind == GC_EV_MESSAGE && evs[i].msg.key && !strcmp(evs[i].msg.key, key)) return i;
    return -1;
}

static void turn_cases(void)
{
    const char *why = NULL;
    new_run("Sparky", NULL, NULL);
    const gc_screen *s = gc_run_screen(&run);
    CHECK(s->kind == GC_SCREEN_COMMAND);
    gc_choice ch;
    memset(&ch, 0, sizeof ch);
    ch.kind = GC_CH_FIGHT;
    ch.b = -1;
    for (int i = 0; i < 4; i++) if (s->move_ok[i]) { ch.a = (int16_t)i; break; }
    CHECK(gc_run_choose(&run, &ch, &why));
    /* battle-core's events of the turn: "{{pokemonNameWithAffix}} used {{moveName}}!" of each side */
    int player_used = 0, enemy_used = 0;
    for (int i = 0; i < run.n_battle_events; i++) {
        const pr_event *e = &run.battle_events[i];
        if (e->type != PR_EV_TEXT || !e->msg.key || strcmp(e->msg.key, "battle:useMove") || !e->msg.n_args) continue;
        const pr_msg_arg *a = &e->msg.args[0];
        CHECK(a->kind == PR_ARG_POKEMON);
        if (a->s) {
            player_used++;
            CHECK(!strcmp(a->s, "Sparky"));
            CHECK(!strncmp(text(&e->msg), "Sparky used ", 12));
        } else {
            enemy_used++;
            CHECK(strncmp(text(&e->msg), "Sparky", 6) != 0);   /* the enemy is not the nicknamed one */
        }
    }
    CHECK(player_used == 1);
    (void)enemy_used;
}

static void evolve_cases(void)
{
    new_run("Sparky", NULL, NULL);
    drain();
    gc_mon *m = &run.party[0];
    m->b.level = 16;
    gc_mon_recalc(&run, m);
    memset(&gc_int(&run)->rq, 0, sizeof gc_int(&run)->rq);
    gc_int(&run)->rq.n_levels = 1;
    gc_rphase up = gc_rp(RP_LEVEL_UP, 0, 15, 16);
    CHECK(gc_phase_start(&run, &up));
    gc_rphase request;
    int found = 0;
    gc_event evs[256];
    int n = 0;
    while (gc_rq_next(&gc_int(&run)->rq, &request)) {
        if (request.type != RP_EVOLUTION) continue;
        found++;
        CHECK(gc_evo_data[request.b].species == SPECIES_ID_IVYSAUR);
        CHECK(!gc_phase_start(&run, &request) && run.screen.kind == GC_SCREEN_EVOLVE);
        while (n < 256 && gc_run_next_event(&run, &evs[n])) n++;
        const int evolving = find_msg(evs, n, "menu:evolving", 0);
        CHECK(evolving >= 0);
        CHECK(!strcmp(text(&evs[evolving].msg), "What?\nSparky is evolving!"));   /* upstream: preEvolvedPokemonName = getPokemonNameWithAffix */
        bool ok = false;
        const char *why = NULL;
        CHECK(gc_phase_choose(&run, &request, &(gc_choice){GC_CH_EVOLVE, 0, 0, 0}, &why, &ok) && ok);
        while (n < 256 && gc_run_next_event(&run, &evs[n])) n++;
        const int done = find_msg(evs, n, "menu:evolutionDone", 0);
        CHECK(done >= 0);
        CHECK(evs[done].msg.n_args == 2 && evs[done].msg.args[1].kind == PR_ARG_POKEMON_NAME && !evs[done].msg.args[1].s);
        CHECK(!strcmp(text(&evs[done].msg), "Congratulations!\nYour Sparky evolved into Ivysaur!"));   /* upstream: this.pokemon.name */
        break;
    }
    CHECK(found == 1);
    CHECK(m->b.species == SPECIES_ID_IVYSAUR && !strcmp(m->b.nickname, "Sparky"));   /* the nickname stays through an evolution, as upstream's */
}

int main(void)
{
    valid_cases();
    set_cases();
    starter_cases();
    save_cases();
    chip_cases();
    turn_cases();
    evolve_cases();
    printf("nickname tests: %u checks, 0 failures\n", checks);
    return 0;
}
