/* The party and summary screens over the battle (see party_screen.h). */
#include "party_screen.h"
#include "battle_view.h"

#include <stdio.h>
#include <string.h>

#include "gm_item_ui.h"
#include "i18n.h"
#include "menus.h"
#include "pr_data.h"
#include "pr_enums.h"
#include "ui.h"

static const gc_run* g_run;
static char g_trainer_id[12];

void ps_set_run(const void* gc_run_ptr) {
  g_run = (const gc_run*)gc_run_ptr;
  snprintf(g_trainer_id, sizeof g_trainer_id, "%d", g_run ? g_run->trainer_id : 0);
}

/* gameData.trainerId and the Pokemon's metBiome / metLevel / metWave: what the summary's profile page shows */
static void fill_met(menu_mon* m, const gc_mon* g) {
  m->met_biome = g->met_biome;
  m->met_level = g->met_level > 0 ? (uint16_t)g->met_level : m->level;
  m->met_wave = g->met_wave > 0 ? (uint16_t)g->met_wave : 0;
}

/* The run's items the Pokemon holds (PokemonHeldItemModifier.pokemonId == its id): the summary's held-items row. Upstream
 * sorts them with modifierSortFunc (class name, then item name); with no class names here they go by English name. */
static void fill_items(menu_mon* m, uint32_t id) {
  m->nitems = 0;
  if (!g_run || !id) return;
  const gc_mon* holder = NULL;
  for (int i = 0; i < g_run->n_party && i < PR_MAX_PARTY; i++)
    if (g_run->party[i].b.id == id) holder = &g_run->party[i];
  for (int i = 0; i < g_run->n_items && i < GC_MAX_ITEMS && m->nitems < MENU_MAX_ITEMS; i++) {
    const gc_item* it = &g_run->items[i];
    if (it->detached || it->pokemon_id != id || !it->key) continue;
    const gm_item_ui* u = gm_item_ui_find(it->key, it->arg);
    if (!u) u = gm_item_ui_find(it->key, -1);
    menu_held_item* h = &m->items[m->nitems];
    memset(h, 0, sizeof *h);
    snprintf(h->name, sizeof h->name, "%s", u && u->name ? u->name : it->key);
    h->icon = u ? u->icon : NULL;
    h->count = (uint16_t)(it->stack > 0 ? it->stack : 0);
    h->transferable = !it->non_transferable;
    h->src = (int16_t)i;
    h->desc = u && u->desc && !u->dynamic ? u->desc : NULL;
    if (holder) {
      const int max = gc_item_max_held(g_run, it->key, it->arg, holder);
      h->max = (uint16_t)(max > 0 ? max : 0);
    }
    int k = m->nitems++;
    while (k > 0 && strcmp(m->items[k - 1].name, m->items[k].name) > 0) {
      const menu_held_item t = m->items[k - 1];
      m->items[k - 1] = m->items[k];
      m->items[k] = t;
      k--;
    }
  }
}

/* The run's own entry for a battle-core Pokemon (the party members of a run are `run->party[i].b`) */
static const gc_mon* run_mon_of(const pr_pokemon* p) {
  if (!g_run) return NULL;
  for (int i = 0; i < g_run->n_party && i < PR_MAX_PARTY; i++)
    if (&g_run->party[i].b == p) return &g_run->party[i];
  return NULL;
}

static struct {
  menu_mon party[PR_MAX_PARTY];
  int n;
  party_mode mode;
  void (*on_pick)(int, int, void*);
  void* arg;
  bool open;
} S;

