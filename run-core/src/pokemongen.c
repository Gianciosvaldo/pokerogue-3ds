/* A new enemy Pokemon without a data source: the rolls of the Pokemon constructor and the
 * EnemyPokemon constructor around generateAndPopulateMoveset (src/field/pokemon.ts), and
 * getSpeciesFormIndex (src/battle-scene.ts). The moveset itself isn't ported yet; the two halves
 * here are the rolls before it and after it, in upstream's order. The JS each step replaces is
 * quoted where it happens. */
#include "pr/run.h"
#include <math.h>

#define BASE_HIDDEN_ABILITY_RATE 256   /* src/data/balance/rates.ts */
#define BASE_SHINY_CHANCE 64
#define SHINY_VARIANT_CHANCE 4
#define SHINY_EPIC_CHANCE 1
#define TRAINER_MIN_FRIENDSHIP 50       /* src/data/balance/starters.ts */
#define TRAINER_MAX_FRIENDSHIP_WAVE 145

static const pr_species_gen *species_gen(int id)
{
    int lo = 0, hi = pr_species_gen_count - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (pr_species_gens[mid].id == id) return &pr_species_gens[mid];
        if (pr_species_gens[mid].id < id) lo = mid + 1; else hi = mid - 1;
    }
    return NULL;
}

static int seed_int(pr_run *run, int range) { return pr_rng_seed_int(&run->rng, range, 0); }

/* getSpeciesForm(): species.forms[formIndex] when it has forms, else the species; NULL where
 * upstream reads an undefined form */
static const pr_form_gen *form_gen(const pr_species_gen *sp, int form)
{
    if (!sp->n_forms) return &pr_form_gens[sp->form_start];
    if (form < 0 || form >= sp->n_forms) return NULL;
    return &pr_form_gens[sp->form_start + form];
}

