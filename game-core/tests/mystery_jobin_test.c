/* Jobin's beetle duel (src/gc_jobin.c) against lorenzo's spec (2026-11-23). The spec's tables are written out again below, as plain words, apart from the
 * code's own arrays: every round of every duel the encounter plays is checked against them (the flavour line names Jobin's action, so the oracle knows it). */
#include "../src/internal.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

/* ---------------------------------------------------------------- the spec, as words */
typedef struct { const char *player, *jobin; int you, him, susp; } spec_row;
static const spec_row STANDARD[] = {
    {"hornLock", "hornLock", -10, -10, -10}, {"hornLock", "flank", 0, -30, 0},      {"hornLock", "digIn", -15, +15, 0},
    {"digIn", "digIn", 0, 0, -10},           {"digIn", "hornLock", +15, -15, 0},    {"digIn", "flank", -25, 0, 0},
    {"flank", "flank", -10, -10, -10},       {"flank", "digIn", 0, -25, 0},         {"flank", "hornLock", -30, 0, 0},
};
/* Jobin's attack, then the player's action (the spec's own order of lines) */
static const spec_row KING[] = {
    {"superheatedHorn", "digIn", +10, -20, 0}, {"superheatedHorn", "hornLock", -25, -5, 0}, {"superheatedHorn", "flank", -45, 0, 0}, {"superheatedHorn", "standTrick", 0, -15, 35},
    {"thermalDash", "hornLock", 0, -20, 0},    {"thermalDash", "flank", -20, -5, 0},        {"thermalDash", "digIn", -40, 0, 0},      {"thermalDash", "standTrick", 0, -15, 35},
    {"bubblingSap", "flank", 0, -10, 0},       {"bubblingSap", "digIn", -25, 0, 0},         {"bubblingSap", "hornLock", -40, +15, 0}, {"bubblingSap", "standTrick", 0, -15, 35},
};
static const char *const MOVE_KEY[4] = {"hornLock", "digIn", "flank", "standTrick"};
static const char *const KING_KEY[3] = {"superheatedHorn", "thermalDash", "bubblingSap"};
static const char *const TELE_TEXT[3] = {"A faint heat shimmer rises from Jobin's beetle's horn...", "Heat waves ripple behind Jobin's beetle as it coils...",
                                         "The sap under your beetle's legs starts to smoke..."};
static int move_index(const char *k) { for (int i = 0; i < 4; i++) if (!strcmp(MOVE_KEY[i], k)) return i; return -1; }
static int king_index(const char *k) { for (int i = 0; i < 3; i++) if (!strcmp(KING_KEY[i], k)) return i; return -1; }

static void test_tables(void)
{
    for (size_t i = 0; i < sizeof STANDARD / sizeof *STANDARD; i++) {
        int you, him, susp;
        gc_jobin_normal_delta(move_index(STANDARD[i].player), move_index(STANDARD[i].jobin), &you, &him, &susp);
        assert(you == STANDARD[i].you && him == STANDARD[i].him && susp == STANDARD[i].susp);
    }
    for (size_t i = 0; i < sizeof KING / sizeof *KING; i++) {
        int you, him, susp;
        gc_jobin_king_delta(king_index(KING[i].player), move_index(KING[i].jobin), &you, &him, &susp);
        assert(you == KING[i].you && him == KING[i].him && susp == KING[i].susp);
    }
    /* the bluffs: the normal move that beats the player's counter */
    assert(gc_jobin_bluff_move(2) == 0 && gc_jobin_bluff_move(0) == 2 && gc_jobin_bluff_move(1) == 1);   /* Sap -> Horn Lock, Horn -> Flank, Dash -> Dig In */
    /* the Stand Trick against any normal move: nothing for you, -35 for him, +35 suspicion */
    for (int j = 0; j < 3; j++) {
        gc_jobin_state s;
        memset(&s, 0, sizeof s);
        s.grip_you = s.grip_jobin = 100;
        gc_jobin_round(&s, 3, j, -1);
        assert(s.grip_you == 100 && s.grip_jobin == 65 && s.susp == 35);
    }
    assert(gc_jobin_pool_species(0) == SPECIES_ID_HERACROSS && gc_jobin_pool_species(10) == SPECIES_ID_CRUSTLE && gc_jobin_pool_species(11) == -1);
    int in_pool = 0;
    for (int sp = 1; sp < 1100; sp++) in_pool += gc_jobin_in_pool(sp);
    assert(in_pool == 11);
    puts("ok tables (9 standard cells, 12 Speed King cells, the Stand Trick row, the bluffs, the pool of 11)");
}

