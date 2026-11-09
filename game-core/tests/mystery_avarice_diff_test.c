/* Absolute Avarice against upstream at 8555c08: replays the "-avarice" trace of tests/mystery-avarice.producer.test.ts (the real encounter: onInit in
 * executeWithSeedOffset(.., wave), onVisualsStart really run, onOptionPhase in (.., getSeedOffset() * 500), the battle option's await taking the rest out of the
 * offset, initBattleWithEnemyConfig really run, the rewards callback and the boss's post-summon effect called by hand, catchPokemon recorded and completed with the
 * real Pokemon.addToParty) through game-core.
 *
 * Per case, compared with what upstream did: the starting stats; which options are offered; that onInit leaves the run's RNG alone and its config (Greedent, boss,
 * three bars, never shiny, the four moves, one Berry for every stack stolen, the post-summon tag); the party's items once the Berries are gone; the run RNG after
 * the option; the rewards set; for the fight: the boss that enters (id, form, level, IVs, nature, ability, HP, stats, moves, three bars, the Berries it holds
 * and their stacks), Stuff Cheeks at itself before the first command, that fleeing and catching are off, the Reviver Seeds and the text the rewards callback gives,
 * and the stat stages the post-summon effect raises; for the plea: the Berries each Pokemon gets back; for the feast: the Greedent as it is caught and as it joins.
 * Usage: mystery_avarice_diff_test <trace>; exit 0 only with 0 mismatches. */
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
static void rec(const char *want)
{
    char w[16];
    word(w, sizeof w);
    if (strcmp(w, want)) { fprintf(stderr, "wrong record: wanted %s, got %s\n", want, w); exit(2); }
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
static void read_boss(boss_row *b)
{
    b->species = (int)integer(); b->form = (int)integer(); b->level = (int)integer();
    integer();   /* isBoss */
    b->segments = (int)integer();
    for (int k = 0; k < 6; k++) b->ivs[k] = (int)integer();
    b->nature = (int)integer(); b->ability = (int)integer(); b->passive = (int)integer(); b->shiny = (int)integer(); b->variant = (int)integer();
    b->gender = (int)integer(); b->friendship = (int)integer(); b->id = integer();
    b->hp = (int)integer();
    integer();   /* max HP: stats[0] */
    for (int k = 0; k < 6; k++) b->stats[k] = (int)integer();
    b->n_moves = (int)integer();
    for (int k = 0; k < b->n_moves && k < 4; k++) b->moves[k] = (int)integer();
}

/* the player's list against upstream's, as a set: upstream sorts the list by the holders' ids, which are random there */
static void compare_items(const char *what, const item_row *want, int n_want, const int *st)
{
    (void)st;
    bool used[GC_MAX_ITEMS] = {false};
    EQ(what, run.n_items, n_want);
    for (int k = 0; k < n_want; k++) {
        const uint32_t holder = want[k].holder < 0 ? 0 : (uint32_t)(0x4000 + want[k].holder);
        bool found = false;
        for (int j = 0; j < run.n_items && !found; j++) {
            const gc_item *it = &run.items[j];
            if (used[j]) continue;
            if (!strcmp(it->key, want[k].key) && it->arg == want[k].arg && it->pokemon_id == holder && it->stack == want[k].stack) { used[j] = found = true; }
        }
        CHECK(found, "%s: %s/%d held by %d x%d is missing", what, want[k].key, want[k].arg, want[k].holder, want[k].stack);
    }
}

int main(int argc, char **argv)
{
    if (argc != 2 || !(f = fopen(argv[1], "r"))) { fputs("usage: mystery_avarice_diff_test <trace>\n", stderr); return 2; }
    tag('V');
    integer();
    integer();
    int n_mons = 0;
    int cases = 0, mismatches = 0, skipped = 0, by_option[3] = {0}, items_checked = 0, moves_checked = 0, caught_checked = 0, berries_back = 0, seeds = 0, stages_checked = 0, capped = 0;
    char c;
    while (fscanf(f, " %c", &c) == 1) {
        if (c == 'Z') break;
        if (c == 'P') {
            n_mons = (int)integer();
            if (n_mons < 1 || n_mons > 6) { fputs("party size\n", stderr); return 2; }
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
        static item_row before_items[120], boss_rows[120], after_items[120], visual_items[120], final_items[120];
        const int n_before = read_items(before_items, 1);
        tag('A');
        const int meets[3] = {(int)integer(), (int)integer(), (int)integer()};
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
        struct { int species; double level_add; int boss, segments, shiny_free, n_moves, moves[4], n_tags; char tag_name[64]; int n_mods, arg[160], stack[160], disable_switch, has_effects; } init;
        memset(&init, 0, sizeof init);
        init.species = (int)integer(); init.level_add = real(); init.boss = (int)integer(); init.segments = (int)integer(); init.shiny_free = (int)integer();
        init.n_moves = (int)integer();
        for (int k = 0; k < init.n_moves && k < 4; k++) init.moves[k] = (int)integer();
        init.n_tags = (int)integer();
        if (init.n_tags > 0) word(init.tag_name, sizeof init.tag_name);
        init.n_mods = (int)integer();
        for (int k = 0; k < init.n_mods && k < 160; k++) { init.arg[k] = (int)integer(); init.stack[k] = (int)integer(); }
        init.disable_switch = (int)integer(); init.has_effects = (int)integer();
        tag('M');
        int stolen_n[6], stolen_type[6][40], stolen_stack[6][40];
        for (int m = 0; m < n_mons; m++) {
            if (m) semicolon();
            stolen_n[m] = (int)integer();
            for (int k = 0; k < stolen_n[m] && k < 40; k++) { stolen_type[m][k] = (int)integer(); stolen_stack[m][k] = (int)integer(); }
        }
        (void)stolen_type;
        tag('G');
        char up_name[64];
        word(up_name, sizeof up_name);
        tag('T');
        const int n_visual = read_items(visual_items, 1);
        tag('Q');
        (void)rng_vals();
        const pr_rng after_option = rng_vals();
        tag('S');
        const int n_rewards = (int)integer(), fill = (int)integer(), n_funcs = (int)integer(), n_opts = (int)integer(), n_tiers = (int)integer(), has_cb = (int)integer();
        const int leave_calls = (int)integer(), leave_heal = (int)integer();
        rec("X2");
        const int n_said = (int)integer();
        for (int k = 0; k < n_said; k++) { char tmp[96]; word(tmp, sizeof tmp); }
        tag('Y');
        const int n_caught = (int)integer();
        boss_row pre, post;
        memset(&pre, 0, sizeof pre);
        memset(&post, 0, sizeof post);
        int c_ball_null = 0, c_type = 0, c_show = 0, c_obtain = 0, c_added = 0, c_pokeball = 0;
        for (int k = 0; k < n_caught; k++) {
            if (k) semicolon();
            read_boss(&pre);
            semicolon();
            c_ball_null = (int)integer(); c_type = (int)integer(); c_show = (int)integer(); c_obtain = (int)integer(); c_added = (int)integer();
            if (c_added) { c_pokeball = (int)integer(); read_boss(&post); }
        }
        tag('V');
        const int n_after = read_items(after_items, 1);
        boss_row up;
        memset(&up, 0, sizeof up);
        int st_effect = 0, n_boss_items = 0, n_effects = 0, dbl = 0, n_post = 0, mult = 1, boss_index = 0, n_levels = 0, levels[2] = {0, 0}, n_cb_said = 0, n_final = 0, ai = 0;
        char post_fx[4][96], cb_said[4][96], tags[96] = "", food[64] = "";
        int fx[4][5];
        if (option == 0) {
            tag('D');
            integer();
            dbl = (int)integer();
            n_levels = (int)integer();
            for (int k = 0; k < n_levels; k++) { const int l = (int)integer(); if (k < 2) levels[k] = l; }
            const int n_queued = (int)integer();
            { char tmp[64]; for (int k = 0; k < n_queued; k++) word(tmp, sizeof tmp); }
            tag('X'); read_boss(&up);
            tag('N'); st_effect = (int)integer(); ai = (int)integer(); word(tags, sizeof tags); integer();
            tag('J'); n_boss_items = (int)integer();
            for (int k = 0; k < n_boss_items; k++) { word(boss_rows[k].key, sizeof boss_rows[k].key); boss_rows[k].arg = (int)integer(); boss_rows[k].stack = (int)integer(); }
            tag('F'); n_effects = (int)integer();
            for (int k = 0; k < n_effects && k < 4; k++) {
                if (k) semicolon();
                for (int j = 0; j < 5; j++) fx[k][j] = (int)integer();
            }
            rec("Tk"); word(food, sizeof food);
            rec("Cb"); n_cb_said = (int)integer();
            for (int k = 0; k < n_cb_said && k < 4; k++) word(cb_said[k], sizeof cb_said[k]);
            rec("Va"); n_final = read_items(final_items, 1);
            tag('K'); (void)rng_vals(); (void)rng_vals(); n_post = (int)integer();
            for (int k = 0; k < n_post && k < 4; k++) word(post_fx[k], sizeof post_fx[k]);
            tag('Z'); mult = (int)integer(); boss_index = (int)integer();
        }
        tag('E');

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
        run.mystery_type = GC_ME_ABSOLUTE_AVARICE;
        memset(&run.screen, 0, sizeof run.screen);
        run.screen.kind = GC_SCREEN_MYSTERY;
        run.screen.mystery_type = GC_ME_ABSOLUTE_AVARICE;
        { gc_event e; while (gc_run_next_event(&run, &e)) {} }
        gc_mystery_setup_screen(&run);
        CHECK(run.screen.n_options == 3, "%d options", run.screen.n_options);
        for (int o = 0; o < 3; o++) CHECK(run.screen.option_ok[o] == (meets[o] != 0), "option %d enabled %d, upstream %d", o, run.screen.option_ok[o], meets[o]);
        for (int o = 0; o < 3; o++) EQ("no pick", run.screen.option_pick[o], 0);
        CHECK(same_rng(&run.run.rng, &after_init), "onInit moved the run RNG (c %.0f, upstream %.0f)", run.run.rng.c, after_init.c);
        EQ("greedentName token", !strcmp(up_name, "Greedent"), 1);
        {
            bool token = false;
            for (int a = 0; a < run.screen.n_me_args; a++) if (run.screen.me_args[a].name && !strcmp(run.screen.me_args[a].name, "greedentName")) token = true;
            EQ("greedentName token on the screen", token, 1);
        }

        /* onInit's config */
        EQ("onInit species", init.species, SPECIES_ID_GREEDENT);
        EQ("onInit is a boss", init.boss, 1);
        EQ("onInit bars", init.segments, 3);
        EQ("onInit shiny lock", init.shiny_free, 0);
        EQ("onInit level modifier", (int)init.level_add, 1);
        EQ("onInit move count", init.n_moves, 4);
        { static const int want[4] = {MOVE_ID_THRASH, MOVE_ID_CRUNCH, MOVE_ID_BODY_PRESS, MOVE_ID_SLACK_OFF};
          for (int k = 0; k < 4; k++) EQ("onInit move", want[k], init.moves[k]); }
        EQ("onInit tag count", init.n_tags, 1);
        CHECK(!strcmp(init.tag_name, "MYSTERY_ENCOUNTER_POST_SUMMON"), "onInit tag %s", init.tag_name);
        EQ("onInit switching stays on", init.disable_switch, 0);
        EQ("onInit post-summon effect", init.has_effects, 1);
        {   /* one config per stack of every Berry: the sum of the stacks stolen */
            int stolen_total = 0, config_total = 0;
            for (int m = 0; m < n_mons; m++) for (int k = 0; k < stolen_n[m] && k < 40; k++) stolen_total += stolen_stack[m][k];
            for (int k = 0; k < init.n_mods && k < 160; k++) { config_total += init.stack[k]; EQ("onInit item stack", init.stack[k], 1); }
            EQ("onInit item count is the stacks stolen", config_total, stolen_total);
        }

        { gc_event e; while (gc_run_next_event(&run, &e)) {} }
        bool ok = false;
        const char *w2 = "";
        gc_mystery_execute(&run, option, 0, 0, &w2, &ok);
        CHECK(ok, "option refused: %s", w2);
        CHECK(same_rng(&run.run.rng, &after_option), "the run RNG after the option differs (c %.0f, upstream %.0f)", run.run.rng.c, after_option.c);
        if (option == 0) EQ("the visuals leave the party Berry-free", n_visual, n_after);   /* the fight adds nothing before its callback */
        {   /* onVisualsStart: no Berry is left, and the other items stay as they were */
            for (int k = 0; k < run.n_items; k++) if (!strcmp(run.items[k].key, "BERRY") && option == 0) CHECK(0, "a Berry is left in the party");
            int expect_other = 0;
            for (int k = 0; k < n_visual; k++) expect_other += strcmp(visual_items[k].key, "BERRY") != 0;
            int kept = 0;
            for (int k = 0; k < n_before; k++) kept += strcmp(before_items[k].key, "BERRY") != 0;
            EQ("items after the visuals are the non-Berry items", n_visual, kept);
            (void)expect_other;
        }
        EQ("rewards set", n_rewards, option == 0 ? 1 : 0);
        EQ("rewards no options", n_opts, 0);
        EQ("rewards no tiers", n_tiers, 0);
        EQ("callback given", has_cb, option == 0 ? 1 : 0);
        EQ("no catching", run.me_catch_allowed, 0);
        int c_said = 0, n_enraged = 0;
        { gc_event e;
          while (gc_run_next_event(&run, &e)) {
              if (e.kind == GC_EV_MESSAGE && e.msg.key && !strncmp(e.msg.key, "mysteryEncounters/absoluteAvarice:", 34)) c_said++;
              if (e.kind != GC_EV_BATTLE) continue;
              for (int j = e.a; j < e.a + e.b; j++) {
                  const pr_event *pe = &run.battle_events[j];
                  if (pe->type != PR_EV_TEXT || !pe->msg.key || strcmp(pe->msg.key, "mysteryEncounters/absoluteAvarice:option.1.bossEnraged")) continue;
                  n_enraged++;
                  bool token = false;
                  for (int a = 0; a < pe->msg.n_args; a++) if (pe->msg.args[a].name && !strcmp(pe->msg.args[a].name, "greedentName")) token = true;
                  EQ("enraged message has its token", token, 1);
              }
          } }
        EQ("the option itself says nothing", c_said, 0);
        EQ("enraged message count", n_enraged, option == 0 ? 1 : 0);

        if (option == 0) {
            CHECK(run.n_enemy == 1 && run.in_battle && run.me_battle, "no battle (%d enemies)", run.n_enemy);
            EQ("double", run.double_battle, dbl);
            EQ("leave calls", leave_calls, 0);
            EQ("one funcless shop", n_funcs, 0);
            EQ("rewards set", gc_int(&run)->me_reward.active, 1);
            EQ("rewards fill", gc_int(&run)->me_reward.fill, n_rewards == 1 && fill);
            EQ("reward is no item", gc_int(&run)->me_reward.has_item, 0);
            EQ("reward function", gc_int(&run)->me_reward.func, -1);
            EQ("reward callback before the shop", gc_int(&run)->me_reward.before, 1);
            EQ("fleeing is off", gc_int(&run)->me_no_flee, 1);
            EQ("switching stays on", gc_int(&run)->me_no_switch, 0);
            EQ("enemy levels", n_levels, 1);
            const gc_mon *e = &run.enemy[0];
            EQ("boss species", e->b.species, up.species);
            EQ("boss form", e->b.form, up.form);
            EQ("boss level", e->b.level, up.level);
            EQ("boss level is the battle's", e->b.level, levels[0]);
            EQ("boss segments", e->b.boss_segments, up.segments);
            EQ("boss segment index", e->b.boss_segment_index, boss_index);
            for (int k = 0; k < 6; k++) { char n[32]; snprintf(n, sizeof n, "boss iv %d", k); EQ(n, e->b.ivs[k], up.ivs[k]); }
            EQ("boss nature", e->has_custom_nature ? e->base_nature : e->b.nature, up.nature);
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
            (void)ai;
            CHECK(!strcmp(tags, "MYSTERY_ENCOUNTER_POST_SUMMON"), "upstream's boss tags are %s", tags);
            EQ("boss item count", run.n_enemy_items, n_boss_items);
            for (int k = 0; k < run.n_enemy_items && k < n_boss_items; k++) {
                const gc_item *it = &run.enemy_items[k];
                CHECK(!strcmp(it->key, boss_rows[k].key) && it->arg == boss_rows[k].arg && it->stack == boss_rows[k].stack && it->pokemon_id == e->b.id,
                      "boss item %d is %s/%d x%d, upstream %s/%d x%d", k, it->key, it->arg, it->stack, boss_rows[k].key, boss_rows[k].arg, boss_rows[k].stack);
                items_checked++;
                capped += boss_rows[k].stack == (boss_rows[k].arg == 0 || boss_rows[k].arg == 1 || boss_rows[k].arg == 2 || boss_rows[k].arg == 10 ? 2 : 3);
            }
            compare_items("party items during the fight", visual_items, n_visual, st);
            /* startOfBattleEffects: Stuff Cheeks from the Greedent at itself, ignoring PP */
            EQ("start-of-battle effects", gc_int(&run)->n_me_start, n_effects);
            for (int k = 0; k < n_effects && k < gc_int(&run)->n_me_start && k < 4; k++) {
                EQ("effect source (enemy)", gc_int(&run)->me_start[k].source, fx[k][0]);
                EQ("effect target count", fx[k][1], 1);
                EQ("effect target", gc_int(&run)->me_start[k].target, fx[k][2]);
                EQ("effect move", gc_int(&run)->me_start[k].move, fx[k][3]);
                EQ("effect use mode", gc_int(&run)->me_start[k].use_mode, fx[k][4]);
            }
            /* the post-summon effect: its message, then Sp. Def (and from wave 50 Speed) up one stage; Simple doubles it, Contrary reverses it */
            EQ("post-summon effects", n_post, 2);
            CHECK(n_post == 2 && !strcmp(post_fx[0], "MSG_mysteryEncounters/absoluteAvarice:option.1.bossEnraged"), "upstream's post-summon message differs");
            CHECK(n_post == 2 && !strcmp(post_fx[1], wave < 50 ? "StatStageChangePhase_4_1" : "StatStageChangePhase_4_1_5_1"), "upstream's post-summon stat change is %s", n_post == 2 ? post_fx[1] : "-");
            EQ("post-summon flag spent", e->b.me_post_summon, PR_ME_POST_NONE);
            EQ("Sp. Def stage", e->b.stat_stages[STAT_SPDEF - 1], mult);
            EQ("Speed stage", e->b.stat_stages[STAT_SPD - 1], wave < 50 ? 0 : mult);
            EQ("Atk stage", e->b.stat_stages[STAT_ATK - 1], 0);
            EQ("Def stage", e->b.stat_stages[STAT_DEF - 1], 0);
            EQ("Sp. Atk stage", e->b.stat_stages[STAT_SPATK - 1], 0);
            stages_checked += 5;
            /* the rewards callback: a Reviver Seed for everyone who holds none, then foodStash */
            gc_avarice_reward(&run);
            { gc_event ev; int n_stash = 0;
              while (gc_run_next_event(&run, &ev))
                  if (ev.kind == GC_EV_MESSAGE && ev.msg.key && !strcmp(ev.msg.key, "mysteryEncounters/absoluteAvarice:option.1.foodStash")) {
                      n_stash++;
                      bool name = false, reward = false;
                      for (int a = 0; a < ev.msg.n_args; a++) {
                          if (ev.msg.args[a].name && !strcmp(ev.msg.args[a].name, "greedentName")) name = true;
                          if (ev.msg.args[a].name && !strcmp(ev.msg.args[a].name, "foodReward")) reward = ev.msg.args[a].s && !strcmp(ev.msg.args[a].s, "modifierType:ModifierType.REVIVER_SEED.name");
                      }
                      EQ("foodStash has the Greedent's name", name, 1);
                      EQ("foodStash names the Reviver Seed", reward, 1);
                  }
              EQ("foodStash said once", n_stash, n_cb_said); }
            CHECK(n_cb_said == 1 && !strcmp(cb_said[0], "mysteryEncounters/absoluteAvarice:option.1.foodStash"), "upstream's callback says something else");
            CHECK(!strcmp(food, "Reviver_Seed"), "upstream's foodReward token is %s", food);
            compare_items("party items after the callback", final_items, n_final, st);
            for (int k = 0; k < n_final; k++) seeds += !strcmp(final_items[k].key, "REVIVER_SEED");
            gc_avarice_reward(&run);
            { gc_event ev; int again = 0;
              while (gc_run_next_event(&run, &ev)) again += ev.kind == GC_EV_MESSAGE;
              EQ("a second callback says nothing", again, 0); }
            EQ("a second callback gives nothing", run.n_items, n_final);
        } else {
            EQ("leave calls", leave_calls, 1);
            EQ("leaves with the heal shop", leave_heal, 1);
            int healshop = 0, shop = 0, catches = 0;
            for (gc_rphase p; gc_rq_next(&gc_int(&run)->rq, &p);) {
                if (p.type == RP_SELECT_MODIFIER) { healshop += p.a == 1; shop += p.a == 2; }
                if (p.type == RP_CATCH) {
                    catches++;
                    EQ("the catch is of the first enemy", p.a, 0);
                    EQ("the catch says no text", p.c, 2);
                    const gc_mon *e = &run.enemy[p.a];
                    EQ("caught species", e->b.species, pre.species);
                    EQ("caught form", e->b.form, pre.form);
                    EQ("caught level", e->b.level, pre.level);
                    for (int k = 0; k < 6; k++) { char nm[32]; snprintf(nm, sizeof nm, "caught iv %d", k); EQ(nm, e->b.ivs[k], pre.ivs[k]); }
                    EQ("caught nature", e->has_custom_nature ? e->base_nature : e->b.nature, pre.nature);
                    EQ("caught ability index", e->ability_index, pre.ability);
                    EQ("caught passive", e->b.passive_flag, pre.passive);
                    EQ("caught shiny", e->b.shiny, pre.shiny);
                    EQ("caught gender", e->b.gender, pre.gender);
                    EQ("caught friendship", e->b.friendship, pre.friendship);
                    EQ("caught id", e->b.id, pre.id);
                    EQ("caught max hp", e->b.stats[0], pre.stats[0]);
                    EQ("caught hp", e->b.hp, pre.hp);
                    for (int k = 0; k < 6; k++) { char nm[32]; snprintf(nm, sizeof nm, "caught stat %d", k); EQ(nm, e->b.stats[k], pre.stats[k]); }
                    EQ("caught move count", e->b.n_moves, pre.n_moves);
                    for (int k = 0; k < pre.n_moves && k < 4; k++) { char nm[32]; snprintf(nm, sizeof nm, "caught move %d", k); EQ(nm, e->b.moves[k], pre.moves[k]); moves_checked++; }
                    const bool done = gc_catch_start(&run, &p);
                    CHECK(done, "the catch asked a question with %d in the party", n_mons);
                }
            }
            EQ("the heal shop", healshop, 1);
            EQ("no item shop", shop, 0);
            EQ("no reward function", n_funcs, 0);
            EQ("no battle", run.in_battle, 0);
            compare_items("party items afterwards", after_items, n_after, st);
            if (option == 1) {
                EQ("no catch", catches, 0);
                EQ("catchPokemon not called", n_caught, 0);
                for (int k = 0; k < n_after; k++) berries_back += !strcmp(after_items[k].key, "BERRY");
            } else {
                EQ("the Greedent is caught", catches, 1);
                EQ("catchPokemon called once", n_caught, 1);
                EQ("no ball thrown", c_ball_null, 1);
                EQ("a Pokeball", c_type, 0);
                EQ("no catch text", c_show, 0);
                EQ("not an obtain", c_obtain, 0);
                EQ("the party had room", c_added, 1);
                EQ("party grew", run.n_party, n_mons + 1);
                if (run.n_party == n_mons + 1 && c_added) {
                    const gc_mon *g = &run.party[run.party_order[n_mons]];
                    caught_checked++;
                    EQ("joined species", g->b.species, post.species);
                    EQ("joined form", g->b.form, post.form);
                    EQ("joined level", g->b.level, post.level);
                    EQ("joined is no boss", g->b.is_boss, post.segments ? 1 : 0);
                    for (int k = 0; k < 6; k++) { char nm[32]; snprintf(nm, sizeof nm, "joined iv %d", k); EQ(nm, g->b.ivs[k], post.ivs[k]); }
                    EQ("joined nature", g->has_custom_nature ? g->base_nature : g->b.nature, post.nature);
                    EQ("joined ability index", g->ability_index, post.ability);
                    EQ("joined passive", g->b.passive_flag, post.passive);
                    EQ("joined shiny", g->b.shiny, post.shiny);
                    EQ("joined variant", g->variant, post.variant);
                    EQ("joined gender", g->b.gender, post.gender);
                    EQ("joined friendship", g->b.friendship, post.friendship);
                    EQ("joined id", g->b.id, post.id);
                    EQ("joined ball", g->pokeball, c_pokeball);
                    EQ("joined max hp", g->b.stats[0], post.stats[0]);
                    EQ("joined hp", g->b.hp, post.hp);
                    for (int k = 0; k < 6; k++) { char nm[32]; snprintf(nm, sizeof nm, "joined stat %d", k); EQ(nm, g->b.stats[k], post.stats[k]); }
                    EQ("joined move count", g->b.n_moves, post.n_moves);
                    for (int k = 0; k < post.n_moves && k < 4; k++) { char nm[32]; snprintf(nm, sizeof nm, "joined move %d", k); EQ(nm, g->b.moves[k], post.moves[k]); moves_checked++; }
                    EQ("joined is not on the field", g->b.on_field, 0);
                }
            }
        }
        for (int m = 0; m < n_mons; m++) {
            const gc_mon *mon = &run.party[st[m]];
            char nm[48];
            snprintf(nm, sizeof nm, "party %d hp", m); EQ(nm, mon->b.hp, hps[m]);
        }
        if (bad) mismatches++;
        cases++; by_option[option]++;
    }
    printf("Mystery encounter Absolute Avarice: %d cases run (fight %d, plea %d, feast %d), %d skipped (the feast with a full party, which asks what to release: host-tested), %d Berry items back from the plea, %d Reviver Seeds after the fight, "
           "%d boss items compared (%d at their stack's maximum), %d stat stages, %d caught Pokemon compared, %d moves compared, %d fields. TOTAL mismatches %d\n",
           cases, by_option[0], by_option[1], by_option[2], skipped, berries_back, seeds, items_checked, capped, stages_checked, caught_checked, moves_checked, fields_checked, mismatches);
    return mismatches ? 1 : 0;
}
