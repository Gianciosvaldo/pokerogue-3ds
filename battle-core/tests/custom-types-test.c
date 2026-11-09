/* Replay of the actual pinned getTypes() with customPokemonData.types / fusionCustomPokemonData.types
 * (tests/custom-types.producer.test.ts) against pr_get_types. A malformed trace is exit 2, a mismatch exit 1. */
#include "pr/engine.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int num(char **p, long lo, long hi, long *out)
{
    char *end; long v = strtol(*p, &end, 10);
    if (end == *p || v < lo || v > hi) return 0;
    *p = end; *out = v; return 1;
}
#define NEXT(lo, hi, var) do { long v_; if (!num(&q, (lo), (hi), &v_)) return 2; (var) = v_; } while (0)

int main(int argc, char **argv)
{
    if (argc != 2) return 2;
    FILE *f = fopen(argv[1], "r");
    if (!f) return 2;
    static char line[1024];
    unsigned version, n, seed; char extra;
    if (!fgets(line, sizeof line, f) || sscanf(line, "CUSTOMTYPES %u %u %u %c", &version, &n, &seed, &extra) != 3 || version != 1 || n < 1000) return 2;
    unsigned failures = 0, rows = 0, with_custom = 0, with_fusion_custom = 0, effective = 0;
    for (unsigned i = 0; i < n; i++) {
        long idx, sp, form, fsp, fform, c0, c1, fc0, fc1, nsum, s0, s1, added, tera, ttype;
        if (!fgets(line, sizeof line, f) || line[0] != 'C') return 2;
        char *q = line + 1;
        NEXT(0, 1000000, idx); NEXT(1, 65535, sp); NEXT(0, 255, form); NEXT(0, 65535, fsp); NEXT(0, 255, fform);
        NEXT(0, 18, c0); NEXT(0, 18, c1); NEXT(0, 18, fc0); NEXT(0, 18, fc1);
        NEXT(0, 2, nsum); NEXT(0, 17, s0); NEXT(0, 17, s1); NEXT(0, 17, added); NEXT(0, 1, tera); NEXT(-1, 18, ttype);
        if ((unsigned)idx != i) return 2;
        pr_pokemon p; memset(&p, 0, sizeof p);
        p.species = (uint16_t)sp; p.form = (uint8_t)form;
        p.fusion_species = (uint16_t)fsp; p.fusion_form = (uint8_t)fform;
        const pr_species *s = pr_pokemon_species(&p);
        if (!s || form >= (s->nforms ? s->nforms : 1)) return 2;
        /* custom slots: the trace carries 0 = null, else type+1, exactly the field's own encoding */
        p.custom_types[0] = (uint8_t)c0; p.custom_types[1] = (uint8_t)c1;
        p.fusion_custom_types[0] = (uint8_t)fc0; p.fusion_custom_types[1] = (uint8_t)fc1;
        p.n_summon_types = (uint8_t)nsum; p.summon_types[0] = (int8_t)s0; p.summon_types[1] = (int8_t)s1;
        p.added_type = (int8_t)added; p.terastallized = tera != 0; p.tera_type = (int8_t)ttype;
        if (c0 || c1) with_custom++;
        if (fsp && (fc0 || fc1)) with_fusion_custom++;
        for (int m = 0; m < 16; m++) {
            long mask, cnt, want[5] = {0};
            if (!fgets(line, sizeof line, f) || line[0] != 'R') return 2;
            q = line + 1;
            NEXT(0, 15, mask); NEXT(1, 5, cnt);
            if (mask != m) return 2;
            for (int k = 0; k < cnt; k++) NEXT(-1, 18, want[k]);
            unsigned flags = (m & 1 ? PR_TYPES_NO_TERA : 0) | (m & 2 ? PR_TYPES_ORIGINAL_IF_STELLAR : 0)
                           | (m & 4 ? PR_TYPES_BYPASS_SUMMON : 0) | (m & 8 ? PR_TYPES_IGNORE_THIRD : 0);
            int8_t got[PR_MAX_TYPES] = {0};
            int gn = pr_get_types(&p, got, flags);
            bool same = gn == cnt;
            for (int k = 0; same && k < cnt; k++) same = got[k] == want[k];
            rows++;
            if (!same) {
                if (failures < 12) fprintf(stderr, "case %u mask %d: %ld+%ld form %ld/%ld custom %ld %ld fusion %ld %ld expected %ld types, got %d\n",
                                           i, m, sp, fsp, form, fform, c0, c1, fc0, fc1, cnt, gn);
                failures++;
            }
            /* does the custom data change the answer at all? counted once per case, on the plain mask */
            if (m == 0 && (c0 || c1 || fc0 || fc1)) {
                pr_pokemon bare = p; memset(bare.custom_types, 0, 2); memset(bare.fusion_custom_types, 0, 2);
                int8_t g2[PR_MAX_TYPES] = {0}; int n2 = pr_get_types(&bare, g2, 0);
                if (n2 != gn || memcmp(g2, got, (size_t)gn)) effective++;
            }
        }
    }
    unsigned done;
    if (!fgets(line, sizeof line, f) || sscanf(line, "CUSTOMTYPES_COMPLETE %u %c", &done, &extra) != 1 || done != n || fgetc(f) != EOF) return 2;
    fclose(f);
    printf("custom types: %u cases (%u rows), %u with custom types, %u with fusion custom types, %u where they change the result, %u failures (seed %u)\n",
           n, rows, with_custom, with_fusion_custom, effective, failures, seed);
    /* a corpus that never exercises the field proves nothing */
    if (with_custom < n / 10 || with_fusion_custom < n / 20 || effective < n / 20) return 2;
    return failures ? 1 : 0;
}