static void test_meters(void)
{
    gc_jobin_state s;
    /* consecutive Stand Tricks: +35, +50, +70, and +70 after that; any other move resets the streak */
    memset(&s, 0, sizeof s);
    s.grip_you = s.grip_jobin = 100;
    gc_jobin_round(&s, 3, 0, -1); assert(s.susp == 35 && s.streak == 1);
    gc_jobin_round(&s, 3, 0, -1); assert(s.susp == 85 && s.streak == 2 && s.d_susp == 50);
    gc_jobin_round(&s, 3, 0, -1); assert(s.susp == 100 && s.streak == 3 && gc_jobin_outcome(&s) == 4);   /* +70 would be 155: capped at 100 */
    assert(gc_jobin_stand_suspicion(1) == 35 && gc_jobin_stand_suspicion(2) == 50 && gc_jobin_stand_suspicion(3) == 70 && gc_jobin_stand_suspicion(4) == 70 && gc_jobin_stand_suspicion(9) == 70);
    memset(&s, 0, sizeof s);
    s.grip_you = s.grip_jobin = 100;
    gc_jobin_round(&s, 3, 0, -1);           /* +35 */
    gc_jobin_round(&s, 1, 1, -1);           /* Dig In against Dig In: -10, streak over */
    assert(s.susp == 25 && s.streak == 0);
    gc_jobin_round(&s, 3, 0, -1);           /* back to +35, not +50 */
    assert(s.susp == 60 && s.streak == 1 && s.used_stand);
    /* caps: Grip never above 100, Suspicion never below 0, none above 100 */
    memset(&s, 0, sizeof s);
    s.grip_you = s.grip_jobin = 95;
    gc_jobin_round(&s, 1, 0, -1);           /* Dig In against Horn Lock: +15 for you, -15 for him */
    assert(s.grip_you == 100 && s.grip_jobin == 80 && s.d_you == 5 && s.d_him == -15);
    gc_jobin_round(&s, 0, 1, -1);           /* Horn Lock against Dig In: -15 / +15 */
    assert(s.grip_you == 85 && s.grip_jobin == 95);
    s.susp = 5;
    gc_jobin_round(&s, 1, 1, -1);           /* the staredown: -10 suspicion, floor 0 */
    assert(s.susp == 0 && s.d_susp == -5);
    /* the ends: flipped, both at once (a rematch), caught beats a flip */
    memset(&s, 0, sizeof s);
    s.grip_you = 10; s.grip_jobin = 60;
    gc_jobin_round(&s, 0, 0, -1);           /* deadlock */
    assert(s.grip_you == 0 && gc_jobin_outcome(&s) == 1);
    s.grip_you = 10; s.grip_jobin = 10;
    gc_jobin_round(&s, 2, 2, -1);
    assert(gc_jobin_outcome(&s) == 3);
    memset(&s, 0, sizeof s);
    s.grip_you = 100; s.grip_jobin = 20; s.susp = 80;
    gc_jobin_round(&s, 3, 0, -1);           /* he is flipped and the suspicion is full in the same round */
    assert(s.grip_jobin == 0 && gc_jobin_outcome(&s) == 4);
    puts("ok meters (Stand Trick streak 35/50/70/70 and its reset, the caps, flipped / both / caught)");
}

/* ---------------------------------------------------------------- the encounter on a run */
static gc_run run;
static gc_system sys;
static bool offer = true;

static bool pillar(void *user, const struct gc_run *r, int which, gc_custom_team *team)
{
    (void)user; (void)r; (void)team;
    return which < 0 && offer;
}
static const gc_custom_hooks hooks = {.pillar = pillar};

static void make_run(const char *seed, bool with_hook, const int *party, int n_party, int wave)
{
    memset(&run, 0, sizeof run);
    gc_run_new(&run, GAME_MODES_CLASSIC, seed, 7);
    if (with_hook) gc_run_set_custom(&run, &hooks);
    gc_system_init_defaults(&sys);
    run.sys = &sys;
    run.wave = wave;
    run.biome = BIOME_ID_PLAINS;
    run.n_items = 0;
    run.n_party = n_party;
    for (int i = 0; i < n_party; i++) {
        gc_starter st;
        memset(&st, 0, sizeof st);
        st.species = (uint16_t)party[i];
        st.tera_type = -1;
        for (int k = 0; k < 6; k++) st.ivs[k] = 15;
        st.moves[0] = MOVE_ID_TACKLE;
        gc_mon_new_player(&run, &run.party[i], &st, 40 + i);
        run.party_order[i] = (int8_t)i;
    }
    run.money = 5000;
    run.battle_type = BATTLE_TYPE_WILD;
    gc_items_to_battle(&run);
}

/* a run on the first screen; the seed is searched so Jobin's beetle is not `avoid` (the own beetle then can be used) */
static void start(const char *prefix, const int *party, int n_party, int avoid)
{
    for (int i = 0;; i++) {
        char seed[32];
        snprintf(seed, sizeof seed, "%s-%d", prefix, i);
        make_run(seed, true, party, n_party, 45);
        if (avoid < 0 || gc_jobin_roll_species(&run) != avoid) break;
    }
    gc_rq_reset(&gc_int(&run)->rq);
    run.mystery_type = GC_ME_JOBIN;
    gc_rq_unshift(&run, gc_rp(RP_MYSTERY, GC_ME_JOBIN, GC_ME_GREAT, 0));
    gc_run_until_screen(&run);
    assert(run.screen.kind == GC_SCREEN_MYSTERY && run.screen.mystery_type == GC_ME_JOBIN && run.screen.n_options == 3);
}

static bool choose(int a, int b)
{
    gc_choice c = {GC_CH_MYSTERY, (int16_t)a, (int16_t)b, 0};
    const char *why = NULL;
    return gc_run_choose(&run, &c, &why);
}

