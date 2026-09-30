/* A new wave: upstream's NewBattlePhase (battleScene.newBattle and its post-battle cleanup) and the
 * EncounterPhase's generation of the enemy side (src/battle-scene.ts, src/phases/encounter-phase.ts),
 * through run-core. What run-core can't generate yet gets a stand-in that is counted in run->stubs:
 * fixed classic battles and mystery encounters become wild battles, a double battle is played as a
 * single one (the M1 rule), and a wild double's second enemy isn't made. */
#include "internal.h"
#include <stdio.h>

/* run-core's trainer entry for a TrainerType (the table skips the unused numbers), NULL = none */
static const pr_trainer_info *trainer_info(int type)
{
    int lo = 0, hi = pr_trainer_count - 1;
    while (lo <= hi) {
        const int mid = (lo + hi) / 2;
        if (pr_trainers[mid].type == type) return &pr_trainers[mid];
        if (pr_trainers[mid].type < type) lo = mid + 1; else hi = mid - 1;
    }
    return NULL;
}

const gc_trainer_config *gc_trainer_config_of(int type)
{
    int lo = 0, hi = gc_trainer_config_count - 1;
    while (lo <= hi) {
        const int mid = (lo + hi) / 2;
        if (gc_trainer_configs[mid].type == type) return &gc_trainer_configs[mid];
        if (gc_trainer_configs[mid].type < type) lo = mid + 1; else hi = mid - 1;
    }
    return NULL;
}

/* trainersCommon's key for a name run-core rolled (index `n` among the class's names), into out */
static void name_key(char *out, size_t cap, const char *class_key, const char *gender, int n)
{
    snprintf(out, cap, "$t(trainersCommon:%s%s.%d)", class_key, gender, n + 1);
}

/* Copies tpl into out with each @N / @P replaced by n / p */
static void fill_names(char *out, size_t cap, const char *tpl, const char *n, const char *p)
{
    size_t o = 0;
    for (; *tpl && o + 1 < cap; tpl++) {
        const char *put = tpl[0] == '@' && tpl[1] == 'N' ? n : tpl[0] == '@' && tpl[1] == 'P' ? p : NULL;
        if (!put) { out[o++] = *tpl; continue; }
        while (*put && o + 1 < cap) out[o++] = *put++;
        tpl++;
    }
    out[o] = 0;
}

/* Trainer.getName for the current trainer (run->trainer_names), from the templates the dump took from
 * upstream's getName and the names run-core rolled. The texts sit in a ring of three, so a message of
 * the battle before that is still queued keeps its name. A double-only pair (Twins) has one name,
 * "Amy & May", which upstream splits in two: its send-out here names both */
void gc_trainer_names(gc_run *run)
{
    gc_internal *in = gc_int(run);
    for (int i = 0; i < GC_TNAME_COUNT_; i++) run->trainer_names[i] = NULL;
    const gc_trainer_config *c = run->battle_type == BATTLE_TYPE_TRAINER ? gc_trainer_config_of(run->trainer_type) : NULL;
    const int v = run->trainer_variant;
    if (!c || v < 0 || v > 2) return;
    in->name_ring = (uint8_t)((in->name_ring + 1) % 3);
    /* the Trainer constructor: the name from trainersCommon:<class>, .male/.female by variant when the
     * class has them; a partner's from .female when it has it */
    const pr_trainer_info *t = trainer_info(run->trainer_type);
    char n[80] = "", p[80] = "";
    if (c->class_key && run->trainer_name >= 0)
        name_key(n, sizeof n, c->class_key,
                 t && t->names_male >= 0 ? (v == TRAINER_VARIANT_FEMALE ? ".female" : ".male") : "", run->trainer_name);
    if (c->class_key && run->trainer_partner_name >= 0)
        name_key(p, sizeof p, c->class_key, t && t->names_female >= 0 ? ".female" : "", run->trainer_partner_name);
    for (int i = 0; i < GC_TNAME_COUNT_; i++) {
        char *out = in->name_text[in->name_ring][i];
        const char *tpl = gc_trainer_name_templates[c->names[v][i]];
        if (c->double_only && !p[0]) {
            /* the pair's one name stands for "@N & @P", and for each half */
            char whole[128];
            const char *pair = strstr(tpl, "@N & @P");
            if (pair) {
                snprintf(whole, sizeof whole, "%.*s@N%s", (int)(pair - tpl), tpl, pair + 7);
                tpl = whole;
            }
            fill_names(out, sizeof in->name_text[0][0], tpl, n, n);
        } else {
            fill_names(out, sizeof in->name_text[0][0], tpl, n, p);
        }
        run->trainer_names[i] = out;
        /* the message argument gc_m_trainer gives until messages can take a text with several keys: the
         * one key when the name is just that, else the name after the title (the second key) */
        char *arg = in->name_arg[in->name_ring][i];
        arg[0] = 0;
        int segs = 0;
        for (const char *q = strstr(out, "$t("); q; q = strstr(q, "$t(")) {
            const char *e = strchr(q, ')');
            if (!e) break;
            if (segs++ < 2) snprintf(arg, sizeof in->name_arg[0][0], "%.*s", (int)(e - q - 3), q + 3);
            q = e;
        }
        const size_t len = strlen(out);
        if (segs == 1 && !strncmp(out, "$t(", 3) && out[len - 1] == ')') in->name_exact[in->name_ring] |= (uint8_t)(1u << i);
        else in->name_exact[in->name_ring] &= (uint8_t)~(1u << i);
    }
}

