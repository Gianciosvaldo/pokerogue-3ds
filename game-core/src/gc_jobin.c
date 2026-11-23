/* Jobin's beetle duel (a Mystery Encounter of lorenzo's own for Bizarre runs; his spec and answers 2026-11-23, custom policy, no upstream).
 *
 * Jobin offers a gentleman's bet: a 1v1 duel of "grip" between two Bug-type beetles (BEETLE_POOL), played with four moves (Horn Lock, Dig In, Flank, Stand
 * Trick) and two meters: each beetle's Grip (100% to start, the HP bar of the duel) and the Suspicion (0% to start, shared). A beetle at 0% Grip is flipped;
 * 100% Suspicion and Jobin calls the player out. Levels, held items, EVs, IVs and abilities play no part: the duel is these tables and nothing else.
 *
 * The rules below are the spec's tables, number for number (the player's action against Jobin's, then the three changes). What the spec left open and
 * this file decides, each marked "default" so the owner can change it in one place:
 *   - Jobin's three normal moves come up with equal odds, the Speed King attack among the three also equal (answered 2026-11-23).
 *   - the first telegraph can come after round 1 (a round is special only when announced the round before, so round 1 never is).
 *   - a bluff counts as a telegraph for "not two rounds in a row".
 *   - the Stand Trick streak resets on any other move (the spec: "consecutive").
 *   - against a bluffed telegraph Jobin plays the normal move, so a Stand Trick is the ordinary row (0 / -35 / +35).
 *   - after a simultaneous 0% Grip the round's own Suspicion change stands (the spec: "suspicion remains the same" is read as: the rematch adds none).
 *   - Suspicion reaching 100% wins over a flip in the same round (he still fights, spec 2026-11-23). */
#include "gc/achv.h"
#include "internal.h"
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

enum { M_HORN, M_DIG, M_FLANK, M_STAND };                  /* the four moves, the player's and (the first three) Jobin's */
enum { K_HORN, K_DASH, K_SAP };                            /* Superheated Horn, Thermal Dash, Bubbling Sap */
#define KING_PERCENT 33                                    /* "0.33 chance per round" (2026-11-23) */
#define BLUFF_PERCENT 25                                   /* the telegraph is a bluff 1 time in 4 */
#define REMATCH_GRIP 50                                    /* after both flipped at once (changed from 75 by lorenzo 2026-11-23) */

static const uint16_t POOL[GC_JOBIN_POOL] = {SPECIES_ID_HERACROSS, SPECIES_ID_PINSIR, SPECIES_ID_SCIZOR, SPECIES_ID_VIKAVOLT, SPECIES_ID_GOLISOPOD,
                                             SPECIES_ID_ORBEETLE, SPECIES_ID_LOKIX, SPECIES_ID_RABSCA, SPECIES_ID_KLEAVOR, SPECIES_ID_LEDIAN, SPECIES_ID_CRUSTLE};

int gc_jobin_pool_species(int i) { return i >= 0 && i < GC_JOBIN_POOL ? POOL[i] : -1; }

bool gc_jobin_in_pool(int species)
{
    for (int i = 0; i < GC_JOBIN_POOL; i++) if (POOL[i] == species) return true;
    return false;
}

/* ---------------------------------------------------------------- the tables */
typedef struct { int8_t you, him, susp; } delta;

/* [player's move][Jobin's move], Horn Lock, Dig In, Flank (the spec's STANDARD MOVES table; the Stand Trick row is below) */
static const delta NORMAL[3][3] = {
    {{-10, -10, -10}, {-15, +15, 0}, {0, -30, 0}},
    {{+15, -15, 0}, {0, 0, -10}, {-25, 0, 0}},
    {{-30, 0, 0}, {0, -25, 0}, {-10, -10, -10}},
};
/* [Speed King attack][player's move], Horn Lock, Dig In, Flank, Stand Trick (JOBIN'S SPEED KING MOVES). The Stand Trick's suspicion is the streak's. */
static const delta KING[3][4] = {
    {{-25, -5, 0}, {+10, -20, 0}, {-45, 0, 0}, {0, -15, 35}},   /* Superheated Horn */
    {{0, -20, 0}, {-40, 0, 0}, {-20, -5, 0}, {0, -15, 35}},     /* Thermal Dash */
    {{-40, +15, 0}, {-25, 0, 0}, {0, -10, 0}, {0, -15, 35}},    /* Bubbling Sap */
};
static const int8_t BLUFF_MOVE[3] = {M_FLANK, M_DIG, M_HORN};   /* the normal move that beats the player's counter: Horn -> Flank, Dash -> Dig In, Sap -> Horn Lock */

int gc_jobin_stand_suspicion(int streak) { return streak <= 1 ? 35 : streak == 2 ? 50 : 70; }   /* +35, +50, +70, and +70 after that */

