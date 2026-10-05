/* The app's run driver (see run_screen.h). */
#include "run_screen.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ball_menu.h"
#include "clear_ui.h"
#include "game_over_ui.h"
#include "gm_enum_names.h"
#include "pr_data.h"
#include "battle_anim.h"
#include "battle_events.h"
#include "battle_view.h"
#include "i18n.h"
#include "menus.h"
#include "party_screen.h"
#include "pr_enums.h"
#include "save.h"
#include "shop_ui.h"
#include "ui.h"
#include "ui_widgets.h"

extern int gc_item_stack(const gc_run *run, const char *key);

#define EV_CAP 400      /* screen events one game-core event can make */
#define CHUNK 150       /* battle_view queues at most 160 at a time */
#define TXT_CAP 12
#define TXT_LEN 320

static struct {
  gc_run run;
  rs_config cfg;
  rs_stats st;
  uint64_t rng;                 /* the stand-in team's */
  int active_slot;              /* active save slot (0..4) */
  /* the events of the current drain, handled one by one */
  gc_event q[GC_MAX_EVENTS];
  bool skip[GC_MAX_EVENTS];
  int nq, qi;
  bool swap_prompt;             /* the drain has "swap or release" for a caught Pokemon */
  bool learn_drop;              /* the next drain starts with the learn dialog's own messages */
  bool swap_replaced;           /* a caught Pokemon replaced the one on the field */
  /* what the battle screen shows */
  bool present[2];              /* a Pokemon of each side is out */
  int shown;                    /* party index of the player's Pokemon on screen, -1 none */
  bool fade_done;               /* a fade is already at black */
  int wave_ev;                  /* the WAVE event being shown */
  /* the screen events being played */
  bv_event ev[EV_CAP];
  char txt[TXT_CAP][TXT_LEN];
  int n_ev, ev_pos, n_txt;
  int refused_in_row;
} G = {.shown = -1, .active_slot = 0};

static void pump(void* arg);
static void screen_step(void);

void rs_set_slot(int slot) {
  if (slot >= 0 && slot < 5) G.active_slot = slot;
}

int rs_get_slot(void) {
  return G.active_slot;
}

void rs_on_end_screen_dismissed(void) {
  memset(&G.run, 0, sizeof G.run);
}

/* ------------------------------------------------------------------ helpers */

static bool msg_is(const gc_event* e, const char* key) {
  return e->kind == GC_EV_MESSAGE && e->msg.key && !strcmp(e->msg.key, key);
}

static int pidx_of(const pr_pokemon* p) {
  if (!p) return -1;
  const gc_mon* m = (const gc_mon*)p; /* pr_pokemon b is gc_mon's first member */
  if (m < G.run.party || m >= G.run.party + G.run.n_party) return -1;
  return (int)(m - G.run.party);
}

static int slot_of(int party_index) {
  for (int k = 0; k < G.run.n_party; k++)
    if (G.run.party_order[k] == party_index) return k;
  return -1;
}

static gc_mon before_exp(int idx) {
  gc_mon m = G.run.party[idx];
  bool first = true;
  for (int j = G.qi; j < G.nq; j++) {
    const gc_event* e = &G.q[j];
    if (e->kind != GC_EV_EXP || e->a != idx) continue;
    m.exp -= e->b;
    if (first) m.b.level = (uint8_t)e->c;
    first = false;
  }
  return m;
}

static float exp_ratio_cb(const pr_pokemon* p) {
  int i = pidx_of(p);
  if (i < 0) return 0.0f;
  const gc_mon m = before_exp(i);
  return (float)gc_exp_level_fraction(&m);
}

static int level_of_cb(const pr_pokemon* p) {
  int i = pidx_of(p);
  return i < 0 ? p->level : before_exp(i).b.level;
}

static uint32_t rnd(uint32_t n) {
  G.rng ^= G.rng << 13;
  G.rng ^= G.rng >> 7;
  G.rng ^= G.rng << 17;
  return n ? (uint32_t)(G.rng % n) : 0;
}

static const char* why_text(const char* why, char* out, int n) {
  if (!why) why = "";
  if (strchr(why, ':') && !strchr(why, ' ')) {
    char name[48] = "";
    if (G.run.n_party > 0) snprintf(name, sizeof name, "%s", i18n_species_name(G.run.party[0].b.species));
    return i18n_t(out, n, why, "pokemonName", name, NULL);
  }
  snprintf(out, (size_t)n, "%s", why);
  return out;
}

/* ------------------------------------------------------------------ building screen events */