/* the messages since the last drain, as one string of keys; every one of Jobin's waits for A */
static int moves_n, moves_you, moves_him, update_seen;   /* the round's animation event: how many came, what they name; whether the meters already moved */
static void drain(char *out, size_t n)
{
    out[0] = 0;
    moves_n = moves_you = moves_him = update_seen = 0;
    gc_event e;
    while (gc_run_next_event(&run, &e)) {
        if (e.kind == GC_EV_JOBIN && (e.hp & 255) == GC_JOBIN_HUD_MOVES) { assert(!update_seen); moves_n++; moves_you = e.a; moves_him = e.b; }   /* the animations come before the meters move */
        if (e.kind == GC_EV_JOBIN && (e.hp & 255) == GC_JOBIN_HUD_UPDATE) update_seen = true;
        if (e.kind == GC_EV_MESSAGE && e.msg.key) {
            if (strstr(e.msg.key, "jobin:")) assert(e.prompt);
            if (strlen(out) + strlen(e.msg.key) + 2 < n) { strcat(out, e.msg.key); strcat(out, "|"); }
        }
    }
}

static const int BEETLES[4] = {SPECIES_ID_VENUSAUR, SPECIES_ID_HERACROSS, SPECIES_ID_LAPRAS, SPECIES_ID_PINSIR};

static void test_offer(void)
{
    int bizarre = 0, plain = 0, early = 0, late = 0;
    for (int i = 0; i < 600; i++) {
        char seed[16];
        int t, tier;
        snprintf(seed, sizeof seed, "jo-%d", i);
        make_run(seed, true, BEETLES, 4, 20 + i % 121);
        pr_run_reset_seed(&run.run, run.wave);
        assert(gc_mystery_select(&run, &t, &tier));
        bizarre += t == GC_ME_JOBIN;
        if (t == GC_ME_JOBIN) assert(tier == GC_ME_GREAT);
        make_run(seed, false, BEETLES, 4, 20 + i % 121);
        pr_run_reset_seed(&run.run, run.wave);
        assert(gc_mystery_select(&run, &t, &tier));
        plain += t == GC_ME_JOBIN;
        make_run(seed, true, BEETLES, 4, 10 + i % 10);
        pr_run_reset_seed(&run.run, run.wave);
        assert(gc_mystery_select(&run, &t, &tier));
        early += t == GC_ME_JOBIN;
        make_run(seed, true, BEETLES, 4, 141 + i % 40);
        pr_run_reset_seed(&run.run, run.wave);
        assert(gc_mystery_select(&run, &t, &tier));
        late += t == GC_ME_JOBIN;
    }
    assert(bizarre > 0 && plain == 0 && early == 0 && late == 0);
    int tier, minw, maxw, seen, party, ch, mask;
    double money;
    assert(gc_mystery_rule(GC_ME_JOBIN, &tier, &minw, &maxw, &seen, &party, &ch, &money, &mask));
    assert(tier == GC_ME_GREAT && minw == 20 && maxw == 140 && seen == 1);
    for (int b = 0; b <= BIOME_ID_END; b++) assert(gc_mystery_in_biome(b, GC_ME_JOBIN));
    assert(GC_ME_JOBIN >= GC_ME_UPSTREAM_COUNT);
    printf("ok offer (Bizarre only, Great, waves 20..140, any biome, once: drawn %d of 600; never plain, before 20 or after 140)\n", bizarre);
}

static void test_first_screen(void)
{
    /* walking away costs nothing and gives nothing */
    start("jw", BEETLES, 4, -1);
    assert(choose(GC_JOBIN_WALK_AWAY, 0));
    assert(run.n_party == 4 && run.money == 5000 && run.screen.kind != GC_SCREEN_REWARD && !gc_int(&run)->jobin.active);
    /* own beetle: a pool member of the party that is not Jobin's species; the rental is always there */
    start("jf", BEETLES, 4, SPECIES_ID_HERACROSS);
    assert(run.screen.option_pick[GC_JOBIN_OWN] == GC_PICK_MEMBER && run.screen.option_ok[GC_JOBIN_OWN] && run.screen.option_ok[GC_JOBIN_RENTAL] && run.screen.option_ok[GC_JOBIN_WALK_AWAY]);
    assert(gc_jobin_pick_ok(&run, 0, 1) && !gc_jobin_pick_ok(&run, 0, 0) && !gc_jobin_pick_ok(&run, 0, 2) && !gc_jobin_pick_ok(&run, 1, 1) && !gc_jobin_pick_ok(&run, 0, 9));
    assert(run.screen.n_me_args == 1 && !strcmp(run.screen.me_args[0].name, "jobinBeetle") && run.screen.me_args[0].kind == PR_ARG_POKEMON_NAME);
    assert(!choose(GC_JOBIN_OWN, 0));   /* a Venusaur is no beetle */
    assert(!choose(GC_JOBIN_OWN, 9));
    /* without a beetle of its own the option is greyed out (the loan still works) */
    static const int NONE[3] = {SPECIES_ID_VENUSAUR, SPECIES_ID_LAPRAS, SPECIES_ID_PIKACHU};
    start("jn", NONE, 3, -1);
    assert(!run.screen.option_ok[GC_JOBIN_OWN] && run.screen.option_ok[GC_JOBIN_RENTAL]);
    assert(!choose(GC_JOBIN_OWN, 1));
    /* a fainted beetle may still be used; no same-species duel: the species Jobin drew is not a choice */
    int same = 0;
    for (int i = 0; i < 40; i++) {
        char seed[16];
        snprintf(seed, sizeof seed, "js-%d", i);
        make_run(seed, true, BEETLES, 4, 45);
        run.party[1].b.hp = 0;
        gc_rq_reset(&gc_int(&run)->rq);
        run.mystery_type = GC_ME_JOBIN;
        gc_rq_unshift(&run, gc_rp(RP_MYSTERY, GC_ME_JOBIN, GC_ME_GREAT, 0));
        gc_run_until_screen(&run);
        const int jobin = gc_jobin_roll_species(&run);
        assert(gc_jobin_in_pool(jobin));
        for (int k = 1; k <= 3; k += 2) {
            const bool ok = gc_jobin_pick_ok(&run, 0, k);
            assert(ok == (run.party[k].b.species != jobin));
            same += !ok;
        }
        if (jobin != SPECIES_ID_HERACROSS) assert(gc_jobin_pick_ok(&run, 0, 1));   /* the fainted Heracross */
    }
    assert(same > 0);
    puts("ok first screen (walk away free; own beetle: a pool member of the party, fainted too, never Jobin's species; greyed out without one; the loan always)");
}

