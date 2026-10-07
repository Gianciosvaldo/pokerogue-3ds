/* Additive game inventory boundary oracle. The battle-core producer and full
 * turn comparator are included unchanged. Rebuild modifiers through gc_items
 * before every real turn; compare status, phases, messages and RNG afterward. */
#include "../src/internal.h"
static void inventory_orb_turn(pr_battle *, pr_engine *);
static int mapped[2][2], inventory_turns;
#define pr_run_turn inventory_orb_turn
#include <stdarg.h>
#include <stdio.h>
static int guarded_printf(const char *format, ...);
static int message_cases, message_same, message_differ, message_unported;
static int reported_stats, reported_parse;
#define printf guarded_printf
#define main established_turn_main
#include "../../battle-core/tests/status-orb-replay.inc"
#undef main
#undef printf
#undef pr_run_turn

static int guarded_printf(const char *format, ...)
{
    char output[65536];
    va_list args; va_start(args, format);
    int n = vsnprintf(output, sizeof output, format, args);
    va_end(args);
    if (!strncmp(output, "texts of covered turns ", 23))
        sscanf(output, "texts of covered turns %d: identical %d, differ %d, a message with no text yet %d",
               &message_cases, &message_same, &message_differ, &message_unported);
    if (!strncmp(output, "cases ", 6)) {
        int total, skipped;
        sscanf(output, "cases %d (skipped %d: records not read yet), parse mismatches %d, stats mismatches %d",
               &total, &skipped, &reported_parse, &reported_stats);
    }
    fputs(output, stdout);
    return n;
}


#include "../../battle-core/tests/statusorb-guards.inc"

int main(int argc, char **argv)
{
    if (argc != 2 || !getenv("TURNTEST_LOCALES")) {
        fprintf(stderr, "usage: TURNTEST_LOCALES=<upstream>/locales/en orb_diff_test <trace>\n");
        return 2;
    }
    if (orb_guard_checks()) { fprintf(stderr, "orb guard checks failed\n"); return 1; }
    FILE *f = fopen(argv[1], "r");
    if (!f) { perror("trace"); return 2; }
    char line[LINE_LEN];
    int version, wanted, cases = 0, ends = 0, footer = 0;
    if (!fgets(line, sizeof line, f) || sscanf(line, "# STATUSORB_CONTRACT %d %d", &version, &wanted) != 2
        || version != 1 || wanted < 20 || wanted % 20) { fclose(f); return 2; }
    while (fgets(line, sizeof line, f)) {
        if (footer) { fclose(f); fprintf(stderr, "records after completion\n"); return 2; }
        if (!strncmp(line, "CASE ", 5)) {
            int id;
            if (sscanf(line, "CASE %d", &id) != 1 || id != cases || cases != ends) { fclose(f); return 2; }
            cases++;
        } else if (!strncmp(line, "END ", 4)) {
            if (cases != ends + 1) { fclose(f); return 2; }
            ends++;
        } else if (!strncmp(line, "# STATUSORB_COMPLETE ", 21)) {
            int count;
            if (sscanf(line, "# STATUSORB_COMPLETE %d", &count) != 1 || count != wanted) { fclose(f); return 2; }
            footer = 1;
        }
    }
    fclose(f);
    if (!footer || cases != wanted || ends != wanted) {
        fprintf(stderr, "incomplete Status orb trace: requested %d cases %d ends %d footer %d\n", wanted, cases, ends, footer);
        return 2;
    }
    int status = established_turn_main(argc, argv);
    if (status) return status;
    int covered = 0;
    for (int i = 0; i < n_ran_phase; i++) if (!strcmp(ran_phase[i].name, "TurnStartPhase")) covered = ran_phase[i].n;
    if (covered != wanted) { fprintf(stderr, "Status orb coverage %d/%d\n", covered, wanted); return 1; }
    if (message_cases != wanted || message_same != wanted || message_differ || message_unported || reported_parse || reported_stats) {
        fprintf(stderr, "Status orb message comparison incomplete or different\n"); return 1;
    }
    if (inventory_turns != wanted) return 1;
    for (int side = 0; side < 2; side++) for (int effect = 0; effect < 2; effect++)
        if (mapped[side][effect] != wanted / 2) return 1;
    printf("Orb inventory: %d full turns, toxic/flame per side %d/%d %d/%d, unchanged inventories\n",
           inventory_turns, mapped[0][0], mapped[0][1], mapped[1][0], mapped[1][1]);
    return 0;
}

static void inventory_orb_turn(pr_battle *bt, pr_engine *e)
{
    static gc_run run;
    memset(&run, 0, sizeof run);
    run.bt = *bt;
    for (int side = 0; side < 2; side++) {
        if (bt->n_mods[side] != 1) { fprintf(stderr, "unsupported Orb fixture inventory\n"); exit(2); }
        const pr_modifier *m = &bt->mods[side][0];
        if (m->cls != PR_MOD_TURN_STATUS_EFFECT || m->unknown_subclass || m->virtual_stack_count
            || m->pokemon_id < -1 || m->pokemon_id > UINT32_MAX
            || (m->status_effect != STATUS_EFFECT_TOXIC && m->status_effect != STATUS_EFFECT_BURN)) exit(2);
        const int flame = m->status_effect == STATUS_EFFECT_BURN;
        gc_item *it = side ? run.enemy_items : run.items;
        *it = (gc_item){flame ? "FLAME_ORB" : "TOXIC_ORB", -1, m->stack_count,
                        m->pokemon_id == -1 ? 0 : (uint32_t)m->pokemon_id};
        if (side) run.n_enemy_items = 1; else run.n_items = 1;
        mapped[side][flame]++;
    }
    gc_item old[2] = {run.items[0], run.enemy_items[0]};
    pr_rng rng = bt->rng;
    gc_items_to_battle(&run);
    if (memcmp(&rng, &run.bt.rng, sizeof rng)) exit(1);
    memcpy(bt->mods, run.bt.mods, sizeof bt->mods);
    memcpy(bt->n_mods, run.bt.n_mods, sizeof bt->n_mods);
    pr_run_turn(bt, e);
    run.bt = *bt;
    gc_items_from_battle(&run);
    if (run.n_items != 1 || run.n_enemy_items != 1
        || memcmp(&old[0], &run.items[0], sizeof(gc_item))
        || memcmp(&old[1], &run.enemy_items[0], sizeof(gc_item))) {
        fprintf(stderr, "Orb inventory changed during turn\n"); exit(1);
    }
    inventory_turns++;
}
