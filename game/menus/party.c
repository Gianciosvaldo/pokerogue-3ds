/*
 * Party screen: port of upstream src/ui/handlers/party-ui-handler.ts (8555c08).
 * Positions are upstream's; upstream's party container sits at the canvas
 * bottom (y 180), so every upstream y here is written as Y(upstream y).
 *
 * Not ported yet (hidden, not faked): the Pokedex option (no Pokedex screen
 * yet), Rename (needs a keyboard), and the move info overlay beside the
 * Memory Mushroom move list.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "menus.h"

#define Y(y) (180.0f + (y))
#define MAX_OPTS (MENU_MAX_FORM_ITEMS + 8)

static struct {
  bool active, suspended;
  party_args a;
  party_events ev;
  menu_mon party[6];
  int n;

  int cursor, last_cursor, last_left, last_right;

  bool options_mode, options_scroll;
  int options[MAX_OPTS], nopt;
  int options_cursor, options_scroll_cursor, options_scroll_total;
  float options_w;

  bool transfer_mode, transfer_all;
  int transfer_cursor, transfer_option_cursor;
  int transfer_qty[MENU_MAX_ITEMS], transfer_qty_max[MENU_MAX_ITEMS];
  char slot_desc[6][32]; /* "Able"/"Not able" labels over the HP bar (transfer, TM) */
  bool slot_desc_on[6];

  bool block_input;
  bool discard_selected; /* the transfer/discard mode button (cursor 7) */
  mui_msg msg;
  float msg_w, msg_h;    /* the message window */
  mui_confirm confirm;

  int pending_slot;      /* slot a confirm / message callback acts on */
  int pending_option;
  float anim_ms;         /* icon bob: toggles every 200 ms */
} P;

/* ------------------------------------------------------------ helpers */

static bool item_manage_mode(void) { return P.a.mode == PARTY_MODIFIER_TRANSFER || P.a.mode == PARTY_DISCARD; }
static bool allow_cancel(void) { return !(P.a.mode == PARTY_FAINT_SWITCH || P.a.mode == PARTY_REVIVAL_BLESSING); }
static const menu_mon* mon(int i) { return (i >= 0 && i < P.n) ? &P.party[i] : NULL; }
static bool fainted(const menu_mon* m) { return m->hp == 0; }

static int transferable_items(const menu_mon* m, int* idx) {
  int k = 0;
  for (int i = 0; i < m->nitems; i++)
    if (m->items[i].transferable) {
      if (idx) idx[k] = i;
      k++;
    }
  return k;
}

static void show_text(const char* text, int delay, mui_cb cb, bool prompt);
static void show_party_text(void);
static void update_options(void);
static bool set_cursor(int c);
static void clear_options(void);

static void msg_box_for(const char* text) {
  P.msg_w = 262;
  P.msg_h = strchr(text, '\n') ? 42 : 30;
}

/* PartyUiHandler.showText: "" means the default "Choose a Pokemon." */
static void show_text(const char* text, int delay, mui_cb cb, bool prompt) {
  if (!text || !*text) text = mui_t("partyUiHandler:choosePokemon");
  msg_box_for(text);
  mui_msg_show(&P.msg, text, delay, cb, NULL, prompt);
}

static void back_to_default(void* ctx) {
  (void)ctx;
  show_text("", 0, NULL, false);
}

static void show_party_text(void) {
  switch (P.a.mode) {
    case PARTY_MODIFIER_TRANSFER: show_text(mui_t("partyUiHandler:partyTransfer"), 20, NULL, false); break;
    case PARTY_DISCARD: show_text(mui_t("partyUiHandler:partyDiscard"), 20, NULL, false); break;
    default: show_text("", 0, NULL, false); break;
  }
}

/* ------------------------------------------------------------ open / close */

static void reset_transfer_labels(void) {
  for (int i = 0; i < 6; i++) P.slot_desc_on[i] = false;
  if (P.a.mode == PARTY_TM_MODIFIER) {
    for (int i = 0; i < P.n; i++) {
      const char* k = P.party[i].tm_state == 2   ? "partyUiHandler:learned"
                      : P.party[i].tm_state == 1 ? "partyUiHandler:able"
                                                 : "partyUiHandler:notAble";
      snprintf(P.slot_desc[i], sizeof P.slot_desc[i], "%s", mui_t(k));
      P.slot_desc_on[i] = true;
    }
  }
}

void party_open(const party_args* args, const menu_mon* party, int n, const party_events* ev) {
  memset(&P, 0, sizeof P);
  P.active = true;
  P.a = *args;
  if (P.a.battler_count < 1) P.a.battler_count = 1;
  if (P.a.max_level <= 0) P.a.max_level = 10000;
  if (ev) P.ev = *ev;
  P.n = n > 6 ? 6 : n;
  memcpy(P.party, party, sizeof(menu_mon) * P.n);
  P.cursor = args->initial_cursor >= 0 && args->initial_cursor < P.n ? args->initial_cursor : 0;
  reset_transfer_labels();
  show_party_text();
}

void party_refresh(const menu_mon* party, int n) {
  P.n = n > 6 ? 6 : n;
  memcpy(P.party, party, sizeof(menu_mon) * P.n);
  if (P.cursor < 6 && P.cursor >= P.n) P.cursor = P.n - 1;
  if (P.last_cursor < 6 && P.last_cursor >= P.n) P.last_cursor = P.n - 1;
  if (P.cursor < 0) P.cursor = 6;
  reset_transfer_labels();
  if (P.options_mode) update_options();
}

void party_show_text(const char* text, bool prompt) {
  if (P.options_mode) clear_options();
  show_text(text, 20, prompt ? back_to_default : NULL, prompt);
}

void party_resume(void) { P.suspended = false; }
void party_close(void) { P.active = false; }
bool party_active(void) { return P.active; }

/* ------------------------------------------------------------ options */

static bool localized_option(int o) {
  switch (o) {
    case PO_SEND_OUT: case PO_SUMMARY: case PO_POKEDEX: case PO_CANCEL: case PO_APPLY: case PO_RELEASE:
    case PO_TEACH: case PO_SPLICE: case PO_UNSPLICE: case PO_REVIVE: case PO_TRANSFER: case PO_UNPAUSE_EVOLUTION:
    case PO_PASS_BATON: case PO_RENAME: case PO_SELECT:
      return true;
    default:
      return false;
  }
}

