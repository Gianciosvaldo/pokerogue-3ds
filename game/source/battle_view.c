/* The battle screen (see battle_view.h). Positions are upstream's: the field container for the
 * arena and sprites, fieldUI for the info boxes, the ui container for the bottom strip. */
#include "battle_view.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "battle_anim.h"
#include "gm_enum_names.h"
#include "i18n.h"
#include "pr_data.h"
#include "tween.h"
#include "ui.h"
#include "ui_text.h"
#include "ui_widgets.h"

#define EV_MAX 160
#define EV_TEXT 256

/* ------------------------------------------------------------------ state */

typedef struct {
  bv_mon mon;
  bool present;       /* on the field */
  bool sprite_on;     /* sprite visible (damage blink) */
  float drop;         /* faint: px the sprite has dropped */
  float scale;        /* summon */
  bool black;         /* intro silhouette */
  float info_dx;      /* info box slide offset */
  bool info_on;
  float hp_shown;     /* the HP bar's displayed hp (tweened) */
  float exp_shown;
  char sprite_key[64];
} side;

typedef enum { M_IDLE, M_EVENTS, M_COMMAND, M_FIGHT } mode;

static struct {
  bv_setup setup;
  char biome[32];
  side s[2]; /* 0 player, 1 enemy */
  float arena_player_x, arena_enemy_x, enemy_slide;
  msgbox msg;
  float msg_wrap;
  mode mode;
  int cmd_cursor, move_cursor;
  /* events */
  bv_event ev[EV_MAX];
  char ev_text[EV_MAX][EV_TEXT];
  bv_mon ev_mon[EV_MAX];
  int n_ev, cur;
  int step;       /* the current event's stage */
  float t;        /* ms in the current stage */
  tween tw, tw2;
  void (*on_done)(void*);
  void* done_arg;
  void (*on_command)(bv_command, void*);
  void* command_arg;
  char ability_text[96];
  float ability_t;
  float ko_wait;
  bool blink;
  float fps;
  int fps_frames;
  float fps_t;
} B;

static side* side_of(int battler) { return &B.s[battler >= 2 ? 1 : 0]; }
bv_mon* bv_mon_of(int battler) { return &side_of(battler)->mon; }

/* ------------------------------------------------------------------ helpers */

static void sprite_key(const bv_mon* m, bool back, char* out, int n) {
  /* upstream getSpriteAtlasPath without forms, variants or gender differences yet */
  snprintf(out, n, "images/pokemon/%s%s%d", back ? "back/" : "", m->shiny ? "shiny/" : "", m->species);
}

static void set_mon(side* sd, const bv_mon* m, bool player) {
  sd->mon = *m;
  sprite_key(m, player, sd->sprite_key, sizeof sd->sprite_key);
  if (!img_has(sd->sprite_key) && m->shiny) sprite_key(&(bv_mon){.species = m->species}, player, sd->sprite_key,
                                                         sizeof sd->sprite_key);
  sd->hp_shown = m->hp;
  sd->exp_shown = m->exp_ratio;
  sd->sprite_on = true;
  sd->drop = 0;
  sd->scale = 1;
  sd->black = false;
  ba_show(player ? 0 : 2);
}

static void cry(const bv_mon* m) {
  char k[32];
  snprintf(k, sizeof k, "cry/%d", m->species);
  ui_play_sound(k);
}

static const char* name_with_affix(int battler) {
  /* getPokemonNameWithAffix: "Wild X" / "the opposing X" for enemies */
  static char out[2][96];
  static int i;
  i ^= 1;
  side* sd = side_of(battler);
  if (battler < 2) return snprintf(out[i], 96, "%s", sd->mon.name), out[i];
  i18n_t(out[i], 96, B.setup.trainer ? "battle:foePokemonWithAffix" : "battle:wildPokemonWithAffix", "pokemonName",
         sd->mon.name, NULL);
  return out[i];
}

/* ------------------------------------------------------------------ drawing: field */

static const rr_frame* sprite_frame(side* sd) {
  int frame = (int)(ui_now() * 10 / 1000); /* upstream anims run at 10 fps */
  return img_anim_frame(sd->sprite_key, frame);
}

/* The field as battle animations see it: where each Pokemon stands and its current frame. */
static void anim_field(ba_mon out[4]) {
  memset(out, 0, 4 * sizeof *out);
  for (int k = 0; k < 2; k++) {
    side* sd = &B.s[k];
    ba_mon* m = &out[k ? 2 : 0];
    m->on_field = sd->present;
    m->player = !k;
    m->x = k ? 236 + B.enemy_slide : 106;
    m->y = k ? 84 : 148;
    m->frame = sprite_frame(sd);
    m->half_h = (m->frame ? m->frame->src_h : 40) * sd->scale / 2;
    m->species = sd->mon.species;
  }
}