/* trainer.getPartyTemplate(), as run-core's newbattle.c resolves it for the Battle constructor */
static const pr_party_template *party_template(const pr_run *run, int trainer_type, int index, int prev_wave)
{
    const pr_trainer_info *t = trainer_info(trainer_type);
    if (!t) return NULL;
    if (!t->has_template_func) return index >= 0 && index < t->n_templates ? &pr_party_templates[t->templates[index]] : NULL;
    const pr_template_segment *best = NULL;
    for (int i = 0; i < t->n_segments; i++) {
        const pr_template_segment *s = &pr_template_segments[t->segment_start + i];
        if (s->mode != run->mode) continue;
        if (prev_wave == 0 ? s->from_wave == 0 : (s->from_wave > 0 && s->from_wave <= prev_wave)) best = s;
    }
    return best ? &pr_party_templates[best->template] : NULL;
}

static void enemy_ctx(gc_run *run, pr_enemy_gen_ctx *ec)
{
    memset(ec, 0, sizeof *ec);
    ec->wave = run->wave;
    ec->biome = run->biome;
    ec->wave_cycle_offset = run->wave_cycle_offset;
    ec->battle_type = run->battle_type;
    ec->trainer_type = run->trainer_type;
    ec->hidden_ability_charm_stacks = gc_item_stack(run, "ABILITY_CHARM");
    ec->shiny_charm_stacks = gc_item_stack(run, "SHINY_CHARM");
    ec->trainer_id = (uint16_t)run->trainer_id;
    ec->secret_id = (uint16_t)run->secret_id;
    ec->event_shiny_multiplier = 1;
}

static int party_luck(gc_run *run)
{
    pr_luck_member m[6];
    for (int i = 0; i < run->n_party; i++) {
        memset(&m[i], 0, sizeof m[i]);
        m[i].species = run->party[i].b.species;
        m[i].luck = run->party[i].luck;
        m[i].allowed = gc_mon_allowed(&run->party[i]);
    }
    return pr_run_get_party_luck_value(&run->run, m, run->n_party, NULL, 0, 0);
}

/* The enemy's held items: regenerateModifierPoolThresholds + generateEnemyModifiers */
static void enemy_items(gc_run *run)
{
    pr_enemy_items_ctx ic;
    memset(&ic, 0, sizeof ic);
    ic.wave = run->wave;
    ic.trainer_battle = run->battle_type == BATTLE_TYPE_TRAINER;
    ic.double_battle = false;
    ic.trainer_type = run->trainer_type;
    ic.n_party = run->n_enemy;
    for (int i = 0; i < run->n_enemy; i++) {
        const pr_pokemon *p = &run->enemy[i].b;
        ic.party[i].boss = p->is_boss;
        /* getTypesForItemSpawn: each attack move's type, in moveset order */
        for (int k = 0; k < p->n_moves && ic.party[i].n_attack_types < 16; k++)
            if (p->moves[k] && pr_move_is_attack(p->moves[k]))
                ic.party[i].attack_types[ic.party[i].n_attack_types++] = pr_moves[p->moves[k]].type;
    }
    static pr_enemy_items out;
    run->n_enemy_items = 0;
    if (pr_run_generate_enemy_items(&run->run, &ic, &out) < 0) { gc_stub(run, GC_STUB_UNPORTED_ENEMY); return; }
    for (int i = 0; i < run->n_enemy; i++)
        for (int k = 0; k < out.n[i] && run->n_enemy_items < GC_MAX_ITEMS; k++) {
            gc_item *it = &run->enemy_items[run->n_enemy_items++];
            const bool token = out.items[i][k].item >= 14;   /* the ENEMY_* tokens and the Mini Black Hole */
            it->key = pr_item_names[out.items[i][k].item];
            it->arg = out.items[i][k].arg;
            it->stack = 1;
            it->pokemon_id = token && out.items[i][k].item != (uint8_t)pr_item_mini_black_hole ? 0 : run->enemy[i].b.id;
            /* stack onto a matching entry, as addEnemyModifier does */
            for (int j = 0; j < run->n_enemy_items - 1; j++) {
                gc_item *o = &run->enemy_items[j];
                if (o->key == it->key && o->arg == it->arg && o->pokemon_id == it->pokemon_id) {
                    o->stack++;
                    run->n_enemy_items--;
                    break;
                }
            }
        }
}

