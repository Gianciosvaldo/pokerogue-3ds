/*
 * PokeRogue 3DS skeleton: boots, draws a battle scene on both screens from
 * upstream assets on the SD card, and saves progress to the SD card.
 *
 * Screen split (a default, easy to change with rr_set_origin):
 *   top    400x240: the 320x180 battle field at 1:1, centred (origin 40,30)
 *   bottom 320x240: the message / command window strip (upstream y 132..180)
 *                   at the bottom (origin 0,60), debug info above it.
 * Positions below are upstream's own numbers (field coords, and fieldUI/ui
 * coords where upstream uses y relative to the canvas bottom = 180).
 *
 * Controls: D-pad moves the command cursor, A confirms (FIGHT deals damage),
 * X toggles a sprite stress test, SELECT toggles debug text, START saves and quits.
 */
#include <3ds.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pak.h"
#include "rr.h"
#include "save.h"
#include "snd.h"

int paktest_run(void);

#define ASSETS "sdmc:/3ds/pokerogue/assets/"
#define UI_Y(y) (180.0f + (y)) /* upstream ui/fieldUI containers sit at canvas bottom */

/* ---------------------------------------------------------------- assets */

typedef struct {
  rr_tex* arena_bg;
  rr_tex* arena_a;
  rr_tex* arena_b;
  rr_atlas* enemy;
  rr_atlas* player;
  rr_tex* pbinfo_enemy;
  rr_tex* pbinfo_player;
  rr_atlas* hp_overlay;
  rr_tex* exp_overlay;
  rr_atlas* msg_bg;
  rr_tex* window;
  rr_tex* cursor;
  rr_font* font_msg;  /* TextStyle.MESSAGE / WINDOW: 96px / 6 = 16 */
  rr_font* font_info; /* TextStyle.BATTLE_INFO: 72px / 6 = 12 */
  int missing;
  char first_missing[128];
} assets;

static void note_missing(assets* a, const char* p, const void* loaded) {
  if (loaded) return;
  if (!a->missing++) snprintf(a->first_missing, sizeof a->first_missing, "%s", p);
}

/* Asset packs from the asset pipeline take precedence over loose upstream files. */
static bool g_paks;

static void key_of(char* out, size_t n, const char* rel) {
  snprintf(out, n, "%s", rel);
  char* dot = strrchr(out, '.');
  if (dot && !strcmp(dot, ".png")) *dot = 0;
}

static rr_tex* load_tex(assets* a, const char* rel) {
  char p[256];
  if (g_paks) {
    key_of(p, sizeof p, rel);
    rr_tex* t = rr_tex_load_key(p);
    if (t) return t;
  }
  snprintf(p, sizeof p, ASSETS "%s", rel);
  rr_tex* t = rr_tex_load_png(p);
  note_missing(a, rel, t);
  return t;
}

static rr_atlas* load_atlas(assets* a, const char* rel_noext) {
  char png[256], json[256];
  if (g_paks) {
    rr_atlas* at = rr_atlas_load_key(rel_noext);
    if (at) return at;
  }
  snprintf(png, sizeof png, ASSETS "%s.png", rel_noext);
  snprintf(json, sizeof json, ASSETS "%s.json", rel_noext);
  rr_atlas* at = rr_atlas_load(png, json);
  note_missing(a, rel_noext, at);
  return at;
}

static rr_font* load_font(assets* a, const char* rel, int px) {
  char p[256];
  if (g_paks) {
    snprintf(p, sizeof p, "fonts/emerald@%d", px);
    rr_font* f = rr_font_load_key(p);
    if (f) return f;
  }
  snprintf(p, sizeof p, ASSETS "%s", rel);
  rr_font* f = rr_font_load(p, px);
  note_missing(a, rel, f);
  return f;
}

