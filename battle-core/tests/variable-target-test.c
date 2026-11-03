#include "pr/engine.h"
#include <stdio.h>
#include <string.h>

static int run_case(const char *name, int move, int terrain, int species, bool tera, int preturn,
                    int expected_multiple, const int8_t *expected, int expected_n)
{
    pr_battle bt;
    pr_pokemon mons[4];
    memset(&bt, 0, sizeof bt);
    memset(mons, 0, sizeof mons);
    bt.double_battle = true;
    bt.terrain = terrain;
    bt.pre_commands[0].command = preturn;
    for (int i = 0; i < 4; i++) {
        mons[i].id = (uint32_t)(100 + i);
        mons[i].species = i == 0 ? species : SPECIES_ID_RATTATA;
        mons[i].hp = 10;
        mons[i].on_field = true;
        mons[i].is_player = i < 2;
        mons[i].terastallized = i == 0 && tera;
        bt.field[i] = &mons[i];
        bt.party[i < 2 ? 0 : 1][i & 1] = &mons[i];
        bt.n_party[i < 2 ? 0 : 1]++;
    }
    int8_t targets[4] = {-1, -1, -1, -1};
    bool multiple = false;
    int n = pr_get_move_targets(&bt, &mons[0], move, -1, targets, &multiple);
    if (n != expected_n || multiple != (bool)expected_multiple || memcmp(targets, expected, (size_t)n)) {
        fprintf(stderr, "%s: got n=%d multiple=%d targets=%d,%d,%d,%d; expected n=%d multiple=%d\n",
                name, n, multiple, targets[0], targets[1], targets[2], targets[3], expected_n,
                expected_multiple);
        return 1;
    }
    return 0;
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage: variable-target-test SOURCE-OBSERVATIONS\n");
        return 2;
    }
    FILE *input = fopen(argv[1], "r");
    if (!input) return 2;
    pr_closures_init();
    int failures = 0;
    char line[256];
    if (!fgets(line, sizeof line, input) || strcmp(line, "VARIABLE_TARGET 1 5\n")) return 2;
    for (int i = 0; i < 5; i++) {
        int move, terrain, species, tera, command, n, multiple, targets[4];
        if (!fgets(line, sizeof line, input)
            || sscanf(line, "CASE %d %d %d %d %d %d %d %d %d %d %d", &move, &terrain, &species, &tera,
                      &command, &n, &multiple, &targets[0], &targets[1], &targets[2], &targets[3]) != 11
            || n < 0 || n > 4 || (tera != 0 && tera != 1) || (multiple != 0 && multiple != 1)) return 2;
        int8_t expected[4];
        for (int k = 0; k < 4; k++) expected[k] = (int8_t)targets[k];
        char name[32];
        snprintf(name, sizeof name, "source case %d", i);
        failures += run_case(name, move, terrain, species, tera, command, multiple, expected, n);
    }
    if (fgets(line, sizeof line, input)) return 2;
    fclose(input);
    printf("Variable-target doubles source cases: 5, %d mismatches\n", failures);
    return failures ? 1 : 0;
}
