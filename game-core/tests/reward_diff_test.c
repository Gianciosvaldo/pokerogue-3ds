/* Replays tests/reward.producer.test.ts's cases (upstream 8555c08) through game-core's reward adapter: each case's
 * party, items, balls, unlocks, arena and learnsets are set up as game-core holds them, then
 *   - every value gc_reward_ctx hands reward-core is compared with what upstream's own calls return (the party
 *     member's abilities, statuses, types, moves and PP, the TM and Memory Mushroom learnsets, egg moves,
 *     item-spawn types, treasure counts; the items with their holders; luck, time of day, weather...),
 *   - the screens (first roll or no-victory screen, rerolls with and without the Lock Capsule, the copy) are
 *     rolled through gc_port_reward_options from upstream's RNG state and compared: options, arguments, tiers,
 *     upgrade counts, the reroll price and the RNG state after.
 * Usage: reward_diff_test <cases file> */
#include "../src/internal.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static FILE *input;
static gc_run run;
static gc_system sys;

static long long integer(void)
{
    long long v;
    if (fscanf(input, "%lld", &v) != 1) { fputs("malformed input\n", stderr); exit(2); }
    return v;
}
static double real(void)
{
    double v;
    if (fscanf(input, "%lf", &v) != 1) { fputs("malformed input\n", stderr); exit(2); }
    return v;
}
static void word(char *out, size_t cap)
{
    char fmt[16];
    snprintf(fmt, sizeof fmt, "%%%zus", cap - 1);
    if (fscanf(input, fmt, out) != 1) { fputs("malformed input\n", stderr); exit(2); }
}
static int count(int cap)
{
    const long long n = integer();
    if (n < 0 || n > cap) { fprintf(stderr, "invalid count %lld (cap %d)\n", n, cap); exit(2); }
    return (int)n;
}
static void record(char expected)
{
    char c;
    if (fscanf(input, " %c", &c) != 1 || c != expected) { fprintf(stderr, "wrong record: wanted %c, got %c\n", expected, c); exit(2); }
}

/* mismatches by what differed */
#define MAX_KINDS 64
static const char *kind_name[MAX_KINDS];
static long kind_count[MAX_KINDS];
static int kinds, cases_bad_inputs, cases_bad_screens;
static int case_bad;   /* mismatches in the case being replayed */
static int shown;

static void note(const char *what, int mon, long a, long b)
{
    int k = 0;
    while (k < kinds && strcmp(kind_name[k], what)) k++;
    if (k == kinds) {
        if (kinds == MAX_KINDS) return;
        kind_name[kinds++] = what;
    }
    kind_count[k]++;
    case_bad++;
    if (shown < 24) { shown++; fprintf(stderr, "mismatch: %s (mon %d): game-core %ld, upstream %ld\n", what, mon, a, b); }
}
#define EQ(what, mon, ours, theirs) do { const long a_ = (long)(ours), b_ = (long)(theirs); if (a_ != b_) note(what, mon, a_, b_); } while (0)

typedef struct { int16_t level; uint16_t move; int8_t source; } row;
static int read_rows(row *rows, int cap)
{
    const int n = count(cap);
    for (int i = 0; i < n; i++) {
        rows[i].level = (int16_t)integer();
        rows[i].move = (uint16_t)integer();
        rows[i].source = (int8_t)integer();
    }
    return n;
}

typedef struct {
    uint32_t id;
    int species, form, fspecies, fform, gender, level, hp, max_hp, status, nature, tera, friendship, ability_idx;
    int base_ability, cur_ability, passive_id, passive_flag, shiny, variant, pause, starter;
    int ivs[6], stats[6];
    int can_apply, can_apply_passive, allowed, can_toxic, can_burn, learnable, treasure;
    int n_types, types[4], n_moves, mv[4][4];
    row l[PR_REWARD_MAX_LEVEL_MOVES], r[PR_REWARD_MAX_LEVEL_MOVES], m[PR_REWARD_MAX_LEVEL_MOVES + 64];
    int nl, nr, nm, n_egg, egg[4], n_attack, attack[PR_REWARD_MAX_ATTACK_TYPES];
} mon_case;