/* getSpeciesFormIndex(species, gender, nature, ignoreArena); nature -1 = undefined */
static int form_index(pr_rng *rng, pr_run *run, const pr_enemy_gen_ctx *ctx, int species, int gender, int nature, bool ignore_arena)
{
    const pr_species_gen *sp = species_gen(species);
    if (!sp) { run->unported |= PR_RUN_UNPORTED_SPECIES; return 0; }
    /* if (species.forms.length === 0) return 0; (not in an egg phase: isEggPhase is false) */
    if (!sp->n_forms) return 0;
    const bool trainer_battle = ctx->battle_type == BATTLE_TYPE_TRAINER;
    const int s = species;
    /* Trainers with a specialty type (config.hasSpecialtyType()) get a form of that type */
    int specialty = -1;
    if (trainer_battle && ctx->trainer_type >= 0 && ctx->trainer_type < pr_trainer_specialty_count)
        specialty = pr_trainer_specialty[ctx->trainer_type];
    if (specialty >= 0) {
        if (s == SPECIES_ID_WORMADAM) {
            switch (specialty) {
            case POKEMON_TYPE_GROUND: return 1;
            case POKEMON_TYPE_STEEL: return 2;
            case POKEMON_TYPE_GRASS: return 0;
            }
        }
        if (s == SPECIES_ID_ROTOM) {
            switch (specialty) {
            case POKEMON_TYPE_FLYING: return 4;
            case POKEMON_TYPE_GHOST: return 0;
            case POKEMON_TYPE_FIRE: return 1;
            case POKEMON_TYPE_WATER: return 2;
            case POKEMON_TYPE_GRASS: return 5;
            case POKEMON_TYPE_ICE: return 3;
            }
        }
        if (s == SPECIES_ID_ORICORIO) {
            switch (specialty) {
            case POKEMON_TYPE_GHOST: return 3;
            case POKEMON_TYPE_FIRE: return 0;
            case POKEMON_TYPE_ELECTRIC: return 1;
            case POKEMON_TYPE_PSYCHIC: return 2;
            }
        }
        if (s == SPECIES_ID_PALDEA_TAUROS) {
            switch (specialty) {
            case POKEMON_TYPE_FIRE: return 1;
            case POKEMON_TYPE_WATER: return 2;
            }
        }
        if (s == SPECIES_ID_SILVALLY || s == SPECIES_ID_ARCEUS) return specialty;
    }
    switch (s) {
    case SPECIES_ID_UNOWN: case SPECIES_ID_SHELLOS: case SPECIES_ID_GASTRODON: case SPECIES_ID_ROTOM:
    case SPECIES_ID_BASCULIN: case SPECIES_ID_DEERLING: case SPECIES_ID_SAWSBUCK: case SPECIES_ID_SCATTERBUG:
    case SPECIES_ID_SPEWPA: case SPECIES_ID_VIVILLON: case SPECIES_ID_FLABEBE: case SPECIES_ID_FLOETTE:
    case SPECIES_ID_FLORGES: case SPECIES_ID_FURFROU: case SPECIES_ID_PUMPKABOO: case SPECIES_ID_GOURGEIST:
    case SPECIES_ID_ORICORIO: case SPECIES_ID_ZARUDE: case SPECIES_ID_SQUAWKABILLY: case SPECIES_ID_PALDEA_TAUROS:
        return pr_rng_seed_int(rng, sp->n_forms, 0);
    case SPECIES_ID_SINISTEA: case SPECIES_ID_POLTEAGEIST: case SPECIES_ID_MAUSHOLD: case SPECIES_ID_DUDUNSPARCE:
    case SPECIES_ID_POLTCHAGEIST: case SPECIES_ID_SINISTCHA:
        return pr_rng_seed_int(rng, 16, 0) ? 0 : 1;
    case SPECIES_ID_PICHU:
        return pr_rng_seed_int(rng, 8, 0) ? 0 : 1;
    case SPECIES_ID_PIKACHU:   /* no Cosplay or Partner Pikachu for trainers before wave 30 */
        if (trainer_battle && ctx->wave < 30) return 0;
        return pr_rng_seed_int(rng, 8, 0);
    case SPECIES_ID_EEVEE:
        if (trainer_battle && ctx->wave < 30) return 0;
        return pr_rng_seed_int(rng, 2, 0);
    case SPECIES_ID_MAGEARNA: case SPECIES_ID_URSHIFU:
        return pr_rng_seed_int(rng, 2, 0);
    case SPECIES_ID_TATSUGIRI:
        return pr_rng_seed_int(rng, 3, 0);
    case SPECIES_ID_ZYGARDE:
        return pr_rng_seed_int(rng, 4, 0);
    case SPECIES_ID_MINIOR:
        return pr_rng_seed_int(rng, 7, 0);
    case SPECIES_ID_ALCREMIE:
        return pr_rng_seed_int(rng, 9, 0);
    case SPECIES_ID_MEOWSTIC: case SPECIES_ID_INDEEDEE: case SPECIES_ID_BASCULEGION: case SPECIES_ID_OINKOLOGNE:
        return gender == GENDER_FEMALE ? 1 : 0;
    case SPECIES_ID_TOXTRICITY:
        /* the Low Key form for these natures (the constructor asks before the nature is rolled, with
         * nature undefined: 0; a fusion asks with the Pokemon's nature) */
        switch (nature) {
        case NATURE_LONELY: case NATURE_BOLD: case NATURE_RELAXED: case NATURE_TIMID: case NATURE_SERIOUS:
        case NATURE_MODEST: case NATURE_MILD: case NATURE_QUIET: case NATURE_BASHFUL: case NATURE_CALM:
        case NATURE_GENTLE: case NATURE_CAREFUL:
            return 1;
        }
        return 0;
    case SPECIES_ID_GIMMIGHOUL:   /* gameMode.hasMysteryEncounters: CLASSIC and CHALLENGE */
        if (run->mode == GAME_MODES_CLASSIC || run->mode == GAME_MODES_CHALLENGE)
            return 1;
        return pr_rng_seed_int(rng, sp->n_forms, 0);
    case SPECIES_ID_ETERNATUS:
        if (!pr_run_is_battle_classic_final_boss(run, ctx->wave) && ctx->wave > 0 && !(ctx->wave % 1000))
            return 1;
        return 0;
    case SPECIES_ID_BURMY: case SPECIES_ID_WORMADAM:   /* the constructor: ignoreArena = isPlayer() = false */
        if (ignore_arena) return pr_rng_seed_int(rng, sp->n_forms, 0);
        switch (ctx->biome) {
        case BIOME_ID_BEACH: return 1;
        case BIOME_ID_SLUM: return 2;
        }
        return 0;
    case SPECIES_ID_LYCANROC:
        if (ignore_arena) return pr_rng_seed_int(rng, sp->n_forms, 0);
        switch (pr_run_get_time_of_day(ctx->biome, ctx->wave, ctx->wave_cycle_offset)) {
        case TIME_OF_DAY_DAWN: case TIME_OF_DAY_DAY: return 0;
        case TIME_OF_DAY_DUSK: return 2;
        case TIME_OF_DAY_NIGHT: return 1;
        }
        return 0;
    }
    return 0;
}

