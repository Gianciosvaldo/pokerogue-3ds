/* The party and summary screens over the battle (see party_screen.h). */
#include "party_screen.h"
#include "gc/reward.h"
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
  m->met_level = g->met_level > 0 ? (uint32_t)g->met_level : m->level;
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

/* The reward-time screens list the party in upstream's party order (run->party_order, which switching in battle changes), not in
 * party[]'s storage order, which never moves. `order[slot]` is the index into party[] of the member shown in `slot`; the callbacks
 * these screens take and give use party[] indexes, so a slot becomes `order[slot]` on its way out. A party_order that is not a
 * permutation of the party falls back to storage order. */
static int order[PR_MAX_PARTY];

static void set_order(const gc_run* run) {
  const int n = run ? (run->n_party < PR_MAX_PARTY ? run->n_party : PR_MAX_PARTY) : 0;
  int used = 0;
  bool ordered = true;
  for (int slot = 0; slot < n; slot++) {
    const int storage = run->party_order[slot];
    if (storage < 0 || storage >= n || (used & (1 << storage))) {
      ordered = false;
      break;
    }
    used |= 1 << storage;
    order[slot] = storage;
  }
  if (!ordered)
    for (int slot = 0; slot < n; slot++) order[slot] = slot;
}

static int storage_at(int slot) { return slot >= 0 && slot < S.n && slot < PR_MAX_PARTY ? order[slot] : slot; }

/* Converts gc_mon to menu_mon for item menus */
static void gc_to_menu_mon(const gc_mon* gm, menu_mon* m) {
  memset(m, 0, sizeof *m);
  const pr_pokemon* p = &gm->b;
  m->species = p->species;
  m->form = p->form;
  m->gender = p->gender;
  m->shiny = p->shiny;
  m->variant = p->variant;
  snprintf(m->name, sizeof m->name, "%s", i18n_species_name(p->species));
  m->level = p->level;
  m->hp = p->hp > 0 ? (uint32_t)p->hp : 0;
  m->max_hp = (uint32_t)p->stats[STAT_HP];
  m->status = p->status;
  for (int i = 0; i < p->n_moves && i < 4; i++) {
    int base=pr_moves[p->moves[i]].pp;
    int max=p->max_pp_override[i]>0?p->max_pp_override[i]:base+p->pp_up[i]*(base/5>0?base/5:1);
    m->moves[i] = (menu_move){p->moves[i], p->pp_used[i], (uint8_t)max, false, 0};
  }
  m->nmoves = p->n_moves;
  m->allowed_in_battle = true;
  int8_t types[PR_MAX_TYPES];
  int nt = pr_get_types(p, types, 0);
  m->types[0] = nt > 0 ? types[0] : 0;
  m->types[1] = nt > 1 ? types[1] : -1;
  m->ability = p->base_ability;
  m->passive = p->passive;
  m->has_passive = p->passive_flag;
  m->nature = p->nature;
  for (int i = 0; i < 6; i++) {
    m->stats[i] = (uint32_t)p->stats[i];
    m->ivs[i] = p->ivs[i];
  }
  m->show_battle_stages = p->on_field;
  memcpy(m->stat_stages, p->stat_stages, sizeof m->stat_stages);
  m->friendship = p->friendship;
  m->tera_type = p->tera_type;
  m->met_biome = -1;
  m->met_level = p->level;
  if(gm)m->pokerus=gm->pokerus;
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
  m->variant = p->variant;
  snprintf(m->name, sizeof m->name, "%s", i18n_species_name(p->species));
  m->level = p->level;
  m->hp = p->hp > 0 ? (uint32_t)p->hp : 0;
  m->max_hp = (uint32_t)p->stats[STAT_HP];
  m->status = p->status;
  for (int i = 0; i < p->n_moves && i < 4; i++) {
    int base=pr_moves[p->moves[i]].pp;
    int max=p->max_pp_override[i]>0?p->max_pp_override[i]:base+p->pp_up[i]*(base/5>0?base/5:1);
    m->moves[i] = (menu_move){p->moves[i], p->pp_used[i], (uint8_t)max, false, 0};
  }
  m->nmoves = p->n_moves;
  m->allowed_in_battle = true;
  int8_t types[PR_MAX_TYPES];
  int nt = pr_get_types(p, types, 0);
  m->types[0] = nt > 0 ? types[0] : 0;
  m->types[1] = nt > 1 ? types[1] : -1;
  m->ability = p->base_ability;
  m->passive = p->passive;
  m->has_passive = p->passive_flag;
  m->nature = p->nature;
  for (int i = 0; i < 6; i++) {
    m->stats[i] = (uint32_t)p->stats[i];
    m->ivs[i] = p->ivs[i];
  }
  m->show_battle_stages = p->on_field;
  memcpy(m->stat_stages, p->stat_stages, sizeof m->stat_stages);
  m->friendship = p->friendship;
  m->tera_type = p->tera_type;
  m->met_biome = -1;
  m->met_level = p->level;
  const gc_mon* g = run_mon_of(p);
  if (g) {m->pokerus=g->pokerus;fill_met(m, g);}
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
  summary_update(dt / ui_game_speed());
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
  int tm_move;
} ItemS;