static bv_event* ev_new(void) { return G.n_ev < EV_CAP ? &G.ev[G.n_ev++] : NULL; }
static char* txt_new(void) { return G.txt[G.n_txt < TXT_CAP ? G.n_txt++ : TXT_CAP - 1]; }

static void put_msg(const char* text, bool prompt) {
  bv_event* e = ev_new();
  if (!e) return;
  char* t = txt_new();
  snprintf(t, TXT_LEN, "%s", text);
  *e = (bv_event){.type = BEV_MESSAGE, .battler = -1, .text = t, .prompt = prompt};
}

static bool put_gc_msg(const pr_msg* m) {
  if (!m->key) return false;
  char* t = txt_new();
  pr_msg_format(m, be_text_source(), t, TXT_LEN);
  if (!t[0]) return false;
  bv_event* e = ev_new();
  if (!e) return false;
  *e = (bv_event){.type = BEV_MESSAGE, .battler = -1, .text = t};
  return true;
}

static void play_chunk(void) {
  int n = G.n_ev - G.ev_pos;
  if (n > CHUNK) n = CHUNK;
  const int pos = G.ev_pos;
  G.ev_pos += n;
  G.st.bev += (uint32_t)n;
  bv_play(&G.ev[pos], n, pump, NULL);
}

static void say(const char* text) {
  G.n_ev = G.ev_pos = G.n_txt = 0;
  put_msg(text, false);
  play_chunk();
}

/* ------------------------------------------------------------------ events */

static void drain(void) {
  G.nq = G.qi = 0;
  G.swap_prompt = false;
  memset(G.skip, 0, sizeof G.skip);
  while (G.nq < GC_MAX_EVENTS && gc_run_next_event(&G.run, &G.q[G.nq])) G.nq++;
  const gc_screen* s = gc_run_screen(&G.run);

  if (G.learn_drop) {
    G.learn_drop = false;
    for (int i = 0; i < G.nq; i++) {
      const gc_event* e = &G.q[i];
      if (msg_is(e, "battle:learnMoveNotLearned") || msg_is(e, "battle:countdownPoof") ||
          msg_is(e, "battle:learnMoveForgetSuccess") || msg_is(e, "battle:learnMoveAnd") ||
          e->kind == GC_EV_MOVE_LEARNED || (i > 0 && G.skip[i - 1] && G.q[i - 1].kind == GC_EV_MOVE_LEARNED && msg_is(e, "battle:learnMove"))) {
        G.skip[i] = true;
        continue;
      }
      break;
    }
  }
  for (int i = 0; i < G.nq; i++) {
    const gc_event* e = &G.q[i];
    if (msg_is(e, "menu:swapOrRelease")) G.swap_prompt = true;
    if (s->kind == GC_SCREEN_LEARN_MOVE && (msg_is(e, "battle:learnMovePrompt") || msg_is(e, "battle:learnMoveLimitReached")))
      G.skip[i] = true;
    if (e->kind == GC_EV_BATTLE) {
      for (int j = i + 1; j < G.nq && G.q[j].kind == GC_EV_MESSAGE; j++) {
        const char* k = G.q[j].msg.key;
        if (!k || (strcmp(k, "battle:playerGo") && strcmp(k, "battle:trainerSendOut"))) continue;
        for (int b = 0; b < e->b; b++) {
          const pr_event* be = &G.run.battle_events[e->a + b];
          if (be->type == PR_EV_TEXT && be->msg.key && !strcmp(be->msg.key, k)) {
            G.skip[j] = true;
            G.st.dup_dropped++;
            break;
          }
        }
      }
    }
  }
}

static bool settled_after(int i) {
  const gc_screen* s = gc_run_screen(&G.run);
  if (s->kind != GC_SCREEN_COMMAND && s->kind != GC_SCREEN_SWITCH) return false;
  for (int j = i + 1; j < G.nq; j++) {
    int k = G.q[j].kind;
    if (k == GC_EV_BATTLE || k == GC_EV_WAVE || k == GC_EV_EXP || k == GC_EV_SUMMON || k == GC_EV_RECALL) return false;
  }
  return true;
}

static bv_mon make_mon(const gc_mon* m, bool player) {
  bv_mon out;
  be_mon(&G.run.bt, &m->b, &out);
  if (player) out.exp_ratio = (float)gc_exp_level_fraction(m);
  return out;
}

static void preload_moves(void) {
  for (int i = 0; i < G.run.n_party; i++)
    for (int k = 0; k < G.run.party[i].b.n_moves; k++) ba_preload_move(G.run.party[i].b.moves[k]);
  for (int i = 0; i < G.run.n_enemy; i++)
    for (int k = 0; k < G.run.enemy[i].b.n_moves; k++) ba_preload_move(G.run.enemy[i].b.moves[k]);
}

