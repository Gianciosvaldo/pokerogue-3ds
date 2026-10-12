/* Replays the upstream biome weather/terrain trace (tests/biome-fields.producer.test.ts) through gc_arena_new_battle:
 * the weather and terrain left (type, turns left, max duration), the messages emitted in order, and the run RNG.
 * Usage: biome_fields_diff_test <trace>; exit 0 only with 0 mismatches. */
#include "../src/internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static gc_run run;
static gc_system sys;

static int rd_rng(FILE *f, pr_rng *r)
{
    char tag[8];
    return fscanf(f, "%7s %lf %lf %lf %lf", tag, &r->c, &r->s0, &r->s1, &r->s2) == 5;
}

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "usage: biome_fields_diff_test <trace>\n"); return 2; }
    FILE *f = fopen(argv[1], "r");
    if (!f) { perror(argv[1]); return 2; }
    memset(&run, 0, sizeof run);
    gc_system_init_defaults(&sys);
    gc_run_new(&run, GAME_MODES_CLASSIC, "biome-fields-diff", 42);
    gc_starter team[1];
    memset(team, 0, sizeof team);
    team[0].species = SPECIES_ID_BULBASAUR;
    team[0].tera_type = -1;
    for (int k = 0; k < 6; k++) team[0].ivs[k] = 15;
    const char *why;
    if (!gc_run_set_starters(&run, team, 1, &why)) { fprintf(stderr, "starters: %s\n", why); return 2; }
    run.sys = &sys;
    gc_event e;
    while (gc_run_next_event(&run, &e)) {}

    char tag[8];
    int cases = 0, mismatches = 0, total = 0, version = 0, shown = 0;
    int by_weather[10] = {0}, msgs_seen = 0, night_rolls = 0;
    while (fscanf(f, "%7s", tag) == 1) {
        if (!strcmp(tag, "V")) { if (fscanf(f, "%d %d", &version, &total) != 2) return 2; continue; }
        if (!strcmp(tag, "Z")) { int n; if (fscanf(f, "%d", &n) != 1 || n != cases) { fprintf(stderr, "case count %d != %d\n", cases, n); return 2; } continue; }
        if (strcmp(tag, "C")) { fprintf(stderr, "bad tag %s\n", tag); return 2; }
        int i, biome, wave, off, mode, pw, pwt, pwm, pt, ptt, ptm;
        if (fscanf(f, "%d %d %d %d %d %d %d %d %d %d %d", &i, &biome, &wave, &off, &mode, &pw, &pwt, &pwm, &pt, &ptt, &ptm) != 11) return 2;
        pr_rng before, after;
        if (!rd_rng(f, &before)) return 2;
        int w, wt, wm, t, tt, tm, nmsg;
        if (fscanf(f, "%7s %d %d %d %d %d %d", tag, &w, &wt, &wm, &t, &tt, &tm) != 7) return 2;
        if (fscanf(f, "%7s %d", tag, &nmsg) != 2) return 2;
        char want[4][96];
        if (nmsg > 4) return 2;
        for (int k = 0; k < nmsg; k++) if (fscanf(f, "%95s", want[k]) != 1) return 2;
        if (!rd_rng(f, &after)) return 2;
        if (fscanf(f, "%7s", tag) != 1 || strcmp(tag, "E")) return 2;

        run.biome = biome; run.wave = wave; run.wave_cycle_offset = off;
        run.run.rng = before;
        pr_battle *bt = &run.bt;
        bt->weather = (uint8_t)pw; bt->weather_turns_left = (int16_t)pwt; bt->weather_max_duration = (int16_t)pwm;
        bt->terrain = (uint8_t)pt; bt->terrain_turns_left = (int16_t)ptt; bt->terrain_max_duration = (int16_t)ptm;
        while (gc_run_next_event(&run, &e)) {}
        gc_arena_new_battle(&run, mode == 0 || mode == 1, mode == 0 || mode == 2);
        char got[16][96];
        int ngot = 0, extra = 0;
        while (gc_run_next_event(&run, &e)) {
            if (e.kind == GC_EV_MESSAGE && ngot < 16) { snprintf(got[ngot++], 96, "%s", e.msg.key); }
            else extra++;
        }
        bool bad = bt->weather != w || bt->weather_turns_left != wt || bt->weather_max_duration != wm
                   || bt->terrain != t || bt->terrain_turns_left != tt || bt->terrain_max_duration != tm
                   || ngot != nmsg || extra
                   || run.run.rng.c != after.c || run.run.rng.s0 != after.s0 || run.run.rng.s1 != after.s1 || run.run.rng.s2 != after.s2;
        for (int k = 0; !bad && k < nmsg; k++) bad = strcmp(got[k], want[k]) != 0;
        if (bad) {
            if (shown++ < 10)
                fprintf(stderr, "case %d biome %d wave %d off %d mode %d prior w%d/%d t%d/%d: got w%d/%d/%d t%d/%d/%d %d msgs (%s), want w%d/%d/%d t%d/%d/%d %d msgs (%s)%s\n",
                        i, biome, wave, off, mode, pw, pwt, pt, ptt, bt->weather, bt->weather_turns_left, bt->weather_max_duration,
                        bt->terrain, bt->terrain_turns_left, bt->terrain_max_duration, ngot, ngot ? got[0] : "", w, wt, wm, t, tt, tm,
                        nmsg, nmsg ? want[0] : "", memcmp(&run.run.rng, &after, sizeof after) ? " rng differs" : "");
            mismatches++;
        }
        cases++; msgs_seen += nmsg;
        if (mode == 0 || mode == 2) by_weather[w]++;
        {
            const int tod = pr_run_get_time_of_day(biome, wave, off);
            night_rolls += (mode == 0 || mode == 2) && (tod == TIME_OF_DAY_DUSK || tod == TIME_OF_DAY_NIGHT);
        }
    }
    printf("Biome weather and terrain: %d cases (%d messages, %d rolled at dusk or night; weathers", cases, msgs_seen, night_rolls);
    for (int k = 0; k < 10; k++) printf(" %d", by_weather[k]);
    printf("), TOTAL mismatches %d\n", mismatches);
    return mismatches ? 1 : 0;
}
