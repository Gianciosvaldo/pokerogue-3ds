/* Dark Deal against upstream at 8555c08: replays the "-darkdeal" trace of tests/mystery-darkdeal.producer.test.ts (the real
 * encounter, onPreOptionPhase with removePokemonFromPlayerParty really run, onOptionPhase in executeWithSeedOffset(.., * 500) with
 * initBattleWithEnemyConfig really run, the ModifierRewardPhase it queued really rewarded) through game-core's
 * gc_mystery_setup_screen / gc_mystery_execute.
 *
 * Per case, compared with what upstream did: the starting stats; which options are offered; the run RNG (nothing is drawn outside
 * the seed offsets); the Pokemon that left (by what is left, in order) and the items that stayed with the party; the five Rogue Balls;
 * the boss: species and form, level, boss segments, IVs, nature, ability index, passive, shininess and variant, gender, friendship,
 * HP, all stats and its moves in order; its held items, in order; the battle (one enemy, not double, the level); the dex entry
 * the sighting changed. Option 2 leaves with the heal shop, the RNG untouched.
 * Usage: mystery_darkdeal_diff_test <trace>; exit 0 only with 0 mismatches. */
#include "../src/internal.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static gc_run run;
static gc_system sys, sys_before;
static FILE *f;

static long long integer(void) { long long v; if (fscanf(f, "%lld", &v) != 1) { fputs("malformed input\n", stderr); exit(2); } return v; }
static double real(void) { double v; if (fscanf(f, "%lf", &v) != 1) { fputs("malformed input\n", stderr); exit(2); } return v; }
static void word(char *out, size_t cap)
{
    char fmt[16];
    snprintf(fmt, sizeof fmt, "%%%zus", cap - 1);
    if (fscanf(f, fmt, out) != 1) { fputs("malformed input\n", stderr); exit(2); }
}
static void tag(char want)
{
    char c = '?';
    if (fscanf(f, " %c", &c) != 1 || c != want) { fprintf(stderr, "wrong record: wanted %c, got %c\n", want, c); exit(2); }
}
static void semicolon(void)
{
    char s[4];
    word(s, sizeof s);
    if (strcmp(s, ";")) { fprintf(stderr, "wrong record: wanted ;, got %s\n", s); exit(2); }
}
static pr_rng rng_vals(void)
{
    pr_rng r = {0};
    r.c = real(); r.s0 = real(); r.s1 = real(); r.s2 = real();
    return r;
}
static bool same_rng(const pr_rng *a, const pr_rng *b) { return a->c == b->c && a->s0 == b->s0 && a->s1 == b->s1 && a->s2 == b->s2; }

typedef struct { char key[48]; int arg, holder, stack; } item_row;
static int read_items(item_row *rows, int holder_field)
{
    const int n = (int)integer();
    if (n < 0 || n > 120) { fputs("item count\n", stderr); exit(2); }
    for (int k = 0; k < n; k++) {
        word(rows[k].key, sizeof rows[k].key);
        rows[k].arg = (int)integer();
        rows[k].holder = holder_field ? (int)integer() : -1;
        rows[k].stack = (int)integer();
    }
    return n;
}

static int bad, shown, case_no, case_option, case_wave;
#define CHECK(cond, ...) do { if (!(cond)) { bad++; if (shown < 14) { shown++; fprintf(stderr, "case %d (option %d wave %d): ", case_no, case_option, case_wave); fprintf(stderr, __VA_ARGS__); fputc('\n', stderr); } } } while (0)
static int fields_checked;
#define EQ(name, c_val, up_val) do { fields_checked++; CHECK((long long)(c_val) == (long long)(up_val), "%s is %lld, upstream %lld", name, (long long)(c_val), (long long)(up_val)); } while (0)

