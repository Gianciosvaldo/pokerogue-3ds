/* The end of a run (see result_screen.h). */
#include "result_screen.h"

#include <stdio.h>
#include <string.h>

#include "char_sprite.h"
#include "dialogue_view.h"
#include "gm_enum_names.h"
#include "i18n.h"
#include "postmortem_screen.h"
#include <math.h>

#include "run_summary.h"
#include "toast_view.h"
#include "ui.h"
#include "ui_text.h"
#include "ui_widgets.h"

/* AchvBar: it slides in over 500 ms, is in place until 10 s after it began (showAchv's delayedCall(10000, hide)), slides out over 500 ms */
#define TOAST_IN_MS 500.0f
#define TOAST_OUT_AT_MS 10000.0f
#define TOAST_OUT_MS 500.0f

/* GameOverPhase's way into the end card of a Classic win: fadeIn(500), the rival slides in, her dialogue (one page per A), fadeOut(500),
 * she is hidden (750 under the black), then EndCardPhase's fadeIn(1000) and its prompt */
typedef enum { ST_CARD, ST_FADE_IN, ST_SLIDE, ST_TALK, ST_FADE_OUT, ST_HIDE, ST_CARD_FADE } stage_id;
#define FADE_MS 500.0f
#define CARD_FADE_MS 1000.0f
/* upstream: showCharacter(`rival_${settings.isPlayerFemale ? "m" : "f"}`) and the name `isPlayerFemale ? name : nameFemale`: the player here is the
 * male one (the end card is end_m), so the rival is Ivy */
#define RIVAL_ATLAS "images/character/rival_f"
#define RIVAL_NAME_KEY "trainerNames:rivalFemale"

static struct {
  result_args a;
  msgbox msg;
  stage_id stage;
  float stage_ms;
  bool talk;                /* the rival's ending dialogue comes first */
  bool tall;                /* the box has three lines: there are messages, or a dialogue */
  char talk_text[1024];     /* miscDialogue:ending, with its markers and "$" pages */
  char talk_name[48];
  char talk_variant[32];    /* the expression she comes in with: the first @c{} of the text */
  bool done;
  char note[192]; /* a.note copied: the caller's text may be gone by the time the screen is drawn */
  wg_gains gains;           /* a.gains copied */
  int msgs[WG_MAX], n_msgs; /* the gains that are messages, in order; page is the one in the box (-1 = the line of the result) */
  int page;
  int toasts[WG_MAX], n_toasts; /* the gains that are bars */
  int toast;                    /* the bar on screen (index into toasts), == n_toasts when none is */
  float toast_ms;               /* how long it has been up */
  wg_toast info;                /* its words, and what tv_draw gets */
  tv_info tv;
} R;

/* the box and its window grow from two lines to three when there are messages to tell: a first ribbon is two long lines */
#define BOX_LINES_PLAIN 2
#define BOX_LINES_TALL 3
#define BOX_H_PLAIN 42.0f
#define BOX_H_TALL 58.0f

static void show_page_text(const char* text) {
  char wrapped[512];
  text_opts o = text_defaults();
  o.wrap_w = 300;
  text_wrap(TS_WINDOW, text, &o, wrapped, sizeof wrapped);
  msgbox_show(&R.msg, wrapped, -1, true);
}

/* the next message of the gains: UnlockPhase / RibbonModifierRewardPhase / GameOverModifierRewardPhase play the level up fanfare with it */
static void show_gain(int page) {
  char text[256];
  R.page = page;
  wg_message_of(&R.gains.item[R.msgs[page]], text, sizeof text);
  ui_play_sound("se/level_up_fanfare");
  show_page_text(text);
}

static void start_toast(int i) {
  R.toast = i;
  R.toast_ms = 0;
  if (i >= R.n_toasts) return;
  wg_toast_of(&R.gains.item[R.toasts[i]], &R.info);
  R.tv = (tv_info){R.info.title, R.info.desc, R.info.score, R.info.icon, R.info.tier};
  ui_play_sound("se/achv");
}