static void wave_black(void* arg) {
  (void)arg;
  const gc_event* w = &G.q[G.wave_ev];
  const bool trainer = w->c == BATTLE_TYPE_TRAINER;
  const bool mystery = w->c == BATTLE_TYPE_MYSTERY_ENCOUNTER;
  img_drop_prefix("images/pokemon/");
  img_drop_prefix("images/arenas/");
  ba_drop_moves();
  preload_moves();

  bv_setup s;
  memset(&s, 0, sizeof s);
  s.biome = w->b;
  s.trainer = trainer;
  if (G.present[0] && G.shown >= 0) {
    s.player = *bv_mon_of(BV_PLAYER);
    s.player_present = true;
  } else if (G.run.n_party > 0) {
    s.player = make_mon(&G.run.party[G.run.party_order[0]], true);
    s.player_present = true;
  }
  if (!trainer && !mystery && G.run.n_enemy > 0) s.enemy = make_mon(&G.run.enemy[0], false);
  bv_begin(&s);
  G.present[0] = s.player_present;
  G.present[1] = !trainer && !mystery && s.enemy.species > 0;
  if (!G.present[0]) G.shown = -1;

  G.n_ev = G.ev_pos = G.n_txt = 0;
  bv_event* e = ev_new();
  char* t = txt_new();
  t[0] = 0;
  if (trainer || mystery) {
    for (int j = G.wave_ev + 1; j < G.nq && j <= G.wave_ev + 5; j++) {
      if (G.q[j].kind == GC_EV_MESSAGE && !G.skip[j]) {
        if (G.q[j].msg.key) pr_msg_format(&G.q[j].msg, be_text_source(), t, TXT_LEN);
        G.skip[j] = true;
        break;
      }
    }
    if (mystery && !t[0]) snprintf(t, TXT_LEN, "A strange encounter!");
  } else {
    for (int j = G.wave_ev + 1; j < G.nq && j <= G.wave_ev + 5; j++)
      if (G.q[j].kind == GC_EV_MESSAGE && msg_is(&G.q[j], "battle:singleWildAppeared")) {
        G.skip[j] = true;
        break;
      }
  }
  *e = (bv_event){.type = BEV_INTRO_WILD, .battler = BV_ENEMY, .a = trainer || mystery ? 1 : 0, .text = t};
  play_chunk();
}

static void begin_wave(int i) {
  G.wave_ev = i;
  G.st.waves++;
  if (G.fade_done || ui_depth() == 0) {
    G.fade_done = false;
    wave_black(NULL);
  } else {
    ui_fade(250, 100, 250, wave_black, NULL);
  }
}

static void sync_shown(void) {
  const gc_mon* me = gc_field_mon(&G.run, 0);
  bv_mon* shown = bv_mon_of(BV_PLAYER);
  if (!me || !G.present[0]) return;
  const int idx = (int)(me - G.run.party);
  if (idx != G.shown) {
    G.st.shown_mismatch++;
    return;
  }
  bv_mon m;
  be_mon(&G.run.bt, &me->b, &m);
  shown->level = m.level;
  shown->max_hp = m.max_hp;
  shown->n_moves = m.n_moves;
  for (int i = 0; i < 4; i++) {
    shown->moves[i] = m.moves[i];
    shown->pp_left[i] = m.pp_left[i];
    shown->pp_max[i] = m.pp_max[i];
  }
  snprintf(shown->name, sizeof shown->name, "%s", m.name);
  shown->n_types = m.n_types;
  for (int i = 0; i < 3; i++) shown->types[i] = m.types[i];
}

