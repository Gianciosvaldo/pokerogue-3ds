/* gcplay: game-core on a PC terminal.
 *
 *   gcplay [--seed S] [--starters BULBASAUR,CHARMANDER,...]     play a run by typing the choices
 *   gcplay --auto N [--first K] [--jobs J] [--verbose]           N seeded soak runs with random choices
 *   --save-test     soak runs: at each save point, save, wipe the run and load it back (the digest must not change)
 *   --ask-switch    the SWITCH battle style: a wild battle's start asks "Will you switch Pokemon?" (run->ask_switch)
 *   --locales DIR   upstream's locales/en folder: messages as English text instead of keys
 *
 * The soak runs pick random legal answers on every screen (their own RNG, seeded per run), each in its
 * own process so a crash or a hang is caught and reported with its seed, and sum up where the runs
 * ended, the stand-ins they used and battle-core's unported effects they hit. */
#define _POSIX_C_SOURCE 200809L
#include "gc/run.h"
#include "pr/ai.h"
#include "pr/reward.h"
#include <errno.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

static const char *species_name(int id)
{
    for (int i = 0; i < PR_SPECIES_COUNT; i++) if (pr_species_table[i].id == id) return pr_species_table[i].name;
    return "?";
}
static const char *move_name(int id) { return id > 0 && id < PR_MOVE_COUNT ? pr_moves[id].name : "-"; }

/* ---------------------------------------------------------------- printing */
/* upstream's English locale files (locales/en/<file>.json) when --locales names the folder */
static const char *locales;
static const char *locale_get(void *ctx, const char *file, int fallback, size_t *len)
{
    (void)ctx;
    static struct { char name[64]; char *text; size_t len; } cache[96];
    static int n_cache;
    if (fallback || !locales) return NULL;
    for (int i = 0; i < n_cache; i++)
        if (!strcmp(cache[i].name, file)) { *len = cache[i].len; return cache[i].text; }
    if (n_cache >= 96) return NULL;
    char path[512];
    snprintf(path, sizeof path, "%s/%s.json", locales, file);
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
    }
    snprintf(cache[n_cache].name, sizeof cache[n_cache].name, "%s", file);
    cache[n_cache].text = text;
    cache[n_cache].len = n;
    n_cache++;
    *len = n;
    return text;
}

static void print_msg(const pr_msg *m)
{
    if (!m->key) { printf("  [text not ported: %s]\n", m->args[0].s ? m->args[0].s : "?"); return; }
    if (locales) {
        static const pr_text_source src = {locale_get, NULL, "en"};
        char text[1024];
        pr_msg_format(m, &src, text, sizeof text);
        for (char *c = text; *c; c++) if (*c == '\n') *c = ' ';
        printf("  %s\n", text);
        return;
    }
    printf("  [%s]", m->key);
    for (int i = 0; i < m->n_args; i++) {
        const pr_msg_arg *a = &m->args[i];
        printf(" %s=", a->name);
        switch (a->kind) {
        case PR_ARG_POKEMON: case PR_ARG_POKEMON_NAME:
            printf("%s%s", a->kind == PR_ARG_POKEMON && (a->v & PR_MSG_POKEMON_ENEMY) ? (a->v & PR_MSG_POKEMON_FOE ? "Foe " : "Wild ") : "",
                   species_name((int)(a->v >> 16)));
            break;
        case PR_ARG_MOVE: printf("%s", move_name((int)(a->v & 0xffff))); break;
        case PR_ARG_KEY: case PR_ARG_STR: printf("%s", a->s ? a->s : ""); break;
        default: printf("%d", (int)a->v); break;
        }
    }
    printf("\n");
}

static void print_battle_event(const pr_event *e)
{
    switch (e->type) {
    case PR_EV_TEXT: print_msg(&e->msg); break;
    case PR_EV_DAMAGE: case PR_EV_HP: {
        const pr_event_field *f = &e->field[e->battler];
        printf("    %s %s HP %d/%d\n", e->battler < 2 ? "you" : "foe", species_name(f->species), f->hp, f->max_hp);
        break;
    }
    default: break;
    }
}

static void print_event(const gc_run *run, const gc_event *e)
{
    switch (e->kind) {
    case GC_EV_MESSAGE: print_msg(&e->msg); break;
    case GC_EV_BATTLE:
        for (int i = 0; i < e->b; i++) print_battle_event(&run->battle_events[e->a + i]);
        break;
    case GC_EV_WAVE: printf("== wave %d  biome %d  %s\n", e->a, e->b, e->c == 1 ? "trainer" : "wild"); break;
    case GC_EV_BIOME: printf("== new biome %d\n", e->a); break;
    case GC_EV_HP: {
        const gc_mon *m = e->a ? &run->enemy[e->b] : &run->party[e->b];
        printf("  %s %s HP %d/%d\n", e->a ? "foe" : "you", species_name(m->b.species), e->c, m->b.stats[0]);
        break;
    }
    case GC_EV_EXP: printf("  exp +%.0f (party %d)\n", e->exp_amount, e->a); break;
    case GC_EV_MONEY: printf("  money %+.17g -> %.17g\n", e->money_change, e->money_after); break;
    case GC_EV_TURN: printf("-- turn %d\n", e->a); break;
    default: break;
    }
}