static void leave_result(void) {
  R.done = true;
  if (R.a.on_done) R.a.on_done(R.a.arg);
}

/* the post-mortem's buttons: A leaves for the title, B comes back to this screen with its line waiting again */
static void on_postmortem(pm_exit how, void* arg) {
  (void)arg;
  if (how == PM_CONTINUE) {
    leave_result();
    return;
  }
  result_args again = R.a; /* result_screen_open clears R */
  again.gains = NULL;      /* what the win gave was told before the post-mortem */
  again.ending = false;    /* and so was the rival's ending */
  result_screen_open(&again);
}

/* the end card's own line, and the gains behind it */
static void show_end_card_line(void) {
  char line[160];
  if (R.a.victory)
    snprintf(line, sizeof line, "You cleared the run at wave %d!", R.a.run.wave);
  else
    snprintf(line, sizeof line, "You were defeated at wave %d.", R.a.run.wave);
  show_page_text(line);
}

static void set_stage(stage_id st) {
  R.stage = st;
  R.stage_ms = 0;
}

static bool rs_input(ui_screen* s, ui_button b) {
  (void)s;
  if (R.done) return true;
  /* the fades and the slides are not waiting for the player */
  if (R.stage != ST_CARD && R.stage != ST_TALK) return true;
  if (msgbox_input(&R.msg, b)) {
    if (msgbox_done(&R.msg)) {
      if (R.stage == ST_TALK) {
        set_stage(ST_FADE_OUT); /* the last page was answered: ui.fadeOut(500) */
        return true;
      }
      if (R.page + 1 < R.n_msgs) {
        show_gain(R.page + 1);
        return true;
      }
      const pm_args pm = {R.a.report, on_postmortem, NULL};
      if (!postmortem_open(&pm)) leave_result();
    }
    return true;
  }
  return false;
}

/* A tap on the bottom screen is A: the next page of the line, the rival's dialogue, and the way on to the post-mortem. */
static bool rs_touch(ui_screen* s, float x, float y) {
  (void)x, (void)y;
  return rs_input(s, BTN_ACTION);
}

static void rs_update(ui_screen* s, float dt) {
  (void)s;
  msgbox_update(&R.msg, dt);
  chs_update(dt);
  R.stage_ms += dt;
  switch (R.stage) {
    case ST_FADE_IN:
      if (R.stage_ms >= FADE_MS) {
        set_stage(ST_SLIDE);
        chs_show(RIVAL_ATLAS, R.talk_variant); /* showCharacter(...).then(showDialogue) */
      }
      break;
    case ST_SLIDE:
      if (!chs_busy()) {
        set_stage(ST_TALK);
        msgbox_show(&R.msg, R.talk_text, -1, true);
      }
      break;
    case ST_TALK: {
      const char* v = msgbox_take_variant(&R.msg); /* MessageUiHandler: charSprite.setVariant when the typing passes @c{} */
      if (v) chs_set_variant(v);
      break;
    }
    case ST_FADE_OUT:
      if (R.stage_ms >= FADE_MS) {
        set_stage(ST_HIDE);
        msgbox_clear(&R.msg);
        chs_hide(); /* charSprite.hide() under the black */
      }
      break;
    case ST_HIDE:
      if (!chs_busy()) {
        chs_reset();
        set_stage(ST_CARD_FADE);
      }
      break;
    case ST_CARD_FADE:
      if (R.stage_ms >= CARD_FADE_MS) {
        set_stage(ST_CARD);
        show_end_card_line(); /* EndCardPhase: fadeIn(1000), then the prompt */
      }
      break;
    case ST_CARD:
      break;
  }
  if (R.toast < R.n_toasts) {
    R.toast_ms += dt;
    if (R.toast_ms >= TOAST_OUT_AT_MS + TOAST_OUT_MS) start_toast(R.toast + 1);
  }
}