int pr_run_get_species_form_index(pr_run *run, const pr_enemy_gen_ctx *ctx, int gender)
{
    return form_index(&run->rng, run, ctx, ctx->species, gender, -1, false);
}

void pr_run_get_eternatus_moveset(int form, bool inverse_battle, int moveset[4])
{
    if (form) {
        moveset[0] = MOVE_ID_DYNAMAX_CANNON;
        moveset[1] = MOVE_ID_CROSS_POISON;
        moveset[2] = inverse_battle ? MOVE_ID_EARTH_POWER : MOVE_ID_FLAMETHROWER;
        moveset[3] = MOVE_ID_RECOVER;
    } else {
        moveset[0] = MOVE_ID_ETERNABEAM;
        moveset[1] = MOVE_ID_SLUDGE_BOMB;
        moveset[2] = inverse_battle ? MOVE_ID_EARTH_POWER : MOVE_ID_FLAMETHROWER;
        moveset[3] = MOVE_ID_COSMIC_POWER;
    }
}

/* generateFusionSpecies() for an enemy made by the constructor (level > 1, fused) */
static int generate_fusion(pr_run *run, const pr_enemy_gen_ctx *ctx, const pr_species_gen *base, pr_enemy_gen *out)
{
    /* the Ability Charm, skipped when this.hasTrainer(): still false inside the constructor */
    double chance = BASE_HIDDEN_ABILITY_RATE;
    if (ctx->hidden_ability_charm_stacks > 0) chance *= js_pow(2, -1 - ctx->hidden_ability_charm_stacks);
    const bool has_hidden = !seed_int(run, (int)chance);
    const int rand_ability = seed_int(run, 2);
    /* randomSpecies(waveIndex || 0, level, false, species.getCompatibleFusionSpeciesFilter(), true): the
     * catchable species the filter keeps, as they are (filterAllEvolutions), in registry order */
    static uint16_t list[1400];
    int n = 0;
    for (int i = 0; i < pr_species_gen_count; i++) {
        const pr_species_gen *c = &pr_species_gens[i];
        if (!c->catchable) continue;
        if (!(base->sub_legendary || base->legendary || base->mythical
              || (c->has_evolutions == base->has_evolutions && c->has_prevolution == base->has_prevolution)))
            continue;
        if (c->sub_legendary != base->sub_legendary || c->legendary != base->legendary || c->mythical != base->mythical)
            continue;
        if (!(base->trainer_forbidden || !c->trainer_forbidden)) continue;
        if (c->id == SPECIES_ID_DITTO) continue;
        list[n++] = c->id;
    }
    if (!n) { run->unported |= PR_RUN_UNDEFINED_UPSTREAM; return -1; }   /* randSeedItem([]) */
    const int fusion = list[n == 1 ? 0 : pr_rng_integer_in_range(&run->rng, 0, n - 1)];
    const pr_species_gen *fs = species_gen(fusion);
    out->fusion_species = fusion;
    out->fusion_ability_index = fs->has_hidden && has_hidden ? 2 : fs->same_abilities ? 0 : rand_ability;
    /* fusionGender: (id % 256) * 0.390625 < malePercent */
    if (fs->male_percent < 0) out->fusion_gender = GENDER_GENDERLESS;
    else out->fusion_gender = (out->id % 256) * 0.390625 < fs->male_percent ? GENDER_MALE : GENDER_FEMALE;
    /* getSpeciesFormIndex(fusionSpecies, fusionGender, getNature(), true) */
    out->fusion_form = form_index(&run->rng, run, ctx, fusion, out->fusion_gender, out->nature, true);
    return 0;
}

