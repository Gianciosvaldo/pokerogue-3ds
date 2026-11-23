/* Challenge Mode: the values' admission, the save section, and the win's ribbons (game-core/CHALLENGE-VALIDATION.md).
 * The starter rules (cost, select view, starter Pokemon) are checked against upstream by tools/challengestartersdiff.sh; this is the part
 * with no upstream function to replay: what game-core accepts and refuses. */
#include "../src/internal.h"
#include <stdio.h>
#include <stdlib.h>

static gc_run run, loaded;
static gc_system sys, before;
static unsigned char saved[GC_SAVE_MAX], mutated[GC_SAVE_MAX];
static int checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr, "Challenge host line %d: %s\n", __LINE__, #x); exit(1); } } while (0)

static uint32_t hashname(const char *s) { uint32_t v = 2166136261u; for (; *s; s++) v = (v ^ (unsigned char)*s) * 16777619u; return v; }
static uint32_t u32(const unsigned char *p) { uint32_t n; memcpy(&n, p, 4); return n; }
static void p32(unsigned char *p, uint32_t n) { memcpy(p, &n, 4); }
static uint32_t crc(const unsigned char *p, size_t n)
{
    uint32_t c = ~0u;
    for (size_t i = 0; i < n; i++) { c ^= p[i]; for (int b = 0; b < 8; b++) c = c >> 1 ^ ((c & 1) ? 0xedb88320u : 0); }
    return ~c;
}
static size_t section_at(const unsigned char *buf, size_t n, const char *name)
{
    for (size_t at = 20; at + 8 <= n;) {
        const uint32_t len = u32(buf + at + 4);
        CHECK(len <= n - at - 8);
        if (u32(buf + at) == hashname(name)) return at;
        at += 8 + len;
    }
    return 0;
}
static void reseal(size_t n) { p32(mutated + 12, (uint32_t)(n - 20)); p32(mutated + 16, crc(mutated + 20, n - 20)); }

static void new_run(int mode)
{
    gc_system_init_defaults(&sys);
    gc_run_new(&run, mode, "challenge-host", 777);
    run.sys = &sys;
}
static gc_starter bulbasaur(void)
{
    gc_starter st = {.species = SPECIES_ID_BULBASAUR, .tera_type = -1};
    memset(st.ivs, 31, 6);
    return st;
}
/* a profile with every nature of Bulbasaur unlocked, so the starter's own nature is the only thing that can refuse it */
static void unlock_natures(void) { sys.dex[SPECIES_ID_BULBASAUR].nature_attr = 0x3FFFFFEu; }
static gc_challenge_set fresh(int v) { gc_challenge_set s; memset(&s, 0, sizeof s); s.value[CHALLENGES_FRESH_START] = (uint8_t)v; return s; }
static void finish(bool victory) { gc_rphase phase = gc_rp(RP_GAME_OVER, victory, 0, 0); CHECK(!gc_phase_start(&run, &phase)); }