static void on_item_select(void* ctx, int slot, int option) {
  (void)ctx;
  if (option == PO_CANCEL) {
    if (ItemS.on_item_pick) ItemS.on_item_pick(6, -1, ItemS.item_arg);
    return;
  }
  if (S.mode == PARTY_REMEMBER_MOVE_MODIFIER && option >= 0 && option < S.party[slot].nrelearn) {
    if (ItemS.on_item_pick) ItemS.on_item_pick(storage_at(slot), option, ItemS.item_arg);
    return;
  }
  if (option >= PO_MOVE_1 && option <= PO_MOVE_4) {
    int move_idx = option - PO_MOVE_1;
    if (ItemS.on_item_pick) ItemS.on_item_pick(storage_at(slot), move_idx, ItemS.item_arg);
    return;
  }
  if (ItemS.on_item_pick) ItemS.on_item_pick(storage_at(slot), -1, ItemS.item_arg);
}

static const char* filter_tm(void* ctx, int slot) {
  (void)ctx;
  return gc_mon_can_learn_tm(&g_run->party[storage_at(slot)], ItemS.tm_move) ? NULL : mui_t("partyUiHandler:anyEffect");
}

static const char* filter_remember(void* ctx,int slot) {
  (void)ctx;return S.party[slot].nrelearn ? NULL : mui_t("partyUiHandler:anyEffect");
}
static const char* filter_tera_shard(void* ctx,int slot) {
  (void)ctx;return gc_mon_can_use_tera_shard(&g_run->party[storage_at(slot)]) ? NULL : mui_t("partyUiHandler:anyEffect");
}

static void open_item(const void* gc_run_ptr, int pmode, bool target_move, int tm_move, bool tera_shard, void (*on_pick)(int slot, int move_slot, void* arg), void* arg) {
  const gc_run* run = (const gc_run*)gc_run_ptr;
  ps_set_run(run);
  memset(&S, 0, sizeof S);
  memset(&ItemS, 0, sizeof ItemS);
  S.mode = (party_mode)pmode;
  ItemS.on_item_pick = on_pick;
  ItemS.item_arg = arg;
  ItemS.target_move = target_move;
  ItemS.tm_move = tm_move;

  if (run) {
    set_order(run);
    for (int slot = 0; slot < run->n_party && slot < PR_MAX_PARTY; slot++) {
      const gc_mon* m = &run->party[order[slot]];
      gc_to_menu_mon(m, &S.party[slot]);
      if (S.mode == PARTY_TM_MODIFIER) S.party[slot].tm_state = (uint8_t)gc_mon_tm_state(m, tm_move);
      if (S.mode == PARTY_REMEMBER_MOVE_MODIFIER) {
        static pr_reward_memory_move rows[GC_MEMORY_MOVE_CAPACITY];
        typedef char memory_capacity_fits[(GC_MEMORY_MOVE_CAPACITY <= sizeof S.party[slot].relearn / sizeof S.party[slot].relearn[0] && GC_MEMORY_MOVE_CAPACITY < PO_SCROLL_UP) ? 1 : -1];
        (void)sizeof(memory_capacity_fits);
        int n=gc_memory_moves((gc_run*)gc_run_ptr,order[slot],rows,GC_MEMORY_MOVE_CAPACITY);
        if (n > 0) {
          S.party[slot].nrelearn=(uint16_t)n;
          for (int i=0;i<n;i++) {S.party[slot].relearn[i]=rows[i].move;S.party[slot].relearn_level[i]=rows[i].level>0?(uint32_t)rows[i].level:0;}
        }
      }
    }
    S.n = run->n_party;
  }

  party_args a;
  memset(&a, 0, sizeof a);
  a.mode = S.mode;
  a.battler_count = 1;
  a.in_reward_phase = true;
  a.show_move_pp = target_move;
  if (S.mode == PARTY_TM_MODIFIER) a.filter = filter_tm;
  if (S.mode == PARTY_REMEMBER_MOVE_MODIFIER) a.filter = filter_remember;
  if (tera_shard) a.filter = filter_tera_shard;

  static party_events ev;
  ev = (party_events){on_item_select, NULL, NULL, NULL, NULL, on_summary, NULL};
  party_open(&a, S.party, S.n, &ev);
  S.open = true;
  ui_push(&g_screen);
}

