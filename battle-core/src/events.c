/* Messages and the event list the game screen plays back (include/pr/events.h). */
#include "pr/engine.h"
#include <string.h>

static pr_msg msgn(const char *key, int n, const pr_msg_arg *a)
{
    pr_msg m;
    memset(&m, 0, sizeof m);
    m.key = key;
    for (int i = 0; i < n && i < PR_MSG_MAX_ARGS; i++) m.args[m.n_args++] = a[i];
    return m;
}

pr_msg pr_msg0(const char *key) { return msgn(key, 0, NULL); }
pr_msg pr_msg1(const char *key, pr_msg_arg a) { return msgn(key, 1, &a); }
pr_msg pr_msg2(const char *key, pr_msg_arg a, pr_msg_arg b)
{
    pr_msg_arg v[] = {a, b};
    return msgn(key, 2, v);
}
pr_msg pr_msg3(const char *key, pr_msg_arg a, pr_msg_arg b, pr_msg_arg c)
{
    pr_msg_arg v[] = {a, b, c};
    return msgn(key, 3, v);
}
pr_msg pr_msg4(const char *key, pr_msg_arg a, pr_msg_arg b, pr_msg_arg c, pr_msg_arg d)
{
    pr_msg_arg v[] = {a, b, c, d};
    return msgn(key, 4, v);
}

static pr_msg_arg arg(const char *name, uint8_t kind, uint32_t v, const char *s)
{
    pr_msg_arg a = {name, kind, v, s, 0};
    return a;
}

/* Battle.isClassicFinalBoss: gameMode.isClassic && isWaveFinal(wave) */
static bool classic_final_boss(const pr_battle *bt)
{
    return bt->game_mode == GAME_MODES_CLASSIC && bt->wave_index == 200;
}

static uint32_t pokemon_v(const pr_battle *bt, const pr_pokemon *p)
{
    if (!p) return pr_msg_pokemon(0, 0, 0);   /* "MissingNo." */
    uint8_t flags = 0;
    if (!p->is_player) {
        flags |= PR_MSG_POKEMON_ENEMY;
        if (p->has_trainer || classic_final_boss(bt)) flags |= PR_MSG_POKEMON_FOE;
    }
    return pr_msg_pokemon(p->illusion.active ? p->illusion.species : p->species,
                          p->illusion.active ? p->illusion.form : p->form, flags);
}

pr_msg_arg pr_arg_pokemon(const pr_battle *bt, const char *name, const pr_pokemon *p)
{
    pr_msg_arg a = arg(name, PR_ARG_POKEMON, pokemon_v(bt, p), NULL);
    if (p) {
        uint16_t species = p->illusion.active ? p->illusion.fusion_species : p->fusion_species;
        if (species) a.fusion_v = pr_msg_pokemon(species,
                            p->illusion.active ? p->illusion.fusion_form : p->fusion_form, 0);
    }
    return a;
}
pr_msg_arg pr_arg_pokemon_name(const pr_battle *bt, const char *name, const pr_pokemon *p)
{
    pr_msg_arg a = pr_arg_pokemon(bt, name, p);
    a.kind = PR_ARG_POKEMON_NAME;
    return a;
}
pr_msg_arg pr_arg_int(const char *name, int32_t v) { return arg(name, PR_ARG_INT, (uint32_t)v, NULL); }
pr_msg_arg pr_arg_int64(const char *name, int64_t v)
{
    uint64_t bits = (uint64_t)v;
    pr_msg_arg a = arg(name, PR_ARG_INT64, (uint32_t)bits, NULL);
    a.fusion_v = (uint32_t)(bits >> 32);
    return a;
}
pr_msg_arg pr_arg_money_en_us(const char *name, double v)
{
    uint64_t bits;
    memcpy(&bits, &v, sizeof bits);
    pr_msg_arg a = arg(name, PR_ARG_MONEY_EN_US, (uint32_t)bits, NULL);
    a.fusion_v = (uint32_t)(bits >> 32);
    return a;
}
pr_msg_arg pr_arg_move(const char *name, int move) { return arg(name, PR_ARG_MOVE, (uint32_t)move, NULL); }
pr_msg_arg pr_arg_ability(const char *name, int ability) { return arg(name, PR_ARG_ABILITY, (uint32_t)ability, NULL); }
pr_msg_arg pr_arg_type(const char *name, int type) { return arg(name, PR_ARG_TYPE, (uint32_t)type, NULL); }
pr_msg_arg pr_arg_stat(const char *name, int stat) { return arg(name, PR_ARG_STAT, (uint32_t)stat, NULL); }
pr_msg_arg pr_arg_stats(const char *name, const int8_t *stats, int n)
{
    return arg(name, PR_ARG_STATS, pr_msg_stats(stats, n), NULL);
}
pr_msg_arg pr_arg_key(const char *name, const char *key) { return arg(name, PR_ARG_KEY, 0, key); }

