/* Memory Mushroom / move relearning (Task 43).
 * Ported from PlayerPokemon.getLearnableLevelMoves(), RememberMoveModifierType,
 * PartyUiMode.REMEMBER_MOVE_MODIFIER and RememberMoveModifier.apply() at upstream 8555c08. */
#include "pr/reward.h"
#include <string.h>

#define MEMORY_MAX (PR_REWARD_MAX_LEVEL_MOVES + 4 + PR_REWARD_MAX_USED_TMS)

static bool known_move(const pr_reward_mon *m, uint16_t move)
{
    for (int i = 0; i < m->n_moves; i++)
        if (m->moves[i].move == move) return true;
    return false;
}

static bool move_in(const pr_reward_memory_move *v, int n, uint16_t move)
{
    for (int i = 0; i < n; i++)
        if (v[i].move == move) return true;
    return false;
}

static int base_source(int source)
{
    return source - source % 2;
}

int pr_reward_memory_moves(const pr_reward_ctx *ctx, int party_idx,
                           pr_reward_memory_move *out, int out_cap)
{
    if (!ctx || party_idx < 0 || party_idx >= ctx->n_party || out_cap < 0) return -1;
    const pr_reward_mon *m = &ctx->party[party_idx];
    pr_reward_memory_move tmp[MEMORY_MAX];
    int n = 0;

    /* getLevelMoves({
     *   startingLevel: 1, includeEvolutionMoves: true,
     *   includePrevolutionMoves: true, includeRelearnerMoves: true
     * })
     * The learnset parser lives with the caller; reward-core keeps the exact resulting rows. */
    for (int i = 0; i < m->n_level_moves && n < MEMORY_MAX; i++) {
        tmp[n++] = (pr_reward_memory_move){
            m->level_move_levels[i],
            m->level_moves[i],
            m->level_move_sources[i],
        };
    }

    /* Only run starters can relearn their unlocked egg moves; Fresh Start/challenge handling is
     * supplied as egg_relearn_allowed, and Daily never includes egg moves. */
    if (m->met_as_starter && m->egg_relearn_allowed
        && (!ctx->run || ctx->run->mode != GAME_MODES_DAILY)) {
        for (int i = 0; i < m->n_egg_moves && n < MEMORY_MAX; i++)
            tmp[n++] = (pr_reward_memory_move){-1, m->egg_moves[i], PR_LEARN_SOURCE_EGG};
    }

    /* usedTMs are appended only when that move is not already available from another source. */
    const int n_before_tms = n;
    for (int i = 0; i < m->n_used_tms && n < MEMORY_MAX; i++) {
        const uint16_t move = m->used_tms[i];
        /* Upstream filters the entire usedTMs array against the pre-existing rows,
         * not against earlier TMs appended from that array. Duplicate history survives. */
        if (!move_in(tmp, n_before_tms, move))
            tmp[n++] = (pr_reward_memory_move){-1, move, PR_LEARN_SOURCE_TM};
    }

    /* Remove currently known moves. */
    int w = 0;
    for (int i = 0; i < n; i++)
        if (!known_move(m, tmp[i].move)) tmp[w++] = tmp[i];
    n = w;

    /* JS Array.sort is stable. Sort by getBaseLearnableMoveSource descending while preserving
     * source order within the same group. */
    for (int i = 1; i < n; i++) {
        pr_reward_memory_move v = tmp[i];
        const int key = base_source(v.source);
        int j = i;
        while (j > 0 && base_source(tmp[j - 1].source) < key) {
            tmp[j] = tmp[j - 1];
            j--;
        }
        tmp[j] = v;
    }

    if (n > out_cap || (n && !out)) return -1;
    if (n) memcpy(out, tmp, (size_t)n * sizeof *out);
    return n;
}

int pr_reward_memory_begin(const pr_reward_ctx *ctx, int party_idx,
                           pr_reward_memory_move *out, int out_cap,
                           pr_reward_target_state *state)
{
    if (!state) return -1;
    memset(state, 0, sizeof *state);
    state->party_idx = party_idx;
    state->option_idx = -1;
    state->cost = -1.0;

    const int n = pr_reward_memory_moves(ctx, party_idx, out, out_cap);
    if (n <= 0) return n;

    state->mode = PR_REWARD_TARGET_REMEMBER_MOVE;
    return n;
}

bool pr_reward_memory_choose(const pr_reward_ctx *ctx, int party_idx, int option_idx, double cost,
                             pr_reward_target_state *state)
{
    if (!state) return false;
    pr_reward_memory_move moves[MEMORY_MAX];
    const int n = pr_reward_memory_moves(ctx, party_idx, moves, MEMORY_MAX);
    if (n <= 0 || option_idx < 0 || option_idx >= n) return false;

    memset(state, 0, sizeof *state);
    state->mode = PR_REWARD_TARGET_LEARN_MOVE;
    state->party_idx = party_idx;
    state->option_idx = option_idx;
    state->move = moves[option_idx].move;
    state->cost = cost;
    state->copy_reward_screen = true;
    return true;
}
