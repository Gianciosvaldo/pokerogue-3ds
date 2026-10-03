#include "../src/internal.h"
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>

static gc_run run, loaded;
static unsigned char saved[GC_SAVE_MAX];
static int checks;
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "item bridge FAIL line %d: %s\n", __LINE__, #x); return 1; } checks++; } while (0)

static void item(const char *key, int arg, int side, uint32_t owner, int stack)
{
    memset(&run, 0, sizeof run);
    gc_item *it = side ? run.enemy_items : run.items;
    *it = (gc_item){key, (int16_t)arg, stack, owner};
    if (side) run.n_enemy_items = 1; else run.n_items = 1;
    gc_items_to_battle(&run);
}

static int differential(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) { perror(path); return 1; }
    char line[512], key[64], cls[80], mode;
    int arg, side, stack, transferable, berry, step, expected, cases = 0, wrong = 0;
    uint32_t owner;
    while (fgets(line, sizeof line, f)) {
        mode = line[0];
        int bad = 0;
        if (mode == 'M') {
            if (sscanf(line, "M %63s %d %d %" SCNu32 " %d %79s %d %d", key, &arg, &side,
                       &owner, &stack, cls, &transferable, &berry) != 8) return 2;
            item(key, arg, side, owner, stack);
            const pr_modifier *m = &run.bt.mods[side][0];
            bad = run.bt.n_mods[side] != 1 || m->unknown_subclass ||
                strcmp(pr_mod_class_name[m->cls], cls) || m->stack_count != stack ||
                m->pokemon_id != owner || m->is_transferable != transferable || m->berry_type != berry;
        } else if (mode == 'C') {
            if (sscanf(line, "C %63s %d %d %" SCNu32 " %d %d %d", key, &arg, &side,
                       &owner, &stack, &step, &expected) != 7) return 2;
            item(key, arg, side, owner, stack);
            pr_pokemon *p = side ? &run.enemy[0].b : &run.party[0].b;
            p->id = owner; p->is_player = !side;
            for (int i = 0; i < step; i++) {
                if (!run.bt.n_mods[side]) return 2;
                pr_lose_held_item(&run.bt, &run.eng, p, &run.bt.mods[side][0]);
                gc_items_from_battle(&run);
            }
            const int n = side ? run.n_enemy_items : run.n_items;
            const gc_item *it = side ? run.enemy_items : run.items;
            bad = expected ? n != 1 || it[0].stack != expected : n != 0;
            gc_items_to_battle(&run);
            bad |= expected ? run.bt.n_mods[side] != 1 || run.bt.mods[side][0].stack_count != expected
                            : run.bt.n_mods[side] != 0;
        } else return 2;
        cases++; wrong += !!bad;
        if (bad) fprintf(stderr, "item bridge mismatch case %d: %s", cases, line);
    }
    fclose(f);
    printf("item bridge differential: %d cases, %d identical, %d mismatches\n", cases, cases - wrong, wrong);
    return !cases || wrong;
}