static bool handle_event(int i) {
  const gc_event* e = &G.q[i];
  G.st.events[e->kind & 15]++;
  G.n_ev = G.ev_pos = G.n_txt = 0;
  switch (e->kind) {
    case GC_EV_MESSAGE:
      put_gc_msg(&e->msg);
      break;

    case GC_EV_WAVE:
      begin_wave(i);
      return true;

    case GC_EV_BATTLE: {
      be_view v;
      memset(&v, 0, sizeof v);
      v.mon[0] = *bv_mon_of(BV_PLAYER);
      v.mon[1] = *bv_mon_of(BV_ENEMY);
      v.present[0] = G.present[0];
      v.present[1] = G.present[1];
      v.exp_ratio = exp_ratio_cb;
      v.level_of = level_of_cb;
      const pr_pokemon* enemy_ptr[6];
      for (int k = 0; k < G.run.n_enemy && k < 6; k++) enemy_ptr[k] = &G.run.enemy[k].b;
      v.enemy_party = enemy_ptr;
      v.n_enemy_party = G.run.n_enemy < 6 ? G.run.n_enemy : 6;
      G.n_ev = be_convert_events(&G.run.bt, &G.run.battle_events[e->a], e->b, settled_after(i), &v, G.ev, EV_CAP);
      G.present[0] = v.present[0];
      G.present[1] = v.present[1];
      if (v.sent[0]) G.shown = pidx_of(v.sent[0]);
      G.st.not_found += v.not_found;
      break;
    }

    case GC_EV_SUMMON:
    case GC_EV_RECALL: {
      const int side = e->a;
      const bool recall = e->kind == GC_EV_RECALL;
      if (side == 0) {
        if (recall) {
          if (G.present[0]) {
            *ev_new() = (bv_event){.type = BEV_WITHDRAW, .battler = BV_PLAYER};
            G.present[0] = false;
            G.shown = -1;
          }
          break;
        }
        if (G.present[0] && G.shown == e->b) break;
        if (G.present[0]) {
          *ev_new() = (bv_event){.type = BEV_WITHDRAW, .battler = BV_PLAYER};
          G.present[0] = false;
        }
      } else {
        if (recall) {
          if (G.present[1]) {
            *ev_new() = (bv_event){.type = BEV_WITHDRAW, .battler = BV_ENEMY};
            G.present[1] = false;
          }
          break;
        }
        if (G.present[1]) break;
      }
      static bv_mon sent;
      const gc_mon* m = side == 0 ? &G.run.party[e->b] : &G.run.enemy[e->b];
      sent = make_mon(m, side == 0);
      char* text = NULL;
      if (i + 1 < G.nq && (msg_is(&G.q[i + 1], "battle:playerGo") || msg_is(&G.q[i + 1], "battle:trainerSendOut"))) {
        text = txt_new();
        pr_msg_format(&G.q[i + 1].msg, be_text_source(), text, TXT_LEN);
        G.skip[i + 1] = true;
      }
      if (side == 1 && text) put_msg(text, false);
      *ev_new() = (bv_event){.type = BEV_SUMMON, .battler = side ? BV_ENEMY : BV_PLAYER, .mon = &sent, .text = side ? NULL : text};
      G.present[side] = true;
      if (side == 0) G.shown = e->b;
      break;
    }

    case GC_EV_HP: {
      if (e->a != 0 || e->b != G.shown || !G.present[0]) break;
      const bv_mon* sh = bv_mon_of(BV_PLAYER);
      if (e->c > sh->hp)
        *ev_new() = (bv_event){.type = BEV_HEAL, .battler = BV_PLAYER, .a = e->c};
      else if (e->c < sh->hp)
        *ev_new() = (bv_event){.type = BEV_DAMAGE, .battler = BV_PLAYER, .a = e->c, .b = HIT_RESULT_INDIRECT};
      break;
    }

    case GC_EV_EXP: {
      if (e->a != G.shown || !G.present[0]) break;
      if (i + 1 < G.nq && G.q[i + 1].kind == GC_EV_LEVEL_UP && G.q[i + 1].a == e->a) break;
      const gc_mon m = before_exp(e->a);
      *ev_new() = (bv_event){.type = BEV_EXP, .battler = BV_PLAYER, .a = (int)(gc_exp_level_fraction(&m) * 1000)};
      break;
    }

    case GC_EV_LEVEL_UP: {
      if (e->a != G.shown || !G.present[0]) break;
      bool last = true;
      for (int j = i + 1; j < G.nq; j++)
        if (G.q[j].kind == GC_EV_LEVEL_UP && G.q[j].a == e->a) last = false;
      gc_mon m = before_exp(e->a);
      m.b.level = e->b;
      static bv_mon after;
      after = make_mon(&m, true);
      *ev_new() = (bv_event){.type = BEV_EXP, .battler = BV_PLAYER, .a = last ? (int)(after.exp_ratio * 1000) : 0,
                             .b = e->b, .mon = &after};
      break;
    }

    case GC_EV_EVOLVED:
      if (e->a == G.shown && G.present[0]) {
        const pr_pokemon* p = &G.run.party[e->a].b;
        *ev_new() = (bv_event){.type = BEV_FORM_CHANGE, .battler = BV_PLAYER, .a = p->species, .b = p->form};
        bv_mon m;
        be_mon(&G.run.bt, p, &m);
        bv_mon* sh = bv_mon_of(BV_PLAYER);
        snprintf(sh->name, sizeof sh->name, "%s", m.name);
        sh->max_hp = m.max_hp;
        sh->level = m.level;
        if (m.hp > sh->hp)
          *ev_new() = (bv_event){.type = BEV_HEAL, .battler = BV_PLAYER, .a = m.hp};
        else if (m.hp < sh->hp)
          *ev_new() = (bv_event){.type = BEV_DAMAGE, .battler = BV_PLAYER, .a = m.hp, .b = HIT_RESULT_INDIRECT};
      }
      break;

    case GC_EV_SAVE_POINT:
      if (!bv_playing()) {
        char name[32];
        snprintf(name, sizeof name, "session_%d", G.active_slot);
        char buf[GC_SAVE_MAX];
        size_t len = gc_run_save(&G.run, buf, sizeof buf);
        if (len > 0) {
          save_write(name, buf, len);
        }
      }
      break;

    case GC_EV_MYSTERY: {
      char* t = txt_new();
      snprintf(t, TXT_LEN, "A Mystery Encounter (%d) happens! It is not built yet; the first option is taken.", (int)e->a);
      put_msg(t, false);
      break;
    }

    default:
      break;
  }
  if (G.n_ev == 0) return false;
  play_chunk();
  return true;
}

