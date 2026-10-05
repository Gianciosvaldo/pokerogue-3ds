/* C test driver for Task 71: BerryModifier triggers and consumption */
#include "pr/engine.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *locales_dir(void)
{
    const char *d = getenv("TURNTEST_LOCALES");
    return d ? d : "/home/claude/pokerogue/locales/en";
}

static const char *locale_get(void *ctx, const char *file, int fallback, size_t *len)
{
    (void)ctx;
    static struct { char name[64]; char *text; size_t len; } cache[96];
    static int n_cache;
    if (fallback) return NULL;
    for (int i = 0; i < n_cache; i++)
        if (!strcmp(cache[i].name, file)) { *len = cache[i].len; return cache[i].text; }
    if (n_cache >= 96) return NULL;
    char path[512];
    snprintf(path, sizeof path, "%s/%s.json", locales_dir(), file);
    FILE *f = fopen(path, "rb");
    char *text = NULL;
    size_t n = 0;
    if (f) {
        fseek(f, 0, SEEK_END);
        long sz = ftell(f);
        fseek(f, 0, SEEK_SET);
        text = malloc((size_t)sz + 1);
        n = fread(text, 1, (size_t)sz, f);
        text[n] = 0;
        fclose(f);
        static const char *const hidden[] = {"\n  \"return\": ", "\n  \"static\": "};
        for (int i = 0; i < 2; i++) {
            char *k = strstr(text, hidden[i]);
            if (k) k[4] = '_';
        }
    }
    snprintf(cache[n_cache].name, sizeof cache[n_cache].name, "%s", file);
    cache[n_cache].text = text;
    cache[n_cache].len = n;
    n_cache++;
    *len = n;
    return text;
}

static FILE *in;
static int value(void) { int v; if (fscanf(in, "%d", &v) != 1) exit(2); return v; }
static void state(pr_rng *r) {
    char s[256];
    if (fscanf(in, " %255s", s) != 1 || sscanf(s, "!rnd,%lf,%lf,%lf,%lf", &r->c, &r->s0, &r->s1, &r->s2) != 4) exit(2);
}