static void admission(void)
{
    const char *why = NULL;
    /* a run that is not Challenge Mode takes no values */
    new_run(GAME_MODES_CLASSIC);
    gc_challenge_set f1 = fresh(1);
    CHECK(!gc_run_set_challenges(&run, &f1, &why) && !run.run.has_challenges);
    /* the values are checked against their maxima, and only the challenges played so far are accepted */
    new_run(GAME_MODES_CHALLENGE);
    unlock_natures();
    CHECK(!run.run.has_challenges && gc_challenge_max(CHALLENGES_FRESH_START) == 2 && gc_challenge_max(CHALLENGES_SINGLE_TYPE) == 18);
    gc_challenge_set bad = fresh(3);
    CHECK(!gc_run_set_challenges(&run, &bad, &why) && !run.run.has_challenges);
    gc_challenge_set unplayed = fresh(0);
    unplayed.value[CHALLENGES_MOVESET_RANDOMIZER] = 1;   /* not played yet */
    CHECK(gc_challenge_set_valid(&unplayed, NULL) && !gc_run_set_challenges(&run, &unplayed, &why) && !run.run.has_challenges);
    /* the battle rules are played: Inverse Battle, Flip Stat and Passives reach run-core's flags and the run's battle (run->bt computes the
     * stats and passives of Pokemon made outside a battle) */
    gc_challenge_set rules = fresh(0);
    rules.value[CHALLENGES_INVERSE_BATTLE] = 1;
    rules.value[CHALLENGES_FLIP_STAT] = 1;
    rules.value[CHALLENGES_PASSIVES] = 2;
    CHECK(gc_run_set_challenges(&run, &rules, &why) && !gc_challenges_unplayed(&run));
    const uint32_t rule_bits = (1u << CHALLENGES_INVERSE_BATTLE) | (1u << CHALLENGES_FLIP_STAT) | (1u << CHALLENGES_PASSIVES);
    CHECK(run.run.challenge_flags == rule_bits && run.run.challenge_passives == 2);
    CHECK(run.bt.challenge_flags == rule_bits && run.bt.challenge_flags_known && run.bt.challenge_passives == 2);
    rules.value[CHALLENGES_PASSIVES] = 1;
    CHECK(gc_run_set_challenges(&run, &rules, &why) && run.run.challenge_passives == 1 && run.bt.challenge_passives == 1);
    gc_challenge_set off = fresh(0);
    CHECK(gc_run_set_challenges(&run, &off, &why) && run.bt.challenge_flags == 0 && run.bt.challenge_passives == 0);
    gc_challenge_set lower = fresh(0);
    lower.value[CHALLENGES_LOWER_STARTER_POINTS] = 1;   /* not in upstream's allChallenges: no value */
    CHECK(!gc_challenge_set_valid(&lower, NULL));
    CHECK(!gc_run_set_challenges(&run, NULL, &why));
    /* Fresh Start 1 and 2 are taken and read by run-core's flags; 0 clears them */
    gc_challenge_set f2 = fresh(2);
    CHECK(gc_run_set_challenges(&run, &f2, &why));
    CHECK(run.run.has_challenges && run.run.challenge_flags == (1u << CHALLENGES_FRESH_START) && run.run.challenge_fresh_start == 2);
    CHECK(!gc_challenges_unplayed(&run));
    gc_challenge_set none = fresh(0);
    CHECK(gc_run_set_challenges(&run, &none, &why) && !run.run.has_challenges && run.run.challenge_flags == 0 && !gc_challenges_unplayed(&run));
    /* Challenge Mode with a set nobody told game-core about stays refused (the callers from before the values) */
    run.run.has_challenges = true;
    CHECK(gc_challenges_unplayed(&run));
    run.run.has_challenges = false;
    /* never after the starters */
    CHECK(gc_run_set_challenges(&run, &f1, &why));
    gc_starter st = bulbasaur();
    CHECK(gc_run_set_starters(&run, &st, 1, &why));
    CHECK(!gc_run_set_challenges(&run, &f2, &why) && run.challenges.value[CHALLENGES_FRESH_START] == 1);
}

static void starters(void)
{
    const char *why = NULL;
    new_run(GAME_MODES_CHALLENGE);
    unlock_natures();
    gc_challenge_set f1 = fresh(1);
    CHECK(gc_run_set_challenges(&run, &f1, &why));
    gc_starter st = bulbasaur();
    /* Fresh Start keeps the neutral natures (Hardy, Docile, Serious, Bashful, Quirky) and no other */
    for (int nat = 0; nat < 25; nat++) {
        new_run(GAME_MODES_CHALLENGE);
        unlock_natures();
        CHECK(gc_run_set_challenges(&run, &f1, &why));
        gc_starter n = bulbasaur();
        n.nature = (uint8_t)nat;
        CHECK(gc_run_set_starters(&run, &n, 1, &why) == (nat % 6 == 0));
        if (nat % 6 != 0) CHECK(!strcmp(why, "starterSelectUiHandler:lockedNature"));
    }
    new_run(GAME_MODES_CHALLENGE);
    unlock_natures();
    CHECK(gc_run_set_challenges(&run, &f1, &why));
    /* the hidden ability is not unlocked under Fresh Start, and no more is a passive */
    gc_starter hidden = bulbasaur();
    hidden.ability_index = 2;
    CHECK(!gc_run_set_starters(&run, &hidden, 1, &why));
    CHECK(gc_run_set_starters(&run, &st, 1, &why));
    const gc_mon *m = &run.party[0];
    CHECK(run.n_party == 1 && m->luck == 0 && !m->b.passive_flag && m->b.ivs[0] == 15 && m->b.ivs[5] == 15);
    CHECK(m->b.tera_type == gc_form(SPECIES_ID_BULBASAUR, 0)->type1);
    CHECK(m->b.n_moves >= 1 && m->b.n_moves <= 4);
    /* a Classic run and a Challenge run without Fresh Start leave the starter as it came */
    new_run(GAME_MODES_CHALLENGE);
    unlock_natures();
    gc_challenge_set zero = fresh(0);
    CHECK(gc_run_set_challenges(&run, &zero, &why));
    CHECK(gc_run_set_starters(&run, &st, 1, &why) && run.party[0].b.ivs[0] == 31);
    new_run(GAME_MODES_CLASSIC);
    unlock_natures();
    CHECK(gc_run_set_starters(&run, &st, 1, &why) && run.party[0].b.ivs[0] == 31);
}

