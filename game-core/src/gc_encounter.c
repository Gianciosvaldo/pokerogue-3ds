/* A new wave: upstream's NewBattlePhase (battleScene.newBattle and its post-battle cleanup) and the
 * EncounterPhase's generation of the enemy side (src/battle-scene.ts, src/phases/encounter-phase.ts),
 * through run-core. What run-core can't generate yet gets a stand-in that is counted in run->stubs:
 * unsupported fixed classic battles and mystery encounters become counted wild stand-ins. */
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

bool gc_trainer_is_double(const gc_run *run)
{
    const pr_trainer_info *t = trainer_info(run->trainer_type);
    return run->trainer_variant == TRAINER_VARIANT_DOUBLE || (t && t->double_only);
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

void gc_enemy_ctx(gc_run *run, pr_enemy_gen_ctx *ec)
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

/* getPartyLuckValue(party) */
int gc_party_luck(gc_run *run)
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
    ic.double_battle = run->double_battle;
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
            memset(it, 0, sizeof *it); /* fresh generated stock must not inherit prior-holder state */
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
    gc_cache_generated_item_tiers(run);
}

/* gc_mon_new_enemy again once pr_run_enemy_pokemon_finish has filled in the rest of the rolls, keeping
 * the moveset made in between */
void gc_refresh_enemy(gc_run *run, gc_mon *m, int species, int level, const pr_enemy_gen *g, int slot)
{
    uint16_t mv[4];
    int8_t pp_up[4];
    int16_t max_pp[4];
    const int n = m->b.n_moves;
    memcpy(mv, m->b.moves, sizeof mv);
    memcpy(pp_up, m->b.pp_up, sizeof pp_up);
    memcpy(max_pp, m->b.max_pp_override, sizeof max_pp);
    gc_mon_new_enemy(run, m, species, level, g, slot);
    for (int k = 0; k < n; k++) {
        gc_mon_set_move(m, k, mv[k]);
        m->b.pp_up[k] = pp_up[k];
        m->b.max_pp_override[k] = max_pp[k];
    }
}

/* A wild battle's enemy: the species and addEnemyPokemon's rolls (run-core), the moveset, then the rest
 * of the constructor (run-core) */
static bool gen_wild_slot(gc_run *run, int level,int idx)
{
    pr_wild_enemy_ctx wc;
    memset(&wc, 0, sizeof wc);
    gc_enemy_ctx(run, &wc.enemy);
    wc.level = level;
    wc.luck = gc_party_luck(run);
    wc.golden_bug_net = gc_item_stack(run, "GOLDEN_BUG_NET") > 0;
    pr_wild_enemy w;
    if (pr_run_gen_wild_enemy(&run->run, &wc, &w) < 0) return false;
    gc_mon *m = &run->enemy[idx];
    gc_mon_new_enemy(run, m, w.species, level, &w.mon, 0);
    if (!gc_enemy_moveset(run, m, &w.mon)) gc_port_enemy_moveset(run, m);
    wc.enemy.species = w.species;
    wc.enemy.level = level;
    wc.enemy.boss = w.boss;
    wc.enemy.shiny_lock = w.shiny_lock;
    if (pr_run_enemy_pokemon_finish(&run->run, &wc.enemy, &w.mon) < 0) return false;
    gc_refresh_enemy(run, m, w.species, level, &w.mon, 0);
    /* the classic final boss gets perfect IVs */
    if (run->run.mode == GAME_MODES_CLASSIC && pr_run_is_wave_final(run->run.mode, run->wave))
        for (int i = 0; i < 6; i++) m->b.ivs[i] = 31;
    run->n_enemy = idx+1;
    return true;
}

static bool gen_wild(gc_run *run,int level) { return gen_wild_slot(run,level,0); }

bool gc_gen_wild(gc_run *run, int level)
{
    return gen_wild(run, level);
}