static const char* option_key(int o) {
  switch (o) {
    case PO_CANCEL: return "partyUiHandler:cancel";
    case PO_SEND_OUT: return "partyUiHandler:sendOut";
    case PO_PASS_BATON: return "partyUiHandler:passBaton";
    case PO_REVIVE: return "partyUiHandler:revive";
    case PO_APPLY: return "partyUiHandler:apply";
    case PO_TEACH: return "partyUiHandler:teach";
    case PO_TRANSFER: return "partyUiHandler:transfer";
    case PO_SUMMARY: return "partyUiHandler:summary";
    case PO_POKEDEX: return "partyUiHandler:pokedex";
    case PO_UNPAUSE_EVOLUTION: return "partyUiHandler:unpauseEvolution";
    case PO_SPLICE: return "partyUiHandler:splice";
    case PO_UNSPLICE: return "partyUiHandler:unsplice";
    case PO_RELEASE: return "partyUiHandler:release";
    case PO_RENAME: return "partyUiHandler:rename";
    case PO_SELECT: return "partyUiHandler:select";
    case PO_DISCARD: return "partyUiHandler:discard";
    default: return "";
  }
}

extern const char* menu_move_name(uint16_t id); /* moves.c */

/* The label of one option (updateOptionsWindow's switch). */
static void option_label(int opt, char* out, size_t n, rr_color* col, rr_color* sh) {
  const menu_mon* m = mon(P.cursor);
  *col = mui_style_color(TS_WINDOW, false);
  *sh = mui_style_color(TS_WINDOW, true);
  if (opt == PO_SCROLL_UP) {
    snprintf(out, n, "\xE2\x86\x91");
  } else if (opt == PO_SCROLL_DOWN) {
    snprintf(out, n, "\xE2\x86\x93");
  } else if (P.a.mode == PARTY_CHECK && opt >= PO_FORM_CHANGE_ITEM && opt - PO_FORM_CHANGE_ITEM < m->nform_items) {
    const menu_form_item* item = &m->form_items[opt - PO_FORM_CHANGE_ITEM];
    snprintf(out, n, "%s %s", mui_t(item->active ? "partyUiHandler:deactivate" : "partyUiHandler:activate"), item->name);
  } else if ((P.a.mode != PARTY_REMEMBER_MOVE_MODIFIER && (P.a.mode != PARTY_MODIFIER_TRANSFER || P.transfer_mode) &&
              P.a.mode != PARTY_DISCARD) ||
             opt == PO_CANCEL) {
    if (opt >= PO_MOVE_1 && opt <= PO_MOVE_4) {
      const menu_move* mv = &m->moves[opt - PO_MOVE_1];
      if (P.a.show_move_pp)
        snprintf(out, n, "%s %d/%d", menu_move_name(mv->id), mv->pp_max - mv->pp_used, mv->pp_max);
      else
        snprintf(out, n, "%s", menu_move_name(mv->id));
    } else if (opt == PO_UNPAUSE_EVOLUTION) {
      snprintf(out, n, "%s",
               mui_t(m->pause_evolutions ? "partyUiHandler:unpauseEvolution" : "partyUiHandler:pauseEvolution"));
    } else if (localized_option(opt)) {
      snprintf(out, n, "%s", mui_t(option_key(opt)));
    } else {
      snprintf(out, n, "%s", mui_t(option_key(opt)));
    }
  } else if (P.a.mode == PARTY_REMEMBER_MOVE_MODIFIER) {
    snprintf(out, n, "%s", menu_move_name(m->relearn[opt]));
  } else if (opt == PO_ALL) {
    snprintf(out, n, "%s (%d)", mui_t("partyUiHandler:all"), transferable_items(m, NULL));
  } else {
    int idx[MENU_MAX_ITEMS];
    transferable_items(m, idx);
    const menu_held_item* it = &m->items[idx[opt]];
    if (item_manage_mode() && P.transfer_qty_max[opt] > 1 && !P.transfer_mode) {
      /* " (n)", red when the stack is at its maximum; drawn as one colour here */
      snprintf(out, n, "%s (%d)", it->name, P.transfer_qty[opt]);
      if (P.transfer_qty_max[opt] == it->max) {
        *col = mui_style_color(TS_SUMMARY_RED, false);
        *sh = mui_style_color(TS_SUMMARY_RED, true);
      }
    } else {
      snprintf(out, n, "%s", it->name);
    }
  }
}

static void add_common_options(const menu_mon* m) {
  if (P.ev.on_summary) P.options[P.nopt++] = PO_SUMMARY;
  /* PO_POKEDEX and PO_RENAME: not ported yet (see the file comment) */
  if (m->has_evolutions) P.options[P.nopt++] = PO_UNPAUSE_EVOLUTION;
}

static void add_cancel_and_scroll(void) {
  P.options_scroll_total = P.nopt;
  int start = P.options_scroll_cursor;
  int end = P.options_scroll_total;
  int take = (!start || P.options_scroll_cursor + 8 >= P.options_scroll_total) ? 8 : 7;
  if (start + take < end) end = start + take;
  P.options_scroll = P.options_scroll_total > 9;
  if (P.options_scroll) {
    int tmp[9], k = 0;
    if (start) tmp[k++] = PO_SCROLL_UP;
    for (int i = start; i < end; i++) tmp[k++] = P.options[i];
    if (end < P.options_scroll_total) tmp[k++] = PO_SCROLL_DOWN;
    memcpy(P.options, tmp, sizeof(int) * k);
    P.nopt = k;
  }
  P.options[P.nopt++] = PO_CANCEL;
}

static void add_transfer_item_options(const menu_mon* m) {
  int k = transferable_items(m, NULL);
  for (int i = 0; i < k; i++) P.options[P.nopt++] = i;
  if (k > 1) P.options[P.nopt++] = PO_ALL;
}

static void update_options_window(void) {
  int widest = 0;
  for (int i = 0; i < P.nopt; i++) {
    char lab[96];
    rr_color c, s;
    option_label(P.options[i], lab, sizeof lab, &c, &s);
    int w = mui_text_width(TS_WINDOW, lab);
    if (P.a.mode == PARTY_REMEMBER_MOVE_MODIFIER && P.options[i] >= 0 && P.options[i] < PO_SCROLL_UP &&
        mon(P.cursor)->relearn_level[P.options[i]]) {
      /* level prefix ("12") before the move name */
      char lv[8];
      snprintf(lv, sizeof lv, "%d", mon(P.cursor)->relearn_level[P.options[i]]);
      w += mui_text_width(TS_WINDOW, lv) + 1;
    }
    if (w > widest) widest = w;
  }
  P.options_w = widest + 24 > 94 ? widest + 24 : 94;
  /* updatePartyMessageBox: the message box shrinks beside a wide options window */
  float shrink = P.options_w - 56 > 0 ? P.options_w - 56 : 0;
  P.msg_w = 262 - shrink;
  P.msg_h = item_manage_mode() ? 42 : 30;
}