static void party_rule(void)
{
    const char *why = NULL;
    gc_challenge_set f1 = fresh(1), off = fresh(0);
    gc_starter bulb = bulbasaur(), mewtwo = bulbasaur();
    mewtwo.species = SPECIES_ID_MEWTWO;
    CHECK(gc_challenge_starter_valid(&f1, SPECIES_ID_BULBASAUR, 0, false) && !gc_challenge_starter_valid(&f1, SPECIES_ID_MEWTWO, 0, false));
    CHECK(!gc_challenge_starter_valid(&f1, SPECIES_ID_MEWTWO, 0, true));   /* no evolution to rescue it (the soft step proper comes with Single Type) */
    CHECK(gc_challenge_party_ok(&f1, &bulb, 1, &why));
    CHECK(!gc_challenge_party_ok(&f1, &mewtwo, 1, &why));
    gc_starter pair[2] = {bulb, mewtwo};
    CHECK(!gc_challenge_party_ok(&f1, pair, 2, &why) && !strcmp(why, "a starter this challenge does not allow"));
    CHECK(gc_challenge_party_ok(&off, pair, 2, &why));   /* no Fresh Start: every starter is valid */
}

static void ribbons(void)
{
    typedef struct { const char *what; uint8_t v[GC_CHALLENGE_COUNT]; uint64_t want; } rc;
#define V(...) { __VA_ARGS__ }
    static const rc table[] = {
        {"no challenge", V(0), GC_RIBBON_CLASSIC},
        {"Moveset Randomizer: none", V([CHALLENGES_MOVESET_RANDOMIZER] = 1, [CHALLENGES_FRESH_START] = 1), 0},
        {"Fresh Start", V([CHALLENGES_FRESH_START] = 1), GC_RIBBON_CLASSIC | GC_RIBBON_FRESH_START},
        {"Nuzlocke", V([CHALLENGES_FRESH_START] = 2, [CHALLENGES_LIMITED_CATCH] = 1, [CHALLENGES_HARDCORE] = 1),
         GC_RIBBON_CLASSIC | GC_RIBBON_FRESH_START | GC_RIBBON_LIMITED_CATCH | GC_RIBBON_HARDCORE | GC_RIBBON_NUZLOCKE},
        {"Fresh Start and Limited Catch, no Hardcore: no Nuzlocke", V([CHALLENGES_FRESH_START] = 1, [CHALLENGES_LIMITED_CATCH] = 1),
         GC_RIBBON_CLASSIC | GC_RIBBON_FRESH_START | GC_RIBBON_LIMITED_CATCH},
        {"Fresh Start and Hardcore, no Limited Catch: no Nuzlocke", V([CHALLENGES_FRESH_START] = 1, [CHALLENGES_HARDCORE] = 1),
         GC_RIBBON_CLASSIC | GC_RIBBON_FRESH_START | GC_RIBBON_HARDCORE},
        {"Limited Catch and Hardcore, no Fresh Start", V([CHALLENGES_LIMITED_CATCH] = 1, [CHALLENGES_HARDCORE] = 1),
         GC_RIBBON_CLASSIC | GC_RIBBON_LIMITED_CATCH | GC_RIBBON_HARDCORE},
        {"Flip Stat keeps only its own", V([CHALLENGES_FLIP_STAT] = 1, [CHALLENGES_FRESH_START] = 1), GC_RIBBON_FLIP_STATS},
        {"Inverse Battle keeps only its own", V([CHALLENGES_INVERSE_BATTLE] = 1, [CHALLENGES_HARDCORE] = 1), GC_RIBBON_INVERSE},
        {"Passives on all", V([CHALLENGES_PASSIVES] = 2, [CHALLENGES_HARDCORE] = 1), GC_RIBBON_PASSIVE},
        {"Passives on starters", V([CHALLENGES_PASSIVES] = 1), GC_RIBBON_CLASSIC | GC_RIBBON_PASSIVE},
        {"Single Generation 3", V([CHALLENGES_SINGLE_GENERATION] = 3), GC_RIBBON_CLASSIC | (GC_RIBBON_MONO_GEN_1 << 2)},
        {"Single Type 5", V([CHALLENGES_SINGLE_TYPE] = 5), GC_RIBBON_CLASSIC | (GC_RIBBON_MONO_NORMAL << 4)},
        {"Limited Support 3", V([CHALLENGES_LIMITED_SUPPORT] = 3), GC_RIBBON_CLASSIC | GC_RIBBON_NO_HEAL | GC_RIBBON_NO_SHOP | GC_RIBBON_NO_SUPPORT},
    };
    new_run(GAME_MODES_CHALLENGE);
    for (size_t i = 0; i < sizeof table / sizeof table[0]; i++) {
        memcpy(run.challenges.value, table[i].v, GC_CHALLENGE_COUNT);
        if (gc_challenge_ribbon_flags(&run) != table[i].want) { fprintf(stderr, "ribbons: %s: %llx, expected %llx\n", table[i].what, (unsigned long long)gc_challenge_ribbon_flags(&run), (unsigned long long)table[i].want); exit(1); }
        checks++;
    }
    new_run(GAME_MODES_CLASSIC);
    CHECK(gc_challenge_ribbon_flags(&run) == GC_RIBBON_CLASSIC);
}

