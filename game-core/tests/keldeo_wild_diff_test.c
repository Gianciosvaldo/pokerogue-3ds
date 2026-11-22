/* A wild Keldeo's form change (src/gc_moveset.c: gc_enemy_learned_form_target, gc_enemy_quiet_form_change) against upstream at 8555c08: replays the trace of
 * tests/keldeo-wild.producer.test.ts. "K" records are a Keldeo of a random shape (game mode, boss segments, form, level, ability slot, nature, IVs, weather, double
 * battle) whose moveset upstream's generateAndPopulateMoveset made in executeWithSeedOffset(.., offset, seed). Compared: the moves, the next two draws of the RNG,
 * the stats, whether the call queued a QuietFormChangePhase (Pokemon.generateAndPopulateMoveset's triggerPokemonFormChange) and its target form; then, for a queued
 * phase run for the Pokemon on the field and visible: the form, the ability slot and ability, the stats, the HP, the dex entry's seen and caught flags and the text.
 * GC_LOCALES names upstream's locales/en folder. Usage: keldeo_wild_diff_test <trace>; exit 0 only with 0 mismatches. */
#include "../src/internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static gc_run run;
static gc_system sys;
static FILE *f;
static int bad, shown;
#define CHECK(cond, ...) do { if (!(cond)) { bad++; if (shown < 14 || getenv("KW_ALL")) { shown++; fprintf(stderr, __VA_ARGS__); fputc('\n', stderr); } } } while (0)

static void malformed(void) { fputs("malformed input\n", stderr); exit(2); }
static long long integer(void) { long long v; if (fscanf(f, "%lld", &v) != 1) malformed(); return v; }
static unsigned long long unsigned_integer(void) { unsigned long long v; if (fscanf(f, "%llu", &v) != 1) malformed(); return v; }
static void word(char *out, size_t cap)
{
    char fmt[16];
    snprintf(fmt, sizeof fmt, "%%%zus", cap - 1);
    if (fscanf(f, fmt, out) != 1) malformed();
}

/* upstream's locales/en folder: the text is formatted by battle-core's pr_msg_format from the files upstream's i18next read */
static const char *locale_get(void *ctx, const char *file, int fallback, size_t *len)
{
    (void)ctx;
    static struct { char name[64]; char *text; size_t len; } cache[8];
    static int n_cache;
    const char *dir = getenv("GC_LOCALES");
    if (fallback) return NULL;
    if (!dir) { fputs("GC_LOCALES must name upstream's locales/en folder (tools/keldeowilddiff.sh sets it)\n", stderr); exit(2); }
    for (int i = 0; i < n_cache; i++)
        if (!strcmp(cache[i].name, file)) { *len = cache[i].len; return cache[i].text; }
    if (n_cache >= 8) return NULL;
    char path[512];
    snprintf(path, sizeof path, "%s/%s.json", dir, file);
    FILE *lf = fopen(path, "rb");
    if (!lf) { fprintf(stderr, "cannot read %s\n", path); exit(2); }
    fseek(lf, 0, SEEK_END);
    const long sz = ftell(lf);
    fseek(lf, 0, SEEK_SET);
    char *text = malloc((size_t)sz + 1);
    const size_t n = fread(text, 1, (size_t)sz, lf);
    text[n] = 0;
    fclose(lf);
    snprintf(cache[n_cache].name, sizeof cache[n_cache].name, "%s", file);
    cache[n_cache].text = text;
    cache[n_cache].len = n;
    n_cache++;
    *len = n;
    return text;
}

static void text_hex(const pr_msg *m, char *out, size_t cap)
{
    char s[1024];
    pr_text_source src = {locale_get, NULL, "en"};
    const size_t n = pr_msg_format(m, &src, s, sizeof s);
    if (n >= sizeof s || 2 * n + 2 > cap) malformed();
    if (!n) { strcpy(out, "-"); return; }
    for (size_t j = 0; j < n; j++) sprintf(out + 2 * j, "%02x", (unsigned char)s[j]);
}