/* ---------------------------------------------------------------- duels, checked against the spec round by round */
static uint32_t rnd_state = 12345;
static int rnd(int n) { rnd_state = rnd_state * 1664525u + 1013904223u; return (int)((rnd_state >> 8) % (uint32_t)n); }

typedef struct { int rounds, telegraphs, eligible, bluffs, kings, ended; } duel_stats;

/* plays the screens of the encounter until the duel ends; `pick` chooses the move (-1: random, without the Stand Trick unless stand is true) */
static void play_duel(duel_stats *st, bool stand, int fixed_move)
{
    gc_internal *in = gc_int(&run);
    int prev_tele = -1;
    char keys[8192];
    drain(keys, sizeof keys);
    while (run.screen.kind == GC_SCREEN_MYSTERY && run.screen.me_stage == GC_ME_STAGE_JOBIN_ROUND) {
        assert(run.screen.n_options == 4);
        const int tele = in->jobin.tele;
        assert(!(prev_tele >= 0 && tele >= 0));   /* never two special rounds in a row, a bluff included */
        int arg_tele = -2;
        for (int a = 0; a < run.screen.n_me_args; a++) if (!strcmp(run.screen.me_args[a].name, "telegraph")) arg_tele = (int)run.screen.me_args[a].v;
        assert(arg_tele == tele);
        const bool bluff = in->jobin.bluff;
        const int before_you = in->jobin.grip_you, before_him = in->jobin.grip_jobin, before_susp = in->jobin.susp, streak = in->jobin.streak;
        const int move = fixed_move >= 0 ? fixed_move : rnd(stand ? 4 : 3);
        assert(choose(move, 0));
        drain(keys, sizeof keys);
        st->rounds++;
        /* the animations (lorenzo 2026-11-23), written out again here: exactly one event a round; yours is Horn Attack / Harden / Lunge (the Stand Trick: none),
         * his is the powered attack's move in a Speed King round and a plain move's otherwise (never none) */
        {
            static const int PLAIN[3] = {MOVE_ID_HORN_ATTACK, MOVE_ID_HARDEN, MOVE_ID_LUNGE};
            static const int POWERED[3] = {MOVE_ID_FLARE_BLITZ, MOVE_ID_FLAME_CHARGE, MOVE_ID_BURNING_BULWARK};
            assert(moves_n == 1);
            assert(moves_you == (move == 3 ? 0 : PLAIN[move]));
            if (tele >= 0 && !bluff) assert(moves_him == POWERED[tele]);
            else assert(moves_him == PLAIN[0] || moves_him == PLAIN[1] || moves_him == PLAIN[2]);
            if (tele >= 0 && bluff) assert(moves_him == PLAIN[tele == 2 ? 0 : tele == 0 ? 2 : 1]);   /* a bluff plays the plain move that beats your counter */
        }
        /* the flavour line names Jobin's action; the oracle's table gives the changes */
        int you = 0, him = 0, susp = 0;
        bool found = false;
        if (tele >= 0 && !bluff) {
            st->kings++;
            for (size_t i = 0; i < sizeof KING / sizeof *KING && !found; i++)
                if (king_index(KING[i].player) == tele && move_index(KING[i].jobin) == move) { you = KING[i].you; him = KING[i].him; susp = KING[i].susp; found = true; }
            if (move == 3) susp = streak == 0 ? 35 : streak == 1 ? 50 : 70;   /* the streak scales every Stand Trick, a Speed King round's too */
            assert(found);
        } else if (move == 3) {
            you = 0; him = -35; susp = streak == 0 ? 35 : streak == 1 ? 50 : 70;
            found = true;
        } else {
            char *p = strstr(keys, "jobin:round.");
            assert(p);
            char pk[24], jk[24];
            assert(sscanf(p + 12, "%23[A-Za-z].%23[A-Za-z]", pk, jk) == 2);
            assert(move_index(pk) == move);
            if (bluff) { assert(strstr(keys, "jobin:bluff.reveal")); st->bluffs++; assert(move_index(jk) == (tele == 2 ? 0 : tele == 0 ? 2 : 1)); }
            for (size_t i = 0; i < sizeof STANDARD / sizeof *STANDARD && !found; i++)
                if (move_index(STANDARD[i].player) == move && move_index(STANDARD[i].jobin) == move_index(jk)) { you = STANDARD[i].you; him = STANDARD[i].him; susp = STANDARD[i].susp; found = true; }
            assert(found);
        }
        if (tele >= 0 && !bluff) assert(!strstr(keys, "bluff.reveal"));
        const int ey = (int)fmin(100, fmax(0, before_you + you)), eh = (int)fmin(100, fmax(0, before_him + him)), es = (int)fmin(100, fmax(0, before_susp + susp));
        const bool both = ey == 0 && eh == 0 && es < 100;
        assert(in->jobin.susp == es);
        if (both) { assert(in->jobin.grip_you == 50 && in->jobin.grip_jobin == 50 && strstr(keys, "jobin:end.rematch")); }
        else { assert(in->jobin.grip_you == ey && in->jobin.grip_jobin == eh); }
        assert(in->jobin.grip_you >= 0 && in->jobin.grip_you <= 100 && in->jobin.grip_jobin >= 0 && in->jobin.grip_jobin <= 100 && in->jobin.susp >= 0 && in->jobin.susp <= 100);
        assert(strstr(keys, "jobin:round.result"));
        const bool ended = es >= 100 || ((ey == 0 || eh == 0) && !both);
        if (!ended) {
            if (tele < 0) st->eligible++;   /* the round just played was not special: a telegraph could come */
            if (in->jobin.tele >= 0) {
                st->telegraphs++;
                assert(tele < 0);
                char want[64];
                snprintf(want, sizeof want, "jobin:telegraph.%s", KING_KEY[in->jobin.tele]);
                assert(strstr(keys, want));
            } else {
                assert(!strstr(keys, "jobin:telegraph."));
            }
        } else {
            assert(!strstr(keys, "jobin:telegraph."));
        }
        prev_tele = tele;
        (void)TELE_TEXT;
    }
    st->ended++;
}

