/* The Safari Zone against upstream at 8555c08: replays the "-safari" trace of tests/mystery-safari.producer.test.ts (the real encounter: onInit in executeWithSeedOffset(.., wave),
 * the entry and each of the four actions run in executeWithSeedOffset(.., getSeedOffset() * 500), the throw's rolls one frame later, the animations completed at once) through
 * game-core, driving it the way the app does: the entry on the encounter's screen, then the four actions on the minigame's, catches through RP_CATCH and RP_SAFARI.
 *
 * Per case, compared with what upstream did: the starting stats, which options are offered, the {{numEncounters}} and {{money}} tokens, and then after every step: the run
 * RNG, the Pokemon left, the catch and flee stages, the money and what was paid, whether the minigame goes on, the cursor and the four actions handed back, whether and how
 * the encounter left, the ball's twitch rate and rolls, the texts said (shown at once, and queued), and the Pokemon summoned (species, form, level, shiny, variant, IVs,
 * nature, ability, gender, id, HP, stats, moves, luck, catch rate, Tera type).
 * Usage: mystery_safari_diff_test <trace>; exit 0 only with 0 mismatches. */
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
    if (fscanf(f, " %c", &c) != 1 || c != want) { fprintf(stderr, "wrong record: wanted %c, got %c at byte %ld\n", want, c, ftell(f)); exit(2); }
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

static int bad, shown, case_no, case_wave, step_no;
#define CHECK(cond, ...) do { if (!(cond)) { bad++; if (shown < 14) { shown++; fprintf(stderr, "case %d (wave %d, step %d): ", case_no, case_wave, step_no); fprintf(stderr, __VA_ARGS__); fputc('\n', stderr); } } } while (0)
static int fields_checked;
#define EQ(name, c_val, up_val) do { fields_checked++; CHECK((long long)(c_val) == (long long)(up_val), "%s is %lld, upstream %lld", name, (long long)(c_val), (long long)(up_val)); } while (0)

/* the messages the encounter's code says, as upstream says them: shown at once (showEncounterText, the animation helpers) or queued (queueMessage) */
static const char *const shown_keys[] = {
    "mysteryEncounters/safariZone:safari.eating", "mysteryEncounters/safariZone:safari.busyEating", "mysteryEncounters/safariZone:safari.angry",
    "mysteryEncounters/safariZone:safari.besideItselfAngry", "battle:pokemonBrokeFree", "battle:pokemonFled", "battle:playerFled"};
static const char *const queued_keys[] = {
    "mysteryEncounterMessages:paidMoney", "mysteryEncounters/safariZone:safari.remainingCount", "mysteryEncounters/safariZone:safari.watching"};
#define N_SHOWN ((int)(sizeof shown_keys / sizeof shown_keys[0]))
#define N_QUEUED ((int)(sizeof queued_keys / sizeof queued_keys[0]))

typedef struct { char shown[8][96], queued[8][96]; int n_shown, n_queued; int remaining_arg; int amount_arg; } said_t;

static void collect(said_t *said)
{
    memset(said, 0, sizeof *said);
    said->remaining_arg = said->amount_arg = -1;
    gc_event ev;
    while (gc_run_next_event(&run, &ev)) {
        if (ev.kind != GC_EV_MESSAGE || !ev.msg.key) continue;
        bool is_shown = false, is_queued = false;
        for (int k = 0; k < N_SHOWN; k++) is_shown |= !strcmp(ev.msg.key, shown_keys[k]);
        for (int k = 0; k < N_QUEUED; k++) is_queued |= !strcmp(ev.msg.key, queued_keys[k]);
        if (is_shown && said->n_shown < 8) snprintf(said->shown[said->n_shown++], sizeof said->shown[0], "T:%s", ev.msg.key);
        if (is_queued && said->n_queued < 8) {
            snprintf(said->queued[said->n_queued++], sizeof said->queued[0], "msg:%s", ev.msg.key);
            for (int a = 0; a < ev.msg.n_args; a++) {
                if (ev.msg.args[a].name && !strcmp(ev.msg.args[a].name, "remainingCount")) said->remaining_arg = (int)ev.msg.args[a].v;
                if (ev.msg.args[a].name && !strcmp(ev.msg.args[a].name, "amount")) said->amount_arg = (int)ev.msg.args[a].v;
            }
        }
    }
}