void ps_open_item(const void* run,int mode,bool target_move,int tm_move,void (*pick)(int,int,void*),void* arg) {
  open_item(run,mode,target_move,tm_move,false,pick,arg);
}
void ps_open_reward(const void* run,const char* key,bool target_move,int item_arg,void (*pick)(int,int,void*),void* arg) {
  bool tm=!strcmp(key,"TM_COMMON")||!strcmp(key,"TM_GREAT")||!strcmp(key,"TM_ULTRA");
  int mode=!strcmp(key,"MEMORY_MUSHROOM")?PARTY_REMEMBER_MOVE_MODIFIER:tm?PARTY_TM_MODIFIER:target_move?PARTY_MOVE_MODIFIER:PARTY_MODIFIER;
  open_item(run,mode,target_move,item_arg,!strcmp(key,"TERA_SHARD"),pick,arg);
}

/* ---- Manage Items: the reward screen's transfer (SelectModifierPhase.openModifierTransferScreen) ---- */

static struct {
  void (*on_move)(int item_src, int count, int to, void* arg);
  void (*on_done)(void* arg);
  void* arg;
} TrS;

static void transfer_fill(void) {
  set_order(g_run);
  for (int slot = 0; slot < g_run->n_party && slot < PR_MAX_PARTY; slot++) gc_to_menu_mon(&g_run->party[order[slot]], &S.party[slot]);
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
  if (TrS.on_move) TrS.on_move(src, count, to < 0 ? -1 : storage_at(to), TrS.arg);
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

static struct {
  void (*on_form)(int, int, void*);
  void (*on_pause)(int, bool, void*);
  void (*on_done)(void*);
  void* arg;
} Check;
/* One item belongs to one member; total storage follows the full inventory,
 * independently of the summary's 24 displayed items. No large stack arrays. */
static menu_form_item form_items[PR_MAX_MODIFIERS];
static int form_indices[PR_MAX_MODIFIERS];
static void check_form(void* ctx, int slot, int item) {
  (void)ctx;
  if (Check.on_form && slot>=0 && slot<S.n) Check.on_form(order[slot],item,Check.arg);
}
static void check_pause(void* ctx, int slot, bool paused) {
  (void)ctx;
  if (Check.on_pause && slot>=0 && slot<S.n) Check.on_pause(order[slot],paused,Check.arg);
}
static void check_select(void* ctx, int slot, int option) {
  (void)ctx;(void)slot;
  if (option == PO_CANCEL && Check.on_done) Check.on_done(Check.arg);
}
void ps_open_check(const void* gc_run_ptr, int cursor, void (*on_form)(int,int,void*),
                   void (*on_pause)(int,bool,void*), void (*on_done)(void*), void* arg) {
  const gc_run* run=gc_run_ptr;
  if (!run || run->n_party<1 || run->n_party>PR_MAX_PARTY) return;
  _Static_assert(MENU_MAX_FORM_ITEMS >= PR_MAX_MODIFIERS,"full form inventory fits menu");
  ps_set_run(run);memset(&S,0,sizeof S);S.mode=PARTY_CHECK;S.n=run->n_party;
  Check.on_form=on_form;Check.on_pause=on_pause;Check.on_done=on_done;Check.arg=arg;
  set_order(run);
  for(int slot=0;slot<S.n;slot++)gc_to_menu_mon(&run->party[order[slot]],&S.party[slot]);
  int count=0,initial=0;
  for(int slot=0;slot<S.n;slot++) {
    int storage=order[slot];if(storage==cursor)initial=slot;
    const int n=gc_form_item_options(run,storage,form_indices,PR_MAX_MODIFIERS);
    if(n<0 || n>PR_MAX_MODIFIERS-count) {
      /* Invalid aliased party identities cannot expose a partial action list. */
      for(int k=0;k<S.n;k++)S.party[k].nform_items=0;
      break;
    }
    S.party[slot].form_items=form_items+count;S.party[slot].nform_items=n;
    for(int k=0;k<n;k++) {
      const int index=form_indices[k];const gc_item* item=&run->items[index];
      const gm_item_ui* label=gm_item_ui_find(item->key,item->arg);
      if(!label)label=gm_item_ui_find("FORM_CHANGE_ITEM",item->arg);
      if(!label)label=gm_item_ui_find("RARE_FORM_CHANGE_ITEM",item->arg);
      form_items[count++]=(menu_form_item){label?label->name:item->arg==0?"None":item->key,index,!item->form_inactive};
    }
  }
  party_args a={0};a.mode=PARTY_CHECK;a.initial_cursor=initial;
  a.battler_count=run->double_battle?2:1;a.double_battle=run->double_battle;
  a.max_level=run->bt.max_exp_level;a.in_reward_phase=run->screen.kind==GC_SCREEN_REWARD;
  a.preserve_roster=true;
  party_events ev={0};ev.on_select=check_select;ev.on_pause_evolution=check_pause;
  ev.on_summary=on_summary;ev.on_form_item=check_form;
  party_open(&a,S.party,S.n,&ev);S.open=true;ui_push(&g_screen);
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
  summary_update(dt / ui_game_speed());
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
