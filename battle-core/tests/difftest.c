#define _POSIX_C_SOURCE 200809L
/* C half of the differential test: replays cases written by
 * difftest.producer.test.ts (run inside upstream) and compares every answer.
 * A case whose computation touched an unported attribute is counted as
 * "not covered" (listed by class), never as a pass. */
#include "pr/battle.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MON_FIELDS 49

/* The counterfactual replays: state left out to see whether the answer depends on it */
enum { CF_TAGS, CF_PASSIVES, CF_HISTORY, CF_COUNT_ };

static const char *const battler_tag_names[] = {BATTLER_TAG_TYPE_VALUES};
static const char *const arena_tag_names[] = {ARENA_TAG_TYPE_VALUES};

static int name_index(const char *const *names, int n, const char *s)
{
    for (int i = 0; i < n; i++)
        if (!strcmp(names[i], s)) return i;
    return -1;
}

/* Is `name` one of the '<'-separated class names in the upstream prototype chain? */
static bool chain_has(const char *chain, const char *name)
{
    size_t n = strlen(name);
    for (const char *c = chain; *c;) {
        const char *e = strchr(c, '<');
        size_t len = e ? (size_t)(e - c) : strlen(c);
        if (len == n && !strncmp(c, name, n)) return true;
        if (!e) break;
        c = e + 1;
    }
    return false;
}

/* JS number equality for the expected answers, with NaN matching NaN */
static bool same(double c, double js)
{
    return c == js || (c != c && js != js);
}

static char *next_tok(char **save)
{
    char *t = strtok_r(NULL, " \n", save);
    if (!t) { fprintf(stderr, "short line\n"); exit(2); }
    return t;
}

static int read_mon(char **save, pr_pokemon *p)
{
    long long v[MON_FIELDS];
    for (int i = 0; i < MON_FIELDS; i++) {
        char *t = strtok_r(NULL, " \n", save);
        if (!t) return 0;
        v[i] = strtoll(t, NULL, 10);
    }
    memset(p, 0, sizeof *p);
    p->species = (uint16_t)v[0];
    p->form = (uint8_t)v[1];
    p->level = (uint8_t)v[2];
    for (int i = 0; i < 6; i++) p->ivs[i] = (uint8_t)v[3 + i];
    p->nature = (uint8_t)v[9];
    for (int i = 0; i < 7; i++) p->stat_stages[i] = (int8_t)v[10 + i];
    p->status = (uint8_t)v[17];
    p->terastallized = v[18];
    p->tera_type = (int8_t)v[19];
    p->hp = v[20];
    p->ability = (uint16_t)v[21];
    p->gender = (int8_t)v[22];
    p->is_boss = v[23];
    p->stellar_types_boosted = (uint32_t)v[24];
    p->summon_hit_count = (uint8_t)v[25];
    p->n_move_history = (uint16_t)v[26];      /* the Hm/Ha lines fill the entries */
    p->n_attacks_received = (uint16_t)v[27];
    p->acted = v[28];
    p->stat_stages_decreased = v[29];
    p->joined_round = v[30];
    p->damage_taken = v[31];
    p->combining_pledge = (uint16_t)v[32];
    p->hit_count = (int8_t)v[33];
    p->hits_left = (int8_t)v[34];
    p->n_tags = (uint8_t)v[35]; /* the T lines fill tags[] */
    p->friendship = (uint8_t)v[36];
    p->has_trainer = v[37];
    for (int i = 0; i < 4; i++) {
        p->moves[i] = (uint16_t)v[38 + i];
        p->pp_used[i] = (uint8_t)v[42 + i];
    }
    p->id = (uint32_t)v[46];
    p->passive_flag = v[47];
    p->custom_passive = (int16_t)v[48];
    p->base_ability = ABILITY_ID_BALL_FETCH;
    p->type_override[0] = p->type_override[1] = -2;
    p->turn_move_effectiveness = -1; /* turnData.moveEffectiveness unset */
    return 1;
}