/* the phases that follow an answer, as gc_run_until_screen runs them, up to the heal shop the encounter leaves with (not run: its shop rolls would move the RNG) */
static bool settle(void)
{
    gc_rqueue *q = &gc_int(&run)->rq;
    for (int guard = 0; guard < 64; guard++) {
        gc_rphase p;
        if (!gc_rq_next(q, &p)) return false;
        if (p.type == RP_SELECT_MODIFIER) return true;
        q->current = p;
        if (!gc_phase_start(&run, &q->current)) { q->waiting = true; return false; }
        q->current.type = RP_NONE;
    }
    fputs("the queue did not settle\n", stderr);
    exit(2);
}

static bool answer(const gc_choice *c)
{
    gc_rqueue *q = &gc_int(&run)->rq;
    const char *why = "";
    bool ok = false;
    const bool done = gc_phase_choose(&run, &q->current, c, &why, &ok);
    CHECK(ok, "the answer was refused: %s", why);
    if (!ok) return false;
    if (done) {
        q->waiting = false;
        q->current.type = RP_NONE;
        memset(&run.screen, 0, sizeof run.screen);
        return settle();
    }
    return false;
}

int main(int argc, char **argv)
{
    if (argc != 2 || !(f = fopen(argv[1], "r"))) { fputs("usage: mystery_safari_diff_test <trace>\n", stderr); return 2; }
    tag('V');
    integer();
    integer();
    int n_mons = 0, species_of[6], form_of[6];
    int cases = 0, mismatches = 0, skipped = 0, steps_run = 0, by_action[5] = {0}, catches = 0, flights = 0, shinies = 0, hiddens = 0, full_party = 0, ended = 0;
    char c;
    while (fscanf(f, " %c", &c) == 1) {
        if (c == 'Z') break;
        if (c == 'P') {
            n_mons = (int)integer();
            if (n_mons < 1 || n_mons > 5) { fputs("party size\n", stderr); return 2; }
            for (int m = 0; m < n_mons; m++) { species_of[m] = (int)integer(); form_of[m] = (int)integer(); }
            continue;
        }
        if (c != 'C' || !n_mons) { fprintf(stderr, "wrong record %c\n", c); return 2; }
        char run_seed[64], wave_seed[64];
        case_no = (int)integer();
        word(run_seed, sizeof run_seed); word(wave_seed, sizeof wave_seed);
        const int wave = (int)integer(), biome = (int)integer();
        const int trainer_id = (int)integer(), secret_id = (int)integer();
        const double money = real();
        case_wave = wave; step_no = -1;
        tag('U');
        int levels[6];
        for (int m = 0; m < n_mons; m++) levels[m] = (int)integer();
        tag('L');
        const int n_charms = (int)integer();
        char charm_key[2][32];
        int charm_stack[2];
        for (int k = 0; k < n_charms && k < 2; k++) {
            if (k) semicolon();
            word(charm_key[k], sizeof charm_key[k]);
            charm_stack[k] = (int)integer();
        }
        tag('A');
        const int meets_encounter = (int)integer(), meets_enter = (int)integer(), meets_leave = (int)integer();
        (void)meets_encounter;
        tag('R');
        const pr_rng before = rng_vals();
        tag('K');
        const int up_num_encounters = (int)integer();
        const double up_money_token = real();

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
            run.party[m] = tmpl;
            gc_mon *mon = &run.party[m];
            mon->b.id = (uint32_t)(0x4000 + m);
            mon->b.species = (uint16_t)species_of[m];
            mon->b.form = (uint8_t)form_of[m];
            mon->b.fusion_species = 0;
            mon->b.level = (uint8_t)levels[m];
            mon->b.status = 0;
            mon->b.on_field = false;
            run.party_order[m] = (int8_t)m;
        }
        run.n_party = n_mons;
        for (int k = 0; k < n_charms && k < 2; k++) gc_item_add(&run, charm_key[k], -1, 0, charm_stack[k]);
        gc_items_to_battle(&run);
        for (int m = 0; m < n_mons; m++) {
            pr_calculate_stats(&run.bt, &run.party[m].b);
            run.party[m].b.hp = run.party[m].b.stats[0];
        }
        run.trainer_id = trainer_id;
        run.secret_id = secret_id;
        run.wave = wave;
        run.biome = biome;
        run.money = money;
        run.n_enemy = 0;
        run.in_battle = false;
        run.me_battle = false;
        run.battle_type = BATTLE_TYPE_WILD;
        pr_seed_from_ascii(&run.run.wave_seed, wave_seed);
        run.run.rng = before;
        gc_rq_reset(&gc_int(&run)->rq);
        run.mystery_type = GC_ME_SAFARI_ZONE;
        gc_rq_unshift(&run, gc_rp(RP_MYSTERY, GC_ME_SAFARI_ZONE, GC_ME_GREAT, 0));
        { gc_rqueue *q = &gc_int(&run)->rq; gc_rphase p; gc_rq_next(q, &p); q->current = p; q->waiting = !gc_phase_start(&run, &q->current); }
        { gc_event e; while (gc_run_next_event(&run, &e)) {} }
        bad = 0;
        CHECK(run.screen.kind == GC_SCREEN_MYSTERY && run.screen.n_options == 2, "%d options", run.screen.n_options);
        EQ("enter is offered as upstream says", run.screen.option_ok[0], meets_enter);
        EQ("leave is offered as upstream says", run.screen.option_ok[1], meets_leave);
        EQ("no party pick", run.screen.option_pick[0] + run.screen.option_pick[1], 0);
        CHECK(same_rng(&run.run.rng, &before), "setting the screen moved the run RNG");
        {
            bool num = false, mon = false;
            for (int a = 0; a < run.screen.n_me_args; a++) {
                const pr_msg_arg *g = &run.screen.me_args[a];
                if (g->name && !strcmp(g->name, "numEncounters")) { num = true; EQ("numEncounters token", g->v, up_num_encounters); }
                if (g->name && !strcmp(g->name, "money")) { mon = true; EQ("money token", (long long)gc_safari_cost(&run), (long long)up_money_token); }
            }
            CHECK(num && mon, "the screen does not carry numEncounters and money");
        }

        char t = '?';
        if (fscanf(f, " %c", &t) != 1) return 2;
        if (t == 'E') {
            skipped++;
            EQ("the entry is disabled with no money", run.screen.option_ok[0], 0);
            if (bad) mismatches++;
            continue;
        }
        ungetc(t, f);

        double money_before = run.money;
        bool left = false;
        int remaining_up = 3;
        while (fscanf(f, " %c", &t) == 1 && t == 'S') {
            step_no++;
            char label[8];
            word(label, sizeof label);
            const pr_rng up_rng = rng_vals();
            tag('M');
            const int up_remaining = (int)integer(), up_catch = (int)integer(), up_flee = (int)integer();
            const double up_money = real(), up_paid = real();
            const int up_continuous = (int)integer();
            tag('N');
            const int up_cursor = (int)integer(), up_handed = (int)integer(), up_leaves = (int)integer(), up_heal = (int)integer(), up_twitch = (int)integer();
            char rolls[16];
            word(rolls, sizeof rolls);
            integer();   /* the enemy party's length: the old Pokemon wait there for the VictoryPhase's experience */
            tag('W');
            char remaining_tok[32], name_tok[96];
            word(remaining_tok, sizeof remaining_tok);
            word(name_tok, sizeof name_tok);
            char up_shown[8][96], up_queued[8][96];
            const int n_up_shown = (int)integer();
            for (int k = 0; k < n_up_shown && k < 8; k++) word(up_shown[k], sizeof up_shown[k]);
            const int n_up_queued = (int)integer();
            int n_up_queued_msgs = 0;
            for (int k = 0; k < n_up_queued; k++) {
                char tmp[96];
                word(tmp, sizeof tmp);
                if (!strncmp(tmp, "msg:", 4) && n_up_queued_msgs < 8) snprintf(up_queued[n_up_queued_msgs++], sizeof up_queued[0], "%s", tmp);
            }
            tag('Y');
            char first[32];
            word(first, sizeof first);
            struct { int species, form, level, shiny, variant, ivs[6], nature, ability, gender; long long id; int hp, max, stats[6], n_moves, moves[4], luck, catch_rate, tera; } up;
            const bool has_mon = strcmp(first, "-") != 0;
            memset(&up, 0, sizeof up);
            if (has_mon) {
                up.species = atoi(first); up.form = (int)integer(); up.level = (int)integer(); up.shiny = (int)integer(); up.variant = (int)integer();
                for (int k = 0; k < 6; k++) up.ivs[k] = (int)integer();
                up.nature = (int)integer(); up.ability = (int)integer(); up.gender = (int)integer(); up.id = integer();
                up.hp = (int)integer(); up.max = (int)integer();
                for (int k = 0; k < 6; k++) up.stats[k] = (int)integer();
                up.n_moves = (int)integer();
                for (int k = 0; k < up.n_moves && k < 4; k++) up.moves[k] = (int)integer();
                up.luck = (int)integer(); up.catch_rate = (int)integer(); up.tera = (int)integer();
            }

            tag('Q');
            const int n_up_trace = (int)integer();
            char up_trace[12][24];
            for (int k = 0; k < n_up_trace && k < 12; k++) word(up_trace[k], sizeof up_trace[k]);

            /* the step */
            int action = !strcmp(label, "enter") ? -1 : label[1] - '0';
            gc_choice ch = {GC_CH_MYSTERY, (int16_t)(action < 0 ? 0 : action), 0, 0};
            const int summon_before = remaining_up;
            (void)summon_before;
            money_before = run.money;
            by_action[action + 1]++;
            steps_run++;
            if (action < 0) CHECK(run.screen.me_stage == GC_ME_STAGE_FIRST, "the entry is not on the first screen");
            else CHECK(run.screen.me_stage == GC_ME_STAGE_SAFARI && run.screen.n_options == 4, "the actions are not on the minigame's screen");
            const int cursor_before = run.screen.slot;
            (void)cursor_before;
            left = answer(&ch);
            if (!left && gc_int(&run)->rq.waiting && run.screen.kind == GC_SCREEN_SWITCH) {
                /* a full party asks what to release: the Pokemon is let go (not in upstream's trace, which stops at catchPokemon) */
                full_party++;
                gc_choice no = {GC_CH_PICK, -1, 0, 0};
                left = answer(&no);
            }
            said_t said;
            collect(&said);

            const gc_internal *in = gc_int(&run);
            EQ("remaining", in->safari.remaining, up_remaining);
            EQ("catch stage", in->safari.catch_stage, up_catch);
            EQ("flee stage", in->safari.flee_stage, up_flee);
            EQ("money", (long long)run.money, (long long)up_money);
            EQ("paid", (long long)(money_before - run.money), (long long)up_paid);
            EQ("the minigame goes on", in->safari.active, up_continuous);
            CHECK(same_rng(&run.run.rng, &up_rng), "run RNG differs (c %.0f s0 %.17g, upstream c %.0f s0 %.17g)", run.run.rng.c, run.run.rng.s0, up_rng.c, up_rng.s0);
            EQ("left", left, up_leaves > 0);
            EQ("left with the heal shop", up_leaves ? up_heal : 1, 1);
            if (!left) {
                CHECK(gc_int(&run)->rq.waiting && run.screen.kind == GC_SCREEN_MYSTERY && run.screen.me_stage == GC_ME_STAGE_SAFARI, "the four actions are not waiting");
                EQ("four actions handed back", run.screen.n_options, up_handed);
                EQ("cursor", run.screen.slot, up_cursor);
            } else {
                EQ("no actions handed back", up_handed, 0);
                ended++;
            }
            if (strcmp(rolls, "-")) EQ("twitch rate", in->safari.last_twitch, up_twitch);
            else EQ("no ball thrown", in->safari.last_twitch, -1);
            /* the ball: the roll outcome is the Pokemon being caught (a Pokemon summoned or the way out follows) or the ball breaking free (the same Pokemon) */
            if (!strcmp(rolls, "ok")) catches++;
            if (!strcmp(label, "a3") || (n_up_shown && !strcmp(up_shown[0], "T:battle:pokemonFled"))) flights++;

            EQ("rolls on the run RNG", in->safari.n_trace, n_up_trace);
            for (int k = 0; k < in->safari.n_trace && k < n_up_trace && k < 12; k++) {
                char mine[24];
                snprintf(mine, sizeof mine, "%d:%d", in->safari.trace_max[k], in->safari.trace_val[k]);
                CHECK(!strcmp(mine, up_trace[k]), "roll %d is %s, upstream %s", k, mine, up_trace[k]);
            }
            EQ("texts shown", said.n_shown, n_up_shown);
            for (int k = 0; k < said.n_shown && k < n_up_shown && k < 8; k++) CHECK(!strcmp(said.shown[k], up_shown[k]), "text %d is %s, upstream %s", k, said.shown[k], up_shown[k]);
            EQ("texts queued", said.n_queued, n_up_queued_msgs);
            for (int k = 0; k < said.n_queued && k < n_up_queued_msgs && k < 8; k++) CHECK(!strcmp(said.queued[k], up_queued[k]), "queued %d is %s, upstream %s", k, said.queued[k], up_queued[k]);
            if (said.remaining_arg >= 0) EQ("remainingCount", said.remaining_arg, atoi(remaining_tok));
            if (said.amount_arg >= 0) EQ("amount paid", said.amount_arg, (long long)up_paid);
            remaining_up = up_remaining;

            if (has_mon) {
                const gc_mon *e = &run.enemy[0];
                CHECK(run.n_enemy == 1 || left, "no Pokemon in front of the player");
                EQ("species", e->b.species, up.species);
                EQ("form", e->b.form, up.form);
                EQ("level", e->b.level, up.level);
                EQ("shiny", e->b.shiny, up.shiny);
                EQ("variant", e->variant, up.variant);
                for (int k = 0; k < 6; k++) { char n[32]; snprintf(n, sizeof n, "iv %d", k); EQ(n, e->b.ivs[k], up.ivs[k]); }
                EQ("nature", e->has_custom_nature ? e->base_nature : e->b.nature, up.nature);
                EQ("ability index", e->ability_index, up.ability);
                EQ("gender", e->b.gender, up.gender);
                EQ("id", e->b.id, up.id);
                EQ("max hp", e->b.stats[0], up.max);
                /* the last Pokemon caught has left the field when the encounter does (removePokemon: HP 0), a thing upstream's trace stops short of */
                if (!(left && !strcmp(rolls, "ok"))) EQ("hp", e->b.hp, up.hp);
                for (int k = 0; k < 6; k++) { char n[32]; snprintf(n, sizeof n, "stat %d", k); EQ(n, e->b.stats[k], up.stats[k]); }
                EQ("move count", e->b.n_moves, up.n_moves);
                for (int k = 0; k < up.n_moves && k < 4; k++) { char n[32]; snprintf(n, sizeof n, "move %d", k); EQ(n, e->b.moves[k], up.moves[k]); }
                EQ("luck", e->luck, up.luck);
                EQ("catch rate", gc_form(e->b.species, 0)->catch_rate, up.catch_rate);
                EQ("tera type", e->b.tera_type, up.tera);
                if (e->b.shiny) shinies++;
                if (e->ability_index == 2) hiddens++;
            }
        }
        if (t != 'E') { fprintf(stderr, "wrong record: wanted E, got %c\n", t); return 2; }
        if (bad) mismatches++;
        cases++;
    }
    printf("Mystery encounter Safari Zone: %d cases run (%d with no money for the entry), %d steps (enter %d, ball %d, bait %d, mud %d, run %d), %d catches, %d flights, "
           "%d Pokemon shiny, %d with the hidden ability, %d full-party prompts answered, %d ways out, %d fields. TOTAL mismatches %d\n",
           cases, skipped, steps_run, by_action[0], by_action[1], by_action[2], by_action[3], by_action[4], catches, flights, shinies, hiddens, full_party, ended, fields_checked,
           mismatches);
    return mismatches ? 1 : 0;
}
