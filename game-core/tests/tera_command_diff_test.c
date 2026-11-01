/* The player's Terastallize command against upstream at 8555c08: replays the trace of tests/tera-command.producer.test.ts (a real singles and a
 * real doubles battle; each case sets the Tera Orb, the arena's Tera counter, the Tera already chosen for slot 0, the field Pokemon's species,
 * form, fusion half, Tera type and moves with used PP, then asks upstream's canTerastallize, CommandUiHandler.canTera, getTeraType and
 * CommandPhase.handleCommand(TERA)) through game-core's gc_can_terastallize / gc_battle_command_screen / gc_battle_player_command.
 *
 * Per case, compared with what upstream did: canTerastallize; canTera (the screen's can_tera); the Tera type (pr_get_tera_type and the screen's
 * tera_type when offered); when the menu offers the command, whether the command was accepted (a spent move while another has PP is refused;
 * Struggle is taken when every move is spent) and the pre-turn command (command, skip, targets) and the turn command (command, cursor, move)
 * it wrote; when the menu does not offer it game-core must refuse and leave the commands as they were.
 * Usage: tera_command_diff_test <trace>; exit 0 only with 0 mismatches. */
#include "../src/internal.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static gc_run single_run, double_run;
static gc_system sys;
static FILE *f;

static long long integer(void) { long long v; if (fscanf(f, "%lld", &v) != 1) { fputs("malformed input\n", stderr); exit(2); } return v; }
static void tag(char want)
{
    char c;
    if (fscanf(f, " %c", &c) != 1 || c != want) { fprintf(stderr, "wrong record: wanted %c, got %c\n", want, c); exit(2); }
}
static void bracket(char want)
{
    char c;
    if (fscanf(f, " %c", &c) != 1 || c != want) { fprintf(stderr, "wrong bracket: wanted %c\n", want); exit(2); }
}

typedef struct {
    int species, form, fusion, fusion_form, tera, n_moves;
    int move[4], pp_used[4];
} mon_row;

static void start_single(void)
{
    gc_run *r = &single_run;
    memset(r, 0, sizeof *r);
    gc_system_init_defaults(&sys);
    gc_run_new(r, GAME_MODES_CLASSIC, "tera-command-diff", 4242);
    r->sys = &sys;
    r->n_items = 0;
    r->n_party = 2;
    for (int i = 0; i < 2; i++) {
        gc_starter s;
        memset(&s, 0, sizeof s);
        s.species = (uint16_t)(i == 0 ? SPECIES_ID_PIKACHU : SPECIES_ID_BULBASAUR);
        s.tera_type = -1;
        for (int k = 0; k < 6; k++) s.ivs[k] = 31;
        s.moves[0] = MOVE_ID_SPLASH;
        gc_mon_new_player(r, &r->party[i], &s, 50);
        r->party_order[i] = (int8_t)i;
    }
    r->money = 1000;
    r->battle_type = BATTLE_TYPE_WILD;
    gc_items_to_battle(r);
    gc_rq_reset(&gc_int(r)->rq);
    r->wave = 5;
    r->in_battle = true;
    gc_rq_push(r, gc_rp(RP_NEW_BATTLE, 0, 0, 0));
    gc_run_until_screen(r);
    gc_event e;
    while (gc_run_next_event(r, &e)) {}
    if (r->screen.kind != GC_SCREEN_COMMAND || !r->bt.field[0]) { fputs("no singles battle\n", stderr); exit(2); }
}

static void start_double(void)
{
    gc_run *r = &double_run;
    for (int seed = 1; seed <= 60; seed++) {
        char key[64];
        snprintf(key, sizeof key, "tera-diff-doubles-%d", seed);
        memset(r, 0, sizeof *r);
        gc_run_new(r, GAME_MODES_ENDLESS, key, seed);
        r->wave = 1;
        gc_starter team[3] = {{.species = 1, .tera_type = -1}, {.species = 4, .tera_type = -1}, {.species = 7, .tera_type = -1}};
        for (int k = 0; k < 3; k++) memset(team[k].ivs, 15, 6);
        const char *why = NULL;
        if (!gc_run_set_starters(r, team, 3, &why)) { fprintf(stderr, "set_starters: %s\n", why ? why : "?"); exit(2); }
        gc_event e;
        while (gc_run_next_event(r, &e)) {}
        if (r->double_battle && r->battle_type == BATTLE_TYPE_WILD && r->bt.field[0] && r->bt.field[1] && r->screen.kind == GC_SCREEN_COMMAND) return;
    }
    fputs("no doubles battle\n", stderr);
    exit(2);
}