static void assets_load(assets* a) {
  memset(a, 0, sizeof *a);
  a->arena_bg = load_tex(a, "images/arenas/grass_bg.png");
  a->arena_a = load_tex(a, "images/arenas/grass_a.png");
  a->arena_b = load_tex(a, "images/arenas/grass_b.png");
  a->enemy = load_atlas(a, "images/pokemon/445");      /* Garchomp, front */
  a->player = load_atlas(a, "images/pokemon/back/257"); /* Blaziken, back */
  a->pbinfo_enemy = load_tex(a, "images/ui/pbinfo_enemy_mini.png");
  a->pbinfo_player = load_tex(a, "images/ui/pbinfo_player.png");
  a->hp_overlay = load_atlas(a, "images/ui/overlay_hp");
  a->exp_overlay = load_tex(a, "images/ui/overlay_exp.png");
  a->msg_bg = load_atlas(a, "images/ui/bg");
  a->window = load_tex(a, "images/ui/windows/window_1.png");
  a->cursor = load_tex(a, "images/ui/cursor.png");
  a->font_msg = load_font(a, "fonts/pokemon-emerald-pro.ttf", 16);
  a->font_info = load_font(a, "fonts/pokemon-emerald-pro.ttf", 12);
}

/* Draws a texture, or a labelled placeholder box when it is missing. */
static void draw_tex_or_box(rr_tex* t, float x, float y, float w, float h, const rr_draw_opts* o, rr_color c) {
  if (t) {
    rr_frame f = rr_tex_frame(t);
    rr_draw(&f, x, y, o);
  } else {
    float ox = o ? o->origin_x * w : 0, oy = o ? o->origin_y * h : 0;
    rr_rect(x - ox, y - oy, w, h, c);
  }
}

/* ---------------------------------------------------------------- game state */

typedef struct {
  int boots;
  int wave;
  int enemy_hp, enemy_max;
  int player_hp, player_max;
  int exp; /* 0..85 px of the EXP bar */
} game_state;

static void state_default(game_state* s) {
  s->boots = 0;
  s->wave = 1;
  s->enemy_max = s->enemy_hp = 357;
  s->player_max = s->player_hp = 301;
  s->exp = 20;
}

static int json_int(const char* js, const char* key, int dflt) {
  char pat[64];
  snprintf(pat, sizeof pat, "\"%s\":", key);
  const char* p = strstr(js, pat);
  return p ? atoi(p + strlen(pat)) : dflt;
}

static char g_save_msg[96];

static void state_load(game_state* s) {
  state_default(s);
  char* buf;
  size_t len;
  bool bak;
  save_result r = save_read("session_0", &buf, &len, &bak);
  if (r == SAVE_OK) {
    s->boots = json_int(buf, "boots", 0);
    s->wave = json_int(buf, "wave", 1);
    s->enemy_hp = json_int(buf, "enemyHp", s->enemy_max);
    s->player_hp = json_int(buf, "playerHp", s->player_max);
    s->exp = json_int(buf, "exp", 20);
    free(buf);
    snprintf(g_save_msg, sizeof g_save_msg, "Loaded save%s: wave %d", bak ? " (from backup)" : "", s->wave);
  } else {
    snprintf(g_save_msg, sizeof g_save_msg, "No save loaded (%s)", save_result_str(r));
  }
}

static void state_save(const game_state* s) {
  char js[256];
  int n = snprintf(js, sizeof js,
                   "{\"boots\":%d,\"wave\":%d,\"enemyHp\":%d,\"playerHp\":%d,\"exp\":%d,\"savedAt\":%llu}",
                   s->boots, s->wave, s->enemy_hp, s->player_hp, s->exp, (unsigned long long)osGetTime());
  save_result r = save_write("session_0", js, n);
  snprintf(g_save_msg, sizeof g_save_msg, "Saved wave %d: %s", s->wave, save_result_str(r));
}

/* ---------------------------------------------------------------- UI pieces */

static const rr_color MSG_COL = RR_RGBA(0xf8, 0xf8, 0xf8, 255);
static const rr_color MSG_SHADOW = RR_RGBA(0x6b, 0x5a, 0x73, 255);