/* ------------------------------------------------------------------ the driver */

static void pump(void* arg) {
  (void)arg;
  for (;;) {
    if (G.ev_pos < G.n_ev) {
      play_chunk();
      return;
    }
    if (G.qi < G.nq) {
      const int i = G.qi++;
      if (G.skip[i]) {
        continue;
      }
      if (handle_event(i)) return;
      continue;
    }
    screen_step();
    return;
  }
}

static bool submit(const gc_choice* c) {
  const char* why = NULL;
  if (gc_run_choose(&G.run, c, &why)) {
    G.refused_in_row = 0;
    drain();
    return true;
  }
  char buf[200];
  G.st.refused++;
  G.refused_in_row++;
  say(why_text(why, buf, sizeof buf));
  return false;
}

static bool answer(const gc_choice* c) {
  if (!submit(c)) return false;
  pump(NULL);
  return true;
}

/* ---- commands ---- */

static char g_reason[4][160];
static void on_command(bv_command c, void* arg);

static bool resync_field(void) {
  const gc_mon* me = gc_field_mon(&G.run, 0);
  if (!me) return false;
  const int idx = (int)(me - G.run.party);
  if (G.present[0] && G.shown == idx && bv_mon_of(BV_PLAYER)->species == me->b.species) {
    G.n_ev = G.ev_pos = G.n_txt = 0;
    for (int side = 0; side < 2; side++) {
      const gc_mon* m = gc_field_mon(&G.run, side);
      const bv_mon* sh = bv_mon_of(side ? BV_ENEMY : BV_PLAYER);
      if (!m || !G.present[side] || sh->species != m->b.species) continue;
      const int battler = side ? BV_ENEMY : BV_PLAYER;
      if (m->b.hp > sh->hp)
        *ev_new() = (bv_event){.type = BEV_HEAL, .battler = battler, .a = m->b.hp};
      else if (m->b.hp < sh->hp)
        *ev_new() = (bv_event){.type = BEV_DAMAGE, .battler = battler, .a = m->b.hp, .b = HIT_RESULT_INDIRECT};
      if (m->b.status != sh->status) *ev_new() = (bv_event){.type = BEV_STATUS, .battler = battler, .a = m->b.status};
    }
    if (!G.n_ev) return false;
    G.st.hp_resynced++;
    play_chunk();
    return true;
  }
  if (G.swap_replaced)
    G.swap_replaced = false;
  else
    G.st.shown_mismatch++;
  G.n_ev = G.ev_pos = G.n_txt = 0;
  if (G.present[0]) *ev_new() = (bv_event){.type = BEV_WITHDRAW, .battler = BV_PLAYER};
  static bv_mon sent;
  sent = make_mon(me, true);
  *ev_new() = (bv_event){.type = BEV_SUMMON, .battler = BV_PLAYER, .mon = &sent};
  G.present[0] = true;
  G.shown = idx;
  play_chunk();
  return true;
}

static void ask(void) {
  const gc_screen* s = gc_run_screen(&G.run);
  sync_shown();
  bv_command_opts o;
  memset(&o, 0, sizeof o);
  o.can_ball = s->can_ball;
  o.can_switch = s->can_switch;
  o.can_run = s->can_run;
  o.struggle = s->struggle;
  const gc_mon* me = gc_field_mon(&G.run, 0);
  for (int i = 0; i < 4; i++) {
    o.move_ok[i] = s->move_ok[i];
    if (!s->move_ok[i] && s->move_why[i] && me && i < me->b.n_moves) {
      char name[48];
      snprintf(name, sizeof name, "%s", i18n_species_name(me->b.species));
      i18n_t(g_reason[i], sizeof g_reason[i], s->move_why[i], "moveName", i18n_move_name(me->b.moves[i]),
             "pokemonNameWithAffix", name, "pokemonName", name, NULL);
      o.move_why[i] = g_reason[i];
    }
  }
  bv_ask_command(&o, on_command, NULL);
}

