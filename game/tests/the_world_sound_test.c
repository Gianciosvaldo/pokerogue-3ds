/* DIO's time stop (BEV_WORLD) and its sounds: se/the_world starts with the inverted circle, the grey stage waits for it to play out (at most
 * WORLD_HOLD_MAX_MS past the circle), and se/the_world_resume starts as the colour comes back. The real event code; the message box is a stand-in
 * that is done at once, so the lines take no time here. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "../source/battle_view.c"
static char played[8][32];
static int n_played;
static float sound_len;   /* what ui_sound_seconds answers for se/the_world */
static int n_load;
const rr_draw_opts RR_DEFAULT_OPTS={0,0,1,1,0,0xffffffff};
const char *i18n_language(void){return "en";}
rr_atlas *img_get(const char *k){(void)k;return NULL;}
int rr_atlas_count(const rr_atlas *a){(void)a;return 0;}
const rr_frame *rr_atlas_frame(const rr_atlas *a,int i){(void)a;(void)i;return NULL;}

/* the boss's two sheets: Eternatus 100 px cell with its 66 rows from y 28; Eternamax 112 px cell with its 98 rows from y 9 */
static rr_frame frame_base={.src_w=100,.src_h=100,.off_x=2,.off_y=28,.w=97,.h=66},frame_max={.src_w=112,.src_h=112,.off_x=8,.off_y=9,.w=100,.h=98};
static rr_frame frame_plain={.src_w=96,.src_h=96,.w=96,.h=96};
static int frame_for_boss=-1;   /* form the stand-in answers for the boss's side: 0 base, >0 Eternamax */
static const rr_frame *answer(void){return B.s[1].mon.species==890?(B.s[1].mon.form>0&&B.s[1].sprite_key[0]=='M'?&frame_max:&frame_base):&frame_plain;}
const rr_frame *img_frame(const char *key,const char *name){(void)key;(void)name;return &frame_plain;}
const rr_frame *img_anim_frame(const char *key,int frame){(void)key;(void)frame;return answer();}
bool img_has(const char *key){(void)key;return true;}
bool menu_sprite_load(menu_sprite *s,const menu_mon *m,bool player){(void)s;(void)m;(void)player;return false;}
void menu_sprite_free(menu_sprite *s){(void)s;}
const rr_frame *menu_sprite_frame(const menu_sprite *s,uint32_t ms){(void)s;(void)ms;return &frame_plain;}
bool ba_release_graphics(void){return false;}
void ba_show(int battler){(void)battler;}
void ba_draw(ba_layer layer,const ba_mon field[4]){(void)layer;(void)field;}
bool ba_hidden(int battler){(void)battler;return false;}
static float cur_zoom=1;
void rr_set_zoom(float z,float cx,float cy){(void)cx;(void)cy;cur_zoom=z;}
void rr_rect(float x,float y,float w,float h,rr_color c){(void)x;(void)y;(void)w;(void)h;(void)c;}
void rr_draw_cropped(const rr_frame *f,float x,float y,int w,int h,const rr_draw_opts *o){(void)f;(void)x;(void)y;(void)w;(void)h;(void)o;}
void rr_draw(const rr_frame *f,float x,float y,const rr_draw_opts *o){(void)f;(void)x;(void)y;(void)o;}
void rr_set_tone(int r,int g,int b){(void)r;(void)g;(void)b;}
void rr_set_grey(int a){(void)a;}
void rr_circle(float x,float y,float r,rr_color c){(void)x;(void)y;(void)r;(void)c;}
int rr_frame_alpha(const rr_frame *f,int x,int y){(void)f;(void)x;(void)y;return 255;}
float ui_now(void){return 0;}
void chs_show(const char *a,const char *v){(void)a;(void)v;}
void chs_set_variant(const char *v){(void)v;}
void chs_hide(void){}
void chs_reset(void){}
const char *i18n_t(char *out,int n,const char *key,...){(void)key;if(n>0)out[0]=0;return out;}
const char *i18n_ability_name(int id){(void)id;return "";}
bool ba_play(ba_kind kind,int id,int user,int target,const ba_mon field[4]){(void)kind;(void)id;(void)user;(void)target;(void)field;return false;}
bool ba_active(void){return false;}
void ba_update(float dt,const ba_mon field[4]){(void)dt;(void)field;}
bool chs_busy(void){return false;}
void msgbox_show(msgbox *m,const char *t,float d,bool p){(void)m;(void)t;(void)d;(void)p;}
void msgbox_clear(msgbox *m){(void)m;}
bool msgbox_done(const msgbox *m){(void)m;return true;}
const char *msgbox_take_variant(msgbox *m){(void)m;return NULL;}
float text_draw(text_style st,float x,float y,const char *s,const text_opts *o){(void)st;(void)x;(void)y;(void)o;return (float)strlen(s)*4;}
float text_width(text_style st,const char *s,const text_opts *o){(void)st;(void)o;return (float)strlen(s)*4;}
text_opts text_defaults(void){text_opts o;memset(&o,0,sizeof o);return o;}
rr_color text_color(text_style s,bool shadow){(void)s;(void)shadow;return 0;}
void ui_play_sound(const char *key){if(n_played<8)snprintf(played[n_played++],32,"%s",key);}
float ui_sound_seconds(const char *key){return !strcmp(key,"se/the_world")?sound_len:0;}
int mui_reclaim_images(void){return 0;}
void ba_debug_text(char* out, size_t n){if(n)out[0]=0;}