int main(int argc, char **argv)
{
    if (argc < 2 || !(f = fopen(argv[1], "r"))) { fputs("usage: keldeo_wild_diff_test <trace>\n", stderr); return 2; }
    char c;
    if (fscanf(f, " %c", &c) != 1 || c != 'V') return 2;
    integer();
    memset(&run, 0, sizeof run);
    gc_run_new(&run, GAME_MODES_CLASSIC, "keldeo-wild", 7);
    gc_system_init_defaults(&sys);
    run.sys = &sys;
    run.wave = 45;
    run.battle_type = BATTLE_TYPE_WILD;

    int cases = 0, queued_cases = 0, changed = 0, reverted = 0, unqueued = 0, moves_checked = 0, with_secret_sword = 0, boss_cases = 0, texts_checked = 0;
    static int by_mode[GAME_MODES_CHALLENGE + 1];
    while (fscanf(f, " %c", &c) == 1) {
        if (c == 'Z') break;
        if (c != 'K') { fprintf(stderr, "wrong record %c\n", c); return 2; }
        const int idx = (int)integer(), mode = (int)integer(), boss = (int)integer(), segments = (int)integer(), form0 = (int)integer(), level = (int)integer();
        const int ability_index = (int)integer(), nature = (int)integer();
        int ivs[6];
        for (int k = 0; k < 6; k++) ivs[k] = (int)integer();
        const int weather = (int)integer(), dbl = (int)integer(), offset = (int)integer();
        char seed_text[64];
        word(seed_text, sizeof seed_text);
        const unsigned long long seen0 = unsigned_integer(), caught0 = unsigned_integer();
        int stats[6], n, up_moves[4] = {0};
        for (int k = 0; k < 6; k++) stats[k] = (int)integer();
        n = (int)integer();
        for (int k = 0; k < n && k < 4; k++) up_moves[k] = (int)integer();
        const int d1 = (int)integer(), d2 = (int)integer();
        char tag[4];
        word(tag, sizeof tag);
        if (strcmp(tag, "Q")) malformed();
        const int queued = (int)integer(), target = (int)integer();
        word(tag, sizeof tag);
        if (strcmp(tag, "X")) malformed();
        const int x_form = (int)integer(), x_ability_index = (int)integer(), x_ability = (int)integer();
        int x_stats[6];
        for (int k = 0; k < 6; k++) x_stats[k] = (int)integer();
        const int x_hp = (int)integer();
        const unsigned long long x_seen = unsigned_integer(), x_caught = unsigned_integer();
        const int n_text = (int)integer();
        char x_text[2][2048];
        for (int k = 0; k < n_text && k < 2; k++) word(x_text[k], sizeof x_text[k]);
        cases++;
        by_mode[mode]++;
        if (boss) boss_cases++;

        /* the Pokemon: a boss has its segments (so its passive), the weather and a double battle are the arena's */
        pr_enemy_gen g;
        memset(&g, 0, sizeof g);
        g.ability_index = ability_index;
        g.fusion_species = -1;
        g.id = 1;
        for (int k = 0; k < 6; k++) g.ivs[k] = ivs[k];
        g.gender = -1;
        g.form = form0;
        g.nature = nature;
        g.friendship = -1;
        g.boss_segments = boss ? segments : 0;
        run.bt.weather = (uint8_t)weather;
        run.run.mode = mode;
        run.double_battle = dbl != 0;
        gc_mon_new_enemy(&run, &run.enemy[0], SPECIES_ID_KELDEO, level, &g, 0);
        run.n_enemy = 1;
        gc_mon *m = &run.enemy[0];
        pr_seedstr seed;
        pr_seed_from_ascii(&seed, seed_text);
        pr_rng r = pr_run_seed_offset_rng(&run.run, offset, &seed);
        if (!gc_generate_moveset(&run, &r, m)) { CHECK(0, "case %d: mode %d boss %d form %d level %d double %d: the generator declined", idx, mode, boss, form0, level, dbl); continue; }
        const int e1 = pr_rng_seed_int(&r, 1000000, 0), e2 = pr_rng_seed_int(&r, 1000000, 0);
        CHECK(m->b.n_moves == n, "case %d: level %d boss %d: %d moves, upstream %d", idx, level, boss, m->b.n_moves, n);
        bool secret_sword = false;
        for (int k = 0; k < n && k < 4; k++) {
            moves_checked++;
            CHECK(m->b.moves[k] == up_moves[k], "case %d: level %d boss %d mode %d: move %d is %d, upstream %d", idx, level, boss, mode, k, m->b.moves[k], up_moves[k]);
            secret_sword |= up_moves[k] == MOVE_ID_SECRET_SWORD;
        }
        if (secret_sword) with_secret_sword++;
        CHECK(e1 == d1 && e2 == d2, "case %d: level %d: the RNG afterwards gives %d %d, upstream %d %d", idx, level, e1, e2, d1, d2);
        pr_calculate_stats(&run.bt, &m->b);
        for (int k = 0; k < 6; k++) CHECK(m->b.stats[k] == stats[k], "case %d: mode %d boss %d level %d: stat %d is %d, upstream %d", idx, mode, boss, level, k, m->b.stats[k], stats[k]);
        m->b.hp = m->b.stats[STAT_HP];

        /* the trigger that generateAndPopulateMoveset ends with */
        sys.dex[SPECIES_ID_KELDEO].seen_attr = seen0;
        sys.dex[SPECIES_ID_KELDEO].caught_attr = caught0;
        const int got = gc_enemy_learned_form_target(&run, m);
        CHECK((got >= 0) == (queued != 0) && (!queued || got == target), "case %d: mode %d form %d level %d moves %d %d %d %d: form change %d, upstream %s %d", idx, mode, form0, level,
              m->b.moves[0], m->b.moves[1], m->b.moves[2], m->b.moves[3], got, queued ? "queued" : "none", target);
        gc_event ev;
        while (gc_run_next_event(&run, &ev)) {}
        int texts = 0;
        char actual[2][2048];
        if (queued && got >= 0) {
            queued_cases++;
            gc_enemy_quiet_form_change(&run, 0, got);
            while (gc_run_next_event(&run, &ev))
                if (ev.kind == GC_EV_MESSAGE) { if (texts < 2) text_hex(&ev.msg, actual[texts], sizeof actual[texts]); texts++; }
            if (m->b.form != form0) changed++;
            if (m->b.form != form0 && target == 0) reverted++;
        } else unqueued++;
        CHECK(m->b.form == x_form, "case %d: form %d, upstream %d", idx, m->b.form, x_form);
        CHECK(m->ability_index == x_ability_index && m->b.ability == x_ability, "case %d: ability slot %d ability %d, upstream %d %d", idx, m->ability_index, m->b.ability, x_ability_index, x_ability);
        for (int k = 0; k < 6; k++) CHECK(m->b.stats[k] == x_stats[k], "case %d: stat %d after the form change is %d, upstream %d", idx, k, m->b.stats[k], x_stats[k]);
        CHECK(m->b.hp == x_hp, "case %d: HP %d after the form change, upstream %d", idx, m->b.hp, x_hp);
        CHECK(sys.dex[SPECIES_ID_KELDEO].seen_attr == x_seen && sys.dex[SPECIES_ID_KELDEO].caught_attr == x_caught,
              "case %d: dex seen %llu caught %llu, upstream %llu %llu", idx, (unsigned long long)sys.dex[SPECIES_ID_KELDEO].seen_attr,
              (unsigned long long)sys.dex[SPECIES_ID_KELDEO].caught_attr, x_seen, x_caught);
        CHECK(texts == n_text, "case %d: %d texts, upstream %d", idx, texts, n_text);
        for (int k = 0; k < texts && k < n_text && k < 2; k++) {
            texts_checked++;
            CHECK(!strcmp(actual[k], x_text[k]), "case %d: text %d is %s, upstream %s", idx, k, actual[k], x_text[k]);
        }
    }
    printf("keldeo wild: %d cases (%d bosses, %d with Secret Sword, %d moves), %d queued a form change (%d changed form, %d back to the first form), %d queued none, %d texts, modes",
           cases, boss_cases, with_secret_sword, moves_checked, queued_cases, changed, reverted, unqueued, texts_checked);
    for (int k = 0; k <= GAME_MODES_CHALLENGE; k++) printf(" %d", by_mode[k]);
    printf("\nTOTAL %d\n", bad);
    return bad || !cases ? 1 : 0;
}
