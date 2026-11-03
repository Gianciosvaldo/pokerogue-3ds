/* The Strong Stuff against upstream at 8555c08: replays the "-strong" trace of tests/mystery-strong.producer.test.ts (the real encounter:
 * onInit in executeWithSeedOffset(.., wave), onOptionPhase in (.., getSeedOffset() * 500), the battle's await taking the rest out of the
 * offset, initBattleWithEnemyConfig really run, the boss's post-summon effect called by hand) through game-core.
 *
 * Per case, compared with what upstream did: the starting stats; which options are offered; that onInit leaves the run's RNG alone and its
 * config (Shuckle, boss, five bars, never shiny, Hardy, the four moves, the six items, a full level boost per ten waves, no switching); the
 * run RNG after the option; the rewards set; for the juice: every party member's items (who got the -15 and who the +10) and recalculated
 * stats, HP and the result text; for the fight: the boss that enters (id, form, level, IVs, nature, ability, HP, stats, moves, five bars, the
 * items with their stacks), the start-of-battle Gastro Acid and Stealth Rock, that fleeing is off and catching is not allowed, and the Def and Sp.
 * Def stages the post-summon effect raises.
 * Usage: mystery_strong_diff_test <trace>; exit 0 only with 0 mismatches. */
#include "../src/internal.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static gc_run run;
static gc_system sys;
static FILE *f;