static void setup_win(const gc_challenge_set *set)
{
    new_run(set ? GAME_MODES_CHALLENGE : GAME_MODES_CLASSIC);
    unlock_natures();
    const char *why;
    if (set) CHECK(gc_run_set_challenges(&run, set, &why));
    gc_starter st = bulbasaur();
    CHECK(gc_run_set_starters(&run, &st, 1, &why));
    run.sys = &sys;
    run.wave = 200;
}

static void win(void)
{
    /* a Classic win, as before */
    setup_win(NULL);
    finish(true);
    CHECK(sys.stats.classic_wins == 1 && sys.dex[1].ribbon_flags == GC_RIBBON_CLASSIC);
    /* a Challenge win counts as a Classic one and gives the challenge's ribbons */
    gc_challenge_set f1 = fresh(1);
    setup_win(&f1);
    finish(true);
    CHECK(sys.stats.classic_wins == 1 && run.terminal_result == 2);
    CHECK(sys.dex[1].ribbon_flags == (GC_RIBBON_CLASSIC | GC_RIBBON_FRESH_START));
    /* a win with a challenge that is not played is refused, and never saved */
    setup_win(&f1);
    run.challenges.value[CHALLENGES_MOVESET_RANDOMIZER] = 1;
    gc_challenge_sync(&run);
    before = sys;
    finish(true);
    CHECK(!memcmp(&sys, &before, sizeof sys) && run.terminal_result == 0 && !gc_run_can_save(&run));
}

static void save(void)
{
    const char *why = NULL;
    gc_challenge_set f2 = fresh(2);
    setup_win(&f2);
    run.wave = 5;
    CHECK(gc_run_can_save(&run));
    size_t n = gc_run_save(&run, saved, sizeof saved);
    CHECK(n > 0 && gc_run_load(&loaded, saved, n, &why));
    CHECK(loaded.run.mode == GAME_MODES_CHALLENGE && loaded.challenges.value[CHALLENGES_FRESH_START] == 2);
    CHECK(loaded.run.has_challenges && loaded.run.challenge_flags == (1u << CHALLENGES_FRESH_START) && loaded.run.challenge_fresh_start == 2);
    CHECK(loaded.load_dropped == 0);
    {
        gc_challenge_set r = fresh(1);
        r.value[CHALLENGES_INVERSE_BATTLE] = 1;
        r.value[CHALLENGES_FLIP_STAT] = 1;
        r.value[CHALLENGES_PASSIVES] = 2;
        setup_win(&r);
        run.wave = 5;
        size_t rn = gc_run_save(&run, mutated, sizeof mutated);
        CHECK(rn > 0 && gc_run_load(&loaded, mutated, rn, &why));
        CHECK(loaded.challenges.value[CHALLENGES_INVERSE_BATTLE] == 1 && loaded.challenges.value[CHALLENGES_FLIP_STAT] == 1 &&
              loaded.challenges.value[CHALLENGES_PASSIVES] == 2 && loaded.run.challenge_passives == 2);
        CHECK(loaded.bt.challenge_flags == loaded.run.challenge_flags && loaded.bt.challenge_passives == 2);
        setup_win(&f2);
        run.wave = 5;
        n = gc_run_save(&run, saved, sizeof saved);
    }
    /* the section is the Challenge Mode's alone: a Classic save has none, a Challenge one must have it */
    size_t at = section_at(saved, n, "challenges");
    CHECK(at != 0);
    /* a value above its maximum, a challenge this build does not play, and a section that lost its row are all refused */
    memcpy(mutated, saved, n); mutated[at + 18 + CHALLENGES_FRESH_START] = 3; reseal(n);
    CHECK(!gc_run_load(&loaded, mutated, n, &why));
    memcpy(mutated, saved, n); mutated[at + 18 + CHALLENGES_MOVESET_RANDOMIZER] = 1; reseal(n);
    CHECK(!gc_run_load(&loaded, mutated, n, &why));
    memcpy(mutated, saved, n); mutated[at + 18 + CHALLENGES_LOWER_STARTER_POINTS] = 1; reseal(n);
    CHECK(!gc_run_load(&loaded, mutated, n, &why));
    memcpy(mutated, saved, n); p32(mutated + at, hashname("not-challenges")); reseal(n);
    CHECK(!gc_run_load(&loaded, mutated, n, &why));
    /* a Classic save: no section, and the same bytes with a Challenge Mode flag are not a Challenge run */
    setup_win(NULL);
    run.wave = 5;
    n = gc_run_save(&run, saved, sizeof saved);
    CHECK(n > 0 && section_at(saved, n, "challenges") == 0 && gc_run_load(&loaded, saved, n, &why) && loaded.run.mode == GAME_MODES_CLASSIC && !loaded.run.has_challenges);
}

