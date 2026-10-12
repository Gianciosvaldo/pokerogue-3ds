/* Replays the upstream evolution-item trace (tests/item-evolution.producer.test.ts) through gc_port_apply_reward: whether the
 * item is taken, and the Pokemon's state, dex records and RNG after the evolution is chosen. Usage: item_evolution_test <trace>.
 * Exit 0 only with 0 mismatches. */
#include "../src/internal.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

static gc_run run;
static gc_system sys;
static void token(FILE *f, const char *want)
{
    char s[32];
    if (fscanf(f, "%31s", s) != 1 || strcmp(s, want)) {
        fprintf(stderr, "incomplete/invalid item evolution trace (expected %s, got %s)\n", want, s); exit(2);
    }
}
static int number(FILE *f) { int n; if (fscanf(f, "%d", &n) != 1) exit(2); return n; }
#define NV 26
static void values(const gc_mon *m, int out[NV])
{
    const pr_pokemon *p = &m->b;
    int i = 0;
    out[i++] = p->species; out[i++] = p->form; out[i++] = p->level;
    out[i++] = m->ability_index; out[i++] = p->base_ability;
    out[i++] = m->pause_evolutions; out[i++] = p->hp;
    for (int j = 0; j < 6; j++) out[i++] = p->stats[j];
    out[i++] = p->n_moves;
    for (int j = 0; j < 4; j++) { out[i++] = p->moves[j]; out[i++] = p->pp_used[j]; out[i++] = p->pp_up[j]; }
}