void gc_jobin_normal_delta(int player, int jobin, int *you, int *him, int *susp)
{
    const delta d = NORMAL[player][jobin];
    *you = d.you; *him = d.him; *susp = d.susp;
}

void gc_jobin_king_delta(int king, int player, int *you, int *him, int *susp)
{
    const delta d = KING[king][player];
    *you = d.you; *him = d.him; *susp = d.susp;
}

int gc_jobin_bluff_move(int king) { return BLUFF_MOVE[king]; }

static int clamp100(int v) { return v < 0 ? 0 : v > 100 ? 100 : v; }

/* One round: the player's move against Jobin's action (`king` >= 0: his Speed King attack, else `jobin` the normal move). The meters move, the result is
 * what happened. `streak` is the player's consecutive Stand Tricks before this round. */
void gc_jobin_round(gc_jobin_state *s, int player, int jobin, int king)
{
    int you, him, susp;
    if (player == M_STAND) {
        s->streak++;
        s->used_stand = true;
        you = 0;
        him = king >= 0 ? KING[king][M_STAND].him : -35;
        susp = gc_jobin_stand_suspicion(s->streak);
    } else {
        s->streak = 0;
        if (king >= 0) gc_jobin_king_delta(king, player, &you, &him, &susp);
        else gc_jobin_normal_delta(player, jobin, &you, &him, &susp);
    }
    const int before_you = s->grip_you, before_him = s->grip_jobin, before_susp = s->susp;
    s->grip_you = (int16_t)clamp100(s->grip_you + you);
    s->grip_jobin = (int16_t)clamp100(s->grip_jobin + him);
    s->susp = (int16_t)clamp100(s->susp + susp);
    /* what the meters really moved (the caps swallow some): what the line says */
    s->d_you = (int8_t)(s->grip_you - before_you);
    s->d_him = (int8_t)(s->grip_jobin - before_him);
    s->d_susp = (int8_t)(s->susp - before_susp);
}

/* the end of a round: 0 goes on, 1 the player's beetle is flipped, 2 Jobin's, 3 both at once (the rematch), 4 caught (100% Suspicion, beats the rest) */
int gc_jobin_outcome(const gc_jobin_state *s)
{
    if (s->susp >= 100) return 4;
    if (s->grip_you <= 0 && s->grip_jobin <= 0) return 3;
    if (s->grip_you <= 0) return 1;
    if (s->grip_jobin <= 0) return 2;
    return 0;
}

/* ---------------------------------------------------------------- texts */
static const char *const MOVE_NAMES[4] = {"Horn Lock", "Dig In", "Flank", "Stand Trick"};
static const char *const KING_NAMES[3] = {"Superheated Horn", "Thermal Dash", "Bubbling Sap"};
const char *gc_jobin_move_name(int m) { return m >= 0 && m < 4 ? MOVE_NAMES[m] : "?"; }
const char *gc_jobin_king_name(int k) { return k >= 0 && k < 3 ? KING_NAMES[k] : "?"; }

#define JB "mysteryEncounters/jobin:"
static const char *const NORMAL_LINE[3][3] = {
    {JB "round.hornLock.hornLock", JB "round.hornLock.digIn", JB "round.hornLock.flank"},
    {JB "round.digIn.hornLock", JB "round.digIn.digIn", JB "round.digIn.flank"},
    {JB "round.flank.hornLock", JB "round.flank.digIn", JB "round.flank.flank"},
};
static const char *const KING_LINE[3][4] = {
    {JB "round.superheatedHorn.hornLock", JB "round.superheatedHorn.digIn", JB "round.superheatedHorn.flank", JB "round.superheatedHorn.standTrick"},
    {JB "round.thermalDash.hornLock", JB "round.thermalDash.digIn", JB "round.thermalDash.flank", JB "round.thermalDash.standTrick"},
    {JB "round.bubblingSap.hornLock", JB "round.bubblingSap.digIn", JB "round.bubblingSap.flank", JB "round.bubblingSap.standTrick"},
};
static const char *const TELEGRAPH_LINE[3] = {JB "telegraph.superheatedHorn", JB "telegraph.thermalDash", JB "telegraph.bubblingSap"};   /* the spec's lines */

/* every line waits for A (the spec: "the player needs to click A after every important piece of information") */
static gc_event *say(gc_run *run, const char *key, int n, ...)
{
    gc_event *ev = gc_ev_new(run, GC_EV_MESSAGE);
    if (!ev) return NULL;
    ev->msg.key = key;
    va_list ap;
    va_start(ap, n);
    for (int i = 0; i < n; i++) {
        const pr_msg_arg a = va_arg(ap, pr_msg_arg);
        if (i < PR_MSG_MAX_ARGS) ev->msg.args[ev->msg.n_args++] = a;
    }
    va_end(ap);
    ev->prompt = true;
    return ev;
}