static void draw_sprite(side* sd, bool player) {
  if (!sd->present || !sd->sprite_on || sd->scale <= 0 || ba_hidden(player ? 0 : 2)) return;
  const rr_frame* f = sprite_frame(sd);
  float x = player ? 106 : 236 + B.enemy_slide;
  float y = player ? 148 : 84;
  if (!f) {
    rr_rect(x - 20, y - 40 + sd->drop, 40, 40, player ? RR_RGBA(0xd0, 0x60, 0x40, 255) : RR_RGBA(0x50, 0x60, 0xa0, 255));
    return;
  }
  rr_draw_opts o = RR_DEFAULT_OPTS;
  o.origin_x = 0.5f;
  o.origin_y = 1.0f;
  o.scale_x = o.scale_y = sd->scale;
  if (sd->black) o.tint = RR_RGBA(0, 0, 0, 255);
  if (sd->drop > 0) {
    /* fainting: the sprite sinks below its base line */
    int vis = (int)(f->src_h - sd->drop);
    if (vis <= 0) return;
    o.origin_y = 0;
    rr_draw_cropped(f, x, y - f->src_h + sd->drop, f->src_w, vis, &o);
    return;
  }
  rr_draw(f, x, y, &o);
}

static void draw_field(void) {
  char k[64];
  snprintf(k, sizeof k, "images/arenas/%s_bg", B.biome);
  if (img_has(k))
    img_draw(k, NULL, 0, 0, 0, 0);
  else
    rr_rect(0, 0, 320, 180, RR_RGBA(0x88, 0xc8, 0xf0, 255));
  snprintf(k, sizeof k, "images/arenas/%s_b", B.biome);
  img_draw(k, NULL, B.arena_enemy_x, 0, 0, 0);
  snprintf(k, sizeof k, "images/arenas/%s_a", B.biome);
  img_draw(k, NULL, B.arena_player_x, 0, 0, 0);
  ba_mon field[4];
  anim_field(field);
  ba_draw(BA_LAYER_BELOW_ENEMY, field);
  draw_sprite(&B.s[1], false);
  ba_draw(BA_LAYER_MID, field);
  draw_sprite(&B.s[0], true);
  ba_draw(BA_LAYER_TOP, field);
}

/* ------------------------------------------------------------------ drawing: info boxes */

static const char* hp_frame(float ratio) { return ratio > 0.5f ? "high" : ratio > 0.25f ? "medium" : "low"; }

static void draw_numbers(const char* atlas, const char* s, float x, float y, int dir) {
  /* one image per character, 8 px apart, image origin (0.5, 0.5) */
  int n = (int)strlen(s);
  for (int i = 0; i < n; i++) {
    char fr[2] = {s[dir > 0 ? i : n - 1 - i], 0};
    img_draw(atlas, fr, x + dir * i * 8, y, 0.5f, 0.5f);
  }
}

static void draw_level(float lx, float ly, int level, bool red) {
  /* levelContainer at (lx, ly): overlay_lv at (5.5, 0) origin (1, 0.5); numbers at (9.5, -0.5) */
  char s[8];
  snprintf(s, sizeof s, "%d", level);
  int extra = (int)strlen(s) - 3;
  if (extra > 0) lx -= 8 * extra;
  img_draw("images/ui/text_images/en/battle_ui/overlay_lv", NULL, lx + 5.5f, ly, 1, 0.5f);
  draw_numbers(red ? "images/ui/numbers_red" : "images/ui/numbers", s, lx + 9.5f, ly - 0.5f, 1);
}

static void draw_types(const bv_mon* m, bool player, float bx, float by) {
  char k[48], fr[24];
  const char* who = player ? "player" : "enemy";
  float x1 = player ? -139 : -15, y1 = player ? -17 : -15.5f, y2 = player ? -1 : -2.5f;
  snprintf(k, sizeof k, "images/ui/pbinfo_%s_type%s", who, m->n_types > 1 ? "1" : "");
  gm_enum_lower(GM_E_POKEMON_TYPE, m->types[0], fr, sizeof fr);
  img_draw(k, fr, bx + x1, by + y1, 0, 0);
  if (m->n_types > 1) {
    snprintf(k, sizeof k, "images/ui/pbinfo_%s_type2", who);
    gm_enum_lower(GM_E_POKEMON_TYPE, m->types[1], fr, sizeof fr);
    img_draw(k, fr, bx + x1, by + y2, 0, 0);
  }
}

