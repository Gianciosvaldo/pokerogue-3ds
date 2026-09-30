/* The learn-move dialog: upstream's LearnMovePhase (src/phases/learn-move-phase.ts)
 * from start() on, minus the parts that aren't UI (TM and Memory Mushroom
 * bookkeeping, move animations, form changes: the run logic does those). */
#include <stdio.h>
#include <string.h>

#include "menus.h"
#include "pr_data.h"

static struct {
  learn_move_args a;
  learn_move_host h;
  char text[640];
} L;

static void show(const char* text, bool prompt, menu_cb done) {
  snprintf(L.text, sizeof L.text, "%s", text);
  L.h.show_text(L.h.host, L.text, prompt, done, NULL);
}

static void end(void* ctx) {
  (void)ctx;
  if (L.h.on_end) L.h.on_end(L.h.host);
}

static const char* new_move_name(void) { return menu_move_name(L.a.move); }

/* learnMove: the move goes into `slot`; `pre` is the "Poof!" text or NULL */
static void learned(void* ctx) {
  (void)ctx;
  mui_sfx("se/level_up_fanfare");
  char t[256];
  mui_tf(t, sizeof t, "battle:learnMove", "pokemonName", L.a.pokemon_name, "moveName", new_move_name(), NULL);
  show(t, true, end);
}

static void learn(int slot, const char* pre) {
  if (L.h.on_result) L.h.on_result(L.h.host, slot);
  if (pre)
    show(pre, true, learned);
  else
    learned(NULL);
}

static void replace_check(void);

static void not_learned(void* ctx) {
  (void)ctx;
  if (L.h.on_result) L.h.on_result(L.h.host, -1);
  char t[256];
  mui_tf(t, sizeof t, "battle:learnMoveNotLearned", "pokemonName", L.a.pokemon_name, "moveName", new_move_name(),
         NULL);
  show(t, true, end);
}

static void replace_again(void* ctx) {
  (void)ctx;
  replace_check();
}

static void stop_teaching_confirm(void* ctx) {
  (void)ctx;
  L.h.confirm(L.h.host, not_learned, replace_again, NULL);
}

/* rejectMoveAndEnd */
static void reject(void) {
  if (!L.a.level_move_confirmation) {
    not_learned(NULL);
    return;
  }
  char t[256];
  mui_tf(t, sizeof t, "battle:learnMoveStopTeaching", "moveName", new_move_name(), NULL);
  show(t, false, stop_teaching_confirm);
}

static void on_summary_pick(void* ctx, int move_index) {
  (void)ctx;
  const menu_mon* m = &L.a.party[L.a.index];
  summary_close();
  if (move_index == 4 || move_index >= m->nmoves) {
    reject();
    return;
  }
  char forgot[256], full[640];
  mui_tf(forgot, sizeof forgot, "battle:learnMoveForgetSuccess", "pokemonName", L.a.pokemon_name, "moveName",
         menu_move_name(m->moves[move_index].id), NULL);
  snprintf(full, sizeof full, "%s$%s$%s", mui_t("battle:countdownPoof"), forgot, mui_t("battle:learnMoveAnd"));
  learn(move_index, full);
}

static void open_summary(void* ctx) {
  (void)ctx;
  summary_args s;
  memset(&s, 0, sizeof s);
  s.mode = SUMMARY_LEARN_MOVE;
  s.party = L.a.party;
  s.n = L.a.n;
  s.index = L.a.index;
  s.new_move = L.a.move;
  s.new_move_type = -1;
  s.trainer_name = L.a.trainer_name;
  s.trainer_id = L.a.trainer_id;
  s.player_female = L.a.player_female;
  summary_events ev = {NULL, NULL, on_summary_pick, NULL};
  summary_open(&s, &ev);
}

/* forgetMoveProcess */
static void forget_process(void* ctx) {
  (void)ctx;
  show(mui_t("battle:learnMoveForgetQuestion"), true, open_summary);
}

static void reject_cb(void* ctx) {
  (void)ctx;
  reject();
}

static void replace_confirm(void* ctx) {
  (void)ctx;
  L.h.confirm(L.h.host, forget_process, reject_cb, NULL);
}

static void replace_question(void* ctx) {
  (void)ctx;
  char t[256];
  mui_tf(t, sizeof t, "battle:learnMoveReplaceQuestion", "moveName", new_move_name(), NULL);
  show(t, false, replace_confirm);
}

/* replaceMoveCheck */
static void replace_check(void) {
  char a[256], b[256], t[520];
  mui_tf(a, sizeof a, "battle:learnMovePrompt", "pokemonName", L.a.pokemon_name, "moveName", new_move_name(), NULL);
  mui_tf(b, sizeof b, "battle:learnMoveLimitReached", "pokemonName", L.a.pokemon_name, NULL);
  snprintf(t, sizeof t, "%s$%s", a, b);
  show(t, true, replace_question);
}

void learn_move_start(const learn_move_args* args, const learn_move_host* host) {
  L.a = *args;
  L.h = *host;
  const menu_mon* m = &args->party[args->index];
  if (args->move >= PR_MOVE_COUNT || pr_moves[args->move].unimplemented) {
    end(NULL);
    return;
  }
  for (int i = 0; i < m->nmoves; i++)
    if (m->moves[i].id == args->move && args->move != 166 /* Sketch */) {
      end(NULL);
      return;
    }
  if (m->nmoves < 4)
    learn(m->nmoves, NULL);
  else
    replace_check();
}