/* ---- the run rules of Limited Support, Hardcore and Limited Catch (game-core/CHALLENGE-VALIDATION.md, "PR 3") ---- */
static gc_challenge_set only(int id, int v) { gc_challenge_set s; memset(&s, 0, sizeof s); s.value[id] = (uint8_t)v; return s; }
static void two_starters(const gc_challenge_set *set)
{
    new_run(GAME_MODES_CHALLENGE);
    unlock_natures();
    const char *why;
    CHECK(gc_run_set_challenges(&run, set, &why));
    gc_starter st[2] = {bulbasaur(), bulbasaur()};
    st[1].species = SPECIES_ID_CHARMANDER;
    sys.dex[SPECIES_ID_CHARMANDER].nature_attr = 0x3FFFFFEu;
    if (!gc_run_set_starters(&run, st, 2, &why)) fprintf(stderr, "starters: %s\n", why);
    CHECK(run.n_party == 2);
    run.sys = &sys;
    gc_rq_clear(&run);
}
static int next_phase(void) { gc_rphase p; return gc_rq_next(&gc_int(&run)->rq, &p) ? p.type : -1; }
static int shop_size(int wave)
{
    run.wave = wave;
    gc_rphase p = gc_rp(RP_SELECT_MODIFIER, 0, 0, 0);
    CHECK(!gc_phase_start(&run, &p) && run.screen.kind == GC_SCREEN_REWARD && run.screen.n_options > 0);
    return run.screen.n_shop;
}
static bool has_shop_item(const pr_shop_option *o, int n, int item) { for (int i = 0; i < n; i++) if (o[i].item == item) return true; return false; }

