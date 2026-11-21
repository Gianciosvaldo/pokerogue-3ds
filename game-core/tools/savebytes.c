/* Saves of runs that never had a nickname (or any other state a build may have added since): every point a run can be saved, as a length and an
 * FNV-1a hash per save, and the system profile (gc_system_save) the run has built up at that point, the same way. tools/save-bytes-diff.sh builds this against two trees and compares what they print, so a change to the save format that
 * should not change a run's bytes shows up. Uses only what both trees have (the starters, the choices, gc_run_save). The party is leveled to 50 and
 * healed at every command so the runs reach the late waves (trainers, encounters, shops, biomes). */
#include "internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static gc_run run;
static gc_system sys;
static uint8_t buf[GC_SAVE_MAX], sbuf[1 << 20];
static uint64_t fnv(const uint8_t *p, size_t n){uint64_t h=1469598103934665603ULL;for(size_t i=0;i<n;i++){h^=p[i];h*=1099511628211ULL;}return h;}
int main(int argc, char **argv)
{
    const int runs = argc > 1 ? atoi(argv[1]) : 24;
    const int species[] = {SPECIES_ID_BULBASAUR, SPECIES_ID_CHARMANDER, SPECIES_ID_SQUIRTLE, SPECIES_ID_PIKACHU, SPECIES_ID_EEVEE, SPECIES_ID_MUDKIP, SPECIES_ID_TORCHIC, SPECIES_ID_RALTS};
    unsigned saves = 0;
    for (int r = 0; r < runs; r++) {
        char seed[32];
        snprintf(seed, sizeof seed, "bytes-%d", r);
        gc_system_init_defaults(&sys);
        memset(&run, 0, sizeof run);
        gc_run_new(&run, r % 5 == 4 ? GAME_MODES_ENDLESS : GAME_MODES_CLASSIC, seed, (uint32_t)(r * 7 + 1));
        gc_starter st[3];
        memset(st, 0, sizeof st);
        const int n = 1 + r % 3;
        for (int i = 0; i < n; i++) { st[i].species = (uint16_t)species[(r + i * 3) % 8]; st[i].tera_type = -1; memset(st[i].ivs, 15, 6); }
        const char *why = NULL;
        if (!gc_run_set_starters(&run, st, n, &why)) { printf("run %d: refused (%s)\n", r, why); continue; }
        run.sys = &sys;
        for (int i = 0; i < run.n_party; i++) { run.party[i].b.level = 50; gc_mon_recalc(&run, &run.party[i]); }
        for (int step = 0; step < 6000 && run.wave < 40; step++) {
            gc_event e;
            while (gc_run_next_event(&run, &e)) {}
            if (gc_run_can_save(&run)) {
                const size_t len = gc_run_save(&run, buf, sizeof buf);
                printf("run %d wave %d screen %d: %zu %016llx\n", r, run.wave, gc_run_screen(&run)->kind, len, (unsigned long long)fnv(buf, len));
                const size_t slen = gc_system_save(&sys, sbuf, sizeof sbuf);
                printf("run %d wave %d profile: %zu %016llx\n", r, run.wave, slen, (unsigned long long)fnv(sbuf, slen));
                saves++;
            }
            const gc_screen *s = gc_run_screen(&run);
            gc_choice c;
            memset(&c, 0, sizeof c);
            switch (s->kind) {
            case GC_SCREEN_MYSTERY: c.kind = GC_CH_MYSTERY; c.b = 0; for (int i = 0; i < run.n_party; i++) if (run.party[i].b.hp > 0) { c.b = (int16_t)i; break; } break;
            case GC_SCREEN_COMMAND: for (int i = 0; i < run.n_party; i++) run.party[i].b.hp = run.party[i].b.stats[0]; c.kind = GC_CH_FIGHT; c.b = -1; for (int i = 0; i < 4; i++) if (s->move_ok[i]) { c.a = (int16_t)i; break; } break;
            case GC_SCREEN_TARGET: c.kind = GC_CH_TARGET; c.a = -1; for (int k = 2; k < 4; k++) if (s->target_ok[k]) { c.a = (int16_t)k; break; } if (c.a < 0) for (int k = 0; k < 2; k++) if (s->target_ok[k]) { c.a = (int16_t)k; break; } break;
            case GC_SCREEN_SWITCH: c.kind = GC_CH_PICK; c.a = -1; for (int i = 0; i < run.n_party; i++) if (s->pick_ok[i]) { c.a = (int16_t)i; break; } break;
            case GC_SCREEN_LEARN_MOVE: c.kind = GC_CH_FORGET; c.a = -1; break;
            case GC_SCREEN_EVOLVE: c.kind = GC_CH_EVOLVE; c.a = 0; break;
            case GC_SCREEN_REWARD: c.kind = GC_CH_SKIP; break;
            case GC_SCREEN_BIOME: c.kind = GC_CH_BIOME; break;
            default: c.kind = GC_CH_CONFIRM; break;
            }
            if (!gc_run_choose(&run, &c, &why)) break;
        }
    }
    printf("saves %u\n", saves);
    return 0;
}