static void ask_cb(void* arg) {
  (void)arg;
  screen_step();
}

static void on_switch_pick(int slot, int option, void* arg) {
  (void)arg;
  if (option != PS_SEND_OUT && option != PS_CANCEL) return;
  ps_close();
  if (option == PS_CANCEL || slot < 0 || slot >= G.run.n_party) {
    ask_cb(NULL);
    return;
  }
  gc_choice c = {.kind = GC_CH_POKEMON, .a = G.run.party_order[slot]};
  if (answer(&c)) G.st.switches++;
}

static void on_ball_pick(int ball, void* arg) {
  (void)arg;
  if (ball < 0) {
    ask_cb(NULL);
    return;
  }
  gc_choice c = {.kind = GC_CH_BALL, .a = (int16_t)ball};
  if (answer(&c)) G.st.balls++;
}

static void on_command(bv_command c, void* arg) {
  (void)arg;
  gc_choice ch;
  memset(&ch, 0, sizeof ch);
  switch (c.command) {
    case 0:
      ch.kind = GC_CH_FIGHT;
      ch.a = (int16_t)c.move_slot;
      ch.b = -1;
      if (answer(&ch)) G.st.fights++;
      break;
    case 1: {
      int counts[BALL_TYPES];
      for (int i = 0; i < BALL_TYPES; i++) counts[i] = G.run.pokeballs[i];
      ball_menu_open(counts, on_ball_pick, NULL);
      break;
    }
    case 2:
      ps_open(&G.run.bt, PS_SWITCH, on_switch_pick, NULL);
      break;
    default:
      ch.kind = GC_CH_RUN;
      if (answer(&ch)) G.st.runs_tried++;
      break;
  }
}

static void on_send_pick(int slot, int option, void* arg) {
  (void)arg;
  if (option != PS_SEND_OUT && option != PS_CANCEL) return;
  const gc_screen* s = gc_run_screen(&G.run);
  gc_choice c = {.kind = GC_CH_PICK, .a = -1};
  if (option == PS_SEND_OUT && slot >= 0 && slot < G.run.n_party) c.a = G.run.party_order[slot];
  else if (!s->can_skip) return;
  ps_close();
  const bool swap = G.swap_prompt;
  if (!submit(&c)) return;
  if (swap && c.a >= 0 && c.a == G.shown) {
    G.present[0] = false;
    G.shown = -1;
    G.swap_replaced = true;
  }
  pump(NULL);
}

static void open_switch(const gc_screen* s) {
  ps_open(&G.run.bt, s->can_skip ? PS_SWITCH : PS_FAINT_SWITCH, on_send_pick, NULL);
}

static struct {
  void (*yes)(void*);
  void (*no)(void*);
  void* ctx;
} LC;

static void lh_show(void* host, const char* text, bool prompt, void (*done)(void*), void* done_ctx) {
  (void)host;
  G.n_ev = G.ev_pos = G.n_txt = 0;
  put_msg(text, prompt);
  G.st.bev++;
  bv_play(&G.ev[0], 1, done, done_ctx);
}

static void lh_answered(int yes, void* arg) {
  (void)arg;
  void (*f)(void*) = yes ? LC.yes : LC.no;
  if (f) f(LC.ctx);
}

static void lh_confirm(void* host, void (*yes)(void*), void (*no)(void*), void* ctx) {
  (void)host;
  LC.yes = yes;
  LC.no = no;
  LC.ctx = ctx;
  ui_push_confirm(lh_answered, NULL);
}

static void lh_result(void* host, int slot) {
  (void)host;
  gc_choice c = {.kind = GC_CH_FORGET, .a = (int16_t)slot};
  const char* why = NULL;
  G.learn_drop = true;
  if (!gc_run_choose(&G.run, &c, &why)) {
    G.learn_drop = false;
    G.st.refused++;
    G.st.stuck++;
    return;
  }
  drain();
}

static void lh_end(void* host) {
  (void)host;
  pump(NULL);
}

static void open_learn(const gc_screen* s) {
  static const ps_learn_host host = {lh_show, lh_confirm, lh_result, lh_end, NULL};
  const int slot = slot_of(s->party);
  ps_learn_move(&G.run.bt, slot < 0 ? 0 : slot, s->move, G.run.trainer_id, &host);
}

static gc_choice rc;