static int lifecycle(void)
{
    memset(&run, 0, sizeof run);
    gc_item_add(&run, "BERRY", BERRY_TYPE_SITRUS, 123, 2);
    gc_item_add(&run, "BERRY", BERRY_TYPE_LUM, 123, 3);
    gc_item_add(&run, "BERRY", BERRY_TYPE_SITRUS, 456, 4);
    gc_item_add(&run, "WHITE_HERB", -1, 123, 1);
    gc_item_add(&run, "MAP", -1, 0, 1);
    gc_items_to_battle(&run);
    run.party[0].b.id = 123; run.party[0].b.is_player = true;
    pr_lose_held_item(&run.bt, &run.eng, &run.party[0].b, &run.bt.mods[0][0]);
    gc_items_from_battle(&run);
    CHECK(run.items[0].stack == 1 && run.items[1].stack == 3 && run.items[2].stack == 4);
    pr_lose_held_item(&run.bt, &run.eng, &run.party[0].b, &run.bt.mods[0][0]);
    gc_items_from_battle(&run);
    CHECK(run.n_items == 4 && run.items[0].arg == BERRY_TYPE_LUM);
    run.party[0].b.stat_stages[0] = -2;
    CHECK(pr_apply_reset_negative_stat_stage_modifier(&run.bt, &run.eng, &run.party[0].b));
    gc_items_from_battle(&run);
    CHECK(run.n_items == 3 && run.party[0].b.stat_stages[0] == 0);
    CHECK(!strcmp(run.items[2].key, "MAP") && run.items[2].stack == 1);
    gc_items_from_battle(&run); gc_items_to_battle(&run);
    CHECK(run.n_items == 3 && run.bt.n_mods[0] == 3);

    /* Exercise actual battle-core berry effects through the adapter on both sides. */
    for (int side = 0; side < 2; side++) {
        const int berries[] = {BERRY_TYPE_SITRUS, BERRY_TYPE_LUM, BERRY_TYPE_LEPPA};
        for (int b = 0; b < 3; b++) {
            item("BERRY", berries[b], side, side ? 456 : 123, 1);
            pr_pokemon *p = side ? &run.enemy[0].b : &run.party[0].b;
            p->id = side ? 456 : 123; p->is_player = !side;
            p->species = SPECIES_ID_BULBASAUR; p->on_field = true;
            p->stats[0] = 100; p->hp = 20;
            p->status = STATUS_EFFECT_POISON;
            p->moves[0] = MOVE_ID_TACKLE; p->n_moves = 1; p->max_pp_override[0] = -1; p->pp_used[0] = gc_move_max_pp(p, 0);
            int initial_pp = p->pp_used[0];
            run.bt.field[side ? 2 : 0] = p;
            CHECK(pr_berry_should_apply(&run.bt, p, &run.bt.mods[side][0]));
            pr_apply_berry(&run.bt, &run.eng, p, &run.bt.mods[side][0]);
            /* HP healing and status reset are queued phases upstream and in battle-core. */
            for (int ph = 0; ph < PR_MAX_PHASES; ph++)
                if (run.eng.used[ph] && (run.eng.pool[ph].type == PH_RESET_STATUS ||
                                         run.eng.pool[ph].type == PH_POKEMON_HEAL))
                    pr_phase_run(&run.bt, &run.eng, ph);
            gc_items_from_battle(&run);
            CHECK((side ? run.n_enemy_items : run.n_items) == 0 && p->has_eaten_berry);
            if (b == 0) CHECK(p->hp == 45);
            if (b == 1) CHECK(p->status == STATUS_EFFECT_NONE);
            if (b == 2) CHECK(p->pp_used[0] == initial_pp - 10);
            gc_items_to_battle(&run);
            CHECK(run.bt.n_mods[side] == 0);
        }
    }

    /* Overflow entries were never represented and therefore were never consumed. */
    memset(&run, 0, sizeof run);
    for (int i = 0; i < GC_MAX_ITEMS; i++)
        gc_item_add(&run, "BERRY", BERRY_TYPE_LEPPA, (uint32_t)(i + 1), 1);
    gc_items_to_battle(&run);
    run.party[0].b.id = 1; run.party[0].b.is_player = true;
    pr_lose_held_item(&run.bt, &run.eng, &run.party[0].b, &run.bt.mods[0][0]);
    gc_items_from_battle(&run); gc_items_from_battle(&run);
    CHECK(run.n_items == GC_MAX_ITEMS - 1);
    CHECK(run.items[PR_MAX_MODIFIERS - 1].pokemon_id == PR_MAX_MODIFIERS + 1);

    /* A legal encounter save serializes the reconciled inventory and rebuilds it on load. */
    gc_run_new(&run, GAME_MODES_CLASSIC, "itembridge-save", 777);
    gc_starter starter = {0}; starter.species = SPECIES_ID_BULBASAUR; starter.tera_type = -1;
    const char *why = NULL;
    CHECK(gc_run_set_starters(&run, &starter, 1, &why));
    uint32_t id = run.party[0].b.id;
    gc_item_add(&run, "BERRY", BERRY_TYPE_LEPPA, id, 2);
    gc_item_add(&run, "WHITE_HERB", -1, id, 1);
    gc_items_to_battle(&run);
    pr_lose_held_item(&run.bt, &run.eng, &run.party[0].b, &run.bt.mods[0][0]);
    gc_items_from_battle(&run);
    size_t n = gc_run_save(&run, saved, sizeof saved);
    CHECK(n > 0);
    CHECK(gc_run_load(&loaded, saved, n, &why));
    CHECK(loaded.n_items == 2 && loaded.items[0].stack == 1);
    CHECK(loaded.items[0].pokemon_id == id && loaded.items[0].arg == BERRY_TYPE_LEPPA);
    CHECK(loaded.bt.mods[0][0].cls == PR_MOD_BERRY && loaded.bt.mods[0][0].stack_count == 1);
    CHECK(loaded.bt.mods[0][1].cls == PR_MOD_RESET_NEGATIVE_STAT_STAGE);
    printf("item bridge lifecycle: %d checks passed\n", checks);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc == 2) return differential(argv[1]);
    if (argc != 1) return 2;
    return lifecycle();
}