static pr_msg_arg species_arg(const char *name, int species, int form)
{
    pr_msg_arg a = pr_arg_int(name, 0);
    a.kind = PR_ARG_POKEMON_NAME;
    a.v = pr_msg_pokemon((uint16_t)species, (uint8_t)form, 0);
    return a;
}

/* a text argument that outlives the call (the events are played later) */
static pr_msg_arg str_arg(const char *name, const char *text)
{
    static char ring[32][24];
    static unsigned next;
    char *p = ring[next++ % 32];
    snprintf(p, 24, "%s", text);
    pr_msg_arg a = pr_arg_int(name, 0);
    a.kind = PR_ARG_STR;
    a.s = p;
    return a;
}

static pr_msg_arg delta_arg(const char *name, int d)
{
    char t[24];
    if (d > 0) snprintf(t, sizeof t, "+%d%%", d);
    else if (d < 0) snprintf(t, sizeof t, "%d%%", d);
    else snprintf(t, sizeof t, "no change");
    return str_arg(name, t);
}

/* the HUD: the meters now (a, b, c), `what` in hp's low byte and the Speed King's attack above it */
static void hud(gc_run *run, int what, int king)
{
    const gc_jobin_state *s = &gc_int(run)->jobin;
    gc_event *ev = gc_ev_new(run, GC_EV_JOBIN);
    if (!ev) return;
    ev->a = s->grip_you;
    ev->b = s->grip_jobin;
    ev->c = s->susp;
    ev->hp = what | (king << 8);
}

/* ---------------------------------------------------------------- the beetles */
/* Jobin's beetle: the wave's own stream, so the first screen and the duel agree without anything drawn from the run */
int gc_jobin_roll_species(const gc_run *run)
{
    pr_rng r = pr_run_seed_offset_rng(&run->run, run->wave + 0x4A4F, NULL);
    return POOL[pr_rng_seed_int(&r, GC_JOBIN_POOL, 0)];
}

bool gc_jobin_pick_ok(const gc_run *run, int option, int idx)
{
    if (idx < 0 || idx >= run->n_party) return false;
    switch (run->screen.me_stage) {
    case GC_ME_STAGE_FIRST: {
        /* his own beetle: a pool species in the party (a fainted one too), never the one Jobin drew (no same-species duel) */
        if (option != 0) return false;
        const int sp = run->party[idx].b.species;
        return gc_jobin_in_pool(sp) && sp != gc_jobin_roll_species(run);
    }
    case GC_ME_STAGE_JOBIN_KEEP: return option == 0 && run->n_party >= 6;   /* the member the beetle replaces */
    case GC_ME_STAGE_JOBIN_CHARCOAL: return option == 0;                    /* any Pokemon */
    default: return false;
    }
}

bool gc_jobin_own_available(const gc_run *run)
{
    for (int i = 0; i < run->n_party; i++) {
        const int sp = run->party[i].b.species;
        if (gc_jobin_in_pool(sp) && sp != gc_jobin_roll_species(run)) return true;
    }
    return false;
}

/* a beetle at the wave's level, a Pokemon of the party's kind: the wild-style constructor (gc_gts.c), then a player's. `variant` >= 0: shiny of that variant. */
static bool make_beetle(gc_run *run, int species, int variant, gc_mon *out)
{
    gc_internal *in = gc_int(run);
    pr_rng r = in->jobin.gen;
    pr_rng lv = in->jobin.gen;
    const int level = pr_run_get_level_for_wave(&run->run, &lv, run->wave);
    if (!gc_new_enemy_mon(run, &r, species, level > 0 ? level : 5, out)) return false;
    out->b.is_player = true;
    out->b.has_trainer = false;
    out->b.on_field = false;
    out->b.switch_out_status = true;
    out->b.is_boss = false;
    out->b.boss_segments = out->b.boss_segment_index = 0;
    if (variant >= 0) { out->b.shiny = true; out->b.variant = (uint8_t)variant; }
    /* the id is what held items and the screens find a Pokemon by: two beetles made from the same stream (a rental, then a shiny one) must not share
     * one, nor may a beetle share it with anyone in the party */
    for (bool again = true; again;) {
        again = false;
        for (int i = 0; i < run->n_party; i++)
            if (run->party[i].b.id == out->b.id) { out->b.id = out->b.id * 2654435761u + 1u; again = true; break; }
    }
    out->pokeball = (uint8_t)gc_stub_rand(run, 4);
    out->met_wave = (int16_t)run->wave;
    out->met_level = out->b.level;
    out->met_biome = (int8_t)run->biome;
    pr_reset_summon_data(&out->b);
    pr_reset_turn_data(&out->b);
    pr_calculate_stats(&run->bt, &out->b);
    out->b.hp = out->b.stats[STAT_HP];
    return true;
}