static void update_options(void) {
  const menu_mon* m = mon(P.cursor);
  P.nopt = 0;
  if (!m) return;
  if (fainted(m) && P.a.hardcore) {
    switch (P.a.mode) {
      case PARTY_MODIFIER_TRANSFER:
        if (P.transfer_mode) {
          P.options[P.nopt++] = PO_TRANSFER;
          add_common_options(m);
        } else {
          add_transfer_item_options(m);
        }
        break;
      case PARTY_DISCARD: add_transfer_item_options(m); break;
      case PARTY_SWITCH:
      case PARTY_RELEASE:
      case PARTY_CHECK: P.options[P.nopt++] = PO_RELEASE; break;
      default: break;
    }
    add_cancel_and_scroll();
    update_options_window();
    return;
  }
  switch (P.a.mode) {
    case PARTY_MOVE_MODIFIER:
      for (int i = 0; i < m->nmoves; i++) P.options[P.nopt++] = PO_MOVE_1 + i;
      break;
    case PARTY_REMEMBER_MOVE_MODIFIER:
      for (int i = 0; i < m->nrelearn && P.nopt < MAX_OPTS - 3; i++) P.options[P.nopt++] = i;
      break;
    case PARTY_MODIFIER_TRANSFER:
      if (P.transfer_mode) {
        P.options[P.nopt++] = PO_TRANSFER;
        add_common_options(m);
      } else {
        add_transfer_item_options(m);
      }
      break;
    case PARTY_DISCARD: add_transfer_item_options(m); break;
    case PARTY_SWITCH:
    case PARTY_FAINT_SWITCH:
    case PARTY_POST_BATTLE_SWITCH:
      if (P.cursor >= P.a.battler_count) {
        bool baton_item = P.a.mode != PARTY_FAINT_SWITCH && P.a.baton_item;
        bool baton_move = P.a.mode == PARTY_FAINT_SWITCH && P.a.baton_pass_move;
        if (baton_item && !baton_move && P.a.prefer_baton_pass) P.options[P.nopt++] = PO_PASS_BATON;
        P.options[P.nopt++] = (baton_move && !baton_item) ? PO_PASS_BATON : PO_SEND_OUT;
        if (baton_item && !baton_move && !P.a.prefer_baton_pass) P.options[P.nopt++] = PO_PASS_BATON;
      }
      add_common_options(m);
      break;
    case PARTY_REVIVAL_BLESSING:
      P.options[P.nopt++] = PO_REVIVE;
      add_common_options(m);
      break;
    case PARTY_MODIFIER:
      P.options[P.nopt++] = PO_APPLY;
      add_common_options(m);
      break;
    case PARTY_TM_MODIFIER:
      P.options[P.nopt++] = PO_TEACH;
      add_common_options(m);
      break;
    case PARTY_SPLICE:
      if (P.transfer_mode) {
        if (P.cursor != P.transfer_cursor) P.options[P.nopt++] = PO_SPLICE;
      } else {
        P.options[P.nopt++] = PO_APPLY;
      }
      add_common_options(m);
      if (m->fusion) P.options[P.nopt++] = PO_UNSPLICE;
      break;
    case PARTY_RELEASE:
      P.options[P.nopt++] = PO_RELEASE;
      add_common_options(m);
      break;
    case PARTY_CHECK:
      add_common_options(m);
      if (P.a.in_reward_phase) {
        if (!P.a.preserve_roster) {
          if (m->fusion) P.options[P.nopt++] = PO_UNSPLICE;
          P.options[P.nopt++] = PO_RELEASE;
        }
        if (P.ev.on_form_item && m->form_items && m->nform_items > 0 && m->nform_items <= MENU_MAX_FORM_ITEMS)
          for (int i = 0; i < m->nform_items; i++) P.options[P.nopt++] = PO_FORM_CHANGE_ITEM + i;
      }
      break;
    case PARTY_SELECT:
      P.options[P.nopt++] = PO_SELECT;
      add_common_options(m);
      break;
  }
  add_cancel_and_scroll();
  update_options_window();
}

static void show_options(void) {
  if (P.cursor == 6) return;
  P.options_mode = true;
  const char* k = "partyUiHandler:doWhatWithThisPokemon";
  switch (P.a.mode) {
    case PARTY_MOVE_MODIFIER: k = "partyUiHandler:selectAMove"; break;
    case PARTY_MODIFIER_TRANSFER:
      if (!P.transfer_mode) k = "partyUiHandler:changeQuantity";
      break;
    case PARTY_SPLICE:
      if (!P.transfer_mode) k = "partyUiHandler:selectAnotherPokemonToSplice";
      break;
    case PARTY_DISCARD: k = "partyUiHandler:changeQuantityDiscard"; break;
    default: break;
  }
  show_text(mui_t(k), 0, NULL, false);
  update_options();
  P.options_cursor = -1;
  set_cursor(0);
}

static void clear_options(void) {
  P.options_mode = false;
  P.options_scroll = false;
  P.options_scroll_cursor = 0;
  P.options_scroll_total = 0;
  P.nopt = 0;
  P.msg_w = 262;
  P.msg_h = 30;
  show_party_text();
}

static int options_cursor_with_scroll(void) {
  return P.options_cursor + P.options_scroll_cursor + (P.nopt && P.options[0] == PO_SCROLL_UP ? -1 : 0);
}

/* ------------------------------------------------------------ cursor */

static bool set_options_cursor(int c) {
  bool changed = P.options_cursor != c;
  bool is_scroll = false;
  if (changed && P.options_scroll) {
    if (abs(c - P.options_cursor) == P.nopt - 1) {
      P.options_scroll_cursor = c ? P.options_scroll_total - 8 : 0;
      update_options();
    } else {
      bool down = c && c > P.options_cursor;
      if (down) {
        if (P.options[c] == PO_SCROLL_DOWN) {
          is_scroll = true;
          P.options_scroll_cursor++;
        }
      } else if (!c && P.options_scroll_cursor) {
        is_scroll = true;
        P.options_scroll_cursor--;
      }
      if (is_scroll && P.options_scroll_cursor == 1) P.options_scroll_cursor += down ? 1 : -1;
    }
  }
  if (is_scroll)
    update_options();
  else
    P.options_cursor = c;
  return changed;
}

static bool set_cursor(int c) {
  if (P.options_mode) return set_options_cursor(c);
  bool changed = P.cursor != c;
  if (changed) {
    P.last_cursor = P.cursor;
    P.cursor = c;
    /* PartyDiscardModeButton.select/deselect show their own messages */
    if (P.last_cursor == 7 && P.discard_selected) {
      P.discard_selected = false;
      show_party_text();
    }
    if (c == 7) {
      P.discard_selected = true;
      show_text(mui_t("partyUiHandler:changeMode"), 20, NULL, false);
    }
  }
  return changed;
}