static void begin_duel(const char *prefix, bool own, int n_party_beetles_hint)
{
    (void)n_party_beetles_hint;
    start(prefix, BEETLES, 4, SPECIES_ID_HERACROSS);
    assert(choose(own ? GC_JOBIN_OWN : GC_JOBIN_RENTAL, own ? 1 : 0));
    assert(run.screen.kind == GC_SCREEN_MYSTERY && run.screen.me_stage == GC_ME_STAGE_JOBIN_ROUND);
    const gc_internal *in = gc_int(&run);
    assert(in->jobin.active && in->jobin.grip_you == 100 && in->jobin.grip_jobin == 100 && in->jobin.susp == 0 && in->jobin.tele == -1);
    assert(in->jobin.rental == !own && in->jobin.you_species != in->jobin.jobin_species && gc_jobin_in_pool(in->jobin.you_species) && gc_jobin_in_pool(in->jobin.jobin_species));
    if (own) assert(in->jobin.own == 1 && in->jobin.you_species == SPECIES_ID_HERACROSS);
}

static void force(int you, int him, int susp, int tele, bool bluff)
{
    gc_jobin_state *s = &gc_int(&run)->jobin;
    s->grip_you = (int16_t)you; s->grip_jobin = (int16_t)him; s->susp = (int16_t)susp; s->tele = (int8_t)tele; s->bluff = bluff;
}

static void test_duels(void)
{
    duel_stats st;
    memset(&st, 0, sizeof st);
    int lost = 0, won = 0, caught = 0;
    for (int i = 0; i < 500; i++) {
        char prefix[16];
        snprintf(prefix, sizeof prefix, "jd%d", i);
        start(prefix, BEETLES, 4, SPECIES_ID_HERACROSS);
        assert(choose(i % 2 ? GC_JOBIN_OWN : GC_JOBIN_RENTAL, 1));
        rnd_state = 777u + (uint32_t)i * 31u;
        play_duel(&st, i % 5 == 0, -1);
        const gc_jobin_state *s = &gc_int(&run)->jobin;
        if (s->caught) caught++;
        else if (run.screen.kind == GC_SCREEN_MYSTERY || run.n_party > 4 || s->grip_jobin == 0) won += s->grip_jobin == 0;
        lost += !s->caught && s->grip_you == 0 && s->grip_jobin > 0;
    }
    assert(st.ended == 500 && st.rounds > 1500 && st.kings > 50 && st.telegraphs > 100 && st.bluffs > 10);
    /* a telegraph came 33% of the rounds that could have one (not after a special one); one in four of the telegraphs is a bluff */
    const double rate = (double)st.telegraphs / st.eligible, bluff_rate = (double)st.bluffs / st.telegraphs;
    assert(rate > 0.28 && rate < 0.38 && bluff_rate > 0.19 && bluff_rate < 0.31);
    assert(won > 50 && lost > 50 && caught > 5);
    printf("ok duels (500 duels, %d rounds, every round matches the spec's tables; telegraphs %.1f%% of eligible rounds, %.1f%% bluffs, never two in a row; %d won, %d lost, %d caught)\n",
           st.rounds, rate * 100, bluff_rate * 100, won, lost, caught);
}