int main(int argc, char **argv)
{
    bool mutate_stat = false, mutate_stack = false;
    if (argc < 2 || argc > 3) return 2;
    if (argc == 3) {
        if (!strcmp(argv[2], "--mutate-stat")) mutate_stat = true;
        else if (!strcmp(argv[2], "--mutate-stack")) mutate_stack = true;
        else return 2;
    }
    if (!(in = fopen(argv[1], "r"))) return 2;

    char tag;
    if (fscanf(in, " %c", &tag) != 1 || tag != 'V' || value() != 1) return 2;
    int n = value(), bad = 0;
    if (n < 1 || n > 100000) return 2;

    int consumed[12] = {0}, rejected[12] = {0};

    for (int i = 0; i < n; i++) {
        pr_battle bt = {0}; pr_engine e = {0}; pr_pokemon p = {0}, enemy = {0};
        int is_player = value();
        int berry_type = value();
        int hp = value();
        int max_hp = value();
        int status = value();
        int confused = value();
        int crit_boost = value();
        int attack_res = value();
        int ability = value();
        int target_pp_used = value();
        int target_move_pp = value();

        p.species = enemy.species = SPECIES_ID_MAGIKARP;
        p.id = 10; enemy.id = 20;
        p.is_player = true; enemy.has_trainer = true;
        p.hp = enemy.hp = 100;
        p.on_field = enemy.on_field = true;
        p.ability = enemy.ability = ABILITY_ID_BALL_FETCH;
        p.ability_override = enemy.ability_override = -1;
        p.fusion_ability_index = enemy.fusion_ability_index = -1;

        pr_pokemon *target = is_player ? &p : &enemy;
        pr_pokemon *user = is_player ? &enemy : &p;

        target->hp = hp;
        target->stats[STAT_HP] = max_hp;
        target->status = status;
        target->ability = ability;
        target->stats[STAT_SPD] = 20; user->stats[STAT_SPD] = 10;

        target->moves[0] = MOVE_ID_TACKLE;
        target->n_moves = 1;
        target->pp_used[0] = (uint8_t)target_pp_used;
        target->max_pp_override[0] = target_move_pp;

        if (confused) pr_add_tag(&bt, NULL, target, BATTLER_TAG_TYPE_CONFUSED, 2, 0, -1);
        if (crit_boost) pr_add_tag(&bt, NULL, target, BATTLER_TAG_TYPE_CRIT_BOOST, 0, 0, -1);

        if (attack_res > 0) {
            pr_attack_record r = {.result = (uint8_t)attack_res, .damage = 20, .source_id = user->id};
            target->attacks_received[0] = r;
            target->n_attacks_received = 1;
        }

        /* Stat enum in TS is ATK=1..EVA=7, C stat_stages array is 0..6 for ATK..EVA */
        for (int s = 0; s < 7; s++) {
            target->stat_stages[s] = (int8_t)value();
        }

        pr_seed_from_ascii(&bt.wave_seed, "berry-wave");
        bt.field[0] = &p; bt.field[2] = &enemy; bt.engine = &e; e.current = -1; e.n_levels = 1;

        int side = is_player ? 0 : 1;
        bt.n_mods[side] = 1;
        pr_modifier *m = &bt.mods[side][0];
        m->cls = PR_MOD_BERRY;
        m->berry_type = berry_type;
        m->stack_count = 1;
        m->pokemon_id = target->id;

        state(&bt.rng);
        pr_rng expected_rng = {0}; state(&expected_rng);

        bool should_apply = pr_berry_should_apply(&bt, target, m);
        bool applied = false;
        if (should_apply) {
            pr_apply_berry(&bt, &e, target, m);
            applied = true;
            consumed[berry_type]++;
        } else {
            rejected[berry_type]++;
        }

        if (mutate_stat) target->stat_stages[STAT_ATK - 1] += 1;
        if (mutate_stack && should_apply) target->hp += 1;

        int exp_should = value();
        int exp_applied = value();
        int exp_hp = value();
        int exp_status = value();
        int exp_crit_tag = value();
        int exp_unburden = value();
        int exp_pp_used = value();

        int exp_stats[7];
        for (int s = 0; s < 7; s++) exp_stats[s] = value();

        int fail = (should_apply != exp_should) || (applied != exp_applied) ||
                   (target->hp != exp_hp) || (target->status != exp_status) ||
                   (pr_has_battler_tag(&bt, target, BATTLER_TAG_TYPE_CRIT_BOOST) != exp_crit_tag) ||
                   (!!pr_get_battler_tag(target, BATTLER_TAG_TYPE_UNBURDEN) != exp_unburden) ||
                   (target->pp_used[0] != exp_pp_used);

        for (int s = 0; s < 7; s++) {
            if (target->stat_stages[s] != exp_stats[s]) fail = 1;
        }

        int messages = value();
        if (messages < 0 || messages > 8) return 2;
        const pr_msg *actual[8]; int nm = 0;
        for (int k = 0; k < PR_MAX_PHASES; k++) {
            if (e.used[k] && e.pool[k].type == PH_MESSAGE) {
                if (nm >= 8) return 2;
                actual[nm++] = &e.pool[k].msg;
            }
        }
        fail |= (nm != messages);

        for (int j = 0; j < messages; j++) {
            int len = value();
            if (len < 0 || len > 1000) return 2;
            char text[1001], rendered[1001] = {0};
            for (int k = 0; k < len; k++) text[k] = (char)value();
            text[len] = 0;
            static const pr_text_source src = {locale_get, NULL, "en"};
            if (j < nm) pr_msg_format(actual[j], &src, rendered, sizeof rendered);
            if (strcmp(text, rendered)) {
                fail = 1;
                if (bad < 8) fprintf(stderr, "message [%s] / [%s]\n", text, rendered);
            }
        }

        fail |= (bt.rng.c != expected_rng.c || bt.rng.s0 != expected_rng.s0 ||
                 bt.rng.s1 != expected_rng.s1 || bt.rng.s2 != expected_rng.s2);

        if (fail) {
            if (bad < 8) {
                fprintf(stderr, "FAIL case %d: type=%d should=%d/%d applied=%d/%d hp=%d/%d status=%d/%d crit=%d/%d unb=%d/%d pp=%d/%d msgs=%d/%d rng_c=%.17g/%.17g\n",
                        i, berry_type, should_apply, exp_should, applied, exp_applied, target->hp, exp_hp, target->status, exp_status,
                        pr_has_battler_tag(&bt, target, BATTLER_TAG_TYPE_CRIT_BOOST), exp_crit_tag,
                        !!pr_get_battler_tag(target, BATTLER_TAG_TYPE_UNBURDEN), exp_unburden,
                        target->pp_used[0], exp_pp_used,
                        nm, messages, bt.rng.c, expected_rng.c);
                for (int s = 0; s < 7; s++) fprintf(stderr, " stat[%d]=%d/%d", s, target->stat_stages[s], exp_stats[s]);
                fprintf(stderr, "\n");
            }
            bad++;
        }
    }

    if (fscanf(in, " %c", &tag) != 1 || tag != 'Z' || value() != n || fscanf(in, " %c", &tag) == 1) return 2;

    printf("Berry: %d cases, %d identical, %d mismatches\n", n, n - bad, bad);
    for (int b = 0; b <= 10; b++) {
        printf("Berry %d: consumed %d, rejected %d\n", b, consumed[b], rejected[b]);
    }

    return bad ? 1 : 0;
}
