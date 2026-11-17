/* The target choice of a single-target move in a double (bv_ask_target, upstream's TargetSelectUiHandler; lorenzo, 2026-11-16: "no visual
 * indicator of the target is present on the top screen, [and] the first target is by default your other pokemon in a double battle if the
 * move is single target"). It used to be an option list on the bottom screen in field order (ally first). The real battle_view.c is run with
 * recording stand-ins for the drawing calls:
 *   1  the cursor starts on the first foe even when the ally is a target too; on the second foe when the first can't be hit; a slot's last
 *      foe is kept for its next turn, an ally pick is not, and a new wave (bv_begin) forgets it
 *   2  the D-pad moves as upstream's processInput: UP to the first foe, DOWN to the player's side, LEFT / RIGHT inside a side, nothing else
 *   3  A answers the pointed field index, B answers -1; both leave the mode
 *   4  on the top screen the pointed Pokemon flashes (alpha 1 -> 0.25 -> 1 over 450 + 450 ms, then 150 ms at 1) and only it; its info box
 *      bobs 1 px down and back every 250 ms; nothing flashes or bobs once the choice is answered
 *   (not run here, they need the event player's and the frame loop's stand-ins): the reset when a Pokemon is sent out (BEV_SUMMON) and the
 * phase advancing in bv_update; both are one line each in battle_view.c)
 *   5  the bottom screen: a tap on a target's tile points at it, a tap on the pointed tile picks it, a tap on a place the move can't reach
 *      does nothing, the strip's "B: back" cancels
 * The timings are upstream's tween constants (target-select-ui-handler.ts at 8555c08), not an upstream run. */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "../source/battle_view.c"
const rr_draw_opts RR_DEFAULT_OPTS={0,0,1,1,0,0xffffffff};
const char *i18n_language(void){return "en";}
rr_atlas *img_get(const char *k){(void)k;return NULL;}
int rr_atlas_count(const rr_atlas *a){(void)a;return 0;}
const rr_frame *rr_atlas_frame(const rr_atlas *a,int i){(void)a;(void)i;return NULL;}
static rr_frame frame96;
const rr_frame *img_frame(const char *key,const char *name){(void)key;(void)name;return &frame96;}
const rr_frame *mui_frame(const char *key,const char *frame){(void)key;(void)frame;return &frame96;}
const rr_frame *img_anim_frame(const char *key,int frame){(void)key;(void)frame;return &frame96;}
bool img_has(const char *key){(void)key;return true;}
typedef struct{char key[96];float x,y;}img_call;
static img_call imgs[256];static int ni;
void rr_set_zoom(float z,float cx,float cy){(void)z;(void)cx;(void)cy;}
void img_draw(const char *key,const char *frame,float x,float y,float ox,float oy){
 (void)frame;(void)ox;(void)oy;
 if(ni<256){snprintf(imgs[ni].key,sizeof imgs[ni].key,"%s",key);imgs[ni].x=x;imgs[ni].y=y;ni++;}
}
void img_draw_opts(const char *key,const char *frame,float x,float y,const rr_draw_opts *o){(void)o;img_draw(key,frame,x,y,0,0);}
static int rects;
void rr_rect(float x,float y,float w,float h,rr_color c){(void)x;(void)y;(void)w;(void)h;(void)c;rects++;}
void rr_draw_cropped(const rr_frame *f,float x,float y,int w,int h,const rr_draw_opts *o){(void)f;(void)x;(void)y;(void)w;(void)h;(void)o;}
static rr_color tints[16];static int nd;
void rr_draw(const rr_frame *f,float x,float y,const rr_draw_opts *o){(void)f;(void)x;(void)y;if(nd<16)tints[nd++]=o?o->tint:0xffffffff;}
void rr_set_tone(int r,int g,int b){(void)r;(void)g;(void)b;}
void rr_set_grey(int a){(void)a;}
void rr_set_blend(rr_blend b){(void)b;}
int rr_frame_alpha(const rr_frame *f,int x,int y){(void)f;(void)x;(void)y;return 255;}
float ui_now(void){return 0;}
const rr_frame *menu_sprite_frame(const menu_sprite *s,uint32_t ms){(void)s;(void)ms;return &frame96;}
bool ba_hidden(int battler){(void)battler;return false;}
float text_draw(text_style st,float x,float y,const char *s,const text_opts *o){(void)st;(void)x;(void)y;(void)o;return (float)strlen(s)*4;}
float text_width(text_style st,const char *s,const text_opts *o){(void)st;(void)o;return (float)strlen(s)*4;}
text_opts text_defaults(void){text_opts o;memset(&o,0,sizeof o);return o;}
rr_color text_color(text_style s,bool shadow){(void)s;(void)shadow;return 0;}
static int selects;
void ui_play_select(void){selects++;}
void ui_window(float x,float y,float w,float h,win_variant v){(void)x;(void)y;(void)w;(void)h;(void)v;}
void msgbox_clear(msgbox* m){(void)m;}
bool msgbox_input(msgbox* m,ui_button b){(void)m;(void)b;return false;}
void ui_play_error(void){}
/* the hint helper: "A: choose" left of x 60, "B: back" from 60 to 120 (the real one measures the texts) */
float hint_x(text_style st,float x0,const char* const* texts,int i){(void)st;(void)texts;return x0+60*i;}
int hint_at(text_style st,float x0,const char* const* texts,int n,float x){(void)st;(void)texts;for(int i=0;i<n;i++)if(x<x0+60*(i+1))return i;return -1;}
int mui_reclaim_images(void){return 0;}
void ba_debug_text(char* out,size_t n){if(n)out[0]=0;}