/* getTypes({includeTeraType: false, bypassSummonData: true, ignoreThirdType: true}) with a fusion:
 * getBaseTypes gives [type1, the fusion's type2 when set and not type1, else its type1 when not type1,
 * else type2 ?? UNKNOWN], then a Set without UNKNOWN when there are two */
static int fused_types(const pr_form_gen *f, const pr_form_gen *ff, int8_t out[2])
{
    int first = f->type1, second = f->type2 == -2 ? POKEMON_TYPE_UNKNOWN : f->type2;
    if (ff->type2 != -2 && ff->type2 != first) second = ff->type2;
    else if (ff->type1 != first) second = ff->type1;
    int n = 0;
    out[n++] = (int8_t)first;
    if (second != first) out[n++] = (int8_t)second;
    if (n == 2 && out[0] == POKEMON_TYPE_UNKNOWN) { out[0] = out[1]; n = 1; }
    else if (n == 2 && out[1] == POKEMON_TYPE_UNKNOWN) n = 1;
    return n;
}

int pr_run_enemy_types(const pr_enemy_gen *mon, int species, int8_t types[2])
{
    const pr_species_gen *sp = species_gen(species);
    if (!sp) return -1;
    const pr_form_gen *f = form_gen(sp, mon->form);
    if (!f) return -1;
    if (mon->fusion_species < 0) {
        types[0] = f->types[0];
        types[1] = f->types[1];
        return f->n_types;
    }
    const pr_species_gen *fs = species_gen(mon->fusion_species);
    const pr_form_gen *ff = fs ? form_gen(fs, mon->fusion_form) : NULL;
    if (!ff) return -1;
    return fused_types(f, ff, types);
}