static bool move_option_cursor(mui_button b) {
  if (b == MB_UP) return set_cursor(P.options_cursor ? P.options_cursor - 1 : P.nopt - 1);
  return set_cursor(P.options_cursor < P.nopt - 1 ? P.options_cursor + 1 : 0);
}

/* ------------------------------------------------------------ actions */

static void clear_transfer(void) {
  P.transfer_mode = false;
  P.transfer_all = false;
  for (int i = 0; i < 6; i++) P.slot_desc_on[i] = false;
  reset_transfer_labels();
}

static void start_transfer(void) {
  P.transfer_mode = true;
  P.transfer_cursor = P.cursor;
  P.transfer_option_cursor = options_cursor_with_scroll();
  P.transfer_all = P.options[P.options_cursor] == PO_ALL;
}

static void play_select(void) { mui_sfx("ui/select"); }
static void play_error(void) { mui_sfx("ui/error"); }

static void summary_option(void) {
  play_select();
  clear_options();
  P.suspended = true;
  P.ev.on_summary(P.ev.ctx, P.cursor);
}

static void unpause_option(void) {
  menu_mon* m = &P.party[P.cursor];
  clear_options();
  play_select();
  m->pause_evolutions = !m->pause_evolutions;
  if (P.ev.on_pause_evolution) P.ev.on_pause_evolution(P.ev.ctx, P.cursor, m->pause_evolutions);
  char buf[160];
  mui_tf(buf, sizeof buf, m->pause_evolutions ? "partyUiHandler:pausedEvolutions" : "partyUiHandler:unpausedEvolutions",
         "pokemonName", m->name, NULL);
  show_text(buf, 20, back_to_default, true);
}

static const char* release_message(const char* name, char* buf, size_t n) {
  int r = rand() % 128;
  const char* k = r < 20    ? "partyUiHandler:goodbye"
                  : r < 40  ? "partyUiHandler:byebye"
                  : r < 60  ? "partyUiHandler:farewell"
                  : r < 80  ? "partyUiHandler:soLong"
                  : r < 100 ? "partyUiHandler:thisIsWhereWePart"
                  : r < 108 ? "partyUiHandler:illMissYou"
                  : r < 116 ? "partyUiHandler:illNeverForgetYou"
                  : r < 124 ? "partyUiHandler:untilWeMeetAgain"
                  : r < 127 ? "partyUiHandler:sayonara"
                            : "partyUiHandler:smellYaLater";
  return mui_tf(buf, n, k, "pokemonName", name, NULL);
}

static void release_done(void* ctx) {
  (void)ctx;
  int slot = P.pending_slot;
  if (P.ev.on_release) P.ev.on_release(P.ev.ctx, slot); /* the caller removes it and calls party_refresh */
  if (P.cursor >= P.n) set_cursor(P.cursor - 1);
  if (P.a.mode == PARTY_RELEASE && P.ev.on_select) P.ev.on_select(P.ev.ctx, slot, PO_RELEASE);
  show_text("", 0, NULL, false);
}

static void do_release(int slot) {
  char buf[160];
  P.pending_slot = slot;
  show_text(release_message(P.party[slot].name, buf, sizeof buf), 20, release_done, true);
}

static void release_yes(void* ctx) {
  (void)ctx;
  do_release(P.pending_slot);
}

static void release_confirm(void* ctx) {
  (void)ctx;
  P.block_input = false;
  mui_confirm_show(&P.confirm, release_yes, back_to_default, NULL);
}

static void release_option(void) {
  const menu_mon* m = mon(P.cursor);
  clear_options();
  play_select();
  if (P.a.mode == PARTY_RELEASE) {
    do_release(P.cursor);
    return;
  }
  if (P.cursor >= P.a.battler_count || !m->allowed_in_battle) {
    char buf[160];
    P.block_input = true;
    P.pending_slot = P.cursor;
    show_text(mui_tf(buf, sizeof buf, "partyUiHandler:releaseConfirmation", "pokemonName", m->name, NULL), 20,
              release_confirm, false);
  } else {
    show_text(mui_t("partyUiHandler:releaseInBattle"), 20, back_to_default, true);
  }
}

static void unsplice_yes(void* ctx) {
  (void)ctx;
  /* the caller unfuses (on_select with PO_UNSPLICE) and refreshes */
  if (P.ev.on_select) P.ev.on_select(P.ev.ctx, P.pending_slot, PO_UNSPLICE);
}

static void unsplice_confirm(void* ctx) {
  (void)ctx;
  mui_confirm_show(&P.confirm, unsplice_yes, back_to_default, NULL);
}

static void unsplice_option(void) {
  const menu_mon* m = mon(P.cursor);
  clear_options();
  play_select();
  char buf[200];
  P.pending_slot = P.cursor;
  /* {{fusionName}} is the fused name, {{pokemonName}} the base species' own name;
   * the display struct only has the fused name, so both use it */
  show_text(mui_tf(buf, sizeof buf, "partyUiHandler:unspliceConfirmation", "fusionName", m->name, "pokemonName",
                   m->name, NULL),
            20, unsplice_confirm, false);
}

static const char* filter_result(int option) {
  const char* r = NULL;
  if (option != PO_TRANSFER && option != PO_SPLICE) {
    if (P.a.filter) r = P.a.filter(P.ev.ctx, P.cursor);
    if (!r && (option == PO_SEND_OUT || option == PO_PASS_BATON) && !P.party[P.cursor].allowed_in_battle) {
      static char buf[160];
      r = mui_tf(buf, sizeof buf, "partyUiHandler:cantBeUsed", "pokemonName", P.party[P.cursor].name, NULL);
    }
    if (!r && P.a.mode == PARTY_MOVE_MODIFIER && P.a.move_filter)
      r = P.a.move_filter(P.ev.ctx, P.cursor, P.options_cursor);
  } else if (P.a.filter) {
    /* PokemonModifierTransferSelectFilter: the caller's filter sees the target slot */
    r = P.a.filter(P.ev.ctx, P.cursor);
  }
  return r;
}

static void emit_select(int slot, int option) {
  if (P.ev.on_select) P.ev.on_select(P.ev.ctx, slot, option);
}

static bool transfer_option(void) {
  if (P.transfer_cursor != P.cursor && P.ev.on_transfer) {
    const menu_mon* from = mon(P.transfer_cursor);
    if (P.transfer_all) {
      int k = transferable_items(from, NULL);
      for (int i = k - 1; i >= 0; i--)
        P.ev.on_transfer(P.ev.ctx, P.transfer_cursor, i, P.transfer_qty_max[i], P.cursor);
    } else {
      P.ev.on_transfer(P.ev.ctx, P.transfer_cursor, P.transfer_option_cursor,
                       P.transfer_qty[P.transfer_option_cursor], P.cursor);
    }
  }
  clear_transfer();
  clear_options();
  play_select();
  return true;
}