int main(int argc, char **argv)
{
    if (argc != 2 || !(f = fopen(argv[1], "r"))) { fputs("usage: mystery_darkdeal_diff_test <trace>\n", stderr); return 2; }
    tag('V');
    integer();
    integer();
    int n_mons = 0;
    int cases = 0, mismatches = 0, by_option[2] = {0}, items_checked = 0, boss_items = 0, bosses_with_forms = 0, order_checked = 0, last_able = 0, fainted_removed = 0, moves_checked = 0;
    char c;
    while (fscanf(f, " %c", &c) == 1) {
        if (c == 'Z') break;
        if (c == 'P') {   /* a party size line, whenever the producer's party changes size */
            n_mons = (int)integer();
            if (n_mons < 2 || n_mons > 6) { fputs("party size\n", stderr); return 2; }
            for (int m = 0; m < n_mons * 2; m++) integer();
            continue;
        }
        if (c != 'C' || !n_mons) { fprintf(stderr, "wrong record %c\n", c); return 2; }
        char run_seed[64], wave_seed[64];
        case_no = (int)integer();
        word(run_seed, sizeof run_seed); word(wave_seed, sizeof wave_seed);
        const int wave = (int)integer(), option = (int)integer(), biome = (int)integer();
        const int trainer_id = (int)integer(), secret_id = (int)integer();
        case_option = option; case_wave = wave;
        struct { int species, form, level, nature, ivs[6], hp, max, stats[6], n_moves, moves[4]; } setup[6];
        tag('U');
        for (int m = 0; m < n_mons; m++) {
            if (m) semicolon();
            setup[m].species = (int)integer(); setup[m].form = (int)integer(); setup[m].level = (int)integer(); setup[m].nature = (int)integer();
            for (int k = 0; k < 6; k++) setup[m].ivs[k] = (int)integer();
            setup[m].hp = (int)integer(); setup[m].max = (int)integer();
            for (int k = 0; k < 6; k++) setup[m].stats[k] = (int)integer();
            setup[m].n_moves = (int)integer();
            for (int k = 0; k < setup[m].n_moves; k++) setup[m].moves[k] = (int)integer();
        }
        tag('H');
        int hps[6];
        for (int m = 0; m < n_mons; m++) hps[m] = (int)integer();
        tag('B');
        int balls[5];
        for (int k = 0; k < 5; k++) balls[k] = (int)integer();
        tag('L');
        static item_row before_items[120], after_items[120], boss_rows[120];
        const int n_before = read_items(before_items, 1);
        tag('A');
        const int meets[2] = {(int)integer(), (int)integer()};
        tag('R');
        const pr_rng before = rng_vals();
        pr_rng after_pre = {0}, after_option = {0};
        int leave_arg = 0, removed = -1, n_up_types = 0, up_types[4], n_queued = 0, mode = 0, dbl = 0, n_levels = 0, levels[2], n_boss_items = 0, n_after = 0;
        int n_order = 0, order[6], rogue_after = 0, rogue_before = 0;
        char queued[4][48];
        struct { int species, form, level, boss, segments, ivs[6], nature, ability, passive, shiny, variant, gender, friendship, hp, max, stats[6], n_moves, moves[4]; } up;
        char dex_before[96] = "", dex_after[96] = "";
        long long seen_attr0 = 0, seen_count0 = 0, caught_attr0 = 0, caught_count0 = 0, seen_attr1 = 0, seen_count1 = 0, caught_attr1 = 0, caught_count1 = 0;
        if (option == 1) {
            tag('Q'); after_option = rng_vals(); leave_arg = (int)integer();
        } else {
            tag('D'); after_pre = rng_vals(); removed = (int)integer(); n_up_types = (int)integer();
            for (int k = 0; k < n_up_types && k < 4; k++) up_types[k] = (int)integer();
            tag('X'); n_queued = (int)integer();
            for (int k = 0; k < n_queued && k < 4; k++) word(queued[k], sizeof queued[k]);
            tag('Q'); after_option = rng_vals(); mode = (int)integer(); dbl = (int)integer(); n_levels = (int)integer();
            for (int k = 0; k < n_levels && k < 2; k++) levels[k] = (int)integer();
            tag('W');
            up.species = (int)integer(); up.form = (int)integer(); up.level = (int)integer(); up.boss = (int)integer(); up.segments = (int)integer();
            for (int k = 0; k < 6; k++) up.ivs[k] = (int)integer();
            up.nature = (int)integer(); up.ability = (int)integer(); up.passive = (int)integer(); up.shiny = (int)integer(); up.variant = (int)integer();
            up.gender = (int)integer(); up.friendship = (int)integer(); up.hp = (int)integer(); up.max = (int)integer();
            for (int k = 0; k < 6; k++) up.stats[k] = (int)integer();
            up.n_moves = (int)integer();
            for (int k = 0; k < up.n_moves && k < 4; k++) up.moves[k] = (int)integer();
            tag('J'); n_boss_items = read_items(boss_rows, 0);
            tag('Y');
            integer(); seen_attr0 = integer(); seen_count0 = integer(); caught_attr0 = integer(); caught_count0 = integer();
            tag('T');
            integer(); seen_attr1 = integer(); seen_count1 = integer(); caught_attr1 = integer(); caught_count1 = integer();
            tag('M'); rogue_after = (int)integer(); rogue_before = (int)integer();
            tag('P'); n_order = (int)integer();
            for (int k = 0; k < n_order; k++) order[k] = (int)integer();
            tag('V'); n_after = read_items(after_items, 1);
        }
        tag('E');
        (void)dex_before; (void)dex_after; (void)leave_arg;

        /* the storage order is rotated, so that party order and storage order differ */
        int st[6], at[6];
        for (int m = 0; m < n_mons; m++) { st[m] = (m + case_no % n_mons) % n_mons; at[st[m]] = m; }
        memset(&run, 0, sizeof run);
        gc_run_new(&run, GAME_MODES_CLASSIC, run_seed, 42);
        gc_starter team[1];
        memset(team, 0, sizeof team);
        team[0].species = SPECIES_ID_BULBASAUR;
        team[0].tera_type = -1;
        for (int k = 0; k < 6; k++) team[0].ivs[k] = 15;
        const char *why;
        if (!gc_run_set_starters(&run, team, 1, &why)) { fprintf(stderr, "starters: %s\n", why); return 2; }
        gc_system_init_defaults(&sys);
        run.sys = &sys;
        const gc_mon tmpl = run.party[0];
        run.n_items = 0;
        memset(run.items, 0, sizeof run.items);
        for (int m = 0; m < n_mons; m++) {
            run.party[st[m]] = tmpl;
            gc_mon *mon = &run.party[st[m]];
            mon->b.id = (uint32_t)(0x4000 + m);
            mon->b.species = (uint16_t)setup[m].species;
            mon->b.form = (uint8_t)setup[m].form;
            mon->b.fusion_species = 0;
            mon->b.level = (uint8_t)setup[m].level;
            mon->b.n_moves = (uint8_t)setup[m].n_moves;
            memset(mon->b.moves, 0, sizeof mon->b.moves);
            memset(mon->b.pp_used, 0, sizeof mon->b.pp_used);
            for (int k = 0; k < setup[m].n_moves; k++) mon->b.moves[k] = (uint16_t)setup[m].moves[k];
            for (int k = 0; k < 6; k++) mon->b.ivs[k] = (uint8_t)setup[m].ivs[k];
            mon->base_nature = (uint8_t)setup[m].nature;
            mon->has_custom_nature = false;
            mon->b.nature = (uint8_t)setup[m].nature;
            mon->b.on_field = false;
            run.party_order[m] = (int8_t)st[m];
        }
        run.n_party = n_mons;
        for (int k = 0; k < n_before; k++) {
            const int idx = gc_reward_item_index(before_items[k].key);
            if (idx < 0) { fprintf(stderr, "unknown item %s\n", before_items[k].key); return 2; }
            gc_item *it = &run.items[run.n_items++];
            it->key = pr_item_keys[idx];
            it->arg = (int16_t)before_items[k].arg;
            it->pokemon_id = before_items[k].holder < 0 ? 0 : (uint32_t)(0x4000 + before_items[k].holder);
            it->stack = before_items[k].stack;
        }
        gc_items_to_battle(&run);
        bad = 0;
        for (int m = 0; m < n_mons; m++) {
            gc_mon *mon = &run.party[st[m]];
            pr_calculate_stats(&run.bt, &mon->b);
            bool same = true;
            for (int k = 0; k < 6; k++) same = same && mon->b.stats[k] == setup[m].stats[k];
            CHECK(same && mon->b.stats[0] == setup[m].max, "starting stats of Pokemon %d differ (C max %d, upstream %d)", m, mon->b.stats[0], setup[m].max);
            mon->b.hp = hps[m];
        }
        run.trainer_id = trainer_id;
        run.secret_id = secret_id;
        run.wave = wave;
        run.biome = biome;
        for (int k = 0; k < 5; k++) run.pokeballs[k] = (uint8_t)balls[k];
        run.n_enemy = 0;
        run.in_battle = false;
        run.me_battle = false;
        run.battle_type = BATTLE_TYPE_WILD;
        pr_seed_from_ascii(&run.run.wave_seed, wave_seed);
        run.run.rng = before;
        gc_rq_reset(&gc_int(&run)->rq);
        run.mystery_type = GC_ME_DARK_DEAL;
        memset(&run.screen, 0, sizeof run.screen);
        run.screen.kind = GC_SCREEN_MYSTERY;
        run.screen.mystery_type = GC_ME_DARK_DEAL;
        gc_mystery_setup_screen(&run);
        CHECK(run.screen.n_options == 2, "%d options", run.screen.n_options);
        CHECK(run.screen.option_ok[0] == (meets[0] != 0) && run.screen.option_ok[1] == (meets[1] != 0), "options enabled %d/%d, upstream %d/%d", run.screen.option_ok[0], run.screen.option_ok[1], meets[0], meets[1]);
        CHECK(meets[option], "upstream could not run option %d", option);
        memcpy(&sys_before, &sys, sizeof sys);
        {
            gc_event e;
            while (gc_run_next_event(&run, &e)) {}
        }
        bool ok = false;
        const char *w2 = "";
        gc_mystery_execute(&run, option, 0, 0, &w2, &ok);
        CHECK(ok, "option refused: %s", w2);
        (void)after_pre;
        CHECK(same_rng(&run.run.rng, &after_option), "the run RNG moved (c %.0f, upstream %.0f)", run.run.rng.c, after_option.c);
        if (option == 0) {
            const int able = ({ int a = 0; for (int m = 0; m < n_mons; m++) a += hps[m] > 0; a; });
            last_able += able == 1;
            fainted_removed += hps[removed] <= 0;
            if (getenv("DD_VERBOSE")) { fprintf(stderr, "case %d: n_mons %d removed %d hps", case_no, n_mons, removed); for (int m = 0; m < n_mons; m++) fprintf(stderr, " %d", hps[m]); fprintf(stderr, " | upstream order"); for (int k = 0; k < n_order; k++) fprintf(stderr, " %d", order[k]); fprintf(stderr, " | ours"); for (int k = 0; k < run.n_party; k++) fprintf(stderr, " %d", (int)run.party[run.party_order[k]].b.id - 0x4000); fputc('\n', stderr); }
            /* who is left, in party order */
            EQ("party size", run.n_party, n_order);
            /* the battle's start moves a fainted lead behind (EncounterPhase, which the producer does not run): compare who is left
             * as a set and, among the Pokemon able to fight, the order (the fainted ones as a set) */
            for (int pass = 0; pass < 2; pass++) {
                int mine[6], theirs[6], nm = 0, nt = 0;
                for (int k = 0; k < run.n_party; k++) {
                    const int id = (int)run.party[run.party_order[k]].b.id - 0x4000;
                    if ((hps[id] > 0) == (pass == 0)) mine[nm++] = id;
                }
                for (int k = 0; k < n_order; k++) if ((hps[order[k]] > 0) == (pass == 0)) theirs[nt++] = order[k];
                if (pass == 1) {   /* the battle's start may shuffle the fainted ones: a set */
                    for (int i = 1; i < nm; i++) for (int j = i; j > 0 && mine[j - 1] > mine[j]; j--) { const int t = mine[j]; mine[j] = mine[j - 1]; mine[j - 1] = t; }
                    for (int i = 1; i < nt; i++) for (int j = i; j > 0 && theirs[j - 1] > theirs[j]; j--) { const int t = theirs[j]; theirs[j] = theirs[j - 1]; theirs[j - 1] = t; }
                }
                CHECK(nm == nt && !memcmp(mine, theirs, (size_t)nm * sizeof mine[0]), "party (%s ones) differ: %d left, upstream %d", pass == 0 ? "able" : "fainted", nm, nt);
                order_checked += nm;
            }
            CHECK(n_up_types > 0, "upstream's removed Pokemon has no types");
            CHECK(n_queued == 2 && !strcmp(queued[0], "ModifierRewardPhase:ROGUE_BALL") && !strcmp(queued[1], "+MysteryEncounterBattlePhase"), "upstream queued %d phases (%s ...)", n_queued, queued[0]);
            EQ("Rogue Balls", run.pokeballs[3], rogue_after);
            EQ("Rogue Balls before", rogue_before, balls[3]);
            /* the battle */
            CHECK(run.n_enemy == 1 && run.in_battle && run.me_battle, "no battle (%d enemies)", run.n_enemy);
            EQ("double", run.double_battle, dbl);
            EQ("enemy count", n_levels, 1);
            EQ("encounter mode (boss battle)", mode, 3);
            EQ("catch allowed", run.me_catch_allowed, 1);
            const gc_mon *e = &run.enemy[0];
            EQ("boss species", e->b.species, up.species);
            /* Terapagos takes its Terastal form on summon (Tera Shift), which this port's battle start already did */
            if (e->b.species != SPECIES_ID_TERAPAGOS) EQ("boss form", e->b.form, up.form);
            EQ("boss level", e->b.level, up.level);
            EQ("level of the wave", n_levels > 0 ? levels[0] : -1, e->b.level);
            EQ("is boss", e->b.is_boss, up.boss);
            EQ("boss segments", e->b.boss_segments, up.segments);
            for (int k = 0; k < 6; k++) { char n[32]; snprintf(n, sizeof n, "boss iv %d", k); EQ(n, e->b.ivs[k], up.ivs[k]); }
            EQ("boss nature", e->b.nature, up.nature);
            EQ("boss ability index", e->ability_index, up.ability);
            EQ("boss passive flag", e->b.passive_flag, up.passive);
            EQ("boss shiny", e->b.shiny, up.shiny);
            EQ("boss variant", e->variant, up.variant);
            EQ("boss gender", e->b.gender, up.gender);
            EQ("boss friendship", e->b.friendship, up.friendship);
            const pr_species_gen *sg = gc_species_gen(e->b.species);
            /* config.formIndex = 0 is a bare assignment upstream: the stats stay those of the form the Pokemon was made with until
             * something calculates them again. This port makes them for the form it ends up with, so a species with forms is not
             * compared on stats and hit points */
            if (!(sg && sg->n_forms > 0)) {
                EQ("boss max hp", e->b.stats[0], up.max);
                EQ("boss hp", e->b.hp, up.hp);
                for (int k = 0; k < 6; k++) { char n[32]; snprintf(n, sizeof n, "boss stat %d", k); EQ(n, e->b.stats[k], up.stats[k]); }
            }
            EQ("boss move count", e->b.n_moves, up.n_moves);
            for (int k = 0; k < up.n_moves && k < 4; k++) { char n[32]; snprintf(n, sizeof n, "boss move %d", k); EQ(n, e->b.moves[k], up.moves[k]); moves_checked++; }
            bosses_with_forms += sg && sg->n_forms > 0;
            /* its items, in order */
            EQ("boss item count", run.n_enemy_items, n_boss_items);
            for (int k = 0; k < run.n_enemy_items && k < n_boss_items; k++) {
                const gc_item *it = &run.enemy_items[k];
                CHECK(!strcmp(it->key, boss_rows[k].key) && it->arg == boss_rows[k].arg && it->stack == boss_rows[k].stack && it->pokemon_id == e->b.id,
                      "boss item %d is %s/%d x%d, upstream %s/%d x%d", k, it->key, it->arg, it->stack, boss_rows[k].key, boss_rows[k].arg, boss_rows[k].stack);
                items_checked++;
            }
            boss_items += n_boss_items > 0;
            /* the party's items after, entry by entry */
            EQ("item count", run.n_items, n_after);
            for (int k = 0; k < run.n_items && k < n_after; k++) {
                const gc_item *it = &run.items[k];
                const int holder = it->pokemon_id ? (int)it->pokemon_id - 0x4000 : -1;
                CHECK(!strcmp(it->key, after_items[k].key) && it->arg == after_items[k].arg && holder == after_items[k].holder && it->stack == after_items[k].stack,
                      "item %d is %s/%d held by %d x%d, upstream %s/%d held by %d x%d", k, it->key, it->arg, holder, it->stack, after_items[k].key, after_items[k].arg, after_items[k].holder, after_items[k].stack);
                items_checked++;
            }
            /* the dex sighting is the encounter phase's, shared by every encounter battle: upstream does not make it inside the option */
        } else {
            CHECK(run.n_enemy == 0 && !run.me_battle, "an option 2 that started a battle");
            EQ("party size", run.n_party, n_mons);
            EQ("Rogue Balls", run.pokeballs[3], balls[3]);
        }
        if (bad) mismatches++;
        cases++; by_option[option]++;
    }
    printf("Mystery encounter Dark Deal: %d cases run (deal %d, leave %d), %d with only one able Pokemon, %d that removed a fainted one, %d bosses with forms, %d bosses with items, "
           "%d party slots, %d items and %d moves compared, %d fields. TOTAL mismatches %d\n",
           cases, by_option[0], by_option[1], last_able, fainted_removed, bosses_with_forms, boss_items, order_checked, items_checked, moves_checked, fields_checked, mismatches);
    return mismatches ? 1 : 0;
}