/* Converts gc_mon to menu_mon for item menus */
static void gc_to_menu_mon(const gc_mon* gm, menu_mon* m) {
  memset(m, 0, sizeof *m);
  const pr_pokemon* p = &gm->b;
  m->species = p->species;
  m->form = p->form;
  m->gender = p->gender;
  m->shiny = p->shiny;
  snprintf(m->name, sizeof m->name, "%s", i18n_species_name(p->species));
  m->level = p->level;
  m->hp = p->hp > 0 ? (uint32_t)p->hp : 0;
  m->max_hp = (uint32_t)p->stats[STAT_HP];
  m->status = p->status;
  for (int i = 0; i < p->n_moves && i < 4; i++) {
    int max = p->max_pp_override[i] >= 0 ? p->max_pp_override[i] : pr_moves[p->moves[i]].pp;
    m->moves[i] = (menu_move){p->moves[i], p->pp_used[i], (uint8_t)max, false, 0};
  }
  m->nmoves = p->n_moves;
  m->allowed_in_battle = true;
  int8_t types[PR_MAX_TYPES];
  int nt = pr_get_types(p, types, 0);
  m->types[0] = nt > 0 ? types[0] : 0;
  m->types[1] = nt > 1 ? types[1] : -1;
  m->ability = p->ability;
  m->passive = p->passive;
  m->has_passive = p->passive_flag;
  m->nature = p->nature;
  for (int i = 0; i < 6; i++) {
    m->stats[i] = (uint32_t)p->stats[i];
    m->ivs[i] = p->ivs[i];
  }
  m->friendship = p->friendship;
  m->tera_type = p->tera_type;
  m->met_biome = -1;
  m->met_level = p->level;
  fill_met(m, gm);
  fill_items(m, p->id);
}

/* battle-core's Pokemon as the menus show it */
static void ps_menu_mon(const pr_battle* bt, const pr_pokemon* p, menu_mon* m) {
  (void)bt;
  memset(m, 0, sizeof *m);
  m->species = p->species;
  m->form = p->form;
  m->gender = p->gender;
  m->shiny = p->shiny;
  snprintf(m->name, sizeof m->name, "%s", i18n_species_name(p->species));
  m->level = p->level;
  m->hp = p->hp > 0 ? (uint32_t)p->hp : 0;
  m->max_hp = (uint32_t)p->stats[STAT_HP];
  m->status = p->status;
  for (int i = 0; i < p->n_moves && i < 4; i++) {
    int max = p->max_pp_override[i] >= 0 ? p->max_pp_override[i] : pr_moves[p->moves[i]].pp;
    m->moves[i] = (menu_move){p->moves[i], p->pp_used[i], (uint8_t)max, false, 0};
  }
  m->nmoves = p->n_moves;
  m->allowed_in_battle = true;
  int8_t types[PR_MAX_TYPES];
  int nt = pr_get_types(p, types, 0);
  m->types[0] = nt > 0 ? types[0] : 0;
  m->types[1] = nt > 1 ? types[1] : -1;
  m->ability = p->ability;
  m->passive = p->passive;
  m->has_passive = p->passive_flag;
  m->nature = p->nature;
  for (int i = 0; i < 6; i++) {
    m->stats[i] = (uint32_t)p->stats[i];
    m->ivs[i] = p->ivs[i];
  }
  m->friendship = p->friendship;
  m->tera_type = p->tera_type;
  m->met_biome = -1;
  m->met_level = p->level;
  const gc_mon* g = run_mon_of(p);
  if (g) fill_met(m, g);
  if (g) fill_items(m, p->id);
}

/* ---- callbacks ---- */

static const char* filter_non_fainted(void* ctx, int slot) {
  /* PartyUiHandler.FilterNonFainted */
  (void)ctx;
  static char buf[160];
  if (S.party[slot].hp > 0) return NULL;
  return i18n_t(buf, sizeof buf, "partyUiHandler:noEnergy", "pokemonName", S.party[slot].name, NULL);
}

static const char* filter_fainted(void* ctx, int slot) {
  (void)ctx;
  if(S.party[slot].hp<=0)return NULL;
  static char buf[160];
  return i18n_t(buf,sizeof buf,"partyUiHandler:hasEnergy","pokemonName",S.party[slot].name,NULL);
}

static void on_select(void* ctx, int slot, int option) {
  (void)ctx;
  if (option == PO_CANCEL && (S.mode == PARTY_FAINT_SWITCH || S.mode == PARTY_REVIVAL_BLESSING)) return;
  if (S.on_pick) S.on_pick(slot, option, S.arg);
}

static void sum_close(void* ctx) {
  (void)ctx;
  party_resume();
}
static void sum_swap(void* ctx, int slot, int a, int b) {
  /* the moveset order is battle-core's; the summary shows the swap until it closes */
  (void)ctx, (void)slot, (void)a, (void)b;
}