static int checks,failed;
#define CHECK(x) do{checks++;if(!(x)){failed++;fprintf(stderr,"target select FAIL line %d: %s\n",__LINE__,#x);}}while(0)
static bool near(float a,float b){return fabsf(a-b)<=.002f;}

static int answered,answers;
static void on_target(int f,void *arg){(void)arg;answered=f;answers++;}
static void put(int field,int species){
 side *sd=side_of(field);memset(sd,0,sizeof *sd);sd->present=sd->info_on=sd->sprite_on=true;sd->mon.species=species;sd->mon.level=30;
 sd->mon.max_hp=sd->mon.hp=60;sd->hp_shown=60;sd->scale=1;sd->mon_alpha=1;snprintf(sd->sprite_key,sizeof sd->sprite_key,"images/pokemon/%d",species);snprintf(sd->mon.name,sizeof sd->mon.name,"Mon%d",species);
}
static void field4(void){
 memset(&B,0,sizeof B);B.setup.doubles=true;B.begun=true;B.field_scale=1;snprintf(B.biome,sizeof B.biome,"plains");
 put(0,1);put(1,4);put(2,7);put(3,10);
}
static void ask(int slot,bool a0,bool a1,bool a2,bool a3){
 bv_target_opts o={{a0,a1,a2,a3},slot,"Tackle"};answers=0;answered=-9;bv_ask_target(&o,on_target,NULL);
}
static void in(ui_button b){bv_input(NULL,b);}
/* the tint the sprite of a field index is drawn with (0xffffffff: none) */
static rr_color tint_of(int field){nd=0;side *sd=side_of(field);draw_sprite(sd,field<2);return nd?tints[0]:0;}
static float box_y(int field){
 ni=0;side *sd=side_of(field);
 const char *key=field<2?"images/ui/pbinfo_player_mini":"images/ui/pbinfo_enemy_mini";
 if(field<2)draw_player_info(sd);else draw_enemy_info(sd);
 for(int i=0;i<ni;i++)if(!strcmp(imgs[i].key,key))return imgs[i].y;
 return -999;
}
static int alpha_of(rr_color c){return (int)(c&0xff);} /* RR_RGBA: alpha is the low byte */