static void draw_hp_bar(assets* a, float x, float y, float frac) {
  /* overlay_hp.json has "high"/"medium"/"low" frames; upstream crops the width */
  const char* name = frac > 0.5f ? "high" : frac > 0.25f ? "medium" : "low";
  const rr_frame* f = rr_atlas_find(a->hp_overlay, name);
  if (!f) f = rr_atlas_frame(a->hp_overlay, 0);
  int w = (int)ceilf(48 * frac);
  if (f)
    rr_draw_cropped(f, x, y, w, 6, NULL);
  else
    rr_rect(x, y, w, 2, RR_RGBA(0x58, 0xd0, 0x80, 255));
}

static void draw_info_boxes(assets* a, const game_state* s, float enemy_frac, float player_frac) {
  rr_draw_opts o = RR_DEFAULT_OPTS;
  o.origin_x = 1;
  o.origin_y = 0.5f;
  char buf[32];

  /* EnemyBattleInfo: container (140, -141) in fieldUI */
  float ex = 140, ey = UI_Y(-141);
  draw_tex_or_box(a->pbinfo_enemy, ex, ey, 130, 31, &o, RR_RGBA(0xf0, 0xf0, 0xd8, 255));
  rr_text(a->font_info, ex - 124, ey - 11.2f, "Garchomp", MSG_COL, MSG_SHADOW);
  rr_text(a->font_info, ex - 50 + 2, ey - 5 - 6, "Lv.100", MSG_COL, MSG_SHADOW);
  draw_hp_bar(a, ex - 71, ey + 4.5f, enemy_frac);

  /* PlayerBattleInfo: container (320 - 10, -72) in fieldUI */
  float px = 310, py = UI_Y(-72);
  draw_tex_or_box(a->pbinfo_player, px, py, 130, 42, &o, RR_RGBA(0xf0, 0xf0, 0xd8, 255));
  rr_text(a->font_info, px - 115, py - 15.2f, "Blaziken", MSG_COL, MSG_SHADOW);
  rr_text(a->font_info, px - 41 + 2, py - 10 - 6, "Lv.100", MSG_COL, MSG_SHADOW);
  draw_hp_bar(a, px - 61, py - 1, player_frac);
  snprintf(buf, sizeof buf, "%d/%d", s->player_hp, s->player_max);
  rr_text(a->font_info, px - 15 - rr_text_width(a->font_info, buf), py + 10 - 6, buf, MSG_COL, MSG_SHADOW);
  if (a->exp_overlay) {
    rr_frame f = rr_tex_frame(a->exp_overlay);
    rr_draw_cropped(&f, px - 98, py + 18, s->exp, 2, NULL);
  }
}

static const char* COMMANDS[4] = {"FIGHT", "BALL", "POKéMON", "RUN"};

static void draw_bottom_ui(assets* a, const char* msg, int shown_chars, int cursor, bool show_commands,
                           bool blink) {
  /* bg frame "1" (window style 1): 320x48, origin (0,1) at ui y=0 */
  const rr_frame* bgf = rr_atlas_find(a->msg_bg, "1");
  if (bgf) {
    rr_draw_opts o = RR_DEFAULT_OPTS;
    o.origin_y = 1;
    rr_draw(bgf, 0, UI_Y(0), &o);
  } else {
    rr_rect(0, UI_Y(-48), 320, 48, RR_RGBA(0x30, 0x50, 0x68, 255));
  }

  /* message text at container (12, -39), two lines max */
  char line[160];
  snprintf(line, sizeof line, "%.*s", shown_chars, msg);
  rr_text(a->font_msg, 12, UI_Y(-39), line, MSG_COL, MSG_SHADOW);

  if (show_commands) {
    /* command window: addWindow(202, -48?, 118, 48); upstream adds it at y 0 with origin 0
       inside the ui container, then positions via the handler; drawn at the strip's right. */
    if (a->window) {
      rr_frame wf = rr_tex_frame(a->window);
      rr_nineslice(&wf, 202, UI_Y(-48), 118, 48, 8);
    } else {
      rr_rect(202, UI_Y(-48), 118, 48, RR_RGBA(0xf8, 0xf8, 0xf8, 255));
    }
    float cx = 217, cy = UI_Y(-38.7f);
    for (int c = 0; c < 4; c++)
      rr_text(a->font_msg, cx + (c % 2 ? 55.8f : 0), cy + (c < 2 ? 0 : 16), COMMANDS[c],
              RR_RGBA(0xf8, 0xf8, 0xf8, 255), MSG_SHADOW);
    rr_draw_opts o = RR_DEFAULT_OPTS;
    o.origin_x = o.origin_y = 0.5f; /* Phaser image default origin */
    float kx = cx - 5 + (cursor % 2 ? 56 : 0), ky = cy + 8 + (cursor >= 2 ? 16 : 0);
    if (a->cursor) {
      rr_frame f = rr_tex_frame(a->cursor);
      rr_draw(&f, kx, ky, &o);
    } else {
      rr_rect(kx - 3, ky - 5, 6, 10, RR_RGBA(0x40, 0x40, 0x40, 255));
    }
  } else if (blink && shown_chars >= (int)strlen(msg)) {
    rr_rect(300, UI_Y(-12), 6, 4, MSG_COL); /* stand-in for the animated prompt arrow */
  }
}

