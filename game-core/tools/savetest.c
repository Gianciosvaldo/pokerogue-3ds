/* Checks on gc_run_save / gc_run_load beside the soak runs' round trips (gcplay --save-test): a damaged,
 * cut or newer save is refused, and a save from a build whose structs differ loads what still matches.
 * Prints each check; exits 1 if one fails. */
#include "gc/run.h"
#include <stdio.h>
#include <string.h>

static int failed;
#define CHECK(c, what) do { const int ok_ = (c); printf("%s  %s\n", ok_ ? "ok  " : "FAIL", what); if (!ok_) failed = 1; } while (0)

static uint32_t fnv(const char *s)
{
    uint32_t h = 2166136261u;
    while (*s) { h ^= (uint8_t)*s++; h *= 16777619u; }
    return h;
}

static uint32_t crc32(const uint8_t *p, size_t n)
{
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < n; i++) {
        c ^= p[i];
        for (int k = 0; k < 8; k++) c = c >> 1 ^ (0xEDB88320u & (0u - (c & 1)));
    }
    return ~c;
}

static void fix_crc(uint8_t *buf)
{
    uint32_t len;
    memcpy(&len, buf + 12, 4);
    const uint32_t c = crc32(buf + 20, len);
    memcpy(buf + 16, &c, 4);
}

/* Renames every stored field called `from` (its hash) as if an older build had named it otherwise */
static int rename_field(uint8_t *buf, size_t n, const char *from, const char *to)
{
    const uint32_t a = fnv(from), b = fnv(to);
    int hits = 0;
    for (size_t i = 20; i + 4 <= n; i++)
        if (!memcmp(buf + i, &a, 4)) { memcpy(buf + i, &b, 4); hits++; }
    fix_crc(buf);
    return hits;
}

static gc_run run, back;
static uint8_t buf[GC_SAVE_MAX], copy[GC_SAVE_MAX];

int main(void)
{
    const char *why;
    gc_run_new(&run, GAME_MODES_CLASSIC, "savetest0000000000000000", 7);
    gc_starter team[3];
    memset(team, 0, sizeof team);
    team[0].species = SPECIES_ID_BULBASAUR;
    team[1].species = SPECIES_ID_CHARMANDER;
    team[2].species = SPECIES_ID_SQUIRTLE;
    for (int i = 0; i < 3; i++) { team[i].tera_type = -1; memset(team[i].ivs, 15, 6); }
    CHECK(gc_run_set_starters(&run, team, 3, &why), "a new run with three starters");
    gc_event e;
    while (gc_run_next_event(&run, &e)) {}
    /* Set Mega form on party[0] and Fusion on party[1] to test form/fusion saving */
    run.party[0].b.species = SPECIES_ID_VENUSAUR;
    run.party[0].b.form = 1;
    run.party[1].b.fusion_species = SPECIES_ID_CHARMANDER;
    run.party[1].b.fusion_form = 0;
    run.party[1].b.fusion_ability_index = 1;
    run.party[0].n_used_tms = 1;
    run.party[0].used_tms[0] = MOVE_ID_FLAMETHROWER;
    CHECK(gc_run_can_save(&run), "saving is allowed at wave 1's first command");
    const size_t n = gc_run_save(&run, buf, sizeof buf);
    CHECK(n > 0 && n <= GC_SAVE_MAX, "the save fits in GC_SAVE_MAX");
    static uint8_t small[64];
    CHECK(gc_run_save(&run, small, sizeof small) == 0 && !gc_run_load(&back, small, sizeof small, &why),
          "a buffer too small gets nothing that loads");

    const bool loaded = gc_run_load(&back, buf, n, &why);
    if (!loaded || back.load_dropped) printf("        (%s, %d dropped)\n", loaded ? "loaded" : why, back.load_dropped);
    CHECK(loaded && back.load_dropped == 0, "it loads back");
    CHECK(back.wave == run.wave && back.n_party == run.n_party && back.money == run.money
              && gc_run_screen(&back)->kind == GC_SCREEN_COMMAND,
          "the same wave, party, money and screen");
    int same = 1;
    for (int i = 0; i < run.n_party; i++)
        same &= back.party[i].b.species == run.party[i].b.species && back.party[i].b.hp == run.party[i].b.hp
                && back.party[i].exp == run.party[i].exp && !memcmp(back.party[i].b.moves, run.party[i].b.moves, 8);
    same &= (back.party[0].b.form == run.party[0].b.form);
    same &= (back.party[1].b.fusion_species == run.party[1].b.fusion_species
             && back.party[1].b.fusion_form == run.party[1].b.fusion_form
             && back.party[1].b.fusion_ability_index == run.party[1].b.fusion_ability_index);
    CHECK(same, "each party member's species, form, fusion data, HP, exp and moves");
    CHECK(back.party[0].n_used_tms == 1 && back.party[0].used_tms[0] == MOVE_ID_FLAMETHROWER,
          "format-2 save retains used_tms");

    /* a format-1 save loads successfully and has empty used_tms */
    memcpy(copy, buf, n);
    uint16_t fmt1 = 1;
    memcpy(copy + 8, &fmt1, 2);
    fix_crc(copy);
    CHECK(gc_run_load(&back, copy, n, &why), "a format-1 save loads successfully");
    CHECK(back.party[0].n_used_tms == 0, "format-1 save has empty used_tms");

    /* one turn later the save point is over */
    gc_choice c = {GC_CH_FIGHT, 0, -1, 0};
    for (int k = 0; k < 4 && !gc_run_choose(&back, &c, &why); k++) c.a++;
    CHECK(!gc_run_can_save(&back) || back.wave != run.wave, "no saving once the battle's first turn has run");

    memcpy(copy, buf, n);
    copy[n / 2] ^= 0x40;
    CHECK(!gc_run_load(&back, copy, n, &why), "a damaged save is refused");
    printf("        (%s)\n", why);
    CHECK(!gc_run_load(&back, buf, n - 10, &why), "a cut save is refused");
    memcpy(copy, buf, n);
    copy[8] = 99;
    CHECK(!gc_run_load(&back, copy, n, &why), "a save in a newer format is refused");
    printf("        (%s)\n", why);
    CHECK(!gc_run_load(&back, "hello", 5, &why), "something else is refused");

    /* a save from a build where the Pokemon's friendship field was called otherwise: everything else loads */
    memcpy(copy, buf, n);
    const int hits = rename_field(copy, n, "friendship", "old_friendship");
    CHECK(hits == 2, "the party's and the enemy's friendship field renamed");
    CHECK(gc_run_load(&back, copy, n, &why) && back.load_dropped == 2 && back.party[0].b.friendship == 0
              && back.party[0].b.hp == run.party[0].b.hp,
          "a field this build lacks is dropped (and counted); the rest loads");
    printf("%s\n", failed ? "FAILED" : "all passed");
    return failed;
}