static void print_mon(const gc_mon *m)
{
    const pr_pokemon *b = &m->b;
    printf("%s L%d HP %d/%d", species_name(b->species), b->level, b->hp, b->stats[0]);
    if (b->status) printf(" st%d", b->status);
    printf(" [");
    for (int i = 0; i < b->n_moves; i++) printf("%s%s %d/%d", i ? ", " : "", move_name(b->moves[i]), b->pp_used[i], pr_moves[b->moves[i]].pp);
    printf("]");
}

static const gc_mon *command_actor(const gc_run *run){
 const pr_pokemon *p=run->bt.field[run->screen.slot==1?1:0];
 for(int i=0;i<run->n_party;i++)if(&run->party[i].b==p)return &run->party[i];return NULL;
}
static const gc_mon *command_foe(const gc_run *run){
 for(int k=2;k<4;k++){const pr_pokemon *p=run->bt.field[k];if(p&&p->hp>0&&p->on_field)
  for(int i=0;i<run->n_enemy;i++)if(&run->enemy[i].b==p)return &run->enemy[i];}return NULL;
}
static void print_screen(const gc_run *run)
{
    const gc_screen *s = gc_run_screen(run);
    switch (s->kind) {
    case GC_SCREEN_TARGET:
        printf("TARGET (BattlerIndex, c cancels):");
        for(int i=0;i<4;i++)if(s->target_ok[i])printf(" %d",i);printf("\n");break;
    case GC_SCREEN_COMMAND: {
        const gc_mon *me = command_actor(run), *foe = command_foe(run);
        printf("\nwave %d  money %.0f\n  foe: ", run->wave, run->money);
        if (foe) print_mon(foe);
        printf("\n  you: ");
        if (me) print_mon(me);
        printf("\nCOMMAND: f <slot 0-3> | p <party> (switch)%s\n", s->struggle ? "  (no usable move: f 0 = Struggle)" : "");
        for (int i = 0; i < 4; i++) if (me && i < me->b.n_moves && !s->move_ok[i]) printf("  slot %d: %s\n", i, s->move_why[i]);
        break;
    }
    case GC_SCREEN_SWITCH:
        printf("SWITCH (send in a party member%s):\n", s->can_skip ? ", -1 for none" : "");
        for (int i = 0; i < run->n_party; i++) if (s->pick_ok[i]) { printf("  %d: ", i); print_mon(&run->party[i]); printf("\n"); }
        break;
    case GC_SCREEN_LEARN_MOVE:
        printf("LEARN MOVE: %s wants %s. Forget slot 0-3, or -1: ", species_name(run->party[s->party].b.species), move_name(s->move));
        print_mon(&run->party[s->party]);
        printf("\n");
        break;
    case GC_SCREEN_EVOLVE:
        printf("EVOLVE: %s into %s? y / n / N (n and pause)\n", species_name(run->party[s->party].b.species), species_name(s->species));
        break;
    case GC_SCREEN_REWARD:
        printf("REWARD (money %.0f): o <i> [party] [move] | s <i> [party] [move] | r (reroll %.0f) | x\n", run->money, s->reroll_cost);
        for (int i = 0; i < s->n_options; i++) printf("  o%d %s\n", i, s->options[i].key);
        for (int i = 0; i < s->n_shop; i++) printf("  s%d %s %.0f\n", i, s->shop[i].key, s->shop[i].cost);
        for (int i = 0; i < run->n_party; i++) { printf("  party %d: ", i); print_mon(&run->party[i]); printf("\n"); }
        break;
    case GC_SCREEN_BIOME:
        printf("BIOME:");
        for (int i = 0; i < s->n_biomes; i++) printf(" %d=%d", i, s->biomes[i]);
        printf("\n");
        break;
    case GC_SCREEN_MYSTERY:
        printf("MYSTERY (type %d tier %d): options %d\n", s->mystery_type, s->mystery_tier, s->n_options);
        for (int i = 0; i < s->n_options; i++) printf("  option %d: %s\n", i, s->option_ok[i] ? "ok" : "disabled");
        break;
    case GC_SCREEN_CHECK_SWITCH: printf("WILL YOU SWITCH POKEMON? y / n\n"); break;
    case GC_SCREEN_GAME_OVER: printf("GAME OVER at wave %d, score %d\n", run->wave, run->score); break;
    case GC_SCREEN_CLEAR: printf("CLEAR! score %d\n", run->score); break;
    default: printf("(screen %d)\n", s->kind); break;
    }
}

static void drain(gc_run *run, bool show)
{
    gc_event e;
    while (gc_run_next_event(run, &e)) if (show) print_event(run, &e);
}

/* ---------------------------------------------------------------- the random chooser */
static uint64_t rs;
static uint32_t rnd(uint32_t n)
{
    rs ^= rs << 13; rs ^= rs >> 7; rs ^= rs << 17;
    return n ? (uint32_t)(rs % n) : 0;
}

static int alive_member(const gc_run *run)
{
    int c[6], n = 0;
    for (int i = 0; i < run->n_party; i++) if (run->party[i].b.hp > 0) c[n++] = i;
    return n ? c[rnd((uint32_t)n)] : 0;
}

/* ---- a plain player: the strongest move, heal the hurt, revive the fainted. 15% of its answers are
 * random so the runs stay varied ---- */