static void draw_name(const bv_mon* m, bool player, float x, float y) {
  /* updateNameText: shortened with "." until it fits */
  char name[48];
  snprintf(name, sizeof name, "%s", m->name);
  float limit = 60 - ((m->gender < 0 ? 0 : 6) + (m->shiny ? 8 : 0) +
                      (fminf((float)snprintf(NULL, 0, "%d", m->level), 3) - 3) * 8);
  if (!player && m->boss) limit += 38;
  while (text_width(TS_BATTLE_INFO, name, NULL) > limit && strlen(name) > 2) {
    size_t n = strlen(name);
    int cut = name[n - 1] == '.' ? 2 : 1;
    name[n - cut] = 0;
    while (n > 1 && name[strlen(name) - 1] == ' ') name[strlen(name) - 1] = 0;
    strcat(name, ".");
  }
  float w = text_draw(TS_BATTLE_INFO, x, y, name, NULL);
  if (m->gender >= 0) {
    /* getGenderSymbol / getGenderColor */
    text_opts o = text_defaults();
    o.color = m->gender == 0 ? RR_RGBA(0x40, 0xc8, 0xf8, 255) : RR_RGBA(0xf8, 0x90, 0x90, 255);
    o.shadow = m->gender == 0 ? RR_RGBA(0x00, 0x60, 0x90, 255) : RR_RGBA(0x98, 0x40, 0x38, 255);
    text_draw(TS_BATTLE_INFO, x + w, y, m->gender == 0 ? "♂" : "♀", &o);
  }
  if (m->status) {
    char fr[24];
    gm_enum_lower(GM_E_STATUS_EFFECT, m->status, fr, sizeof fr);
    img_draw("images/statuses", fr, x, y + 11.5f, 0, 0);
  }
}

static void draw_player_info(side* sd) {
  if (!sd->present || !sd->info_on) return;
  const bv_mon* m = &sd->mon;
  float bx = 310 + sd->info_dx, by = -72; /* PlayerBattleInfo container */
  img_draw("images/ui/pbinfo_player", NULL, bx, by, 1, 0.5f);
  draw_types(m, true, bx, by);
  draw_name(m, true, bx - 115, by - 15.2f);
  draw_level(bx - 41, by - 10, m->level, false);
  float ratio = m->max_hp ? sd->hp_shown / m->max_hp : 0;
  const rr_frame* hf = img_frame("images/ui/overlay_hp", hp_frame(ratio));
  int w = (int)floorf(48 * ratio + 0.5f);
  if (w < 1 && sd->hp_shown > 0) w = 1;
  if (hf && w > 0) rr_draw_cropped(hf, bx - 61, by - 1, w, hf->src_h, NULL);
  img_draw("images/ui/text_images/en/battle_ui/overlay_hp_label", NULL, bx - 62, by - 4, 1, 0);
  /* HP numbers: right to left from (-15, 10), "hp/max" */
  char s[24];
  snprintf(s, sizeof s, "%d/%d", (int)ceilf(sd->hp_shown - 0.001f), m->max_hp);
  draw_numbers("images/ui/numbers", s, bx - 15, by + 10, -1);
  /* EXP bar: overlay_exp at (-98, 18) masked to 85 px */
  img_draw("images/ui/text_images/en/battle_ui/overlay_exp_label", NULL, bx - 91, by + 20, 1, 1);
  const rr_frame* ef = img_frame("images/ui/overlay_exp", NULL);
  int ew = (int)floorf(85 * sd->exp_shown + 0.5f);
  if (ef && ew > 0) rr_draw_cropped(ef, bx - 98, by + 18, ew, ef->src_h, NULL);
}

static void draw_enemy_info(side* sd) {
  if (!sd->present || !sd->info_on) return;
  const bv_mon* m = &sd->mon;
  float bx = 140 + sd->info_dx, by = -141; /* EnemyBattleInfo container */
  img_draw("images/ui/pbinfo_enemy_mini", NULL, bx, by, 1, 0.5f);
  draw_types(m, false, bx, by);
  draw_name(m, false, bx - 124, by - 11.2f);
  if (m->caught_before) img_draw("images/ui/icon_owned", NULL, bx - 124, by - 11.2f + 11.75f, 0, 0);
  draw_level(bx - 50, by - 5, m->level, false);
  float ratio = m->max_hp ? sd->hp_shown / m->max_hp : 0;
  const rr_frame* hf = img_frame("images/ui/overlay_hp", hp_frame(ratio));
  int w = (int)floorf(48 * ratio + 0.5f);
  if (w < 1 && sd->hp_shown > 0) w = 1;
  if (hf && w > 0) rr_draw_cropped(hf, bx - 71, by + 4.5f, w, hf->src_h, NULL);
  img_draw("images/ui/text_images/en/battle_ui/overlay_hp_label", NULL, bx - 72, by + 1.5f, 1, 0);
}

/* ------------------------------------------------------------------ drawing: bottom strip */