/* ---------------------------------------------------------------- the encounter */
static void open_stage(gc_run *run, int stage, int n_options)
{
    gc_screen *s = &run->screen;
    memset(s, 0, sizeof *s);
    s->kind = GC_SCREEN_MYSTERY;
    s->mystery_type = GC_ME_JOBIN;
    s->mystery_tier = GC_ME_GREAT;
    s->me_stage = (uint8_t)stage;
    s->n_options = n_options;
    for (int i = 0; i < n_options; i++) s->option_ok[i] = true;
    for (int i = 0; i < 3; i++) s->mc_trainer[i] = -1;
}

static void push_stage(gc_run *run, int stage, int b) { gc_rq_push(run, gc_rp(RP_JOBIN, stage, b, 0)); }

void gc_jobin_setup_screen(gc_run *run)
{
    gc_screen *s = &run->screen;
    s->option_pick[GC_JOBIN_OWN] = GC_PICK_MEMBER;
    s->option_ok[GC_JOBIN_OWN] = gc_jobin_own_available(run);
    s->me_args[s->n_me_args++] = species_arg("jobinBeetle", gc_jobin_roll_species(run), 0);
}

void gc_jobin_execute(gc_run *run, int option, int member)
{
    gc_internal *in = gc_int(run);
    memset(&in->jobin, 0, sizeof in->jobin);
    in->jobin.own = -1;
    in->jobin.tele = -1;
    if (option == GC_JOBIN_WALK_AWAY) { gc_me_leave(run); return; }
    gc_jobin_state *s = &in->jobin;
    s->active = true;
    s->grip_you = s->grip_jobin = 100;
    s->jobin_species = (uint16_t)gc_jobin_roll_species(run);
    s->r = gc_me_option_rng(run);
    s->gen = gc_me_option_rng(run);
    pr_rng_seed_int(&s->gen, 1000, 0);   /* a stream of its own apart from the duel's rolls */
    if (option == GC_JOBIN_OWN && member >= 0 && member < run->n_party) {
        const gc_mon *m = &run->party[member];
        s->own = (int8_t)member;
        s->you_species = m->b.species;
        s->you_form = m->b.form;
        s->you_shiny = m->b.shiny;
        s->you_variant = m->b.variant;
    } else {
        /* a rental: any pool species but Jobin's */
        s->rental = true;
        int sp;
        do sp = POOL[pr_rng_seed_int(&s->r, GC_JOBIN_POOL, 0)]; while (sp == s->jobin_species);
        s->you_species = (uint16_t)sp;
    }
    hud(run, GC_JOBIN_HUD_START, 0);   /* the drum and the beetles are on the screen before the lines that name them */
    say(run, JB "start.jobin", 1, species_arg("jobinBeetle", s->jobin_species, 0));
    if (s->rental) say(run, JB "start.rental", 1, species_arg("yourBeetle", s->you_species, 0));
    else say(run, JB "start.own", 1, pr_arg_pokemon_name(&run->bt, "yourBeetle", &run->party[s->own].b));
    push_stage(run, GC_JOBIN_STAGE_ROUND, 0);
}

/* ---------------------------------------------------------------- the round */
static void announce_next(gc_run *run, bool was_special)
{
    gc_jobin_state *s = &gc_int(run)->jobin;
    s->tele = -1;
    s->bluff = false;
    if (was_special) return;   /* never two rounds in a row (a bluffed telegraph counts as one) */
    if ((int)pr_rng_seed_int(&s->r, 100, 0) >= KING_PERCENT) return;
    s->tele = (int8_t)pr_rng_seed_int(&s->r, 3, 0);
    s->bluff = (int)pr_rng_seed_int(&s->r, 100, 0) < BLUFF_PERCENT;
    hud(run, GC_JOBIN_HUD_TELEGRAPH, s->tele);   /* the Speed King steps in, then the line says what he is doing */
    say(run, TELEGRAPH_LINE[s->tele], 0);
}

static void jobin_win(gc_run *run);
static void jobin_lose(gc_run *run);
static void jobin_caught(gc_run *run);