static void set_mon(pr_pokemon *p, const mon_row *m)
{
    p->species = (uint16_t)m->species;
    p->form = (uint8_t)m->form;
    p->fusion_species = (uint16_t)(m->fusion < 0 ? 0 : m->fusion);
    p->fusion_form = (uint8_t)m->fusion_form;
    p->tera_type = (int8_t)m->tera;
    p->terastallized = false;
    memset(p->moves, 0, sizeof p->moves);
    memset(p->pp_used, 0, sizeof p->pp_used);
    p->n_moves = (uint8_t)m->n_moves;
    for (int i = 0; i < m->n_moves; i++) {
        p->moves[i] = (uint16_t)m->move[i];
        p->pp_used[i] = (uint8_t)m->pp_used[i];
        p->max_pp_override[i] = -1;
    }
    pr_sync_moveset_aliases(p);
}

int main(int argc, char **argv)
{
    if (argc != 2 || !(f = fopen(argv[1], "r"))) { fputs("usage: tera_command_diff_test <trace>\n", stderr); return 2; }
    char head[16];
    int version, cases;
    if (fscanf(f, "%15s %d %d", head, &version, &cases) != 3 || strcmp(head, "TERACMD") || version != 1 || cases < 1) { fputs("bad header\n", stderr); return 2; }
    start_single();
    start_double();
    int mismatches = 0, compared = 0, offered = 0, refused_ok = 0, accepted = 0, not_accepted = 0, on_slot1 = 0, planned = 0, struggle = 0, stellar = 0;
    int by_orb = 0, by_used = 0, by_form = 0;
    for (int i = 0; i < cases; i++) {
        tag('C');
        const int id = (int)integer(), dbl = (int)integer(), field = (int)integer(), orb = (int)integer(), used = (int)integer(), slot0 = (int)integer();
        const int cursor = (int)integer(), max_teras = (int)integer(), nmons = (int)integer();
        if (id != i || max_teras != 1) { fputs("case numbering\n", stderr); return 2; }
        mon_row mons[2];
        memset(mons, 0, sizeof mons);
        for (int k = 0; k < nmons; k++) {
            bracket('[');
            mon_row *m = &mons[k];
            m->species = (int)integer(); m->form = (int)integer(); m->fusion = (int)integer(); m->fusion_form = (int)integer(); m->tera = (int)integer();
            m->n_moves = (int)integer();
            if (m->n_moves < 1 || m->n_moves > 4) { fputs("moves\n", stderr); return 2; }
            for (int j = 0; j < m->n_moves; j++) { m->move[j] = (int)integer(); m->pp_used[j] = (int)integer(); }
            bracket(']');
        }
        tag('R');
        const int u_can_tera = (int)integer(), u_can_pokemon = (int)integer(), u_tera_type = (int)integer(), u_ok = (int)integer();
        const int u_pre_cmd = (int)integer(), u_pre_skip = (int)integer(), u_pre_n = (int)integer();
        int u_pre_t[4] = {0};
        for (int j = 0; j < u_pre_n; j++) { const int v = (int)integer(); if (j < 4) u_pre_t[j] = v; }
        const int u_cmd = (int)integer(), u_cursor = (int)integer(), u_move = (int)integer();

        gc_run *r = dbl ? &double_run : &single_run;
        if ((dbl ? 2 : 1) != nmons) { fputs("party size\n", stderr); return 2; }
        for (int k = 0; k < nmons; k++) set_mon(r->bt.field[k], &mons[k]);
        while (gc_item_stack(r, "TERA_ORB") > 0) gc_item_remove_stacks(r, "TERA_ORB", 1);
        if (orb) gc_item_add(r, "TERA_ORB", -1, 0, 1);
        r->bt.player_teras_used_known = true;
        r->bt.player_teras_used = used;
        for (int k = 0; k < 4; k++) {
            memset(&r->bt.commands[k], 0, sizeof r->bt.commands[k]);
            memset(&r->bt.pre_commands[k], 0, sizeof r->bt.pre_commands[k]);
            r->bt.commands[k].command = r->bt.pre_commands[k].command = -1;
        }
        if (slot0) {
            r->bt.pre_commands[0].command = COMMAND_TERA;
            r->bt.pre_commands[0].n_targets = 1;
            r->bt.pre_commands[0].targets[0] = 0;
        }
        r->screen.slot = (int8_t)field;
        gc_battle_command_screen(r);
        pr_pokemon *mon = r->bt.field[field];
        char why_buf[64];
        why_buf[0] = 0;
        int bad = 0;
#define EXPECT(cond, what) do { if (!(cond)) { bad++; if (mismatches < 12) fprintf(stderr, "case %d (%s, slot %d): %s\n", i, dbl ? "doubles" : "singles", field, what); } } while (0)
        EXPECT(gc_can_terastallize(r, mon) == (u_can_pokemon != 0), "canTerastallize");
        EXPECT(r->screen.can_tera == (u_can_tera != 0), "canTera (the screen's can_tera)");
        EXPECT(pr_get_tera_type(mon) == u_tera_type, "getTeraType");
        if (u_can_tera) EXPECT(r->screen.tera_type == u_tera_type, "the button's type");
        const struct pr_turn_command before_turn = r->bt.commands[field], before_pre = r->bt.pre_commands[field];
        const char *why = NULL;
        const bool ok = gc_battle_player_command(r, &(gc_choice){GC_CH_FIGHT, (int16_t)cursor, -1, 1}, &why);
        if (u_can_tera) {
            offered++;
            EXPECT(ok == (u_ok != 0), "the command is accepted as upstream's CommandPhase does");
            if (u_ok) {
                accepted++;
                const struct pr_turn_command *pc = &r->bt.pre_commands[field], *tc = &r->bt.commands[field];
                EXPECT(pc->command == u_pre_cmd && (pc->skip ? 1 : 0) == u_pre_skip, "pre-turn command and skip");
                EXPECT(u_pre_n == 1 && pc->n_targets == 1 && pc->targets[0] == u_pre_t[0], "pre-turn command target");
                EXPECT(tc->command == u_cmd && tc->cursor == u_cursor && tc->move == u_move, "turn command (cursor, move)");
                if (u_move == MOVE_ID_STRUGGLE) struggle++;
            } else {
                not_accepted++;
            }
            if (field == 1) on_slot1++;
            if (slot0) planned++;
            if (u_tera_type == POKEMON_TYPE_STELLAR) stellar++;
        } else {
            refused_ok++;
            EXPECT(!ok && why && *why, "game-core refuses where the menu does not offer the command");
            EXPECT(!memcmp(&before_turn, &r->bt.commands[field], sizeof before_turn) && !memcmp(&before_pre, &r->bt.pre_commands[field], sizeof before_pre),
                   "a refused command leaves the commands as they were");
        }
        if (!orb) by_orb++;
        if (used) by_used++;
        if (mon->form) by_form++;
        compared++;
        if (bad) mismatches++;
        (void)why_buf;
    }
    char end[16];
    if (fscanf(f, "%15s", end) != 1 || strcmp(end, "END")) { fputs("missing END\n", stderr); return 2; }
    printf("TOTAL cases=%d compared=%d mismatches=%d offered=%d accepted=%d refused-by-pp=%d not-offered(refused)=%d slot1=%d planned-slot0=%d struggle=%d stellar=%d "
           "no-orb=%d counter-used=%d non-base-form=%d\n",
           cases, compared, mismatches, offered, accepted, not_accepted, refused_ok, on_slot1, planned, struggle, stellar, by_orb, by_used, by_form);
    return mismatches ? 1 : 0;
}