static const pr_form *form_of(const pr_pokemon *p)
{
    for (int i = 0; i < PR_SPECIES_COUNT; i++)
        if (pr_species_table[i].id == p->species) {
            const pr_species *sp = &pr_species_table[i];
            return &pr_forms[sp->form0 + (p->form < sp->nforms ? p->form : 0)];
        }
    return &pr_forms[0];
}

static double move_score(const pr_pokemon *user, const pr_pokemon *foe, int move)
{
    const pr_move *m = &pr_moves[move];
    if (m->power <= 0) return 15;
    const pr_form *uf = form_of(user), *ff = form_of(foe);
    double s = m->power * (m->type == uf->type1 || m->type == uf->type2 ? 1.5 : 1);
    if (m->type >= 0 && m->type < PR_TYPE_COUNT) {
        if (ff->type1 >= 0) s *= pr_type_chart_x2[m->type][ff->type1] / 2.0;
        if (ff->type2 >= 0) s *= pr_type_chart_x2[m->type][ff->type2] / 2.0;
    }
    return s;
}

static int most_hurt(const gc_run *run, bool fainted)
{
    int best = -1;
    double worst = 2;
    for (int i = 0; i < run->n_party; i++) {
        const pr_pokemon *b = &run->party[i].b;
        if ((b->hp <= 0) != fainted) continue;
        double f = b->stats[0] ? (double)b->hp / b->stats[0] : 1;
        if (f < worst) { worst = f; best = i; }
    }
    return fainted || worst < 0.6 ? best : -1;
}

static bool plain_choice(const gc_run *run, gc_choice *c)
{
    const gc_screen *s = gc_run_screen(run);
    memset(c, 0, sizeof *c);
    switch (s->kind) {
    case GC_SCREEN_TARGET:
        c->kind=GC_CH_TARGET;
        for(int i=2;i<6;i++)if(s->target_ok[i%4]){c->a=i%4;return true;}return false;
    case GC_SCREEN_COMMAND: {
        const gc_mon *me = command_actor(run), *foe = command_foe(run);
        if (!me || !foe) return false;
        if (s->can_ball) {
            for (int i = 7; i >= 0; i--) {
                if (run->pokeballs[i] > 0 && (foe->b.hp * 2 < foe->b.stats[0] || rnd(3) == 0)) {
                    c->kind = GC_CH_BALL; c->a = (int16_t)i; return true;
                }
            }
        }
        if (s->can_switch && me->b.hp * 4 < me->b.stats[0] && rnd(3) == 0) {
            for (int i = 0; i < run->n_party; i++)
                if (!run->party[i].b.on_field && run->party[i].b.hp * 2 > run->party[i].b.stats[0]) {
                    c->kind = GC_CH_POKEMON; c->a = (int16_t)i; return true;
                }
        }
        double best = -1;
        c->kind = GC_CH_FIGHT; c->a = 0; c->b = -1;
        for (int i = 0; i < 4; i++)
            if (s->move_ok[i]) {
                double sc = move_score(&me->b, &foe->b, me->b.moves[i]);
                if (sc > best) { best = sc; c->a = (int16_t)i; }
            }
        return true;
    }
    case GC_SCREEN_SWITCH: {
        int best = -1, hp = -1;
        for (int i = 0; i < run->n_party; i++) if (s->pick_ok[i] && run->party[i].b.hp > hp) { hp = run->party[i].b.hp; best = i; }
        c->kind = GC_CH_PICK; c->a = (int16_t)best;
        return best >= 0;
    }
    case GC_SCREEN_LEARN_MOVE: {
        const gc_mon *m = &run->party[s->party];
        int worst = -1, wp = pr_moves[s->move].power;
        for (int i = 0; i < m->b.n_moves; i++) if (pr_moves[m->b.moves[i]].power < wp) { wp = pr_moves[m->b.moves[i]].power; worst = i; }
        c->kind = GC_CH_FORGET; c->a = (int16_t)worst;
        return true;
    }
    case GC_SCREEN_EVOLVE: c->kind = GC_CH_EVOLVE; return true;
    case GC_SCREEN_CHECK_SWITCH: c->kind = GC_CH_CONFIRM; c->a = 0; return true;
    case GC_SCREEN_MYSTERY: {
        int best = -1;
        for (int i = 0; i < s->n_options; i++) if (s->option_ok[i]) { best = i; break; }
        if (best < 0) return false;
        c->kind = GC_CH_MYSTERY;
        c->a = (int16_t)best;
        c->b = (int16_t)alive_member(run);
        c->c = 0;
        return true;
    }
    case GC_SCREEN_REWARD: {
        const int fainted = most_hurt(run, true), hurt = most_hurt(run, false);
        /* buy a potion for a badly hurt member once */
        if (hurt >= 0 && run->party[hurt].b.hp * 3 < run->party[hurt].b.stats[0])
            for (int i = 0; i < s->n_shop; i++)
                if (strstr(s->shop[i].key, "POTION") && s->shop[i].cost <= run->money && rnd(2)) {
                    c->kind = GC_CH_SHOP; c->a = (int16_t)i; c->b = (int16_t)hurt; return true;
                }
        int pick = -1, target = -1;
        for (int i = 0; i < s->n_options && pick < 0; i++) {
            const char *k = s->options[i].key;
            if (fainted >= 0 && strstr(k, "REVIVE")) { pick = i; target = fainted; }
        }
        for (int i = 0; i < s->n_options && pick < 0; i++) {
            const char *k = s->options[i].key;
            if (hurt >= 0 && (strstr(k, "POTION") || strstr(k, "RESTORE"))) { pick = i; target = hurt; }
        }
        for (int i = 0; i < s->n_options && pick < 0; i++)
            if (!strcmp(s->options[i].key, "RARE_CANDY")) {
                pick = i;
                int lv = 999;
                for (int k = 0; k < run->n_party; k++) if (run->party[k].b.hp > 0 && run->party[k].b.level < lv) { lv = run->party[k].b.level; target = k; }
            }
        if (pick < 0) { pick = 0; target = alive_member(run); }
        if (!s->n_options) { c->kind = GC_CH_SKIP; return true; }
        c->kind = GC_CH_REWARD; c->a = (int16_t)pick; c->b = (int16_t)target; c->c = 0;
        return true;
    }
    default:
        return false;
    }
}