static void draw_command_menu(void) {
  ui_window(202, -48, 118, 48, WIN_NORMAL);
  static const char* const KEYS[4] = {"commandUiHandler:fight", "commandUiHandler:ball", "commandUiHandler:pokemon",
                                      "commandUiHandler:run"};
  for (int c = 0; c < 4; c++) {
    char s[32];
    i18n_t(s, sizeof s, KEYS[c], NULL);
    text_draw(TS_WINDOW_BATTLE_COMMAND, 217 + (c % 2 ? 55.8f : 0), -38.7f + (c < 2 ? 0 : 16), s, NULL);
  }
  ui_cursor(217 - 5 + (B.cmd_cursor % 2 ? 56 : 0), -38.7f + 8 + (B.cmd_cursor >= 2 ? 16 : 0));
}

static text_style pp_style(int left, int max) {
  float r = max ? (float)left / max : 1;
  if (r > 0.25f && r <= 0.5f) return TS_MOVE_PP_HALF_FULL;
  if (r > 0 && r <= 0.25f) return TS_MOVE_PP_NEAR_EMPTY;
  if (r == 0) return TS_MOVE_PP_EMPTY;
  return TS_MOVE_PP_FULL;
}

static void draw_fight_menu(void) {
  const bv_mon* m = &B.s[0].mon;
  ui_window(0, -48, 243, 48, WIN_NORMAL);
  ui_window(240, -48, 80, 48, WIN_NORMAL);
  for (int i = 0; i < 4; i++) {
    const char* nm = i < m->n_moves ? i18n_move_name(m->moves[i]) : "-";
    text_draw(TS_WINDOW, 18 + (i % 2 ? 114 : 0), -38.7f + (i < 2 ? 0 : 16), nm, NULL);
  }
  ui_cursor(13 + (B.move_cursor % 2 ? 114 : 0), -31 + (B.move_cursor >= 2 ? 15 : 0));
  if (B.move_cursor >= m->n_moves) return;
  int mv = m->moves[B.move_cursor];
  const pr_move* pm = mv >= 0 && mv < PR_MOVE_COUNT ? &pr_moves[mv] : NULL;
  char fr[24];
  /* moveInfoContainer at (1, 0) */
  rr_draw_opts o = RR_DEFAULT_OPTS;
  o.origin_x = o.origin_y = 0.5f;
  o.scale_x = o.scale_y = 0.8f;
  if (pm) {
    img_draw_opts("images/types", gm_enum_lower(GM_E_POKEMON_TYPE, pm->type, fr, sizeof fr), 1 + 320 - 57, -36, &o);
    img_draw("images/categories", gm_enum_lower(GM_E_MOVE_CATEGORY, pm->category, fr, sizeof fr), 1 + 320 - 25, -36,
             0.5f, 0.5f);
  }
  text_opts lo = text_defaults();
  lo.origin_y = 0.5f;
  text_opts ro = text_defaults();
  ro.origin_x = 1;
  ro.origin_y = 0.5f;
  char s[32];
  text_draw(TS_MOVE_INFO_CONTENT, 1 + 320 - 70, -26, i18n_t(s, sizeof s, "fightUiHandler:pp", NULL), &lo);
  text_draw(TS_MOVE_INFO_CONTENT, 1 + 320 - 70, -18, i18n_t(s, sizeof s, "fightUiHandler:power", NULL), &lo);
  text_draw(TS_MOVE_INFO_CONTENT, 1 + 320 - 70, -10, i18n_t(s, sizeof s, "fightUiHandler:accuracy", NULL), &lo);
  int left = m->pp_left[B.move_cursor], max = m->pp_max[B.move_cursor];
  snprintf(s, sizeof s, "%2d/%2d", left, max);
  text_style ps = pp_style(left, max);
  text_opts po = ro;
  po.color = text_color(ps, false);
  po.shadow = text_color(ps, true);
  text_draw(TS_MOVE_INFO_CONTENT, 1 + 320 - 12, -26, s, &po);
  if (pm) {
    if (pm->power >= 0)
      snprintf(s, sizeof s, "%d", pm->power);
    else
      snprintf(s, sizeof s, "---");
    text_draw(TS_MOVE_INFO_CONTENT, 1 + 320 - 12, -18, s, &ro);
    if (pm->accuracy >= 0)
      snprintf(s, sizeof s, "%d", pm->accuracy);
    else
      snprintf(s, sizeof s, "---");
    text_draw(TS_MOVE_INFO_CONTENT, 1 + 320 - 12, -10, s, &ro);
  }
}

/* The free part of the bottom screen above the strip (y -240..-48): the move's description in
 * the fight menu (upstream shows it on the info button, MoveInfoOverlay). */
static void draw_move_details(void) {
  const bv_mon* m = &B.s[0].mon;
  if (B.move_cursor >= m->n_moves) return;
  char k[96], c[64], s[400];
  snprintf(k, sizeof k, "move:%s.effect", gm_enum_camel(GM_E_MOVE_ID, m->moves[B.move_cursor], c, sizeof c));
  i18n_t(s, sizeof s, k, NULL);
  ui_window(0, -48 - 52, 320, 52, WIN_NORMAL);
  text_opts o = text_defaults();
  o.wrap_w = 320 - 16;
  o.max_lines = 3;
  text_draw(TS_WINDOW, 8, -48 - 52 + 4, s, &o);
}