static void open_shop_ui(void);

static void on_item_target_picked(int slot, int move_slot, void* arg) {
  ps_close();
  if (slot < 0 || slot >= G.run.n_party) {
    open_shop_ui();
    return;
  }
  rc.b = (int16_t)slot;
  rc.c = (int16_t)move_slot;
  answer(&rc);
}

static void prompt_item_target(const gc_offer* offer) {
  bool move_target = (offer->target == GC_TARGET_MOVE);
  ps_open_item(&G.run, PARTY_MODIFIER, move_target, on_item_target_picked, NULL);
}

static void on_shop_select_reward(void* ctx, int option_idx) {
  (void)ctx;
  const gc_screen* s = gc_run_screen(&G.run);
  if (option_idx < 0 || option_idx >= s->n_options) return;
  memset(&rc, 0, sizeof rc);
  rc.kind = GC_CH_REWARD;
  rc.a = (int16_t)option_idx;
  rc.b = rc.c = -1;
  const gc_offer* offer = &s->options[option_idx];
  shop_ui_close();
  if (offer->target != GC_TARGET_NONE) {
    prompt_item_target(offer);
  } else {
    answer(&rc);
  }
}

static void on_shop_select_shop(void* ctx, int shop_idx) {
  (void)ctx;
  const gc_screen* s = gc_run_screen(&G.run);
  if (shop_idx < 0 || shop_idx >= s->n_shop) return;
  memset(&rc, 0, sizeof rc);
  rc.kind = GC_CH_SHOP;
  rc.a = (int16_t)shop_idx;
  rc.b = rc.c = -1;
  const gc_offer* offer = &s->shop[shop_idx];
  shop_ui_close();
  if (offer->target != GC_TARGET_NONE) {
    prompt_item_target(offer);
  } else {
    answer(&rc);
  }
}

static void on_shop_reroll(void* ctx, bool lock_capsule) {
  (void)ctx;
  shop_ui_close();
  memset(&rc, 0, sizeof rc);
  if (lock_capsule) {
    rc.kind = GC_CH_LOCK_RARITIES;
  } else {
    rc.kind = GC_CH_REROLL;
  }
  answer(&rc);
}

static void on_shop_skip(void* ctx) {
  (void)ctx;
  shop_ui_close();
  memset(&rc, 0, sizeof rc);
  rc.kind = GC_CH_SKIP;
  answer(&rc);
}

static int calc_party_luck(void) {
  int total = 0;
  for (int i = 0; i < G.run.n_party; i++) {
    total += G.run.party[i].luck;
  }
  return total;
}

static void open_shop_ui(void) {
  const gc_screen* s = gc_run_screen(&G.run);
  shop_ui_args args;
  memset(&args, 0, sizeof args);
  args.screen = s;
  args.player_money = G.run.money;
  args.has_lock_capsule = gc_item_stack(&G.run, "LOCK_CAPSULE") > 0;
  args.party_luck = calc_party_luck();

  static const shop_ui_events ev = {
    on_shop_select_reward,
    on_shop_select_shop,
    on_shop_reroll,
    on_shop_skip,
    NULL
  };

  shop_ui_open(&args, &ev);
}

static void on_biome_map_select(void* ctx, int option) {
  (void)ctx;
  biome_map_close();
  gc_choice c = {.kind = GC_CH_BIOME, .a = (int16_t)(option < 0 ? 0 : option)};
  answer(&c);
}

static void open_biome_map(void) {
  const gc_screen* s = gc_run_screen(&G.run);
  biome_map_args ba = {s->biomes, s->n_biomes};
  biome_map_events be = {on_biome_map_select, NULL};
  biome_map_open(&ba, &be);
}

static void mystery_auto(void) {
  const gc_screen* s = gc_run_screen(&G.run);
  gc_choice c = {.kind = GC_CH_MYSTERY};
  for (int i = 0; i < s->n_options && i < 8; i++)
    if (s->option_ok[i]) {
      c.a = (int16_t)i;
      break;
    }
  for (int i = 0; i < G.run.n_party; i++)
    if (G.run.party[i].b.hp > 0) {
      c.b = (int16_t)i;
      break;
    }
  answer(&c);
}

/* ---- the run ends ---- */

static void end_run(int kind) {
  if (kind == GC_SCREEN_CLEAR) {
    G.st.clears++;
    clear_ui_open((int)G.run.wave, G.run.score, G.run.money, G.active_slot);
  } else {
    G.st.game_overs++;
    game_over_ui_open((int)G.run.wave, G.run.score, G.active_slot);
  }
}

