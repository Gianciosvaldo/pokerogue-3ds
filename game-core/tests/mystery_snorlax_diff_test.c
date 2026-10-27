/* Slumbering Snorlax against upstream at 8555c08: replays the "-snorlax" trace of tests/mystery-snorlax.producer.test.ts (the real
 * encounter: onInit in executeWithSeedOffset(.., wave), then one option with onOptionPhase in (.., * 500) and initBattleWithEnemyConfig
 * really run) through game-core.
 *
 * Per case, compared with what upstream did: the starting stats; which options are offered and the Pokemon that steals; that onInit leaves
 * the run's RNG alone; for the battle: the run RNG after the option, the rewards set (Leftovers, fill the rest), the boss that enters (new
 * id, level, IVs, nature, HP, stats, moves, sleep and its six turns, items with their rolled stacks, segments), the start-of-battle Snore
 * and that fleeing is off; for the rest: the party heal, the message, no shop; for the steal: the Leftovers shop, the experience and the
 * Pokemon that gets it.
 * Usage: mystery_snorlax_diff_test <trace>; exit 0 only with 0 mismatches. */
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

int main(int argc, char **argv)
{
    if (argc != 2 || !(f = fopen(argv[1], "r"))) { fputs("usage: mystery_snorlax_diff_test <trace>\n", stderr); return 2; }
    tag('V');
    integer();
    integer();
    int n_mons = 0;
    int lead_differs = 0, cases = 0, mismatches = 0, by_option[3] = {0}, skipped = 0, items_checked = 0, moves_checked = 0, zero_stacks = 0, effects_checked = 0, steals_exp = 0;
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
        struct { int species, form, level, nature, ivs[6], hp, max, stats[6], n_moves, moves[4], luck, ability; } setup[6];
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
        }
        tag('H');
        int hps[6];
        for (int m = 0; m < n_mons; m++) hps[m] = (int)integer();
        tag('B');
        int balls[5];
        for (int k = 0; k < 5; k++) balls[k] = (int)integer();
        tag('L');
        static item_row before_items[120], boss_rows[120];
        const int n_before = read_items(before_items, 1);
        tag('A');
        const int meets[3] = {(int)integer(), (int)integer(), (int)integer()};
        const int up_primary = (int)integer();
        {
            char t = '?';
            if (fscanf(f, " %c", &t) != 1) return 2;
            if (t == 'E') { skipped++; continue; }   /* the option could not be chosen upstream */
            ungetc(t, f);
        }
        tag('R');
        const pr_rng before = rng_vals();
        tag('I');
        const pr_rng after_init = rng_vals();
        tag('W');
        struct { int species; double level_add; int boss, shiny, status, status_turns, nature, n_moves, moves[4], n_mods, arg[8], stack[8], ai; double scale; } init;
        memset(&init, 0, sizeof init);
        init.species = (int)integer(); init.level_add = real(); init.boss = (int)integer(); init.shiny = (int)integer();
        init.status = (int)integer(); init.status_turns = (int)integer(); init.nature = (int)integer();
        init.n_moves = (int)integer();
        for (int k = 0; k < init.n_moves && k < 4; k++) init.moves[k] = (int)integer();
        init.n_mods = (int)integer();
        for (int k = 0; k < init.n_mods && k < 8; k++) { init.arg[k] = (int)integer(); init.stack[k] = (int)integer(); }
        init.ai = (int)integer(); init.scale = real();
        pr_rng after_option = {0};
        int mode = 0, dbl = 0, n_levels = 0, levels[2], n_queued = 0, n_rewards = 0, fill = 0, n_funcs = 0, n_opts = 0, leave_calls = 0, leave_heal = -1, n_boss_items = 0;
        int exp_calls = 0, exp_who = -1, exp_amount = -1, exp_wave = -1, n_effects = 0, n_heals = 0, n_msgs = 0;
        char queued[4][48], tags_text[96] = "", func_names[4][24], heal_text[2][64], msg_key[2][96], fx_dump[4][8];
        int fx[4][5];
        int st_effect = 0, st_turns = -1, up_ai = 0;
        double up_scale = 0;
        boss_row up;
        memset(&up, 0, sizeof up);
        tag('Q'); after_option = rng_vals();
        if (option == 0) {
            mode = (int)integer(); dbl = (int)integer(); n_levels = (int)integer();
            for (int k = 0; k < n_levels && k < 2; k++) levels[k] = (int)integer();
            n_queued = (int)integer();
            for (int k = 0; k < n_queued && k < 4; k++) word(queued[k], sizeof queued[k]);
        }
        tag('S'); n_rewards = (int)integer(); fill = (int)integer(); n_funcs = (int)integer();
        for (int k = 0; k < n_funcs && k < 4; k++) word(func_names[k], sizeof func_names[k]);
        n_opts = (int)integer(); leave_calls = (int)integer(); leave_heal = (int)integer();
        if (option == 0) {
            tag('X'); read_boss(&up, true);
            tag('N'); st_effect = (int)integer(); st_turns = (int)integer(); up_scale = real(); up_ai = (int)integer();
            tag('T'); word(tags_text, sizeof tags_text);
            tag('J'); n_boss_items = read_items(boss_rows, 0);
            tag('F'); n_effects = (int)integer();
            for (int k = 0; k < n_effects && k < 4; k++) {
                if (k) semicolon();
                for (int j = 0; j < 5; j++) fx[k][j] = (int)integer();
            }
        } else if (option == 1) {
            tag('K'); exp_calls = (int)integer();
            tag('D'); n_heals = (int)integer();
            for (int k = 0; k < n_heals && k < 2; k++) word(heal_text[k], sizeof heal_text[k]);
            n_msgs = (int)integer();
            for (int k = 0; k < n_msgs && k < 2; k++) word(msg_key[k], sizeof msg_key[k]);
        } else {
            tag('K'); exp_calls = (int)integer(); exp_who = (int)integer(); exp_amount = (int)integer(); exp_wave = (int)integer();
        }
        tag('E');
        (void)fx_dump; (void)n_opts; (void)mode; (void)levels; (void)n_queued; (void)queued; (void)up_ai; (void)up_scale; (void)init.ai; (void)init.scale;

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
        run.mystery_type = GC_ME_SLUMBERING_SNORLAX;
        memset(&run.screen, 0, sizeof run.screen);
        run.screen.kind = GC_SCREEN_MYSTERY;
        run.screen.mystery_type = GC_ME_SLUMBERING_SNORLAX;
        gc_mystery_setup_screen(&run);
        CHECK(run.screen.n_options == 3, "%d options", run.screen.n_options);
        for (int o = 0; o < 3; o++) CHECK(run.screen.option_ok[o] == (meets[o] != 0), "option %d enabled %d, upstream %d", o, run.screen.option_ok[o], meets[o]);
        {
            const int p = gc_mystery_primary_pokemon(&run, GC_ME_SLUMBERING_SNORLAX, 2);
            EQ("primary pokemon of the steal option", p >= 0 ? (int)run.party[p].b.id - 0x4000 : -1, up_primary);
        }
        CHECK(same_rng(&run.run.rng, &after_init), "onInit moved the run RNG (c %.0f, upstream %.0f)", run.run.rng.c, after_init.c);
        /* onInit's config: the boss, the wave's level boost, the sleep, the nature, the moves; the C side keeps none of it */
        EQ("onInit species", init.species, SPECIES_ID_SNORLAX);
        EQ("onInit is a boss", init.boss, 1);
        EQ("onInit shiny lock", init.shiny, 0);
        EQ("onInit status is sleep", init.status, STATUS_EFFECT_SLEEP);
        EQ("onInit nature is Docile", init.nature, NATURE_DOCILE);
        EQ("onInit level modifier x2", (int)(init.level_add * 2), 1);
        EQ("onInit move count", init.n_moves, 4);
        { static const int want[4] = {MOVE_ID_BODY_SLAM, MOVE_ID_CRUNCH, MOVE_ID_SLEEP_TALK, MOVE_ID_REST};
          for (int k = 0; k < 4; k++) EQ("onInit move", want[k], init.moves[k]); }
        EQ("onInit modifier count", init.n_mods, 5);

        memcpy(&sys_before, &sys, sizeof sys);
        {
            gc_event e;
            while (gc_run_next_event(&run, &e)) {}
        }
        bool ok = false;
        const char *w2 = "";
        gc_mystery_execute(&run, option, 0, 0, &w2, &ok);
        CHECK(ok, "option refused: %s", w2);
        CHECK(same_rng(&run.run.rng, &after_option), "the run RNG after the option differs (c %.0f, upstream %.0f)", run.run.rng.c, after_option.c);
        EQ("leave calls", leave_calls, option == 0 ? 0 : 1);
        if (option == 0) {
            CHECK(!strcmp(tags_text, "-"), "the boss's tags (upstream %s)", tags_text);
            CHECK(run.n_enemy == 1 && run.in_battle && run.me_battle, "no battle (%d enemies)", run.n_enemy);
            EQ("double", run.double_battle, dbl);
            EQ("catch allowed", run.me_catch_allowed, 1);
            EQ("fleeing is off", gc_int(&run)->me_no_flee, 1);
            EQ("rewards set", gc_int(&run)->me_reward.active, n_rewards == 1);
            EQ("reward is a function", gc_int(&run)->me_reward.has_item, 0);
            EQ("one reward function", n_funcs, 1);
            CHECK(!strcmp(func_names[0], "LEFTOVERS") && gc_int(&run)->me_reward.func == gc_reward_item_index("LEFTOVERS"), "reward function is %d, upstream %s", gc_int(&run)->me_reward.func, func_names[0]);
            EQ("rewards fill the rest", gc_int(&run)->me_reward.fill, fill);
            const gc_mon *e = &run.enemy[0];
            EQ("boss species", e->b.species, up.species);
            EQ("boss form", e->b.form, up.form);
            EQ("boss level", e->b.level, up.level);
            EQ("boss segments", e->b.boss_segments, up.segments);
            for (int k = 0; k < 6; k++) { char n[32]; snprintf(n, sizeof n, "boss iv %d", k); EQ(n, e->b.ivs[k], up.ivs[k]); }
            EQ("boss nature", e->b.nature, up.nature);
            EQ("boss ability index", e->ability_index, up.ability);
            EQ("boss passive flag", e->b.passive_flag, up.passive);
            EQ("boss shiny", e->b.shiny, up.shiny);
            EQ("boss variant", e->variant, up.variant);
            EQ("boss gender", e->b.gender, up.gender);
            EQ("boss friendship", e->b.friendship, up.friendship);
            EQ("boss id (new)", e->b.id, up.id);
            EQ("boss max hp", e->b.stats[0], up.stats[0]);
            EQ("boss hp", e->b.hp, up.hp);
            for (int k = 0; k < 6; k++) { char n[32]; snprintf(n, sizeof n, "boss stat %d", k); EQ(n, e->b.stats[k], up.stats[k]); }
            EQ("boss move count", e->b.n_moves, up.n_moves);
            for (int k = 0; k < up.n_moves && k < 4; k++) { char n[32]; snprintf(n, sizeof n, "boss move %d", k); EQ(n, e->b.moves[k], up.moves[k]); moves_checked++; }
            EQ("boss status", e->b.status, st_effect);
            EQ("boss sleep turns", e->b.status_sleep_turns, st_turns);
            EQ("boss item count", run.n_enemy_items, n_boss_items);
            for (int k = 0; k < run.n_enemy_items && k < n_boss_items; k++) {
                const gc_item *it = &run.enemy_items[k];
                CHECK(!strcmp(it->key, boss_rows[k].key) && it->arg == boss_rows[k].arg && it->stack == boss_rows[k].stack && it->pokemon_id == e->b.id,
                      "boss item %d is %s/%d x%d, upstream %s/%d x%d", k, it->key, it->arg, it->stack, boss_rows[k].key, boss_rows[k].arg, boss_rows[k].stack);
                items_checked++;
            }
            /* the stacks the config asked for are the ones the boss holds (the held-item caps do not bite: 1 of each) */
            for (int k = 0; k < 3; k++) EQ("onInit stack of a fixed item", init.stack[k], 1);
            { int soothe = 0, egg = 0;
              for (int k = 0; k < n_boss_items; k++) { soothe += !strcmp(boss_rows[k].key, "SOOTHE_BELL") ? boss_rows[k].stack : 0; egg += !strcmp(boss_rows[k].key, "LUCKY_EGG") ? boss_rows[k].stack : 0; }
              EQ("Soothe Bell stack rolled by onInit", init.stack[3], soothe);
              EQ("Lucky Egg stack rolled by onInit", init.stack[4], egg);
              zero_stacks += (init.stack[3] == 0) + (init.stack[4] == 0); }
            EQ("party item count", run.n_items, n_before);
            /* startOfBattleEffects: one Snore, from the enemy at the player's lead, ignoring PP */
            EQ("start-of-battle effects", gc_int(&run)->n_me_start, n_effects);
            if (n_effects == 1 && gc_int(&run)->n_me_start == 1) {
                EQ("effect source (enemy)", gc_int(&run)->me_start[0].source, fx[0][0]);
                EQ("effect target count", fx[0][1], 1);
                EQ("effect target", gc_int(&run)->me_start[0].target, fx[0][2]);
                EQ("effect move", gc_int(&run)->me_start[0].move, fx[0][3]);
                EQ("effect use mode", gc_int(&run)->me_start[0].use_mode, fx[0][4]);
                effects_checked++;
            }
        } else if (option == 1) {
            CHECK(run.n_enemy == 0 && !run.me_battle, "a rest that started a battle");
            EQ("rewards set", n_rewards, 0);
            EQ("rest leaves without the heal shop", leave_heal, 0);
            EQ("no experience", exp_calls, 0);
            EQ("one heal queued", n_heals, 1);
            CHECK(!strcmp(heal_text[0], "PartyHealPhase:true"), "upstream queued %s", heal_text[0]);
            EQ("one message", n_msgs, 1);
            int heals = 0, shop = 0, healshop = 0, msgs = 0;
            for (gc_rphase p; gc_rq_next(&gc_int(&run)->rq, &p);) {
                heals += p.type == RP_PARTY_HEAL;
                if (p.type == RP_SELECT_MODIFIER) { healshop += p.a == 1; shop += p.a == 2; }
            }
            { gc_event e;
              while (gc_run_next_event(&run, &e))
                  if (e.kind == GC_EV_MESSAGE && e.msg.key && !strcmp(e.msg.key, msg_key[0])) msgs++; }
            EQ("party healed", heals, 1);
            EQ("no heal shop", healshop, 0);
            EQ("no item shop", shop, 0);
            EQ("the rest message", msgs, 1);
            CHECK(!strcmp(msg_key[0], "mysteryEncounters/slumberingSnorlax:option.2.restResult"), "upstream's message is %s", msg_key[0]);
        } else {
            CHECK(run.n_enemy == 0 && !run.me_battle, "a steal that started a battle");
            EQ("rewards set", n_rewards, 1);
            EQ("steal leaves without the heal shop", leave_heal, 0);
            EQ("one reward function", n_funcs, 1);
            CHECK(!strcmp(func_names[0], "LEFTOVERS"), "upstream's reward is %s", func_names[0]);
            EQ("shop fills the rest", fill, 0);
            const pr_reward_custom *cs = &gc_int(&run)->reward.me_custom;
            EQ("shop functions", cs->n_funcs, 1);
            EQ("shop options", cs->n_options, 0);
            if (cs->n_funcs == 1) EQ("shop function is Leftovers", cs->funcs[0], gc_reward_item_index("LEFTOVERS"));
            EQ("shop fill", cs->fill_remaining, 0);
            EQ("experience set", exp_calls, 1);
            /* upstream gives it to the encounter's primary Pokemon (the lead), which is the Pokemon that stole only when that one leads */
            lead_differs += exp_who != up_primary;
            EQ("base experience of Snorlax", gc_form(SPECIES_ID_SNORLAX, 0)->base_exp, exp_amount);
            EQ("experience scaled by the wave", exp_wave, 1);
            bool others = false;
            int heal = 0, shop = 0;
            double got = -1;
            for (gc_rphase p; gc_rq_next(&gc_int(&run)->rq, &p);) {
                if (p.type == RP_SELECT_MODIFIER) { heal += p.a == 1; shop += p.a == 2; }
                if (p.type != RP_EXP) continue;
                if ((int)run.party[p.a].b.id - 0x4000 != exp_who) others = true;
                else got = p.d;
            }
            EQ("steal opens the item shop", shop, 1);
            EQ("steal opens no heal shop", heal, 0);
            { pr_exp_party pp; int idx[PR_MAX_EXP_PARTY]; double amt[PR_MAX_EXP_PARTY];
              const uint32_t id = (uint32_t)(0x4000 + exp_who);
              gc_exp_party_build(&run, &pp, &id, 1);
              pp.trainer_battle = false; pp.me_exp_multiplier = 1.0;
              const int n = pr_run_apply_party_exp(&pp, exp_amount, true, run.wave, idx, amt);
              double want = 0;
              for (int k = 0; k < n; k++) if (run.party[run.party_order[idx[k]]].b.id == id) want = amt[k];
              EQ("experience the Pokemon that stole gets", (long long)(got < 0 ? 0 : got), (long long)want); }
            CHECK(!others || gc_item_stack(&run, "EXP_SHARE") > 0, "experience for a Pokemon that did not steal, without an Exp. Share");
            steals_exp++;
        }
        if (bad) mismatches++;
        cases++; by_option[option]++;
    }
    printf("Mystery encounter Slumbering Snorlax: %d cases run (battle %d, rest %d, steal %d), %d skipped (option not available), %d fights with the Snore start effect compared, "
           "%d steals with experience (%d to a lead that did not steal), %d items (%d rolled stacks of zero) and %d moves compared, %d fields. TOTAL mismatches %d\n",
           cases, by_option[0], by_option[1], by_option[2], skipped, effects_checked, steals_exp, lead_differs, items_checked, zero_stacks, moves_checked, fields_checked, mismatches);
    return mismatches ? 1 : 0;
}
