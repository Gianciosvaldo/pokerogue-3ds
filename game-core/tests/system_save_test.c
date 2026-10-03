/* Tests for offline system save/load serialization (gc_system_save / gc_system_load).
 * Verifies that achievement flags, Classic clears, ribbons, stat milestones, dex/starters,
 * eggs and vouchers are saved and reloaded accurately without overwriting active run data. */

#include "gc/system.h"
#include "gc/run.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#define CHECK(cond, msg) \
    do { \
        if (!(cond)) { \
            fprintf(stderr, "FAIL: %s (line %d)\n", msg, __LINE__); \
            assert(cond); \
        } else { \
            printf("ok    %s\n", msg); \
        } \
    } while (0)

static gc_system sys1;
static gc_system sys2;
static gc_run run;
static uint8_t save_buf[128 * 1024];

int main(void) {
    const char *why = NULL;

    /* 1. Init default system data */
    gc_system_init_defaults(&sys1);

    /* 2. Set achievement flags, Classic clears / ribbons, and stat milestones */
    CHECK(!gc_system_has_achievement(&sys1, 10), "achievement 10 initially locked");
    CHECK(gc_system_unlock_achievement(&sys1, 10), "unlock achievement 10");
    CHECK(gc_system_unlock_achievement(&sys1, 42), "unlock achievement 42");
    CHECK(gc_system_has_achievement(&sys1, 10), "achievement 10 unlocked");
    CHECK(gc_system_has_achievement(&sys1, 42), "achievement 42 unlocked");

    /* Classic win and ribbons */
    CHECK(gc_system_get_ribbons(&sys1, 1) == 0, "Bulbasaur initial ribbons is 0");
    gc_system_add_classic_win(&sys1, 1);
    gc_system_add_classic_win(&sys1, 1);
    CHECK(gc_system_get_ribbons(&sys1, 1) == 2, "Bulbasaur has 2 classic wins / ribbons");
    CHECK(sys1.stats.classic_wins == 2, "classic_wins stat milestone incremented");

    /* Stat milestones */
    sys1.stats.endless_wins = 5;
    sys1.stats.total_battles = 150;
    sys1.stats.total_catches = 80;
    sys1.stats.total_hatches = 12;
    sys1.stats.total_shinies = 3;
    sys1.stats.highest_wave = 200;
    sys1.stats.highest_damage = 99999;
    sys1.stats.total_money_earned = 12345678;

    /* Eggs and Vouchers */
    gc_system_add_egg(&sys1, 501, 10, 1, 25);
    gc_system_unlock_voucher(&sys1, 100, GC_VOUCHER_PLUS);
    gc_system_add_voucher_count(&sys1, GC_VOUCHER_REGULAR, 3);

    /* Starter candy */
    gc_system_add_starter_candy(&sys1, 1, 15);

    /* 3. Serialize system data */
    size_t len = gc_system_save(&sys1, save_buf, sizeof save_buf);
    CHECK(len > 20, "gc_system_save produced valid non-empty buffer");

    /* 4. Set up an active run and record its state */
    gc_starter starters[3];
    memset(starters, 0, sizeof starters);
    starters[0].species = 1;
    starters[1].species = 4;
    starters[2].species = 7;
    gc_run_new(&run, GAME_MODES_CLASSIC, "systemsavetest000", 12345);
    CHECK(gc_run_set_starters(&run, starters, 3, &why), "active run started");
    int initial_wave = run.wave;
    double initial_money = run.money;
    int initial_n_party = run.n_party;
    uint16_t lead_species = run.party[0].b.species;

    /* 5. Deserialize system data into sys2 */
    CHECK(gc_system_load(&sys2, save_buf, len, &why), "gc_system_load succeeded");

    /* 6. Verify sys2 contents match sys1 */
    CHECK(gc_system_has_achievement(&sys2, 10), "reloaded achievement 10 is unlocked");
    CHECK(gc_system_has_achievement(&sys2, 42), "reloaded achievement 42 is unlocked");
    CHECK(!gc_system_has_achievement(&sys2, 99), "reloaded achievement 99 is locked");

    CHECK(gc_system_get_ribbons(&sys2, 1) == 2, "reloaded Bulbasaur ribbons == 2");
    CHECK(sys2.stats.classic_wins == 2, "reloaded classic_wins milestone == 2");
    CHECK(sys2.stats.endless_wins == 5, "reloaded endless_wins milestone == 5");
    CHECK(sys2.stats.total_battles == 150, "reloaded total_battles milestone == 150");
    CHECK(sys2.stats.total_catches == 80, "reloaded total_catches milestone == 80");
    CHECK(sys2.stats.total_hatches == 12, "reloaded total_hatches milestone == 12");
    CHECK(sys2.stats.total_shinies == 3, "reloaded total_shinies milestone == 3");
    CHECK(sys2.stats.highest_wave == 200, "reloaded highest_wave milestone == 200");
    CHECK(sys2.stats.highest_damage == 99999, "reloaded highest_damage milestone == 99999");
    CHECK(sys2.stats.total_money_earned == 12345678, "reloaded total_money_earned milestone == 12345678");

    CHECK(sys2.n_eggs == 1 && sys2.eggs[0].id == 501, "reloaded egg data intact");
    CHECK(gc_system_has_voucher_unlock(&sys2, 100), "reloaded voucher unlock intact");
    CHECK(sys2.voucher_counts[GC_VOUCHER_REGULAR] == 3, "reloaded voucher count intact");
    CHECK(sys2.starters[1].candy_count == 15, "reloaded starter candy count intact");

    /* 7. Verify active run data is completely untouched */
    CHECK(run.wave == initial_wave, "active run wave untouched");
    CHECK(run.money == initial_money, "active run money untouched");
    CHECK(run.n_party == initial_n_party, "active run n_party untouched");
    CHECK(run.party[0].b.species == lead_species, "active run lead species untouched");

    /* 8. Verify error handling for invalid/damaged system save buffers */
    uint8_t bad_buf[100];
    memcpy(bad_buf, save_buf, sizeof bad_buf);
    bad_buf[25] ^= 0xFF; /* corrupt body */
    CHECK(!gc_system_load(&sys2, bad_buf, sizeof bad_buf, &why), "corrupted system save rejected");

    memcpy(bad_buf, save_buf, sizeof bad_buf);
    bad_buf[0] = 'X'; /* corrupt magic */
    CHECK(!gc_system_load(&sys2, bad_buf, sizeof bad_buf, &why), "invalid magic system save rejected");

    printf("All system save tests passed successfully!\n");
    return 0;
}