static int held(uint32_t id, const char *key, int arg, bool *locked)
{
    int n = 0;
    for (int i = 0; i < run.n_items; i++)
        if (run.items[i].pokemon_id == id && !strcmp(run.items[i].key, key) && run.items[i].arg == arg) { n += run.items[i].stack; if (locked) *locked = run.items[i].non_transferable; }
    return n;
}

static void test_win_own(void)
{
    begin_duel("jwo", true, 0);
    const uint32_t id = run.party[1].b.id;
    const uint32_t wins = sys.achv.jobin_wins;
    force(100, 15, 0, 0, false);
    assert(choose(1, 0));   /* Dig In against Superheated Horn: +10 / -20, he is flipped */
    char keys[4096];
    drain(keys, sizeof keys);
    assert(strstr(keys, "jobin:end.jobinFlipped") && strstr(keys, "jobin:win.line") && strstr(keys, "jobin:give.own") && !strstr(keys, "end.youFlipped"));
    assert(sys.achv.jobin_wins == wins + 1 && sys.achv.jobin_punish_wins == 0);
    /* a Grip Claw, a King's Rock and two Silver Powder, none of them movable */
    bool lock = false;
    assert(held(id, "GRIP_CLAW", -1, &lock) == 1 && lock);
    assert(held(id, "KINGS_ROCK", -1, &lock) == 1 && lock);
    assert(held(id, "ATTACK_TYPE_BOOSTER", POKEMON_TYPE_BUG, &lock) == 2 && lock);
    assert(run.money == 5000 && run.party[1].b.hp > 0);
    /* a clean win (not one Stand Trick): the shiny beetle is offered, on the screen of a caught Pokemon's replacement */
    assert(run.screen.kind == GC_SCREEN_MYSTERY && run.screen.me_stage == GC_ME_STAGE_JOBIN_KEEP && run.screen.n_options == 2);
    int bonus = -1, party_full = -1, variant = -1;
    for (int a = 0; a < run.screen.n_me_args; a++) {
        if (!strcmp(run.screen.me_args[a].name, "bonus")) bonus = (int)run.screen.me_args[a].v;
        if (!strcmp(run.screen.me_args[a].name, "partyFull")) party_full = (int)run.screen.me_args[a].v;
        if (!strcmp(run.screen.me_args[a].name, "variant")) variant = (int)run.screen.me_args[a].v;
    }
    assert(bonus == 1 && party_full == 0 && variant >= 0 && variant <= 2 && run.screen.option_pick[0] == GC_PICK_NONE);
    assert(choose(0, 0));
    assert(run.n_party == 5);
    const gc_mon *b = &run.party[4];
    assert(gc_jobin_in_pool(b->b.species) && b->b.shiny && b->b.variant == variant && b->b.hp == b->b.stats[STAT_HP] && b->b.is_player);
    assert(held(b->b.id, "ATTACK_TYPE_BOOSTER", POKEMON_TYPE_BUG, NULL) == 0);   /* the shiny beetle holds nothing */
    assert(run.screen.kind != GC_SCREEN_MYSTERY && !gc_int(&run)->jobin.active);
    /* leaving the shiny beetle */
    begin_duel("jwo2", true, 0);
    force(100, 15, 0, 0, false);
    assert(choose(1, 0));
    assert(run.screen.me_stage == GC_ME_STAGE_JOBIN_KEEP);
    assert(choose(1, 0));
    assert(run.n_party == 4 && run.screen.kind != GC_SCREEN_MYSTERY);
    /* one Stand Trick and the clean-win beetle is gone, the own beetle's items stay */
    begin_duel("jwo3", true, 0);
    force(100, 30, 0, -1, false);
    assert(choose(3, 0));
    drain(keys, sizeof keys);
    assert(strstr(keys, "jobin:end.jobinFlipped") && strstr(keys, "jobin:give.own") && !strstr(keys, "bonus.line"));
    assert(held(run.party[1].b.id, "GRIP_CLAW", -1, NULL) == 1 && run.n_party == 4 && run.screen.kind != GC_SCREEN_MYSTERY);
    /* the star tiers of the shiny beetle: 60 / 30 / 10 */
    int tier[3] = {0, 0, 0};
    for (int i = 0; i < 600; i++) {
        char prefix[16];
        snprintf(prefix, sizeof prefix, "jb%d", i);
        begin_duel(prefix, true, 0);
        force(100, 15, 0, 0, false);
        assert(choose(1, 0));
        assert(run.screen.me_stage == GC_ME_STAGE_JOBIN_KEEP);
        const gc_internal *in = gc_int(&run);
        assert(gc_jobin_in_pool(in->jobin.bonus_species) && in->jobin.bonus_variant <= 2);
        tier[in->jobin.bonus_variant]++;
    }
    assert(tier[0] > 310 && tier[0] < 410 && tier[1] > 140 && tier[1] < 220 && tier[2] > 35 && tier[2] < 90);
    printf("ok win with your own beetle (Grip Claw, King's Rock, 2 Silver Powder locked; clean win: a shiny beetle %d/%d/%d of 600 yellow/blue/red; a Stand Trick: none)\n", tier[0], tier[1], tier[2]);
}

