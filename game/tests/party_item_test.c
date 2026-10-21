/* Real party_screen.c -> party.c, with rendering and unrelated summary dialog mocked.
 * The caller closes the party and restores its shop just as run_screen.c does. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "gc/run.h"
#include "menus.h"
#include "party_screen.h"
#include "party_fake.h"
#include "ui.h"

static ui_screen shop = {.name = "shop"};
static ui_screen* stack[4] = {&shop};
static int depth = 1, picked = -1, move_slot, calls;
void ui_push(ui_screen* s) { assert(depth < 4); stack[depth++] = s; }
void ui_pop(void) { assert(depth > 1); depth--; }
ui_screen* ui_top(void) { return stack[depth - 1]; }
bool ui_space(ui_space_id s) { (void)s; return true; }
void mui_set_origin(rr_screen s, int x, int y) { (void)s; (void)x; (void)y; }
void mui_tick(float dt) { (void)dt; }
void mui_init(void) {}
void rr_rect(float x, float y, float w, float h, rr_color c) { (void)x; (void)y; (void)w; (void)h; (void)c; }
const char* i18n_species_name(int id) { (void)id; return "Pokemon"; }
const char* i18n_t(char* out, int n, const char* key, ...) { snprintf(out, n, "%s", key); return out; }
void bv_set_info(const char* name, const char* desc) { (void)name; (void)desc; }
void summary_open(const summary_args* a, const summary_events* e) { (void)a; (void)e; assert(0); }
void summary_close(void) {}
bool summary_active(void) { return false; }
bool summary_input(mui_button b) { (void)b; return false; }
bool summary_touch(float x, float y) { (void)x; (void)y; return false; }
void summary_update(float dt) { (void)dt; }
void summary_draw(void) {}
void learn_move_start(const learn_move_args* a, const learn_move_host* h) { (void)a; (void)h; assert(0); }

static void on_pick(int slot, int move, void* arg) {
  (void)arg;
  calls++;
  picked = slot;
  move_slot = move;
  ps_close();
  assert(ui_top() == &shop);
}
static void input(ui_button b) { ui_screen* s = ui_top(); assert(s != &shop); assert(s->input(s, b)); }
static gc_run run;
static void open(void) {
  picked = -1;
  ps_open_item(&run, PARTY_TM_MODIFIER, false, MOVE_ID_FLAMETHROWER, on_pick, NULL);
  assert(depth == 2 && party_active());
}
int main(void) {
  run.n_party = 3;
  for (int i = 0; i < 3; i++) {
    pr_pokemon* p = &run.party[i].b;
    p->species = i == 1 ? SPECIES_ID_MAGIKARP : SPECIES_ID_CHARIZARD;
    p->level = 50;
    p->hp = p->stats[0] = 100;
    p->n_moves = 1;
    p->moves[0] = i == 2 ? MOVE_ID_FLAMETHROWER : MOVE_ID_TACKLE;
    for (int j = 0; j < 4; j++) p->max_pp_override[j] = -1;
  }
  open();
  pf_clear_text();
  ui_screen* s = ui_top();
  s->draw(s, RR_BOTTOM);
  assert(pf_text_count("partyUiHandler:able") == 1);
  assert(pf_text_count("partyUiHandler:notAble") == 1);
  assert(pf_text_count("partyUiHandler:learned") == 1);
  input(BTN_ACTION); /* options: Teach, Summary, Cancel */
  pf_clear_text(); s->draw(s, RR_BOTTOM);
  assert(pf_text_count("partyUiHandler:teach") == 1);
  input(BTN_CANCEL); /* back out of options, then of the party */
  assert(picked == -1 && depth == 2);
  input(BTN_CANCEL);
  assert(picked == 6 && move_slot == -1 && calls == 1 && !party_active());

  open();
  s = ui_top();
  assert(s->touch(s, 291, 194)); /* physical Cancel button (canvas y164 +30) */
  assert(picked == 6 && calls == 2 && depth == 1);

  open();
  input(BTN_RIGHT); /* incompatible Magikarp */
  input(BTN_ACTION); input(BTN_ACTION);
  assert(picked == -1 && calls == 2 && depth == 2);
  input(BTN_DOWN); /* already learned on Charizard */
  input(BTN_ACTION); input(BTN_ACTION);
  assert(picked == -1 && calls == 2 && depth == 2);
  input(BTN_CANCEL);
  assert(picked == 6 && calls == 3);

  open();
  input(BTN_ACTION); input(BTN_ACTION); /* eligible Charizard -> Teach */
  assert(picked == 0 && move_slot == -1 && calls == 4 && depth == 1);
  /* A fresh opening must clear the old TM filter/labels for other item types. */
  ps_open_item(&run, PARTY_MOVE_MODIFIER, true, 0, on_pick, NULL);
  pf_clear_text(); s = ui_top(); s->draw(s, RR_BOTTOM);
  assert(!pf_text_count("partyUiHandler:able"));
  input(BTN_ACTION); input(BTN_ACTION);
  assert(picked == 0 && move_slot == 0 && calls == 5 && depth == 1);
  /* A temporary known move can say Learned while the permanent TM filter permits it. */
  pr_pokemon* p = &run.party[0].b;
  p->summon_moves_active = true;
  p->transform_moves[0] = MOVE_ID_FLAMETHROWER;
  open(); pf_clear_text(); s = ui_top(); s->draw(s, RR_BOTTOM);
  assert(pf_text_count("partyUiHandler:learned") == 2);
  input(BTN_ACTION); input(BTN_ACTION);
  assert(picked == 0 && calls == 6 && depth == 1);
  /* Conversely, raw-known TM is refused even when the effective moves make it say Able. */
  p->moves[0] = MOVE_ID_FLAMETHROWER;
  p->transformed = true; p->transform_n_moves = 1;
  p->transform_moves[0] = MOVE_ID_SURF;
  open(); pf_clear_text(); s = ui_top(); s->draw(s, RR_BOTTOM);
  assert(pf_text_count("partyUiHandler:able") == 1);
  input(BTN_ACTION); input(BTN_ACTION);
  assert(picked == -1 && calls == 6 && depth == 2);
  input(BTN_CANCEL);
  assert(picked == 6 && calls == 7 && depth == 1);
  puts("party_item_test: TM labels, Teach/filter, B/touch cancellation and move-item reset passed");
  return 0;
}