/* ---------------------------------------------------------------- main */

typedef enum { MODE_MESSAGE, MODE_COMMAND } ui_mode;

int main(void) {
  osSetSpeedupEnable(true); /* no-op on Old 3DS; lets a New 3DS run at 804 MHz */
  if (!rr_init()) return 1;
  if (paktest_run()) {
    rr_exit();
    return 0;
  }
  g_paks = pak_mount_default() > 0;
  bool save_ok = save_init();
  bool is_new = false;
  APT_CheckNew3DS(&is_new);

  u64 t_load = osGetTime();
  assets A;
  assets_load(&A);
  t_load = osGetTime() - t_load;

  game_state S;
  state_load(&S);
  S.boots++;

  char msg[160];
  snprintf(msg, sizeof msg, "A wild Garchomp appeared!\n(boot #%d, wave %d)", S.boots, S.wave);
  ui_mode mode = MODE_MESSAGE;
  int shown = 0, cursor = 0;
  bool debug = true, stress = false;
  float enemy_disp = (float)S.enemy_hp / S.enemy_max; /* tweened HP fraction */
  float player_disp = (float)S.player_hp / S.player_max;
  u32 frame = 0;
  u64 fps_t0 = osGetTime();
  int fps_frames = 0;
  float fps = 0;

  while (aptMainLoop()) {
    hidScanInput();
    u32 down = hidKeysDown();
    if (down & KEY_START) break;
    if (down & KEY_SELECT) debug = !debug;
    if (down & KEY_X) stress = !stress;

    if (mode == MODE_MESSAGE) {
      if (frame % 2 == 0 && shown < (int)strlen(msg)) shown++;
      if (down & (KEY_A | KEY_B)) {
        if (shown < (int)strlen(msg)) {
          shown = strlen(msg);
        } else {
          mode = MODE_COMMAND;
          snprintf(msg, sizeof msg, "What will\nBlaziken do?");
          shown = strlen(msg);
        }
      }
    } else {
      if (down & KEY_LEFT) cursor &= ~1;
      if (down & KEY_RIGHT) cursor |= 1;
      if (down & KEY_UP) cursor &= ~2;
      if (down & KEY_DOWN) cursor |= 2;
      if (down & KEY_A) {
        if (cursor == 0) {
          int dmg = S.enemy_max / 5 + rand() % (S.enemy_max / 5);
          S.enemy_hp = S.enemy_hp > dmg ? S.enemy_hp - dmg : 0;
          if (S.enemy_hp == 0) {
            S.wave++;
            S.enemy_hp = S.enemy_max;
            S.exp = (S.exp + 17) % 86;
            snprintf(msg, sizeof msg, "Blaziken used Blaze Kick!\nWave %d. Game saved.", S.wave);
            state_save(&S); /* autosave at wave end, like upstream's session save */
          } else {
            snprintf(msg, sizeof msg, "Blaziken used Blaze Kick!");
          }
        } else if (cursor == 3) {
          state_save(&S);
          snprintf(msg, sizeof msg, "Game saved on the SD card.\nPress START to quit.");
        } else {
          snprintf(msg, sizeof msg, "%s isn't wired up yet.", COMMANDS[cursor]);
        }
        mode = MODE_MESSAGE;
        shown = 0;
      }
    }

    /* HP bar tween: upstream animates HP changes; approach the target smoothly */
    float et = (float)S.enemy_hp / S.enemy_max, pt = (float)S.player_hp / S.player_max;
    if (S.enemy_hp == S.enemy_max && enemy_disp < et - 0.5f) enemy_disp = 0; /* fainted -> refill */
    enemy_disp += (et - enemy_disp) * 0.08f;
    player_disp += (pt - player_disp) * 0.08f;

    /* sprite animation at upstream's 10 fps */
    int anim = (frame * 10) / 60;

    rr_frame_begin();

    /* ---- top screen: battle field ---- */
    rr_target(RR_TOP, RR_RGBA(0, 0, 0, 255));
    draw_tex_or_box(A.arena_bg, 0, 0, 320, 180, NULL, RR_RGBA(0x88, 0xc8, 0xf0, 255));
    draw_tex_or_box(A.arena_b, 20, 0, 320, 132, NULL, RR_RGBA(0, 0, 0, 0)); /* arenaEnemy base */
    draw_tex_or_box(A.arena_a, 0, 0, 320, 132, NULL, RR_RGBA(0, 0, 0, 0));  /* arenaPlayer base */

    rr_draw_opts po = RR_DEFAULT_OPTS;
    po.origin_x = 0.5f;
    po.origin_y = 1.0f;
    int en = rr_atlas_count(A.enemy), pn = rr_atlas_count(A.player);
    if (en)
      rr_draw(rr_atlas_frame(A.enemy, anim % en), 236, 84, &po);
    else
      rr_rect(236 - 30, 84 - 60, 60, 60, RR_RGBA(0x50, 0x60, 0xa0, 255));
    if (stress && en) {
      for (int i = 0; i < 40; i++)
        rr_draw(rr_atlas_frame(A.enemy, (anim + i) % en), 20 + (i % 10) * 30, 60 + (i / 10) * 30, &po);
    }
    if (pn)
      rr_draw(rr_atlas_frame(A.player, anim % pn), 106, 148, &po);
    else
      rr_rect(106 - 30, 148 - 60, 60, 60, RR_RGBA(0xd0, 0x60, 0x40, 255));

    draw_info_boxes(&A, &S, enemy_disp, player_disp);

    /* ---- bottom screen: message / command strip + debug ---- */
    rr_target(RR_BOTTOM, RR_RGBA(0x18, 0x18, 0x20, 255));
    draw_bottom_ui(&A, msg, shown, cursor, mode == MODE_COMMAND, (frame / 20) % 2);

    if (debug) {
      rr_stats st = rr_get_stats();
      char d[640];
      snprintf(d, sizeof d,
               "PokeRogue 3DS skeleton - %s\n"
               "%.1f fps  cpu %.2f ms  gpu %.2f ms%s\n"
               "textures: %d, %.2f MB   load %llu ms\n"
               "linear free %.1f MB  vram free %.1f MB\n"
               "missing assets: %d%s%s\n"
               "save dir: %s\n%s\n"
               "D-pad/A: menu  X: stress  SELECT: hide  START: quit",
               is_new ? "New 3DS" : "Old 3DS", fps, st.cpu_ms, st.gpu_ms, stress ? "  [stress x41]" : "",
               st.tex_count, st.tex_bytes / 1048576.0, (unsigned long long)t_load, st.linear_free / 1048576.0,
               st.vram_free / 1048576.0, A.missing, A.missing ? " first: " : "", A.missing ? A.first_missing : "",
               save_ok ? "ok" : "FAILED", g_save_msg);
      rr_text_sys(4, 4 - 60, 0.45f, d, RR_RGBA(0xe0, 0xe0, 0xe0, 255));
    }

    rr_frame_end();

    frame++;
    fps_frames++;
    u64 now = osGetTime();
    if (now - fps_t0 >= 1000) {
      fps = fps_frames * 1000.0f / (now - fps_t0);
      fps_frames = 0;
      fps_t0 = now;
    }
  }

  state_save(&S);
  rr_exit();
  return 0;
}