int pr_run_enemy_pokemon_base_rng(pr_rng *rng, pr_run *run, const pr_enemy_gen_ctx *ctx, pr_enemy_gen *out)
{
    const pr_species_gen *sp = species_gen(ctx->species);
    if (!sp) { run->unported |= PR_RUN_UNPORTED_SPECIES; return -1; }
    out->fusion_species = -1;
    out->shiny = false;   /* EnemyPokemon passes shiny = false (no data source) */
    out->variant = 0;     /* ...and variant undefined: this.shiny ? generateShinyVariant() : 0 */
    out->boss_segments = 0;
    out->friendship = -1;
    out->rival_pending = false;
    out->rival_fight = out->rival_slot = 0;

    double chance = BASE_HIDDEN_ABILITY_RATE;
    if (ctx->hidden_ability_charm_stacks > 0) chance *= js_pow(2, -1 - ctx->hidden_ability_charm_stacks);
    int regular = sp->same_abilities ? 0 : pr_rng_seed_int(rng, 2, 0);
    bool hidden = sp->has_hidden ? !pr_rng_seed_int(rng, (int)chance, 0) : false;
    out->ability_index = hidden ? 2 : regular;

    out->id = (uint32_t)floor(pr_rng_real_in_range(rng, 0, 4294967296.0));
    const uint32_t id = out->id;
    out->ivs[0] = (id & 0x3e000000u) >> 25;
    out->ivs[1] = (id & 0x01f00000u) >> 20;
    out->ivs[2] = (id & 0x000f8000u) >> 15;
    out->ivs[3] = (id & 0x00007c00u) >> 10;
    out->ivs[4] = (id & 0x000003e0u) >> 5;
    out->ivs[5] = id & 0x0000001fu;

    if (sp->male_percent < 0) out->gender = GENDER_GENDERLESS;
    else out->gender = pr_rng_frac(rng) * 100 <= sp->male_percent ? GENDER_MALE : GENDER_FEMALE;

    bool ignore_arena = ctx->trainer_slot == 0 && ctx->wave < 0;
    /* Daily starters use a temporary offset RNG. Form rolls must advance that same
     * stream, rather than consuming the run's saved outer RNG. */
    out->form = form_index(rng, run, ctx, ctx->species, out->gender, -1, ignore_arena);

    if (ctx->trainer_slot == 0 && ctx->wave < 0) {
        /* Player starter: trySetShiny() check, then nature */
        const uint32_t f = (out->id >> 16) ^ (out->id & 0xffffu);
        out->shiny = f < BASE_SHINY_CHANCE;
        if (out->shiny) {
            const pr_form_gen *fg = form_gen(sp, out->form);
            if (fg && fg->has_variants) {
                pr_rng r = pr_run_seed_offset_rng(run, (int)(out->id & 0xffffu), &run->wave_seed);
                int rand = pr_rng_seed_int(&r, 10, 0);
                out->variant = rand >= SHINY_VARIANT_CHANCE ? 0 : rand >= SHINY_EPIC_CHANCE ? 1 : 2;
            }
        }
        out->nature = pr_rng_seed_int(rng, PR_NATURE_COUNT, 0);
        out->luck = out->shiny ? out->variant + 1 : 0;

        /* PlayerPokemon rolls teraType after nature. Daily starter generation shares one
         * offset-0 RNG across all three starters, so omitting this roll for a dual-type
         * starter shifts the RNG sequence of every starter that follows. */
        int8_t types[2];
        int n_types = pr_run_enemy_types(out, ctx->species, types);
        if (n_types < 0) { run->unported |= PR_RUN_UNDEFINED_UPSTREAM; return -1; }
        out->tera_type = n_types == 1 ? types[0] : types[pr_rng_integer_in_range(rng, 0, n_types - 1)];
        return 0;
    }

    out->nature = pr_rng_seed_int(rng, PR_NATURE_COUNT, 0);

    for (int i = 0; i < 4; i++) out->moveset[i] = 0;
    if (ctx->species == SPECIES_ID_ETERNATUS) {
        bool inverse = run->has_challenges && (run->challenge_flags & (1u << CHALLENGES_INVERSE_BATTLE));
        pr_run_get_eternatus_moveset(out->form, inverse, out->moveset);
    }

    if (ctx->level > 1) {
        bool fused = run->mode == GAME_MODES_SPLICED_ENDLESS;
        if (!fused && ctx->enemy_fusion_stacks > 0)
            fused = !(pr_rng_frac(rng) > ctx->enemy_fusion_chance * ctx->enemy_fusion_stacks);
        if (fused && generate_fusion(run, ctx, sp, out) < 0) return -1;
    }
    out->luck = 0;

    int8_t types[2];
    const int n_types = pr_run_enemy_types(out, ctx->species, types);
    if (n_types < 0) { run->unported |= PR_RUN_UNDEFINED_UPSTREAM; return -1; }
    out->tera_type = n_types == 1 ? types[0] : types[pr_rng_integer_in_range(rng, 0, n_types - 1)];

    if (ctx->boss) {
        if (ctx->species == SPECIES_ID_ETERNATUS && pr_run_is_battle_classic_final_boss(run, ctx->wave))
            out->boss_segments = (out->form == 1) ? 5 : 4;
        else
            out->boss_segments = pr_run_get_encounter_boss_segments(run, ctx->wave, ctx->level, ctx->species, true);
    }
    return 0;
}

int pr_run_enemy_pokemon_base(pr_run *run, const pr_enemy_gen_ctx *ctx, pr_enemy_gen *out)
{
    return pr_run_enemy_pokemon_base_rng(&run->rng, run, ctx, out);
}