/* --auto: how often each reward class was taken or refused (a reward pick or a shop
 * purchase on the reward screen, for any reason it came back false), by key.
 * Each run counts in its own process. */
#define N_APPLY_KEYS PR_ITEM_COUNT_
static uint32_t apply_taken[N_APPLY_KEYS], apply_refused[N_APPLY_KEYS];

static bool tallied_choose(gc_run *run, const gc_choice *c, const char **why)
{
    const gc_screen *s = gc_run_screen(run);
    int k = -1;
    if (s->kind == GC_SCREEN_REWARD && (c->kind == GC_CH_REWARD || c->kind == GC_CH_SHOP)) {
        const bool reward = c->kind == GC_CH_REWARD;
        const gc_offer *o = c->a < 0 ? NULL : reward ? (c->a < s->n_options ? &s->options[c->a] : NULL)
                                                      : (c->a < s->n_shop ? &s->shop[c->a] : NULL);
        for (int i = 0; o && i < PR_ITEM_COUNT_ && k < 0; i++) if (!strcmp(o->key, pr_item_keys[i])) k = i;
    }
    const bool ok = gc_run_choose(run, c, why);
    if (k >= 0) (ok ? apply_taken : apply_refused)[k]++;
    return ok;
}

/* one legal answer (the plain player's, or a random one); false when nothing was accepted */
static bool auto_choose(gc_run *run)
{
    const gc_screen *s = gc_run_screen(run);
    gc_choice c = {0};
    const char *why;
    if (rnd(100) >= 15 && plain_choice(run, &c) && tallied_choose(run, &c, &why)) return true;
    for (int tries = 0; tries < 64; tries++) {
        memset(&c, 0, sizeof c);
        switch (s->kind) {
        case GC_SCREEN_TARGET:
            c.kind=GC_CH_TARGET;for(int i=0;i<4;i++)if(s->target_ok[i]){c.a=i;break;}break;
        case GC_SCREEN_COMMAND: {
            if (s->can_ball && rnd(100) < 15) {
                int avail[8], n_avail = 0;
                for (int i = 0; i < 8; i++) if (run->pokeballs[i] > 0) avail[n_avail++] = i;
                if (n_avail > 0) {
                    c.kind = GC_CH_BALL;
                    c.a = (int16_t)avail[rnd((uint32_t)n_avail)];
                    break;
                }
            }
            if (s->can_switch && rnd(100) < 10) {
                c.kind = GC_CH_POKEMON;
                c.a = (int16_t)rnd((uint32_t)run->n_party);
            } else {
                c.kind = GC_CH_FIGHT;
                int ok[4], n = 0;
                for (int i = 0; i < 4; i++) if (s->move_ok[i]) ok[n++] = i;
                c.a = (int16_t)(n ? ok[rnd((uint32_t)n)] : 0);
                c.b = -1;
            }
            break;
        }
        case GC_SCREEN_SWITCH: {
            int ok[6], n = 0;
            for (int i = 0; i < run->n_party; i++) if (s->pick_ok[i]) ok[n++] = i;
            c.kind = GC_CH_PICK;
            if (s->can_skip && rnd(4) == 0) {
                c.a = -1;
            } else {
                c.a = (int16_t)(n ? ok[rnd((uint32_t)n)] : -1);
            }
            break;
        }
        case GC_SCREEN_LEARN_MOVE:
            c.kind = GC_CH_FORGET;
            c.a = (int16_t)((int)rnd(5) - 1);
            break;
        case GC_SCREEN_EVOLVE:
            c.kind = rnd(10) ? GC_CH_EVOLVE : GC_CH_CANCEL;
            c.a = (int16_t)(c.kind == GC_CH_CANCEL && rnd(2));
            break;
        case GC_SCREEN_CHECK_SWITCH:
            c.kind = GC_CH_CONFIRM;
            c.a = (int16_t)(rnd(4) == 0);
            break;
        case GC_SCREEN_REWARD:
            if (tries > 40) { c.kind = GC_CH_SKIP; break; }
            if (s->n_shop && rnd(100) < 5) {
                c.kind = GC_CH_SHOP;
                c.a = (int16_t)rnd((uint32_t)s->n_shop);
            } else if (s->reroll_cost >= 0 && s->reroll_cost < run->money / 4 && rnd(100) < 5) {
                c.kind = GC_CH_REROLL;
            } else if (s->n_options) {
                c.kind = GC_CH_REWARD;
                c.a = (int16_t)rnd((uint32_t)s->n_options);
            } else {
                c.kind = GC_CH_SKIP;
            }
            c.b = (int16_t)(tries < 20 ? alive_member(run) : (int)rnd((uint32_t)run->n_party));
            c.c = (int16_t)rnd(4);
            break;
        case GC_SCREEN_BIOME:
            c.kind = GC_CH_BIOME;
            c.a = (int16_t)rnd((uint32_t)s->n_biomes);
            break;
        case GC_SCREEN_MYSTERY: {
            int ok[8], n = 0;
            for (int i = 0; i < s->n_options; i++) if (s->option_ok[i]) ok[n++] = i;
            c.kind = GC_CH_MYSTERY;
            c.a = (int16_t)(n ? ok[rnd((uint32_t)n)] : 0);
            c.b = (int16_t)alive_member(run);
            c.c = 0;
            break;
        }
        default:
            return false;
        }
        if (tallied_choose(run, &c, &why)) return true;
    }
    return false;
}

