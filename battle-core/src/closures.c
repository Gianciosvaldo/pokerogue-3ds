/* Links hand-ported closures to upstream closure ids by owner + source hash. */
#include "pr/battle.h"
#include <string.h>

extern const pr_closure_reg pr_move_closures[], pr_ability_closures[];
extern const int pr_move_closures_n, pr_ability_closures_n;

static pr_closure_fn table[PR_CLOSURE_COUNT];
static bool ready;
static int ported;

void pr_closures_init(void)
{
    if (ready) return;
    const pr_closure_reg *regs[2] = {pr_move_closures, pr_ability_closures};
    const int nregs[2] = {pr_move_closures_n, pr_ability_closures_n};
    for (int i = 0; i < PR_CLOSURE_COUNT; i++)
        for (int r = 0; r < 2 && !table[i]; r++)
            for (int k = 0; k < nregs[r]; k++)
                if (!strcmp(pr_closures_meta[i].owner, regs[r][k].owner)
                    && !strcmp(pr_closures_meta[i].hash, regs[r][k].hash)) {
                    table[i] = regs[r][k].fn;
                    ported++;
                    break;
                }
    ready = true;
}

int pr_closures_ported(void)
{
    pr_closures_init();
    return ported;
}

double pr_closure_call(pr_battle *bt, uint16_t id, uint16_t cls, double fallback,
                       const pr_pokemon *a, const pr_pokemon *b, int move)
{
    pr_closures_init();
    if (id < PR_CLOSURE_COUNT && table[id]) return table[id](bt, &pr_closures_meta[id], a, b, move);
    pr_note_unported(bt, cls);
    return fallback;
}