pr_msg pr_msg_p(const pr_battle *bt, const char *key, const pr_pokemon *p)
{
    return pr_msg1(key, pr_arg_pokemon(bt, "pokemonNameWithAffix", p));
}

pr_event *pr_event_add(pr_battle *bt, pr_engine *e, int type, int battler, int target, int id, int32_t value,
                  const pr_msg *m)
{
    if (!e) return NULL;
    /* Older callers may reset the public diagnostic count directly. */
    if (e->n_events_acknowledged > e->n_events) e->n_events_acknowledged = 0;
    if (e->n_events >= PR_MAX_EVENTS) {
        int pending = e->n_events - e->n_events_acknowledged;
        if (pending && !bt->event_sink) { e->events_overflow = true; return NULL; }
        if (pending) bt->event_sink(e->events + e->n_events_acknowledged, pending, bt->event_sink_ctx);
        e->n_events = e->n_events_acknowledged = 0;
    }
    pr_event *ev = &e->events[e->n_events++];
    memset(ev, 0, sizeof *ev);
    ev->type = (uint8_t)type;
    ev->battler = (int8_t)battler;
    ev->target = (int8_t)target;
    ev->id = (uint16_t)id;
    ev->value = value;
    if (m) ev->msg = *m;
    ev->weather = bt->weather;
    ev->terrain = bt->terrain;
    for (int i = 0; i < 4; i++) {
        const pr_pokemon *p = bt->field[i];
        pr_event_field *f = &ev->field[i];
        f->party_slot = -1;
        if (!p) continue;
        f->pokemon_id_known = true;
        f->pokemon_id = p->id;
        /* PR_DEV_ILLUSION: the field shows a disguise (getSpeciesForm(false, useIllusion)) */
        const bool look = pr_shows_illusion(bt, p);
        const pr_form *form = pr_shown_form(bt, p);
        f->species = form->species;
        f->form = form->form_index;
        int side = i < 2 ? 0 : 1;
        for (int k = 0; k < bt->n_party[side]; k++)
            if (bt->party[side][k] == p) f->party_slot = (int8_t)k;
        f->hp = p->hp;
        f->max_hp = p->stats[STAT_HP];
        f->status = p->status;
        f->fainted = p->hp <= 0;
        f->substitute = pr_get_battler_tag(p, BATTLER_TAG_TYPE_SUBSTITUTE) != NULL;
        /* SemiInvulnerableTag.onAdd hides the sprite, onRemove shows it again; leaveField hides it */
        f->visible = p->on_field && !pr_find_battler_tag_class(p, PR_TAGCLASS_SEMI_INVULNERABLE);
        f->gender = (int8_t)(look ? pr_apparent_gender(p, true) : pr_get_gender(p));
        f->variant = p->variant;
        f->sprite_scale = (float)pr_get_sprite_scale(p);
        int8_t types[PR_MAX_TYPES]; int n = pr_get_types(p, types, look ? PR_TYPES_USE_ILLUSION : 0);
        for (int j = 0; j < n && j < 3; j++) f->types[f->n_types++] = types[j];
    }
    if (type == PR_EV_TRANSFORM && battler >= 0 && battler < 4 && bt->field[battler]) {
        const pr_pokemon *p = bt->field[battler];
        ev->transform.gender = (int8_t)pr_get_gender(p);
        ev->transform.sprite_scale = (float)pr_get_sprite_scale(p);
        int8_t types[PR_MAX_TYPES]; int n = pr_get_types(p, types, 0);
        for (int i = 0; i < n && i < 3; i++) ev->transform.types[ev->transform.n_types++] = types[i];
        memcpy(ev->transform.stat_stages, p->stat_stages, sizeof p->stat_stages);
        ev->transform.n_moves = PR_N_MOVES(p);
        for (int i = 0; i < PR_N_MOVES(p) && i < 4; i++) {
            ev->transform.moves[i] = PR_MOVES(p)[i];
            ev->transform.pp_max[i] = PR_MAX_PP(p)[i] > 0 ? PR_MAX_PP(p)[i] : pr_moves[PR_MOVES(p)[i]].pp;
            ev->transform.pp_left[i] = ev->transform.pp_max[i] - PR_PP_USED(p)[i];
        }
    }
    return ev;
}

void pr_event_acknowledge(pr_engine *e)
{
    if (e) e->n_events_acknowledged = e->n_events;
}

void pr_show_text(pr_battle *bt, pr_engine *e, pr_msg m)
{
    pr_event_add(bt, e, PR_EV_TEXT, -1, -1, 0, 0, &m);
}

/* ---- keys chosen by an enum value, as upstream's get...Message helpers pick them ---- */

/* status-effect.ts getStatusEffectMessageKey(effect) + "." + kind; "" for NONE, where the
 * helpers return "" */
const char *pr_status_key(int effect, int kind)
{
#define ST(s) {"statusEffect:" s ".obtain", "statusEffect:" s ".obtainSource", "statusEffect:" s ".activation", \
               "statusEffect:" s ".overlap", "statusEffect:" s ".heal", "statusEffect:" s ".description"}
    static const char *const keys[][6] = {ST("none"), ST("poison"), ST("toxic"), ST("paralysis"), ST("sleep"),
                                          ST("freeze"), ST("burn")};