/* the bar on the top screen: Sine.easeOut in, Sine.easeIn out */
static void draw_toast(void) {
  if (R.toast >= R.n_toasts) return;
  float slide = 1.0f;
  if (R.toast_ms < TOAST_IN_MS)
    slide = sinf(R.toast_ms / TOAST_IN_MS * 1.5707963f);
  else if (R.toast_ms >= TOAST_OUT_AT_MS)
    slide = cosf((R.toast_ms - TOAST_OUT_AT_MS) / TOAST_OUT_MS * 1.5707963f); /* 1 - Sine.easeIn */
  tv_draw(&R.tv, slide);
}

/* ui.fadeIn / fadeOut: the black over both screens; 0 = clear */
static float fade_alpha(void) {
  switch (R.stage) {
    case ST_FADE_IN: return 1.0f - R.stage_ms / FADE_MS;
    case ST_FADE_OUT: return R.stage_ms / FADE_MS;
    case ST_HIDE: return 1.0f;
    case ST_CARD_FADE: return 1.0f - R.stage_ms / CARD_FADE_MS;
    default: return 0.0f;
  }
}

static void draw_fade(float w, float h) {
  float a = fade_alpha();
  if (a <= 0.0f) return;
  if (a > 1.0f) a = 1.0f;
  rr_rect(0, 0, w, h, RR_RGBA(0, 0, 0, (int)(a * 255.0f + 0.5f)));
}

/* the arena the last wave was fought in, with nobody on it: the rival comes in over it */
static void draw_arena(void) {
  char name[48], key[96];
  gm_enum_lower(GM_E_BIOME_ID, R.a.run.biome, name, sizeof name);
  if (!name[0]) return;
  snprintf(key, sizeof key, "images/arenas/%s_bg", name);
  if (img_has(key)) img_draw(key, NULL, 0, 0, 0, 0);
  snprintf(key, sizeof key, "images/arenas/%s_b", name);
  if (img_has(key)) img_draw(key, NULL, 0, 0, 0, 0);
  snprintf(key, sizeof key, "images/arenas/%s_a", name);
  if (img_has(key)) img_draw(key, NULL, 0, 0, 0, 0);
}

static void draw_top(void) {
  if (ui_space(UI_PHYS_TOP)) rr_rect(0, 0, 400, 240, RR_RGBA(0, 0, 0, 255));
  if (ui_space(UI_FIELD)) {
    text_opts o = text_defaults();
    o.origin_x = o.origin_y = 0.5f;
    o.font_px = 128;
    if (R.a.victory && R.stage >= ST_FADE_IN && R.stage <= ST_HIDE) {
      draw_arena();
      chs_draw();
    } else if (R.a.victory) {
      /* addImage(0, 0, "end_m") with origin 0 and scale 0.5; the text at (width / 2, height - 16) */
      if (img_has("images/cg/end_m")) {
        rr_draw_opts d = RR_DEFAULT_OPTS;
        d.scale_x = d.scale_y = 0.5f;
        img_draw_opts("images/cg/end_m", NULL, 0, 0, &d);
      }
      char t[64];
      i18n_t(t, sizeof t, "battle:congratulations", NULL);
      text_draw(TS_SUMMARY, 160, 164, t, &o);
    } else {
      char name[48], key[96];
      gm_enum_lower(GM_E_BIOME_ID, R.a.run.biome, name, sizeof name);
      if (name[0]) {
        snprintf(key, sizeof key, "images/arenas/%s_bg", name);
        if (img_has(key)) img_draw(key, NULL, 0, 0, 0, 0);
      }
      rr_rect(0, 0, 320, 180, RR_RGBA(0, 0, 0, 160));
      text_draw(TS_SUMMARY, 160, 90, "Game Over", &o);
    }
  }
  if (ui_space(UI_PHYS_TOP)) draw_fade(400, 240);
  /* the bars are the UI's, over the fade as over the scene */
  if (R.a.victory && ui_space(UI_FIELD)) draw_toast();
}