static void test_win_rental(void)
{
    begin_duel("jwr", false, 0);
    const int lvl_wave = run.wave;
    (void)lvl_wave;
    const gc_internal *in = gc_int(&run);
    const int species = in->jobin.you_species;
    force(100, 15, 0, 0, false);
    const int relic_before = gc_item_stack(&run, "RELIC_GOLD");
    assert(choose(1, 0));
    assert(gc_item_stack(&run, "RELIC_GOLD") == relic_before + 1);   /* the Relic Gold */
    assert(run.screen.kind == GC_SCREEN_MYSTERY && run.screen.me_stage == GC_ME_STAGE_JOBIN_CHARCOAL && run.screen.n_options == 1 && run.screen.option_pick[0] == GC_PICK_MEMBER);
    assert(!choose(0, 9));
    const uint32_t id2 = run.party[2].b.id;
    assert(choose(0, 2));   /* the Charcoal to anyone */
    bool lock = true;
    assert(held(id2, "ATTACK_TYPE_BOOSTER", POKEMON_TYPE_FIRE, &lock) == 1 && !lock);
    /* the loan: keep it, holding one Silver Powder that is not locked */
    assert(run.screen.me_stage == GC_ME_STAGE_JOBIN_KEEP);
    assert(choose(0, 0));
    assert(run.n_party == 5 && run.party[4].b.species == species && !run.party[4].b.shiny);
    lock = true;
    assert(held(run.party[4].b.id, "ATTACK_TYPE_BOOSTER", POKEMON_TYPE_BUG, &lock) == 1 && !lock);
    /* the clean win's shiny beetle follows; with six in the party it asks for the member it replaces */
    assert(run.screen.me_stage == GC_ME_STAGE_JOBIN_KEEP);
    assert(choose(0, 0));   /* take the shiny one too: two beetles made from his stream must not share an id (held items and the screens find a Pokemon by it) */
    assert(run.n_party == 6 && run.party[5].b.shiny);
    for (int i = 0; i < run.n_party; i++)
        for (int k = 0; k < i; k++) assert(run.party[i].b.id != run.party[k].b.id);
    assert(run.screen.kind != GC_SCREEN_MYSTERY && run.money == 5000);
    /* a full party: the loan replaces a member picked (its items dropped); leaving it is free */
    static const int SIX[6] = {SPECIES_ID_VENUSAUR, SPECIES_ID_HERACROSS, SPECIES_ID_LAPRAS, SPECIES_ID_PIKACHU, SPECIES_ID_SNORLAX, SPECIES_ID_ALAKAZAM};
    start("jwr6", SIX, 6, -1);
    assert(choose(GC_JOBIN_RENTAL, 0));
    force(100, 15, 0, 0, false);
    gc_item_add(&run, "LEFTOVERS", -1, run.party[3].b.id, 1);
    const uint32_t gone = run.party[3].b.id;
    assert(choose(1, 0));
    assert(choose(0, 0));   /* the charcoal */
    assert(run.screen.me_stage == GC_ME_STAGE_JOBIN_KEEP && run.screen.option_pick[0] == GC_PICK_MEMBER);
    assert(!choose(0, 9));
    assert(choose(0, 3));
    assert(run.n_party == 6 && run.party[3].b.id != gone && held(gone, "LEFTOVERS", -1, NULL) == 0);
    assert(run.screen.me_stage == GC_ME_STAGE_JOBIN_KEEP);   /* the shiny one */
    assert(choose(1, 0));
    assert(run.n_party == 6 && run.screen.kind != GC_SCREEN_MYSTERY);
    puts("ok win with a loaned beetle (a Relic Gold, a Charcoal for anyone, the beetle kept with one unlocked Silver Powder, replace with six, leave)");
}

static void test_loss(void)
{
    begin_duel("jl1", true, 0);
    force(20, 100, 0, 0, false);
    assert(choose(2, 0));   /* Flank against Superheated Horn: -45 */
    char keys[4096];
    drain(keys, sizeof keys);
    assert(strstr(keys, "jobin:end.youFlipped") && strstr(keys, "jobin:lose.own") && !strstr(keys, "win.line"));
    assert(run.money == 2500 && run.party[1].b.hp == 0 && run.n_party == 4 && run.screen.kind != GC_SCREEN_MYSTERY);
    assert(sys.achv.jobin_wins == 0);
    /* an odd amount rounds the cut down: he takes floor(half) */
    begin_duel("jl2", true, 0);
    run.money = 5001;
    force(20, 100, 0, 0, false);
    assert(choose(2, 0));
    assert(run.money == 2501);
    /* a beetle that was fainted already: no further consequence beyond the money */
    begin_duel("jl3", true, 0);
    run.party[1].b.hp = 0;
    force(20, 100, 0, 0, false);
    assert(choose(2, 0));
    assert(run.money == 2500 && run.party[1].b.hp == 0);
    /* the loan: all the money, nobody's beetle harmed */
    begin_duel("jl4", false, 0);
    force(20, 100, 0, 0, false);
    assert(choose(2, 0));
    drain(keys, sizeof keys);
    assert(strstr(keys, "jobin:lose.rental") && run.money == 0 && run.party[1].b.hp > 0 && run.party[3].b.hp > 0);
    puts("ok loss (own beetle: half the money, it faints, a fainted one only the money; the loan: all the money)");
}