/* the held item's description goes to the battle screen's message window, empty under the summary */
static void sum_item_info(void* ctx, const char* name, const char* desc) {
  (void)ctx;
  bv_set_info(name, desc);
}

static void on_summary(void* ctx, int slot) {
  (void)ctx;
  summary_args a;
  memset(&a, 0, sizeof a);
  a.mode = SUMMARY_DEFAULT;
  a.page = SUMMARY_PROFILE;
  a.party = S.party;
  a.n = S.n;
  a.index = slot;
  a.from_party = true;
  a.new_move_type = -1;
  a.trainer_id = g_run ? g_trainer_id : NULL;
  static const summary_events ev = {sum_close, sum_swap, NULL, NULL, sum_item_info};
  summary_open(&a, &ev);
}

/* ---- the ui screen ---- */

static bool ps_input(ui_screen* s, ui_button b) {
  (void)s;
  mui_button mb = (mui_button)b; /* both are upstream's Button enum, in order */
  if (summary_active()) return summary_input(mb);
  return party_input(mb);
}

static void ps_update(ui_screen* s, float dt) {
  (void)s;
  mui_tick(dt);
  party_update(dt);
  summary_update(dt);
}

static void ps_draw(ui_screen* s, rr_screen which) {
  (void)s;
  if (which != RR_BOTTOM) return;
  if (ui_space(UI_PHYS_BOTTOM)) rr_rect(0, 0, 320, 240, RR_RGBA(0, 0, 0, 255));
  mui_set_origin(RR_BOTTOM, 0, 30); /* UI_BOTTOM_CANVAS */
  if (summary_active())
    summary_draw();
  else
    party_draw();
}

static bool ps_touch(ui_screen* s, float x, float y) {
  (void)s;
  /* UI_BOTTOM_CANVAS: the canvas starts 30 px down */
  if (summary_active()) return summary_touch(x, y - 30);
  return party_touch(x, y - 30);
}

static ui_screen g_screen = {"party", UI_OVERLAY, NULL, NULL, NULL, ps_input, ps_update, ps_draw, NULL, ps_touch};

void ps_init(void) { mui_init(); }

void ps_open(const pr_battle* bt, ps_mode pmode, void (*on_pick)(int, int, void*), void* arg) {
  _Static_assert((int)PS_SWITCH == (int)PARTY_SWITCH && (int)PS_FAINT_SWITCH == (int)PARTY_FAINT_SWITCH &&
                     (int)PS_RELEASE == (int)PARTY_RELEASE && (int)PS_RELEASE_PICK == (int)PO_RELEASE &&
                     (int)PS_SEND_OUT == (int)PO_SEND_OUT && (int)PS_CANCEL == (int)PO_CANCEL &&
                     (int)PS_REVIVAL_BLESSING == (int)PARTY_REVIVAL_BLESSING && (int)PS_REVIVE_PICK == (int)PO_REVIVE,
                 "party_screen.h values");
  party_mode mode = (party_mode)pmode;
  memset(&S, 0, sizeof S);
  S.mode = mode;
  S.on_pick = on_pick;
  S.arg = arg;
  for (int i = 0; i < bt->n_party[0] && i < PR_MAX_PARTY; i++) ps_menu_mon(bt, bt->party[0][i], &S.party[i]);
  S.n = bt->n_party[0];
  party_args a;
  memset(&a, 0, sizeof a);
  a.mode = mode;
  a.battler_count = 1;
  a.max_level = bt->max_exp_level;
  a.filter = mode == PARTY_REVIVAL_BLESSING ? filter_fainted : filter_non_fainted;
  static party_events ev;
  ev = (party_events){on_select, NULL, NULL, NULL, NULL, on_summary, NULL};
  party_open(&a, S.party, S.n, &ev);
  S.open = true;
  ui_push(&g_screen);
}

void ps_close(void) {
  if (!S.open) return;
  S.open = false;
  summary_close();
  party_close();
  if (ui_top() == &g_screen) ui_pop();
}

static struct {
  void (*on_item_pick)(int slot, int move_slot, void* arg);
  void* item_arg;
  bool target_move;
} ItemS;