int pr_run_enemy_pokemon_finish(pr_run *run, const pr_enemy_gen_ctx *ctx, pr_enemy_gen *out)
{
    const pr_species_gen *sp = species_gen(ctx->species);
    if (!sp) { run->unported |= PR_RUN_UNPORTED_SPECIES; return -1; }
    /* Rival genPartyMember runs inside executeWithSeedOffset for the whole constructor + postProcess.
     * Generic enemies use the ambient run RNG; rival members resume the saved per-slot offset RNG. */
    pr_rng *finish_rng = out->rival_pending ? &out->rival_rng : &run->rng;
    const bool has_trainer = ctx->trainer_slot != 0;   /* now this.trainerSlot is set */
    if (ctx->shiny_lock) {
        out->shiny = false;
    } else {
        /* trySetShiny(): (E ^ F) < threshold, E = trainerId ^ secretId, F = the id's halves xored */
        const uint32_t e = (uint32_t)(ctx->trainer_id ^ ctx->secret_id), f = (out->id >> 16) ^ (out->id & 0xffffu);
        double threshold = BASE_SHINY_CHANCE;
        if (ctx->event_active) {
            if (has_trainer && ctx->event_trainer_shiny_chance > 0)
                threshold = fmax(ctx->event_trainer_shiny_chance, threshold);
            else
                threshold *= ctx->event_shiny_multiplier;
        }
        if (!has_trainer && ctx->shiny_charm_stacks > 0) threshold *= js_pow(2, 1 + ctx->shiny_charm_stacks);
        out->shiny = (e ^ f) < threshold;
    }
    if (out->shiny) {
        /* generateShinyVariant(): 0 without variant data for "<id>-<formKey>" or "<id>"; else
         * randSeedInt(10) on the wave seed at offset id (shiftCharCodes wraps each code unit to 16
         * bits, so the id's low 16 bits are the whole offset) */
        const pr_form_gen *fg = form_gen(sp, out->form);
        if (!fg) { run->unported |= PR_RUN_UNDEFINED_UPSTREAM; return -1; }
        if (!fg->has_variants) {
            out->variant = 0;
        } else {
            pr_rng r = pr_run_seed_offset_rng(run, (int)(out->id & 0xffffu), &run->wave_seed);
            int rand = pr_rng_seed_int(&r, 10, 0);
            out->variant = rand >= SHINY_VARIANT_CHANCE ? 0 : rand >= SHINY_EPIC_CHANCE ? 1 : 2;
        }
    }
    out->luck = out->shiny ? out->variant + 1 : 0;

    /* applyCustomDailyBossConfig / applyCustomDailyConfig: a custom daily seed's boss and forced hidden
     * abilities (both return at once without one) */
    if (run->mode == GAME_MODES_DAILY && run->daily_custom) { run->unported |= PR_RUN_UNPORTED_DAILY; return -1; }

    /* A trainer's Pokemon: six randSeedIntRange(floor(waveIndex / 10), 31) IVs (from wave 320 the
     * range is empty and randSeedInt returns its minimum, above 31, without rolling), and friendship */
    if (has_trainer) {
        const int lo = ctx->wave / 10;
        for (int i = 0; i < 6; i++) out->ivs[i] = pr_rng_seed_int(finish_rng, 31 - lo + 1, lo);
        double fr = js_round(255 * ((double)ctx->wave / TRAINER_MAX_FRIENDSHIP_WAVE));
        out->friendship = (int)(fr < TRAINER_MIN_FRIENDSHIP ? TRAINER_MIN_FRIENDSHIP : fr > 255 ? 255 : fr);
    }
    /* applyChallenges(MOVESET_MODIFY) */
    if (pr_run_has_unported_challenges(run)) { run->unported |= PR_RUN_UNPORTED_CHALLENGES; return -1; }

    /* addEnemyPokemon, after the constructor: a boss rolls a second id and each IV moves 3/4 of the
     * way from the lower to the higher of the two, rounded:
     * Math.round(Phaser.Math.Linear(min, max, 0.75)) with Linear(p0, p1, t) = (p1 - p0) * t + p0 */
    if (ctx->boss) {
        const uint32_t id2 = (uint32_t)floor(pr_rng_real_in_range(finish_rng, 0, 4294967296.0));
        const int iv2[6] = {(id2 & 0x3e000000u) >> 25, (id2 & 0x01f00000u) >> 20, (id2 & 0x000f8000u) >> 15,
                            (id2 & 0x00007c00u) >> 10, (id2 & 0x000003e0u) >> 5, id2 & 0x0000001fu};
        for (int i = 0; i < 6; i++) {
            const double lo = out->ivs[i] < iv2[i] ? out->ivs[i] : iv2[i];
            const double hi = out->ivs[i] < iv2[i] ? iv2[i] : out->ivs[i];
            out->ivs[i] = (int)js_round((hi - lo) * 0.75 + lo);
        }
    }
    if (ctx->species == SPECIES_ID_ETERNATUS && pr_run_is_battle_classic_final_boss(run, ctx->wave)) {
        for (int i = 0; i < 6; i++) out->ivs[i] = 31;
    }
    if (out->rival_pending) pr_run_rival_finish(run, ctx, out);
    return 0;
}