/* a random team within the budget, from every species with a starter cost */
static int random_team(gc_starter *out)
{
    int budget = gc_starter_budget(GAME_MODES_CLASSIC), n = 0;
    for (int tries = 0; tries < 200 && n < 6 && budget > 0; tries++) {
        const pr_species *sp = &pr_species_table[rnd(PR_SPECIES_COUNT)];
        const int cost = gc_starter_cost(sp->id);
        if (cost <= 0 || cost > budget) continue;
        bool dup = false;
        for (int k = 0; k < n; k++) dup |= out[k].species == sp->id;
        if (dup) continue;
        gc_starter *s = &out[n++];
        memset(s, 0, sizeof *s);
        s->species = sp->id;
        s->ability_index = (uint8_t)rnd(2);
        s->nature = (uint8_t)rnd(25);
        s->female = sp->male_permille >= 0 && (int)rnd(1000) >= sp->male_permille;
        s->tera_type = -1;
        for (int i = 0; i < 6; i++) s->ivs[i] = (uint8_t)rnd(32);
        budget -= cost;
    }
    return n;
}

/* ---------------------------------------------------------------- soak runs */
typedef struct {
    int status;                 /* 0 ended, 1 stuck, 2 crashed, 3 timed out */
    int wave, turns, score, clear;
    int waves_to_trainer;
    uint32_t stubs[16];
    uint32_t unported[GC_MAX_UNPORTED];
    uint32_t run_core[8];
    struct { char site[40]; uint32_t n; } texts[32];   /* battle texts not ported yet, by engine site */
    int n_texts;
    char stuck_screen;
    char team[64];
    uint32_t apply_taken[N_APPLY_KEYS], apply_refused[N_APPLY_KEYS];   /* tallied_choose, per apply_keys */
    int saves;                  /* --save-test: save/load round trips */
    uint32_t save_max;          /* the largest save, bytes */
} soak_result;

/* --save-test: at each save point, save, wipe the run and load it back, then play on */
static int save_test;
/* --ask-switch: run->ask_switch for every run (a setting: gc_run_new and gc_run_load don't keep it) */
static bool ask_switch;

static void soak_one(int index, int verbose, soak_result *r)
{
    static gc_run run;
    char seed[32];
    snprintf(seed, sizeof seed, "soak%08d", index);
    rs = 0x9E3779B97F4A7C15ull ^ ((uint64_t)index * 0xD1B54A32D192ED03ull);
    rnd(1);
    gc_run_new(&run, GAME_MODES_CLASSIC, seed, (uint32_t)index);
    run.ask_switch = ask_switch;
    gc_starter team[6];
    int n = random_team(team);
    const char *why;
    r->team[0] = 0;
    for (int i = 0; i < n; i++) {
        size_t l = strlen(r->team);
        snprintf(r->team + l, sizeof r->team - l, "%s%d", i ? "," : "", team[i].species);
    }
    if (!gc_run_set_starters(&run, team, n, &why)) { r->status = 1; r->stuck_screen = 'S'; return; }
    int saved_wave = -1;
    for (int step = 0; step < 200000; step++) {
        gc_event e;
        while (gc_run_next_event(&run, &e)) {
            if (e.kind == GC_EV_TURN) r->turns++;
            if (e.kind == GC_EV_BATTLE)
                for (int i = 0; i < e.b; i++) {
                    const pr_event *be = &run.battle_events[e.a + i];
                    if (be->type != PR_EV_TEXT || be->msg.key) continue;
                    const char *site = be->msg.args[0].s ? be->msg.args[0].s : "?";
                    int k = 0;
                    while (k < r->n_texts && strcmp(r->texts[k].site, site)) k++;
                    if (k == r->n_texts && r->n_texts < 32) snprintf(r->texts[r->n_texts++].site, 40, "%s", site);
                    if (k < r->n_texts) r->texts[k].n++;
                }
            if (verbose) print_event(&run, &e);
        }
        if (save_test && run.wave != saved_wave && gc_run_can_save(&run)) {
            static uint8_t buf[GC_SAVE_MAX];
            saved_wave = run.wave;
            const size_t len = gc_run_save(&run, buf, sizeof buf);
            if (!len) { r->status = 1; r->stuck_screen = 'W'; break; }
            if (len > r->save_max) r->save_max = (uint32_t)len;
            r->saves++;
            memset(&run, 0xA5, sizeof run);
            if (!gc_run_load(&run, buf, len, &why) || run.load_dropped) {
                if (verbose) printf("load failed: %s (dropped %d)\n", why, run.load_dropped);
                r->status = 1;
                r->stuck_screen = 'L';
                break;
            }
            run.ask_switch = ask_switch;
            if (verbose) printf("-- saved (%u bytes) and loaded back\n", (unsigned)len);
            continue;
        }
        const gc_screen *s = gc_run_screen(&run);
        if (s->kind == GC_SCREEN_GAME_OVER || s->kind == GC_SCREEN_CLEAR) { r->clear = s->kind == GC_SCREEN_CLEAR; break; }
        if (verbose) print_screen(&run);
        if (!auto_choose(&run)) { r->status = 1; r->stuck_screen = (char)('0' + s->kind); break; }
    }
    r->wave = run.wave;
    r->score = run.score;
    memcpy(r->stubs, run.stubs, sizeof r->stubs);
    memcpy(r->unported, run.unported, sizeof r->unported);
    memcpy(r->run_core, run.run_core_unported, sizeof r->run_core);
    memcpy(r->apply_taken, apply_taken, sizeof apply_taken);
    memcpy(r->apply_refused, apply_refused, sizeof apply_refused);
}