static void run_rules(void)
{
    /* Limited Support: 1 no heal (the shop stays), 2 no shop (the heal stays), 3 neither. Without the heal, the biome's last wave queues a
     * plain reward screen where the PartyHealPhase was; the X0 wave has no shop */
    for (int v = 0; v <= 3; v++) {
        gc_challenge_set set = only(CHALLENGES_LIMITED_SUPPORT, v);
        two_starters(&set);
        const bool heal = v == 0 || v == 2, shop = v <= 1;
        CHECK(run.run.challenge_limited_support == v);
        CHECK(pr_run_get_party_heal_status(&run.run) == heal && pr_run_get_shop_status(&run.run) == shop);
        CHECK((shop_size(12) > 0) == shop);
        run.wave = 10;
        run.party[0].b.hp = 1;
        gc_rq_clear(&run);
        gc_rphase p = gc_rp(RP_SELECT_BIOME, 0, 0, 0);
        CHECK(gc_phase_start(&run, &p));
        /* unshiftNew keeps the order of the calls: the heal (or the screen), then SwitchBiomePhase */
        CHECK(next_phase() == (heal ? RP_PARTY_HEAL : RP_SELECT_MODIFIER));
        CHECK(next_phase() == RP_SWITCH_BIOME);
        CHECK(next_phase() == -1);
        if (!heal) {
            CHECK(shop_size(10) == 0);
            CHECK(run.party[0].b.hp == 1);
        }
        /* a wave that is not a biome's last neither heals nor adds a screen */
        run.wave = 12;
        gc_rq_clear(&run);
        p = gc_rp(RP_SELECT_BIOME, 0, 0, 0);
        if (gc_phase_start(&run, &p)) {
            CHECK(next_phase() == RP_SWITCH_BIOME);
            CHECK(next_phase() == -1);
        }
    }

    /* Hardcore: no revive group in the shop, the party heal leaves a fainted Pokemon fainted (and heals the rest), Revival Blessing is not
     * usable by the player's Pokemon nor callable by any (CallMoveAttrWithBanlist.isMoveAllowed) */
    for (int hardcore = 0; hardcore < 2; hardcore++) {
        gc_challenge_set set = only(CHALLENGES_HARDCORE, hardcore);
        two_starters(&set);
        CHECK(pr_run_prevent_revive(&run.run) == (bool)hardcore);
        pr_shop_option o[PR_SHOP_ITEM_COUNT_];
        const int n = pr_run_get_player_shop_options(&run.run, 199, 1000, o);
        CHECK(n > 0 && !run.run.unported);
        CHECK(has_shop_item(o, n, PR_SHOP_REVIVE) == !hardcore && has_shop_item(o, n, PR_SHOP_MAX_REVIVE) == !hardcore
              && has_shop_item(o, n, PR_SHOP_SACRED_ASH) == !hardcore && has_shop_item(o, n, PR_SHOP_POTION));
        pr_pokemon *a = &run.party[0].b, *b = &run.party[1].b;
        a->hp = 0;
        a->status = STATUS_EFFECT_FAINT;
        b->hp = 1;
        b->status = STATUS_EFFECT_POISON;
        PR_PP_USED(a)[0] = PR_PP_USED(b)[0] = 3;
        pr_sync_moveset_aliases(a);
        pr_sync_moveset_aliases(b);
        gc_rphase heal = gc_rp(RP_PARTY_HEAL, 0, 0, 0);
        run.bt.player_teras_used = 1;
        CHECK(gc_phase_start(&run, &heal));
        CHECK(run.bt.player_teras_used == 0);   /* playerTerasUsed resets either way */
        CHECK(b->hp == b->stats[0] && b->status == STATUS_EFFECT_NONE && PR_PP_USED(b)[0] == 0);
        if (hardcore) CHECK(a->hp == 0 && a->status == STATUS_EFFECT_FAINT && PR_PP_USED(a)[0] == 3);
        else CHECK(a->hp == a->stats[0] && a->status == STATUS_EFFECT_NONE && PR_PP_USED(a)[0] == 0);

        CHECK(pr_challenge_move_allowed(&run.bt, MOVE_ID_REVIVAL_BLESSING) == !hardcore && pr_challenge_move_allowed(&run.bt, MOVE_ID_TACKLE));
        PR_MOVES(b)[0] = MOVE_ID_REVIVAL_BLESSING;
        pr_sync_moveset_aliases(b);
        pr_msg why = {0};
        CHECK(pr_move_usable(&run.bt, b, 0, false, true, &why) == !hardcore);
        if (hardcore) CHECK(why.key && !strcmp(why.key, "battle:moveCannotUseChallenge"));
        b->is_player = false;   /* the enemy's Pokemon: PokemonMove.isUsable asks the challenge only for the player's */
        CHECK(pr_move_usable(&run.bt, b, 0, false, true, &why));
        b->is_player = true;
        /* Copycat: the last move, unless the challenge bars it */
        const pr_move *cc = &pr_moves[MOVE_ID_COPYCAT];
        const pr_attr *copy = NULL;
        for (unsigned i = 0; i < cc->nattr; i++) if (pr_attrs[cc->attr0 + i].cls == PR_AC_CopyMoveAttr) copy = &pr_attrs[cc->attr0 + i];
        CHECK(copy);
        run.bt.last_move = MOVE_ID_REVIVAL_BLESSING;
        CHECK(pr_call_move_get(&run.bt, NULL, MOVE_ID_COPYCAT, copy, b, NULL) == (hardcore ? MOVE_ID_NONE : MOVE_ID_REVIVAL_BLESSING));
    }
}

int main(void)
{
    admission();
    starters();
    party_rule();
    ribbons();
    win();
    save();
    run_rules();
    printf("Challenge host: %d checks passed\n", checks);
    return 0;
}