static bool gen_trainer(gc_run *run, const pr_new_battle *nb, int prev_wave)
{
    const pr_party_template *tpl = party_template(&run->run, nb->trainer_type, nb->template_index, prev_wave);
    if (!tpl) return false;
    pr_trainer_party_ctx tc;
    memset(&tc, 0, sizeof tc);
    gc_enemy_ctx(run, &tc.enemy);
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
        ec.trainer_slot = tc.is_double && e % 2 ? TRAINER_SLOT_TRAINER_PARTNER : TRAINER_SLOT_TRAINER;
        gc_mon *m = &run->enemy[run->n_enemy];
        gc_mon_new_enemy(run, m, party.species[e], nb->levels[e], &party.mons[e], ec.trainer_slot);
        gc_port_enemy_moveset(run, m);
        if (pr_run_enemy_pokemon_finish(&run->run, &ec, &party.mons[e]) < 0) return false;
        gc_refresh_enemy(run, m, party.species[e], nb->levels[e], &party.mons[e], ec.trainer_slot);
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

/* doPostBattleCleanup's resetArenaState for the battle of `wave` (prev_wave 0 = the run's first), and whether
 * EncounterPhase rolls the biome's weather and terrain for it (not NextEncounterPhase, a wave in the same biome) */
static void arena_flags(const gc_run *run, int prev_wave, bool *reset, bool *new_biome, bool *roll)
{
    *new_biome = prev_wave && gc_is_new_biome(run, prev_wave);
    *reset = !prev_wave || *new_biome || run->battle_type == BATTLE_TYPE_TRAINER
             || run->battle_type == BATTLE_TYPE_MYSTERY_ENCOUNTER
             || (run->run.mode == GAME_MODES_CLASSIC && pr_run_is_wave_final(run->run.mode,run->wave));
    *roll = !prev_wave || *new_biome || run->run.mode == GAME_MODES_DAILY;
}

/* What follows the enemy party's generation: passives and stats, the dex's seen flags, the enemy's items, the AI's view, the trainer names.
 * Also run after a custom rematch replaced the wild enemy (gc_custom_build_enemy), so the rematch enemy goes through the same paths. */
void gc_enemy_finish_party(gc_run *run)
{
    gc_internal *in = gc_int(run);
    for (int i = 0; i < run->n_enemy; i++) {
        in->enemy_start_species[i] = run->enemy[i].b.species;
        in->enemy_start_form[i] = run->enemy[i].b.form;
        pr_update_passive(&run->bt, &run->enemy[i].b);
        pr_calculate_stats(&run->bt, &run->enemy[i].b);
        if (run->sys) {
            bool is_trainer = (run->battle_type == BATTLE_TYPE_TRAINER);
            gc_system_set_pokemon_seen(run->sys, &run->enemy[i], true, is_trainer);
        }
    }
    enemy_items(run);
    gc_ai_enemy_party(run);
    gc_trainer_names(run);
}

/* setNature changes the underlying nature even if a Mint/custom nature keeps
 * getNature (b.nature) unchanged. The scene owns that split and saves both. */
bool gc_set_encounter_nature(pr_battle *bt, pr_pokemon *p, int nature, void *data)
{
    gc_run *run = data;
    gc_mon *m = NULL;
    for (int k = 0; k < run->n_party; k++) if (&run->party[k].b == p) m = &run->party[k];
    for (int k = 0; k < run->n_enemy; k++) if (&run->enemy[k].b == p) m = &run->enemy[k];
    if (!m || nature < 0 || nature >= PR_NATURE_COUNT) return false;
    m->base_nature = (uint8_t)nature;
    if (!m->has_custom_nature) p->nature = (uint8_t)nature;
    pr_update_passive(bt, p);
    pr_recalculate_stats(bt, p);
    return true;
}

static void sync_wild_nature(gc_run *run, bool new_double,int idx)
{
    pr_battle *bt = &run->bt;
    bool outgoing_double = bt->double_battle;
    bt->engine = &run->eng;
    bt->game_mode = run->run.mode;
    bt->challenge_flags = run->run.mode == GAME_MODES_CHALLENGE ? run->run.challenge_flags : 0;
    bt->challenge_flags_known = !run->run.has_challenges || run->run.challenge_flags != 0;
    bt->wave_index = run->wave;
    bt->double_battle = new_double;
    bt->n_party[0] = run->n_party;
    for (int k = 0; k < run->n_party; k++) {
        pr_pokemon *p = &run->party[run->party_order[k]].b;
        bt->party[0][k] = p;
        pr_update_passive(bt, p);
    }
    gc_items_to_battle(run);
    bt->rng = run->run.rng;
    bt->rng_live = true;
    pr_sync_encounter_nature(bt, &run->enemy[idx].b, gc_set_encounter_nature, run);
    run->run.rng = bt->rng;
    gc_battle_note_unported(run);
    bt->double_battle = outgoing_double;
}

/* newBattle() + EncounterPhase.start() for the next wave */
typedef struct { gc_run *run; int n_lures; } double_chance_context;

static double scene_double_chance(void *data, int wave)
{
    double_chance_context *ctx = data;
    gc_run *run = ctx->run;
    pr_battle *bt = &run->bt;
    /* Before the first encounter, party objects exist but the battle field has
     * not been constructed. Later calls retain the outgoing double/arena state. */
    bt->engine = &run->eng;
    bt->game_mode = run->run.mode;
    bt->challenge_flags = run->run.mode == GAME_MODES_CHALLENGE ? run->run.challenge_flags : 0;
    bt->challenge_flags_known = !run->run.has_challenges || run->run.challenge_flags != 0;
    bt->n_party[0] = run->n_party;
    for (int k = 0; k < run->n_party; k++) {
        pr_pokemon *p = &run->party[run->party_order[k]].b;
        bt->party[0][k] = p;
        pr_update_passive(bt, p);
    }
    bt->rng = run->run.rng;
    bt->rng_live = true;
    double chance = pr_get_double_battle_chance(bt, wave, ctx->n_lures);
    run->run.rng = bt->rng;
    gc_battle_note_unported(run);
    return chance;
}

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
    in->battle_won = false;
    run->me_battle = run->me_catch_allowed = false;

    pr_new_battle_ctx ctx;
    memset(&ctx, 0, sizeof ctx);
    ctx.biome = run->biome;
    ctx.trainer_chance = run->biome >= 0 && run->biome < PR_BIOME_ID_LIMIT ? pr_biomes[run->biome].trainer_chance : 0;
    ctx.prev_wave = prev_wave;
    if (!gc_lure_count(run, &ctx.n_lures)) gc_stub(run, GC_STUB_REWARD_INPUT);
    double_chance_context chance_ctx = {run, ctx.n_lures};
    ctx.double_chance = scene_double_chance;
    ctx.double_chance_ctx = &chance_ctx;
    ctx.me = run->me;
    pr_new_battle nb;
    unsigned before = run->run.unported;
    int rc = pr_run_new_battle(&run->run, wave, &ctx, &nb);
    note_run_core(run, before);
    bool stand_in = rc < 0;   /* a stand-in for a battle run-core could not make (gc/custom.h: GC_ENC_FIXED_OR_SCRIPTED) */
    if (rc < 0) {
        /* A fixed battle/config closure still outside the current game-core integration: use the
         * established wild stand-in. Mystery Encounters are handled separately below. */
        gc_stub(run, GC_STUB_UNPORTED_ENEMY);
        run->run.unported = before;
        pr_run_reset_seed(&run->run, wave);
        pr_rng br = pr_run_battle_rng(&run->run, wave);
        memset(&nb, 0, sizeof nb);
        nb.battle_type = BATTLE_TYPE_WILD;
        nb.trainer_type = nb.variant = nb.template_index = -1;
        pr_run_random_seed_string(&br, nb.battle_seed, 16);
        nb.double_battle=false;
        nb.n_levels = 1;
        nb.levels[0] = pr_run_get_level_for_wave(&run->run, &br, wave);
    }
    run->double_battle=nb.double_battle;
    run->battle_type = nb.battle_type;
    run->trainer_type = nb.trainer_type;
    run->trainer_variant = nb.variant;
    run->trainer_name = (int16_t)nb.name;
    run->trainer_partner_name = (int16_t)nb.partner_name;
    memcpy(run->battle_seed, nb.battle_seed, sizeof run->battle_seed);

    /* doPostBattleCleanup (not for the first battle): Pokerus spreads */
    if (prev_wave) {
        bool pk[6];
        for (int i = 0; i < run->n_party; i++) pk[i] = run->party[run->party_order[i]].pokerus;
        pr_run_try_spread_pokerus(&run->run, wave, pk, run->n_party);
        for (int i = 0; i < run->n_party; i++) run->party[run->party_order[i]].pokerus = pk[i];
    }

    /* doPostBattleCleanup precedes the new encounter's generation and save point.
     * The old double flag and player field still belong to the outgoing battle. */
    if (prev_wave) {
        bool reset, new_biome, roll;
        arena_flags(run,prev_wave,&reset,&new_biome,&roll);
        if (reset) {
            gc_battle_cleanup_players(run);
        }
    }

    if (nb.battle_type == BATTLE_TYPE_MYSTERY_ENCOUNTER) {
        int type = GC_ME_MYSTERIOUS_CHALLENGERS, tier = GC_ME_COMMON;
        run->battle_type = BATTLE_TYPE_MYSTERY_ENCOUNTER;
        memcpy(run->battle_seed, nb.battle_seed, sizeof run->battle_seed);
        if (!gc_mystery_select(run, &type, &tier)) {
            type = GC_ME_MYSTERIOUS_CHALLENGERS;
            tier = GC_ME_GREAT;
        }
        gc_mystery_record(run, type, tier);
        in->pending_me_miss = false;

        gc_event *ev = gc_ev_new(run, GC_EV_WAVE);
        if (ev) { ev->a = wave; ev->b = run->biome; ev->c = BATTLE_TYPE_MYSTERY_ENCOUNTER; }
        ev = gc_ev_new(run, GC_EV_MYSTERY);
        if (ev) { ev->a = type; ev->b = tier; ev->c = wave; }

        /* resetArenaEffects, then the biome's weather and terrain, before the save */
        bool reset, new_biome, roll;
        arena_flags(run, prev_wave, &reset, &new_biome, &roll);
        gc_arena_new_battle(run, reset && !prev_wave, roll);

        if (in->custom) {   /* custom content: the wave is reported (a mystery encounter is never offered a rematch) */
            gc_custom_encounter enc;
            memset(&enc, 0, sizeof enc);
            enc.wave = wave;
            enc.biome = run->biome;
            enc.kind = pr_run_is_wave_final(run->run.mode, wave) ? GC_ENC_FINAL : GC_ENC_MYSTERY;
            gc_custom_report(run, &enc);
        }

        /* Upstream saves at the encounter start before showing the mystery node. */
        gc_ev_new(run, GC_EV_SAVE_POINT);
        in->at_save_point = true;
        pr_run_reset_seed(&run->run, wave);
        gc_rq_unshift(run, gc_rp(RP_MYSTERY, type, tier, 0));
        return;
    }

    in->pending_me_miss = pr_run_is_mystery_encounter_valid_for_wave(
        &run->run, nb.battle_type == BATTLE_TYPE_WILD, wave);

    /* EncounterPhase: the enemy party */
    bool ok;
    if(nb.battle_type==BATTLE_TYPE_TRAINER)ok=gen_trainer(run,&nb,prev_wave);
    else {
        ok=true;
        for(int i=0;i<(nb.double_battle?2:1)&&ok;i++) {
            ok=gen_wild_slot(run,nb.levels[i],i);
            if(ok&&!stand_in)sync_wild_nature(run,nb.double_battle,i);
        }
    }
    note_run_core(run, before);
    if (!ok) {
        /* run-core couldn't make it (a trainer's config closure, an unported species pool...): a wild
         * battle of the same level stands in, rolled again on the wave's seed */
        stand_in = true;
        gc_stub(run, GC_STUB_UNPORTED_ENEMY);
        run->run.unported = before;
        run->battle_type = BATTLE_TYPE_WILD;
        run->double_battle=false;
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

    /* EncounterPhase distributes shields by primary-form BST when both wild
     * opponents are bosses; the denominator includes the whole enemy party. */
    if(run->battle_type==BATTLE_TYPE_WILD&&run->double_battle){
        int bosses=0,total=0;
        for(int k=0;k<run->n_enemy;k++){pr_pokemon *p=&run->enemy[k].b;bosses+=p->is_boss;const pr_form *f=gc_form(p->species,p->form);for(int j=0;j<6;j++)total+=f->base_stats[j];}
        if(bosses>1&&total>0)for(int k=0;k<run->n_enemy;k++){
            pr_pokemon *p=&run->enemy[k].b;if(!p->is_boss)continue;const pr_form *f=gc_form(p->species,p->form);int bst=0;for(int j=0;j<6;j++)bst+=f->base_stats[j];
            p->boss_segments=(uint8_t)ceil((double)p->boss_segments*bst/total);p->boss_segment_index=p->boss_segments-1;
        }
    }
    gc_enemy_finish_party(run);
    run->run.unported = before;
    /* newBattle's resetArenaEffects, then arena.setBiomeWeather / setBiomeTerrain: before the save point and
     * resetSeed, so what they roll is only the arena, not the rolls after them */
    bool reset, new_biome, roll;
    arena_flags(run, prev_wave, &reset, &new_biome, &roll);
    /* Cleanup already reset the outgoing arena. Its removal callbacks may
     * have established new weather, which survives an ordinary trainer entry. */
    gc_arena_new_battle(run, reset && !prev_wave, roll);

    gc_event *ev = gc_ev_new(run, GC_EV_WAVE);
    if (ev) { ev->a = wave; ev->b = run->biome; ev->c = run->battle_type; }
    if (run->battle_type == BATTLE_TYPE_TRAINER) {
        ev = gc_ev_new(run, GC_EV_TRAINER);
        if (ev) { ev->a = run->trainer_type; ev->b = run->trainer_variant; }
    }
    /* custom content (gc/custom.h): the wave is reported once its enemy is final, before the save point. The player may be asked here; the
     * rest of the encounter (from the save point on) then runs when the answer comes (gc_custom_answer), or at once when nobody is asked. */
    if (in->custom) {
        gc_custom_encounter enc;
        memset(&enc, 0, sizeof enc);
        enc.wave = wave;
        enc.biome = run->biome;
        enc.level = run->n_enemy ? run->enemy[0].b.level : 0;
        enc.species = run->n_enemy ? run->enemy[0].b.species : 0;
        bool boss = false;
        for (int i = 0; i < run->n_enemy; i++) boss |= run->enemy[i].b.is_boss;
        if (pr_run_is_wave_final(run->run.mode, wave)) enc.kind = GC_ENC_FINAL;
        else if (boss) enc.kind = GC_ENC_BOSS;
        else if (run->battle_type == BATTLE_TYPE_TRAINER) enc.kind = GC_ENC_TRAINER;
        else if (stand_in) enc.kind = GC_ENC_FIXED_OR_SCRIPTED;
        else enc.kind = GC_ENC_ORDINARY_WILD;
        if (nb.double_battle) enc.flags |= GC_ENF_DOUBLES;
        if (run->n_enemy && run->enemy[0].b.fusion_species) enc.flags |= GC_ENF_FUSED;
        if (run->n_enemy && run->enemy[0].b.terastallized) enc.flags |= GC_ENF_TERA;
        if (gc_custom_report(run, &enc)) {
            in->custom_offering = true;
            in->custom_offer_reset = reset;
            in->custom_offer_new_biome = new_biome;
            gc_rq_unshift(run, gc_rp(RP_CUSTOM_OFFER, 0, 0, 0));
            return;
        }
    }
    gc_new_battle_start(run, reset, new_biome);
}

/* EncounterPhase from the save point on: what newBattle's start does once the enemy and the arena are set */
void gc_new_battle_start(gc_run *run, bool reset, bool new_biome)
{
    gc_internal *in = gc_int(run);
    const int wave = run->wave;
    /* the save point: gameData.saveAll at the start of the encounter, before anything is shown */
    gc_ev_new(run, GC_EV_SAVE_POINT);
    in->at_save_point = true;
    if (in->pending_me_miss) {
        gc_mystery_increment_miss(run);
        in->pending_me_miss = false;
    }
    /* doEncounter: resetWaveData, then resetSeed() once the encounter is set up */
    pr_run_reset_seed(&run->run, wave);

    if (run->battle_type == BATTLE_TYPE_TRAINER) {
        gc_ev_msg(run, run->double_battle ? "battle:trainerAppearedDouble" : "battle:trainerAppeared", 1, gc_m_trainer(run, "trainerName", GC_TNAME_TITLED));
    } else {
        if(run->double_battle&&run->n_enemy>1)
            gc_ev_msg(run,"battle:multiWildAppeared",2,gc_m_mon_name(run,"pokemonName1",1,0),gc_m_mon_name(run,"pokemonName2",1,1));
        else gc_ev_msg(run, "battle:singleWildAppeared", 1, gc_m_mon_name(run, "pokemonName", 1, 0));
    }
    /* doPostBattleCleanup's resetArenaState */
    const bool sent_out = gc_battle_setup_field(run, reset);
    if (run->battle_type == BATTLE_TYPE_TRAINER)
        gc_ev_msg(run, "battle:trainerSendOut", 2, gc_m_trainer(run, "trainerName", GC_TNAME_TRAINER), gc_m_mon_name(run, "pokemonName", 1, 0));
    if (sent_out) gc_ev_msg(run, "battle:playerGo", 1, gc_m_mon(run, "pokemonName", 0, gc_player_active_index(run)));
    /* CheckSwitchPhase: battle-core's start stopped to ask "Will you switch?" (run->ask_switch); the rest of the
     * start (InitEncounterPhase and the enemy's PostSummonPhase) runs once it is answered */
    if (run->eng.awaiting_confirm) gc_rq_unshift(run, gc_rp(RP_CHECK_SWITCH, 0, 0, 0));
    /* LevelCapPhase, after a new biome's encounter */
    if (new_biome && run->bt.max_exp_level > in->last_max_exp_level)
        gc_ev_msg(run, "battle:levelCapUp", 1, pr_arg_int("levelCap", run->bt.max_exp_level));
}