static void draw_ability_bar(void) {
  if (B.ability_t <= 0) return;
  float w = text_width(TS_WINDOW, B.ability_text, NULL) + 16;
  ui_window(0, -48 - 30, w, 26, WIN_THIN);
  text_draw(TS_WINDOW, 8, -48 - 30 + 5, B.ability_text, NULL);
}

static void draw_bottom(void) {
  /* the strip's background: bg frame <window style> at (0,0), origin (0,1) */
  char fr[4];
  snprintf(fr, sizeof fr, "%d", ui_window_style());
  if (B.mode != M_FIGHT) {
    if (img_frame("images/ui/bg", fr))
      img_draw("images/ui/bg", fr, 0, 0, 0, 1);
    else
      rr_rect(0, -48, 320, 48, RR_RGBA(0x30, 0x50, 0x68, 255));
  }
  if (B.mode == M_COMMAND) draw_command_menu();
  if (B.mode == M_FIGHT) {
    draw_fight_menu();
    draw_move_details();
  } else {
    msgbox_draw(&B.msg, 12, -39);
  }
  draw_ability_bar();
}

static void draw_debug(void) {
  if (!ui_space(UI_PHYS_BOTTOM)) return;
  rr_stats st = rr_get_stats();
  char d[160];
  snprintf(d, sizeof d, "%.0f fps  cpu %.1f ms  gpu %.1f ms  tex %.1f MB", B.fps, st.cpu_ms, st.gpu_ms,
           st.tex_bytes / 1048576.0);
  rr_text_sys(2, 2, 0.4f, d, RR_RGBA(0x90, 0x90, 0x98, 255));
}

/* ------------------------------------------------------------------ events */

static void finish_event(void) {
  B.cur++;
  B.step = 0;
  B.t = 0;
}

static float hp_tween_ms(float from, float to) {
  float d = fabsf(from - to) * 5; /* Phaser.Math.Clamp(|lastHp - hp| * 5, 250, 5000) */
  return d < 250 ? 250 : d > 5000 ? 5000 : d;
}