/* gc_mon_new_enemy again once pr_run_enemy_pokemon_finish has filled in the rest of the rolls, keeping
 * the moveset made in between */
static void refresh_enemy(gc_run *run, gc_mon *m, int species, int level, const pr_enemy_gen *g, int slot)
{
    uint16_t mv[4];
    const int n = m->b.n_moves;
    memcpy(mv, m->b.moves, sizeof mv);
    gc_mon_new_enemy(run, m, species, level, g, slot);
    for (int k = 0; k < n; k++) gc_mon_set_move(m, k, mv[k]);
}

/* A wild battle's enemy: the species and addEnemyPokemon's rolls (run-core), the moveset, then the rest
 * of the constructor (run-core) */
static bool gen_wild(gc_run *run, int level)
{
    pr_wild_enemy_ctx wc;
    memset(&wc, 0, sizeof wc);
    enemy_ctx(run, &wc.enemy);
    wc.level = level;
    wc.luck = party_luck(run);
    wc.golden_bug_net = gc_item_stack(run, "GOLDEN_BUG_NET") > 0;
    pr_wild_enemy w;
    if (pr_run_gen_wild_enemy(&run->run, &wc, &w) < 0) return false;
    gc_mon *m = &run->enemy[0];
    gc_mon_new_enemy(run, m, w.species, level, &w.mon, 0);
    gc_port_enemy_moveset(run, m);
    wc.enemy.species = w.species;
    wc.enemy.level = level;
    wc.enemy.boss = w.boss;
    wc.enemy.shiny_lock = w.shiny_lock;
    if (pr_run_enemy_pokemon_finish(&run->run, &wc.enemy, &w.mon) < 0) return false;
    refresh_enemy(run, m, w.species, level, &w.mon, 0);
    /* the classic final boss gets perfect IVs */
    if (run->run.mode == GAME_MODES_CLASSIC && pr_run_is_wave_final(run->run.mode, run->wave))
        for (int i = 0; i < 6; i++) m->b.ivs[i] = 31;
    run->n_enemy = 1;
    return true;
}

static bool gen_trainer(gc_run *run, const pr_new_battle *nb, int prev_wave)
{
    const pr_party_template *tpl = party_template(&run->run, nb->trainer_type, nb->template_index, prev_wave);
    if (!tpl) return false;
    pr_trainer_party_ctx tc;
    memset(&tc, 0, sizeof tc);
    enemy_ctx(run, &tc.enemy);
    const pr_trainer_info *t = trainer_info(nb->trainer_type);
    tc.is_double = (t && t->double_only) || nb->variant == TRAINER_VARIANT_DOUBLE;
    tc.classic_wave20_trainer = run->wave == 20 && run->run.mode == GAME_MODES_CLASSIC;
    tc.template_ = tpl;
    tc.n_levels = nb->n_levels;
    memcpy(tc.levels, nb->levels, sizeof tc.levels);
    static pr_trainer_party party;
    if (pr_run_gen_trainer_party(&run->run, &tc, &party) < 0) return false;
    run->n_enemy = 0;
    for (int e = 0; e < party.n && e < 6; e++) {
        pr_enemy_gen_ctx ec = tc.enemy;
        ec.species = party.species[e];
        ec.level = nb->levels[e];
        ec.trainer_slot = TRAINER_SLOT_TRAINER;
        gc_mon *m = &run->enemy[run->n_enemy];
        gc_mon_new_enemy(run, m, party.species[e], nb->levels[e], &party.mons[e], TRAINER_SLOT_TRAINER);
        gc_port_enemy_moveset(run, m);
        if (pr_run_enemy_pokemon_finish(&run->run, &ec, &party.mons[e]) < 0) return false;
        refresh_enemy(run, m, party.species[e], nb->levels[e], &party.mons[e], TRAINER_SLOT_TRAINER);
        run->n_enemy++;
    }
    return run->n_enemy > 0;
}