typedef struct { char site[40]; uint32_t runs, n; } text_site;
static int cmp_site(const void *a, const void *b)
{
    const text_site *x = a, *y = b;
    return x->runs < y->runs ? 1 : x->runs > y->runs ? -1 : strcmp(x->site, y->site);
}

static int cmp_desc(const void *a, const void *b)
{
    const uint32_t *x = a, *y = b;
    return (y[1] > x[1]) - (y[1] < x[1]);
}

static int soak(int n, int first, int jobs, int verbose, int timeout)
{
    static soak_result agg_r;
    static uint32_t hit_runs[GC_MAX_UNPORTED], hit_total[GC_MAX_UNPORTED];
    uint32_t stub_runs[16] = {0}, stub_total[16] = {0}, rc_runs[8] = {0}, rc_total[8] = {0};
    uint32_t taken_runs[N_APPLY_KEYS] = {0}, taken_total[N_APPLY_KEYS] = {0}, refused_total[N_APPLY_KEYS] = {0};
    static text_site texts[256];
    int n_texts = 0;
    int ended = 0, stuck = 0, crashed = 0, timed = 0, clears = 0;
    long turns = 0;
    int wave_hist[21] = {0};
    long saves = 0;
    uint32_t save_max = 0, digest = 0;
    int running = 0, next = 0;
    struct { pid_t pid; int fd; int index; } slot[64];
    if (jobs > 64) jobs = 64;
    if (verbose) jobs = 1;
    while (next < n || running) {
        while (running < jobs && next < n) {
            int p[2];
            if (pipe(p)) { perror("pipe"); return 2; }
            pid_t pid = fork();
            if (pid == 0) {
                close(p[0]);
                alarm((unsigned)timeout);
                soak_result r;
                memset(&r, 0, sizeof r);
                soak_one(first + next, verbose, &r);
                const char *b = (const char *)&r;
                size_t left = sizeof r;
                while (left) { ssize_t w = write(p[1], b, left); if (w <= 0) break; b += w; left -= (size_t)w; }
                fflush(stdout);   /* _exit doesn't flush: a verbose run's last lines would be lost */
                _exit(0);
            }
            close(p[1]);
            slot[running].pid = pid;
            slot[running].fd = p[0];
            slot[running].index = first + next;
            running++;
            next++;
        }
        int st;
        pid_t done = wait(&st);
        if (done < 0) { if (errno == EINTR) continue; break; }
        int k = 0;
        while (k < running && slot[k].pid != done) k++;
        if (k == running) continue;
        soak_result *r = &agg_r;
        memset(r, 0, sizeof *r);
        size_t got = 0;
        char *b = (char *)r;
        ssize_t rd;
        while (got < sizeof *r && (rd = read(slot[k].fd, b + got, sizeof *r - got)) > 0) got += (size_t)rd;
        close(slot[k].fd);
        const int index = slot[k].index;
        slot[k] = slot[--running];
        if (WIFSIGNALED(st) || got != sizeof *r) {
            if (WIFSIGNALED(st) && WTERMSIG(st) == SIGALRM) { timed++; printf("TIMEOUT seed soak%08d\n", index); }
            else { crashed++; printf("CRASH seed soak%08d (signal %d)\n", index, WIFSIGNALED(st) ? WTERMSIG(st) : 0); }
            continue;
        }
        if (r->status == 1) { stuck++; printf("STUCK seed soak%08d at wave %d (screen %c) team %s\n", index, r->wave, r->stuck_screen, r->team); }
        else ended++;
        clears += r->clear;
        turns += r->turns;
        saves += r->saves;
        if (r->save_max > save_max) save_max = r->save_max;
        {
            /* each run's outcome, mixed in whatever order the runs finish: the same number when the same runs
             * play out the same way (with or without --save-test) */
            uint32_t h = 2166136261u;
            const uint32_t v[5] = {(uint32_t)index, (uint32_t)r->wave, (uint32_t)r->turns, (uint32_t)r->score, (uint32_t)r->clear};
            for (int i = 0; i < 5; i++) { h ^= v[i]; h *= 16777619u; }
            for (int i = 0; i < 16; i++) { h ^= r->stubs[i]; h *= 16777619u; }
            for (int i = 0; i < GC_MAX_UNPORTED; i++) if (r->unported[i]) { h ^= (uint32_t)i << 20 ^ r->unported[i]; h *= 16777619u; }
            if(getenv("GCPLAY_RUN_OUTCOMES")){printf("OUTCOME %d %08x wave%d turns%d score%d",index,h,r->wave,r->turns,r->score);for(int i=0;i<16;i++)if(r->stubs[i])printf(" stub%d=%u",i,r->stubs[i]);for(int i=0;i<GC_MAX_UNPORTED;i++)if(r->unported[i])printf(" unp%d=%u",i,r->unported[i]);printf("\n");fflush(stdout);}
            digest ^= h;
        }
        wave_hist[r->wave / 10 > 20 ? 20 : r->wave / 10]++;
        for (int i = 0; i < 16; i++) { stub_total[i] += r->stubs[i]; stub_runs[i] += r->stubs[i] > 0; }
        for (int i = 0; i < 8; i++) { rc_total[i] += r->run_core[i]; rc_runs[i] += r->run_core[i] > 0; }
        for (int i = 0; i < N_APPLY_KEYS; i++) {
            taken_total[i] += r->apply_taken[i];
            taken_runs[i] += r->apply_taken[i] > 0;
            refused_total[i] += r->apply_refused[i];
        }
        for (int t = 0; t < r->n_texts; t++) {
            int k = 0;
            while (k < n_texts && strcmp(texts[k].site, r->texts[t].site)) k++;
            if (k == n_texts) { if (n_texts >= 256) continue; snprintf(texts[n_texts++].site, 40, "%s", r->texts[t].site); }
            texts[k].runs++;
            texts[k].n += r->texts[t].n;
        }
        for (int i = 0; i < GC_MAX_UNPORTED; i++) { hit_total[i] += r->unported[i]; hit_runs[i] += r->unported[i] > 0; }
    }
    printf("\n%d runs: %d ended (%d cleared), %d stuck, %d crashed, %d timed out; %ld turns (%.1f per run)\n",
           n, ended, clears, stuck, crashed, timed, turns, n ? (double)turns / n : 0.0);
    printf("final wave: ");
    for (int i = 0; i <= 20; i++) if (wave_hist[i]) printf(" %d-%d:%d", i * 10, i * 10 + 9, wave_hist[i]);
    printf("\nruns' digest: %08x", digest);
    if (save_test) printf("\nsave/load round trips: %ld (largest save %u bytes of %u)", saves, save_max, (unsigned)GC_SAVE_MAX);
    printf("\n\nstand-ins (runs that used it, times):\n");
    for (int i = 0; i < GC_STUB_COUNT_; i++)
        if (stub_total[i]) printf("  %-50s %6u runs %9u\n", gc_stub_name[i], stub_runs[i], stub_total[i]);
    printf("\nreward choices by class (taken: runs, times; refused: times):\n");
    for (int i = 0; i < N_APPLY_KEYS; i++)
        if (taken_total[i] || refused_total[i])
            printf("  %-50s %6u runs %9u taken %9u refused\n", pr_item_keys[i], taken_runs[i], taken_total[i], refused_total[i]);
    static const char *const rc_name[8] = {"daily custom seed", "challenges", "upstream undefined", "species pool",
                                           "fixed classic battle", "trainer config", "bit 6", "bit 7"};
    printf("\nrun-core stand-ins (runs, times):\n");
    for (int i = 0; i < 8; i++) if (rc_total[i]) printf("  %-50s %6u runs %9u\n", rc_name[i], rc_runs[i], rc_total[i]);
    qsort(texts, (size_t)n_texts, sizeof texts[0], cmp_site);
    printf("\nbattle texts not ported yet, by runs that showed one (%d engine sites; the top 15):\n", n_texts);
    for (int k = 0; k < n_texts && k < 15; k++) printf("  %-44s %6u runs %9u\n", texts[k].site, texts[k].runs, texts[k].n);
    static uint32_t order[GC_MAX_UNPORTED][2];
    int m = 0;
    for (int i = 0; i < GC_MAX_UNPORTED; i++) if (hit_runs[i]) { order[m][0] = (uint32_t)i; order[m][1] = hit_runs[i]; m++; }
    qsort(order, (size_t)m, sizeof order[0], cmp_desc);
    printf("\nbattle-core and ai-core unported, by runs that hit it (%d kinds):\n", m);
    for (int i = 0; i < m; i++)
        printf("  %-44s %6u runs %9u\n", pr_ai_unported_name((uint16_t)order[i][0]), order[i][1], hit_total[order[i][0]]);
    return crashed || timed ? 1 : 0;
}