int main(int argc, char **argv)
{
    FILE *f = fopen(argc > 1 ? argv[1] : "cases.txt", "r");
    if (!f) { perror("cases"); return 2; }
    static char line[4096], eline[4096];
    pr_battle bt;
    memset(&bt, 0, sizeof bt);
    int total = 0, covered = 0, pass = 0, fail = 0, stat_fail = 0, state_fail = 0;
    static int uncovered_by[PR_FEAT_COUNT_], deferred_by[PR_FEAT_COUNT_];
    int with_deferred = 0, matters[CF_COUNT_] = {0};
    pr_pokemon A, D;
    int mods_expected[2] = {0, 0}, arena_tags_expected = 0;
    /* battler tags read from T lines before the case line: [0] player (A), [1] enemy (D) */
    static pr_battler_tag tags[2][PR_MAX_BATTLER_TAGS];
    int n_tags[2] = {0, 0};
    bool have_half_hp = false;
    double half_hp = 0;
    /* move histories and attacks received read from Hm/Ha lines: [0] A, [1] D */
    static pr_pokemon hist[2];

    while (fgets(line, sizeof line, f)) {
        char *save;
        char *tag = strtok_r(line, " \n", &save);
        if (!tag) continue;
        if (!strcmp(tag, "Z")) {
            char *seed = strtok_r(NULL, " \n", &save);
            pr_seed_from_ascii(&bt.battle_seed, seed);
            bt.turn = atoi(strtok_r(NULL, " \n", &save));
            bt.battle_rng_live = false;
            continue;
        }
        if (!strcmp(tag, "W")) {
            int w[16];
            char *t;
            bt.turn = atoi(strtok_r(NULL, " \n", &save));
            pr_seed_from_ascii(&bt.wave_seed, strtok_r(NULL, " \n", &save));
            for (int i = 0; i < 15 && (t = strtok_r(NULL, " \n", &save)); i++) w[i] = atoi(t);
            bt.terrain = (uint8_t)w[0];
            bt.player_faints = w[1];
            bt.enemy_faints = w[2];
            /* only the newest entry of each faint history is given */
            for (int k = 0; k < 2; k++) {
                bt.n_faint_history[k] = w[3 + k] >= 0;
                bt.faint_history[k][0].id = 0;
                bt.faint_history[k][0].turn = w[3 + k];
            }
            bt.n_last_turn_order = (uint8_t)w[5];
            arena_tags_expected = w[6];
            bt.n_arena_tags = 0;
            n_tags[0] = n_tags[1] = 0;
            mods_expected[0] = w[7];
            mods_expected[1] = w[8];
            bt.n_mods[0] = bt.n_mods[1] = 0;
            bt.max_exp_level = w[9];
            for (int i = 0; i < 4; i++) bt.commands[i].command = (int8_t)w[10 + i];
            bt.rng_live = false;
            bt.n_party[0] = bt.n_party[1] = 0;
            bt.n_queued_movers = 0;
            have_half_hp = false;
            memset(hist, 0, sizeof hist);
            continue;
        }
        if (!strcmp(tag, "G")) {
            /* G c s0 s1 s2: Phaser.Math.RND.state() */
            bt.rng.c = strtod(next_tok(&save), NULL);
            bt.rng.s0 = strtod(next_tok(&save), NULL);
            bt.rng.s1 = strtod(next_tok(&save), NULL);
            bt.rng.s2 = strtod(next_tok(&save), NULL);
            bt.rng_live = true;
            continue;
        }
        if (!strcmp(tag, "P")) {
            /* P side n (id species status)*n: the party in order, the Pokemon on the field first */
            int side = atoi(next_tok(&save)), n = atoi(next_tok(&save));
            if (side < 0 || side > 1 || n < 0 || n > PR_MAX_PARTY) { fprintf(stderr, "bad P line\n"); return 2; }
            static pr_pokemon party_store[2][PR_MAX_PARTY];
            bt.n_party[side] = (uint8_t)n;
            for (int i = 0; i < n; i++) {
                pr_pokemon *m = &party_store[side][i];
                memset(m, 0, sizeof *m);
                m->id = (uint32_t)strtoul(next_tok(&save), NULL, 10);
                m->species = (uint16_t)atoi(next_tok(&save));
                m->status = (uint8_t)atoi(next_tok(&save));
                bt.party[side][i] = m;
            }
            continue;
        }
        if (!strcmp(tag, "Q")) {
            /* Q n id...: the Pokemon of each queued MovePhase */
            int n = atoi(next_tok(&save));
            if (n < 0 || n > PR_MAX_QUEUED_MOVERS) { fprintf(stderr, "bad Q line\n"); return 2; }
            bt.n_queued_movers = (uint8_t)n;
            for (int i = 0; i < n; i++) bt.queued_movers[i] = (uint32_t)strtoul(next_tok(&save), NULL, 10);
            continue;
        }
        if (!strcmp(tag, "O")) {
            /* O n id...: dynamicQueueManager.getLastTurnOrder() */
            int n = atoi(next_tok(&save));
            if (n < 0 || n > PR_MAX_TURN_ORDER || n != bt.n_last_turn_order) { fprintf(stderr, "bad O line\n"); return 2; }
            for (int i = 0; i < n; i++) bt.last_turn_order[i] = (uint32_t)strtoul(next_tok(&save), NULL, 10);
            continue;
        }
        if (!strcmp(tag, "Hm")) {
            /* Hm who n (move useMode result)*n: summonData.moveHistory, oldest first (result -1 = undefined) */
            int who = atoi(next_tok(&save)), n = atoi(next_tok(&save));
            if (who < 0 || who > 1 || n < 0) { fprintf(stderr, "bad Hm line\n"); return 2; }
            hist[who].n_move_history = 0;
            for (int i = 0; i < n; i++) {
                pr_turn_move tm = {0};
                tm.move = (uint16_t)atoi(next_tok(&save));
                tm.use_mode = (uint8_t)atoi(next_tok(&save));
                int r = atoi(next_tok(&save));
                tm.result = r < 0 ? PR_MOVE_RESULT_UNDEFINED : (uint8_t)r;
                pr_push_move_history(&hist[who], tm);
            }
            continue;
        }
        if (!strcmp(tag, "Ha")) {
            /* Ha who n (move result critical damage sourceId sourceBattlerIndex)*n:
             * turnData.attacksReceived, newest first */
            int who = atoi(next_tok(&save)), n = atoi(next_tok(&save));
            if (who < 0 || who > 1 || n < 0 || n > 32) { fprintf(stderr, "bad Ha line\n"); return 2; }
            pr_attack_record rec[32];
            for (int i = 0; i < n; i++) {
                rec[i].move = (uint16_t)atoi(next_tok(&save));
                rec[i].result = (uint8_t)atoi(next_tok(&save));
                rec[i].critical = atoi(next_tok(&save));
                rec[i].damage = strtod(next_tok(&save), NULL);
                rec[i].source_id = strtoll(next_tok(&save), NULL, 10);
                rec[i].source_battler_index = (int8_t)atoi(next_tok(&save));
            }
            hist[who].n_attacks_received = 0;
            for (int i = n - 1; i >= 0; i--) pr_unshift_attack_received(&hist[who], &rec[i]);
            continue;
        }
        if (!strcmp(tag, "H")) {
            /* H initialHp: the case move's TargetHalfHpDamageAttr state (NaN = undefined) */
            half_hp = strtod(next_tok(&save), NULL);
            have_half_hp = true;
            continue;
        }
        if (!strcmp(tag, "S")) {
            bt.battle_rng.c = strtod(strtok_r(NULL, " \n", &save), NULL);
            bt.battle_rng.s0 = strtod(strtok_r(NULL, " \n", &save), NULL);
            bt.battle_rng.s1 = strtod(strtok_r(NULL, " \n", &save), NULL);
            bt.battle_rng.s2 = strtod(strtok_r(NULL, " \n", &save), NULL);
            bt.battle_rng_live = true;
            continue;
        }
        if (!strcmp(tag, "M")) {
            /* M side class-chain pokemonId stackCount virtualStackCount transferable stat statsMask mult
             *   moveType num nSpecies species... */
            int side = atoi(strtok_r(NULL, " \n", &save));
            if (bt.n_mods[side] >= PR_MAX_MODIFIERS) { fprintf(stderr, "too many modifiers\n"); return 2; }
            pr_modifier *m = &bt.mods[side][bt.n_mods[side]++];
            memset(m, 0, sizeof *m);
            char *chain = strtok_r(NULL, " \n", &save), *cs;
            int depth = 0;
            for (char *c = strtok_r(chain, "<", &cs); c; c = strtok_r(NULL, "<", &cs), depth++) {
                int k = pr_mod_class_by_name(c);
                if (k >= 0) { m->cls = (uint8_t)k; m->unknown_subclass = depth > 0; break; }
            }
            if (!m->cls) { fprintf(stderr, "modifier with no known class: %s\n", chain); return 2; }
            m->pokemon_id = strtoll(strtok_r(NULL, " \n", &save), NULL, 10);
            m->stack_count = atoi(strtok_r(NULL, " \n", &save));
            m->virtual_stack_count = atoi(strtok_r(NULL, " \n", &save));
            m->is_transferable = atoi(strtok_r(NULL, " \n", &save));
            m->stat = (int8_t)atoi(strtok_r(NULL, " \n", &save));
            m->stats_mask = (uint8_t)atoi(strtok_r(NULL, " \n", &save));
            m->mult = strtod(strtok_r(NULL, " \n", &save), NULL);
            m->move_type = (int8_t)atoi(strtok_r(NULL, " \n", &save));
            m->num = (int16_t)atoi(strtok_r(NULL, " \n", &save));
            int ns = atoi(strtok_r(NULL, " \n", &save));
            if (ns > 4) { fprintf(stderr, "species list too long\n"); return 2; }
            for (int i = 0; i < ns; i++) m->species[m->n_species++] = (uint16_t)atoi(strtok_r(NULL, " \n", &save));
            continue;
        }
        if (!strcmp(tag, "T")) {
            /* T who type class-chain turnCount sourceMove sourceId critStages stockpiledCount
             *   autotomizeCount faintCount stat multiplier */
            int who = atoi(next_tok(&save));
            if (who < 0 || who > 1 || n_tags[who] >= PR_MAX_BATTLER_TAGS) { fprintf(stderr, "bad T line\n"); return 2; }
            pr_battler_tag *t = &tags[who][n_tags[who]++];
            memset(t, 0, sizeof *t);
            const char *name = next_tok(&save);
            int type = name_index(battler_tag_names, BATTLER_TAG_TYPE_COUNT_, name);
            if (type < 0) { fprintf(stderr, "unknown battler tag type %s\n", name); return 2; }
            t->type = (uint8_t)type;
            const char *chain = next_tok(&save);
            for (int k = 0; k < PR_TAGCLASS_COUNT_; k++)
                if (chain_has(chain, pr_tagclass_name[k]) != pr_battler_tag_is(type, k)) {
                    fprintf(stderr, "battler tag %s (%s): pr_battler_tag_is disagrees on %s\n", name, chain, pr_tagclass_name[k]);
                    return 2;
                }
            t->turn_count = (int16_t)atoi(next_tok(&save));
            t->source_move = (uint16_t)atoi(next_tok(&save));
            t->source_id = strtoll(next_tok(&save), NULL, 10);
            t->crit_stages = (int8_t)atoi(next_tok(&save));
            t->stockpiled_count = (int8_t)atoi(next_tok(&save));
            t->autotomize_count = (int16_t)atoi(next_tok(&save));
            t->faint_count = (int8_t)atoi(next_tok(&save));
            t->stat = (int8_t)atoi(next_tok(&save));
            t->multiplier = strtod(next_tok(&save), NULL);
            continue;
        }
        if (!strcmp(tag, "A")) {
            /* A type class-chain side turnCount maxDuration sourceMove sourceId sourceCount beingRemoved */
            if (bt.n_arena_tags >= PR_MAX_ARENA_TAGS) { fprintf(stderr, "too many arena tags\n"); return 2; }
            pr_arena_tag *t = &bt.arena_tags[bt.n_arena_tags++];
            memset(t, 0, sizeof *t);
            const char *name = next_tok(&save);
            int type = name_index(arena_tag_names, ARENA_TAG_TYPE_COUNT_, name);
            if (type < 0) { fprintf(stderr, "unknown arena tag type %s\n", name); return 2; }
            t->type = (uint8_t)type;
            const char *chain = next_tok(&save);
            for (int k = 0; k < PR_ARENACLASS_COUNT_; k++)
                if (chain_has(chain, pr_arenaclass_name[k]) != pr_arena_tag_is(type, k)) {
                    fprintf(stderr, "arena tag %s (%s): pr_arena_tag_is disagrees on %s\n", name, chain, pr_arenaclass_name[k]);
                    return 2;
                }
            t->side = (uint8_t)atoi(next_tok(&save));
            t->turn_count = (int16_t)atoi(next_tok(&save));
            t->max_duration = (int16_t)atoi(next_tok(&save));
            t->source_move = (uint16_t)atoi(next_tok(&save));
            t->source_id = strtoll(next_tok(&save), NULL, 10);
            t->source_count = (int8_t)atoi(next_tok(&save));
            t->being_removed = atoi(next_tok(&save));
            continue;
        }
        if (strcmp(tag, "C")) continue;
        if (bt.n_mods[0] != mods_expected[0] || bt.n_mods[1] != mods_expected[1]) {
            fprintf(stderr, "modifier count mismatch\n");
            return 2;
        }
        if (bt.n_arena_tags != arena_tags_expected) {
            fprintf(stderr, "arena tag count mismatch\n");
            return 2;
        }
        if (!read_mon(&save, &A) || !read_mon(&save, &D)) break;
        if (A.n_tags != n_tags[0] || D.n_tags != n_tags[1]) {
            fprintf(stderr, "battler tag count mismatch\n");
            return 2;
        }
        memcpy(A.tags, tags[0], sizeof A.tags);
        memcpy(D.tags, tags[1], sizeof D.tags);
        for (int w = 0; w < 2; w++) {
            pr_pokemon *p = w ? &D : &A;
            if (p->n_move_history != hist[w].n_move_history || p->n_attacks_received != hist[w].n_attacks_received) {
                fprintf(stderr, "history length mismatch\n");
                return 2;
            }
            memcpy(p->move_history, hist[w].move_history, sizeof p->move_history);
            memcpy(p->attacks_received, hist[w].attacks_received, sizeof p->attacks_received);
        }
        A.is_player = true;
        pr_update_passive(&bt, &A);
        pr_update_passive(&bt, &D);
        bt.weather = (uint8_t)atoi(strtok_r(NULL, " \n", &save));
        int move = atoi(strtok_r(NULL, " \n", &save));
        if (have_half_hp) {
            /* The C slot carries over from earlier cases as upstream's attribute does, so it must
             * already hold upstream's value (what earlier first hits stored); then it is synced. */
            double *slot = pr_half_hp_initial(&bt, move);
            if (!slot) { fprintf(stderr, "H line for a move without TargetHalfHpDamageAttr\n"); return 2; }
            if (!same(*slot, half_hp) && ++state_fail <= 5)
                printf("STATE MISMATCH initialHp of move %d: C %.17g | JS %.17g\n", move, *slot, half_hp);
            *slot = half_hp;
        }
        bool crit = atoi(strtok_r(NULL, " \n", &save));
        bool swap = atoi(strtok_r(NULL, " \n", &save));
        if (!fgets(eline, sizeof eline, f)) break;
        double e[19]; /* JS numbers, printed in shortest round-trip form */
        char *es;
        strtok_r(eline, " \n", &es);
        for (int i = 0; i < 19; i++) e[i] = strtod(next_tok(&es), NULL);

        bt.field[0] = &A; bt.field[2] = &D;
        A.on_field = D.on_field = true;
        /* the party members on the field are the field Pokemon themselves */
        for (int side = 0; side < 2; side++)
            for (int i = 0; i < bt.n_party[side]; i++) {
                if (bt.party[side][i]->id == A.id) bt.party[side][i] = &A;
                else if (bt.party[side][i]->id == D.id) bt.party[side][i] = &D;
            }
        bt.n_unported = 0;
        bt.n_deferred = 0;
        total++;
        /* The state before the calls, for the counterfactual run without turn history below */
        static pr_battle bt0;
        static pr_pokemon A0, D0;
        bt0 = bt; A0 = A; D0 = D;
        int32_t hp_a = A.hp, hp_d = D.hp;
        pr_calculate_stats(&bt, &A);
        pr_calculate_stats(&bt, &D);
        A.hp = hp_a;
        D.hp = hp_d;
        bool stats_ok = true;
        for (int i = 0; i < 6; i++)
            if (A.stats[i] != e[i] || D.stats[i] != e[6 + i]) stats_ok = false;
        if (!stats_ok) {
            stat_fail++;
            if (stat_fail <= 5) {
                printf("STATS MISMATCH species %d/%d lv %d nature %d: C", A.species, A.form, A.level, A.nature);
                for (int i = 0; i < 6; i++) printf(" %d", A.stats[i]);
                printf(" | JS");
                for (int i = 0; i < 6; i++) printf(" %.0f", e[i]);
                printf("\n");
            }
        }
        pr_pokemon *src = swap ? &D : &A, *def = swap ? &A : &D;
        pr_damage_result sim = pr_get_attack_damage(&bt, def, src, move, crit, true);
        bool c_crit = pr_get_critical_hit_result(&bt, def, src, move);
        pr_damage_result real = pr_get_attack_damage(&bt, def, src, move, crit, false);

        if (bt.n_unported) {
            for (int i = 0; i < bt.n_unported; i++) uncovered_by[bt.unported[i]]++;
            continue;
        }
        covered++;
        if (bt.n_deferred) with_deferred++;
        for (int i = 0; i < bt.n_deferred; i++) deferred_by[bt.deferred[i]]++;
        bool ok = stats_ok && same(sim.damage, e[12]) && sim.result == e[13] && c_crit == (e[14] != 0)
                  && same(real.damage, e[15]) && real.result == e[16] && src->hit_count == e[17]
                  && src->hits_left == e[18];
        if (ok) {
            /* Would the answer change with some state ignored? Replays the case from the state
             * before the calls without it: the tags (battler and arena), the passives, or the turn
             * history (move histories, attacks received, the last turn order). */
            for (int k = 0; k < CF_COUNT_; k++) {
                static pr_battle cb;
                static pr_pokemon ca, cd;
                cb = bt0; ca = A0; cd = D0;
                if (k == CF_TAGS) { ca.n_tags = cd.n_tags = 0; cb.n_arena_tags = 0; }
                if (k == CF_PASSIVES) { ca.passive = cd.passive = 0; }
                if (k == CF_HISTORY) {
                    ca.n_move_history = cd.n_move_history = 0;
                    ca.n_attacks_received = cd.n_attacks_received = 0;
                    cb.n_last_turn_order = 0;
                }
                cb.field[0] = &ca; cb.field[2] = &cd;
                ca.on_field = cd.on_field = true;
                for (int side = 0; side < 2; side++)
                    for (int i = 0; i < cb.n_party[side]; i++) {
                        if (cb.party[side][i] == &A) cb.party[side][i] = &ca;
                        else if (cb.party[side][i] == &D) cb.party[side][i] = &cd;
                    }
                pr_calculate_stats(&cb, &ca);
                pr_calculate_stats(&cb, &cd);
                ca.hp = hp_a;
                cd.hp = hp_d;
                pr_pokemon *s0 = swap ? &cd : &ca, *d0 = swap ? &ca : &cd;
                pr_damage_result sim0 = pr_get_attack_damage(&cb, d0, s0, move, crit, true);
                bool crit0 = pr_get_critical_hit_result(&cb, d0, s0, move);
                pr_damage_result real0 = pr_get_attack_damage(&cb, d0, s0, move, crit, false);
                if (!same(sim0.damage, sim.damage) || !same(real0.damage, real.damage) || sim0.result != sim.result
                    || real0.result != real.result || crit0 != c_crit)
                    matters[k]++;
            }
            pass++;
        }
        else if (++fail <= 10)
            printf("MISMATCH move %d (%s) A %d/%d D %d/%d crit %d swap %d weather %d terrain %d: C sim %.17g/%d crit %d real %.17g/%d hits %d/%d | JS %.17g/%.0f %.0f %.17g/%.0f hits %.0f/%.0f\n",
                   move, pr_moves[move].name, A.species, A.form, D.species, D.form, crit, swap, bt.weather, bt.terrain,
                   sim.damage, sim.result, c_crit, real.damage, real.result, src->hit_count, src->hits_left, e[12], e[13],
                   e[14], e[15], e[16], e[17], e[18]);
    }
    printf("cases %d, stats mismatches %d, carried-state mismatches %d (all cases)\n", total, stat_fail, state_fail);
    printf("covered %d: pass %d, fail %d; not covered %d\n", covered, pass, fail, total - covered);
    printf("covered cases whose side effects were deferred to the (unported) phase engine: %d\n", with_deferred);
    printf("passing cases whose answer changes if ignoring: tags %d, passives %d, turn history %d\n",
           matters[CF_TAGS], matters[CF_PASSIVES], matters[CF_HISTORY]);
    for (int c = 0; c < PR_FEAT_COUNT_; c++)
        if (deferred_by[c]) printf("  deferred %s: %d\n", pr_unported_name((uint16_t)c), deferred_by[c]);
    printf("not covered because of (class or missing state: cases):\n");
    for (int pass_no = 0; pass_no < 30; pass_no++) {
        int best = -1;
        for (int c = 0; c < PR_FEAT_COUNT_; c++)
            if (uncovered_by[c] && (best < 0 || uncovered_by[c] > uncovered_by[best])) best = c;
        if (best < 0) break;
        printf("  %s: %d\n", pr_unported_name((uint16_t)best), uncovered_by[best]);
        uncovered_by[best] = 0;
    }
    return fail || stat_fail || state_fail ? 1 : 0;
}