#undef ST
    if (effect == STATUS_EFFECT_NONE) return "";
    int row;
    switch (effect) {
    case STATUS_EFFECT_POISON: row = 1; break;
    case STATUS_EFFECT_TOXIC: row = 2; break;
    case STATUS_EFFECT_PARALYSIS: row = 3; break;
    case STATUS_EFFECT_SLEEP: row = 4; break;
    case STATUS_EFFECT_FREEZE: row = 5; break;
    case STATUS_EFFECT_BURN: row = 6; break;
    default: row = 0; break;
    }
    return keys[row][kind];
}

pr_msg pr_status_msg(const pr_battle *bt, int effect, int kind, const pr_pokemon *p)
{
    return pr_msg_p(bt, pr_status_key(effect, kind), p);
}

/* weather.ts: getWeatherStartMessage, ...LapseMessage, ...ClearMessage, getLegendaryWeatherContinuesMessage,
 * getWeatherBlockMessage and getWeatherDamageMessage ("" where they return "") */
const char *pr_weather_key(int weather, int kind)
{
    static const char *const start[] = {"", "weather:sunnyStartMessage", "weather:rainStartMessage",
        "weather:sandstormStartMessage", "weather:hailStartMessage", "weather:snowStartMessage", "weather:fogStartMessage",
        "weather:heavyRainStartMessage", "weather:harshSunStartMessage", "weather:strongWindsStartMessage"};
    static const char *const lapse[] = {"", "weather:sunnyLapseMessage", "weather:rainLapseMessage",
        "weather:sandstormLapseMessage", "weather:hailLapseMessage", "weather:snowLapseMessage", "weather:fogLapseMessage",
        "weather:heavyRainLapseMessage", "weather:harshSunLapseMessage", "weather:strongWindsLapseMessage"};
    static const char *const clear[] = {"", "weather:sunnyClearMessage", "weather:rainClearMessage",
        "weather:sandstormClearMessage", "weather:hailClearMessage", "weather:snowClearMessage", "weather:fogClearMessage",
        "weather:heavyRainClearMessage", "weather:harshSunClearMessage", "weather:strongWindsClearMessage"};
    if (weather < 0 || weather > WEATHER_TYPE_STRONG_WINDS) return "";
    switch (kind) {
    case PR_WEATHER_START: return start[weather];
    case PR_WEATHER_LAPSE: return lapse[weather];
    case PR_WEATHER_CLEAR: return clear[weather];
    case PR_WEATHER_CONTINUE:
        return weather == WEATHER_TYPE_HARSH_SUN ? "weather:harshSunContinueMessage"
             : weather == WEATHER_TYPE_HEAVY_RAIN ? "weather:heavyRainContinueMessage"
             : weather == WEATHER_TYPE_STRONG_WINDS ? "weather:strongWindsContinueMessage" : "";
    case PR_WEATHER_BLOCK:
        return weather == WEATHER_TYPE_HARSH_SUN ? "weather:harshSunEffectMessage"
             : weather == WEATHER_TYPE_HEAVY_RAIN ? "weather:heavyRainEffectMessage" : "weather:defaultEffectMessage";
    case PR_WEATHER_DAMAGE:
        return weather == WEATHER_TYPE_SANDSTORM ? "weather:sandstormDamageMessage"
             : weather == WEATHER_TYPE_HAIL ? "weather:hailDamageMessage" : "";
    }
    return "";
}

/* terrain.ts: getTerrainStartMessage, getTerrainClearMessage, getTerrainName */
const char *pr_terrain_key(int terrain, int kind)
{
    static const char *const keys[][3] = {{"", "", ""},
        {"terrain:mistyStartMessage", "terrain:mistyClearMessage", "terrain:misty"},
        {"terrain:electricStartMessage", "terrain:electricClearMessage", "terrain:electric"},
        {"terrain:grassyStartMessage", "terrain:grassyClearMessage", "terrain:grassy"},
        {"terrain:psychicStartMessage", "terrain:psychicClearMessage", "terrain:psychic"}};
    if (terrain < 0 || terrain > TERRAIN_PSYCHIC) return "";
    return keys[terrain][kind];
}

/* getTerrainBlockMessage(pokemon, terrain) */
pr_msg pr_terrain_block_msg(const pr_battle *bt, int terrain, const pr_pokemon *p)
{
    if (terrain == TERRAIN_MISTY) return pr_msg_p(bt, "terrain:mistyBlockMessage", p);
    if (terrain == TERRAIN_NONE) return pr_msg0("");
    return pr_msg2("terrain:defaultBlockMessage", pr_arg_pokemon(bt, "pokemonNameWithAffix", p),
                   pr_arg_key("terrainName", pr_terrain_key(terrain, PR_TERRAIN_NAME)));
}