static void on_item_select(void* ctx, int slot, int option) {
  (void)ctx;
  if (option == PO_CANCEL) {
    if (ItemS.on_item_pick) ItemS.on_item_pick(6, -1, ItemS.item_arg);
    return;
  }
  if (option >= PO_MOVE_1 && option <= PO_MOVE_4) {
    int move_idx = option - PO_MOVE_1;
    if (ItemS.on_item_pick) ItemS.on_item_pick(slot, move_idx, ItemS.item_arg);
    return;
  }
  if (ItemS.on_item_pick) ItemS.on_item_pick(slot, -1, ItemS.item_arg);
}

void ps_open_item(const void* gc_run_ptr, int pmode, bool target_move, void (*on_pick)(int slot, int move_slot, void* arg), void* arg) {
  const gc_run* run = (const gc_run*)gc_run_ptr;
  ps_set_run(run);
  memset(&S, 0, sizeof S);
  memset(&ItemS, 0, sizeof ItemS);
  S.mode = (party_mode)pmode;
  ItemS.on_item_pick = on_pick;
  ItemS.item_arg = arg;
  ItemS.target_move = target_move;

  if (run) {
    for (int i = 0; i < run->n_party && i < PR_MAX_PARTY; i++) {
      gc_to_menu_mon(&run->party[i], &S.party[i]);
    }
    S.n = run->n_party;
  }

  party_args a;
  memset(&a, 0, sizeof a);
  a.mode = S.mode;
  a.battler_count = 1;
  a.in_reward_phase = true;
  a.show_move_pp = target_move;

  static party_events ev;
  ev = (party_events){on_item_select, NULL, NULL, NULL, NULL, on_summary, NULL};
  party_open(&a, S.party, S.n, &ev);
  S.open = true;
  ui_push(&g_screen);
}

/* ---- Manage Items: the reward screen's transfer (SelectModifierPhase.openModifierTransferScreen) ---- */

static struct {
  void (*on_move)(int item_src, int count, int to, void* arg);
  void (*on_done)(void* arg);
  void* arg;
} TrS;

static void transfer_fill(void) {
  for (int i = 0; i < g_run->n_party && i < PR_MAX_PARTY; i++) gc_to_menu_mon(&g_run->party[i], &S.party[i]);
  S.n = g_run->n_party;
}

/* `item` counts the transferable items of `from` in the order the menu lists them; `to` -1 = discard them */
static void on_transfer_pick(void* ctx, int from, int item, int count, int to) {
  (void)ctx;
  if (!g_run || from < 0 || from >= S.n || to < -1 || to >= S.n) return;
  const menu_mon* m = &S.party[from];
  int src = -1;
  for (int i = 0, k = 0; i < m->nitems; i++)
    if (m->items[i].transferable && k++ == item) src = m->items[i].src;
  if (src < 0) return;
  if (TrS.on_move) TrS.on_move(src, count, to, TrS.arg);
  /* the run's list changed (and with it each item's index): the menu is rebuilt from it */
  transfer_fill();
  party_refresh(S.party, S.n);
}

static void on_discard_pick(void* ctx, int slot, int item, int count) { on_transfer_pick(ctx, slot, item, count, -1); }

static void on_transfer_select(void* ctx, int slot, int option) {
  (void)ctx, (void)slot;
  if (option == PO_CANCEL && TrS.on_done) TrS.on_done(TrS.arg);
}

void ps_open_transfer(const void* gc_run_ptr, void (*on_move)(int item_src, int count, int to, void* arg), void (*on_done)(void* arg),
                      void* arg) {
  const gc_run* run = (const gc_run*)gc_run_ptr;
  if (!run) return;
  ps_set_run(run);
  memset(&S, 0, sizeof S);
  TrS.on_move = on_move;
  TrS.on_done = on_done;
  TrS.arg = arg;
  S.mode = PARTY_MODIFIER_TRANSFER;
  transfer_fill();
  party_args a;
  memset(&a, 0, sizeof a);
  a.mode = PARTY_MODIFIER_TRANSFER;
  a.battler_count = 1;
  a.in_reward_phase = true;
  static party_events ev;
  ev = (party_events){on_transfer_select, on_transfer_pick, on_discard_pick, NULL, NULL, on_summary, NULL};
  party_open(&a, S.party, S.n, &ev);
  S.open = true;
  ui_push(&g_screen);
}

