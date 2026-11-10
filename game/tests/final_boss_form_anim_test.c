/* The final boss' Eternamax change on the battle screen (lorenzo, 2026-11-09: the final fight is "still quite clunky": the form swapped in one
 * frame, and the 1.5x sprite was cut off at the top of the screen).
 * QuietFormChangePhase.playFormChangeTween is: the Pokemon turns white over 1000 ms (Cubic.easeIn), the form changes, the old white shape shrinks
 * to a point (delay 250, 500 ms, Cubic.easeInOut) as the new one grows to its size (500 ms), and the white fades away (delay 250, 1000 ms,
 * Cubic.easeOut). One sprite is in texture memory, so here one follows the other: 3500 ms in all. The real renderer is run with recording stand-ins:
 *   1  the change is played (BEV_FORM_CHANGE with target 1): white from the start of the fade-in, the old picture until the point, one swap exactly,
 *      the new picture growing from 0.01 to its size, the white fading to normal, done at 3500 ms; its Transform sound once
 *   2  a form change without the flag (Mega, G-Max, a switch of form) is the old 300 ms scale tween with no white
 *   3  a sprite bigger than its sheet (Eternamax is 1.5x, 112 px cell, 98 px drawn) is drawn with the field zoomed out just enough for its top
 *      to stay inside the screen (field y -30 after the zoom about (160, 132)); one at normal size is not changed
 * The durations and easings are transcribed from the pinned source (quiet-form-change-phase.ts), the screen's size is the app's; no upstream run. */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "../source/battle_view.c"
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
static int n_load,n_sound;
bool menu_sprite_load(menu_sprite *s,const menu_mon *m,bool player){(void)s;(void)m;(void)player;n_load++;return false;}
void menu_sprite_free(menu_sprite *s){(void)s;}
const rr_frame *menu_sprite_frame(const menu_sprite *s,uint32_t ms){(void)s;(void)ms;return &frame_plain;}
bool ba_release_graphics(void){return false;}
void ba_show(int battler){(void)battler;}
void ba_draw(ba_layer layer,const ba_mon field[4]){(void)layer;(void)field;}
bool ba_hidden(int battler){(void)battler;return false;}
void ui_play_sound(const char *key){if(strstr(key,"Transform"))n_sound++;}
static float cur_zoom=1;
void rr_set_zoom(float z,float cx,float cy){(void)cx;(void)cy;cur_zoom=z;}
void rr_rect(float x,float y,float w,float h,rr_color c){(void)x;(void)y;(void)w;(void)h;(void)c;}
void rr_draw_cropped(const rr_frame *f,float x,float y,int w,int h,const rr_draw_opts *o){(void)f;(void)x;(void)y;(void)w;(void)h;(void)o;}
void rr_draw(const rr_frame *f,float x,float y,const rr_draw_opts *o){(void)f;(void)x;(void)y;(void)o;}
void rr_set_tone(int r,int g,int b){(void)r;(void)g;(void)b;}
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

static int checks;
#define CHECK(x) do{if(!(x)){fprintf(stderr,"final boss form anim FAIL line %d: %s\n",__LINE__,#x);return 1;}checks++;}while(0)
static bool near(float a,float b){return fabsf(a-b)<=.002f;}

static void put(side *sd,int species,float scale){
 memset(sd,0,sizeof *sd);sd->present=sd->info_on=true;sd->sprite_on=true;sd->mon.species=species;sd->mon.level=100;sd->mon.max_hp=300;sd->mon.hp=300;sd->hp_shown=300;
 sd->scale=scale;snprintf(sd->mon.name,sizeof sd->mon.name,"Mon%d",species);
}
/* the screen's frame: B.t counts the ms in the event, as bv's update does before it calls the handler */
static bool at(const bv_event *e,side *sd,float t){B.t=t;return step_quiet_form(e,sd,false);}
static float zoom_now(void){cur_zoom=1;field_zoom(true);return cur_zoom;}

