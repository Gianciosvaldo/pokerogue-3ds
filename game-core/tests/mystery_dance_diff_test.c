/* Dancing Lessons against upstream at 8555c08: replays the "-dance" trace of tests/mystery-dance.producer.test.ts (the real encounter: onInit in executeWithSeedOffset(.., waveIndex),
 * the party screens as a plan, onPreOptionPhase in executeWithSeedOffset(.., getSeedOffset()), onOptionPhase in executeWithSeedOffset(.., getSeedOffset() * 500),
 * initBattleWithEnemyConfig really run for the fight, catchPokemon recorded and completed with the real Pokemon.addToParty, the Pokemon's post-summon effect called by hand)
 * through game-core. Usage: mystery_dance_diff_test <trace>; exit 0 only with 0 mismatches. */
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




static void rec(const char *want)
{
    char w[16];
    word(w, sizeof w);
    if (strcmp(w, want)) { fprintf(stderr, "wrong record: wanted %s, got %s\n", want, w); exit(2); }
}


int main(int argc, char **argv)
{
    if (argc != 2 || !(f = fopen(argv[1], "r"))) { fputs("usage: mystery_dance_diff_test <trace>\n", stderr); return 2; }
    tag('V');
    integer();
    integer();
    int n_mons = 0;
    int cases = 0, mismatches = 0, skipped = 0, by_option[3] = {0}, items_checked = 0, moves_checked = 0, enemy_checked = 0, caught_checked = 0, stages_checked = 0, picks_checked = 0;
    int forms[4] = {0, 0, 0, 0};
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
        const double money_before = (double)integer();
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
        tag('B');
        int balls[5];
        for (int k = 0; k < 5; k++) balls[k] = (int)integer();
        tag('L');
        static item_row before_items[120], boss_rows[120], after_items[120];
        const int n_before = read_items(before_items, 1);
        tag('A');
        const int meets[3] = {(int)integer(), (int)integer(), (int)integer()};
        const int up_primary3 = (int)integer(), up_token_move = (int)integer();
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
        const int init_msgs = (int)integer();
        char up_name[64];
        word(up_name, sizeof up_name);
        rec("X0");
        boss_row first;
        read_boss(&first, true);
        tag('S');
        const int up_member = (int)integer(), up_slot = (int)integer(), up_move = (int)integer(), up_picked = (int)integer();
        tag('Q');
        (void)rng_vals();
        const pr_rng after_option = rng_vals();
        tag('T');
        const int n_rewards = (int)integer(), fill = (int)integer(), n_funcs = (int)integer(), n_given = (int)integer(), n_tiers = (int)integer();
        const int leave_calls = (int)integer(), leave_heal = (int)integer(), has_cb = (int)integer(), baton = (int)integer();
        rec("X2");
        const int n_exp = (int)integer();
        for (int k = 0; k < n_exp; k++) { if (k) semicolon(); (void)integer(); (void)real(); (void)integer(); }
        tag('M');
        const int n_said = (int)integer();
        for (int k = 0; k < n_said; k++) { char tmp[96]; word(tmp, sizeof tmp); }
        tag('O');
        (void)integer();
        const double money_after = (double)integer();
        tag('G');
        const int n_caught = (int)integer();
        boss_row pre, post;
        int c_ball_null = 0, c_type = 0, c_show = 0, c_obtain = 0, c_added = 0, c_pokeball = 0;
        for (int k = 0; k < n_caught; k++) {
            if (k) semicolon();
            read_boss(&pre, true);
            semicolon();
            c_ball_null = (int)integer(); c_type = (int)integer(); c_show = (int)integer(); c_obtain = (int)integer(); c_added = (int)integer();
            if (c_added) { c_pokeball = (int)integer(); read_boss(&post, true); }
        }
        tag('V');
        const int n_after = read_items(after_items, 1);
        tag('H');
        const int n_queued = (int)integer();
        char queued[4][64];
        for (int k = 0; k < n_queued; k++) { char tmp[64]; word(tmp, sizeof tmp); if (k < 4) snprintf(queued[k], 64, "%s", tmp); }
        boss_row up;
        memset(&up, 0, sizeof up);
        int st_effect = 0, post_flag = 0, n_effects = 0, dbl = 0, n_levels = 0, levels[2] = {0, 0}, n_boss_items = 0, n_post = 0, n_post_said = 0, enc_mode = 0;
        char post_effect[96] = "", post_said[96] = "";
        int fx[4][5];
        if (option == 0) {
            tag('D');
            enc_mode = (int)integer();
            dbl = (int)integer();
            n_levels = (int)integer();
            for (int k = 0; k < n_levels; k++) { const int l = (int)integer(); if (k < 2) levels[k] = l; }
            const int nq = (int)integer();
            { char tmp[64]; for (int k = 0; k < nq; k++) word(tmp, sizeof tmp); }
            tag('F'); n_effects = (int)integer();
            for (int k = 0; k < n_effects && k < 4; k++) {
                if (k) semicolon();
                for (int j = 0; j < 5; j++) fx[k][j] = (int)integer();
            }
            tag('J');
            n_boss_items = (int)integer();
            for (int k = 0; k < n_boss_items; k++) {
                (void)integer();
                word(boss_rows[k].key, sizeof boss_rows[k].key);
                boss_rows[k].arg = (int)integer(); boss_rows[k].stack = (int)integer();
            }
            tag('X'); read_boss(&up, true);
            tag('N'); st_effect = (int)integer(); (void)integer(); post_flag = (int)integer(); (void)integer();
            tag('K'); (void)rng_vals(); (void)rng_vals(); n_post = (int)integer();
            for (int k = 0; k < n_post && k < 1; k++) word(post_effect, sizeof post_effect);
            n_post_said = (int)integer();
            for (int k = 0; k < n_post_said && k < 1; k++) word(post_said, sizeof post_said);
        }
        tag('E');
        (void)enc_mode;

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
            mon->b.custom_ability = (int16_t)setup[m].ability;
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
            mon->b.hp = setup[m].hp;
        }
        run.trainer_id = trainer_id;
        run.secret_id = secret_id;
        run.money = money_before;
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
        run.mystery_type = GC_ME_DANCING_LESSONS;
        memset(&run.screen, 0, sizeof run.screen);
        run.screen.kind = GC_SCREEN_MYSTERY;
        run.screen.mystery_type = GC_ME_DANCING_LESSONS;
        { gc_event e; while (gc_run_next_event(&run, &e)) {} }
        gc_mystery_setup_screen(&run);
        CHECK(run.screen.n_options == 3, "%d options", run.screen.n_options);
        for (int o = 0; o < 3; o++) CHECK(run.screen.option_ok[o] == (meets[o] != 0), "option %d enabled %d, upstream %d", o, run.screen.option_ok[o], meets[o]);
        EQ("first option takes no pick", run.screen.option_pick[0], 0);
        EQ("second option picks a member", run.screen.option_pick[1], GC_PICK_MEMBER);
        EQ("third option picks a member and a move", run.screen.option_pick[2], GC_PICK_MEMBER_MOVE);
        CHECK(same_rng(&run.run.rng, &after_init), "onInit moved the run RNG (c %.0f, upstream %.0f)", run.run.rng.c, after_init.c);
        EQ("onInit messages", init_msgs, 0);
        if (meets[2]) {
            const int p = gc_mystery_primary_pokemon(&run, GC_ME_DANCING_LESSONS, 2);
            EQ("third option's primary Pokemon", p >= 0 ? (int)(run.party[p].b.id - 0x4000) : -1, up_primary3);
        }
        {
            /* the dialogue tokens: oricorioName, and option3PrimaryName / option3PrimaryMove when the third option is on */
            const gc_screen *sc = &run.screen;
            bool name_token = false, primary_token = false, move_token = false;
            int primary_species = -1;
            for (int a = 0; a < sc->n_me_args; a++) {
                if (!sc->me_args[a].name) continue;
                if (!strcmp(sc->me_args[a].name, "oricorioName")) name_token = true;
                if (!strcmp(sc->me_args[a].name, "option3PrimaryName")) { primary_token = true; primary_species = (int)(sc->me_args[a].v >> 16); }
                if (!strcmp(sc->me_args[a].name, "option3PrimaryMove")) move_token = true;
            }
            EQ("oricorioName token", name_token, 1);
            EQ("third option's primary token", primary_token, meets[2] != 0);
            EQ("third option's move token", move_token, meets[2] != 0);
            if (meets[2] && primary_token) EQ("third option's primary token names", primary_species, setup[up_primary3].species);
            for (int a = 0; a < sc->n_me_args; a++)
                if (sc->me_args[a].name && !strcmp(sc->me_args[a].name, "option3PrimaryMove")) EQ("third option's move token names", (int)sc->me_args[a].v, up_token_move);
        }

        /* onInit's Oricorio */
        {
            gc_mon rolled;
            if (!gc_dance_roll(&run, &rolled)) { CHECK(0, "no Oricorio rolled (the biome or species is not ported)"); }
            else {
                const gc_mon *e = &rolled;
                enemy_checked++;
                EQ("rolled species", e->b.species, first.species);
                EQ("rolled form", e->b.form, first.form);
                if (e->b.form < 4) forms[e->b.form]++;
                EQ("rolled level", e->b.level, first.level);
                EQ("rolled bars", e->b.boss_segments, first.segments);
                for (int k = 0; k < 6; k++) { char nm[32]; snprintf(nm, sizeof nm, "rolled iv %d", k); EQ(nm, e->b.ivs[k], first.ivs[k]); }
                EQ("rolled nature", e->has_custom_nature ? e->base_nature : e->b.nature, first.nature);
                EQ("rolled ability index", e->ability_index, first.ability);
                EQ("rolled passive flag", e->b.passive_flag, first.passive);
                EQ("rolled shiny", e->b.shiny, first.shiny);
                EQ("rolled variant", e->variant, first.variant);
                EQ("rolled gender", e->b.gender, first.gender);
                EQ("rolled friendship", e->b.friendship, first.friendship);
                EQ("rolled id", e->b.id, first.id);
                EQ("rolled max hp", e->b.stats[0], first.stats[0]);
                EQ("rolled hp", e->b.hp, first.hp);
                for (int k = 0; k < 6; k++) { char nm[32]; snprintf(nm, sizeof nm, "rolled stat %d", k); EQ(nm, e->b.stats[k], first.stats[k]); }
                EQ("rolled move count", e->b.n_moves, first.n_moves);
                for (int k = 0; k < first.n_moves && k < 4; k++) { char nm[32]; snprintf(nm, sizeof nm, "rolled move %d", k); EQ(nm, e->b.moves[k], first.moves[k]); moves_checked++; }
                bool dance = false;
                for (int k = 0; k < e->b.n_moves; k++) dance = dance || e->b.moves[k] == MOVE_ID_REVELATION_DANCE;
                CHECK(dance, "Revelation Dance is not in the moveset");
            }
        }

        /* the party screens' picks, as the plan made them */
        int member = -1;
        if (option > 0) {
            member = up_member;
            EQ("a pick was made", up_picked, 1);
            CHECK(member >= 0 && member < n_mons, "member %d", member);
            if (member >= 0 && member < n_mons) {
                CHECK(gc_mystery_member_ok(&run, option, st[member]), "the party screen refuses member %d for option %d", member, option);
                picks_checked++;
                if (option == 2) {
                    CHECK(gc_mystery_move_ok(&run, option, st[member], up_slot), "the move list refuses slot %d (move %d)", up_slot, up_move);
                    EQ("the picked slot holds the picked move", run.party[st[member]].b.moves[up_slot], up_move);
                    picks_checked++;
                }
            }
        } else EQ("no pick for the battle", up_picked, -1);

        bool ok = false;
        const char *w2 = "";
        gc_mystery_execute(&run, option, option == 0 ? 0 : st[member], option == 2 ? up_slot : 0, &w2, &ok);
        CHECK(ok, "option refused: %s", w2);
        CHECK(same_rng(&run.run.rng, &after_option), "the run RNG after the option differs (c %.0f, upstream %.0f)", run.run.rng.c, after_option.c);
        EQ("money unchanged", (long long)run.money, (long long)money_after);
        {
            /* the encounter's own text comes only from the battle: the Oricorio's post-summon message, with the dialogue token oricorioName */
            int n_enraged = 0, n_other = 0;
            gc_event e;
            while (gc_run_next_event(&run, &e)) {
                if (e.kind == GC_EV_BATTLE) {
                    for (int j = e.a; j < e.a + e.b; j++) {
                        const pr_event *pe = &run.battle_events[j];
                        if (pe->type != PR_EV_TEXT || !pe->msg.key || strcmp(pe->msg.key, "mysteryEncounters/dancingLessons:option.1.bossEnraged")) continue;
                        n_enraged++;
                        bool token = false;
                        for (int a = 0; a < pe->msg.n_args; a++) if (pe->msg.args[a].name && !strcmp(pe->msg.args[a].name, "oricorioName")) token = true;
                        EQ("enraged message has its token", token, 1);
                    }
                    continue;
                }
                if (e.kind == GC_EV_MESSAGE && e.msg.key && !strncmp(e.msg.key, "mysteryEncounters/dancingLessons:", 33)) n_other++;
            }
            EQ("enraged message count", n_enraged, option == 0 ? n_post_said : 0);
            if (option == 0) CHECK(!strcmp(post_said, "mysteryEncounters/dancingLessons:option.1.bossEnraged"), "upstream says %s", post_said);
            EQ("no other text", n_other, 0);
        }

        /* the shop, the lesson and the catch */
        int heal = 0, shop = 0, catches = 0, learn = 0, learn_before_shop = 0;
        for (gc_rphase p; gc_rq_next(&gc_int(&run)->rq, &p);) {
            if (p.type == RP_SELECT_MODIFIER) { heal += p.a == 1; shop += p.a == 2; }
            if (p.type == RP_LEARN_MOVE) {
                learn++;
                learn_before_shop += heal + shop == 0;
                EQ("the lesson is for the picked Pokemon", p.a, st[member]);
                EQ("the lesson is Revelation Dance", p.b, MOVE_ID_REVELATION_DANCE);
                char want[64];
                snprintf(want, sizeof want, "LearnMovePhase_%d_%d", member, MOVE_ID_REVELATION_DANCE);
                CHECK(n_queued == 1 && !strcmp(queued[0], want), "upstream queued %s, C learns %d for %d", n_queued ? queued[0] : "nothing", p.b, p.a);
            }
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
                EQ("caught id", e->b.id, pre.id);
                EQ("caught move count", e->b.n_moves, pre.n_moves);
                for (int k = 0; k < pre.n_moves && k < 4; k++) { char nm[32]; snprintf(nm, sizeof nm, "caught move %d", k); EQ(nm, e->b.moves[k], pre.moves[k]); moves_checked++; }
                const bool done = gc_catch_start(&run, &p);
                CHECK(done, "the catch asked a question with %d in the party", n_mons);
            }
        }
        const gc_internal *in = gc_int(&run);
        if (option == 0) {
            EQ("rewards set", n_rewards, 1);
            EQ("rewards fill the rest", fill, 1);
            EQ("one reward function", n_funcs, 1);
            EQ("it is the Baton", baton, 1);
            EQ("no reward options", n_given, 0);
            EQ("no reward tiers", n_tiers, 0);
            EQ("no callback", has_cb, 0);
            EQ("leave calls", leave_calls, 0);
            EQ("reward active", in->me_reward.active, 1);
            EQ("reward fill", in->me_reward.fill, 1);
            EQ("reward has no single item", in->me_reward.has_item, 0);
            EQ("reward function is the Baton", in->me_reward.func, gc_reward_item_index("BATON"));
            EQ("reward after the battle", in->me_reward.before, 0);
            EQ("no experience", n_exp, 0);
            EQ("fleeing is off", in->me_no_flee, 1);
            EQ("catching is allowed", run.me_catch_allowed, 1);
            CHECK(run.n_enemy == 1 && run.in_battle && run.me_battle, "no battle (%d enemies)", run.n_enemy);
            EQ("single battle", run.double_battle, 0);
            EQ("single battle upstream", dbl, 0);
            EQ("enemy levels", n_levels, 1);
            EQ("one effect", n_effects, 1);
            EQ("battle effects", in->n_me_start, n_effects);
            for (int k = 0; k < n_effects && k < in->n_me_start && k < 4; k++) {
                EQ("effect source", in->me_start[k].source, fx[k][0]);
                EQ("effect target", in->me_start[k].target, fx[k][2]);
                EQ("effect move", in->me_start[k].move, fx[k][3]);
                EQ("effect use mode", in->me_start[k].use_mode, fx[k][4]);
            }
            EQ("enemy items", run.n_enemy_items, n_boss_items);
            const gc_mon *e = &run.enemy[0];
            enemy_checked++;
            EQ("species", e->b.species, up.species);
            EQ("form", e->b.form, up.form);
            EQ("level", e->b.level, up.level);
            EQ("level is the battle's", e->b.level, levels[0]);
            EQ("boss", e->b.is_boss, up.segments ? 1 : 0);
            EQ("segments", e->b.boss_segments, up.segments);
            for (int k = 0; k < 6; k++) { char nm[32]; snprintf(nm, sizeof nm, "iv %d", k); EQ(nm, e->b.ivs[k], up.ivs[k]); }
            EQ("nature", e->has_custom_nature ? e->base_nature : e->b.nature, up.nature);
            EQ("ability index", e->ability_index, up.ability);
            EQ("passive flag", e->b.passive_flag, up.passive);
            EQ("shiny", e->b.shiny, up.shiny);
            EQ("variant", e->variant, up.variant);
            EQ("gender", e->b.gender, up.gender);
            EQ("friendship", e->b.friendship, up.friendship);
            EQ("id", e->b.id, up.id);
            EQ("max hp", e->b.stats[0], up.stats[0]);
            EQ("hp", e->b.hp, up.hp);
            for (int k = 0; k < 6; k++) { char nm[32]; snprintf(nm, sizeof nm, "stat %d", k); EQ(nm, e->b.stats[k], up.stats[k]); }
            EQ("move count", e->b.n_moves, up.n_moves);
            for (int k = 0; k < up.n_moves && k < 4; k++) { char nm[32]; snprintf(nm, sizeof nm, "move %d", k); EQ(nm, e->b.moves[k], up.moves[k]); moves_checked++; }
            EQ("status", e->b.status, st_effect);
            EQ("post-summon tag upstream", post_flag, 1);
            EQ("post-summon flag spent", e->b.me_post_summon, PR_ME_POST_NONE);
            EQ("post-summon effects upstream", n_post, 1);
            CHECK(!strcmp(post_effect, "StatStageChangePhase_1_1_2_1_3_1_4_1"), "upstream's post-summon effect is %s", post_effect);
            {
                /* Atk, Def, SpAtk and SpDef one stage each, Spd none; Simple doubles the change and Contrary turns it round */
                const int ability = pr_get_ability(&e->b, false);
                const int mult = ability == ABILITY_ID_SIMPLE ? 2 : ability == ABILITY_ID_CONTRARY ? -1 : 1;
                const int want[5] = {1, 1, 1, 1, 0};
                for (int s = 0; s < 5; s++) { char nm[32]; snprintf(nm, sizeof nm, "stage %d", s); EQ(nm, e->b.stat_stages[s], want[s] * mult); stages_checked++; }
            }
            for (int k = 0; k < n_boss_items; k++) {
                bool found = false;
                for (int j = 0; j < run.n_enemy_items; j++) {
                    const gc_item *it = &run.enemy_items[j];
                    found = found || (!strcmp(it->key, boss_rows[k].key) && it->arg == boss_rows[k].arg && it->stack == boss_rows[k].stack && it->pokemon_id == e->b.id);
                }
                CHECK(found, "enemy lacks item %s/%d x%d", boss_rows[k].key, boss_rows[k].arg, boss_rows[k].stack);
                items_checked++;
            }
            EQ("no catch", catches, 0);
            EQ("no lesson", learn, 0);
        } else {
            EQ("no rewards set", n_rewards, 0);
            EQ("no experience", n_exp, 0);
            EQ("leave calls", leave_calls, 1);
            EQ("leaves with the heal shop", leave_heal, 1);
            EQ("the heal shop", heal, 1);
            EQ("no item shop", shop, 0);
            EQ("no reward function", n_funcs, 0);
            EQ("no callback", has_cb, 0);
            EQ("no battle", run.n_enemy <= 1, 1);
            CHECK(!run.in_battle, "a battle started");
            EQ("items unchanged", run.n_items, n_after);
            if (option == 1) {
                EQ("one lesson", learn, 1);
                EQ("the lesson comes before the shop", learn_before_shop, 1);
                EQ("no catch", catches, 0);
                EQ("catchPokemon not called", n_caught, 0);
                EQ("party size unchanged", run.n_party, n_mons);
            } else {
                EQ("no lesson", learn, 0);
                EQ("the Oricorio is caught", catches, 1);
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
                    bool has_shown = false;
                    for (int k = 0; k < g->b.n_moves; k++) has_shown = has_shown || g->b.moves[k] == up_move;
                    CHECK(has_shown, "the shown move %d is not in the joined moveset", up_move);
                    EQ("joined is not on the field", g->b.on_field, 0);
                }
            }
        }
        (void)n_after;
        for (int m = 0; m < n_mons; m++) {
            const gc_mon *mon = &run.party[st[m]];
            char nm[48];
            snprintf(nm, sizeof nm, "party %d hp", m); EQ(nm, mon->b.hp, setup[m].hp);
        }
        if (bad) mismatches++;
        cases++; by_option[option]++;
    }
    printf("Mystery encounter Dancing Lessons: %d cases run (battle %d, lesson %d, show %d), %d skipped (the show without a dance move or with a full party), %d Oricorio rolled (forms %d/%d/%d/%d), "
           "%d picks, %d caught Pokemon compared, %d stage sets, %d items and %d moves compared, %d fields. TOTAL mismatches %d\n",
           cases, by_option[0], by_option[1], by_option[2], skipped, enemy_checked, forms[0], forms[1], forms[2], forms[3], picks_checked, caught_checked, stages_checked, items_checked, moves_checked, fields_checked, mismatches);
    return mismatches ? 1 : 0;
}