static void run_event(float dt) {
  bv_event* e = &B.ev[B.cur];
  side* sd = side_of(e->battler);
  bool player = e->battler < 2;
  B.t += dt;
  switch (e->type) {
    case BEV_MESSAGE:
      if (B.step == 0) {
        msgbox_show(&B.msg, e->text ? e->text : "", -1, e->prompt);
        B.msg.wait_ms = e->a ? e->a : e->prompt ? 0 : 1500; /* callbackDelay || (prompt ? 0 : 1500) */
        B.step = 1;
      }
      if (msgbox_done(&B.msg)) finish_event();
      break;

    case BEV_INTRO_WILD:
      if (B.step == 0) {
        side* en = &B.s[1];
        en->present = true;
        en->black = true;
        en->info_on = false;
        tween_start(&B.tw, 0, 300, 2000, EASE_LINEAR);
        B.step = 1;
      }
      if (B.step == 1) {
        tween_update(&B.tw, dt);
        B.arena_player_x = 300 - B.tw.value;
        B.arena_enemy_x = -280 + B.tw.value;
        B.enemy_slide = -300 + B.tw.value;
        if (tween_done(&B.tw)) {
          side* en = &B.s[1];
          en->black = false; /* untint(100) */
          cry(&en->mon);
          en->info_on = true;
          en->info_dx = -150;
          tween_start(&B.tw2, -150, 0, 1000, EASE_CUBIC_OUT);
          char s[128];
          i18n_t(s, sizeof s, "battle:singleWildAppeared", "pokemonName", en->mon.name, NULL);
          msgbox_show(&B.msg, s, -1, false);
          B.msg.wait_ms = 1500;
          B.step = 2;
        }
      }
      if (B.step == 2) {
        tween_update(&B.tw2, dt);
        B.s[1].info_dx = B.tw2.value;
        if (msgbox_done(&B.msg) && tween_done(&B.tw2)) finish_event();
      }
      break;

    case BEV_SUMMON:
      if (B.step == 0) {
        if (e->mon) set_mon(sd, e->mon, player);
        sd->present = true;
        sd->scale = 0;
        sd->info_on = false;
        if (player) {
          char s[128];
          if (e->text)
            snprintf(s, sizeof s, "%s", e->text);
          else
            i18n_t(s, sizeof s, "battle:playerGo", "pokemonName", sd->mon.name, NULL);
          msgbox_show(&B.msg, s, -1, false);
          B.msg.wait_ms = 0;
        }
        B.step = 1;
        B.t = 0;
      }
      if (B.step == 1 && B.t >= (player ? 750 : 250)) {
        /* the ball opens: the Pokemon grows in, cries, and its info box slides in */
        ui_play_sound("se/pb_rel");
        tween_start(&B.tw, 0, 1, 250, EASE_SINE_IN);
        B.step = 2;
      }
      if (B.step == 2) {
        tween_update(&B.tw, dt);
        sd->scale = B.tw.value;
        if (tween_done(&B.tw)) {
          cry(&sd->mon);
          sd->info_on = true;
          sd->info_dx = player ? 150 : -150;
          tween_start(&B.tw2, sd->info_dx, 0, 1000, EASE_CUBIC_OUT);
          B.step = 3;
        }
      }
      if (B.step == 3) {
        tween_update(&B.tw2, dt);
        sd->info_dx = B.tw2.value;
        if (tween_done(&B.tw2)) finish_event();
      }
      break;

    case BEV_WITHDRAW:
      if (B.step == 0) {
        tween_start(&B.tw, 1, 0, 250, EASE_SINE_IN);
        tween_start(&B.tw2, 0, player ? 150 : -150, 500, EASE_CUBIC_IN);
        B.step = 1;
      }
      tween_update(&B.tw, dt);
      tween_update(&B.tw2, dt);
      sd->scale = B.tw.value;
      sd->info_dx = B.tw2.value;
      if (tween_done(&B.tw) && tween_done(&B.tw2)) {
        sd->present = false;
        finish_event();
      }
      break;

    case BEV_MOVE_ANIM: {
      ba_mon field[4];
      anim_field(field);
      if (B.step == 0) {
        B.step = 1;
        if (!ba_play((ba_kind)e->b, e->a, e->battler, e->target, field)) {
          finish_event();
          break;
        }
      }
      ba_update(dt, field);
      if (!ba_active()) finish_event();
      break;
    }

    case BEV_DAMAGE:
      if (B.step == 0) {
        /* DamageAnimPhase. HitResult: 1 EFFECTIVE, 2 EXTREMELY_, 3 SUPER_, 4 NOT_VERY_, 5 MOSTLY_INEFFECTIVE,
         * 6 ONE_HIT_KO, 12 INDIRECT, 14 CONFUSION, 15 INDIRECT_KO. A KO waits 1 s first (upstream inverts
         * the colours meanwhile; not done here). */
        B.ko_wait = e->b == 6 || e->b == 15 ? 1000 : 0;
        B.step = 1;
        B.t = 0;
      }
      if (B.step == 1 && B.t >= B.ko_wait) {
        int r = e->b;
        ui_play_sound(r == 1 || r == 14                        ? "se/hit"
                      : r == 2 || r == 3 || r == 6 || r == 15  ? "se/hit_strong"
                      : r == 4 || r == 5                       ? "se/hit_weak"
                                                               : NULL);
        /* the sprite blinks (hidden, shown... six times 100 ms apart) unless indirect or no damage */
        B.blink = r != 12 && e->a < sd->mon.hp;
        B.step = 2;
        B.t = 0;
      }
      if (B.step == 2) {
        int k = (int)(B.t / 100);
        if (B.blink) sd->sprite_on = k % 2 == 1 || k >= 5;
        if (!B.blink || B.t >= 500) {
          sd->sprite_on = true;
          tween_start(&B.tw, sd->hp_shown, (float)e->a, hp_tween_ms(sd->hp_shown, (float)e->a), EASE_SINE_OUT);
          sd->mon.hp = e->a;
          B.step = 3;
        }
      }
      if (B.step == 3) {
        tween_update(&B.tw, dt);
        sd->hp_shown = B.tw.value;
        if (tween_done(&B.tw)) finish_event();
      }
      break;

    case BEV_HEAL:
      if (B.step == 0) {
        tween_start(&B.tw, sd->hp_shown, (float)e->a, hp_tween_ms(sd->hp_shown, (float)e->a), EASE_SINE_OUT);
        sd->mon.hp = e->a;
        B.step = 1;
      }
      tween_update(&B.tw, dt);
      sd->hp_shown = B.tw.value;
      if (tween_done(&B.tw)) finish_event();
      break;

    case BEV_STATUS:
      sd->mon.status = e->a;
      finish_event();
      break;

    case BEV_STAT_STAGES:
      if (B.step == 0) {
        ui_play_sound(e->b > 0 ? "se/stat_up" : "se/stat_down");
        if (e->a >= 1 && e->a <= 7) {
          int v = sd->mon.stat_stages[e->a - 1] + e->b;
          sd->mon.stat_stages[e->a - 1] = (int8_t)(v > 6 ? 6 : v < -6 ? -6 : v);
        }
        B.step = 1;
      }
      if (B.t >= 600) finish_event();
      break;

    case BEV_ABILITY:
      if (B.step == 0) {
        char s[64];
        snprintf(s, sizeof s, "%s", i18n_ability_name(e->a));
        i18n_t(B.ability_text, sizeof B.ability_text, "fightUiHandler:abilityFlyInText", "pokemonName",
               sd->mon.name, "passive", "", "abilityName", s, NULL);
        for (char* p = B.ability_text; *p; p++)
          if (*p == '\n') *p = ' ';
        B.ability_t = 1500;
        B.step = 1;
      }
      if (B.t >= 500) finish_event();
      break;

    case BEV_FAINT:
      if (B.step == 0) {
        cry(&sd->mon);
        B.step = 1;
        B.t = 0;
      }
      if (B.step == 1 && B.t >= 250) {
        ui_play_sound("se/faint");
        tween_start(&B.tw, 0, 150, 500, EASE_SINE_IN);
        tween_start(&B.tw2, sd->info_dx, sd->info_dx + (player ? 150 : -150), 500, EASE_CUBIC_IN);
        B.step = 2;
      }
      if (B.step == 2) {
        tween_update(&B.tw, dt);
        tween_update(&B.tw2, dt);
        sd->drop = B.tw.value;
        sd->info_dx = B.tw2.value;
        if (tween_done(&B.tw) && tween_done(&B.tw2)) {
          sd->present = false;
          sd->info_on = false;
          finish_event();
        }
      }
      break;

    case BEV_EXP:
      if (B.step == 0) {
        float to = e->a / 1000.0f;
        if (e->b > 0) to = 1;
        tween_start(&B.tw, sd->exp_shown, to, 1650 * fabsf(to - sd->exp_shown), EASE_SINE_IN);
        if (B.tw.active) ui_play_sound("se/exp");
        B.step = 1;
      }
      if (B.step == 1) {
        tween_update(&B.tw, dt);
        sd->exp_shown = B.tw.value;
        if (tween_done(&B.tw)) {
          if (e->b > 0) {
            ui_play_sound("se/level_up");
            sd->mon.level = e->b;
            sd->exp_shown = 0;
            tween_start(&B.tw, 0, e->a / 1000.0f, 1650 * (e->a / 1000.0f), EASE_SINE_IN);
            B.step = 2;
          } else {
            sd->mon.exp_ratio = sd->exp_shown;
            finish_event();
          }
        }
      }
      if (B.step == 2) {
        tween_update(&B.tw, dt);
        sd->exp_shown = B.tw.value;
        if (tween_done(&B.tw)) {
          sd->mon.exp_ratio = sd->exp_shown;
          finish_event();
        }
      }
      break;

    case BEV_WAIT:
      if (B.t >= e->a) finish_event();
      break;
  }
}