int main(void){
 frame96.src_w=frame96.w=96;frame96.src_h=frame96.h=96;
 /* ---- 1: where the cursor starts */
 field4();
 ask(0,false,true,true,true);CHECK(B.mode==M_TARGET&&B.tg.cursor==2);   /* the ally is a target too: still the first foe */
 ask(0,false,true,false,true);CHECK(B.tg.cursor==3);                     /* the first foe is out of reach */
 ask(1,true,false,true,true);CHECK(B.tg.cursor==2);                      /* slot 1 as well */
 ask(0,true,true,false,false);CHECK(B.tg.cursor==0);                     /* the user and its ally (Acupressure): the user first */
 ask(0,false,true,true,true);in(BTN_RIGHT);in(BTN_ACTION);CHECK(answered==3);
 ask(0,false,true,true,true);CHECK(B.tg.cursor==3);                      /* the slot's last foe */
 ask(1,true,false,true,true);CHECK(B.tg.cursor==2);                      /* the other slot keeps its own */
 ask(0,false,true,true,false);CHECK(B.tg.cursor==2);                     /* its last foe is gone: the first foe */
 ask(0,false,true,true,true);CHECK(B.tg.cursor==3);in(BTN_DOWN);in(BTN_ACTION);CHECK(answered==1);
 ask(0,false,true,true,true);CHECK(B.tg.cursor==2);                      /* an ally picked last turn: fresh */
 ask(0,false,true,true,true);in(BTN_RIGHT);in(BTN_CANCEL);CHECK(answered==-1);
 ask(0,false,true,true,true);CHECK(B.tg.cursor==3);                      /* B is remembered too (upstream sets cursor0 on CANCEL) */
 in(BTN_CANCEL);
 field4();ask(0,false,true,true,true);CHECK(B.tg.cursor==2);             /* a new wave: fresh */

 /* ---- 2: the D-pad */
 field4();ask(0,false,true,true,true);selects=0;
 in(BTN_LEFT);CHECK(B.tg.cursor==2&&selects==0);
 in(BTN_UP);CHECK(B.tg.cursor==2);
 in(BTN_RIGHT);CHECK(B.tg.cursor==3&&selects==1);
 in(BTN_RIGHT);CHECK(B.tg.cursor==3);
 in(BTN_DOWN);CHECK(B.tg.cursor==1);                                    /* the player's side: the ally */
 in(BTN_DOWN);CHECK(B.tg.cursor==1);
 in(BTN_LEFT);CHECK(B.tg.cursor==1);                                    /* the user is no target */
 in(BTN_UP);CHECK(B.tg.cursor==3);                                      /* UP from the right-hand ally: the foe straight across */
 in(BTN_LEFT);CHECK(B.tg.cursor==2);
 in(BTN_DOWN);CHECK(B.tg.cursor==1);                                    /* DOWN from the left foe: the only target on the player's side */
 ask(1,true,false,true,true);in(BTN_DOWN);CHECK(B.tg.cursor==0);in(BTN_UP);CHECK(B.tg.cursor==2); /* the left ally: the left foe */
 in(BTN_RIGHT);in(BTN_DOWN);CHECK(B.tg.cursor==0);                     /* nobody below the right foe: the first on that side */
 in(BTN_CANCEL);
 ask(0,false,true,true,false);in(BTN_DOWN);in(BTN_UP);CHECK(B.tg.cursor==2); /* the foe across can't be hit: the first foe */
 in(BTN_CANCEL);
 field4();B.flipped=true;ask(1,true,false,true,true);in(BTN_DOWN);CHECK(B.tg.cursor==0);in(BTN_UP);CHECK(B.tg.cursor==3); /* a swapped survivor keeps its column */
 in(BTN_CANCEL);B.flipped=false;
 ask(0,true,true,true,true);in(BTN_RIGHT);in(BTN_DOWN);CHECK(B.tg.cursor==1);in(BTN_CANCEL); /* (no move offers both of the player's own and the
                                                                                             * foes; a made-up set shows DOWN goes across too) */
 ask(0,false,true,true,true);in(BTN_DOWN);in(BTN_UP);in(BTN_LEFT);
 CHECK(answers==0&&B.mode==M_TARGET);
 ask(1,true,false,false,true);CHECK(B.tg.cursor==3);in(BTN_LEFT);CHECK(B.tg.cursor==3);in(BTN_DOWN);CHECK(B.tg.cursor==0);
 in(BTN_RIGHT);CHECK(B.tg.cursor==0);in(BTN_UP);CHECK(B.tg.cursor==3);
 ask(0,true,true,false,false);in(BTN_UP);CHECK(B.tg.cursor==0);in(BTN_RIGHT);CHECK(B.tg.cursor==1);in(BTN_LEFT);CHECK(B.tg.cursor==0);
 in(BTN_CANCEL);

 /* ---- 3: A and B */
 field4();ask(0,false,true,true,true);in(BTN_ACTION);CHECK(answers==1&&answered==2&&B.mode==M_IDLE);
 in(BTN_ACTION);CHECK(answers==1);                                     /* answered once */
 ask(1,true,false,true,true);in(BTN_CANCEL);CHECK(answers==1&&answered==-1&&B.mode==M_IDLE);

 /* ---- 4: the top screen */
 field4();ask(0,false,true,true,true);
 CHECK(near(tg_flash(0),1)&&near(tg_flash(225),.625f)&&near(tg_flash(450),.25f)&&near(tg_flash(900),1)&&near(tg_flash(1000),1)&&near(tg_flash(1050+450),.25f));
 B.tg.t=450;
 CHECK(alpha_of(tint_of(2))==64);                                       /* the pointed foe at 0.25 */
 CHECK(tint_of(3)==0xffffffff&&tint_of(1)==0xffffffff&&tint_of(0)==0xffffffff); /* nobody else */
 B.tg.t=225;CHECK(alpha_of(tint_of(2))==159);
 B.tg.t=0;{const float y0=box_y(2);B.tg.t=250;CHECK(near(box_y(2)-y0,1));B.tg.t=125;CHECK(near(box_y(2)-y0,.5f));B.tg.t=500;CHECK(near(box_y(2),y0));
  B.tg.t=250;CHECK(near(box_y(3),-114));}                              /* the other foe's box stays */
 in(BTN_RIGHT);CHECK(near(B.tg.t,0));                                   /* and starts again on the new target (setCursor) */
 B.tg.t=450;CHECK(alpha_of(tint_of(3))==64&&tint_of(2)==0xffffffff);
 in(BTN_DOWN);B.tg.t=250;{const float y=box_y(1);CHECK(near(y,-72-12+27+1));CHECK(alpha_of(tint_of(1))<255);}
 side_of(1)->sub_on=true;side_of(1)->mon_alpha=.5f;B.tg.t=0;CHECK(tint_of(1)==0xffffffff); /* behind a doll it flashes from 1 as upstream */
 in(BTN_ACTION);B.tg.t=450;CHECK(alpha_of(tint_of(1))==128);           /* answered: back to the doll's 0.5, nothing flashes */
 CHECK(tint_of(2)==0xffffffff&&near(box_y(2),-141));

 /* ---- 5: the bottom screen */
 field4();ask(0,false,true,true,true);
 {float x,y;
  tg_tile(3,&x,&y);CHECK(bv_touch(NULL,x+10,240+y+10)&&B.tg.cursor==3&&answers==0);   /* points */
  CHECK(bv_touch(NULL,x+10,240+y+10)&&answers==1&&answered==3);                       /* picks */
  ask(0,false,true,true,true);tg_tile(0,&x,&y);CHECK(!bv_touch(NULL,x+10,240+y+10)&&B.tg.cursor==3&&answers==0); /* the user: nothing */
  tg_tile(1,&x,&y);CHECK(bv_touch(NULL,x+TG_W-1,240+y+tg_h()-1)&&B.tg.cursor==1);
  CHECK(!bv_touch(NULL,x+10,240+y+tg_h()+1)||tg_tile_at(x+10,y+tg_h()+1)!=1);         /* just under the tile is not it */
  CHECK(!bv_touch(NULL,x+TG_W+2,240+y+2));                                             /* between the tiles */
  CHECK(bv_touch(NULL,TG_HINT_X+70,230)&&answers==1&&answered==-1);                     /* "B: back" */
  ask(0,false,true,true,true);CHECK(bv_touch(NULL,TG_HINT_X+5,230)&&answered==B.tg.last[0]);  /* "A: choose" */
  /* the tiles do not run into one another, into the debug lines at the top (physical y < 32) or into the strip */
  /* the debug view (the default): two thin rows below its debug rows and the X / Y labels (physical y < 50); the standard view: the big tiles
   * and their heading below the labels */
  for(int v=0;v<2;v++){
   g_view=v?BV_VIEW_STANDARD:BV_VIEW_DEBUG;
   float x0,y0,x1,y1,x2,y2;tg_tile(2,&x0,&y0);tg_tile(3,&x1,&y1);tg_tile(0,&x2,&y2);
   CHECK(x0+TG_W<=x1&&x1+TG_W<=320&&y0+tg_h()<=y2&&y2+tg_h()<=-48);
   CHECK(v?240+y0-12>=32:240+y0>=110);
   CHECK(near(tg_h(),v?TG_H:TG_MINI_H)&&tg_mini()==!v);
   CHECK(tg_tile_at(x2+5,y2+5)==0&&tg_tile_at(x1+5,y1+5)==3);
  }
  g_view=BV_VIEW_DEBUG;
 }
 field4();ask(0,false,true,true,true);rects=0;draw_target_pad();CHECK(rects>=4);  /* the HP bars of the four tiles */
 g_view=BV_VIEW_STANDARD;rects=0;draw_target_pad();CHECK(rects>=4);g_view=BV_VIEW_DEBUG;
 /* the top screen's strip keeps the fight menu while the target is chosen; the bottom one has the hints */
 CHECK(strip_fight(true)&&!strip_fight(false));
 in(BTN_ACTION);CHECK(!strip_fight(true));B.mode=M_FIGHT;CHECK(strip_fight(true)&&strip_fight(false));
 printf("target select: %d checks, %d failed\n",checks,failed);
 return failed?1:0;
}