/* processModifierTransferModeInput: first pick an item, then the target */
static bool modifier_transfer_input(void) {
  int option = P.options[P.options_cursor];
  const menu_mon* m = mon(P.cursor);
  if (P.transfer_mode && option == PO_TRANSFER) return transfer_option();
  if (!P.transfer_mode) {
    start_transfer();
    const menu_mon* from = mon(P.transfer_cursor);
    int idx[MENU_MAX_ITEMS];
    int k = transferable_items(from, idx);
    for (int p = 0; p < P.n; p++) {
      const menu_mon* to = &P.party[p];
      char* out = P.slot_desc[p];
      if (p == P.transfer_cursor) {
        out[0] = 0;
      } else {
        /* matching stacks on the target; "Able" when any can take more */
        int able = 0, matches = 0, considered = 0;
        for (int i = 0; i < k; i++) {
          if (P.transfer_option_cursor != k && i != P.transfer_option_cursor) continue;
          considered++;
          const menu_held_item* it = &from->items[idx[i]];
          const menu_held_item* match = NULL;
          for (int j = 0; j < to->nitems; j++)
            if (!strcmp(to->items[j].name, it->name)) match = &to->items[j];
          if (match) matches++;
          if (!match || match->count < match->max) able++;
        }
        if (matches) {
          if (able)
            snprintf(out, 32, "%s", mui_t("partyUiHandler:able"));
          else
            snprintf(out, 32, "%s", mui_t("partyUiHandler:notAble"));
          if (able && considered > 1) snprintf(out + strlen(out), 32 - strlen(out), " (%d)", able);
        } else {
          snprintf(out, 32, "%s", mui_t("partyUiHandler:able"));
          if (considered > 1) snprintf(out + strlen(out), 32 - strlen(out), " (%d)", considered);
        }
      }
      P.slot_desc_on[p] = true;
    }
    clear_options();
    play_select();
    return true;
  }
  (void)m;
  if (option == PO_SUMMARY && P.ev.on_summary) {
    summary_option();
    return true;
  }
  if (option == PO_UNPAUSE_EVOLUTION) {
    unpause_option();
    return true;
  }
  return false;
}

static void discard_yes(void* ctx) {
  (void)ctx;
  int option = P.pending_option;
  if (P.ev.on_discard) {
    if (option == PO_ALL) {
      int k = transferable_items(mon(P.pending_slot), NULL);
      for (int i = k - 1; i >= 0; i--) P.ev.on_discard(P.ev.ctx, P.pending_slot, i, P.transfer_qty[i]);
    } else {
      P.ev.on_discard(P.ev.ctx, P.pending_slot, option, P.transfer_qty[option]);
    }
  }
  show_party_text();
}

static void discard_no(void* ctx) {
  (void)ctx;
  show_party_text();
}

static void discard_confirm(void* ctx) {
  (void)ctx;
  P.block_input = false;
  mui_confirm_show(&P.confirm, discard_yes, discard_no, NULL);
}

static bool discard_input(void) {
  P.pending_option = P.options[P.options_cursor];
  P.pending_slot = P.cursor;
  clear_options();
  P.block_input = true;
  show_text(mui_t("partyUiHandler:discardConfirmation"), 20, discard_confirm, false);
  return true;
}

static bool remember_move_input(void) {
  int option = P.options[P.options_cursor];
  const char* r = P.a.filter ? P.a.filter(P.ev.ctx, P.cursor) : NULL;
  if (!r) {
    emit_select(P.cursor, option);
    clear_options();
  } else {
    clear_options();
    show_text(r, 20, back_to_default, true);
  }
  play_select();
  return true;
}

static bool option_menu_input(mui_button b);

static bool action_for_option(int option) {
  if (option == PO_CANCEL) return option_menu_input(MB_CANCEL);
  if (option == PO_SCROLL_UP || option == PO_SCROLL_DOWN) return false;

  /* Form options use their own list ordinal, so large global inventory indices
   * cannot collide with move/All option codes. Route before those code ranges. */
  const menu_mon* current = mon(P.cursor);
  if (P.a.mode == PARTY_CHECK && P.a.in_reward_phase && P.ev.on_form_item && current
      && current->form_items && option >= PO_FORM_CHANGE_ITEM && option - PO_FORM_CHANGE_ITEM < current->nform_items) {
    int slot = P.cursor, item = current->form_items[option - PO_FORM_CHANGE_ITEM].inventory_index;
    clear_options();
    P.ev.on_form_item(P.ev.ctx, slot, item);
    return true;
  }

  if (P.a.mode == PARTY_MODIFIER_TRANSFER) return modifier_transfer_input();
  if (P.a.mode == PARTY_DISCARD) return discard_input();
  if (P.a.mode == PARTY_REMEMBER_MOVE_MODIFIER) return remember_move_input();

  if (option == PO_SUMMARY) {
    summary_option();
    return true;
  }
  if (option == PO_UNPAUSE_EVOLUTION) {
    unpause_option();
    return true;
  }
  if (option == PO_UNSPLICE) {
    unsplice_option();
    return true;
  }
  if (option == PO_RELEASE) {
    release_option();
    return true;
  }

  const char* r = filter_result(option);
  if (r) {
    clear_options();
    show_text(r, 20, back_to_default, true);
    return true;
  }

  if (P.a.mode == PARTY_SPLICE) {
    if (option == PO_SPLICE) {
      emit_select(P.transfer_cursor, P.cursor); /* PartyModifierSpliceSelectCallback(from, to) */
      clear_transfer();
    } else if (option == PO_APPLY) {
      start_transfer();
    }
    clear_options();
    play_select();
    return true;
  }

  switch (option) {
    case PO_SEND_OUT:
    case PO_PASS_BATON:
    case PO_REVIVE:
    case PO_APPLY:
    case PO_TEACH:
    case PO_MOVE_1:
    case PO_MOVE_2:
    case PO_MOVE_3:
    case PO_MOVE_4:
    case PO_SELECT:
      clear_options();
      emit_select(P.cursor, option);
      return true;
    default:
      return false;
  }
}

