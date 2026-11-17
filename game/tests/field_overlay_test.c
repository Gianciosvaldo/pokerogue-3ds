/* The grey the field is darkened with behind a dialogue's portrait (upstream's showFieldOverlay, the rival's "Ivy would like to battle!"), on the
 * top screen (lorenzo, 2026-11-16: "the ivy greeting you screen every wave does not fit the whole screen (the semitransparent grey panel)").
 * Upstream's fieldOverlay is 320x132, the field above the message strip; the app drew exactly that into the 320x180 field space, so the 40 px
 * side bands, the 30 px top band and the bottom band of the 400x240 screen stayed bright around a dark rectangle. The real bv_draw is run with a
 * recording stand-in for the drawing calls:
 *   1  overlay at its 0.5 top (the tween's end): one grey 0x424242 rect, alpha 127, in the physical top space, covering the whole 400x240
 *   2  its place in the draw order: after the biome's bands (they are what it darkens), before the portrait (character sprite, on top of the
 *      grey as upstream's is) and before the message strip and the item bar (they are not darkened: opaque windows over it)
 *   3  the tween's other values: alpha follows B.overlay (0.25 -> 63, 1 -> 255); none at all when it is 0 (no portrait, the usual dialogue)
 *   4  the bottom screen never gets it
 * The numbers are the app's own and upstream's colour (0x424242) and 0.5 alpha, not an upstream output; no upstream run. */
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
const rr_frame *img_frame(const char *key,const char *name){(void)key;(void)name;return NULL;}
bool img_has(const char *key){(void)key;return false;}
void img_draw(const char *key,const char *frame,float x,float y,float ox,float oy){(void)key;(void)frame;(void)x;(void)y;(void)ox;(void)oy;}
void img_draw_opts(const char *key,const char *frame,float x,float y,const rr_draw_opts *o){(void)key;(void)frame;(void)x;(void)y;(void)o;}
void rr_draw_cropped(const rr_frame *f,float x,float y,int w,int h,const rr_draw_opts *o){(void)f;(void)x;(void)y;(void)w;(void)h;(void)o;}
void rr_draw(const rr_frame *f,float x,float y,const rr_draw_opts *o){(void)f;(void)x;(void)y;(void)o;}
void rr_set_tone(int r,int g,int b){(void)r;(void)g;(void)b;}
void rr_set_grey(int a){(void)a;}
void rr_circle(float x,float y,float r,rr_color c){(void)x;(void)y;(void)r;(void)c;}
void rr_tri(float x0,float y0,float x1,float y1,float x2,float y2,rr_color c){(void)x0;(void)y0;(void)x1;(void)y1;(void)x2;(void)y2;(void)c;}
void rr_set_zoom(float z,float cx,float cy){(void)z;(void)cx;(void)cy;}
float text_draw(text_style st,float x,float y,const char *s,const text_opts *o){(void)st;(void)x;(void)y;(void)o;return (float)strlen(s)*4;}
float text_width(text_style st,const char *s,const text_opts *o){(void)st;(void)o;return (float)strlen(s)*4;}
text_opts text_defaults(void){text_opts o;memset(&o,0,sizeof o);return o;}
rr_color text_color(text_style s,bool shadow){(void)s;(void)shadow;return 0;}

/* the draw order: what the stand-ins saw, in order, and every grey rect with the physical place it covers */
typedef enum{S_BORDER,S_OVERLAY,S_PORTRAIT,S_STRIP}stage;
static stage seq[16];static int nseq;
typedef struct{float x,y,w,h;unsigned rgb;int a;}rect_call;
static rect_call greys[8];static int ngrey;
static rr_screen drawing;static float ox,oy;
bool ui_space(ui_space_id sp){
 const bool bottom=sp==UI_BOTTOM_UI||sp==UI_BOTTOM_CANVAS||sp==UI_BOTTOM_CANVAS_UI||sp==UI_PHYS_BOTTOM;
 if((bottom?RR_BOTTOM:RR_TOP)!=drawing)return false;
 switch(sp){                                      /* the origins ui.c's ui_space sets */
  case UI_FIELD:case UI_TOP_CANVAS:ox=40;oy=30;break;
  case UI_FIELD_UI:case UI_TOP_CANVAS_UI:ox=40;oy=210;break;
  case UI_BOTTOM_UI:ox=0;oy=240;break;
  case UI_BOTTOM_CANVAS:ox=0;oy=30;break;
  case UI_BOTTOM_CANVAS_UI:ox=0;oy=210;break;
  default:ox=0;oy=0;break;
 }
 return true;
}
rr_screen ui_drawing(void){return drawing;}
void rr_rect(float x,float y,float w,float h,rr_color c){
 const unsigned r=(c>>24)&255,g=(c>>16)&255,b=(c>>8)&255,a=c&255;   /* RR_RGBA: r in the top byte */
 if(r==0x42&&g==0x42&&b==0x42&&ngrey<8){
  greys[ngrey++]=(rect_call){ox+x,oy+y,w,h,(r<<16)|(g<<8)|b,(int)a};
  if(nseq<16)seq[nseq++]=S_OVERLAY;
 }
}
void field_border_draw(const char *key){(void)key;if(nseq<16)seq[nseq++]=S_BORDER;}
void chs_draw(void){if(nseq<16)seq[nseq++]=S_PORTRAIT;}
void ui_window(float x,float y,float w,float h,win_variant style){(void)x;(void)y;(void)w;(void)h;(void)style;if(nseq<16)seq[nseq++]=S_STRIP;}
int ui_window_style(void){return 1;}
void ba_draw(ba_layer layer,const ba_mon field[4]){(void)layer;(void)field;}