/* steps the event 1 ms at a time, pressing A whenever a line waits; returns the ms from the first sound to the event's end (-1 = no sound) */
static float run(int a, int* steps_to_sound) {
  memset(&B, 0, sizeof B);
  snprintf(B.setup.trainer_name, sizeof B.setup.trainer_name, "DIO");
  B.ev[0] = (bv_event){.type = BEV_WORLD, .battler = a ? 2 : -1, .a = a};
  B.n_ev = 1;
  n_played = 0;
  float since = -1;
  int ms = 0;
  for (; ms < 60000 && B.cur == 0; ms++) {
    const int before = n_played;
    run_event(1);
    if (n_played > before && since < 0) {
      since = 0;
      *steps_to_sound = ms;
      continue;
    }
    if (since >= 0) since += 1;
  }
  assert(B.cur == 1);
  return since;
}

int main(void) {
  int at = 0;
  /* the stop with a 2.5 s sound: one sound, after "THE WORLD!" (the circle starts), and the event ends when the sound does */
  sound_len = 2.5f;
  float d = run(1, &at);
  assert(n_played == 1 && !strcmp(played[0], WORLD_SOUND));
  assert(at >= 749);   /* after the 750 ms slide (1 ms steps from 0) and the line */
  assert(fabsf(d - 2500) <= 2);
  /* no sound in the pack: the stop takes its own 700 + 300 ms, as before */
  sound_len = 0;
  d = run(1, &at);
  assert(fabsf(d - (WORLD_GROW_MS + WORLD_DRAIN_MS)) <= 2);
  /* a sound shorter than the circle and the grey: no wait */
  sound_len = 0.4f;
  d = run(1, &at);
  assert(fabsf(d - (WORLD_GROW_MS + WORLD_DRAIN_MS)) <= 2);
  /* a very long file: the battle waits at most WORLD_HOLD_MAX_MS past the circle */
  sound_len = 40;
  d = run(1, &at);
  assert(fabsf(d - (WORLD_GROW_MS + WORLD_HOLD_MAX_MS)) <= 2);
  /* time moves again: its own sound once the line is read, as he slides out (750 ms) */
  sound_len = 2.5f;
  d = run(0, &at);
  assert(n_played == 1 && !strcmp(played[0], WORLD_RESUME_SOUND));
  assert(fabsf(d - 750) <= 2);
  puts("the_world_sound_test: the stop's sound starts with the circle and is waited for (2.5 s, none, short, capped), the resume sound plays at the slide-out: passed");
  return 0;
}