static void draw_bottom(void) {
  if (!ui_space(UI_PHYS_BOTTOM)) return;
  rr_rect(0, 0, 320, 240, RR_RGBA(0x10, 0x10, 0x18, 255));
  ui_window(8, 8, 304, 160, WIN_NORMAL);
  rsum_draw(&R.a.run, 22, 18);
  /* something that was not stored (the custom file): small print in the gap above the line, the 8 px pixel font that is legible here */
  if (R.note[0]) {
    text_opts o = text_defaults();
    o.wrap_w = 300;
    char wrapped[256];
    text_wrap(TS_INSTRUCTIONS_TEXT, R.note, &o, wrapped, sizeof wrapped);
    text_draw(TS_INSTRUCTIONS_TEXT, 10, 170, wrapped, &o);
  }
  const float box_h = R.tall ? BOX_H_TALL : BOX_H_PLAIN;
  ui_window(1, 239 - box_h, 318, box_h, WIN_NORMAL);
  /* showNameText: the name box sits on the message window's top edge while someone talks */
  if (R.stage == ST_TALK) dv_namebox(1, 239 - box_h - 16, R.talk_name, ui_window_style());
  msgbox_draw(&R.msg, 8, 239 - box_h + 8);
  draw_fade(320, 240);
}

static void rs_draw(ui_screen* s, rr_screen which) {
  (void)s;
  if (which == RR_TOP)
    draw_top();
  else
    draw_bottom();
}

static ui_screen g_screen = {"run-result", UI_FADE, NULL, NULL, NULL, rs_input, rs_update, rs_draw, NULL, rs_touch};

void result_screen_open(const result_args* a) {
  /* a->note may point into R itself (the post-mortem's B comes back here with R.a): take it out before R is cleared */
  char keep[sizeof R.note];
  keep[0] = 0;
  if (a->note) snprintf(keep, sizeof keep, "%s", a->note);
  const result_args args = *a;
  wg_gains gains;
  gains.n = 0;
  if (a->gains) gains = *a->gains;
  memset(&R, 0, sizeof R);
  R.a = args;
  R.a.gains = NULL;
  memcpy(R.note, keep, sizeof R.note);
  R.a.note = R.note[0] ? R.note : NULL;
  /* the bars and the unlocks are a win's: a defeat is given none and tells none */
  if (a->victory && gains.n > 0 && gains.n <= WG_MAX) R.gains = gains;
  for (int i = 0; i < R.gains.n; i++) {
    if (wg_is_toast(&R.gains.item[i]))
      R.toasts[R.n_toasts++] = i;
    else
      R.msgs[R.n_msgs++] = i;
  }
  R.page = -1;
  chs_reset();
  /* GameOverPhase: a Classic win shows the rival's ending before the end card (the text may be missing: then it is skipped) */
  if (a->victory && a->ending) {
    i18n_t(R.talk_text, sizeof R.talk_text, "miscDialogue:ending", NULL);
    R.talk = R.talk_text[0] && strcmp(R.talk_text, "miscDialogue:ending") != 0;
    if (R.talk) {
      i18n_t(R.talk_name, sizeof R.talk_name, RIVAL_NAME_KEY, NULL);
      const char* c = strstr(R.talk_text, "@c{");
      const char* end = c ? strchr(c + 3, '}') : NULL;
      snprintf(R.talk_variant, sizeof R.talk_variant, "%.*s", end ? (int)(end - (c + 3)) : 7, end ? c + 3 : "neutral"); /* getCharVariantFromDialogue */
    }
  }
  R.tall = R.n_msgs > 0 || R.talk;
  msgbox_init(&R.msg, TS_WINDOW, 300, R.tall ? BOX_LINES_TALL : BOX_LINES_PLAIN);
  if (R.talk) {
    set_stage(ST_FADE_IN);
  } else {
    set_stage(ST_CARD);
    show_end_card_line();
  }
  start_toast(0);
  ui_set(&g_screen);
}
