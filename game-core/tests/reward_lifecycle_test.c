/* Replay the additive reward-lifecycle producer. Complete traces only. */
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
        fprintf(stderr, "incomplete/invalid lifecycle trace (expected %s)\n", want); exit(2);
    }
}
static int number(FILE *f) { int n; if (fscanf(f, "%d", &n) != 1) exit(2); return n; }
static void values(const gc_mon *m, int out[27])
{
    const pr_pokemon *p = &m->b;
    int i = 0;
    out[i++] = p->species; out[i++] = p->form; out[i++] = p->level;
    out[i++] = p->nature; out[i++] = m->ability_index; out[i++] = p->base_ability;
    out[i++] = m->pause_evolutions; out[i++] = p->hp;
    for (int j = 0; j < 6; j++) out[i++] = p->stats[j];
    out[i++] = p->n_moves;
    for (int j = 0; j < 4; j++) {
        out[i++] = p->moves[j]; out[i++] = p->pp_used[j]; out[i++] = p->pp_up[j];
    }
}

int main(int argc, char **argv)
{
    if (argc != 2) return 2;
    FILE *f = fopen(argv[1], "r"); if (!f) return 2;
    token(f, "V"); if (number(f) != 1) return 2; int n = number(f); if (n < 7) return 2;
    int failures = 0, counts[7] = {0};
    for (int c = 0; c < n; c++) {
        token(f, "C"); if (number(f) != c) return 2;
        int action = number(f), gender = number(f), shiny = number(f), ivs[6];
        if (action < 0 || action > 6) return 2;
        for (int j = 0; j < 6; j++) ivs[j] = number(f);
        token(f, "R"); pr_rng rng = {0};
        if (fscanf(f, "%lf %lf %lf %lf", &rng.c, &rng.s0, &rng.s1, &rng.s2) != 4) return 2;
        token(f, "I"); int input[27]; for (int j = 0; j < 27; j++) input[j] = number(f);
        memset(&sys, 0, sizeof sys);
        gc_run_new(&run, GAME_MODES_CLASSIC, "lifecycle", 123);
        run.sys = &sys; run.n_party = 1; run.party_order[0] = 0;
        gc_starter s = {.species = input[0], .form = input[1], .ability_index = input[4], .nature = input[3], .tera_type = -1};
        for (int j = 0; j < 6; j++) s.ivs[j] = ivs[j];
        gc_mon *m = &run.party[0];
        gc_mon_new_player(&run, m, &s, input[2]);
        /* The producer clears dex records after constructing/calculating inputs. */
        memset(&sys, 0, sizeof sys);
        m->b.gender = gender; m->b.shiny = shiny;
        sys.dex[input[0]].caught_attr = 1;
        m->pause_evolutions = input[6]; m->b.hp = input[7];
        for (int j = 0; j < 6; j++) m->b.stats[j] = input[8+j];
        m->b.n_moves = input[14];
        for (int j = 0; j < 4; j++) {
            m->b.moves[j] = input[15+j*3]; m->b.pp_used[j] = input[16+j*3]; m->b.pp_up[j] = input[17+j*3];
        }
        run.run.rng = rng;
        memset(run.pokeballs,0,sizeof run.pokeballs);
        gc_rq_reset(&gc_int(&run)->rq);
        run.screen.kind = GC_SCREEN_NONE;
        gc_offer o = {.key = action < 3 ? "EVOLUTION_ITEM" : "FORM_CHANGE_ITEM",
                      .arg = action < 3 ? 5 : FORM_CHANGE_ITEM_GRISEOUS_CORE, .target = GC_TARGET_POKEMON, .tier = 2};
        if (action == 6) gc_item_add(&run,o.key,o.arg,m->b.id,1);
        const char *why = NULL;
        bool accepted = action != 5 && gc_port_apply_reward(&run, &o, action == 2 ? 99 : 0, -1, &why);
        token(f, "A"); int expected_accept = number(f), expected_queue = number(f);
        int queued = gc_rq_has(&run, RP_EVOLUTION) + gc_rq_has(&run, RP_REWARD_FORM);
        bool bad = accepted != expected_accept || queued != expected_queue;
        gc_run_until_screen(&run);
        if (accepted && action < 2) {
            if (run.screen.kind != GC_SCREEN_EVOLVE) bad = true;
            else {
                gc_choice ch = {action == 0 ? GC_CH_EVOLVE : GC_CH_CANCEL, 1, 0, 0};
                if (!gc_run_choose(&run, &ch, &why)) bad = true;
            }
        }
        token(f, "T"); int actual[27]; values(m, actual);
        for (int j = 0; j < 27; j++) {
            int expected = number(f);
            if (actual[j] != expected) {
                if (failures < 8) fprintf(stderr, "case %d state[%d]: C=%d upstream=%d\n", c, j, actual[j], expected);
                bad = true;
            }
        }
        for (int j = 0; j < 3; j++) {
            token(f, "D"); int id = number(f); uint64_t seen, caught; unsigned nature;
            if (id < 1 || id > GC_MAX_SPECIES_ID || fscanf(f, "%" SCNu64 " %" SCNu64 " %u", &seen, &caught, &nature) != 3) return 2;
            const gc_dex_entry *d = &sys.dex[id];
            if (d->seen_attr != seen || d->caught_attr != caught || d->nature_attr != nature) {
                if (failures < 8) fprintf(stderr, "case %d dex %d: C=%" PRIu64 "/%" PRIu64 "/%u upstream=%" PRIu64 "/%" PRIu64 "/%u\n", c,id,d->seen_attr,d->caught_attr,d->nature_attr,seen,caught,nature);
                bad = true;
            }
            for (int k = 0; k < 6; k++) if (d->ivs[k] != number(f)) bad = true;
        }
        token(f, "H"); int ni = number(f), stack = number(f), owned = number(f);
        if (run.n_items != ni || (ni && (run.items[0].stack != stack || (run.items[0].pokemon_id == m->b.id) != owned))) bad = true;
        token(f,"B"); for (int j = 0; j < 5; j++) if (run.pokeballs[j] != number(f)) bad = true;
        token(f, "S"); pr_rng after = {0};
        if (fscanf(f, "%lf %lf %lf %lf", &after.c, &after.s0, &after.s1, &after.s2) != 4) return 2;
        if (run.run.rng.c != after.c || run.run.rng.s0 != after.s0 || run.run.rng.s1 != after.s1 || run.run.rng.s2 != after.s2) bad = true;
        token(f, "E"); counts[action]++; failures += bad;
    }
    token(f, "Z"); if (number(f) != n) return 2;
    for (int j = 0; j < 7; j++) if (!counts[j]) return 2;
    char trailing[2]; if (fscanf(f, "%1s", trailing) == 1) return 2;
    fclose(f);
    printf("reward lifecycle: %d cases, %d identical, %d mismatches; paths=%d/%d/%d/%d/%d/%d/%d\n", n,n-failures,failures,counts[0],counts[1],counts[2],counts[3],counts[4],counts[5],counts[6]);
    return failures ? 1 : 0;
}