static void round_choose(gc_run *run, int player)
{
    gc_internal *in = gc_int(run);
    gc_jobin_state *s = &in->jobin;
    const int tele = s->tele;
    const bool bluff = s->bluff && tele >= 0;
    const bool king_round = tele >= 0 && !bluff;
    int jobin = -1;
    if (!king_round) jobin = bluff ? BLUFF_MOVE[tele] : (int)pr_rng_seed_int(&s->r, 3, 0);
    s->round++;
    gc_jobin_round(s, player, jobin, king_round ? tele : -1);
    const pr_msg_arg you = species_arg("yourBeetle", s->you_species, s->you_form), him = species_arg("jobinBeetle", s->jobin_species, 0);
    if (bluff) say(run, JB "bluff.reveal", 1, him);
    if (king_round) say(run, KING_LINE[tele][player], 2, you, him);
    else if (player == M_STAND) say(run, JB "round.standTrick", 2, you, him);
    else say(run, NORMAL_LINE[player][jobin], 2, you, him);
    hud(run, GC_JOBIN_HUD_UPDATE, 0);
    say(run, JB "round.result", 3, delta_arg("youDelta", s->d_you), delta_arg("himDelta", s->d_him), delta_arg("suspDelta", s->d_susp));
    switch (gc_jobin_outcome(s)) {
    case 4: jobin_caught(run); return;
    case 1: say(run, JB "end.youFlipped", 1, you); jobin_lose(run); return;
    case 2: say(run, JB "end.jobinFlipped", 1, him); jobin_win(run); return;
    case 3:
        say(run, JB "end.rematch", 0);
        s->grip_you = s->grip_jobin = REMATCH_GRIP;
        s->streak = 0;
        hud(run, GC_JOBIN_HUD_UPDATE, 0);
        break;
    default: break;
    }
    announce_next(run, tele >= 0);   /* the round just played was special when it had a telegraph: the next one cannot be */
    push_stage(run, GC_JOBIN_STAGE_ROUND, 0);
}

/* ---------------------------------------------------------------- the ends */
static void give_held(gc_run *run, gc_mon *m, const char *key, int arg, int n, bool lock)
{
    int held = 0;
    for (int k = 0; k < run->n_items; k++) {
        const gc_item *it = &run->items[k];
        if (!strcmp(it->key, key) && it->arg == arg && it->pokemon_id == m->b.id) held += it->stack;
    }
    const int room = gc_item_max_held(run, key, arg, m) - held;
    if (room <= 0) return;
    if (n > room) n = room;
    gc_item_add(run, key, (int16_t)arg, m->b.id, n);
    if (!lock) return;
    for (int k = 0; k < run->n_items; k++) {
        gc_item *it = &run->items[k];
        if (!strcmp(it->key, key) && it->arg == arg && it->pokemon_id == m->b.id) it->non_transferable = true;
    }
}

static void jobin_win(gc_run *run)
{
    gc_internal *in = gc_int(run);
    gc_jobin_state *s = &in->jobin;
    gc_achv_jobin_won(run->sys);
    say(run, JB "win.line", 0);
    if (s->rental) {
        push_stage(run, GC_JOBIN_STAGE_GIVE, GC_JOBIN_GIVE_RELIC);
        push_stage(run, GC_JOBIN_STAGE_CHARCOAL, 0);
        push_stage(run, GC_JOBIN_STAGE_KEEP, 0);
    } else {
        push_stage(run, GC_JOBIN_STAGE_GIVE, GC_JOBIN_GIVE_OWN);
    }
    if (!s->used_stand) {   /* a clean win: not one Stand Trick */
        push_stage(run, GC_JOBIN_STAGE_GIVE, GC_JOBIN_GIVE_BONUS);
        push_stage(run, GC_JOBIN_STAGE_KEEP, 1);
    }
    push_stage(run, GC_JOBIN_STAGE_GIVE, GC_JOBIN_GIVE_END);
}

static void jobin_lose(gc_run *run)
{
    gc_internal *in = gc_int(run);
    gc_jobin_state *s = &in->jobin;
    if (s->rental) {
        const double lost = run->money;
        say(run, JB "lose.rental", 0);
        if (lost > 0) { run->money = 0; gc_ev_money(run, -lost); }
    } else {
        gc_mon *m = &run->party[s->own];
        const double lost = floor(run->money / 2);
        say(run, JB "lose.own", 1, pr_arg_pokemon_name(&run->bt, "yourBeetle", &m->b));
        if (lost > 0) { run->money -= lost; gc_ev_money(run, -lost); }
        m->b.hp = 0;   /* it faints (already fainted: nothing more) */
    }
    hud(run, GC_JOBIN_HUD_END, 1);   /* the drum goes and Jobin is back, victorious */
    say(run, JB "outro.lose", 0);
    in->jobin.active = false;
    gc_me_leave(run);
}

/* ---------------------------------------------------------------- caught: the void bet and the punishment fight */
/* Heatmor / Magmortar / Centiskorch / Scizor: Jobin's team (his list 2026-11-23). The moves are the author's choice (he asked for them to be generated);
 * every member holds a Charcoal, with its default ability (slot 0) and its passive unlocked. */