int main(int argc, char **argv)
{
    if (argc != 2) return 2;
    FILE *f = fopen(argv[1], "r"); if (!f) return 2;
    token(f, "V"); if (number(f) != 1) return 2; int n = number(f); if (n < 100) return 2;
    int level_failures = 0, level_cases = 0, failures = 0, accepted_n = 0, rejected_n = 0, shown = 0;
    const int show_max = getenv("GC_IEVO_SHOW") ? atoi(getenv("GC_IEVO_SHOW")) : 8;
    for (int c = 0; c < n; c++) {
        token(f, "C"); if (number(f) != c) return 2;
        const int species = number(f), form = number(f), level = number(f), gender = number(f), friendship = number(f);
        const int nature = number(f), pause = number(f), item = number(f), biome = number(f), wave = number(f), offset = number(f);
        const int shiny = number(f), ability_index = number(f);
        int ivs[6];
        for (int j = 0; j < 6; j++) ivs[j] = number(f);
        bool level_bad = false;
        token(f, "R"); pr_rng rng = {0};
        if (fscanf(f, "%lf %lf %lf %lf", &rng.c, &rng.s0, &rng.s1, &rng.s2) != 4) return 2;
        token(f, "I"); int input[NV]; for (int j = 0; j < NV; j++) input[j] = number(f);
        token(f, "G"); const int root = number(f), root_caught = number(f);
        if (root != gc_species_root_id(species)) { fprintf(stderr, "case %d root %d != %d\n", c, root, gc_species_root_id(species)); return 2; }
        int ncaught = number(f), caught[8];
        if (ncaught > 8) return 2;
        for (int j = 0; j < ncaught; j++) caught[j] = number(f);
        token(f, "H"); int nheld = number(f), held[8], rare[8];
        if (nheld > 8) return 2;
        for (int j = 0; j < nheld; j++) { rare[j] = number(f); held[j] = number(f); }

        token(f, "Q"); char seed[64]; unsigned long long id; int level_evo;
        if (fscanf(f, "%63s %llu %d", seed, &id, &level_evo) != 3) return 2;
        memset(&sys, 0, sizeof sys);
        memset(&run, 0, sizeof run);
        gc_run_new(&run, GAME_MODES_CLASSIC, seed, 123);
        run.sys = &sys; run.n_party = 1; run.party_order[0] = 0;
        gc_starter s = {.species = species, .form = form, .ability_index = ability_index, .nature = nature, .tera_type = -1};
        for (int j = 0; j < 6; j++) s.ivs[j] = ivs[j];
        gc_mon *m = &run.party[0];
        gc_mon_new_player(&run, m, &s, level);
        m->b.id = (uint32_t)id;   /* RANDOM_FORM evolutions read the run seed shifted by the Pokemon's id */
        memset(&sys, 0, sizeof sys);
        m->b.gender = gender; m->b.shiny = shiny; m->b.friendship = friendship;
        sys.dex[species].caught_attr = 1;
        if (root_caught) sys.dex[root].caught_attr = 1;
        for (int j = 0; j < ncaught; j++) sys.dex[caught[j]].caught_attr = 1;
        m->pause_evolutions = pause; m->b.hp = input[6];
        /* the producer's ability (an index past the species' abilities is clamped upstream) is an input */
        m->b.base_ability = (uint16_t)input[4];
        if (m->b.ability_override < 0) m->b.ability = m->b.base_ability;
        for (int j = 0; j < 6; j++) m->b.stats[j] = input[7 + j];
        m->b.n_moves = input[13];
        for (int j = 0; j < 4; j++) {
            m->b.moves[j] = input[14 + j * 3]; m->b.pp_used[j] = input[15 + j * 3]; m->b.pp_up[j] = input[16 + j * 3];
        }
        run.biome = biome; run.wave = wave; run.wave_cycle_offset = offset;
        run.run.rng = rng;
        for (int j = 0; j < nheld; j++) gc_item_add(&run, rare[j] ? "RARE_SPECIES_STAT_BOOSTER" : "SPECIES_STAT_BOOSTER", held[j], m->b.id, 1);
        gc_rq_reset(&gc_int(&run)->rq);
        run.screen.kind = GC_SCREEN_NONE;

        {   /* getEvolution(): the level-up check, on the same Pokemon */
            const int le = gc_mon_get_evolution(&run, m);
            const int got = le < 0 ? 0 : gc_evo_data[le].species;
            if (got != level_evo) {
                if (shown < show_max) fprintf(stderr, "case %d species %d level %d: getEvolution C=%d upstream=%d\n", c, species, level, got, level_evo);
                level_bad = true; level_failures++;
            }
        }
        gc_offer o = {.key = item > 50 ? "RARE_EVOLUTION_ITEM" : "EVOLUTION_ITEM", .arg = item, .target = GC_TARGET_POKEMON, .tier = 2};
        const char *why = NULL;
        bool accepted = gc_port_apply_reward(&run, &o, 0, -1, &why);
        token(f, "A"); int expected_accept = number(f), expected_queue = number(f);
        int queued = gc_rq_has(&run, RP_EVOLUTION);
        bool bad = level_bad || accepted != expected_accept || queued != expected_queue;
        level_cases += level_evo != 0;
        if (bad && shown < show_max) fprintf(stderr, "case %d species %d item %d form %d level %d: accept C=%d upstream=%d\n", c, species, item, form, level, accepted, expected_accept);
        gc_run_until_screen(&run);
        if (accepted && run.screen.kind == GC_SCREEN_EVOLVE) {
            gc_choice ch = {GC_CH_EVOLVE, 1, 0, 0};
            if (!gc_run_choose(&run, &ch, &why)) bad = true;
        } else if (accepted) bad = true;
        token(f, "T"); int actual[NV]; values(m, actual);
        for (int j = 0; j < NV; j++) {
            int expected = number(f);
            if (actual[j] != expected) {
                if (shown < show_max) fprintf(stderr, "case %d (species %d item %d) state[%d]: C=%d upstream=%d\n", c, species, item, j, actual[j], expected);
                bad = true;
            }
        }
        token(f, "D"); int nd = number(f);
        for (int j = 0; j < nd; j++) {
            token(f, "X"); int id = number(f); uint64_t seen, caught_attr; unsigned nat;
            if (id < 1 || id > GC_MAX_SPECIES_ID || fscanf(f, "%" SCNu64 " %" SCNu64 " %u", &seen, &caught_attr, &nat) != 3) return 2;
            const gc_dex_entry *d = &sys.dex[id];
            if (d->seen_attr != seen || d->caught_attr != caught_attr || d->nature_attr != nat) {
                if (shown < show_max) fprintf(stderr, "case %d dex %d: C=%" PRIu64 "/%" PRIu64 "/%u upstream=%" PRIu64 "/%" PRIu64 "/%u\n", c, id, d->seen_attr, d->caught_attr, d->nature_attr, seen, caught_attr, nat);
                bad = true;
            }
            for (int k = 0; k < 6; k++) if (d->ivs[k] != number(f)) bad = true;
        }
        token(f, "S"); pr_rng after = {0};
        if (fscanf(f, "%lf %lf %lf %lf", &after.c, &after.s0, &after.s1, &after.s2) != 4) return 2;
        if (run.run.rng.c != after.c || run.run.rng.s0 != after.s0 || run.run.rng.s1 != after.s1 || run.run.rng.s2 != after.s2) {
            if (shown < show_max) fprintf(stderr, "case %d rng differs\n", c);
            bad = true;
        }
        token(f, "E");
        if (bad) shown++;
        failures += bad; accepted_n += expected_accept; rejected_n += !expected_accept;
    }
    token(f, "Z"); if (number(f) != n) return 2;
    char trailing[2]; if (fscanf(f, "%1s", trailing) == 1) return 2;
    fclose(f);
    printf("item evolution: %d cases (accepted %d, rejected %d; level-up check evolves %d, mismatches %d), %d identical, TOTAL mismatches %d\n", n, accepted_n, rejected_n, level_cases, level_failures, n - failures, failures);
    return failures ? 1 : 0;
}
