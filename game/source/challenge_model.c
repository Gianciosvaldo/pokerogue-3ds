/* Challenge Mode's challenge screen: the model (see challenge_select.h). */
#include <string.h>

#include "challenge_select.h"
#include "pr_enums.h"

/* upstream's allChallenges order (src/data/challenge.ts: allChallenges) over the ids this port has */
static const int ORDER[] = {CHALLENGES_SINGLE_GENERATION, CHALLENGES_SINGLE_TYPE, CHALLENGES_FRESH_START, CHALLENGES_INVERSE_BATTLE,
                            CHALLENGES_FLIP_STAT, CHALLENGES_LIMITED_CATCH, CHALLENGES_LIMITED_SUPPORT, CHALLENGES_HARDCORE,
                            CHALLENGES_PASSIVES, CHALLENGES_MOVESET_RANDOMIZER};

const char* chm_key(int challenge) {
  switch (challenge) {
    case CHALLENGES_SINGLE_GENERATION: return "singleGeneration";
    case CHALLENGES_SINGLE_TYPE: return "singleType";
    case CHALLENGES_FRESH_START: return "freshStart";
    case CHALLENGES_INVERSE_BATTLE: return "inverseBattle";
    case CHALLENGES_FLIP_STAT: return "flipStat";
    case CHALLENGES_LIMITED_CATCH: return "limitedCatch";
    case CHALLENGES_LIMITED_SUPPORT: return "limitedSupport";
    case CHALLENGES_HARDCORE: return "hardcore";
    case CHALLENGES_PASSIVES: return "passives";
    case CHALLENGES_MOVESET_RANDOMIZER: return "movesetRandomizer";
    default: return "";
  }
}

void chm_init(chm_model* m, const gc_challenge_set* initial) {
  memset(m, 0, sizeof *m);
  const uint32_t played = gc_challenge_played_mask();
  for (size_t i = 0; i < sizeof ORDER / sizeof ORDER[0]; i++) {
    const int id = ORDER[i];
    if ((played >> id) & 1u && gc_challenge_max(id) > 0) m->rows[m->n++] = id;
  }
  if (initial) {
    for (int i = 0; i < m->n; i++) {
      const int id = m->rows[i];
      if (initial->value[id] <= gc_challenge_max(id)) m->v.value[id] = initial->value[id];
    }
  }
}

void chm_move(chm_model* m, int dir) {
  if (m->n <= 0) return;
  m->cursor = ((m->cursor + dir) % m->n + m->n) % m->n;
}

int chm_current(const chm_model* m) { return m->cursor >= 0 && m->cursor < m->n ? m->rows[m->cursor] : -1; }

void chm_step(chm_model* m) {
  const int id = chm_current(m);
  if (id < 0) return;
  m->v.value[id] = (uint8_t)(m->v.value[id] >= gc_challenge_max(id) ? 0 : m->v.value[id] + 1);
}

bool chm_change(chm_model* m, int dir) {
  const int id = chm_current(m);
  if (id < 0) return false;
  const int now = m->v.value[id], next = now + (dir > 0 ? 1 : -1);
  if (next < 0 || next > gc_challenge_max(id)) return false;
  m->v.value[id] = (uint8_t)next;
  return true;
}

bool chm_any(const chm_model* m) {
  for (int i = 0; i < m->n; i++)
    if (m->v.value[m->rows[i]]) return true;
  return false;
}

bool chm_can_start(const chm_model* m) { return chm_any(m); }