static void test_rematch(void)
{
    begin_duel("jr", true, 0);
    force(20, 5, 33, 1, false);   /* Thermal Dash against Flank: -20 for you, -5 for him: both at 0 */
    assert(choose(2, 0));
    char keys[4096];
    drain(keys, sizeof keys);
    assert(strstr(keys, "jobin:end.rematch") && !strstr(keys, "Flipped"));
    const gc_internal *in = gc_int(&run);
    assert(in->jobin.grip_you == 50 && in->jobin.grip_jobin == 50 && in->jobin.susp == 33);   /* the suspicion stays as the round left it */
    assert(run.screen.me_stage == GC_ME_STAGE_JOBIN_ROUND && in->jobin.active && run.money == 5000);
    puts("ok rematch (both at 0: both back to 50%, the suspicion as the round left it, the duel goes on)");
}

static void test_caught(void)
{
    begin_duel("jc", true, 0);
    force(100, 100, 70, -1, false);
    assert(choose(3, 0));   /* 70 + 35 */
    char keys[8192];
    drain(keys, sizeof keys);
    assert(strstr(keys, "jobin:caught.void") && strstr(keys, "jobin:caught.line") && !strstr(keys, "win.line") && !strstr(keys, "lose."));
    assert(sys.achv.jobin_wins == 0 && run.money == 5000 && run.party[1].b.hp > 0);   /* void: nothing changes hands */
    gc_internal *in = gc_int(&run);
    assert(in->jobin.caught && in->jobin.punish && gc_jobin_punish_battle(&run) && !in->jobin.active);
    assert(run.battle_type == BATTLE_TYPE_TRAINER && run.n_enemy == 4 && run.in_battle && !in->at_save_point);
    static const int TEAM[4] = {SPECIES_ID_HEATMOR, SPECIES_ID_MAGMORTAR, SPECIES_ID_CENTISKORCH, SPECIES_ID_SCIZOR};
    for (int i = 0; i < 4; i++) {
        assert(run.enemy[i].b.species == TEAM[i]);
        assert(run.enemy[i].b.hp == run.enemy[i].b.stats[STAT_HP] && run.enemy[i].b.n_moves == 4);
        assert(run.enemy[i].b.passive_flag);
        int charcoal = 0;
        for (int k = 0; k < run.n_enemy_items; k++)
            charcoal += run.enemy_items[k].pokemon_id == run.enemy[i].b.id && !strcmp(run.enemy_items[k].key, "ATTACK_TYPE_BOOSTER") && run.enemy_items[k].arg == POKEMON_TYPE_FIRE && run.enemy_items[k].stack == 1;
        assert(charcoal == 1);
    }
    /* no healing beforehand: the party is as it was (a wounded member stays wounded) */
    gc_custom_team team;
    assert(gc_jobin_team(&team) && team.n == 4 && !strcmp(team.name, "Jobin"));
    /* he catches you even when you flipped his beetle in the same round: no standard reward (no win counted) */
    begin_duel("jc2", true, 0);
    force(100, 20, 80, -1, false);
    assert(choose(3, 0));
    drain(keys, sizeof keys);
    assert(strstr(keys, "jobin:caught.line") && !strstr(keys, "win.line") && !strstr(keys, "jobinFlipped") && sys.achv.jobin_wins == 0 && gc_jobin_punish_battle(&run));
    /* the win over his team: the achievement, half the prize money, then the normal reward screen */
    in = gc_int(&run);
    const uint32_t pw = sys.achv.jobin_punish_wins;
    const double m0 = run.money;
    gc_rq_reset(&in->rq);
    gc_queue_battle_end(&run);
    assert(sys.achv.jobin_punish_wins == pw + 1 && !in->jobin.punish && in->jobin.half_money);
    gc_run_until_screen(&run);
    const double half = run.money - m0;
    assert(half > 0 && !in->jobin.half_money);
    gc_rq_reset(&in->rq);
    gc_rq_push(&run, gc_rp(RP_TRAINER_VICTORY, 0, 0, 0));
    const double m1 = run.money;
    gc_run_until_screen(&run);
    const double full = run.money - m1;
    assert(full > 0 && fabs(half * 2 - full) <= 10);   /* the prize money is rounded down to tens */
    printf("ok caught (void bet, the tissue line, Jobin's four with a Charcoal and passives, no win counted; won: achievement, prize money %.0f of %.0f)\n", half, full);
}

int main(void)
{
    test_tables();
    test_meters();
    test_offer();
    test_first_screen();
    test_duels();
    test_win_own();
    test_win_rental();
    test_loss();
    test_rematch();
    test_caught();
    puts("mystery_jobin_test: all passed");
    return 0;
}