/* the run-core stand-ins a call just reported (bits set since `before`), for the soak runs */
static void note_run_core(gc_run *run, unsigned before)
{
    const unsigned now = run->run.unported & ~before;
    for (int b = 0; b < 8; b++) if (now & (1u << b)) run->run_core_unported[b]++;
}

/* isNewBiome(battle of `wave`) */
bool gc_is_new_biome(const gc_run *run, int wave)
{
    const int m = run->run.mode;
    const bool short_biomes = m == GAME_MODES_ENDLESS || m == GAME_MODES_SPLICED_ENDLESS;
    const bool daily = m == GAME_MODES_DAILY;
    return wave % 10 == 0 || (short_biomes && wave % 5 == 0) || ((short_biomes || daily) && wave % 50 == 49);
}

/* newBattle() + EncounterPhase.start() for the next wave */
void gc_new_battle(gc_run *run)
{
    gc_internal *in = gc_int(run);
    const int prev_wave = run->in_battle || run->wave ? run->wave : 0;
    const int wave = prev_wave + 1;
    run->wave = wave;
    run->in_battle = true;
    run->n_enemy = 0;
    run->n_enemy_items = 0;
    run->trainer_type = run->trainer_variant = -1;
    run->trainer_name = run->trainer_partner_name = -1;
    gc_ai_reset(run);
    in->reroll_count = 0;

    pr_new_battle_ctx ctx;
    memset(&ctx, 0, sizeof ctx);
    ctx.biome = run->biome;
    ctx.trainer_chance = run->biome >= 0 && run->biome < PR_BIOME_ID_LIMIT ? pr_biomes[run->biome].trainer_chance : 0;
    ctx.prev_wave = prev_wave;
    ctx.n_lures = 0;   /* the Lures aren't items yet (reward-core) */
    ctx.me = run->me;
    pr_new_battle nb;
    unsigned before = run->run.unported;
    int rc = pr_run_new_battle(&run->run, wave, &ctx, &nb);
    note_run_core(run, before);
    if (rc < 0 || nb.battle_type == BATTLE_TYPE_MYSTERY_ENCOUNTER) {
        /* a fixed classic battle (rival, evil team, Elite Four...) or a mystery encounter: a wild battle
         * of the wave's level stands in (the fixed battle's config and the encounters aren't ported) */
        gc_stub(run, rc < 0 ? GC_STUB_UNPORTED_ENEMY : GC_STUB_MYSTERY_ENCOUNTER);
        if (nb.me_spawn_chance_reset) run->me.spawn_chance = 3;   /* BASE_MYSTERY_ENCOUNTER_SPAWN_WEIGHT */
        run->run.unported = before;
        if (rc < 0) pr_run_reset_seed(&run->run, wave);
        pr_rng br = pr_run_battle_rng(&run->run, wave);
        memset(&nb, 0, sizeof nb);
        nb.battle_type = BATTLE_TYPE_WILD;
        nb.trainer_type = nb.variant = nb.template_index = -1;
        pr_run_random_seed_string(&br, nb.battle_seed, 16);
        nb.n_levels = 1;
        nb.levels[0] = pr_run_get_level_for_wave(&run->run, &br, wave);
    }
    if (nb.double_battle) gc_stub(run, GC_STUB_DOUBLE_AS_SINGLE);
    run->battle_type = nb.battle_type;
    run->trainer_type = nb.trainer_type;
    run->trainer_variant = nb.variant;
    run->trainer_name = (int16_t)nb.name;
    run->trainer_partner_name = (int16_t)nb.partner_name;
    memcpy(run->battle_seed, nb.battle_seed, sizeof run->battle_seed);

    /* doPostBattleCleanup (not for the first battle): Pokerus spreads */
    if (prev_wave) {
        bool pk[6];
        for (int i = 0; i < run->n_party; i++) pk[i] = run->party[i].pokerus;
        pr_run_try_spread_pokerus(&run->run, wave, pk, run->n_party);
        for (int i = 0; i < run->n_party; i++) run->party[i].pokerus = pk[i];
    }

    /* EncounterPhase: the enemy party */
    bool ok = nb.battle_type == BATTLE_TYPE_TRAINER ? gen_trainer(run, &nb, prev_wave) : gen_wild(run, nb.levels[0]);
    note_run_core(run, before);
    if (!ok) {
        /* run-core couldn't make it (a trainer's config closure, an unported species pool...): a wild
         * battle of the same level stands in, rolled again on the wave's seed */
        gc_stub(run, GC_STUB_UNPORTED_ENEMY);
        run->run.unported = before;
        run->battle_type = BATTLE_TYPE_WILD;
        run->trainer_type = run->trainer_variant = -1;
        run->trainer_name = run->trainer_partner_name = -1;
        pr_run_reset_seed(&run->run, wave);
        if (!gen_wild(run, nb.levels[0])) {
            /* last resort: the lead of the player's species list at the wave's level */
            pr_enemy_gen g;
            memset(&g, 0, sizeof g);
            g.id = (uint32_t)floor(pr_rng_real_in_range(&run->run.rng, 0, 4294967296.0));
            g.fusion_species = -1;
            g.friendship = -1;
            gc_mon_new_enemy(run, &run->enemy[0], SPECIES_ID_RATTATA, nb.levels[0], &g, 0);
            gc_port_enemy_moveset(run, &run->enemy[0]);
            run->n_enemy = 1;
            run->run.unported = before;
        }
    }
    for (int i = 0; i < run->n_enemy; i++) {
        pr_update_passive(&run->bt, &run->enemy[i].b);
        pr_calculate_stats(&run->bt, &run->enemy[i].b);
    }
    enemy_items(run);
    gc_ai_enemy_party(run);
    gc_trainer_names(run);
    /* arena.setBiomeWeather / setBiomeTerrain on a new biome: not ported (they roll on the RNG) */
    run->run.unported = before;

    gc_event *ev = gc_ev_new(run, GC_EV_WAVE);
    if (ev) { ev->a = wave; ev->b = run->biome; ev->c = run->battle_type; }
    if (run->battle_type == BATTLE_TYPE_TRAINER) {
        ev = gc_ev_new(run, GC_EV_TRAINER);
        if (ev) { ev->a = run->trainer_type; ev->b = run->trainer_variant; }
    }
    /* the save point: gameData.saveAll at the start of the encounter, before anything is shown */
    gc_ev_new(run, GC_EV_SAVE_POINT);
    in->at_save_point = true;
    /* doEncounter: resetWaveData, then resetSeed() once the encounter is set up */
    pr_run_reset_seed(&run->run, wave);

    if (run->battle_type == BATTLE_TYPE_TRAINER) {
        gc_ev_msg(run, "battle:trainerAppeared", 1, gc_m_trainer(run, "trainerName", GC_TNAME_TITLED));
    } else {
        gc_ev_msg(run, "battle:singleWildAppeared", 1, gc_m_mon_name(run, "pokemonName", 1, 0));
    }
    /* doPostBattleCleanup's resetArenaState */
    const bool new_biome = prev_wave && gc_is_new_biome(run, prev_wave);
    const bool reset = !prev_wave || new_biome || in->last_battle_type == BATTLE_TYPE_TRAINER
                       || in->last_battle_type == BATTLE_TYPE_MYSTERY_ENCOUNTER || in->last_final_boss;
    const bool sent_out = gc_battle_setup_field(run, reset);
    if (run->battle_type == BATTLE_TYPE_TRAINER)
        gc_ev_msg(run, "battle:trainerSendOut", 2, gc_m_trainer(run, "trainerName", GC_TNAME_TRAINER), gc_m_mon_name(run, "pokemonName", 1, 0));
    if (sent_out) gc_ev_msg(run, "battle:playerGo", 1, gc_m_mon(run, "pokemonName", 0, gc_player_active_index(run)));
    /* LevelCapPhase, after a new biome's encounter */
    if (new_biome && run->bt.max_exp_level > in->last_max_exp_level)
        gc_ev_msg(run, "battle:levelCapUp", 1, pr_arg_int("levelCap", run->bt.max_exp_level));
}
