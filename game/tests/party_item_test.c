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
float ui_game_speed(void){return 3;}
void mui_tick(float dt) { (void)dt; }
void mui_init(void) {}
void rr_rect(float x, float y, float w, float h, rr_color c) { (void)x; (void)y; (void)w; (void)h; (void)c; }
const char* i18n_species_name(int id) { (void)id; return "Pokemon"; }
const char* i18n_t(char* out, int n, const char* key, ...) { snprintf(out, n, "%s", key); return out; }
void bv_set_info(const char* name, const char* desc) { (void)name; (void)desc; }
static menu_mon summary_mon;static int summary_calls;
void summary_open(const summary_args* a, const summary_events* e) { (void)e;summary_mon=a->party[a->index];summary_calls++; }
void summary_close(void) {}
bool summary_active(void) { return false; }
bool summary_input(mui_button b) { (void)b; return false; }
bool summary_touch(float x, float y) { (void)x; (void)y; return false; }
static float summary_ms;
void summary_update(float dt) { summary_ms=dt; }
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
static int form_party=-1,form_index=-1,check_back;
static void form_pick(int party,int item,void* arg) {
  (void)arg;form_party=party;form_index=item;ps_close();assert(depth==1);
}
static void check_done(void* arg) {(void)arg;check_back++;ps_close();}
static void check_team_cases(void) {
  memset(&run,0,sizeof run);run.n_party=3;
  for(int k=0;k<3;k++) {
    run.party[k].b.species=SPECIES_ID_VENUSAUR;run.party[k].b.id=100+k;
    run.party[k].b.hp=k?10:0;run.party[k].b.stats[0]=10;run.party[k].b.level=5;
  }
  run.party_order[0]=2;run.party_order[1]=1;run.party_order[2]=0;
  run.screen.kind=GC_SCREEN_REWARD;
  /* Both factories use the same display name, even though the catalogue lists
   * this stone under rare only. The sorted 24-item summary must not cap actions. */
  for(int count=30;count<=PR_MAX_MODIFIERS;count=PR_MAX_MODIFIERS) {
    run.n_items=count;
    for(int k=0;k<count;k++)run.items[k]=(gc_item){.key=k%2?"FORM_CHANGE_ITEM":"RARE_FORM_CHANGE_ITEM",.arg=FORM_CHANGE_ITEM_VENUSAURITE,.stack=1,.pokemon_id=100,.form_inactive=true,.non_transferable=true};
    form_party=form_index=-1;
    ps_open_check(&run,0,form_pick,NULL,check_done,NULL);
    input(BTN_ACTION);pf_clear_text();ui_top()->draw(ui_top(),RR_BOTTOM);
    assert(pf_text_count("partyUiHandler:activate Venusaurite")>0);
    /* Up wraps to Cancel on the final page; the previous row is the last item.
     * Its option ordinal passes the MOVE and ALL code ranges at full capacity. */
    input(BTN_UP);input(BTN_UP);input(BTN_ACTION);
    assert(form_party==0&&form_index==count-1&&depth==1);
    if(count==PR_MAX_MODIFIERS)break;
  }
  run.party[0].b.level=65536;run.party[0].met_level=65536;
  run.party[0].b.base_ability=ABILITY_ID_OVERGROW;run.party[0].b.ability=ABILITY_ID_LEVITATE;
  run.party[0].b.shiny=true;run.party[0].b.variant=2;
  ps_open_check(&run,0,form_pick,NULL,check_done,NULL);
  input(BTN_ACTION);input(BTN_ACTION); /* Summary uses raw base ability, per pinned source. */
  assert(summary_mon.level==65536&&summary_mon.met_level==65536&&summary_mon.ability==ABILITY_ID_OVERGROW&&summary_mon.variant==2);
  ps_close();
  /* The same menu outside rewards is read-only; no form action is exposed. */
  run.screen.kind=GC_SCREEN_COMMAND;form_party=-1;
  ps_open_check(&run,0,form_pick,NULL,check_done,NULL);
  input(BTN_ACTION);pf_clear_text();ui_top()->draw(ui_top(),RR_BOTTOM);
  assert(!pf_text_count("partyUiHandler:activate Venusaurite"));
  input(BTN_CANCEL);input(BTN_CANCEL);assert(check_back==1&&form_party==-1&&depth==1);
}
/* The reward screens list the party in upstream's party order (a lead change in battle moves a member to the front); the callbacks
 * still speak party[] indexes, which is what game-core's choices mean. */
static int moved_src = -1, moved_count, moved_to = -2, moves;
static void on_move(int item_src, int count, int to, void* arg) {
  (void)arg;
  moved_src = item_src;
  moved_count = count;
  moved_to = to;
  moves++;
}
static void transfer_done(void* arg) { (void)arg; ps_close(); }

static void open_item(int mode, int tm_move) {
  picked = -1;
  ps_open_item(&run, mode, false, tm_move, on_pick, NULL);
  assert(depth == 2 && party_active());
}

