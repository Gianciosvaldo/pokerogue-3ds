/* Training Session against upstream at 8555c08: replays the "-training" trace of tests/mystery-training.producer.test.ts (the real encounter:
 * the party picker answered by a plan, then onOptionPhase in (.., * 500) with initBattleWithEnemyConfig really run, and the callback the rewards
 * carry run by hand in the rewards phase's seed offset) through game-core.
 *
 * Per case, compared with what upstream did: the starting stats; what the picker offers (whether the Pokemon can be picked, the 25 natures
 * and the abilities with their "(+)" marks); the run RNG after the option; the clone that enters the battle (a new id, level, IVs, nature,
 * ability, moves with their PP used, segments, HP, stats, items with their stacks), the party and the item list without the Pokemon, the
 * rewards set; then the callback's effects: the IVs, nature or ability, the Pokemon back at the end of the party, the item list, the dex.
 * Usage: mystery_training_diff_test <trace>; exit 0 only with 0 mismatches. */
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

typedef struct { int species, form, level, segments, ivs[6], nature, ability, passive, shiny, variant, gender, friendship; long long id; int luck, hp, stats[6], n_moves, moves[4], tera; } boss_row;
static void read_boss(boss_row *b, bool is_x)
{
    b->species = (int)integer(); b->form = (int)integer(); b->level = (int)integer();
    if (is_x) integer();   /* isBoss */
    b->segments = (int)integer();
    for (int k = 0; k < 6; k++) b->ivs[k] = (int)integer();
    b->nature = (int)integer(); b->ability = (int)integer(); b->passive = (int)integer(); b->shiny = (int)integer(); b->variant = (int)integer();
    b->gender = (int)integer(); b->friendship = (int)integer(); b->id = integer();
    b->luck = is_x ? 0 : (int)integer();
    b->hp = (int)integer();
    if (is_x) integer();   /* max HP: stats[0] */
    for (int k = 0; k < 6; k++) b->stats[k] = (int)integer();
    b->n_moves = (int)integer();
    for (int k = 0; k < b->n_moves && k < 4; k++) b->moves[k] = (int)integer();
    b->tera = is_x ? 0 : (int)integer();
}

typedef struct { int species, form, level, nature, ivs[6], hp, max, stats[6], n_moves, moves[4], luck, ability, ability_index, gender, friendship, shiny, variant, tera, passive, pp[4]; } mon_row;
typedef struct { int n, species[8], ivs[8][6]; long long nature_attr[8], caught[8], seen[8]; int root; long long ability_attr, passive_attr; } dex_row;

static void read_dex(dex_row *d)
{
    d->n = (int)integer();
    if (d->n < 1 || d->n > 8) { fputs("dex chain\n", stderr); exit(2); }
    for (int k = 0; k < d->n; k++) {
        d->species[k] = (int)integer();
        for (int j = 0; j < 6; j++) d->ivs[k][j] = (int)integer();
        d->nature_attr[k] = integer(); d->caught[k] = integer(); d->seen[k] = integer();
    }
    d->root = (int)integer(); d->ability_attr = integer(); d->passive_attr = integer();
}