static bool transfer_left_right(mui_button b) {
  if (!item_manage_mode()) return false;
  int option = P.options[P.options_cursor];
  if (option < 0 || option >= MENU_MAX_ITEMS) return false;
  if (b == MB_LEFT)
    P.transfer_qty[option] = P.transfer_qty[option] == 1 ? P.transfer_qty_max[option] : P.transfer_qty[option] - 1;
  else
    P.transfer_qty[option] = P.transfer_qty[option] == P.transfer_qty_max[option] ? 1 : P.transfer_qty[option] + 1;
  update_options();
  return true;
}

static bool option_menu_input(mui_button b) {
  if (b == MB_CANCEL) {
    clear_options();
    play_select();
    return true;
  }
  if (b == MB_ACTION) return action_for_option(P.options[P.options_cursor]);
  if (b == MB_UP || b == MB_DOWN) {
    if (P.a.mode == PARTY_MODIFIER_TRANSFER) {
      int option = P.options[P.options_cursor];
      if (option != PO_ALL && option >= 0 && option < MENU_MAX_ITEMS)
        P.transfer_qty[option] = P.transfer_qty_max[option];
      update_options();
    }
    return move_option_cursor(b);
  }
  if ((b == MB_LEFT || b == MB_RIGHT) && item_manage_mode()) return transfer_left_right(b);
  return false;
}

static bool party_action(void) {
  if (P.cursor < 6) {
    if ((P.a.mode == PARTY_MODIFIER_TRANSFER && !P.transfer_mode) || P.a.mode == PARTY_DISCARD) {
      const menu_mon* m = mon(P.cursor);
      int idx[MENU_MAX_ITEMS];
      int k = transferable_items(m, idx);
      for (int i = 0; i < k; i++) P.transfer_qty[i] = P.transfer_qty_max[i] = m->items[idx[i]].count;
    }
    show_options();
    play_select();
  }
  if (P.cursor == 7 && !P.transfer_mode) {
    if (P.a.mode == PARTY_DISCARD)
      P.a.mode = PARTY_MODIFIER_TRANSFER;
    else if (P.a.mode == PARTY_MODIFIER_TRANSFER)
      P.a.mode = PARTY_DISCARD;
    else {
      play_error();
      return false;
    }
    play_select();
    return true;
  }
  if (P.cursor == 6) {
    if (allow_cancel()) return party_input(MB_CANCEL);
    play_error();
  }
  return true;
}

static bool party_cancel(void) {
  if ((P.a.mode == PARTY_MODIFIER_TRANSFER || P.a.mode == PARTY_SPLICE) && P.transfer_mode) {
    clear_transfer();
    play_select();
  } else if (allow_cancel()) {
    play_select();
    emit_select(6, PO_CANCEL);
  }
  return true;
}

static bool party_direction(mui_button b) {
  int slots = P.n, bc = P.a.battler_count;
  bool dbl = P.a.double_battle;
  if (P.last_cursor < bc) P.last_left = P.last_cursor;
  if (P.last_cursor >= bc && P.last_cursor < 6) P.last_right = P.last_cursor;
  bool ok = false;
  switch (b) {
    case MB_UP:
      if (item_manage_mode()) {
        if (P.cursor == 1) { ok = set_cursor(dbl ? 0 : 7); break; }
        if (P.cursor == 2) { ok = set_cursor(dbl ? 7 : 1); break; }
        if (P.cursor == 6) { ok = set_cursor(slots <= bc ? 7 : slots - 1); break; }
        if (P.cursor == 7) { ok = set_cursor(dbl && slots > 1 ? 1 : 0); break; }
      }
      ok = set_cursor(P.cursor ? (P.cursor < 6 ? P.cursor - 1 : slots - 1) : 6);
      break;
    case MB_DOWN:
      if (item_manage_mode()) {
        if (P.cursor == 0) { ok = set_cursor(dbl && slots > 1 ? 1 : 7); break; }
        if (P.cursor == 1) { ok = set_cursor(dbl ? 7 : slots > 2 ? 2 : 6); break; }
        if (P.cursor == 7) { ok = set_cursor(slots > bc ? bc : 6); break; }
      }
      ok = set_cursor(P.cursor < 6 ? (P.cursor < slots - 1 ? P.cursor + 1 : 6) : 0);
      break;
    case MB_LEFT:
      if (P.cursor == 6) ok = set_cursor(item_manage_mode() ? 7 : P.last_left);
      if (P.cursor >= bc && P.cursor < 6) ok = set_cursor(P.last_left);
      break;
    case MB_RIGHT:
      if (P.cursor == 7 || slots <= bc) {
        ok = set_cursor(6);
        break;
      }
      if (P.cursor < bc) ok = set_cursor(P.last_right ? P.last_right : bc);
      break;
    default: break;
  }
  if (ok) play_select();
  return ok;
}

bool party_input(mui_button b) {
  if (!P.active || P.suspended) return false;
  if (P.confirm.active) return mui_confirm_input(&P.confirm, b);
  if (mui_msg_input(&P.msg, b)) return true;
  if (P.block_input) return false;
  if (P.options_mode) {
    bool ok = option_menu_input(b);
    if (ok) play_select();
    return ok;
  }
  if (b == MB_ACTION) return party_action();
  if (b == MB_CANCEL) return party_cancel();
  if (b == MB_UP || b == MB_DOWN || b == MB_LEFT || b == MB_RIGHT) return party_direction(b);
  return false;
}

/* The Transfer / Discard button's centre; it is 25 high and (label length * 9 + 3) wide (PartyDiscardModeButton) */
static void mode_button(float* cx, float* cy, float* w) {
  *cx = P.a.double_battle ? 64 : 60;
  *cy = Y(P.a.double_battle ? -58 : -73);
  const char* label = mui_t(P.a.mode == PARTY_MODIFIER_TRANSFER ? "partyUiHandler:transfer" : "partyUiHandler:discard");
  int len = 0;
  for (const char* p = label; *p; p++) len += ((unsigned char)*p & 0xC0) != 0x80;
  *w = (float)(len * 9 + 3);
}

/* Where slot i's background is (its top left) and how big the art is (party_slot_main 110 x 49, party_slot_main_short 110 x 41, party_slot
 * 175 x 24: the pack's frames) */
static void slot_rect(int i, float* x, float* y, float* w, float* h) {
  const bool benched = i >= P.a.battler_count, dbl = P.a.double_battle, manage = item_manage_mode();
  float sx, sy;
  if (benched) {
    sx = 143;
    sy = -196 + (dbl ? -40 : 0) + (28 + (dbl ? 8 : 0)) * i;
  } else {
    sx = 9;
    sy = -148.5f + (dbl ? (manage ? -20 : -8) : 0) + (manage ? (dbl ? 47 : 55) : 64) * i;
  }
  *x = floorf(sx);
  *y = floorf(Y(sy));
  *w = benched ? 175 : 110;
  *h = benched ? 24 : (manage && dbl) ? 41 : 49;
}

