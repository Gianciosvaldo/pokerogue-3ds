/* The app's run driver (see run_screen.h). */
#include "run_screen.h"
#include "run_exp.h"
#include "egg_hatch_screen.h"

#include <stdio.h>
#include <string.h>

#include "ball_menu.h"
#include "bgm.h"
#include "gm_enum_names.h"
#include "trainer_art.h"
#include "pr_data.h"
#include "battle_anim.h"
#include "battle_events.h"
#include "battle_view.h"
#include "combat_observer.h"
#include "custom_run.h"
#include "gm_item_ui.h"
#include "i18n.h"
#include "menus.h"
#include "party_screen.h"
#include "pr_enums.h"
#include "reward_host.h"
#include "run_menu.h"
#include "run_saves.h"
#include "run_text.h"
#include "shop_ui.h"
#include "starter_data.h"
#include "ui.h"
#include "ui_widgets.h"

extern int gc_item_stack(const gc_run *run, const char *key);

#define EV_CAP 400      /* screen events one game-core event can make */
#define CHUNK 150       /* battle_view queues at most 160 at a time */
/* battle-core rolls the three shake checks (attempt_capture_phase) and keeps only whether the ball held: the number of shakes
 * before a ball breaks free is not in its events. A ball that breaks free wobbles once (a stand-in: it can be 0, 1 or 2). */
#define BREAK_FREE_SHAKES 1
#define TXT_CAP 12
#define TXT_LEN 320

static struct {
  gc_run run;
  rs_config cfg;
  rs_stats st;
  int scan_wave, scan_next, scan_target;
  bool scan_enabled;
  int scan_stats[12];
  uint64_t rng;                 /* the stand-in team's */
  /* the events of the current drain, handled one by one (lazily: the screen must have played the
   * earlier ones before a later one is converted) */
  gc_event q[GC_MAX_EVENTS];
  bool skip[GC_MAX_EVENTS];
  int nq, qi;
  bool swap_prompt;             /* the drain has "swap or release" for a caught Pokemon */
  bool release_drop;            /* the next drain follows a release picked on the party screen, which said goodbye itself */
  int ball;                     /* PokeballType of the ball the player just threw (the turn's first events start with its throw), -1 none */
  bool learn_drop;              /* the next drain starts with the learn dialog's own messages */
  bool swap_replaced;           /* a caught Pokemon replaced the one on the field: the screen must show the new one */
  /* what the battle screen shows */
  bool present[4];              /* a Pokemon of each side is out */
  int shown_ally;
  int target_values[4],target_count,ball_target;
  char target_labels[4][96];
  const char *target_options[4];
  int shown;                    /* party index of the player's Pokemon on screen, -1 none */
  bool fade_done;               /* a fade is already at black (a new run): the next wave doesn't fade again */
  bool next_encounter;          /* this wave is the next one of the same biome: no fade, the enemy side slides in over the old one */
  int wave_ev;                  /* the WAVE event being shown */
  uint32_t drain_no;            /* drain() calls of this run, for the combat observer (one number per drain, never reused) */
  /* the screen events being played */
  bv_event ev[EV_CAP];
  char txt[TXT_CAP][TXT_LEN];
  int n_ev, ev_pos, n_txt;
  int refused_in_row;
  /* saving (cfg.save): game-core allows it from GC_EV_SAVE_POINT until the battle's first turn runs */
  bool save_due;                /* a SAVE_POINT came and the run has not been written since */
  bool save_failed_now;         /* the write that just ran failed */
  bool save_failed_told;        /* the player was told the SD card refused a write (told again after one succeeds) */
  int step_timer;               /* a postponed screen_step (ui_after id), 0 = none */
  /* a loaded run (rs_resume): game-core's load makes the wave and the player's summon but not the trainer's intro or the
   * foe's send-out (they were shown when the battle began, before the save) */
  bool resume_wave;             /* the next WAVE event is the loaded battle's */
  bool resume_foe;              /* the foe may be on the field in the run and not on the screen yet */
  cs_defeat defeat;             /* a custom run's attributed defeat (rs_custom_defeat) */
  bool have_defeat;
} G = {.shown = -1, .ball = -1};

static int displayed_slot(int field){return (field%2)*2+(field>=2);}
static int shown_battler(int party){if(G.shown==party&&G.present[0])return 0;if(G.shown_ally==party&&G.present[2])return 1;return -1;}
static const gc_mon* command_mon(void){int field=G.run.screen.slot==1?1:0;const pr_pokemon *p=G.run.bt.field[field];
  for(int i=0;i<G.run.n_party;i++)if(&G.run.party[i].b==p)return &G.run.party[i];return NULL;}
static void pump(void* arg);
static void screen_step(void);
static void refresh_item_bars(void);

/* ------------------------------------------------------------------ helpers */

/* Everything the driver remembers about the scene on screen and the answer in flight: a new run, or a loaded one, starts
 * from nothing. */
static void reset_driver(void) {
  if (G.step_timer) ui_timer_cancel(G.step_timer);
  G.step_timer = 0;
  memset(G.present,0,sizeof G.present);
  G.shown = G.shown_ally = -1;
  G.fade_done = false;
  G.wave_ev = 0;
  G.drain_no = 0;
  G.nq = G.qi = 0;
  G.n_ev = G.ev_pos = G.n_txt = 0;
  G.swap_prompt = G.learn_drop = G.swap_replaced = G.release_drop = false;
  G.ball = -1;
  G.refused_in_row = 0;
  G.save_due = G.save_failed_told = false;
  G.resume_wave = G.resume_foe = false;
}

static bool msg_is(const gc_event* e, const char* key) {
  return e->kind == GC_EV_MESSAGE && e->msg.key && !strcmp(e->msg.key, key);
}

static int pidx_of(const pr_pokemon* p) {
  if (!p) return -1;
  const gc_mon* m = (const gc_mon*)p; /* pr_pokemon b is gc_mon's first member */
  if (m < G.run.party || m >= G.run.party + G.run.n_party) return -1;
  return (int)(m - G.run.party);
}

/* party index -> the party screen's slot (upstream's party order) */
static int slot_of(int party_index) {
  for (int k = 0; k < G.run.n_party; k++)
    if (G.run.party_order[k] == party_index) return k;
  return -1;
}