static const gc_custom_team_member PUNISH[4] = {
    {SPECIES_ID_HEATMOR, 0, 0, false, 0, {MOVE_ID_FLARE_BLITZ, MOVE_ID_SUCKER_PUNCH, MOVE_ID_BODY_SLAM, MOVE_ID_BULK_UP}, 1, {{"ATTACK_TYPE_BOOSTER", POKEMON_TYPE_FIRE, 1}}},
    {SPECIES_ID_MAGMORTAR, 0, 0, false, 0, {MOVE_ID_FLAMETHROWER, MOVE_ID_THUNDERBOLT, MOVE_ID_FOCUS_BLAST, MOVE_ID_WILL_O_WISP}, 1, {{"ATTACK_TYPE_BOOSTER", POKEMON_TYPE_FIRE, 1}}},
    {SPECIES_ID_CENTISKORCH, 0, 0, false, 0, {MOVE_ID_FIRE_LASH, MOVE_ID_LUNGE, MOVE_ID_COIL, MOVE_ID_FLARE_BLITZ}, 1, {{"ATTACK_TYPE_BOOSTER", POKEMON_TYPE_FIRE, 1}}},
    {SPECIES_ID_SCIZOR, 0, 0, false, 0, {MOVE_ID_SWORDS_DANCE, MOVE_ID_BULLET_PUNCH, MOVE_ID_X_SCISSOR, MOVE_ID_SUPERPOWER}, 1, {{"ATTACK_TYPE_BOOSTER", POKEMON_TYPE_FIRE, 1}}},
};

bool gc_jobin_punish_battle(const gc_run *run) { return gc_int((gc_run *)run)->jobin.punish && run->battle_type == BATTLE_TYPE_TRAINER; }

bool gc_jobin_team(gc_custom_team *team)
{
    memset(team, 0, sizeof *team);
    team->name = "Jobin";
    team->passive = -1;
    for (int i = 0; i < 4; i++) team->member[team->n++] = PUNISH[i];
    return gc_custom_team_check(team, NULL);
}

/* the ordinary trainer party of the wave, replaced by his four (levels: the ordinary party's). False: nobody can be built. */
static bool build_punishment(gc_run *run)
{
    gc_internal *in = gc_int(run);
    static gc_custom_team team;
    if (!gc_jobin_team(&team)) return false;
    const int t = TRAINER_TYPE_ACE_TRAINER;   /* the class: AI and money; the team and the name are Jobin's */
    const pr_rng saved = run->run.rng;
    run->run.rng = pr_run_seed_offset_rng(&run->run, run->wave * 7 + 0x4A, NULL);
    run->trainer_type = t;
    run->battle_type = BATTLE_TYPE_TRAINER;
    run->double_battle = false;
    bool ok = false;
    const pr_party_template *tpl = gc_party_template_of(run, t, 0, run->wave);
    if (tpl) {
        uint8_t strengths[8];
        for (int i = 0; i < tpl->size && i < 8; i++) strengths[i] = (uint8_t)tpl->strengths[i];
        int levels[PR_MAX_PARTY_TEMPLATE];
        const int n = pr_run_get_party_levels(&run->run, run->wave, tpl->size, strengths, tpl->strength, false, levels);
        if (n >= 1) {
            run->trainer_variant = TRAINER_VARIANT_DEFAULT;
            run->trainer_name = run->trainer_partner_name = -1;
            in->custom_trainer_name[0] = 0;
            in->custom_team_on = false;
            ok = gc_gen_trainer_party(run, tpl, t, run->trainer_variant, levels, n) && gc_custom_build_trainer(run, &team);
            /* every member has its own passive unlocked (the team's passive field is for a borrowed one) */
            for (int i = 0; ok && i < run->n_enemy; i++) {
                run->enemy[i].b.passive_flag = true;
                pr_update_passive(&run->bt, &run->enemy[i].b);
            }
        }
    }
    run->run.rng = saved;
    return ok;
}

static void jobin_caught(gc_run *run)
{
    gc_internal *in = gc_int(run);
    in->jobin.caught = true;
    hud(run, GC_JOBIN_HUD_END, 0);
    say(run, JB "caught.void", 0);
    say(run, JB "caught.line", 0);   /* "You're sweating quite a lot, are you sure you don't need a tissue" */
    if (!build_punishment(run)) {
        run->battle_type = BATTLE_TYPE_MYSTERY_ENCOUNTER;
        run->n_enemy = 0;
        run->trainer_type = run->trainer_variant = -1;
        gc_stub(run, GC_STUB_UNPORTED_ENEMY);
        in->jobin.active = false;
        gc_me_leave(run);
        return;
    }
    gc_enemy_finish_party(run);
    run->me_battle = true;
    run->me_catch_allowed = false;
    in->me_reward.active = false;
    in->me_no_stats = false;
    in->me_post.pending = false;
    run->mystery_type = run->mystery_tier = -1;
    run->in_battle = true;
    in->jobin.punish = true;
    in->jobin.active = false;
    gc_me_say_trainer_fight(run, in);   /* no healing beforehand: nothing here heals */
}