static bool inside(float x, float y, float rx, float ry, float rw, float rh) { return x >= rx && x < rx + rw && y >= ry && y < ry + rh; }

bool party_touch(float x, float y) {
  if (!P.active || P.suspended) return false;
  if (P.confirm.active) {
    mui_confirm_touch(&P.confirm, x, y);
    return true; /* modal: a tap beside the box takes nothing, as other buttons take nothing there */
  }
  if (mui_msg_input(&P.msg, MB_ACTION)) return true;
  if (P.block_input) return false;
  if (P.options_mode) {
    /* the options window, as draw_options places it: rows of 16 px from 6 px under its top */
    const float right = MUI_W - 1, bottom = Y(-1), bh = 16.0f * P.nopt + 13, left = right - P.options_w, top = bottom - bh;
    if (!inside(x, y, left, top, P.options_w, bh)) {
      const bool ok = option_menu_input(MB_CANCEL); /* a tap outside is B */
      if (ok) play_select();
      return ok;
    }
    if (y < top + 6 || y >= top + 6 + 16.0f * P.nopt) return true; /* the window's padding */
    const int k = (int)((y - top - 6) / 16);
    const int o = P.options[k];
    if (o == PO_SCROLL_UP || o == PO_SCROLL_DOWN) { /* one step of scrolling, as the cursor pressed on the arrow does */
      P.options_cursor = o == PO_SCROLL_UP ? k + 1 : k - 1;
      const bool ok = option_menu_input(o == PO_SCROLL_UP ? MB_UP : MB_DOWN);
      if (ok) play_select();
      return ok;
    }
    P.options_cursor = k;
    const bool ok = option_menu_input(MB_ACTION);
    if (ok) play_select();
    return ok;
  }
  for (int i = 0; i < P.n && i < 6; i++) {
    float sx, sy, sw, sh;
    slot_rect(i, &sx, &sy, &sw, &sh);
    if (!inside(x, y, sx, sy, sw, sh)) continue;
    set_cursor(i);
    return party_action();
  }
  if (inside(x, y, 291 - 26, Y(-16) - 8, 52, 16)) { /* the Cancel button, centred at (291, -16) */
    set_cursor(6);
    return party_action();
  }
  if (item_manage_mode() && !P.transfer_mode) {
    float cx, cy, w;
    mode_button(&cx, &cy, &w);
    if (inside(x, y, cx - w / 2, cy - 12.5f, w, 25)) {
      set_cursor(7);
      return party_action();
    }
  }
  return false;
}

void party_update(float dt_ms) {
  if (!P.active) return;
  P.anim_ms += dt_ms;
  mui_msg_update(&P.msg, dt_ms);
}

/* ------------------------------------------------------------ drawing */

static const char* status_frame(int s) {
  static const char* names[] = {"", "poison", "toxic", "paralysis", "sleep", "freeze", "burn", "faint"};
  return (s > 0 && s < 8) ? names[s] : NULL;
}

static rr_color variant_tint(int v) {
  switch (v) {
    case 1: return MUI_RGB(0x20f8f0);
    case 2: return MUI_RGB(0xe81048);
    default: return MUI_RGB(0xf8c020);
  }
}

static void draw_slot(int i) {
  const menu_mon* m = &P.party[i];
  bool benched = i >= P.a.battler_count;
  bool dbl = P.a.double_battle, manage = item_manage_mode();
  bool selected = P.cursor == i;
  bool transfer = P.transfer_mode && P.transfer_cursor == i;
  float sx, sy, sw, sh;
  slot_rect(i, &sx, &sy, &sw, &sh);

  const char* bgkey = benched ? "party_slot" : (manage && dbl) ? "party_slot_main_short" : "party_slot_main";
  char key[48], frame[64];
  snprintf(key, sizeof key, "ui/%s", bgkey);
  snprintf(frame, sizeof frame, "%s%s%s", bgkey, transfer ? "_swap" : m->hp ? "" : "_fnt", selected ? "_sel" : "");
  mui_image(key, frame, sx, sy, 0, 0);

  float pbx = benched ? 2 : 4, pby = benched ? 12 : 4;
  float namex = benched ? 21 : 24, namey = benched ? 2 : 10;
  int max_name = benched ? 52 : 76 - (m->fusion ? 8 : 0);
  float lvx = namex + 8, lvy = namey + 12;
  float gx = benched ? 36 : 68 - (m->fusion ? 8 : 0), gy = benched ? 0 : -9;
  float spx = benched ? 36 + (m->gender != MG_GENDERLESS ? 8 : 0) : 68, spy = benched ? 0.5f : 3.5f - 12;
  float stx = benched ? 43 : 55, sty = 0;
  float hpx = benched ? 72 : 8, hpy = benched ? 6 : 31;
  float dx = benched ? 94 : 32, dy = benched ? 16 : 46;
  if (manage && dbl && !benched) {
    namey -= 8;
    lvy -= 8;
    hpy -= 8;
    dy -= 8;
  }

  mui_image("ui/party_pb", selected ? "party_pb_sel" : "party_pb", sx + pbx, sy + pby, 0.5f, 0.5f);
  /* PokemonIconAnimHelper: passive bob 1 px, active 2 px, toggling every 200 ms */
  bool toggled = ((int)(P.anim_ms / 200)) & 1;
  float bob = toggled ? (selected ? -2 : -1) : 0;
  menu_draw_icon(m, sx + pbx, sy + pby + bob, 0.5f, 0.5f);

  char name[48];
  snprintf(name, sizeof name, "%s", m->name);
  mui_fit_name(TS_PARTY, name, max_name);
  mui_text(TS_PARTY, sx + namex, sy + namey, name);

  float lx = sx + lvx, ly = sy + lvy;
  mui_image("ui/text_images/en/party_ui/party_slot_overlay_lv", NULL, lx, ly, 0, 0);
  char lv[16];
  snprintf(lv, sizeof lv, "%u", (unsigned)m->level);
  mui_text_o(m->level < (uint32_t)P.a.max_level ? TS_PARTY : TS_PARTY_RED, lx + 9, ly, 0, 0.25f, lv);

  if (m->gender != MG_GENDERLESS) {
    bool f = m->gender == MG_FEMALE;
    rr_color c = MUI_RGB(f ? 0xf89890 : 0x40c8f8), s = MUI_RGB(f ? 0x984038 : 0x006090);
    const char* sym = f ? "\xE2\x99\x80" : "\xE2\x99\x82";
    mui_text_c(TS_PARTY, lx + gx, ly + gy - 0.25f * mui_text_height(TS_PARTY, sym), sym, c, s);
  }
  if (m->fusion) {
    rr_draw_opts o = RR_DEFAULT_OPTS;
    o.scale_x = o.scale_y = 0.5f;
    mui_image_ex("ui/icon_spliced", NULL, floorf(lx + spx), floorf(ly + spy), &o);
  }
  if (m->status) mui_image("statuses", status_frame(m->status), lx + stx, ly + sty, 0, 0);
  if (m->shiny) {
    bool dbl_shiny = m->fusion && m->fusion_shiny;
    rr_draw_opts o = RR_DEFAULT_OPTS;
    o.tint = variant_tint(m->variant);
    float x = sx + namex - 9, y = sy + namey + 3;
    mui_image_ex(dbl_shiny ? "ui/shiny_small_1" : "ui/shiny_small", NULL, x, y, &o);
    if (dbl_shiny) {
      o.tint = variant_tint(m->fusion_variant);
      mui_image_ex("ui/shiny_small_2", NULL, x, y, &o);
    }
  }

  if (P.slot_desc_on[i]) {
    mui_text_o(TS_MESSAGE, sx + dx, sy + dy, 0, 1, P.slot_desc[i]);
    return;
  }
  float bx = sx + hpx, by = sy + hpy;
  mui_image("ui/text_images/en/party_ui/party_slot_overlay_hp", NULL, bx + 15, by, 1, 0);
  mui_image("ui/party_slot_hp_bar", NULL, bx, by, 0, 0);
  float ratio = m->max_hp ? (float)m->hp / m->max_hp : 0;
  const rr_frame* ov = mui_frame("ui/party_slot_hp_overlay", ratio > 0.5f ? "high" : ratio > 0.25f ? "medium" : "low");
  if (ov) {
    /* setScale(hpRatio, 1): the fill shrinks from the right */
    rr_draw_opts o = RR_DEFAULT_OPTS;
    o.scale_x = ratio;
    if (ratio > 0) rr_draw(ov, bx + 16, by + 2, &o);
  }
  char hp[24];
  snprintf(hp, sizeof hp, "%lu/%lu", (unsigned long)m->hp, (unsigned long)m->max_hp);
  const rr_frame* bar = mui_frame("ui/party_slot_hp_bar", NULL);
  float bw = bar ? bar->src_w : 100, bh = bar ? bar->src_h : 7;
  mui_text_o(TS_PARTY, bx + bw - 3, by + bh - 2, 1, 0, hp);
}

