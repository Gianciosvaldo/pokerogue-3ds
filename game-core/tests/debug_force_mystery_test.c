/* The developer hook gc_debug_force_mystery (rundriver.txt "mystery="): off by default, forces one chosen encounter once per run on the first plain wave from min_wave on,
 * never on a multiple of 10, only when the encounter's own party rules hold, and never in a custom run. */
#include "../src/internal.h"
#include <stdio.h>
#include <stdlib.h>
static gc_run run;
static int checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr, "debug force mystery line %d: %s\n", __LINE__, #x); exit(1); } } while (0)

static void start(const char *seed, int n_party)
{
    const char *why = NULL;
    memset(&run, 0, sizeof run);
    gc_run_new(&run, GAME_MODES_CLASSIC, seed, 777);
    static const int sp[3] = {SPECIES_ID_BULBASAUR, SPECIES_ID_CHARMANDER, SPECIES_ID_SQUIRTLE};
    gc_starter s[3];
    memset(s, 0, sizeof s);
    for (int i = 0; i < n_party; i++) {
        s[i].species = sp[i];
        s[i].tera_type = -1;
        s[i].moves[0] = MOVE_ID_TACKLE;
        memset(s[i].ivs, 15, sizeof s[i].ivs);
    }
    CHECK(gc_run_set_starters(&run, s, n_party, &why));
    gc_event ev;
    while (gc_run_next_event(&run, &ev)) {}
}
/* the wave `w` as the run builds it */
static void wave(int w)
{
    run.wave = w - 1;
    gc_new_battle(&run);
    CHECK(run.wave == w);
}
static bool is_me(void) { return run.battle_type == BATTLE_TYPE_MYSTERY_ENCOUNTER; }

int main(void)
{
    /* off by default: no wave of the first 9 is an encounter (the draw needs wave 10+... and the hook is not set) */
    gc_debug_force_mystery(-1, 1);
    start("force-off", 3);
    for (int w = 2; w < 9; w++) { wave(w); CHECK(!is_me()); }

    /* on: the first plain wave from min_wave is the encounter, once */
    gc_debug_force_mystery(GC_ME_WEIRD_DREAM, 4);
    start("force-on", 3);
    wave(2); CHECK(!is_me());
    wave(3); CHECK(!is_me());
    wave(4); CHECK(is_me() && run.mystery_type == GC_ME_WEIRD_DREAM);
    CHECK(run.n_mystery_history == 1 && run.mystery_history[0].tier == GC_ME_ROGUE);
    wave(5); CHECK(!is_me() || run.mystery_type != GC_ME_WEIRD_DREAM);
    wave(6); CHECK(!is_me() || run.mystery_type != GC_ME_WEIRD_DREAM);

    /* a multiple of 10 is never converted: min_wave 10 waits for 11 */
    gc_debug_force_mystery(GC_ME_WEIRD_DREAM, 10);
    start("force-ten", 3);
    wave(9); CHECK(!is_me());
    wave(10); CHECK(!is_me());
    wave(11); CHECK(is_me() && run.mystery_type == GC_ME_WEIRD_DREAM);

    /* Weird Dream needs three Pokemon: with two the wave stays plain */
    gc_debug_force_mystery(GC_ME_WEIRD_DREAM, 1);
    start("force-party", 2);
    for (int w = 2; w < 6; w++) { wave(w); CHECK(!is_me() || run.mystery_type != GC_ME_WEIRD_DREAM); }

    /* out of range ids turn it off */
    gc_debug_force_mystery(GC_ME_COUNT, 1);
    start("force-bad", 3);
    for (int w = 2; w < 6; w++) { wave(w); CHECK(!is_me()); }

    gc_debug_force_mystery(-1, 1);
    printf("debug force mystery: %d checks passed\n", checks);
    return 0;
}