static void screen_step(void) {
  const gc_screen* s = gc_run_screen(&G.run);
  G.st.screens[s->kind & 15]++;
  switch (s->kind) {
    case GC_SCREEN_COMMAND:
      if (!resync_field()) ask();
      break;
    case GC_SCREEN_SWITCH:
      open_switch(s);
      break;
    case GC_SCREEN_LEARN_MOVE:
      open_learn(s);
      break;
    case GC_SCREEN_EVOLVE: {
      gc_choice c = {.kind = GC_CH_EVOLVE};
      answer(&c);
      break;
    }
    case GC_SCREEN_REWARD:
      open_shop_ui();
      break;
    case GC_SCREEN_BIOME:
      open_biome_map();
      break;
    case GC_SCREEN_MYSTERY:
      mystery_auto();
      break;
    case GC_SCREEN_GAME_OVER:
    case GC_SCREEN_CLEAR:
      end_run(s->kind);
      break;
    default:
      G.st.stuck++;
      break;
  }
}

/* ---- a new run ---- */

static int random_team(gc_starter* out) {
  int budget = gc_starter_budget(GAME_MODES_CLASSIC), n = 0;
  for (int tries = 0; tries < 400 && n < 6 && budget > 0; tries++) {
    const pr_species* sp = &pr_species_table[rnd(PR_SPECIES_COUNT)];
    const int cost = gc_starter_cost(sp->id);
    if (cost <= 0 || cost > budget) continue;
    bool dup = false;
    for (int k = 0; k < n; k++) dup |= out[k].species == sp->id;
    if (dup) continue;
    gc_starter* s = &out[n++];
    memset(s, 0, sizeof *s);
    s->species = (uint16_t)sp->id;
    s->ability_index = (uint8_t)rnd(2);
    s->nature = (uint8_t)rnd(25);
    s->female = sp->male_permille >= 0 && (int)rnd(1000) >= sp->male_permille;
    s->tera_type = -1;
    for (int i = 0; i < 6; i++) s->ivs[i] = (uint8_t)rnd(32);
    budget -= cost;
  }
  if (n == 0) {
    for (int k = 0; k < gc_default_starter_count && k < 3; k++) {
      memset(&out[n], 0, sizeof out[n]);
      out[n].species = gc_default_starters[k];
      out[n].tera_type = -1;
      for (int i = 0; i < 6; i++) out[n].ivs[i] = 20;
      n++;
    }
  }
  return n;
}

static void new_run(void) {
  G.st.runs++;
  G.present[0] = G.present[1] = false;
  G.shown = -1;
  G.refused_in_row = 0;
  G.learn_drop = false;
  G.rng = 0x9E3779B97F4A7C15ull ^ ((uint64_t)G.cfg.entropy * 0xD1B54A32D192ED03ull) ^ ((uint64_t)G.st.runs * 0x2545F4914F6CDD1Dull);
  rnd(1);
  gc_run_new(&G.run, GAME_MODES_CLASSIC, G.st.runs == 1 ? G.cfg.seed : NULL, G.cfg.entropy + (uint32_t)G.st.runs);
  gc_starter team[6];
  int n = 0;
  if (G.st.runs == 1 && G.cfg.n_starters > 0) {
    for (; n < G.cfg.n_starters && n < 6; n++) {
      memset(&team[n], 0, sizeof team[n]);
      team[n].species = (uint16_t)G.cfg.starters[n];
      team[n].tera_type = -1;
      for (int i = 0; i < 6; i++) team[n].ivs[i] = 20;
    }
  } else {
    n = random_team(team);
  }
  const char* why = NULL;
  if (!n || !gc_run_set_starters(&G.run, team, n, &why)) {
    G.st.stuck++;
    return;
  }
  drain();
  pump(NULL);
}

void rs_start(const rs_config* cfg) {
  memset(&G.st, 0, sizeof G.st);
  G.cfg = *cfg;
  new_run();
}

bool rs_load_slot(int slot) {
  rs_set_slot(slot);
  char name[32];
  snprintf(name, sizeof name, "session_%d", slot);
  char* buf = NULL;
  size_t len = 0;
  bool used_bak = false;
  save_result res = save_read(name, &buf, &len, &used_bak);
  if (res != SAVE_OK || !buf) return false;

  const char* why = NULL;
  if (!gc_run_load(&G.run, buf, len, &why)) {
    free(buf);
    return false;
  }
  free(buf);

  G.present[0] = G.present[1] = false;
  G.shown = -1;
  G.refused_in_row = 0;
  G.learn_drop = false;

  drain();
  pump(NULL);
  return true;
}

const rs_stats* rs_stats_get(void) { return &G.st; }
const gc_run* rs_run(void) { return &G.run; }