int main(int argc, char **argv)
{
    if (argc != 2 || !(f = fopen(argv[1], "r"))) { fputs("usage: mystery_training_diff_test <trace>\n", stderr); return 2; }
    tag('V');
    integer();
    integer();
    int n_mons = 0;
    int cases = 0, mismatches = 0, by_option[4] = {0}, skipped = 0, unpickable = 0, items_checked = 0, moves_checked = 0, natures_checked = 0, abilities_checked = 0;
    int lead_swapped = 0, improved_checked = 0, dex_checked = 0, all_fainted = 0, tags_heavy = 0;
    char c;
    while (fscanf(f, " %c", &c) == 1) {
        if (c == 'Z') break;
        if (c == 'P') {
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
        mon_row setup[6];
        tag('U');
        for (int m = 0; m < n_mons; m++) {
            if (m) semicolon();
            setup[m].species = (int)integer(); setup[m].form = (int)integer(); setup[m].level = (int)integer(); setup[m].nature = (int)integer();
            for (int k = 0; k < 6; k++) setup[m].ivs[k] = (int)integer();
            setup[m].hp = (int)integer(); setup[m].max = (int)integer();
            for (int k = 0; k < 6; k++) setup[m].stats[k] = (int)integer();
            setup[m].n_moves = (int)integer();
            for (int k = 0; k < setup[m].n_moves; k++) setup[m].moves[k] = (int)integer();
            setup[m].luck = (int)integer();
            setup[m].ability = (int)integer();
            setup[m].ability_index = (int)integer(); setup[m].gender = (int)integer(); setup[m].friendship = (int)integer();
            setup[m].shiny = (int)integer(); setup[m].variant = (int)integer(); setup[m].tera = (int)integer(); setup[m].passive = (int)integer();
            for (int k = 0; k < setup[m].n_moves; k++) setup[m].pp[k] = (int)integer();
        }
        tag('H');
        int hps[6];
        for (int m = 0; m < n_mons; m++) hps[m] = (int)integer();
        tag('B');
        int balls[5];
        for (int k = 0; k < 5; k++) balls[k] = (int)integer();
        tag('L');
        static item_row before_items[120], boss_rows[120], after_items[120], after_items2[120];
        const int n_before = read_items(before_items, 1);
        tag('A');
        int meets[5];
        for (int k = 0; k < 5; k++) meets[k] = (int)integer();
        tag('D');
        dex_row dex_before;
        read_dex(&dex_before);
        tag('O');
        const int up_member = (int)integer(), up_ok = (int)integer(), up_living = (int)integer(), up_n = (int)integer(), up_chosen = (int)integer();
        int up_slot[25], up_new[25];
        for (int k = 0; k < up_n && k < 25; k++) { up_slot[k] = (int)integer(); up_new[k] = (int)integer(); }
        pr_rng rng_before = {0}, after_option = {0};
        int mode = 0, dbl = 0, n_levels = 0, levels[2], n_queued = 0, n_rewards = 0, fill = 0, has_cb = 0, leave_calls = 0, leave_heal = -1;
        char queued[4][48], tags_text[96] = "";
        boss_row up;
        memset(&up, 0, sizeof up);
        int up_pp[4] = {0}, up_tera = 0, up_luck = 0, up_get_nature = 0, st_effect = 0, n_boss_items = 0;
        int n_party_after = 0, party_after[6], n_player_items = 0;
        int n_msgs = 0, stat1 = -1, stat2 = -1, n_items_back = 0, skipped_case = 0;
        char msg_key[2][96];
        pr_rng r0 = {0}, r1 = {0};
        mon_row after_mon;
        int a_ivs[6], a_nature = 0, a_get_nature = 0, a_ability_index = 0, a_ability_id = 0, a_hp = 0, a_max = 0, a_stats[6], a_custom = -1;
        int n_party_back = 0, party_back[6];
        dex_row dex_after;
        memset(&after_mon, 0, sizeof after_mon);
        memset(&dex_after, 0, sizeof dex_after);
        {
            char t = '?';
            if (fscanf(f, " %c", &t) != 1) return 2;
            if (t == 'E') { skipped_case = 1; skipped++; unpickable += !up_ok; all_fainted += up_ok && !up_living; goto compare_picker_only; }
            ungetc(t, f);
        }
        tag('R'); rng_before = rng_vals();
        tag('Q'); after_option = rng_vals();
        if (option == 3) {
            tag('S'); n_rewards = (int)integer(); fill = (int)integer(); has_cb = (int)integer(); leave_calls = (int)integer(); leave_heal = (int)integer();
        } else {
            mode = (int)integer(); dbl = (int)integer(); n_levels = (int)integer();
            for (int k = 0; k < n_levels && k < 2; k++) levels[k] = (int)integer();
            n_queued = (int)integer();
            for (int k = 0; k < n_queued && k < 4; k++) word(queued[k], sizeof queued[k]);
            tag('S'); n_rewards = (int)integer(); fill = (int)integer(); has_cb = (int)integer(); leave_calls = (int)integer();
            tag('X');
            up.species = (int)integer(); up.form = (int)integer(); up.level = (int)integer(); integer(); up.segments = (int)integer();
            for (int k = 0; k < 6; k++) up.ivs[k] = (int)integer();
            up.nature = (int)integer(); up.ability = (int)integer(); up.passive = (int)integer(); up.shiny = (int)integer(); up.variant = (int)integer();
            up.gender = (int)integer(); up.friendship = (int)integer(); up.id = integer(); up.hp = (int)integer(); integer();
            for (int k = 0; k < 6; k++) up.stats[k] = (int)integer();
            up.n_moves = (int)integer();
            for (int k = 0; k < up.n_moves && k < 4; k++) { up.moves[k] = (int)integer(); up_pp[k] = (int)integer(); }
            up_tera = (int)integer(); up_luck = (int)integer(); up_get_nature = (int)integer();
            tag('N'); st_effect = (int)integer(); integer();
            tag('T'); word(tags_text, sizeof tags_text);
            tag('J'); n_boss_items = read_items(boss_rows, 0);
            tag('W'); n_party_after = (int)integer();
            for (int k = 0; k < n_party_after && k < 6; k++) party_after[k] = (int)integer();
            tag('V'); n_player_items = read_items(after_items, 1);
            tag('Y'); r0 = rng_vals(); r1 = rng_vals(); n_msgs = (int)integer();
            for (int k = 0; k < n_msgs && k < 2; k++) word(msg_key[k], sizeof msg_key[k]);
            stat1 = (int)integer(); stat2 = (int)integer();
            tag('a');
            for (int k = 0; k < 6; k++) a_ivs[k] = (int)integer();
            a_nature = (int)integer(); a_get_nature = (int)integer(); a_ability_index = (int)integer(); a_ability_id = (int)integer();
            a_hp = (int)integer(); a_max = (int)integer();
            for (int k = 0; k < 6; k++) a_stats[k] = (int)integer();
            a_custom = (int)integer();
            tag('b'); n_party_back = (int)integer();
            for (int k = 0; k < n_party_back && k < 6; k++) party_back[k] = (int)integer();
            tag('c'); n_items_back = read_items(after_items2, 1);
            tag('d'); read_dex(&dex_after);
        }
        tag('E');
compare_picker_only:;
        (void)mode; (void)levels; (void)n_queued; (void)queued; (void)r0; (void)r1; (void)rng_before; (void)n_player_items; (void)n_levels;

        /* the storage order is rotated, so that party order and storage order differ */
        int st[6];
        for (int m = 0; m < n_mons; m++) st[m] = (m + case_no % n_mons) % n_mons;
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
            mon->luck = (int8_t)setup[m].luck;
            mon->b.ability = mon->b.base_ability = (uint16_t)setup[m].ability;   /* the pool's weights read the party's abilities */
            mon->b.on_field = false;
            mon->ability_index = (uint8_t)setup[m].ability_index;
            mon->b.ability_index = (int8_t)setup[m].ability_index;
            mon->b.gender = (int8_t)setup[m].gender;
            mon->b.friendship = (uint8_t)setup[m].friendship;
            mon->b.shiny = setup[m].shiny;
            mon->variant = mon->b.variant = (uint8_t)setup[m].variant;
            mon->b.tera_type = (int8_t)setup[m].tera;
            mon->b.passive_flag = setup[m].passive;
            for (int k = 0; k < setup[m].n_moves; k++) mon->b.pp_used[k] = (uint8_t)setup[m].pp[k];
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
            CHECK(same && mon->b.stats[0] == setup[m].max, "starting stats of Pokemon %d (species %d lv %d nature %d) differ: C %d %d %d %d %d %d, upstream %d %d %d %d %d %d", m, setup[m].species, setup[m].level, setup[m].nature, mon->b.stats[0], mon->b.stats[1], mon->b.stats[2], mon->b.stats[3], mon->b.stats[4], mon->b.stats[5], setup[m].stats[0], setup[m].stats[1], setup[m].stats[2], setup[m].stats[3], setup[m].stats[4], setup[m].stats[5]);
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
        run.run.rng = rng_before;
        gc_rq_reset(&gc_int(&run)->rq);
        run.mystery_type = GC_ME_TRAINING_SESSION;
        memset(&run.screen, 0, sizeof run.screen);
        run.screen.kind = GC_SCREEN_MYSTERY;
        run.screen.mystery_type = GC_ME_TRAINING_SESSION;
        gc_mystery_setup_screen(&run);
        /* the dex the encounter ran against (the species chain of the picked Pokemon and its starter root) */
        for (int k = 0; k < dex_before.n; k++) {
            gc_dex_entry *d = &sys.dex[dex_before.species[k]];
            for (int j = 0; j < 6; j++) d->ivs[j] = (uint8_t)dex_before.ivs[k][j];
            d->nature_attr = (uint32_t)dex_before.nature_attr[k];
            d->caught_attr = (uint64_t)dex_before.caught[k];
            d->seen_attr = (uint64_t)dex_before.seen[k];
        }
        sys.starters[dex_before.root].ability_attr = (uint32_t)dex_before.ability_attr;
        sys.starters[dex_before.root].passive_attr = (uint32_t)dex_before.passive_attr;

        CHECK(run.screen.n_options == 4, "%d options", run.screen.n_options);
        for (int o = 0; o < 4; o++) CHECK(run.screen.option_ok[o] == (meets[o] != 0), "option %d enabled %d, upstream %d", o, run.screen.option_ok[o], meets[o]);
        int choice = 0;
        if (option < 3) {
            const int member = st[up_member];
            EQ("Pokemon can be picked", gc_training_pick_ok(&run, member), up_ok);
            if (option == 1 && up_ok) {
                EQ("nature list length", up_n, 25);
                for (int k = 0; k < up_n && k < 25; k++) {
                    EQ("nature offered is in range", up_slot[k] >= 0 && up_slot[k] <= 24, 1);
                    EQ("nature (+) mark", gc_training_nature_new(&run, member, up_slot[k]), up_new[k]);
                    natures_checked++;
                }
                if (up_chosen >= 0 && up_chosen < up_n) choice = up_slot[up_chosen];
            } else if (option == 2 && up_ok) {
                int slots[3];
                bool fresh[3];
                const int n = gc_training_abilities(&run, member, slots, fresh);
                EQ("ability list length", n, up_n);
                for (int k = 0; k < n && k < up_n; k++) {
                    EQ("ability slot", slots[k], up_slot[k]);
                    EQ("ability (+) mark", fresh[k], up_new[k]);
                    abilities_checked++;
                }
                if (up_chosen >= 0 && up_chosen < up_n) choice = up_slot[up_chosen];
            }
        }
        if (skipped_case) {
            if (bad) mismatches++;
            cases++;
            continue;
        }

        memcpy(&sys_before, &sys, sizeof sys);
        { gc_event e; while (gc_run_next_event(&run, &e)) {} }
        int level_c = -1;
        if (option < 3) { pr_rng orng = gc_me_option_rng(&run); level_c = (int)pr_run_get_level_for_wave(&run.run, &orng, run.wave); }
        bool ok = false;
        const char *w2 = "";
        gc_mystery_execute(&run, option, option < 3 ? st[up_member] : 0, choice, &w2, &ok);
        CHECK(ok, "option refused: %s", w2);
        CHECK(same_rng(&run.run.rng, &after_option), "the run RNG after the option differs (c %.0f, upstream %.0f)", run.run.rng.c, after_option.c);
        if (option == 3) {
            EQ("leave calls", leave_calls, 1);
            EQ("leave heals", leave_heal, 1);
            EQ("rewards set", n_rewards, 0);
            CHECK(run.n_enemy == 0 && !run.me_battle, "a battle for the leave option");
            { int heal = 0;
              for (gc_rphase p; gc_rq_next(&gc_int(&run)->rq, &p);) if (p.type == RP_SELECT_MODIFIER) heal += p.a == 1;
              EQ("the heal shop opens", heal, 1); }
            if (bad) mismatches++;
            cases++; by_option[option]++;
            continue;
        }
        EQ("leave calls", leave_calls, 0);
        CHECK(!gc_int(&run)->training.away || run.n_enemy == 1, "no clone");
        if (run.n_enemy != 1) { bad++; if (bad) mismatches++; cases++; by_option[option]++; continue; }
        CHECK(run.in_battle && run.me_battle, "no battle");
        EQ("double", run.double_battle, dbl);
        EQ("one enemy level", n_levels, 1);
        EQ("getLevelForWave on the option stream", level_c, n_levels > 0 ? levels[0] : -1);
        EQ("fleeing is off", gc_int(&run)->me_no_flee, 1);
        EQ("rewards set", gc_int(&run)->me_reward.active, n_rewards == 1);
        EQ("rewards fill the rest", gc_int(&run)->me_reward.fill, fill);
        EQ("a callback before the rewards", gc_int(&run)->me_reward.before, has_cb);
        EQ("reward is not an item", gc_int(&run)->me_reward.has_item, 0);
        EQ("enemy not counted as seen (no stats)", gc_int(&run)->me_no_stats, 1);
        if (option == 2) { CHECK(!strcmp(tags_text, "-") || strstr(tags_text, "MYSTERY_ENCOUNTER_POST_SUMMON"), "the boss's tags (upstream %s)", tags_text); tags_heavy += strcmp(tags_text, "-") != 0; }
        else CHECK(!strcmp(tags_text, "-"), "the boss's tags (upstream %s)", tags_text);
        {
            const gc_mon *e = &run.enemy[0];
            EQ("boss species", e->b.species, up.species);
            EQ("boss form", e->b.form, up.form);
            EQ("boss level", e->b.level, up.level);
            EQ("boss segments", e->b.boss_segments, up.segments);
            for (int k = 0; k < 6; k++) { char n[32]; snprintf(n, sizeof n, "boss iv %d", k); EQ(n, e->b.ivs[k], up.ivs[k]); }
            EQ("boss nature", e->has_custom_nature ? e->base_nature : e->b.nature, up.nature);
            EQ("boss effective nature", e->b.nature, up_get_nature);
            EQ("boss ability index", e->ability_index, up.ability);
            EQ("boss passive flag", e->b.passive_flag, up.passive);
            EQ("boss shiny", e->b.shiny, up.shiny);
            EQ("boss variant", e->variant, up.variant);
            EQ("boss gender", e->b.gender, up.gender);
            EQ("boss friendship", e->b.friendship, up.friendship);
            EQ("boss id (new)", e->b.id, up.id);
            EQ("boss luck", e->luck, up_luck);
            EQ("boss tera", e->b.tera_type, up_tera);
            EQ("boss hp", e->b.hp, up.hp);
            for (int k = 0; k < 6; k++) { char n[32]; snprintf(n, sizeof n, "boss stat %d", k); EQ(n, e->b.stats[k], up.stats[k]); }
            EQ("boss move count", e->b.n_moves, up.n_moves);
            for (int k = 0; k < up.n_moves && k < 4; k++) {
                char n[32]; snprintf(n, sizeof n, "boss move %d", k); EQ(n, e->b.moves[k], up.moves[k]);
                snprintf(n, sizeof n, "boss pp used %d", k); EQ(n, e->b.pp_used[k], up_pp[k]);
                moves_checked++;
            }
            EQ("boss status", e->b.status, st_effect);
            EQ("boss item count", run.n_enemy_items, n_boss_items);
            for (int k = 0; k < run.n_enemy_items && k < n_boss_items; k++) {
                const gc_item *it = &run.enemy_items[k];
                CHECK(!strcmp(it->key, boss_rows[k].key) && it->arg == boss_rows[k].arg && it->stack == boss_rows[k].stack && it->pokemon_id == e->b.id,
                      "boss item %d is %s/%d x%d, upstream %s/%d x%d", k, it->key, it->arg, it->stack, boss_rows[k].key, boss_rows[k].arg, boss_rows[k].stack);
                items_checked++;
            }
        }
        EQ("party size without the Pokemon", run.n_party, n_party_after);
        /* a fainted lead is swapped for the first Pokemon that can fight when the battle starts (gc_battle_setup_field); upstream's phase for that is not part of the option */
        const bool lead_ok = n_party_after > 0 && hps[party_after[0]] > 0;
        { int have = 0, want = 0;
          for (int k = 0; k < run.n_party && k < n_party_after; k++) {
              have += 1 << ((int)run.party[run.party_order[k]].b.id - 0x4000);
              want += 1 << party_after[k];
              if (lead_ok) EQ("party order", (int)run.party[run.party_order[k]].b.id - 0x4000, party_after[k]);
          }
          EQ("party members", have, want);
          lead_swapped += !lead_ok; }
        EQ("player item count", run.n_items, n_player_items);
        for (int k = 0; k < run.n_items && k < n_player_items; k++) {
            const gc_item *it = &run.items[k];
            const int holder = it->pokemon_id ? (int)it->pokemon_id - 0x4000 : -1;
            CHECK(!strcmp(it->key, after_items[k].key) && it->arg == after_items[k].arg && holder == after_items[k].holder && it->stack == after_items[k].stack,
                  "player item %d is %s/%d holder %d x%d, upstream %s/%d holder %d x%d", k, it->key, it->arg, holder, it->stack, after_items[k].key, after_items[k].arg, after_items[k].holder, after_items[k].stack);
            items_checked++;
        }

        /* the callback: the rewards phase runs it in the offset of wave * 1000, on a stream of its own */
        { gc_event e; while (gc_run_next_event(&run, &e)) {} }
        const pr_rng g0 = run.run.rng;
        gc_training_return(&run);
        CHECK(same_rng(&run.run.rng, &g0) && same_rng(&r0, &r1), "the callback moved the global RNG");
        int msgs = 0, stat_a = -1, stat_b = -1;
        static const char *const stat_keys[6] = {"pokemonInfo:stat.hp", "pokemonInfo:stat.atk", "pokemonInfo:stat.def", "pokemonInfo:stat.spatk", "pokemonInfo:stat.spdef", "pokemonInfo:stat.spd"};
        char first_key[96] = "";
        { gc_event e;
          while (gc_run_next_event(&run, &e)) {
              if (e.kind != GC_EV_MESSAGE) continue;
              if (!msgs) snprintf(first_key, sizeof first_key, "%s", e.msg.key ? e.msg.key : "");
              msgs++;
              for (int a = 0; a < e.msg.n_args; a++) {
                  const char *nm = e.msg.args[a].name;
                  if (!nm || !e.msg.args[a].s) continue;
                  for (int s = 0; s < 6; s++) {
                      if (strcmp(e.msg.args[a].s, stat_keys[s])) continue;
                      if (!strcmp(nm, "stat1")) stat_a = s;
                      if (!strcmp(nm, "stat2")) stat_b = s;
                  }
              }
          } }
        EQ("messages from the callback", msgs, n_msgs);
        if (n_msgs > 0) {
            static const char *const want_keys[3] = {"mysteryEncounters/trainingSession:option.1.finished", "mysteryEncounters/trainingSession:option.2.finished", "mysteryEncounters/trainingSession:option.3.finished"};
            CHECK(!strcmp(msg_key[0], want_keys[option]) && !strcmp(first_key, msg_key[0]), "message %s, upstream %s", first_key, msg_key[0]);
        }
        if (option == 0) { EQ("first improved stat", stat_a, stat1); EQ("second improved stat", stat_b, stat2); improved_checked += stat1 >= 0; }
        /* the Pokemon back at the end of the party, with the changes */
        EQ("party size after", run.n_party, n_party_back);
        for (int k = 0; k < run.n_party && k < n_party_back; k++) { if (lead_ok || k == run.n_party - 1) EQ("party order after", (int)run.party[run.party_order[k]].b.id - 0x4000, party_back[k]); }
        if (run.n_party >= 1) {
            const gc_mon *p = &run.party[run.party_order[run.n_party - 1]];
            for (int k = 0; k < 6; k++) { char n[32]; snprintf(n, sizeof n, "iv %d after", k); EQ(n, p->b.ivs[k], a_ivs[k]); }
            EQ("nature after (stored)", p->has_custom_nature ? p->base_nature : p->b.nature, a_nature);
            EQ("nature after (effective)", p->b.nature, a_get_nature);
            EQ("custom nature", p->has_custom_nature ? p->b.nature : -1, a_custom);
            EQ("ability index after", p->ability_index, a_ability_index);
            EQ("ability after", p->b.ability, a_ability_id);
            EQ("hp after", p->b.hp, a_hp);
            EQ("max hp after", p->b.stats[0], a_max);
            for (int k = 0; k < 6; k++) { char n[32]; snprintf(n, sizeof n, "stat %d after", k); EQ(n, p->b.stats[k], a_stats[k]); }
        }
        EQ("item count after", run.n_items, n_items_back);
        for (int k = 0; k < run.n_items && k < n_items_back; k++) {
            const gc_item *it = &run.items[k];
            const int holder = it->pokemon_id ? (int)it->pokemon_id - 0x4000 : -1;
            CHECK(!strcmp(it->key, after_items2[k].key) && it->arg == after_items2[k].arg && holder == after_items2[k].holder && it->stack == after_items2[k].stack,
                  "returned item %d is %s/%d holder %d x%d, upstream %s/%d holder %d x%d", k, it->key, it->arg, holder, it->stack, after_items2[k].key, after_items2[k].arg, after_items2[k].holder, after_items2[k].stack);
            items_checked++;
        }
        /* the dex after */
        for (int k = 0; k < dex_after.n; k++) {
            const gc_dex_entry *d = &sys.dex[dex_after.species[k]];
            for (int j = 0; j < 6; j++) { char n[40]; snprintf(n, sizeof n, "dex %d iv %d", dex_after.species[k], j); EQ(n, d->ivs[j], dex_after.ivs[k][j]); }
            EQ("dex nature attr", d->nature_attr, dex_after.nature_attr[k]);
            EQ("dex caught attr", (long long)d->caught_attr, dex_after.caught[k]);
            EQ("dex seen attr", (long long)d->seen_attr, dex_after.seen[k]);
            dex_checked++;
        }
        EQ("starter ability attr", sys.starters[dex_after.root].ability_attr, dex_after.ability_attr);
        EQ("starter passive attr", sys.starters[dex_after.root].passive_attr, dex_after.passive_attr);
        if (bad) mismatches++;
        cases++; by_option[option]++;
    }
    printf("Mystery encounter Training Session: %d cases run (light %d, moderate %d, heavy %d, leave %d), %d skipped (%d Pokemon the picker refuses, %d with nobody else able to fight), "
           "%d natures and %d abilities offered compared, %d IV improvements, %d dex entries, %d items, %d moves and %d heavy-training tags compared (%d fights with a fainted lead, whose swap is the battle start's), %d fields. TOTAL mismatches %d\n",
           cases, by_option[0], by_option[1], by_option[2], by_option[3], skipped, unpickable, all_fainted, natures_checked, abilities_checked, improved_checked, dex_checked, items_checked,
           moves_checked, tags_heavy, lead_swapped, fields_checked, mismatches);
    return mismatches ? 1 : 0;
}