static void order_cases(void) {
  memset(&run, 0, sizeof run);
  run.n_party = 3;
  for (int i = 0; i < 3; i++) {
    pr_pokemon* p = &run.party[i].b;
    p->id = 500 + i;
    p->species = i == 1 ? SPECIES_ID_MAGIKARP : SPECIES_ID_CHARIZARD;
    p->level = 10 + i;
    p->hp = p->stats[0] = 100;
    p->n_moves = 1;
    p->moves[0] = i == 2 ? MOVE_ID_FLAMETHROWER : MOVE_ID_TACKLE;
    for (int j = 0; j < 4; j++) p->max_pp_override[j] = -1;
  }
  /* party[2] led the last battle, then party[0], then party[1]: the screen starts with party[2] */
  run.party_order[0] = 2;
  run.party_order[1] = 0;
  run.party_order[2] = 1;
  ps_set_run(&run);

  /* an item on the slot the player sees: the first slot is party[2], the second party[0] */
  open_item(PARTY_MODIFIER, 0);
  input(BTN_ACTION); input(BTN_DOWN); input(BTN_ACTION); /* Summary */
  assert(summary_calls > 0 && summary_mon.level == 12);
  ps_close();
  open_item(PARTY_MODIFIER, 0);
  input(BTN_ACTION); input(BTN_ACTION); /* Apply */
  assert(picked == 2 && depth == 1);
  open_item(PARTY_MODIFIER, 0);
  input(BTN_DOWN); input(BTN_ACTION); input(BTN_ACTION);
  assert(picked == 0 && depth == 1);
  open_item(PARTY_MODIFIER, 0);
  input(BTN_DOWN); input(BTN_DOWN); input(BTN_ACTION); input(BTN_ACTION);
  assert(picked == 1 && depth == 1);

  /* a TM: party[2] already knows Flamethrower (first slot), party[1] is a Magikarp (last slot); only the second slot, party[0], may learn it */
  open_item(PARTY_TM_MODIFIER, MOVE_ID_FLAMETHROWER);
  input(BTN_ACTION); input(BTN_ACTION);
  assert(picked == -1 && depth == 2);
  input(BTN_DOWN); input(BTN_ACTION); input(BTN_ACTION);
  assert(picked == 0 && depth == 1);
  open_item(PARTY_TM_MODIFIER, MOVE_ID_FLAMETHROWER);
  input(BTN_DOWN); input(BTN_DOWN); input(BTN_ACTION); input(BTN_ACTION);
  assert(picked == -1 && depth == 2);
  input(BTN_CANCEL);
  assert(picked == 6 && depth == 1);

  /* Manage Items: party[0] holds the item and is the second slot; party[2] is the first slot */
  run.n_items = 1;
  run.items[0] = (gc_item){.key = "LEFTOVERS", .stack = 1, .pokemon_id = 500};
  ps_open_transfer(&run, on_move, transfer_done, NULL);
  input(BTN_DOWN); input(BTN_DOWN); /* the cursor goes through the Transfer / Discard button to the second slot */
  input(BTN_ACTION); input(BTN_ACTION); /* its item, then who gets it */
  input(BTN_UP); input(BTN_UP); input(BTN_ACTION); input(BTN_ACTION); /* the first slot: Transfer */
  assert(moves == 1 && moved_src == 0 && moved_count == 1 && moved_to == 2);
  ps_close();

  /* a party_order that is not a permutation shows storage order */
  run.party_order[0] = run.party_order[1] = run.party_order[2] = 0;
  open_item(PARTY_MODIFIER, 0);
  input(BTN_DOWN); input(BTN_ACTION); input(BTN_ACTION);
  assert(picked == 1 && depth == 1);
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
  run.party[0].pokerus=true;ps_set_run(&run);
  open();input(BTN_ACTION);input(BTN_DOWN);input(BTN_ACTION);
  assert(summary_calls==1&&summary_mon.pokerus);ps_close();
  run.bt.n_party[0]=3;for(int i=0;i<3;i++)run.bt.party[0][i]=&run.party[i].b;
  ps_open(&run.bt,PS_SWITCH,on_pick,NULL);input(BTN_ACTION);input(BTN_ACTION);
  assert(summary_calls==2&&summary_mon.pokerus);ps_close();
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
  run.party[0].b.pp_up[0]=2;run.party[0].b.pp_used[0]=4;
  /* A fresh opening must clear the old TM filter/labels for other item types. */
  ps_open_item(&run, PARTY_MOVE_MODIFIER, true, 0, on_pick, NULL);
  pf_clear_text(); s = ui_top(); s->draw(s, RR_BOTTOM);
  assert(!pf_text_count("partyUiHandler:able"));
  s->update(s,6000);assert(summary_ms==2000); /* summary reading clock ignores 3x game speed */
  input(BTN_ACTION);pf_clear_text();s->draw(s,RR_BOTTOM);
  assert(pf_text_count("Tackle 45/49")==1);
  input(BTN_ACTION);
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
  check_team_cases();
  order_cases();
  puts("party_item_test: TM labels, Teach/filter, B/touch cancellation and move-item reset passed");
  return 0;
}