static void draw_options(void) {
  float right = MUI_W - 1, bottom = Y(-1);
  float bh = 16 * P.nopt + 13;
  mui_window(right - P.options_w, bottom - bh, P.options_w, bh);
  int prefix_w = 0;
  if (P.a.mode == PARTY_REMEMBER_MOVE_MODIFIER) {
    for (int i = 0; i < P.nopt; i++)
      if (P.options[i] >= 0 && P.options[i] < PO_SCROLL_UP && mon(P.cursor)->relearn_level[P.options[i]]) {
        char lv[8];
        snprintf(lv, sizeof lv, "%d", mon(P.cursor)->relearn_level[P.options[i]]);
        int w = mui_text_width(TS_WINDOW, lv) + 1;
        if (w > prefix_w) prefix_w = w;
      }
  }
  for (int k = 0; k < P.nopt; k++) {
    int o = P.nopt - 1 - k; /* upstream counts from the bottom */
    int opt = P.options[k];
    char lab[96];
    rr_color c, s;
    option_label(opt, lab, sizeof lab, &c, &s);
    float ty = bottom - 22 - 16 * o;
    mui_text_c(TS_WINDOW, right + 19 - P.options_w + prefix_w, ty, lab, c, s);
    if (prefix_w && opt >= 0 && opt < PO_SCROLL_UP && mon(P.cursor)->relearn_level[opt]) {
      char lv[8];
      snprintf(lv, sizeof lv, "%d", mon(P.cursor)->relearn_level[opt]);
      /* level prefix: origin (1, 0.5) at y - 8 */
      mui_text_o(TS_WINDOW, right + 14 - P.options_w + prefix_w - 1, bottom - 6 - 16 * o - 8, 1, 0.5f, lv);
    }
  }
  if (P.options_cursor >= 0)
    mui_image("ui/cursor", NULL, right + 8 - P.options_w, bottom - 19 - 16 * (P.nopt - 1 - P.options_cursor), 0, 0);
}

void party_draw(void) {
  if (!P.active) return;
  const char* bg = item_manage_mode() ? (P.a.double_battle ? "ui/party_bg_double_manage" : "ui/party_bg")
                                      : (P.a.double_battle ? "ui/party_bg_double" : "ui/party_bg");
  mui_image(bg, NULL, 0, Y(0), 0, 1);
  for (int i = 0; i < P.n; i++) draw_slot(i);

  /* message box: container (0, -32), window (1, 31) origin (0, 1) */
  mui_window(1, Y(-32 + 31) - P.msg_h, P.msg_w, P.msg_h);
  mui_msg_draw(&P.msg, TS_WINDOW, 10, Y(-32) + (mui_msg_lines(&P.msg) > 1 ? -5 : 10));

  /* cancel button at (291, -16), sprites centred */
  bool csel = P.cursor == 6;
  mui_image("ui/party_cancel", csel ? "party_cancel_sel" : "party_cancel", 291, Y(-16), 0.5f, 0.5f);
  mui_image("ui/party_pb", csel ? "party_pb_sel" : "party_pb", 291 - 17, Y(-16), 0.5f, 0.5f);
  mui_text(TS_PARTY_CANCEL_BUTTON, 291 - 10 + 2, Y(-16) - 7, mui_t("partyUiHandler:cancelButton"));

  if (item_manage_mode()) {
    /* PartyDiscardModeButton */
    bool dsel = P.cursor == 7;
    float bx, by, bw;
    mode_button(&bx, &by, &bw);
    bool transfer = P.a.mode == PARTY_MODIFIER_TRANSFER;
    const char* label = mui_t(transfer ? "partyUiHandler:transfer" : "partyUiHandler:discard");
    const rr_frame* f = mui_frame(transfer ? "ui/party_transfer" : "ui/party_discard", dsel ? "selected" : "normal");
    if (f) {
      /* displayWidth = label length * 9 + 3 (upstream stretches the icon) */
      rr_draw_opts o = RR_DEFAULT_OPTS;
      o.origin_x = o.origin_y = 0.5f;
      o.scale_x = bw / (float)f->src_w;
      rr_draw(f, bx, by, &o);
    }
    mui_text(TS_PARTY, bx - 8, by - 7, label);
  }

  if (P.options_mode) draw_options();
  mui_confirm_draw(&P.confirm);
}