static long long integer(void) { long long v; if (fscanf(f, "%lld", &v) != 1) { fprintf(stderr, "malformed input at byte %ld\n", ftell(f)); exit(2); } return v; }
static double real(void) { double v; if (fscanf(f, "%lf", &v) != 1) { fprintf(stderr, "malformed input at byte %ld\n", ftell(f)); exit(2); } return v; }
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
    if (argc != 2 || !(f = fopen(argv[1], "r"))) { fputs("usage: mystery_strong_diff_test <trace>\n", stderr); return 2; }
    tag('V');
    integer();
    integer();
    int n_mons = 0;
    int cases = 0, mismatches = 0, skipped = 0, by_option[2] = {0}, items_checked = 0, moves_checked = 0, ties = 0, juice_items = 0, boosts = 0, contrary = 0;
    char c;
    while (fscanf(f, " %c", &c) == 1) {
        if (c == 'Z') break;
        if (c == 'P') {
            n_mons = (int)integer();
            if (n_mons < 3 || n_mons > 6) { fputs("party size\n", stderr); return 2; }
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
        static item_row before_items[120], boss_rows[120], after_items[120];
        const int n_before = read_items(before_items, 1);
        tag('A');
        const int meets[2] = {(int)integer(), (int)integer()};
        {
            char t = '?';
            if (fscanf(f, " %c", &t) != 1) return 2;
            if (t == 'E') { skipped++; continue; }
            ungetc(t, f);
        }
        tag('R');
        const pr_rng before = rng_vals();
        tag('I');
        const pr_rng after_init = rng_vals();
        tag('W');
        struct { int species; double level_add; int boss, segments, shiny_false, nature, disable_switch, n_moves, moves[4], n_mods, arg[8], stack[8]; } init;
        memset(&init, 0, sizeof init);
        init.species = (int)integer(); init.level_add = real(); init.boss = (int)integer(); init.segments = (int)integer(); init.shiny_false = (int)integer();
        init.nature = (int)integer(); init.disable_switch = (int)integer();
        init.n_moves = (int)integer();
        for (int k = 0; k < init.n_moves && k < 4; k++) init.moves[k] = (int)integer();
        init.n_mods = (int)integer();
        for (int k = 0; k < init.n_mods && k < 8; k++) { init.arg[k] = (int)integer(); init.stack[k] = (int)integer(); }
        tag('Q');
        const pr_rng after_option = rng_vals();
        tag('S');
        const int n_rewards = (int)integer(), fill = (int)integer(), n_funcs = (int)integer();
        char func_name[24] = "";
        for (int k = 0; k < n_funcs; k++) { char tmp[24]; word(tmp, sizeof tmp); if (!k) snprintf(func_name, sizeof func_name, "%s", tmp); }
        const int n_opts = (int)integer(), n_tier_opts = (int)integer(), leave_calls = (int)integer(), leave_heal = (int)integer();
        tag('M');
        const int n_said = (int)integer();
        char said[4][96];
        for (int k = 0; k < n_said && k < 4; k++) word(said[k], sizeof said[k]);
        tag('T');
        const int tok_species[2] = {(int)integer(), (int)integer()};
        const int tok_reduction = (int)integer(), tok_increase = (int)integer();
        tag('Y');
        int up_hp[6], up_max[6], up_stats[6][6];
        for (int m = 0; m < n_mons; m++) {
            if (m) semicolon();
            up_hp[m] = (int)integer(); up_max[m] = (int)integer();
            for (int k = 0; k < 6; k++) up_stats[m][k] = (int)integer();
        }
        tag('V');
        const int n_after = read_items(after_items, 1);
        boss_row up;
        memset(&up, 0, sizeof up);
        int boss_index = 0, st_effect = 0, n_boss_items = 0, n_effects = 0, dbl = 0, n_post = 0, mult = 1;
        char post[4][96], fx_dump[4][8];
        int fx[4][5];
        (void)fx_dump;
        if (option == 1) {
            tag('D');
            integer();
            dbl = (int)integer();
            const int n_levels = (int)integer();
            for (int k = 0; k < n_levels; k++) integer();
            const int n_queued = (int)integer();
            char tmp[64];
            for (int k = 0; k < n_queued; k++) word(tmp, sizeof tmp);
            tag('X'); read_boss(&up, true);
            tag('N'); st_effect = (int)integer(); integer();
            tag('J'); n_boss_items = read_items(boss_rows, 0);
            tag('F'); n_effects = (int)integer();
            for (int k = 0; k < n_effects && k < 4; k++) {
                if (k) semicolon();
                for (int j = 0; j < 5; j++) fx[k][j] = (int)integer();
            }
            tag('K'); (void)rng_vals(); (void)rng_vals(); n_post = (int)integer();
            for (int k = 0; k < n_post && k < 4; k++) word(post[k], sizeof post[k]);
            tag('Z'); mult = (int)integer(); boss_index = (int)integer();
        }
        tag('E');
        (void)n_opts;

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
            mon->b.status = 0;
            mon->b.n_moves = (uint8_t)setup[m].n_moves;
            memset(mon->b.moves, 0, sizeof mon->b.moves);
            memset(mon->b.pp_used, 0, sizeof mon->b.pp_used);
            for (int k = 0; k < setup[m].n_moves; k++) mon->b.moves[k] = (uint16_t)setup[m].moves[k];
            for (int k = 0; k < 6; k++) mon->b.ivs[k] = (uint8_t)setup[m].ivs[k];
            mon->base_nature = (uint8_t)setup[m].nature;
            mon->has_custom_nature = false;
            mon->b.nature = (uint8_t)setup[m].nature;
            mon->luck = (int8_t)setup[m].luck;
            mon->b.ability = mon->b.base_ability = (uint16_t)setup[m].ability;
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
        run.mystery_type = GC_ME_THE_STRONG_STUFF;
        memset(&run.screen, 0, sizeof run.screen);
        run.screen.kind = GC_SCREEN_MYSTERY;
        run.screen.mystery_type = GC_ME_THE_STRONG_STUFF;
        gc_mystery_setup_screen(&run);
        CHECK(run.screen.n_options == 2, "%d options", run.screen.n_options);
        for (int o = 0; o < 2; o++) CHECK(run.screen.option_ok[o] == (meets[o] != 0), "option %d enabled %d, upstream %d", o, run.screen.option_ok[o], meets[o]);
        CHECK(same_rng(&run.run.rng, &after_init), "onInit moved the run RNG (c %.0f, upstream %.0f)", run.run.rng.c, after_init.c);
        /* onInit's config */
        EQ("onInit species", init.species, SPECIES_ID_SHUCKLE);
        EQ("onInit is a boss", init.boss, 1);
        EQ("onInit bars", init.segments, 5);
        EQ("onInit shiny lock", init.shiny_false, 0);
        EQ("onInit nature is Hardy", init.nature, NATURE_HARDY);
        EQ("onInit disables switching", init.disable_switch, 1);
        EQ("onInit level modifier", (int)init.level_add, 1);
        EQ("onInit move count", init.n_moves, 4);
        { static const int want[4] = {MOVE_ID_INFESTATION, MOVE_ID_SALT_CURE, MOVE_ID_GASTRO_ACID, MOVE_ID_HEAL_ORDER};
          for (int k = 0; k < 4; k++) EQ("onInit move", want[k], init.moves[k]); }
        EQ("onInit item count", init.n_mods, 5);
        { static const int want_arg[5] = {BERRY_TYPE_SITRUS, BERRY_TYPE_ENIGMA, BERRY_TYPE_APICOT, BERRY_TYPE_GANLON, BERRY_TYPE_LUM};
          static const int want_stack[5] = {1, 1, 1, 1, 2};
          for (int k = 0; k < 5 && k < init.n_mods; k++) { EQ("onInit berry", init.arg[k], want_arg[k]); EQ("onInit berry stack", init.stack[k], want_stack[k]); } }

        {
            gc_event e;
            while (gc_run_next_event(&run, &e)) {}
        }
        bool ok = false;
        const char *w2 = "";
        gc_mystery_execute(&run, option, 0, 0, &w2, &ok);
        CHECK(ok, "option refused: %s", w2);
        CHECK(same_rng(&run.run.rng, &after_option), "the run RNG after the option differs (c %.0f, upstream %.0f)", run.run.rng.c, after_option.c);
        EQ("rewards set", n_rewards, 1);
        EQ("rewards fill the rest", fill, 1);
        EQ("no reward options", n_tier_opts, 0);
        EQ("no catching", run.me_catch_allowed, 0);

        int c_said = 0, up_said = 0;
        { gc_event e;
          while (gc_run_next_event(&run, &e))
              if (e.kind == GC_EV_MESSAGE && e.msg.key && !strcmp(e.msg.key, "mysteryEncounters/theStrongStuff:option.1.selected2")) {
                  c_said++;
                  /* its tokens: the two Pokemon named (by species: a tie between equal names reads the same) and the two values */
                  int got[4] = {-9, -9, -9, -9};
                  for (int a = 0; a < e.msg.n_args; a++) {
                      const char *n = e.msg.args[a].name;
                      if (!n) continue;
                      if (!strcmp(n, "highBstPokemon1")) got[0] = (int)(e.msg.args[a].v >> 16);
                      else if (!strcmp(n, "highBstPokemon2")) got[1] = (int)(e.msg.args[a].v >> 16);
                      else if (!strcmp(n, "reductionValue")) got[2] = (int)e.msg.args[a].v;
                      else if (!strcmp(n, "increaseValue")) got[3] = (int)e.msg.args[a].v;
                  }
                  EQ("token highBstPokemon1", got[0], tok_species[0]);
                  EQ("token highBstPokemon2", got[1], tok_species[1]);
                  EQ("token reductionValue", got[2], tok_reduction);
                  EQ("token increaseValue", got[3], tok_increase);
              } }
        for (int k = 0; k < n_said && k < 4; k++) up_said += !strcmp(said[k], "mysteryEncounters/theStrongStuff:option.1.selected2");
        EQ("result text", c_said, up_said);

        if (option == 0) {
            CHECK(run.n_enemy == 0 && !run.me_battle, "the juice started a battle");
            EQ("leave calls", leave_calls, 1);
            EQ("leave asks for the heal shop", leave_heal, 1);
            EQ("no reward function", n_funcs, 0);
            const pr_reward_custom *cs = &gc_int(&run)->reward.me_custom;
            EQ("shop fill", cs->fill_remaining, 1);
            EQ("shop options", cs->n_options, 0);
            EQ("shop functions", cs->n_funcs, 0);
            EQ("shop tiers", cs->n_tiers, 0);
            int healshop = 0, shop = 0;
            for (gc_rphase p; gc_rq_next(&gc_int(&run)->rq, &p);) if (p.type == RP_SELECT_MODIFIER) { healshop += p.a == 1; shop += p.a == 2; }
            EQ("item shop", shop, 1);
            EQ("no heal shop", healshop, 0);
            /* the party's items afterwards, as a set */
            EQ("item count", run.n_items, n_after);
            for (int k = 0; k < n_after; k++) {
                bool found = false;
                const uint32_t holder = after_items[k].holder < 0 ? 0 : (uint32_t)(0x4000 + after_items[k].holder);
                for (int j = 0; j < run.n_items && !found; j++) {
                    const gc_item *it = &run.items[j];
                    found = !strcmp(it->key, after_items[k].key) && it->arg == after_items[k].arg && it->pokemon_id == holder && it->stack == after_items[k].stack;
                }
                CHECK(found, "item %s/%d held by %d x%d is missing", after_items[k].key, after_items[k].arg, after_items[k].holder, after_items[k].stack);
                juice_items += !strcmp(after_items[k].key, "MYSTERY_ENCOUNTER_SHUCKLE_JUICE");
            }
            for (int m = 0; m < n_mons; m++) {
                const gc_mon *mon = &run.party[st[m]];
                char nm[40];
                snprintf(nm, sizeof nm, "party %d hp", m); EQ(nm, mon->b.hp, up_hp[m]);
                snprintf(nm, sizeof nm, "party %d max hp", m); EQ(nm, mon->b.stats[0], up_max[m]);
                for (int k = 0; k < 6; k++) { snprintf(nm, sizeof nm, "party %d stat %d", m, k); EQ(nm, mon->b.stats[k], up_stats[m][k]); }
            }
            { /* a tie in the base stat totals */
                int bst[6];
                for (int m = 0; m < n_mons; m++) { const pr_form *fm = gc_form(setup[m].species, setup[m].form); bst[m] = 0; for (int k = 0; k < 6; k++) bst[m] += fm->base_stats[k]; }
                bool tie = false;
                for (int a = 0; a < n_mons; a++) for (int b2 = a + 1; b2 < n_mons; b2++) tie |= bst[a] == bst[b2];
                ties += tie; }
        } else {
            CHECK(run.n_enemy == 1 && run.in_battle && run.me_battle, "no battle (%d enemies)", run.n_enemy);
            EQ("double", run.double_battle, dbl);
            EQ("leave calls", leave_calls, 0);
            EQ("one reward function", n_funcs, 1);
            CHECK(!strcmp(func_name, "SOUL_DEW") && gc_int(&run)->me_reward.func == gc_reward_item_index("SOUL_DEW"), "reward function is %d, upstream %s", gc_int(&run)->me_reward.func, func_name);
            EQ("rewards set", gc_int(&run)->me_reward.active, 1);
            EQ("rewards fill", gc_int(&run)->me_reward.fill, 1);
            EQ("reward is no item", gc_int(&run)->me_reward.has_item, 0);
            EQ("fleeing is off", gc_int(&run)->me_no_flee, 1);
            EQ("no switch flag left behind", gc_int(&run)->me_no_switch, 0);
            const gc_mon *e = &run.enemy[0];
            EQ("boss species", e->b.species, up.species);
            EQ("boss form", e->b.form, up.form);
            EQ("boss level", e->b.level, up.level);
            EQ("boss segments", e->b.boss_segments, up.segments);
            EQ("boss segment index", e->b.boss_segment_index, boss_index);
            for (int k = 0; k < 6; k++) { char n[32]; snprintf(n, sizeof n, "boss iv %d", k); EQ(n, e->b.ivs[k], up.ivs[k]); }
            EQ("boss nature", e->b.nature, up.nature);
            EQ("boss ability index", e->ability_index, up.ability);
            EQ("boss passive flag", e->b.passive_flag, up.passive);
            EQ("boss shiny", e->b.shiny, up.shiny);
            EQ("boss variant", e->variant, up.variant);
            EQ("boss gender", e->b.gender, up.gender);
            EQ("boss friendship", e->b.friendship, up.friendship);
            EQ("boss id", e->b.id, up.id);
            EQ("boss max hp", e->b.stats[0], up.stats[0]);
            EQ("boss hp", e->b.hp, up.hp);
            for (int k = 0; k < 6; k++) { char n[32]; snprintf(n, sizeof n, "boss stat %d", k); EQ(n, e->b.stats[k], up.stats[k]); }
            EQ("boss move count", e->b.n_moves, up.n_moves);
            for (int k = 0; k < up.n_moves && k < 4; k++) { char n[32]; snprintf(n, sizeof n, "boss move %d", k); EQ(n, e->b.moves[k], up.moves[k]); moves_checked++; }
            EQ("boss status", e->b.status, st_effect);
            EQ("boss item count", run.n_enemy_items, n_boss_items);
            for (int k = 0; k < run.n_enemy_items && k < n_boss_items; k++) {
                const gc_item *it = &run.enemy_items[k];
                CHECK(!strcmp(it->key, boss_rows[k].key) && it->arg == boss_rows[k].arg && it->stack == boss_rows[k].stack && it->pokemon_id == e->b.id,
                      "boss item %d is %s/%d x%d, upstream %s/%d x%d", k, it->key, it->arg, it->stack, boss_rows[k].key, boss_rows[k].arg, boss_rows[k].stack);
                items_checked++;
            }
            EQ("party item count", run.n_items, n_before);
            /* startOfBattleEffects: Gastro Acid, then Stealth Rock, from the enemy at the player's lead, ignoring PP */
            EQ("start-of-battle effects", gc_int(&run)->n_me_start, n_effects);
            for (int k = 0; k < n_effects && k < gc_int(&run)->n_me_start && k < 4; k++) {
                EQ("effect source (enemy)", gc_int(&run)->me_start[k].source, fx[k][0]);
                EQ("effect target count", fx[k][1], 1);
                EQ("effect target", gc_int(&run)->me_start[k].target, fx[k][2]);
                EQ("effect move", gc_int(&run)->me_start[k].move, fx[k][3]);
                EQ("effect use mode", gc_int(&run)->me_start[k].use_mode, fx[k][4]);
            }
            /* the post-summon effect: its message, then Def and Sp. Def up one stage (the holder's Simple doubles it, Contrary reverses it) */
            EQ("post-summon effects", n_post, 2);
            CHECK(n_post == 2 && !strcmp(post[0], "MSG_mysteryEncounters/theStrongStuff:option.2.statBoost") && !strcmp(post[1], "StatStageChangePhase_2_1_4_1"), "upstream's post-summon effects differ");
            EQ("post-summon flag spent", run.enemy[0].b.me_post_summon, 0);
            EQ("Def stage", e->b.stat_stages[STAT_DEF - 1], mult);
            EQ("Sp. Def stage", e->b.stat_stages[STAT_SPDEF - 1], mult);
            EQ("Atk stage", e->b.stat_stages[STAT_ATK - 1], 0);
            EQ("Sp. Atk stage", e->b.stat_stages[STAT_SPATK - 1], 0);
            EQ("Spd stage", e->b.stat_stages[STAT_SPD - 1], 0);
            boosts++; contrary += mult == -1;
        }
        if (bad) mismatches++;
        cases++; by_option[option]++;
    }
    printf("Mystery encounter The Strong Stuff: %d cases run (juice %d, battle %d), %d skipped (option not available), %d juice parties with a tie in base stat totals, "
           "%d juice items, %d fights with the post-summon boost compared (%d with a Contrary boss), %d items and %d moves compared, %d fields. TOTAL mismatches %d\n",
           cases, by_option[0], by_option[1], skipped, ties, juice_items, boosts, contrary, items_checked, moves_checked, fields_checked, mismatches);
    return mismatches ? 1 : 0;
}