/* A party member as it was before the experience this drain still has to show (the run is ahead of the screen) */
static gc_mon before_exp(int idx) {
  return rs_exp_before_pending(&G.run.party[idx],idx,G.q+G.qi,G.nq-G.qi);
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

/* "RARE_CANDY" -> "Rare Candy" (stand-in for the reward's own name) */
static const char* pretty_key(const char* key, char* out, int n) {
  int j = 0;
  bool up = true;
  for (int i = 0; key && key[i] && j < n - 1; i++) {
    char c = key[i];
    if (c == '_') {
      out[j++] = ' ';
      up = true;
    } else {
      out[j++] = up ? c : (char)(c >= 'A' && c <= 'Z' ? c + 32 : c);
      up = false;
    }
  }
  out[j] = 0;
  return out;
}

/* game-core's reason for a refusal: an i18n key ("partyUiHandler:noEnergy") or a plain sentence */
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

/* A line with the trainer's name in it ("X would like to battle!"). game-core's name is i18next nesting text
 * ("$t(trainerClasses:aceTrainer) $t(...)"): i18n resolves it when it is the value of an argument; when it comes out
 * unresolved the line says "The trainer". */
static const char* trainer_line(const char* key, int which, const char* mon, char* out, int n) {
  const char* name = G.run.trainer_names[which];
  i18n_t(out, n, key, "trainerName", name ? name : "", "pokemonName", mon ? mon : "", NULL);
  if (!name || strstr(out, "$t(")) i18n_t(out, n, key, "trainerName", "The trainer", "pokemonName", mon ? mon : "", NULL);
  return out;
}

/* ------------------------------------------------------------------ building screen events */

/* LevelUpPhase's prevStats and the stats it shows, for the GC_EV_LEVEL_UP at q[i]. The run's party is already past the whole
 * batch of events, so both are worked out again on a copy: at the level before (the event's c) and at the new level, and as the
 * species before an evolution that comes later in the batch. Pure stat formula (pr_calculate_stats), nothing in the run moves. */
static bool level_up_stats(int i, int out[12]) {
  const gc_event* e = &G.q[i];
  if (e->c < 1 || e->c >= e->b) return false;
  static pr_pokemon p; /* a big struct: off the 3DS stack */
  p = G.run.party[e->a].b;
  for (int j = i + 1; j < G.nq; j++)
    if (G.q[j].kind == GC_EV_EVOLVED && G.q[j].a == e->a) {
      p.species = (uint16_t)G.q[j].b;
      break;
    }
  p.level = e->c;
  pr_calculate_stats(&G.run.bt, &p);
  for (int s = 0; s < 6; s++) out[s] = p.stats[s];
  p.level = e->b;
  pr_calculate_stats(&G.run.bt, &p);
  for (int s = 0; s < 6; s++) out[6 + s] = p.stats[s];
  return true;
}

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
  if (!m->key) return false; /* text not ported in battle-core: nothing to show */
  char* t = txt_new();
  be_format_message(m,t,TXT_LEN);
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

/* Shows one message, then goes on with the driver */
static void say(const char* text) {
  G.n_ev = G.ev_pos = G.n_txt = 0;
  put_msg(text, false);
  play_chunk();
}

/* ------------------------------------------------------------------ events */

/* PartyUiHandler.getReleaseMessage's ten farewells (game-core's copy of what the party screen shows when a member is released) */
static bool is_farewell(const gc_event* e) {
  static const char* const KEYS[] = {"goodbye", "byebye", "farewell", "soLong", "thisIsWhereWePart",
                                     "illMissYou", "illNeverForgetYou", "untilWeMeetAgain", "sayonara", "smellYaLater"};
  if (e->kind != GC_EV_MESSAGE || !e->msg.key || strncmp(e->msg.key, "partyUiHandler:", 15)) return false;
  for (size_t i = 0; i < sizeof KEYS / sizeof *KEYS; i++)
    if (!strcmp(e->msg.key + 15, KEYS[i])) return true;
  return false;
}

static void drain(void) {
  G.nq = G.qi = 0;
  G.swap_prompt = false;
  memset(G.skip, 0, sizeof G.skip);
  while (G.nq < GC_MAX_EVENTS && gc_run_next_event(&G.run, &G.q[G.nq])) G.nq++;
  /* The combat history takes the raw events here, once per drain, before anything is converted or played: the screen's timing,
   * its redraws and its skips cannot count a turn twice or leave one out. */
  co_observe(++G.drain_no, G.q, G.nq, G.run.battle_events, G.run.n_battle_events, G.run.battle_events_overflow, G.run.ev_overflow);
  const gc_screen* s = gc_run_screen(&G.run);

  /* the learn dialog (menus' learn_move_start) shows what game-core says around its screen: drop
   * game-core's copies, both before the screen and after the answer */
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
  const bool release_drop = G.release_drop;
  G.release_drop = false;
  for (int i = 0; i < G.nq; i++) {
    const gc_event* e = &G.q[i];
    /* the app's marker for the full-party question: no text of its own (the locale has no such key) */
    if (msg_is(e, "menu:swapOrRelease")) {
      G.swap_prompt = true;
      G.skip[i] = true;
    }
    /* the party screen already said goodbye when the member was picked (menus' do_release): once is enough */
    if (release_drop && is_farewell(e)) {
      G.skip[i] = true;
      G.st.dup_dropped++;
    }
    if (s->kind == GC_SCREEN_LEARN_MOVE && (msg_is(e, "battle:learnMovePrompt") || msg_is(e, "battle:learnMoveLimitReached")))
      G.skip[i] = true;
    /* a battle's send-out text is in battle-core's events and again as a game-core message right after them */
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

/* The turn's events are the battle as it stands only when the run stopped right after them */
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
  out.pokeball = m->pokeball;
  if (player) out.exp_ratio = (float)gc_exp_level_fraction(m);
  return out;
}

static void preload_moves(void) {
  for (int i = 0; i < G.run.n_party; i++)
    for (int k = 0; k < PR_N_MOVES(&G.run.party[i].b); k++) ba_preload_move(PR_MOVES(&G.run.party[i].b)[k]);
  for (int i = 0; i < G.run.n_enemy; i++)
    for (int k = 0; k < PR_N_MOVES(&G.run.enemy[i].b); k++) ba_preload_move(PR_MOVES(&G.run.enemy[i].b)[k]);
}

/* At black: the new wave's scene, and the intro events (already in G.ev up to the call) */
static void wave_black(void* arg) {
  (void)arg;
  const gc_event* w = &G.q[G.wave_ev];
  const bool trainer = w->c == BATTLE_TYPE_TRAINER;
  const bool mystery = w->c == BATTLE_TYPE_MYSTERY_ENCOUNTER;
  /* a custom rematch is on offer: the ordinary enemy in the run is about to be replaced (or kept), so nobody is shown on the enemy side
   * yet; it appears when the battle starts, after the answer */
  const bool offer = G.cfg.custom && gc_run_screen(&G.run)->kind == GC_SCREEN_CUSTOM_OFFER;
  const bool loaded = G.resume_wave;
  G.resume_wave = false;
  img_drop_prefix("images/pokemon/");
  img_drop_prefix("images/trainer/");
  if (!G.next_encounter) img_drop_prefix("images/arenas/"); /* the same biome's platforms are still on screen, sliding out */
  ba_drop_moves();
  preload_moves();
  bgm_on_wave(&G.run, (int)w->a, (int)w->b, (int)w->c); /* at black: the music starts here, never while something moves */
  ps_set_run(&G.run); /* the summary's ID No. and where each Pokemon was met */

  bv_setup s;
  memset(&s, 0, sizeof s);
  s.biome = w->b;
  s.trainer = trainer;
  if (trainer) for (int k=0;k<2;k++) {
    const char* key=gm_trainer_sprite_key(G.run.trainer_type,G.run.trainer_variant,k);
    if (key && *key) snprintf(s.trainer_keys[k],sizeof s.trainer_keys[k],"images/trainer/%s",key);
  }
  /* When only the old right ally survives, the next encounter promotes it to
   * the lead. Carry its already-visible sprite into slot 0 rather than duplicating
   * it or waiting for a SummonPhase that native correctly does not run. */
  int primary=pidx_of(G.run.bt.field[0]);
  if(G.present[2]&&G.shown_ally>=0&&G.shown_ally==primary&&(!G.present[0]||G.shown!=primary)){
    s.player=*bv_mon_of(1);s.player_present=true;s.next_encounter=G.next_encounter;
    G.shown=G.shown_ally;G.present[0]=true;G.present[2]=false;G.shown_ally=-1;
  }else if (G.present[0] && G.shown >= 0) {
    s.player = *bv_mon_of(BV_PLAYER);
    s.player_present = true;
    s.next_encounter = G.next_encounter;
  } else if (G.run.n_party > 0) {
    s.player = make_mon(&G.run.party[G.run.party_order[0]], true);
  }
  if (!trainer && !mystery && !offer && G.run.n_enemy > 0 && (!loaded||(G.run.bt.field[2]&&G.run.bt.field[2]->on_field&&G.run.bt.field[2]->hp>0))) s.enemy = make_mon(&G.run.enemy[0], false);
  s.doubles=G.run.double_battle;
  if(G.run.double_battle&&G.present[2]&&G.shown_ally>=0){s.player_ally=*bv_mon_of(1);s.player_ally_present=true;}
  if(!trainer&&!mystery&&!offer&&G.run.double_battle&&G.run.n_enemy>1&&(!loaded||(G.run.bt.field[3]&&G.run.bt.field[3]->on_field&&G.run.bt.field[3]->hp>0)))s.enemy_ally=make_mon(&G.run.enemy[1],false);
  bv_begin(&s);
  refresh_item_bars();
  G.present[0] = s.player_present;
  G.present[1] = !trainer && !mystery && !offer && s.enemy.species > 0;
  G.present[2]=s.player_ally_present;G.present[3]=s.enemy_ally.species>0;
  if(!G.present[2])G.shown_ally=-1;
  if (!G.present[0]) G.shown = -1;

  /* the intro: arenas slide in, then the encounter's message */
  G.n_ev = G.ev_pos = G.n_txt = 0;
  bv_event* e = ev_new();
  char* t = txt_new();
  t[0] = 0;
  if (offer) {
    snprintf(t, TXT_LEN, "The %s that beat you wants a rematch!", i18n_species_name(cr_offer_species()));
  } else if (trainer || mystery) {
    /* the first game-core message after the wave is the encounter text ("X would like to battle!") */
    for (int j = G.wave_ev + 1; j < G.nq && j <= G.wave_ev + 5; j++) {
      if (G.q[j].kind == GC_EV_MESSAGE && !G.skip[j]) {
        if (G.q[j].msg.key) be_format_message(&G.q[j].msg,t,TXT_LEN);
        G.skip[j] = true;
        break;
      }
    }
    /* a loaded battle has no encounter message in the run's events: it is made here */
    if (trainer && loaded && !t[0]) trainer_line("battle:trainerAppeared", GC_TNAME_TITLED, NULL, t, TXT_LEN);
    if (mystery && !t[0]) snprintf(t, TXT_LEN, "A strange encounter!");
  } else {
    for (int j = G.wave_ev + 1; j < G.nq && j <= G.wave_ev + 5; j++)
      if (G.q[j].kind == GC_EV_MESSAGE && (msg_is(&G.q[j], "battle:singleWildAppeared")||msg_is(&G.q[j],"battle:doubleWildAppeared"))) {
        G.skip[j] = true; /* the intro shows it */
        break;
      }
  }
  *e = (bv_event){.type = BEV_INTRO_WILD, .battler = BV_ENEMY, .a = trainer || mystery || offer ? 1 : 0, .text = t};
  play_chunk();
}

static void begin_wave(int i) {
  G.wave_ev = i;
  G.st.waves++;
  /* The next wave of the same biome with the lead on the field (upstream's NextEncounterPhase) doesn't fade: the enemy platform and
   * the new enemy slide in over the old ones and the player's side stays. A new biome, a new run and a loaded one still go through
   * black. */
  G.next_encounter = !G.fade_done && ui_depth() > 0 && G.present[0] && G.shown >= 0 && bv_biome() == (int)G.q[i].b;
  if (G.next_encounter || G.fade_done || ui_depth() == 0) {
    G.fade_done = false;
    wave_black(NULL);
  } else {
    const gc_event* w = &G.q[i];
    bgm_on_wave_coming(&G.run, (int)w->a, (int)w->b, (int)w->c); /* the old music goes down with the screen */
    ui_fade(250, 100, 250, wave_black, NULL);
  }
}

/* The Pokemon on the field, synced into the screen's copy: moves and PP (the fight menu reads them there),
 * name and level */
static void sync_primary_shown(void) {
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
    shown->move_power[i]=m.move_power[i];
  }
  snprintf(shown->name, sizeof shown->name, "%s", m.name);
  shown->n_types = m.n_types;
  for (int i = 0; i < 3; i++) shown->types[i] = m.types[i];
}

static void sync_shown(void){
  sync_primary_shown();
  for(int k=0;k<4;k++)if(G.run.bt.field[k]&&G.present[displayed_slot(k)])
    memcpy(bv_mon_of(k)->stat_stages,G.run.bt.field[k]->stat_stages,sizeof bv_mon_of(k)->stat_stages);
  const gc_mon *m=NULL;for(int i=0;i<G.run.n_party;i++)if(&G.run.party[i].b==G.run.bt.field[1])m=&G.run.party[i];
  if(m&&G.present[2]){bv_mon live=make_mon(m,true),*shown=bv_mon_of(1);shown->level=live.level;shown->n_moves=live.n_moves;
    memcpy(shown->moves,live.moves,sizeof shown->moves);memcpy(shown->pp_left,live.pp_left,sizeof shown->pp_left);memcpy(shown->pp_max,live.pp_max,sizeof shown->pp_max);memcpy(shown->move_power,live.move_power,sizeof shown->move_power);
    memcpy(shown->stat_stages,live.stat_stages,sizeof shown->stat_stages);shown->n_types=live.n_types;memcpy(shown->types,live.types,sizeof shown->types);}
}

/* Converts and starts the events of one game-core event. True when something is playing (the driver
 * goes on when it ends), false to go straight on to the next event. */
static bool handle_event(int i) {
  const gc_event* e = &G.q[i];
  if(e->kind<GC_EV_COUNT)G.st.events[e->kind]++;
  G.n_ev = G.ev_pos = G.n_txt = 0;
  switch (e->kind) {
    case GC_EV_EGG_HATCH:
      egg_hatch_open(e,pump,NULL);
      return true;
    case GC_EV_MESSAGE:
      if (msg_is(e, "battle:trainerDefeated")) bgm_on_trainer_defeated(&G.run); /* TrainerVictoryPhase.start */
      if (put_gc_msg(&e->msg) && e->prompt) G.ev[G.n_ev - 1].prompt = true;
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
      v.mon[2]=*bv_mon_of(1);v.mon[3]=*bv_mon_of(3);
      v.present[2]=G.present[2];v.present[3]=G.present[3];
      v.exp_ratio = exp_ratio_cb;
      v.level_of = level_of_cb;
      const pr_pokemon* enemy_ptr[6];
      for (int k = 0; k < G.run.n_enemy && k < 6; k++) enemy_ptr[k] = &G.run.enemy[k].b;
      v.enemy_party = enemy_ptr;
      v.n_enemy_party = G.run.n_enemy < 6 ? G.run.n_enemy : 6;
      if (G.ball >= 0 && (G.present[1]||G.present[3])) {
        /* the ball is thrown first in the turn (commands other than FIGHT go first). Whether it holds is the run's: battle-core
         * shows "Gotcha!" only for a catch. How many shakes a ball that breaks free made is not in the events (see
         * BREAK_FREE_SHAKES) */
        bool caught = false;
        for (int k = 0; k < e->b; k++) {
          const pr_event* be = &G.run.battle_events[e->a + k];
          if (be->type == PR_EV_TEXT && be->msg.key && !strcmp(be->msg.key, "battle:pokemonCaught")) caught = true;
        }
        bv_event* t = ev_new();
        *t = (bv_event){.type = BEV_CATCH, .battler = G.ball_target, .a = G.ball, .b = caught ? 3 : BREAK_FREE_SHAKES, .caught = caught};
        G.st.throws++;
        if (caught) G.st.catches++;
      }
      G.ball = -1;
      G.n_ev += be_convert_events(&G.run.bt, &G.run.battle_events[e->a], e->b, settled_after(i), &v, G.ev + G.n_ev, EV_CAP - G.n_ev);
      G.present[0] = v.present[0];
      G.present[1] = v.present[1];
      G.present[2]=v.present[2];G.present[3]=v.present[3];
      /* a trainer sends out its first Pokemon (battle-core's start of the battle): Battle.started, and the encounter music gives way to the
       * battle music. Here, before the events play: upstream's doSummon plays the music, then the send-out. */
      for (int k = 0; k < G.n_ev; k++)
        if (G.ev[k].type == BEV_SUMMON && G.ev[k].battler == BV_ENEMY) {
          bgm_on_foe_sent_out(&G.run);
          break;
        }
      if (v.sent[0]) G.shown = pidx_of(v.sent[0]);
      if(v.sent[2])G.shown_ally=pidx_of(v.sent[2]);
      G.st.not_found += v.not_found;
      break;
    }

    case GC_EV_SUMMON:
    case GC_EV_RECALL: {
      const int side=e->a,field=side*2+(e->c==1?1:0),index=displayed_slot(field);
      int *shown=field==1?&G.shown_ally:&G.shown;
      const bool recall=e->kind==GC_EV_RECALL;
      if(recall){
        if(G.present[index]){*ev_new()=(bv_event){.type=BEV_WITHDRAW,.battler=field};G.present[index]=false;if(!side)*shown=-1;}
        break;
      }
      if(G.present[index]){
        if(side||*shown==e->b)break;
        *ev_new()=(bv_event){.type=BEV_WITHDRAW,.battler=field};G.present[index]=false;
      }
      static bv_mon sent;
      const gc_mon* m = side == 0 ? &G.run.party[e->b] : &G.run.enemy[e->b];
      sent = make_mon(m, side == 0);
      /* the send-out text follows the summon in game-core's events */
      char* text = NULL;
      if (i + 1 < G.nq && (msg_is(&G.q[i + 1], "battle:playerGo") || msg_is(&G.q[i + 1], "battle:trainerSendOut"))) {
        text = txt_new();
        be_format_message(&G.q[i+1].msg,text,TXT_LEN);
        G.skip[i + 1] = true;
      }
      if (side == 1 && text) put_msg(text, false);
      *ev_new() = (bv_event){.type = BEV_SUMMON, .battler = field, .mon = &sent, .text = side ? NULL : text};
      G.present[index] = true;
      if (side == 0) *shown = e->b;
      break;
    }

    case GC_EV_HP: {
      int field=shown_battler(e->b);if(e->a!=0||field<0)break;
      bv_mon* sh = bv_mon_of(field);
      /* a Vitamin or a Mint changes the maximum HP and game-core only says the new HP: the bar's maximum follows the Pokemon */
      const int max_hp = G.run.party[e->b].b.stats[STAT_HP];
      if (max_hp > 0 && max_hp != sh->max_hp) {
        *ev_new() = (bv_event){.type = BEV_MAX_HP, .battler = field, .a = max_hp};
        sh->max_hp = max_hp;
      }
      if (e->c > sh->hp)
        *ev_new() = (bv_event){.type = BEV_HEAL, .battler = field, .a = e->c};
      else if (e->c < sh->hp)
        *ev_new() = (bv_event){.type = BEV_DAMAGE, .battler = field, .a = e->c, .b = HIT_RESULT_INDIRECT};
      break;
    }

    case GC_EV_EXP: {
      int field=shown_battler(e->a);if(field<0)break;
      /* a level up follows: its event runs the bar to full and starts the next level */
      if (i + 1 < G.nq && G.q[i + 1].kind == GC_EV_LEVEL_UP && G.q[i + 1].a == e->a) break;
      *ev_new() = (bv_event){.type = BEV_EXP, .battler = field, .a = (int)(gc_exp_level_fraction(&G.run.party[e->a]) * 1000)};
      break;
    }

    case GC_EV_LEVEL_UP: {
      if (e->a < 0 || e->a >= G.run.n_party) break;
      int field=shown_battler(e->a);
      if(field>=0) {
        bool last = true;
        for (int j = i + 1; j < G.nq; j++)
          if (G.q[j].kind == GC_EV_LEVEL_UP && G.q[j].a == e->a) last = false;
        static bv_mon after;
        after = make_mon(&G.run.party[e->a], true);
        *ev_new() = (bv_event){.type = BEV_EXP, .battler = field, .a = last ? (int)(after.exp_ratio * 1000) : 0,
                               .b = e->b, .mon = &after};
      }
      /* LevelUpPhase: "X grew to Lv. N!" with the fanfare, then the stat window (prevStats against the new stats), for any
       * party member */
      if (i + 1 < G.nq && msg_is(&G.q[i + 1], "battle:levelUp")) {
        static int stats[12];
        char* t = txt_new();
        be_format_message(&G.q[i+1].msg,t,TXT_LEN);
        G.skip[i + 1] = true;
        const bool ok = level_up_stats(i, stats);
        *ev_new() = (bv_event){.type = BEV_LEVEL_UP, .battler = -1, .text = t, .stats = ok ? stats : NULL};
      }
      break;
    }

    case GC_EV_EVOLVED: {
      int field=shown_battler(e->a);
      if(field>=0) {
        const pr_pokemon* p = &G.run.party[e->a].b;
        *ev_new() = (bv_event){.type = BEV_FORM_CHANGE, .battler = field, .a = p->species, .b = p->form};
        bv_mon m;
        be_mon(&G.run.bt, p, &m);
        bv_mon* sh = bv_mon_of(field);
        snprintf(sh->name, sizeof sh->name, "%s", m.name);
        sh->max_hp = m.max_hp;
        sh->level = m.level;
        /* the new species' stats move the HP with the maximum */
        if (m.hp > sh->hp)
          *ev_new() = (bv_event){.type = BEV_HEAL, .battler = field, .a = m.hp};
        else if (m.hp < sh->hp)
          *ev_new() = (bv_event){.type = BEV_DAMAGE, .battler = field, .a = m.hp, .b = HIT_RESULT_INDIRECT};
      }
      break;
    }

    case GC_EV_MYSTERY: {
      char* t = txt_new();
      snprintf(t, TXT_LEN, "A Mystery Encounter (%d) happens! It is not built yet; the first option is taken.", (int)e->a);
      put_msg(t, false);
      break;
    }

    case GC_EV_SAVE_POINT:
      /* nothing to show; the run is written when the scene is up and the command is about to be asked (save_point) */
      G.save_due = G.cfg.save;
      break;

    default: /* TRAINER, BIOME, TURN, MONEY, MOVE_LEARNED: nothing to show (the messages say it) */
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

/* Gives game-core the player's answer and takes in the events it queued (the driver goes on with pump). A refusal is
 * told to the player, who then answers again. */
static void write_save(void);

static bool submit(const gc_choice* c) {
  const char* why = NULL;
  /* the first turn ends the save point: a save postponed until now is written before the answer goes in */
  if (G.save_due && gc_run_can_save(&G.run)) write_save();
  G.save_due = false;
  if (gc_run_choose(&G.run, c, &why)) {
    G.refused_in_row = 0;
    drain();
    return true;
  }
  char buf[200];
  G.st.refused++;
  G.refused_in_row++;
  say(why_text(why, buf, sizeof buf)); /* the player answers again; a person is never locked out */
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

/* The screen shows the Pokemon the run has on the field. A caught Pokemon that replaces the one out leaves the screen
 * behind game-core's field: bring them together before the command is asked. True when events are playing. */
/* A loaded battle's foe: a trainer's or an encounter's Pokemon is on the field in the run and was never sent out on the
 * screen (a wild one is there from the intro). True when events are playing. */
static bool send_out_foe(void) {
  G.n_ev = G.ev_pos = G.n_txt = 0;
  static bv_mon sent[2];
  for(int field=2;field<4;field++){
    const pr_pokemon *p=G.run.bt.field[field];int index=displayed_slot(field);
    if(!p||!p->on_field||p->hp<=0||G.present[index])continue;
    be_mon(&G.run.bt,p,&sent[field-2]);
    if(G.run.battle_type==BATTLE_TYPE_TRAINER){char *t=txt_new();trainer_line("battle:trainerSendOut",field==3?GC_TNAME_PARTNER:GC_TNAME_TRAINER,sent[field-2].name,t,TXT_LEN);put_msg(t,false);bgm_on_foe_sent_out(&G.run);}
    *ev_new()=(bv_event){.type=BEV_SUMMON,.battler=field,.mon=&sent[field-2]};G.present[index]=true;
  }
  if(!G.n_ev)return false;play_chunk();return true;
}

static bool resync_field(void) {
  /* Illusion can begin/end between waves without a move event, on either
   * secondary slot as well as on the lead. Preserve bars while replacing the look. */
  for(int field=1;field<4;field++){
    pr_pokemon *p=G.run.bt.field[field];bv_mon *shown=bv_mon_of(field);
    if(!p||!p->on_field||p->hp<=0||!G.present[displayed_slot(field)]||!(G.run.bt.deviations&PR_DEV_ILLUSION))continue;
    const int species=shown->species,target=pr_shown_form(&G.run.bt,p)->species;
    if(species!=target&&(p->illusion.active||shown->illusion||species==pr_pokemon_form(p)->species||species==pr_apparent_form(p,true)->species)){
      static bv_mon look;bv_mon live;be_mon(&G.run.bt,p,&live);look=*shown;
      look.illusion=live.illusion;look.species=live.species;look.form=live.form;look.gender=live.gender;look.female=live.female;look.sprite_scale=live.sprite_scale;
      look.n_types=live.n_types;memcpy(look.types,live.types,sizeof look.types);memcpy(look.name,live.name,sizeof look.name);
      G.n_ev=G.ev_pos=G.n_txt=0;*ev_new()=(bv_event){.type=BEV_ILLUSION,.battler=field,.mon=&look};play_chunk();return true;
    }
  }
  const gc_mon* me = gc_field_mon(&G.run, 0);
  if (!me||me->b.hp<=0||!me->b.on_field) return false;
  if (G.resume_foe) {
    G.resume_foe = false;
    if (send_out_foe()) return true;
  }
  const int idx = (int)(me - G.run.party);
  if ((G.run.bt.deviations & PR_DEV_ILLUSION) && G.present[0] && G.shown == idx) {
    /* PR_DEV_ILLUSION: the same Pokemon showing its real look while it has a disguise, or the other way round (a
     * disguise taken or lost with no battle event the screen saw, such as at a battle's start): the look catches up */
    const int sp = bv_mon_of(BV_PLAYER)->species, shown = pr_shown_form(&G.run.bt, &me->b)->species;
    if (sp != shown && (me->b.illusion.active || bv_mon_of(BV_PLAYER)->illusion || sp == pr_pokemon_form(&me->b)->species || sp == pr_apparent_form(&me->b, true)->species)) {
      static bv_mon look;
      const bv_mon live = make_mon(me, true);
      look = *bv_mon_of(BV_PLAYER);                                   /* only the look changes; HP and status catch up after */
      look.illusion=live.illusion;look.species = live.species; look.form = live.form; look.gender = live.gender; look.female = live.female;
      look.sprite_scale = live.sprite_scale; look.n_types = live.n_types;
      memcpy(look.types, live.types, sizeof look.types);
      memcpy(look.name, live.name, sizeof look.name);
      G.n_ev = G.ev_pos = G.n_txt = 0;
      *ev_new() = (bv_event){.type = BEV_ILLUSION, .battler = BV_PLAYER, .a = 0, .mon = &look};
      play_chunk();
      return true;
    }
  }
  if (G.present[0] && G.shown == idx && bv_mon_of(BV_PLAYER)->species == pr_shown_form(&G.run.bt, &me->b)->species) {
    /* the same Pokemon: its HP and status are what the run says (battle-core doesn't make an event for every change, e.g.
     * a Disguise that busts as the battle ends; the converter catches up only while the battle goes on) */
    G.n_ev = G.ev_pos = G.n_txt = 0;
    for (int side = 0; side < 2; side++) {
      const gc_mon* m = gc_field_mon(&G.run, side);
      const bv_mon* sh = bv_mon_of(side ? BV_ENEMY : BV_PLAYER);
      if (!m || !G.present[side] || sh->species != pr_shown_form(&G.run.bt, &m->b)->species) continue;
      const int battler = side ? BV_ENEMY : BV_PLAYER;
      if (m->b.stats[STAT_HP] > 0 && m->b.stats[STAT_HP] != sh->max_hp)
        *ev_new() = (bv_event){.type = BEV_MAX_HP, .battler = battler, .a = m->b.stats[STAT_HP]};
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

/* ---- the in-run menu: START on the command menu or the reward screen ---- */

/* Save and Quit, confirmed. Upstream saves the cached session and system data (what the battle's start wrote) and resets to the title;
 * this run was written at its save point (write_save), so nothing more is written: the run is dropped and the app takes over at black.
 * The system data the battle changed in memory but did not write goes with the battle (the app reads it again). */
static void quit_at_black(void* arg) {
  (void)arg;
  rh_close(); /* the reward screen may be open under the menu: the app's next screen replaces the battle, so nothing may stay above it */
  if (G.cfg.custom) cr_end();
  if (G.cfg.on_quit) G.cfg.on_quit(G.cfg.quit_arg);
}

static void on_menu_quit(void* arg) {
  (void)arg;
  if (G.step_timer) ui_timer_cancel(G.step_timer);
  G.step_timer = 0;
  G.save_due = false;
  G.st.quits++;
  ui_fade(250, 100, 250, quit_at_black, NULL);
}

static void open_run_menu(void* arg) {
  (void)arg;
  if (run_menu_is_open() || !G.cfg.on_quit) return;
  /* a run that is not written anywhere (no save slot) would be lost by quitting: the menu says so as it does for a failed write */
  const runmenu_args a = {.turn = G.run.bt.turn, .unsaved = !G.cfg.save || G.save_failed_told, .on_quit = on_menu_quit};
  run_menu_open(&a);
}

/* The enemy side's items for the bottom screen's bar, in modifierSortFunc's first key (the holder's id, tokens first), then by
 * English name (the port has no class names for its second key) */
static int foe_items(bv_item* out, int cap) {
  static const gc_item* pick[BV_MAX_FOE_ITEMS];
  static const gm_item_ui* ui[BV_MAX_FOE_ITEMS];
  int n = 0;
  for (int i = 0; i < G.run.n_enemy_items && i < GC_MAX_ITEMS && n < cap && n < BV_MAX_FOE_ITEMS; i++) {
    const gc_item* it = &G.run.enemy_items[i];
    if (!it->key || it->stack <= 0) continue;
    const gm_item_ui* u = gm_item_ui_find(it->key, it->arg);
    if (!u) u = gm_item_ui_find(it->key, -1);
    if (!u) continue; /* nothing to draw it with */
    int k = n++;
    pick[k] = it;
    ui[k] = u;
    while (k > 0 && (pick[k - 1]->pokemon_id > pick[k]->pokemon_id ||
                     (pick[k - 1]->pokemon_id == pick[k]->pokemon_id && strcmp(ui[k - 1]->name, ui[k]->name) > 0))) {
      const gc_item* t = pick[k - 1];
      pick[k - 1] = pick[k];
      pick[k] = t;
      const gm_item_ui* tu = ui[k - 1];
      ui[k - 1] = ui[k];
      ui[k] = tu;
      k--;
    }
  }
  for (int i = 0; i < n; i++) out[i] = (bv_item){ui[i]->icon, (int)pick[i]->stack, ui[i]->name, ui[i]->dynamic ? NULL : ui[i]->desc};
  return n;
}

/* The player's own items that no Pokemon holds (Mega Bracelet, Dynamax Band, Tera Orb, Exp. Share, Amulet Coin...), by English name:
 * the second item bar */
static int key_items(bv_item* out, int cap) {
  int n = 0;
  for (int i = 0; i < G.run.n_items && i < GC_MAX_ITEMS && n < cap; i++) {
    const gc_item* it = &G.run.items[i];
    if (!it->key || it->stack <= 0 || it->pokemon_id || it->detached) continue;
    const gm_item_ui* u = gm_item_ui_find(it->key, it->arg);
    if (!u) u = gm_item_ui_find(it->key, -1);
    if (!u) continue;
    int k = n++;
    out[k] = (bv_item){u->icon, (int)it->stack, u->name, u->dynamic ? NULL : u->desc};
    while (k > 0 && strcmp(out[k - 1].name, out[k].name) > 0) {
      const bv_item t = out[k - 1];
      out[k - 1] = out[k];
      out[k] = t;
      k--;
    }
  }
  return n;
}

static void refresh_item_bars(void){
  static bv_item foes[BV_MAX_FOE_ITEMS],keys[BV_MAX_FOE_ITEMS];
  int nf=foe_items(foes,BV_MAX_FOE_ITEMS),nk=key_items(keys,BV_MAX_FOE_ITEMS);
  bv_set_item_bars(foes,nf,keys,nk,gc_party_luck(&G.run));
}

static void ask(void) {
  const gc_screen* s = gc_run_screen(&G.run);
  G.ball = -1;
  sync_shown();
  bv_command_opts o;
  memset(&o, 0, sizeof o);
  o.command_slot=s->slot;
  o.can_undo=s->can_undo;
  o.party_luck = gc_party_luck(&G.run);
  o.can_ball = s->can_ball;
  o.can_switch = s->can_switch;
  o.can_run = s->can_run;
  o.struggle = s->struggle;
  o.on_menu = G.cfg.on_quit ? open_run_menu : NULL;
  static bv_item foe[BV_MAX_FOE_ITEMS];
  o.n_foe_items = foe_items(foe, BV_MAX_FOE_ITEMS);
  o.foe_items = foe;
  static bv_item keys[BV_MAX_FOE_ITEMS];
  o.n_key_items = key_items(keys, BV_MAX_FOE_ITEMS);
  o.key_items = keys;
  o.has_party_balls = true;
  for (int i = 0; i < 6; i++)
    o.party_balls[i] = i >= G.run.n_party ? BV_BALL_EMPTY : G.run.party[i].b.hp > 0 ? BV_BALL_ALIVE : BV_BALL_FAINTED;
  const gc_mon* me = command_mon();
  for (int i = 0; i < 4; i++) {
    o.move_ok[i] = s->move_ok[i];
    if (!s->move_ok[i] && s->move_why[i] && me && i < PR_N_MOVES(&me->b)) {
      char name[48];
      snprintf(name, sizeof name, "%s", i18n_species_name(me->b.species));
      i18n_t(g_reason[i], sizeof g_reason[i], s->move_why[i], "moveName", i18n_move_name(PR_MOVES(&me->b)[i]),
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
  G.ball = ball;
  G.ball_target=G.run.bt.field[2]&&G.run.bt.field[2]->hp>0&&G.run.bt.field[2]->on_field&&!G.run.bt.field[2]->switch_out_status?2:3;
  if (answer(&c))
    G.st.balls++;
  else
    G.ball = -1; /* refused (the player was told why): nothing was thrown */
}

static void on_command(bv_command c, void* arg) {
  (void)arg;
  gc_choice ch;
  memset(&ch, 0, sizeof ch);
  switch (c.command) {
    case 4:
      ch.kind=GC_CH_CANCEL;answer(&ch);break;
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

/* ---- the Pokemon to send in (after a faint, or a move that switches out; or which to release for a catch) ---- */

static void ask_release(void);

/* Answers the switch screen with party index `a` (-1: nobody: the full-party question's No, or backing out of a switch) */
static void finish_pick(int a) {
  const bool swap = G.swap_prompt;
  gc_choice c = {.kind = GC_CH_PICK, .a = (int16_t)a};
  G.release_drop = swap && a >= 0;
  if (!submit(&c)) {
    G.release_drop = false;
    return;
  }
  if (swap) {
    if (a >= 0)
      G.st.releases++;
    else
      G.st.kept_out++;
  }
  if (swap && a >= 0 && a == G.shown) {
    /* the caught Pokemon took the place of the one on the field: the screen's copy is gone */
    G.swap_replaced = true;
  }
  pump(NULL);
}

static void on_send_pick(int slot, int option, void* arg) {
  (void)arg;
  if (option != PS_SEND_OUT && option != PS_RELEASE_PICK && option != PS_REVIVE_PICK && option != PS_CANCEL) return;
  const gc_screen* s = gc_run_screen(&G.run);
  if (G.swap_prompt && option == PS_CANCEL) {
    /* backing out of the release list goes back to the question (upstream's promptRelease) */
    ps_close();
    ask_release();
    return;
  }
  int a = -1;
  if ((option == PS_SEND_OUT || option == PS_RELEASE_PICK || option == PS_REVIVE_PICK) && slot >= 0 && slot < G.run.n_party) a = G.run.party_order[slot];
  else if (!s->can_skip) return; /* no cancelling a faint switch */
  ps_close();
  finish_pick(a);
}

/* "Your party is full. Release a Pokemon to make room for X?" is on screen (game-core's message): Yes picks who goes,
 * No lets the caught Pokemon go */
static void on_release_answer(int yes, void* arg) {
  (void)arg;
  if (yes == 1) {
    ps_open(&G.run.bt, PS_RELEASE, on_send_pick, NULL);
    return;
  }
  finish_pick(-1);
}

static void ask_release(void) { ui_push_confirm(on_release_answer, NULL); }

static void open_switch(const gc_screen* s) {
  if (G.swap_prompt)
    ask_release();
  else
    ps_open(&G.run.bt, s->revival_blessing ? PS_REVIVAL_BLESSING : s->can_skip ? PS_SWITCH : PS_FAINT_SWITCH, on_send_pick, NULL);
}

/* ---- learning a move: menus' own dialog, with the battle's message box and yes/no ---- */

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

/* ---- evolution: "What? X is evolving!" -> confirm prompt -> "X stopped evolving." / "Congratulations! X evolved into Y!" ----
 * Upstream evolution scene text sequence (evolution-phase.ts / evolution-scene-ui-handler.ts):
 * 1. "What? X is evolving!" (i18n key "menu:evolving"): queued by game-core at evolution start and played via BEV_MESSAGE.
 * 2. Evolution prompt (ui_push_confirm): asks the player to proceed or cancel (B button).
 * 3. On cancel (B button / No): answer GC_CH_CANCEL -> game-core queues "menu:stoppedEvolving" ("X stopped evolving."), played via BEV_MESSAGE.
 * 4. On success (Yes): answer GC_CH_EVOLVE -> game-core evolves member, emits GC_EV_EVOLVED, and queues "menu:evolutionDone"
 *    ("Congratulations! Your X evolved into Y!"), played via BEV_MESSAGE. Any post-evolution move learned goes to GC_SCREEN_LEARN_MOVE.
 */

static void on_evolve_answer(int yes, void* arg) {
  (void)arg;
  gc_choice c;
  memset(&c, 0, sizeof c);
  if (yes == 1) {
    c.kind = GC_CH_EVOLVE;
    if (answer(&c)) G.st.evolves++;
  } else {
    c.kind = GC_CH_CANCEL;
    c.a = 0;
    if (answer(&c)) G.st.cancels++;
  }
}

static void open_evolve(const gc_screen* s) {
  (void)s;
  ui_push_confirm(on_evolve_answer, NULL);
}

/* ---- rewards: shop_ui ---- */

static gc_choice rc;          /* current pending choice */

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
  const bool tm = !strcmp(offer->key, "TM_COMMON") || !strcmp(offer->key, "TM_GREAT") || !strcmp(offer->key, "TM_ULTRA");
  ps_open_item(&G.run, tm ? PARTY_TM_MODIFIER : move_target ? PARTY_MOVE_MODIFIER : PARTY_MODIFIER,
               move_target, tm ? offer->arg : 0, on_item_target_picked, NULL);
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
  rh_close();
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
  rh_close();
  if (offer->target != GC_TARGET_NONE) {
    prompt_item_target(offer);
  } else {
    answer(&rc);
  }
}

static void on_shop_reroll(void* ctx, bool lock_capsule) {
  (void)ctx;
  rh_close();
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
  rh_close();
  memset(&rc, 0, sizeof rc);
  rc.kind = GC_CH_SKIP;
  answer(&rc);
}

/* Manage Items: each transfer or discard goes to game-core at once and the party screen shows the run again; backing out shows the
 * same reward screen again (resetModifierSelect). A refused transfer (the target's stack is full) changes nothing, as upstream. */
static void on_manage_move(int item_src, int count, int to, void* arg) {
  (void)arg;
  gc_choice c = {0};
  c.kind = to < 0 ? GC_CH_DISCARD : GC_CH_TRANSFER;
  c.a = (int16_t)to;
  c.b = (int16_t)item_src;
  c.c = (int16_t)count;
  const char* why = NULL;
  /* its events (a moved vitamin's new maximum HP) wait in game-core's queue until the screen closes */
  gc_run_choose(&G.run, &c, &why);
}

static void on_manage_done(void* arg) {
  (void)arg;
  ps_close();
  drain();
  pump(NULL); /* the events, then the same reward screen again */
}

static void on_shop_manage_items(void* ctx) {
  (void)ctx;
  rh_close();
  ps_open_transfer(&G.run, on_manage_move, on_manage_done, NULL);
}

static bool party_has_held_item(void) {
  for (int i = 0; i < G.run.n_items; i++) {
    const gc_item* it = &G.run.items[i];
    if (it->detached || it->non_transferable || it->stack <= 0) continue;
    for (int p = 0; p < G.run.n_party; p++)
      if (it->pokemon_id && it->pokemon_id == G.run.party[p].b.id) return true;
  }
  return false;
}

static int calc_party_luck(void) { return gc_party_luck(&G.run); }

static void open_shop_ui(void) {
  const gc_screen* s = gc_run_screen(&G.run);
  shop_ui_args args;
  memset(&args, 0, sizeof args);
  args.screen = s;
  args.player_money = G.run.money;
  args.has_lock_capsule = gc_item_stack(&G.run, "LOCK_CAPSULE") > 0;
  args.party_luck = calc_party_luck();
  args.party_has_held_item = party_has_held_item();
  args.wave = G.run.wave;
  rt_biome_name(G.run.biome, args.location, sizeof args.location);
  /* what the money items grant now: MoneyRewardPhase's amount at their multipliers (Nugget 1, Big Nugget 2.5, Relic Gold 10)
   * with the Amulet Coins' bonus, which is what upstream's description for them shows */
  {
    static const double mult[3] = {1.0, 2.5, 10.0};
    int stacks[8], n = 0;
    for (int i = 0; i < G.run.n_items && n < 8; i++)
      if (!strcmp(G.run.items[i].key, "AMULET_COIN")) stacks[n++] = G.run.items[i].stack;
    for (int k = 0; k < 3; k++) args.money_item_amount[k] = pr_run_get_money_reward(G.run.wave, mult[k], stacks, n, false);
  }

  static const shop_ui_events ev = {
    on_shop_select_reward,
    on_shop_select_shop,
    on_shop_reroll,
    on_shop_skip,
    on_shop_manage_items,
    NULL
  };

  rh_set_menu(G.cfg.on_quit ? open_run_menu : NULL, NULL);
  rh_open_shop(&args, &ev);
}

/* ---- biome: biome_map ---- */

static void on_biome_map_select(void* ctx, int option) {
  (void)ctx;
  rh_close();
  gc_choice c = {.kind = GC_CH_BIOME, .a = (int16_t)(option < 0 ? 0 : option)};
  answer(&c);
}

static void open_biome_map(void) {
  const gc_screen* s = gc_run_screen(&G.run);
  biome_map_args ba = {s->biomes, s->n_biomes};
  biome_map_events be = {on_biome_map_select, NULL};
  rh_open_biome_map(&ba, &be);
}

/* ---- a mystery encounter, stand-in: the first option that can be taken, with the first healthy Pokemon ---- */

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

/* ---- a custom rematch: the offer (the intro of the wave already said who wants it) ---- */

static void on_offer_answer(int yes, void* arg) {
  (void)arg;
  G.resume_foe=true; /* The offer intentionally hid the enemy during the wave intro. */
  gc_choice c = {.kind = GC_CH_CONFIRM, .a = yes == 1 ? 1 : 0};   /* No and B decline */
  answer(&c);
}

static void offer_asked(void* arg) {
  (void)arg;
  ui_push_confirm(on_offer_answer, NULL);
}

static void open_custom_offer(void) {
  G.n_ev = G.ev_pos = G.n_txt = 0;
  put_msg("Fight it?", false);
  G.st.bev++;
  bv_play(&G.ev[0], 1, offer_asked, NULL);
}

/* ---- the run ends ---- */

static bool new_run(bool from_black);

static void restart_black(void* arg) {
  (void)arg;
  new_run(true);
}

static void restart(void* arg) {
  (void)arg;
  ui_fade(250, 100, 250, restart_black, NULL);
}

static void end_run(int kind) {
  char text[96];
  if (G.step_timer) ui_timer_cancel(G.step_timer);
  G.step_timer = 0;
  G.save_due = false;
  /* the combat history's report is taken now, while the run is whole and before the app clears the slot or starts another */
  co_finish_run(kind == GC_SCREEN_CLEAR, &G.run);
  /* a custom run settles its rematch and names the enemy that finished the party, from the same moment: the run is whole */
  G.have_defeat = G.cfg.custom && cr_on_run_end(&G.run, kind, &G.defeat);
  if (G.cfg.custom) cr_end();
  if (kind == GC_SCREEN_CLEAR)
    G.st.clears++;
  else
    G.st.game_overs++;
  bgm_on_run_end(kind == GC_SCREEN_CLEAR);
  if (G.cfg.on_end) {
    /* the app takes the run from here (the result screen); the run stays readable through rs_run() until it starts another */
    G.cfg.on_end(kind, G.cfg.end_arg);
    return;
  }
  /* no app around (the host test): a message, then a new run */
  if (kind == GC_SCREEN_CLEAR)
    i18n_t(text, sizeof text, "battle:congratulations", NULL);
  else
    snprintf(text, sizeof text, "Game over at wave %d. A new run begins.", (int)G.run.wave);
  G.n_ev = G.ev_pos = G.n_txt = 0;
  put_msg(text, true);
  G.st.bev++;
  bv_play(&G.ev[0], 1, restart, NULL);
}

/* ---- saving ---- */

/* Writes the run into the slot, and the system data it updated (the species it caught) beside it. A refusal of either is counted;
 * save_point tells the player. */
static void write_save(void) {
  bool ok = (G.cfg.custom ? cr_save(G.cfg.slot, &G.run) : rsv_save(G.cfg.slot, &G.run)) == RSV_SAVED;
  if (G.cfg.sys && G.cfg.save_sys && !G.cfg.save_sys()) ok = false;
  if (ok) {
    G.st.saves++;
    G.save_failed_told = false;
  } else {
    G.st.save_failed++;
    G.save_failed_now = true;
  }
}

static void step_later(void* arg) {
  (void)arg;
  G.step_timer = 0;
  screen_step();
}

/* At the COMMAND screen of a battle's first turn, once the scene is up: saves if the run is at a save point. The SD card is
 * slow, so it is never written while something moves: with a fade or events playing, the step waits and comes back. False when
 * the caller must stop (waiting, or a message is on its way and the driver goes on after it). */
static bool save_point(void) {
  if (!G.save_due) return true;
  if (!gc_run_can_save(&G.run)) {
    G.save_due = false;
    return true;
  }
  if (ui_busy() || bv_playing()) {
    G.step_timer = ui_after(50, step_later, NULL);
    if (G.step_timer) return false;
    return true; /* no timer free: submit() writes it before the first turn */
  }
  G.save_due = false;
  G.save_failed_now = false;
  write_save();
  if (G.save_failed_now && !G.save_failed_told) {
    G.save_failed_told = true;
    say("The game could not be saved to the SD card. It will try again at the next battle.");
    return false;
  }
  return true;
}

static void open_custom_offer(void);

/* ScanIvsPhase is presentation only: do not answer or advance the battle command. */
static void scan_done(void* arg) {
  (void)arg;
  G.n_ev = G.ev_pos = G.n_txt = 0;
  pump(NULL);
}

static void scan_answered(int yes, void* arg) {
  (void)arg;
  if (!yes) { scan_done(NULL); return; }
  const pr_pokemon* p = G.run.bt.field[G.scan_target];
  if (!p) { scan_done(NULL); return; }
  uint16_t root = gc_species_root_id(p->species);
  for (int s = 0; s < 6; s++) {
    G.scan_stats[s] = p->ivs[s];
    G.scan_stats[6+s] = G.run.sys && root <= GC_MAX_SPECIES_ID ? G.run.sys->dex[root].ivs[s] : 0;
  }
  G.n_ev = G.ev_pos = G.n_txt = 0;
  *ev_new() = (bv_event){.type=BEV_IVS,.battler=G.scan_target,.stats=G.scan_stats};
  G.st.bev++;
  bv_play(G.ev,G.n_ev,scan_done,NULL);
}

static void scan_confirm(void* arg) { (void)arg; ui_push_confirm(scan_answered,NULL); }

static bool scan_wild_ivs(void) {
  if (G.scan_wave != G.run.wave) {
    G.scan_wave = G.run.wave;
    G.scan_next = 0;
    G.scan_enabled = G.run.battle_type == BATTLE_TYPE_WILD && gc_item_stack(&G.run,"IV_SCANNER") > 0;
  }
  if (!G.scan_enabled) return false;
  while (G.scan_next < 2) {
    const int target = 2 + G.scan_next++;
    const pr_pokemon* p = G.run.bt.field[target];
    if (!p || p->hp <= 0 || !p->on_field) continue;
    G.scan_target = target;
    G.n_ev = G.ev_pos = G.n_txt = 0;
    pr_msg question = pr_msg1("battle:ivScannerUseQuestion",pr_arg_pokemon(&G.run.bt,"pokemonName",p));
    put_gc_msg(&question);
    G.st.bev++;
    bv_play(G.ev,G.n_ev,scan_confirm,NULL);
    return true;
  }
  return false;
}

static void target_done(int row,void *arg){
  (void)arg;gc_choice c={.kind=row<0?GC_CH_CANCEL:GC_CH_TARGET,.a=row>=0&&row<G.target_count?G.target_values[row]:-1};answer(&c);
}
static void ask_target(void){
  G.target_count=0;
  for(int field=0;field<4;field++)if(G.run.screen.target_ok[field]){
    int i=G.target_count++;G.target_values[i]=field;const pr_pokemon *p=G.run.bt.field[field];
    snprintf(G.target_labels[i],sizeof G.target_labels[i],"%s %s: %s",field<2?"Ally":"Foe",field%2?"right":"left",p?i18n_species_name(pr_shown_form(&G.run.bt,p)->species):"?");
    G.target_options[i]=G.target_labels[i];
  }
  ui_push_options(G.target_options,G.target_count,true,target_done,NULL);
}

static void screen_step(void) {
  refresh_item_bars();
  const gc_screen* s = gc_run_screen(&G.run);
  if (G.cfg.custom) {
    /* a rematch won with this screen's events behind it is written first (custom_run.c), and anything it has to tell the player is told
     * before the screen is handled */
    cr_on_screen(&G.run, s->kind);
    const char* note = cr_take_message();
    if (note) {
      say(note);
      return;
    }
  }
  G.st.screens[s->kind & 15]++;
  switch (s->kind) {
    case GC_SCREEN_TARGET:ask_target();break;
    case GC_SCREEN_COMMAND:
      if (scan_wild_ivs()) break;
      if (!save_point()) break;
      if (!resync_field()) ask();
      break;
    case GC_SCREEN_SWITCH:
      open_switch(s);
      break;
    case GC_SCREEN_LEARN_MOVE:
      open_learn(s);
      break;
    case GC_SCREEN_EVOLVE:
      open_evolve(s);
      break;
    case GC_SCREEN_REWARD:
      open_shop_ui();
      break;
    case GC_SCREEN_BIOME:
      open_biome_map();
      break;
    case GC_SCREEN_MYSTERY:
      mystery_auto();
      break;
    case GC_SCREEN_CUSTOM_OFFER:
      open_custom_offer();
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

/* a random team within the cost budget, from every species with a starter cost (game-core's own rule). With system data on the
 * run, from the species it lets the player pick, each with the defaults the starter select screen would give it. */
static int random_team(gc_starter* out) {
  double budget = gc_starter_budget(GAME_MODES_CLASSIC);
  int n = 0;
  const gc_system* sys = G.run.sys;
  if (sys) {
    const int first = (int)rnd(PR_SPECIES_COUNT);
    for (int k = 0; k < PR_SPECIES_COUNT && n < 6 && budget > 0; k++) {
      const pr_species* sp = &pr_species_table[(first + k) % PR_SPECIES_COUNT];
      gc_starter s;
      double cost = 0;
      if (!ss_starter(sys, (uint16_t)sp->id, &s) || !gc_starter_is_available(sys, &s, &cost, NULL) || cost > budget) continue;
      out[n++] = s;
      budget -= cost;
    }
    return n;
  }
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
  return n;
}

/* from_black: a fade is at black now (the wave comes up without fading again) */
static bool new_run(bool from_black) {
  G.st.runs++;
  reset_driver();
  co_begin_run(false);
  G.fade_done = from_black;
  G.rng = 0x9E3779B97F4A7C15ull ^ ((uint64_t)G.cfg.entropy * 0xD1B54A32D192ED03ull) ^ ((uint64_t)G.st.runs * 0x2545F4914F6CDD1Dull);
  rnd(1);
  gc_run_new(&G.run, GAME_MODES_CLASSIC, G.st.runs == 1 ? G.cfg.seed : NULL, G.cfg.entropy + (uint32_t)G.st.runs);
  G.run.sys = G.cfg.sys;
  G.have_defeat = false;
  if (G.cfg.custom) {
    /* the run's identity, its schedule and the hooks, before the starters (the first wave is reported to them) */
    if (!cr_begin(&G.run, G.cfg.entropy + (uint32_t)G.st.runs)) {
      G.st.stuck++;
      return false;
    }
  } else {
    cr_end();
  }
  gc_starter team[6];
  int n = 0;
  if (G.st.runs == 1 && G.cfg.n_team > 0) {
    for (; n < G.cfg.n_team && n < 6; n++) team[n] = G.cfg.team[n];
  } else if (G.st.runs == 1 && G.cfg.n_starters > 0) {
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
    return false;
  }
  drain();
  pump(NULL);
  return true;
}

bool rs_start(const rs_config* cfg) {
  memset(&G.st, 0, sizeof G.st);
  G.cfg = *cfg;
  return new_run(false);
}

rsv_status rs_resume(const rs_config* cfg, char why[RSV_WHY_LEN]) {
  G.cfg = *cfg; /* the stats go on counting: a resume is not a new session */
  reset_driver();
  G.have_defeat = false;
  if (!cfg->custom) cr_end();
  const rsv_status st = cfg->custom ? cr_load(cfg->slot, &G.run, why) : rsv_load(cfg->slot, &G.run, why);
  if (st != RSV_OK) return st; /* G.run is not usable: the caller must not drive it */
  G.run.sys = cfg->sys; /* a saved run holds no system data: it takes the player's */
  /* observation starts here: the battles before the save were not seen (a custom run's save carries the turns it had kept) */
  if (cfg->custom)
    cr_begin_observer();
  else
    co_begin_run(true);
  G.st.runs++;
  G.st.loads++;
  G.resume_wave = G.resume_foe = true;
  G.rng = 0x9E3779B97F4A7C15ull ^ ((uint64_t)G.cfg.entropy * 0xD1B54A32D192ED03ull);
  rnd(1);
  drain();
  /* the loaded run's own save point is the file it came from: not written again until the next battle */
  for (int i = 0; i < G.nq; i++)
    if (G.q[i].kind == GC_EV_SAVE_POINT) G.skip[i] = true;
  pump(NULL);
  return RSV_OK;
}

const cs_defeat* rs_custom_defeat(void) { return G.have_defeat ? &G.defeat : NULL; }
const rs_stats* rs_stats_get(void) { return &G.st; }
const gc_run* rs_run(void) { return &G.run; }