/* the fight is won: the battle's end, his money (half: gc_jobin_money_multiplier) and the normal reward screen */
void gc_jobin_victory(gc_run *run)
{
    gc_internal *in = gc_int(run);
    in->jobin.punish = false;
    in->jobin.half_money = true;
    gc_achv_jobin_punish_won(run->sys);
    gc_rq_push(run, gc_rp(RP_BATTLE_END, 1, 0, 0));
    gc_rq_push(run, gc_rp(RP_TRAINER_VICTORY, 0, 0, 0));
    pr_reward_custom *c = &in->reward.me_custom;
    memset(c, 0, sizeof *c);
    c->fill_remaining = true;
    c->reroll_multiplier = NAN;
    c->allow_luck_upgrades = true;
    gc_me_end_with_shop(run);
}

double gc_jobin_take_money_multiplier(gc_run *run)
{
    gc_jobin_state *s = &gc_int(run)->jobin;
    if (!s->half_money) return 1.0;
    s->half_money = false;
    return 0.5;   /* "50% reduced prize money" */
}

/* ---------------------------------------------------------------- the stages */
bool gc_jobin_stage_start(gc_run *run, const gc_rphase *p)
{
    gc_internal *in = gc_int(run);
    gc_jobin_state *j = &in->jobin;
    gc_screen *s = &run->screen;
    switch (p->a) {
    case GC_JOBIN_STAGE_ROUND:
        open_stage(run, GC_ME_STAGE_JOBIN_ROUND, 4);
        s->me_args[s->n_me_args++] = species_arg("yourBeetle", j->you_species, j->you_form);
        s->me_args[s->n_me_args++] = species_arg("jobinBeetle", j->jobin_species, 0);
        s->me_args[s->n_me_args++] = pr_arg_int("gripYou", j->grip_you);
        s->me_args[s->n_me_args++] = pr_arg_int("gripJobin", j->grip_jobin);
        s->me_args[s->n_me_args++] = pr_arg_int("suspicion", j->susp);
        s->me_args[s->n_me_args++] = pr_arg_int("telegraph", j->tele);   /* the Speed King attack announced for this round, -1 none */
        return false;
    case GC_JOBIN_STAGE_KEEP: {
        gc_mon m;
        const int species = p->b ? j->bonus_species : j->you_species;
        if (!make_beetle(run, species, p->b ? j->bonus_variant : -1, &m)) return true;
        open_stage(run, GC_ME_STAGE_JOBIN_KEEP, 2);
        const bool full = run->n_party >= 6;
        if (full) s->option_pick[0] = GC_PICK_MEMBER;
        s->me_args[s->n_me_args++] = pr_arg_pokemon_name(&run->bt, "pokemonName", &m.b);
        s->me_args[s->n_me_args++] = pr_arg_int("partyFull", full);
        s->me_args[s->n_me_args++] = pr_arg_int("bonus", p->b);
        s->me_args[s->n_me_args++] = pr_arg_int("variant", p->b ? j->bonus_variant : 0);
        return false;
    }
    case GC_JOBIN_STAGE_CHARCOAL:
        open_stage(run, GC_ME_STAGE_JOBIN_CHARCOAL, 1);
        s->option_pick[0] = GC_PICK_MEMBER;
        return false;
    case GC_JOBIN_STAGE_GIVE:
        switch (p->b) {
        case GC_JOBIN_GIVE_OWN: {
            gc_mon *m = &run->party[j->own];
            give_held(run, m, "GRIP_CLAW", -1, 1, true);
            give_held(run, m, "KINGS_ROCK", -1, 1, true);
            give_held(run, m, "ATTACK_TYPE_BOOSTER", POKEMON_TYPE_BUG, 2, true);   /* two Silver Powder */
            gc_me_update_modifiers(run);
            say(run, JB "give.own", 1, pr_arg_pokemon_name(&run->bt, "pokemonName", &m->b));
            break;
        }
        case GC_JOBIN_GIVE_RELIC: {
            gc_rphase g = gc_rp(RP_MODIFIER_REWARD, 0, 0, 0);
            g.key = "RELIC_GOLD";
            gc_rq_unshift(run, g);
            break;
        }
        case GC_JOBIN_GIVE_BONUS: {
            /* a random shiny beetle from his collection, at the wave's level: tier 1 yellow 60%, tier 2 blue 30%, tier 3 red 10% */
            j->bonus_species = POOL[pr_rng_seed_int(&j->gen, GC_JOBIN_POOL, 0)];
            const int roll = (int)pr_rng_seed_int(&j->gen, 100, 0);
            j->bonus_variant = (uint8_t)(roll < 60 ? 0 : roll < 90 ? 1 : 2);
            say(run, JB "bonus.line", 1, species_arg("pokemonName", j->bonus_species, 0));
            break;
        }
        default:   /* GC_JOBIN_GIVE_END */
            hud(run, GC_JOBIN_HUD_END, 2);   /* the drum goes and Jobin is back, defeated */
            say(run, JB "outro.win", 0);
            j->active = false;
            gc_me_leave(run);
            break;
        }
        return true;
    default:
        return true;
    }
}