/* ---------------------------------------------------------------- interactive */
static int parse_species(const char *name)
{
    char *end;
    long v = strtol(name, &end, 10);
    if (*name && !*end) return (int)v;
    for (int i = 0; i < PR_SPECIES_COUNT; i++) if (!strcmp(pr_species_table[i].name, name)) return pr_species_table[i].id;
    return -1;
}

static int play(const char *seed, const char *starters)
{
    static gc_run run;
    gc_run_new(&run, GAME_MODES_CLASSIC, seed, (uint32_t)getpid());
    run.ask_switch = ask_switch;
    gc_starter team[6];
    int n = 0;
    char buf[256];
    snprintf(buf, sizeof buf, "%s", starters ? starters : "BULBASAUR,CHARMANDER,SQUIRTLE");
    for (char *t = strtok(buf, ","); t && n < 6; t = strtok(NULL, ",")) {
        int sp = parse_species(t);
        if (sp < 0) { fprintf(stderr, "no species %s\n", t); return 2; }
        memset(&team[n], 0, sizeof team[n]);
        team[n].species = (uint16_t)sp;
        team[n].tera_type = -1;
        for (int i = 0; i < 6; i++) team[n].ivs[i] = 15;
        n++;
    }
    const char *why;
    if (!gc_run_set_starters(&run, team, n, &why)) { fprintf(stderr, "starters refused: %s\n", why); return 2; }
    char line[256];
    for (;;) {
        drain(&run, true);
        print_screen(&run);
        const gc_screen *s = gc_run_screen(&run);
        if (s->kind == GC_SCREEN_GAME_OVER || s->kind == GC_SCREEN_CLEAR) return 0;
        printf("> ");
        fflush(stdout);
        if (!fgets(line, sizeof line, stdin)) return 0;
        gc_choice c = {0};
        char cmd[8] = "";
        int a = -1, b = -1, d = -1;
        int k = sscanf(line, "%7s %d %d %d", cmd, &a, &b, &d);
        if (k <= 0) continue;
        switch (s->kind) {
        case GC_SCREEN_TARGET:c.kind=cmd[0]=='c'?GC_CH_CANCEL:GC_CH_TARGET;c.a=atoi(cmd);break;
        case GC_SCREEN_COMMAND:
            c.kind = cmd[0] == 'p' ? GC_CH_POKEMON : cmd[0] == 'b' ? GC_CH_BALL : cmd[0] == 'r' ? GC_CH_RUN : GC_CH_FIGHT;
            c.a = (int16_t)a; c.b = -1;
            break;
        case GC_SCREEN_SWITCH: c.kind = GC_CH_PICK; c.a = (int16_t)atoi(cmd); break;
        case GC_SCREEN_LEARN_MOVE: c.kind = GC_CH_FORGET; c.a = (int16_t)atoi(cmd); break;
        case GC_SCREEN_EVOLVE: c.kind = cmd[0] == 'y' ? GC_CH_EVOLVE : GC_CH_CANCEL; c.a = cmd[0] == 'N'; break;
        case GC_SCREEN_CHECK_SWITCH: c.kind = GC_CH_CONFIRM; c.a = cmd[0] == 'y'; break;
        case GC_SCREEN_REWARD:
            c.kind = cmd[0] == 'o' ? GC_CH_REWARD : cmd[0] == 's' ? GC_CH_SHOP : cmd[0] == 'r' ? GC_CH_REROLL : GC_CH_SKIP;
            c.a = (int16_t)a; c.b = (int16_t)b; c.c = (int16_t)d;
            break;
        case GC_SCREEN_BIOME: c.kind = GC_CH_BIOME; c.a = (int16_t)atoi(cmd); break;
        case GC_SCREEN_MYSTERY: c.kind = GC_CH_MYSTERY; c.a = (int16_t)a; c.b = (int16_t)b; c.c = (int16_t)d; break;
        default: break;
        }
        if (!gc_run_choose(&run, &c, &why)) printf("  not allowed: %s\n", why);
    }
}

int main(int argc, char **argv)
{
    const char *seed = NULL, *starters = NULL;
    int autos = 0, first = 0, jobs = 4, verbose = 0, timeout = 60;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--seed") && i + 1 < argc) seed = argv[++i];
        else if (!strcmp(argv[i], "--starters") && i + 1 < argc) starters = argv[++i];
        else if (!strcmp(argv[i], "--auto") && i + 1 < argc) autos = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--first") && i + 1 < argc) first = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--jobs") && i + 1 < argc) jobs = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--timeout") && i + 1 < argc) timeout = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--verbose")) verbose = 1;
        else if (!strcmp(argv[i], "--save-test")) save_test = 1;
        else if (!strcmp(argv[i], "--ask-switch")) ask_switch = true;
        else if (!strcmp(argv[i], "--locales") && i + 1 < argc) locales = argv[++i];
        else { fprintf(stderr, "usage: %s [--seed S] [--starters A,B,C] | --auto N [--first K] [--jobs J] [--verbose] [--save-test] [--ask-switch] [--locales DIR]\n", argv[0]); return 2; }
    }
    if (autos > 0) return soak(autos, first, jobs < 1 ? 1 : jobs, verbose, timeout);
    return play(seed, starters);
}
