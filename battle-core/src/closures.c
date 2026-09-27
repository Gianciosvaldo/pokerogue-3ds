/* Links hand-ported closures to upstream closure ids by owner + source hash. */
#include "pr/battle.h"
#include <string.h>

extern const pr_closure_reg pr_move_closures[], pr_ability_closures[], pr_condition_closures[];
extern const int pr_move_closures_n, pr_ability_closures_n, pr_condition_closures_n;

static pr_closure_fn table[PR_CLOSURE_COUNT];

/* The source hash of `() => true` (the default condition StatStageChangeAttr and co. give
 * themselves): any closure with exactly that source is the constant true, whoever owns it */
#define CONST_TRUE_HASH "24044144b7ba"
static double const_true(pr_battle *bt, const pr_closure_info *self, const pr_pokemon *a, const pr_pokemon *b, int move)
{
    (void)bt; (void)self; (void)a; (void)b; (void)move;
    return 1;
}
static bool ready;
static int ported;

void pr_closures_init(void)
{
    if (ready) return;
    const pr_closure_reg *regs[3] = {pr_move_closures, pr_ability_closures, pr_condition_closures};
    const int nregs[3] = {pr_move_closures_n, pr_ability_closures_n, pr_condition_closures_n};
    for (int i = 0; i < PR_CLOSURE_COUNT; i++) {
        if (!strcmp(pr_closures_meta[i].hash, CONST_TRUE_HASH)) {
            table[i] = const_true;
            ported++;
            continue;
        }
        /* an entry for this owner wins over one for any owner (owner NULL) */
        for (int any = 0; any < 2 && !table[i]; any++)
            for (int r = 0; r < 3 && !table[i]; r++)
                for (int k = 0; k < nregs[r]; k++)
                    if ((any ? !regs[r][k].owner : regs[r][k].owner && !strcmp(pr_closures_meta[i].owner, regs[r][k].owner))
                        && !strcmp(pr_closures_meta[i].hash, regs[r][k].hash)) {
                        table[i] = regs[r][k].fn;
                        ported++;
                        break;
                    }
    }
    ready = true;
}

int pr_closures_ported(void)
{
    pr_closures_init();
    return ported;
}

uint32_t pr_closure_misses[PR_CLOSURE_COUNT], pr_closure_hits[PR_CLOSURE_COUNT];

bool pr_closure_matches(uint16_t id, const char *owner, const char *hash)
{
    return id < PR_CLOSURE_COUNT && !strcmp(pr_closures_meta[id].owner, owner) && !strcmp(pr_closures_meta[id].hash, hash);
}

double pr_closure_call(pr_battle *bt, uint16_t id, uint16_t cls, double fallback,
                       const pr_pokemon *a, const pr_pokemon *b, int move)
{
    pr_closures_init();
    if (id < PR_CLOSURE_COUNT && table[id]) {
        pr_closure_hits[id]++;
        return table[id](bt, &pr_closures_meta[id], a, b, move);
    }
    if (id < PR_CLOSURE_COUNT) pr_closure_misses[id]++;
    pr_note_unported(bt, cls);
    return fallback;
}