/* ------------------------------------------------------------------ the screen */

static bool bv_input(ui_screen* s, ui_button b) {
  (void)s;
  if (B.mode == M_EVENTS) return msgbox_input(&B.msg, b);
  if (B.mode == M_COMMAND) {
    int c = B.cmd_cursor;
    switch (b) {
      case BTN_UP:
        if (c >= 2) B.cmd_cursor -= 2;
        break;
      case BTN_DOWN:
        if (c < 2) B.cmd_cursor += 2;
        break;
      case BTN_LEFT:
        if (c % 2) B.cmd_cursor--;
        break;
      case BTN_RIGHT:
        if (!(c % 2)) B.cmd_cursor++;
        break;
      case BTN_ACTION:
        ui_play_select();
        if (c == 0) {
          B.mode = M_FIGHT;
          return true;
        }
        if (c == 3 || c == 1 || c == 2) {
          /* BALL and POKEMON open the ball and party screens (the menus thread); RUN answers */
          B.mode = M_IDLE;
          msgbox_clear(&B.msg);
          if (B.on_command) B.on_command((bv_command){c, 0}, B.command_arg);
          return true;
        }
        return true;
      default:
        return false;
    }
    if (B.cmd_cursor != c) ui_play_select();
    return B.cmd_cursor != c;
  }
  if (B.mode == M_FIGHT) {
    int c = B.move_cursor;
    switch (b) {
      case BTN_UP:
        if (c >= 2) B.move_cursor -= 2;
        break;
      case BTN_DOWN:
        if (c < 2) B.move_cursor += 2;
        break;
      case BTN_LEFT:
        if (c % 2) B.move_cursor--;
        break;
      case BTN_RIGHT:
        if (!(c % 2)) B.move_cursor++;
        break;
      case BTN_CANCEL:
        ui_play_select();
        B.mode = M_COMMAND;
        return true;
      case BTN_ACTION: {
        const bv_mon* m = &B.s[0].mon;
        if (c >= m->n_moves || m->pp_left[c] <= 0) {
          ui_play_error();
          return false;
        }
        ui_play_select();
        B.mode = M_IDLE;
        msgbox_clear(&B.msg);
        if (B.on_command) B.on_command((bv_command){0, c}, B.command_arg);
        return true;
      }
      default:
        return false;
    }
    if (B.move_cursor != c) ui_play_select();
    return B.move_cursor != c;
  }
  return false;
}