/* ---- the learn-move dialog ---- */

static struct {
  menu_mon party[PR_MAX_PARTY];
  int n;
  char name[48], trainer_id[12];
  ps_learn_host h;
  void (*done)(void*);
  void* done_ctx;
  void (*yes)(void*);
  void (*no)(void*);
  void* cb_ctx;
} L;

/* The summary page over the battle while it is open (menus' summary_open is called by the dialog itself) */
static bool sum_input(ui_screen* s, ui_button b) {
  (void)s;
  return summary_active() && summary_input((mui_button)b);
}

static void sum_update(ui_screen* s, float dt) {
  (void)s;
  mui_tick(dt);
  summary_update(dt);
}

static void sum_draw(ui_screen* s, rr_screen which) {
  (void)s;
  if (which != RR_BOTTOM || !summary_active()) return;
  if (ui_space(UI_PHYS_BOTTOM)) rr_rect(0, 0, 320, 240, RR_RGBA(0, 0, 0, 255));
  mui_set_origin(RR_BOTTOM, 0, 30); /* UI_BOTTOM_CANVAS */
  summary_draw();
}

static bool sum_touch(ui_screen* s, float x, float y) {
  (void)s;
  return summary_active() && summary_touch(x, y - 30); /* UI_BOTTOM_CANVAS: the canvas starts 30 px down */
}

static ui_screen g_sum_screen = {"learn-summary", UI_OVERLAY, NULL, NULL, NULL, sum_input, sum_update, sum_draw, NULL, sum_touch};

/* the summary screen is on top exactly while it is open */
static void lm_sync(void) {
  if (summary_active()) {
    if (ui_top() != &g_sum_screen) ui_push(&g_sum_screen);
  } else if (ui_top() == &g_sum_screen) {
    ui_pop();
  }
}

static void lm_text_done(void* ctx) {
  (void)ctx;
  void (*cb)(void*) = L.done;
  void* c = L.done_ctx;
  L.done = NULL;
  if (cb) cb(c);
  lm_sync();
}

static void lm_show(void* host, const char* text, bool prompt, void (*done)(void*), void* done_ctx) {
  (void)host;
  lm_sync();
  L.done = done;
  L.done_ctx = done_ctx;
  L.h.show_text(L.h.host, text, prompt, lm_text_done, NULL);
}

static void lm_yes(void* ctx) {
  (void)ctx;
  if (L.yes) L.yes(L.cb_ctx);
  lm_sync();
}

static void lm_no(void* ctx) {
  (void)ctx;
  if (L.no) L.no(L.cb_ctx);
  lm_sync();
}

static void lm_confirm(void* host, menu_cb yes, menu_cb no, void* ctx) {
  (void)host;
  lm_sync();
  L.yes = yes;
  L.no = no;
  L.cb_ctx = ctx;
  L.h.confirm(L.h.host, lm_yes, lm_no, NULL);
}

static void lm_result(void* host, int slot) {
  (void)host;
  lm_sync();
  L.h.on_result(L.h.host, slot);
}

static void lm_end(void* host) {
  (void)host;
  lm_sync();
  L.h.on_end(L.h.host);
}

void ps_learn_move(const pr_battle* bt, int party_slot, int move, int trainer_id, const ps_learn_host* host) {
  memset(&L, 0, sizeof L);
  L.h = *host;
  for (int i = 0; i < bt->n_party[0] && i < PR_MAX_PARTY; i++) ps_menu_mon(bt, bt->party[0][i], &L.party[i]);
  L.n = bt->n_party[0];
  snprintf(L.name, sizeof L.name, "%s", L.party[party_slot].name);
  snprintf(L.trainer_id, sizeof L.trainer_id, "%d", trainer_id); /* gameData.trainerId.toString(): not padded */
  learn_move_args a;
  memset(&a, 0, sizeof a);
  a.party = L.party;
  a.n = L.n;
  a.index = party_slot;
  a.move = (uint16_t)move;
  a.pokemon_name = L.name;
  a.level_move_confirmation = true;
  a.trainer_id = L.trainer_id;
  learn_move_host h = {lm_show, lm_confirm, lm_result, lm_end, NULL};
  learn_move_start(&a, &h);
}