int mui_reclaim_images(void) { return 0; } /* battle_view.c frees the menus' idle icon sheets before giving up on a sprite */
void ba_debug_text(char* out, size_t n) { if (n) out[0] = 0; } /* battle_view.c's debug row asks battle_anim.c */
int main(void){
 /* ---- 1: the Eternamax change of the boss */
 memset(&B,0,sizeof B);B.field_scale=1;
 put(side_of(0),445,1);put(side_of(2),890,1);
 side *boss=side_of(2);
 boss->sprite_key[0]='B';boss->sprite_key[1]=0;boss->mon.boss=true;boss->mon.boss_segments=2;
 const float old_scale=boss->scale;
 bv_event ev={.type=BEV_FORM_CHANGE,.battler=2,.a=890,.b=1,.target=1};
 n_load=n_sound=0;
 B.step=0;
 CHECK(!at(&ev,boss,0));
 CHECK(boss->catch_on&&near(boss->catch_untint,1)&&n_sound==1);        /* white starts from the normal look; the sound plays */
 CHECK(boss->mon.form==1&&n_load==0);                                  /* the data changes, the picture stays */
 CHECK(sprite_frame(boss)==&frame_base);
 float prev=boss->catch_untint;
 for(float t=10;t<1000;t+=10){                                         /* turning white: monotone, Cubic.easeIn */
  CHECK(!at(&ev,boss,t));CHECK(boss->catch_untint<=prev+1e-4f&&boss->catch_untint>=0);prev=boss->catch_untint;
  CHECK(sprite_frame(boss)==&frame_base&&near(boss->scale,old_scale)&&n_load==0);
 }
 CHECK(near(1-boss->catch_untint,ease(EASE_CUBIC_IN,.99f)));
 CHECK(!at(&ev,boss,500));CHECK(near(boss->catch_untint,1-ease(EASE_CUBIC_IN,.5f)));
 CHECK(!at(&ev,boss,1000)&&near(boss->catch_untint,0));
 CHECK(!at(&ev,boss,1249)&&near(boss->catch_untint,0)&&near(boss->scale,old_scale));   /* the 250 ms hold */
 /* the old shape shrinks to a point over 500 ms */
 CHECK(!at(&ev,boss,1500));CHECK(near(boss->scale,old_scale+(.01f-old_scale)*ease(EASE_CUBIC_IN_OUT,.5f)));CHECK(n_load==0&&sprite_frame(boss)==&frame_base);
 CHECK(!at(&ev,boss,1749));CHECK(boss->scale<.05f&&n_load==0);
 /* the swap at the point: exactly one load, the new picture, still white */
 CHECK(!at(&ev,boss,1750));CHECK(n_load==1&&boss->catch_untint==0);
 boss->sprite_key[0]='M';                                              /* (the stand-in's key for the Eternamax sheet) */
 CHECK(sprite_frame(boss)==&frame_max&&near(boss->scale,.01f));
 const float target=get_mon_scale(&boss->mon);
 CHECK(target>1.4f);                                                   /* Eternamax is drawn at 1.5x */
 CHECK(!at(&ev,boss,2000));CHECK(near(boss->scale,.01f+(target-.01f)*ease(EASE_CUBIC_IN_OUT,.5f))&&near(boss->catch_untint,0));
 CHECK(!at(&ev,boss,2249));CHECK(boss->scale>target-.05f);
 CHECK(!at(&ev,boss,2400));CHECK(near(boss->scale,target)&&near(boss->catch_untint,0));   /* the 250 ms hold at full size */
 CHECK(!at(&ev,boss,3000));CHECK(near(boss->catch_untint,ease(EASE_CUBIC_OUT,.5f))&&near(boss->scale,target));   /* the white fades away */
 CHECK(!at(&ev,boss,3499));CHECK(boss->catch_untint>.99f);
 CHECK(at(&ev,boss,3500));CHECK(near(boss->catch_untint,1)&&!boss->catch_on&&near(boss->scale,target));
 CHECK(n_load==1&&n_sound==1);

 /* ---- 1b: through the screen's own event loop: the flagged event runs 3500 ms in 10 ms frames and no longer; the sound once */
 {
  memset(&B,0,sizeof B);B.field_scale=1;
  put(side_of(0),445,1);put(side_of(2),890,1);side *b=side_of(2);b->sprite_key[0]='B';b->mon.boss=true;
  B.ev[0]=ev;B.n_ev=1;B.cur=0;B.step=0;B.t=0;n_load=n_sound=0;
  int frames=0;bool white=false;
  while(B.cur<1&&frames<1000){run_event(10);frames++;white|=b->catch_on;}
  CHECK(white&&!b->catch_on&&B.cur==1);
  CHECK(frames>=349&&frames<=352);                                      /* 3500 ms at 10 ms a frame */
  CHECK(n_load==1&&n_sound==1);
 }

 /* ---- 2: another form change (Mega, G-Max...) is not this: no white, the picture changes at once, the 300 ms scale tween */
 {
  memset(&B,0,sizeof B);B.field_scale=1;
  put(side_of(0),445,1);put(side_of(2),6,1);side *s=side_of(2);
  bv_event other={.type=BEV_FORM_CHANGE,.battler=2,.a=890,.b=1};        /* (no target flag; the data's own scale is bigger than 1) */
  s->mon.species=890;
  B.ev[0]=other;B.n_ev=1;B.cur=0;B.step=0;B.t=0;n_load=n_sound=0;
  int frames=0;bool white=false;
  run_event(10);CHECK(n_load==1);                                       /* loaded in the first frame, not in the middle */
  white|=s->catch_on;
  while(B.cur<1&&frames<1000){run_event(10);frames++;white|=s->catch_on;}
  CHECK(!white&&frames<=40&&n_sound==1);                                /* ~300 ms */
 }

 /* ---- 3: a sprite bigger than its sheet is drawn with the field zoomed out so its top stays inside the screen */
 {
  memset(&B,0,sizeof B);B.field_scale=.75f;
  put(side_of(0),445,1);put(side_of(2),890,1.5f);
  side *b=side_of(2);b->sprite_key[0]='M';b->mon.form=1;b->mon.boss=true;
  const float base=B.field_scale*FIELD_ZOOM;                          /* 0.9 */
  float x,y;mon_pos(b,false,&x,&y);
  const float top=y-(frame_max.src_h-frame_max.off_y)*b->scale;       /* where the picture's top row stands in the field */
  CHECK(132+(top-132)*base<-30);                                      /* at 0.9 it is cut off (the bug) */
  const float z=zoom_now();
  CHECK(z<base-.01f);
  CHECK(near(132+(top-132)*z,-30));                                   /* and now it is exactly at the top of the screen */
  /* the zoom-out never makes a normal sprite smaller: Eternatus in its first form, a plain 96 px sprite */
  b->mon.form=0;b->sprite_key[0]='B';b->scale=1;
  CHECK(near(zoom_now(),base));
  put(side_of(2),25,1);b=side_of(2);
  CHECK(near(zoom_now(),base));
  /* a 1.5x sprite that already fits is left alone (a small sheet) */
  put(side_of(2),25,1.5f);b=side_of(2);frame_plain.src_h=40;
  CHECK(near(zoom_now(),base));
  frame_plain.src_h=96;
  /* the single battle with no big sprite: the plain zoom */
  B.field_scale=1;put(side_of(2),25,1);CHECK(near(zoom_now(),FIELD_ZOOM));
 }
 (void)frame_for_boss;
 printf("Final boss form animation: %d checks (white 1 s, one swap at the point, grows from 0.01, fades back over 1 s: 3.5 s; the 1.5x sprite fits the screen's top)\n",checks);
 return 0;
}