static void bv_update(ui_screen* s, float dt) {
  (void)s;
  float frame_dt = dt;
  msgbox_update(&B.msg, dt);
  if (B.ability_t > 0) B.ability_t -= dt;
  if (B.mode == M_EVENTS) {
    while (B.cur < B.n_ev) {
      int before = B.cur;
      run_event(dt);
      dt = 0; /* only the first event this frame gets the time */
      if (B.cur == before) break;
    }
    if (B.cur >= B.n_ev) {
      B.mode = M_IDLE;
      if (B.on_done) {
        void (*fn)(void*) = B.on_done;
        B.on_done = NULL;
        fn(B.done_arg);
      }
    }
  }
  B.fps_frames++;
  B.fps_t += frame_dt;
  if (B.fps_t >= 1000) {
    B.fps = B.fps_frames * 1000.0f / B.fps_t;
    B.fps_frames = 0;
    B.fps_t = 0;
  }
}

static void bv_draw(ui_screen* s, rr_screen which) {
  (void)s;
  if (which == RR_TOP) {
    if (ui_space(UI_FIELD)) draw_field();
    if (ui_space(UI_FIELD_UI)) {
      draw_enemy_info(&B.s[1]);
      draw_player_info(&B.s[0]);
    }
    /* the field is 320x180 on a 400x240 screen: keep sprites that slide past it off the border */
    if (ui_space(UI_PHYS_TOP)) {
      rr_rect(0, 0, 40, 240, RR_RGBA(0, 0, 0, 255));
      rr_rect(360, 0, 40, 240, RR_RGBA(0, 0, 0, 255));
      rr_rect(0, 0, 400, 30, RR_RGBA(0, 0, 0, 255));
      rr_rect(0, 210, 400, 30, RR_RGBA(0, 0, 0, 255));
    }
  } else {
    if (ui_space(UI_PHYS_BOTTOM)) rr_rect(0, 0, 320, 240, RR_RGBA(0x10, 0x10, 0x18, 255));
    if (ui_space(UI_BOTTOM_UI)) draw_bottom();
    draw_debug();
  }
}

static ui_screen g_battle = {"battle", 0, NULL, NULL, NULL, bv_input, bv_update, bv_draw, NULL};

void bv_begin(const bv_setup* setup) {
  memset(&B, 0, sizeof B);
  B.setup = *setup;
  gm_enum_lower(GM_E_BIOME_ID, setup->biome, B.biome, sizeof B.biome);
  set_mon(&B.s[0], &setup->player, true);
  set_mon(&B.s[1], &setup->enemy, false);
  B.arena_player_x = 300;
  B.arena_enemy_x = -280;
  B.enemy_slide = -300;
  msgbox_init(&B.msg, TS_MESSAGE, 1780 / 6.0f, 2);
  B.mode = M_IDLE;
  ui_set(&g_battle);
}

void bv_play(const bv_event* ev, int n, void (*on_done)(void*), void* arg) {
  B.n_ev = 0;
  B.cur = 0;
  B.step = 0;
  B.t = 0;
  for (int i = 0; i < n && i < EV_MAX; i++) {
    B.ev[i] = ev[i];
    if (ev[i].text) {
      snprintf(B.ev_text[i], EV_TEXT, "%s", ev[i].text);
      B.ev[i].text = B.ev_text[i];
    }
    if (ev[i].mon) {
      B.ev_mon[i] = *ev[i].mon;
      B.ev[i].mon = &B.ev_mon[i];
    }
    B.n_ev++;
  }
  B.on_done = on_done;
  B.done_arg = arg;
  B.mode = M_EVENTS;
  B.msg.opts.wrap_w = 1780 / 6.0f;
}

bool bv_playing(void) { return B.mode == M_EVENTS; }

void bv_ask_command(void (*on_command)(bv_command, void*), void* arg) {
  B.on_command = on_command;
  B.command_arg = arg;
  B.mode = M_COMMAND;
  if (B.cmd_cursor == 2) B.cmd_cursor = 0; /* upstream resets POKEMON to FIGHT */
  /* "What will X do?", wrapped to the space left of the command window (1110 / 6) */
  B.msg.opts.wrap_w = 1110 / 6.0f;
  char s[128];
  i18n_t(s, sizeof s, "commandUiHandler:actionMessage", "pokemonName", B.s[0].mon.name, NULL);
  msgbox_show(&B.msg, s, 0, false);
  B.msg.wait_ms = 1e9f;
  (void)name_with_affix;
}