static int checks;
#define CHECK(x) do{if(!(x)){fprintf(stderr,"field overlay FAIL line %d: %s\n",__LINE__,#x);return 1;}checks++;}while(0)
static void reset(void){nseq=0;ngrey=0;}
/* the top screen as the app draws it with the overlay at `v` */
static void draw_top(float v){
 memset(&B,0,sizeof B);reset();drawing=RR_TOP;B.overlay=v;B.begun=true;
 bv_draw(NULL,RR_TOP);
}
static int index_of(stage s){for(int i=0;i<nseq;i++)if(seq[i]==s)return i;return -1;}

int main(void){
 /* ---- 1: the overlay at its top covers the whole screen */
 draw_top(0.5f);
 CHECK(ngrey==1);
 CHECK(greys[0].x==0&&greys[0].y==0&&greys[0].w==400&&greys[0].h==240);
 CHECK(greys[0].rgb==0x424242&&greys[0].a==127);
 /* ---- 2: after the bands it darkens, before the portrait and the strip */
 CHECK(index_of(S_BORDER)>=0&&index_of(S_OVERLAY)>index_of(S_BORDER));
 CHECK(index_of(S_PORTRAIT)>index_of(S_OVERLAY));
 CHECK(index_of(S_STRIP)>index_of(S_OVERLAY));
 /* ---- 3: the alpha is the tween's value; none when there is no portrait */
 draw_top(0.25f);CHECK(ngrey==1&&greys[0].a==63&&greys[0].w==400&&greys[0].h==240);
 draw_top(1.0f);CHECK(ngrey==1&&greys[0].a==255);
 draw_top(0.0f);CHECK(ngrey==0&&index_of(S_OVERLAY)<0&&index_of(S_PORTRAIT)>=0);
 /* ---- 4: the bottom screen never gets it */
 memset(&B,0,sizeof B);reset();drawing=RR_BOTTOM;B.overlay=0.5f;B.begun=true;bv_draw(NULL,RR_BOTTOM);
 CHECK(ngrey==0);
 printf("Field overlay: whole 400x240 top screen, 0x424242 at the tween's alpha, between the bands and the portrait (%d checks)\n",checks);
 return 0;
}
/* what the rest of bv_draw reaches and this test does not look at */
const pr_move pr_moves[PR_MOVE_COUNT];
bool ba_hidden(int battler){(void)battler;return false;}
void dv_namebox_at(rr_screen screen,int origin_x,int origin_y,float x,float y,const char *name,int window_style){(void)screen;(void)origin_x;(void)origin_y;(void)x;(void)y;(void)name;(void)window_style;}
const char *i18n_move_name(int move_id){(void)move_id;return "";}
const char *i18n_stat_name(int stat){(void)stat;return "";}
const char *i18n_t(char *out,int n,const char *key,...){(void)key;if(n>0)out[0]=0;return out;}
const rr_frame *img_anim_frame(const char *key,int i){(void)key;(void)i;return NULL;}
const rr_frame *menu_sprite_frame(const menu_sprite *s,uint32_t ms){(void)s;(void)ms;return NULL;}
void msgbox_draw(const msgbox *m,float x,float y){(void)m;(void)x;(void)y;}
const rr_frame *mui_frame(const char *key,const char *frame){(void)key;(void)frame;return NULL;}
int rr_frame_alpha(const rr_frame *f,int x,int y){(void)f;(void)x;(void)y;return 255;}
rr_stats rr_get_stats(void){rr_stats st;memset(&st,0,sizeof st);return st;}
void rr_set_blend(rr_blend b){(void)b;}
void rr_text_sys(float x,float y,float size,const char *s,rr_color c){(void)x;(void)y;(void)size;(void)s;(void)c;}
void ui_cursor(float x,float y){(void)x;(void)y;}
float ui_now(void){return 0;}
/* the target choice's hints (ui_widgets.c), not under test here */
float hint_x(text_style st, float x0, const char* const* texts, int i) { (void)st; (void)texts; return x0 + 60 * i; }
int hint_at(text_style st, float x0, const char* const* texts, int n, float x) { (void)st; (void)x0; (void)texts; (void)n; (void)x; return -1; }
int mui_reclaim_images(void) { return 0; }
void ba_debug_text(char* out, size_t n) { if (n) out[0] = 0; }