static bool dex_caught(const int *list, int n, int s)
{
    for (int i = 0; i < n; i++) if (list[i] == s) return true;
    return false;
}

int main(int argc, char **argv)
{
    if (argc != 2 || !(input = fopen(argv[1], "r"))) { fputs("usage: reward_diff_test <cases>\n", stderr); return 2; }
    record('V');
    if (integer() != 1) return 2;
    const int n = (int)integer();
    int cases = 0, mons = 0, screens = 0, options = 0, skipped = 0, stat_info = 0;
    long fields = 0;
    static const int caught_list[] = {SPECIES_ID_SHELMET, SPECIES_ID_KARRABLAST, SPECIES_ID_RESHIRAM, SPECIES_ID_ZEKROM,
                                      SPECIES_ID_SOLGALEO, SPECIES_ID_LUNALA, SPECIES_ID_GLASTRIER, SPECIES_ID_SPECTRIER};
    char tag;
    while (fscanf(input, " %c", &tag) == 1) {
        if (tag == 'Z') break;
        if (tag == 'X') { skipped++; char skip[128]; word(skip, sizeof skip); continue; }
        if (tag != 'H') { fprintf(stderr, "wrong record %c\n", tag); return 2; }
        case_bad = 0;
        char seed[64];
        word(seed, sizeof seed);
        const int wave = (int)integer(), biome = (int)integer(), weather = (int)integer(), terrain = (int)integer(), tod = (int)integer(),
                  offset = (int)integer(), eviolite = (int)integer(), mbh = (int)integer();
        int caught[8];
        const int n_caught = count(8);
        for (int i = 0; i < n_caught; i++) caught[i] = (int)integer();
        int balls[5];
        for (int i = 0; i < 5; i++) balls[i] = (int)integer();
        const int luck = (int)integer();
        const int size = count(6);
        const int n_egg_species = count(64);
        int egg_species[64], egg_bits[64];
        for (int i = 0; i < n_egg_species; i++) { egg_species[i] = (int)integer(); egg_bits[i] = (int)integer(); }

        static mon_case mc[6];
        for (int k = 0; k < size; k++) {
            mon_case *c = &mc[k];
            record('P');
            c->id = (uint32_t)integer();
            c->species = (int)integer(); c->form = (int)integer(); c->fspecies = (int)integer(); c->fform = (int)integer();
            c->gender = (int)integer(); c->level = (int)integer(); c->hp = (int)integer(); c->max_hp = (int)integer();
            c->status = (int)integer(); c->nature = (int)integer(); c->tera = (int)integer(); c->friendship = (int)integer();
            c->ability_idx = (int)integer(); c->base_ability = (int)integer(); c->cur_ability = (int)integer();
            c->passive_id = (int)integer(); c->passive_flag = (int)integer(); c->shiny = (int)integer();
            c->variant = (int)integer(); c->pause = (int)integer(); c->starter = (int)integer();
            for (int i = 0; i < 6; i++) c->ivs[i] = (int)integer();
            for (int i = 0; i < 6; i++) c->stats[i] = (int)integer();
            c->can_apply = (int)integer(); c->can_apply_passive = (int)integer(); c->allowed = (int)integer();
            c->can_toxic = (int)integer(); c->can_burn = (int)integer(); c->learnable = (int)integer();
            c->treasure = (int)integer();
            c->n_types = count(4);
            for (int i = 0; i < c->n_types; i++) c->types[i] = (int)integer();
            c->n_moves = count(4);
            for (int i = 0; i < c->n_moves; i++) for (int j = 0; j < 4; j++) c->mv[i][j] = (int)integer();
            c->nl = read_rows(c->l, PR_REWARD_MAX_LEVEL_MOVES);
            c->nr = read_rows(c->r, PR_REWARD_MAX_LEVEL_MOVES);
            c->nm = read_rows(c->m, PR_REWARD_MAX_LEVEL_MOVES + 64);
            c->n_egg = count(4);
            for (int i = 0; i < c->n_egg; i++) c->egg[i] = (int)integer();
            c->n_attack = count(PR_REWARD_MAX_ATTACK_TYPES);
            for (int i = 0; i < c->n_attack; i++) c->attack[i] = (int)integer();
        }
        record('I');
        static struct { char key[64]; int arg, stack, owner; } it[64];
        const int n_items = count(64);
        for (int i = 0; i < n_items; i++) {
            word(it[i].key, sizeof it[i].key);
            it[i].arg = (int)integer(); it[i].stack = (int)integer(); it[i].owner = (int)integer();
        }

        /* ---- game-core's state ---- */
        memset(&run, 0, sizeof run);
        gc_system_init_defaults(&sys);
        gc_run_new(&run, GAME_MODES_CLASSIC, "rewarddiff", 7);
        gc_starter st;
        memset(&st, 0, sizeof st);
        st.species = SPECIES_ID_BULBASAUR;
        st.tera_type = -1;
        const char *why;
        if (!gc_run_set_starters(&run, &st, 1, &why)) { fprintf(stderr, "starters: %s\n", why); return 2; }
        run.sys = &sys;
        run.wave = wave;
        run.biome = (uint8_t)biome;
        run.wave_cycle_offset = offset;
        run.bt.weather = (uint8_t)weather;
        run.bt.terrain = (uint8_t)terrain;
        run.bt.weather_turns_left = run.bt.weather_max_duration = weather ? 5 : 0;
        run.bt.terrain_turns_left = run.bt.terrain_max_duration = terrain ? 5 : 0;
        pr_seed_from_ascii(&run.run.seed, seed);
        sys.unlocks.eviolite = eviolite;
        sys.unlocks.mini_black_hole = mbh;
        for (unsigned i = 0; i < sizeof caught_list / sizeof *caught_list; i++)
            sys.dex[caught_list[i]].caught_attr = dex_caught(caught, n_caught, caught_list[i]) ? 1 : 0;
        for (int i = 0; i < n_egg_species; i++)
            if (egg_species[i] > 0 && egg_species[i] <= GC_MAX_SPECIES_ID) sys.starters[egg_species[i]].egg_moves = (uint8_t)egg_bits[i];
        for (int i = 0; i < 5; i++) run.pokeballs[i] = balls[i];
        run.n_party = size;
        /* the party's slots in a different order from upstream's, to check party_order is what's read */
        int slot[6];
        for (int k = 0; k < size; k++) slot[k] = (k + cases) % size;
        if (cases & 1) for (int k = 0; k < size; k++) slot[k] = size - 1 - slot[k];
        for (int k = 0; k < size; k++) {
            const mon_case *c = &mc[k];
            gc_starter s;
            memset(&s, 0, sizeof s);
            s.species = (uint16_t)c->species;
            s.form = (uint8_t)c->form;
            s.ability_index = (uint8_t)c->ability_idx;
            s.nature = (uint8_t)c->nature;
            s.female = c->gender == 1;
            s.shiny = c->shiny;
            s.variant = (uint8_t)c->variant;
            s.passive = c->passive_flag;
            s.tera_type = (int8_t)c->tera;
            for (int i = 0; i < 6; i++) s.ivs[i] = (uint8_t)c->ivs[i];
            for (int i = 0; i < c->n_moves; i++) s.moves[i] = (uint16_t)c->mv[i][0];
            gc_mon *m = &run.party[slot[k]];
            gc_mon_new_player(&run, m, &s, c->level);
            pr_pokemon *p = &m->b;
            for (int i = 0; i < 6; i++) stat_info += p->stats[i] != c->stats[i];
            p->id = c->id;
            p->gender = (int8_t)c->gender;
            p->fusion_species = (uint16_t)(c->fspecies > 0 ? c->fspecies : 0);
            p->fusion_form = (uint8_t)c->fform;
            p->friendship = (uint8_t)c->friendship;
            p->base_ability = (uint16_t)c->base_ability;
            p->ability = (uint16_t)c->cur_ability;
            p->passive = (uint16_t)c->passive_id;
            p->passive_flag = c->passive_flag;
            for (int i = 0; i < 6; i++) p->stats[i] = (int16_t)c->stats[i];
            p->hp = (int32_t)c->hp;
            p->status = (uint8_t)c->status;
            for (int i = 0; i < c->n_moves; i++) {
                p->moves[i] = (uint16_t)c->mv[i][0];
                p->pp_used[i] = (uint8_t)c->mv[i][1];
                p->pp_up[i] = (uint8_t)c->mv[i][2];
                p->max_pp_override[i] = -1;
            }
            p->n_moves = (uint8_t)c->n_moves;
            m->pause_evolutions = c->pause;
            m->met_wave = c->starter ? -1 : 3;
            run.party_order[k] = (int8_t)slot[k];
            p->id = c->id;
        }
        /* upstream's field is the first party member: allies' abilities (Pastel Veil, Flower Veil) reach everyone */
        run.bt.field[0] = &run.party[slot[0]].b;
        run.bt.field[1] = NULL;
        run.n_items = 0;
        for (int i = 0; i < n_items; i++) {
            const int idx = gc_reward_item_index(it[i].key);
            if (idx < 0) { fprintf(stderr, "unknown item %s\n", it[i].key); return 2; }
            gc_item *g = &run.items[run.n_items++];
            g->key = pr_item_keys[idx];
            g->arg = (int16_t)it[i].arg;
            g->stack = it[i].stack;
            g->pokemon_id = it[i].owner >= 0 ? mc[it[i].owner].id : 0;
        }

        /* ---- the inputs ---- */
        pr_reward_ctx *ctx = gc_reward_ctx(&run);
        fields += 8;
        EQ("ctx.wave", -1, ctx->wave, wave);
        EQ("ctx.luck", -1, ctx->luck, luck);
        EQ("ctx.biome", -1, ctx->biome, biome);
        EQ("ctx.time_of_day", -1, ctx->time_of_day, tod);
        EQ("ctx.weather", -1, ctx->weather, weather);
        EQ("ctx.eviolite", -1, ctx->unlocked_eviolite, eviolite);
        EQ("ctx.mini_black_hole", -1, ctx->unlocked_mini_black_hole, mbh);
        EQ("ctx.n_party", -1, ctx->n_party, size);
        for (int i = 0; i < 5; i++) EQ("ctx.pokeball_counts", -1, ctx->pokeball_counts[i], balls[i]);
        for (unsigned i = 0; i < sizeof caught_list / sizeof *caught_list; i++)
            EQ("ctx.caught", -1, ctx->caught(ctx->caught_user, caught_list[i]), dex_caught(caught, n_caught, caught_list[i]));
        fields += 5 + 8;
        EQ("ctx.n_items", -1, ctx->n_items, n_items);
        for (int i = 0; i < ctx->n_items && i < n_items; i++) {
            const pr_owned_item *o = &ctx->items[i];
            EQ("item.key", i, strcmp(pr_item_keys[o->item], it[i].key) != 0, 0);
            EQ("item.arg", i, o->arg, it[i].arg);
            EQ("item.stack", i, o->stack, it[i].stack);
            EQ("item.owner", i, o->owner, it[i].owner);
            fields += 4;
        }
        for (int k = 0; k < size && k < ctx->n_party; k++) {
            const mon_case *c = &mc[k];
            const pr_reward_mon *r = &ctx->party[k];
            EQ("mon.species", k, r->species, c->species);
            EQ("mon.form", k, r->form, c->form);
            EQ("mon.fusion_species", k, r->fusion_species, c->fspecies);
            EQ("mon.fusion_form", k, r->fusion_form, c->fform);
            EQ("mon.gender", k, r->gender, c->gender);
            EQ("mon.level", k, r->level, c->level);
            EQ("mon.hp", k, r->hp, c->hp);
            EQ("mon.max_hp", k, r->max_hp, c->max_hp);
            EQ("mon.status", k, r->status, c->status);
            EQ("mon.nature", k, r->nature, c->nature);
            EQ("mon.tera_type", k, r->tera_type, c->tera);
            EQ("mon.friendship", k, r->friendship, c->friendship);
            EQ("mon.ability", k, r->ability, c->base_ability);
            EQ("mon.cur_ability", k, r->cur_ability, c->cur_ability);
            EQ("mon.passive", k, r->passive, c->passive_id);
            EQ("mon.can_apply_ability", k, r->can_apply_ability, c->can_apply);
            EQ("mon.can_apply_passive", k, r->can_apply_passive, c->can_apply_passive);
            EQ("mon.pause_evolutions", k, r->pause_evolutions, c->pause);
            EQ("mon.allowed_in_challenge", k, r->allowed_in_challenge, c->allowed);
            EQ("mon.can_toxic", k, r->can_toxic, c->can_toxic);
            EQ("mon.can_burn", k, r->can_burn, c->can_burn);
            EQ("mon.persistent_treasure", k, r->persistent_treasure, c->treasure);
            EQ("mon.met_as_starter", k, r->met_as_starter, c->starter);
            fields += 23;
            EQ("mon.n_types", k, r->n_types, c->n_types);
            for (int i = 0; i < c->n_types && i < r->n_types; i++) EQ("mon.types", k, r->types[i], c->types[i]);
            EQ("mon.n_moves", k, r->n_moves, c->n_moves);
            for (int i = 0; i < c->n_moves && i < r->n_moves; i++) {
                EQ("mon.move", k, r->moves[i].move, c->mv[i][0]);
                EQ("mon.move_pp_used", k, r->moves[i].pp_used, c->mv[i][1]);
                EQ("mon.move_max_pp", k, r->moves[i].max_pp, c->mv[i][3]);
            }
            fields += 2 + c->n_types + c->n_moves * 3;
            /* the TM exclusion list and the Memory Mushroom's own list */
            EQ("learnset.tm_count", k, r->n_level_moves, c->nl);
            for (int i = 0; i < c->nl && i < r->n_level_moves; i++) {
                const bool same = r->level_moves[i] == c->l[i].move && r->level_move_levels[i] == c->l[i].level
                                  && r->level_move_sources[i] == c->l[i].source;
                EQ("learnset.tm_row", k, same, 1);
            }
            EQ("learnset.memory_count", k, r->n_relearn_moves, c->nr);
            for (int i = 0; i < c->nr && i < r->n_relearn_moves; i++) {
                const bool same = r->relearn_moves[i] == c->r[i].move && r->relearn_move_levels[i] == c->r[i].level
                                  && r->relearn_move_sources[i] == c->r[i].source;
                EQ("learnset.memory_row", k, same, 1);
            }
            fields += 2 + c->nl + c->nr;
            if (c->starter) {
                EQ("mon.egg_count", k, r->n_egg_moves, c->n_egg);
                for (int i = 0; i < c->n_egg && i < r->n_egg_moves; i++) EQ("mon.egg_move", k, r->egg_moves[i], c->egg[i]);
                fields += 1 + c->n_egg;
            }
            EQ("mon.attack_type_count", k, r->n_attack_types, c->n_attack);
            for (int i = 0; i < c->n_attack && i < r->n_attack_types; i++) EQ("mon.attack_type", k, r->attack_types[i], c->attack[i]);
            fields += 1 + c->n_attack;
            /* getLearnableLevelMoves(): reward-core's own list from those rows */
            pr_reward_memory_move got[PR_REWARD_MAX_LEVEL_MOVES + PR_REWARD_MAX_USED_TMS + 4];
            const int ng = pr_reward_memory_moves(ctx, k, got, (int)(sizeof got / sizeof got[0]));
            EQ("learnable.count", k, ng, c->nm);
            for (int i = 0; i < c->nm && i < ng; i++)
                EQ("learnable.row", k, got[i].move == c->m[i].move && got[i].level == c->m[i].level && got[i].source == c->m[i].source, 1);
            EQ("mon.has_learnable", k, r->has_learnable_level_moves, c->learnable);
            fields += 2 + c->nm;
            mons++;
        }
        if (case_bad) cases_bad_inputs++;
        const int inputs_bad = case_bad;

        /* ---- the screens ---- */
        gc_internal *in = gc_int(&run);
        int cases_screens = 0;
        while (fscanf(input, " %c", &tag) == 1 && tag == 'S') {
            const int kind = (int)integer(), reroll_count = (int)integer(), lock = (int)integer();
            pr_rng before = {0};
            before.c = real(); before.s0 = real(); before.s1 = real(); before.s2 = real();
            record('O');
            integer();   /* getModifierCount() */
            const int no = count(PR_REWARD_MAX_OPTIONS);
            struct { char key[64]; int arg, tier, upgrade; } want[PR_REWARD_MAX_OPTIONS];
            for (int i = 0; i < no; i++) {
                word(want[i].key, sizeof want[i].key);
                want[i].arg = (int)integer(); want[i].tier = (int)integer(); want[i].upgrade = (int)integer();
            }
            const double cost = real();
            pr_rng after = {0};
            after.c = real(); after.s0 = real(); after.s1 = real(); after.s2 = real();

            in->lock_rarities = lock;
            run.run.rng = before;
            gc_offer out[GC_MAX_OPTIONS];
            double got_cost = -2;
            int got;
            switch (kind) {
            case 0: got = gc_port_reward_options(&run, GC_REWARD_FIRST, out, &got_cost); break;
            case 1: got = gc_port_reward_options(&run, GC_REWARD_RESTRICTED, out, &got_cost); break;
            case 2: got = gc_port_reward_options(&run, GC_REWARD_REROLL, out, &got_cost); break;
            default: gc_reward_queue_copy(&run); got = gc_port_reward_options(&run, GC_REWARD_COPY, out, &got_cost); break;
            }
            EQ("screen.reroll_count", kind, in->reroll_count, reroll_count);
            EQ("screen.option_count", kind, got, no);
            for (int i = 0; i < no && i < got; i++) {
                EQ("screen.option_key", kind, strcmp(out[i].key, want[i].key) != 0, 0);
                EQ("screen.option_arg", kind, out[i].arg, want[i].arg);
                EQ("screen.option_tier", kind, out[i].tier, want[i].tier);
                EQ("screen.option_upgrade", kind, out[i].upgrade_count, want[i].upgrade);
                options++;
            }
            EQ("screen.reroll_cost", kind, got_cost, cost);
            const bool rng_same = run.run.rng.c == after.c && run.run.rng.s0 == after.s0 && run.run.rng.s1 == after.s1
                                  && run.run.rng.s2 == after.s2;
            EQ("screen.rng_after", kind, rng_same, 1);
            EQ("screen.stub", kind, run.stubs[GC_STUB_REWARDS] + run.stubs[GC_STUB_REWARD_INPUT], 0);
            screens++;
            cases_screens++;
        }
        if (tag != 'E') { fprintf(stderr, "wrong record %c\n", tag); return 2; }
        if (case_bad > inputs_bad) cases_bad_screens++;
        cases++;
    }
    long bad = 0;
    for (int k = 0; k < kinds; k++) bad += kind_count[k];
    printf("reward adapter differential: %d cases (%d skipped by upstream), %d party members, %d screens, %d options, "
           "%ld input fields compared; %ld mismatches (%d cases with input mismatches, %d with screen mismatches); "
           "game-core's own stats differed from upstream's in %d of the %d stats it set up\n",
           cases, skipped, mons, screens, options, fields, bad, cases_bad_inputs, cases_bad_screens, stat_info, mons * 6);
    for (int k = 0; k < kinds; k++) printf("  %s: %ld\n", kind_name[k], kind_count[k]);
    (void)n;
    return bad || !cases;
}