/* a beetle joins the party (in place of `replace` with six, whose held items are dropped), as a catch does */
static void join_party(gc_run *run, gc_mon *m, int replace, bool powder)
{
    gc_mon *slot;
    if (run->n_party < 6) {
        slot = &run->party[run->n_party];
        *slot = *m;
        run->party_order[run->n_party] = (int8_t)run->n_party;
        run->n_party++;
    } else if (replace >= 0 && replace < run->n_party) {
        gc_ev_msg(run, JB "keep.farewell", 1, gc_m_mon(run, "pokemonName", 0, replace));   /* the encounter's own line: its members screen is not the party screen that says goodbye */
        gc_items_drop_held_of(run, run->party[replace].b.id);
        slot = &run->party[replace];
        *slot = *m;
    } else {
        return;
    }
    run->bt.n_party[0] = (uint8_t)run->n_party;
    for (int i = 0; i < run->n_party; i++) run->bt.party[0][i] = &run->party[run->party_order[i]].b;
    if (powder) give_held(run, slot, "ATTACK_TYPE_BOOSTER", POKEMON_TYPE_BUG, 1, false);   /* one Silver Powder, not locked */
    gc_me_update_modifiers(run);
}

bool gc_jobin_stage_choose(gc_run *run, const gc_rphase *p, const gc_choice *c, const char **why, bool *ok)
{
    gc_internal *in = gc_int(run);
    gc_jobin_state *j = &in->jobin;
    if (c->kind != GC_CH_MYSTERY) { *why = "choose a mystery encounter action"; return false; }
    if (c->a < 0 || c->a >= run->screen.n_options || !run->screen.option_ok[c->a]) { *why = "option not allowed"; return false; }
    const bool picks = (p->a == GC_JOBIN_STAGE_KEEP && c->a == 0 && run->n_party >= 6) || p->a == GC_JOBIN_STAGE_CHARCOAL;
    if (picks && !gc_jobin_pick_ok(run, c->a, c->b)) { *why = "no such Pokemon"; return false; }
    *ok = true;
    const int b = c->b;
    memset(&run->screen, 0, sizeof run->screen);
    switch (p->a) {
    case GC_JOBIN_STAGE_ROUND:
        round_choose(run, c->a);
        return true;
    case GC_JOBIN_STAGE_CHARCOAL: {
        gc_mon *m = &run->party[b];
        give_held(run, m, "ATTACK_TYPE_BOOSTER", POKEMON_TYPE_FIRE, 1, false);   /* a Charcoal, free to move */
        gc_me_update_modifiers(run);
        say(run, JB "give.charcoal", 1, pr_arg_pokemon_name(&run->bt, "pokemonName", &m->b));
        return true;
    }
    case GC_JOBIN_STAGE_KEEP: {
        if (c->a != 0) { say(run, JB "keep.no", 0); return true; }
        gc_mon m;
        if (!make_beetle(run, p->b ? j->bonus_species : j->you_species, p->b ? j->bonus_variant : -1, &m)) return true;
        join_party(run, &m, b, !p->b);   /* the rental holds one Silver Powder; the shiny one nothing */
        say(run, JB "keep.yes", 1, pr_arg_pokemon_name(&run->bt, "pokemonName", &m.b));
        return true;
    }
    default:
        *why = "nothing to answer";
        *ok = false;
        return false;
    }
}

bool gc_jobin_view_get(const gc_run *run, gc_jobin_view *v)
{
    const gc_jobin_state *s = &gc_int((gc_run *)run)->jobin;
    if (!s->active && !s->punish) return false;
    v->grip_you = s->grip_you;
    v->grip_jobin = s->grip_jobin;
    v->suspicion = s->susp;
    v->telegraph = s->tele;
    v->you_species = s->you_species;
    v->you_form = s->you_form;
    v->you_shiny = s->you_shiny;
    v->you_variant = s->you_variant;
    v->jobin_species = s->jobin_species;
    v->rental = s->rental;
    v->you_index = s->rental ? -1 : s->own;
    return s->active;
}
